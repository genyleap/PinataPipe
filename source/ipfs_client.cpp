#include "ipfs_client.hpp"
#include <chrono>
#include <memory>
#include <thread>

IPFSClient::IPFSClient(const Config& cfg) : config(cfg), curl(curl_easy_init()) {
    if (!curl) {
        Logger::log(LogLevel::ERROR, "Failed to initialize CURL", true);
        throw std::runtime_error("Failed to initialize CURL");
    }
    Logger::log(LogLevel::INFO, "IPFSClient initialized with PINATA_URL: " + std::string(Config::PINATA_URL), true);
    validateKeys();
}

IPFSClient::~IPFSClient() {
    if (curl) curl_easy_cleanup(curl);
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

struct ProgressData {
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};

int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressData* data = static_cast<ProgressData*>(clientp);
    double percent = (ultotal > 0) ? (static_cast<double>(ulnow) / ultotal) * 100.0 : 0.0;
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed = now - data->startTime;
    double elapsedSeconds = elapsed.count();
    double speed = (elapsedSeconds > 0) ? (ulnow / elapsedSeconds) : 0.0;
    double remainingBytes = ultotal - ulnow;
    double eta = (speed > 0) ? (remainingBytes / speed) : 0.0;
    Logger::reportProgress(percent, speed, eta);
    return 0;
}

Result<std::string> IPFSClient::performCURLRequest(const std::string& url, const std::string& method, curl_mime* mime) {
    std::lock_guard<std::mutex> lock(curlMutex);
    std::string response;
    ProgressData progressData;

    Logger::log(LogLevel::INFO, "Preparing " + method + " request to: " + url, true);

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, ("pinata_api_key: " + config.pinataApiKey).c_str());
    headers = curl_slist_append(headers, ("pinata_secret_api_key: " + config.pinataSecret).c_str());
    if (mime) {
        headers = curl_slist_append(headers, "Content-Type: multipart/form-data");
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    if (method == "POST" && mime) {
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    } else {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, nullptr);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, nullptr);
        if (method == "DELETE") {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        } else {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        }
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string error = curl_easy_strerror(res);
        Logger::log(LogLevel::ERROR, "CURL failed: " + error, true);
        curl_slist_free_all(headers);
        return std::unexpected(std::make_pair(IPFSError::CURLFailure, error));
    }

    Logger::log(LogLevel::INFO, "Response: " + response, true);
    curl_slist_free_all(headers);
    return response;
}

void IPFSClient::validateKeys() {
    std::string url = std::string(Config::PINATA_URL) + "data/testAuthentication";
    Logger::log(LogLevel::INFO, "Validating keys with URL: " + url, true);
    auto response = performCURLRequest(url, "GET");
    if (!response) {
        Logger::log(LogLevel::ERROR, "Validation CURL failed: " + response.error().second, true);
        throw std::runtime_error("CURL failed in validation: " + response.error().second);
    }
    auto json = parseJSON(*response);
    if (!json || json->isMember("error")) {
        std::string errorMsg = response ? *response : "No response";
        Logger::log(LogLevel::ERROR, "Key validation failed with response: " + errorMsg, true);
        throw std::runtime_error("Invalid Pinata API keys: " + errorMsg);
    }
    Logger::log(LogLevel::INFO, "Pinata API keys validated successfully", true);
}

Result<Json::Value> IPFSClient::parseJSON(const std::string& data) {
    Json::Value result;
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    std::string errors;
    if (!reader->parse(data.c_str(), data.c_str() + data.size(), &result, &errors)) {
        return std::unexpected(std::make_pair(IPFSError::JSONParseError, "Failed to parse JSON: " + errors));
    }
    return result;
}

Result<std::string> IPFSClient::performUpload(const std::string& filePath, const std::optional<Json::Value>& metadata, int retries, std::chrono::seconds retryDelay) {
    if (!fs::exists(filePath)) {
        Logger::log(LogLevel::ERROR, "File not found: " + filePath, true);
        return std::unexpected(std::make_pair(IPFSError::FileNotFound, "File not found: " + filePath));
    }

    for (int attempt = 0; attempt <= retries; ++attempt) {
        Logger::resetProgress();
        curl_mime* mime = curl_mime_init(curl);
        curl_mimepart* part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filedata(part, filePath.c_str());
        curl_mime_filename(part, fs::path(filePath).filename().string().c_str());

        if (metadata) {
            Json::StreamWriterBuilder writer;
            std::string metadataStr = Json::writeString(writer, *metadata);
            part = curl_mime_addpart(mime);
            curl_mime_name(part, "pinataMetadata");
            curl_mime_data(part, metadataStr.c_str(), CURL_ZERO_TERMINATED);
        }

        std::string url = std::string(Config::PINATA_URL) + "pinning/pinFileToIPFS";
        auto response = performCURLRequest(url, "POST", mime);
        curl_mime_free(mime);

        if (!response) {
            Logger::log(LogLevel::ERROR, "Upload attempt " + std::to_string(attempt + 1) + " failed: " + response.error().second, true);
            if (attempt == retries) return std::unexpected(response.error());
            std::this_thread::sleep_for(retryDelay);
            continue;
        }

        auto json = parseJSON(*response);
        if (!json || !json->isMember("IpfsHash")) {
            Json::StreamWriterBuilder writer;
            std::string errorDetail = json ? Json::writeString(writer, *json) : "No JSON response";
            Logger::log(LogLevel::ERROR, "Pinata error on attempt " + std::to_string(attempt + 1) + ": " + errorDetail, true);
            if (attempt == retries) return std::unexpected(std::make_pair(IPFSError::PinataError, "Pinata response missing IpfsHash: " + errorDetail));
            std::this_thread::sleep_for(retryDelay);
            continue;
        }

        return "ipfs://" + json->get("IpfsHash", "").asString();
    }
    return std::unexpected(std::make_pair(IPFSError::PinataError, "All upload attempts failed"));
}

Result<std::vector<std::string>> IPFSClient::upload(const std::vector<std::string>& files, const std::optional<Json::Value>& metadata, std::unique_ptr<UploadStrategy> strategy) {
    if (!strategy) {
        if (files.size() == 1) {
            strategy = std::make_unique<SingleFileStrategy>();
        } else {
            strategy = std::make_unique<BatchFileStrategy>();
        }
    }
    return strategy->upload(*this, files, metadata);
}

Result<std::string> IPFSClient::retrieveContent(const std::string& ipfsHash) {
    std::string hashStr = ipfsHash;
    if (hashStr.substr(0, 7) == "ipfs://") hashStr = hashStr.substr(7);
    return performCURLRequest(std::string(Config::IPFS_GATEWAY) + hashStr, "GET");
}

Result<Json::Value> IPFSClient::listPins(const std::optional<std::string>& group) {
    std::string url = std::string(Config::PINATA_URL) + "data/pinList";
    if (group) url += "?metadata[name]=" + *group;
    return parseJSON(*performCURLRequest(url, "GET"));
}

Result<void> IPFSClient::deletePin(const std::string& ipfsHash) {
    std::string hashStr = ipfsHash;
    if (hashStr.substr(0, 7) == "ipfs://") hashStr = hashStr.substr(7);
    auto response = performCURLRequest(std::string(Config::PINATA_URL) + "pinning/unpin/" + hashStr, "DELETE");
    if (!response) return std::unexpected(response.error());

    auto json = parseJSON(*response);
    if (!json) {
        if (response->find("error") == std::string::npos) {
            Logger::log(LogLevel::INFO, "Warning: Unexpected non-JSON response: " + *response + ", assuming success", true);
            Logger::log(LogLevel::INFO, "Successfully deleted " + ipfsHash, true);
            return {};
        }
        Logger::log(LogLevel::ERROR, "Delete failed with unparseable response: " + *response, true);
        return std::unexpected(std::make_pair(IPFSError::PinataError, "Failed to delete pin: unparseable response - " + *response));
    }

    if (json->isMember("error")) {
        Json::StreamWriterBuilder writer;
        std::string errorDetail = Json::writeString(writer, *json);
        Logger::log(LogLevel::ERROR, "Delete failed: " + errorDetail, true);
        return std::unexpected(std::make_pair(IPFSError::PinataError, "Failed to delete pin: " + errorDetail));
    }

    Logger::log(LogLevel::INFO, "Successfully deleted " + ipfsHash, true);
    return {};
}

std::string IPFSClient::errorToString(const std::pair<IPFSError, std::string>& error) {
    std::string base;
    switch (error.first) {
    case IPFSError::FileNotFound: base = "File not found"; break;
    case IPFSError::CURLFailure: base = "CURL request failed"; break;
    case IPFSError::JSONParseError: base = "JSON parsing error"; break;
    case IPFSError::PinataError: base = "Pinata service error"; break;
    case IPFSError::InvalidInput: base = "Invalid input"; break;
    }
    return base + " - Details: " + error.second;
}
