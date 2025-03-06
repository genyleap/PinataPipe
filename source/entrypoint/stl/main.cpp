#include "ipfs_client.hpp"
#include <iostream>
#include <vector>
#include <iomanip>

void printUsage() {
    std::cout << "Usage: IPFSTool <command> [arguments] [--verbose]\n";
    std::cout << "Commands:\n";
    std::cout << "  upload <file_path> [--group <group_name>] [--metadata <json>]\n";
    std::cout << "  batch <file1> <file2> ... [--group <group_name>] [--metadata <json>]\n";
    std::cout << "  get <ipfs_hash>\n";
    std::cout << "  list [--group <group_name>]\n";
    std::cout << "  delete <ipfs_hash>\n";
    std::cout << "Options:\n";
    std::cout << "  --verbose  Enable detailed output\n";
    std::cout << "  --group    Assign a group name to uploaded files\n";
}

int main(int argc, char* argv[]) {
    CURLcode globalInitResult = curl_global_init(CURL_GLOBAL_ALL);
    if (globalInitResult != CURLE_OK) {
        std::cerr << "Failed to initialize CURL globally: " << curl_easy_strerror(globalInitResult) << "\n";
        return 1;
    }

    if (argc < 2) {
        printUsage();
        curl_global_cleanup();
        return 1;
    }

    bool verbose = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose") {
            verbose = true;
            Logger::verboseMode = true;
        }
    }

    auto configResult = Config::load();
    if (!configResult) {
        Logger::log(LogLevel::ERROR, "Config load failed: " + std::to_string(static_cast<int>(configResult.error().first)) + " - " + configResult.error().second, true);
        curl_global_cleanup();
        return 1;
    }

    try {
        IPFSClient client(*configResult);
        Logger::setProgressCallback([](double percent, double speed, double eta) {
            int barWidth = 20;
            int pos = static_cast<int>(percent * barWidth / 100.0);
            std::cerr << "\r\033[33m[PROGRESS]\033[0m [";
            for (int i = 0; i < barWidth; ++i) {
                if (i < pos) std::cerr << "#";
                else std::cerr << " ";
            }
            std::cerr << "] " << std::fixed << std::setprecision(1) << percent << "% | "
                      << std::setprecision(0) << speed / 1024 << " KB/s | "
                      << "ETA: " << std::setprecision(1) << eta << "s" << std::flush;
            if (percent == 100.0) std::cerr << "\n";
        });

        std::string command = argv[1];
        if (command == "upload" && argc >= 3) {
            std::vector<std::string> files{argv[2]};
            std::optional<Json::Value> metadata;
            std::optional<std::string> group;
            for (int i = 3; i < argc - 1; i += 2) {
                if (std::string(argv[i]) == "--group") group = std::string(argv[i + 1]);
                else if (std::string(argv[i]) == "--metadata") {
                    auto json = client.parseJSON(argv[i + 1]);
                    if (!json) throw std::runtime_error(client.errorToString(json.error()));
                    metadata = *json;
                }
            }
            if (group) {
                if (!metadata) metadata = Json::Value(Json::objectValue);
                (*metadata)["name"] = *group;
            }
            auto result = client.upload(files, metadata);
            if (result) std::cout << "Uploaded: " << (*result)[0] << "\n";
            else throw std::runtime_error(client.errorToString(result.error()));
        } else if (command == "batch" && argc >= 3) {
            std::vector<std::string> files;
            std::optional<Json::Value> metadata;
            std::optional<std::string> group;
            int i = 2;
            while (i < argc && std::string(argv[i]).substr(0, 2) != "--") {
                files.push_back(argv[i++]);
            }
            for (; i < argc - 1; i += 2) {
                if (std::string(argv[i]) == "--group") group = std::string(argv[i + 1]);
                else if (std::string(argv[i]) == "--metadata") {
                    auto json = client.parseJSON(argv[i + 1]);
                    if (!json) throw std::runtime_error(client.errorToString(json.error()));
                    metadata = *json;
                }
            }
            if (group) {
                if (!metadata) metadata = Json::Value(Json::objectValue);
                (*metadata)["name"] = *group;
            }
            auto result = client.upload(files, metadata);
            if (result) for (const auto& hash : *result) std::cout << "Uploaded: " << hash << "\n";
            else throw std::runtime_error(client.errorToString(result.error()));
        } else if (command == "get" && argc >= 3 && std::string(argv[2]).substr(0, 2) != "--") {
            auto result = client.retrieveContent(argv[2]);
            if (result) std::cout << "Content:\n" << *result << "\n";
            else throw std::runtime_error(client.errorToString(result.error()));
        } else if (command == "list") {
            std::optional<std::string> group;
            if (argc > 3 && std::string(argv[2]) == "--group") group = std::string(argv[3]);
            auto result = client.listPins(group);
            if (result) {
                Json::StreamWriterBuilder writer;
                std::cout << "Pinned Files:\n" << Json::writeString(writer, *result) << "\n";
            } else {
                throw std::runtime_error(client.errorToString(result.error()));
            }
        } else if (command == "delete" && argc >= 3 && std::string(argv[2]).substr(0, 2) != "--") {
            auto result = client.deletePin(argv[2]);
            if (result) std::cout << "Deleted pin: " + std::string(argv[2]) << "\n";
            else throw std::runtime_error(client.errorToString(result.error()));
        } else {
            printUsage();
            curl_global_cleanup();
            return 1;
        }
    } catch (const std::exception& e) {
        Logger::log(LogLevel::ERROR, "Operation failed: " + std::string(e.what()), true);
        std::cerr << "Error: " << e.what() << "\n";
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
