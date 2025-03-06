#include "config.hpp"
#include <fstream>
#include <json/json.h>

std::expected<Config, std::pair<ConfigError, std::string>> Config::load() {
    std::ifstream file("config.json");
    if (!file.is_open()) {
        return std::unexpected(std::make_pair(ConfigError::FileNotFound, "Could not open config.json"));
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        return std::unexpected(std::make_pair(ConfigError::InvalidFormat, "JSON parse error: " + errors));
    }

    Config config;
    config.pinataApiKey = root.get("pinataApiKey", "").asString();
    config.pinataSecret = root.get("pinataSecret", "").asString();

    if (config.pinataApiKey.empty() || config.pinataSecret.empty()) {
        return std::unexpected(std::make_pair(ConfigError::InvalidFormat, "Missing pinataApiKey or pinataSecret in config.json"));
    }

    return config;
}
