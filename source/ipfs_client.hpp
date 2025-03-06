#ifndef IPFS_CLIENT_HPP
#define IPFS_CLIENT_HPP

#include <string>
#include <vector>
#include <expected>
#include <curl/curl.h>
#include <json/json.h>
#include <filesystem>
#include <mutex>
#include "config.hpp"
#include "logger.hpp"

namespace fs = std::filesystem;

enum class IPFSError { FileNotFound, CURLFailure, JSONParseError, PinataError, InvalidInput };

template<typename T>
using Result = std::expected<T, std::pair<IPFSError, std::string>>;

class IPFSClient {
public:
    struct UploadStrategy {
        virtual ~UploadStrategy() = default;
        virtual Result<std::vector<std::string>> upload(IPFSClient& client, const std::vector<std::string>& files, const std::optional<Json::Value>& metadata) = 0;
    };

    explicit IPFSClient(const Config& cfg);
    ~IPFSClient();

    Result<std::vector<std::string>> upload(const std::vector<std::string>& files, const std::optional<Json::Value>& metadata = {}, std::unique_ptr<UploadStrategy> strategy = nullptr);
    Result<std::string> retrieveContent(const std::string& ipfsHash);
    Result<Json::Value> listPins(const std::optional<std::string>& group = std::nullopt);
    Result<void> deletePin(const std::string& ipfsHash);
    Result<Json::Value> parseJSON(const std::string& data);
    Result<std::string> performUpload(const std::string& filePath, const std::optional<Json::Value>& metadata, int retries = 2, std::chrono::seconds retryDelay = std::chrono::seconds(1));
    static std::string errorToString(const std::pair<IPFSError, std::string>& error);

private:
    Config config;
    CURL* curl;
    std::mutex curlMutex;

    Result<std::string> performCURLRequest(const std::string& url, const std::string& method, curl_mime* mime = nullptr);
    void validateKeys();
};

class SingleFileStrategy : public IPFSClient::UploadStrategy {
public:
    Result<std::vector<std::string>> upload(IPFSClient& client, const std::vector<std::string>& files, const std::optional<Json::Value>& metadata) override {
        if (files.size() != 1) {
            return std::unexpected(std::make_pair(IPFSError::InvalidInput, "Single file strategy requires exactly one file"));
        }
        auto result = client.performUpload(files[0], metadata);
        if (!result) return std::unexpected(result.error());
        return std::vector<std::string>{*result};
    }
};

class BatchFileStrategy : public IPFSClient::UploadStrategy {
public:
    Result<std::vector<std::string>> upload(IPFSClient& client, const std::vector<std::string>& files, const std::optional<Json::Value>& metadata) override {
        std::vector<std::string> results;
        for (const auto& file : files) {
            auto result = client.performUpload(file, metadata);
            if (result) {
                results.push_back(*result);
                Logger::log(LogLevel::INFO, "Uploaded " + file + " to " + *result, true);
            } else {
                Logger::log(LogLevel::ERROR, "Failed to upload " + file + ": " + IPFSClient::errorToString(result.error()), true);
            }
        }
        return results;
    }
};

#endif
