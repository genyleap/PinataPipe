#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <expected>

enum class ConfigError { FileNotFound, InvalidFormat };

class Config {
public:
    static inline const char* PINATA_URL = "https://api.pinata.cloud/";
    static inline const char* IPFS_GATEWAY = "https://ipfs.io/ipfs/";
    std::string pinataApiKey;
    std::string pinataSecret;

    static std::expected<Config, std::pair<ConfigError, std::string>> load();
};

#endif
