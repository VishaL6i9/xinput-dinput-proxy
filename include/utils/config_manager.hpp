#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <mutex>
#include "logger.hpp"

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    // Load configuration from file
    bool load(const std::string& filename = "config.ini");
    
    // Save configuration to file
    bool save(const std::string& filename = "config.ini");
    
    // Get configuration values with defaults
    std::string getString(const std::string& key, const std::string& defaultValue = "");
    int getInt(const std::string& key, int defaultValue = 0);
    float getFloat(const std::string& key, float defaultValue = 0.0f);
    bool getBool(const std::string& key, bool defaultValue = false);
    
    // Set configuration values
    void setString(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setFloat(const std::string& key, float value);
    void setBool(const std::string& key, bool value);
    
    // Check if key exists
    bool hasKey(const std::string& key) const;
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    std::unordered_map<std::string, std::string> m_config;
    mutable std::mutex m_mutex;
    
    std::string trim(const std::string& str);
    std::filesystem::path getConfigPath(const std::string& filename);
};
