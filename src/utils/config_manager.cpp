#include "utils/config_manager.hpp"
#include <Windows.h>
#include <algorithm>

std::filesystem::path ConfigManager::getConfigPath(const std::string& filename) {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::filesystem::path exePath(path);
    return exePath.parent_path() / filename;
}

bool ConfigManager::load(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto configPath = getConfigPath(filename);
    std::ifstream file(configPath);
    
    if (!file.is_open()) {
        Logger::log("Config file not found, using defaults: " + configPath.string());
        return false;
    }
    
    m_config.clear();
    std::string line;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        
        // Parse key=value
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            m_config[key] = value;
        }
    }
    
    Logger::log("Configuration loaded from: " + configPath.string());
    return true;
}

bool ConfigManager::save(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto configPath = getConfigPath(filename);
    std::ofstream file(configPath);
    
    if (!file.is_open()) {
        Logger::error("Failed to save config file: " + configPath.string());
        return false;
    }
    
    file << "# XInput-DirectInput Proxy Configuration\n";
    file << "# Auto-generated configuration file\n\n";
    
    for (const auto& [key, value] : m_config) {
        file << key << "=" << value << "\n";
    }
    
    Logger::log("Configuration saved to: " + configPath.string());
    return true;
}

std::string ConfigManager::getString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_config.find(key);
    return (it != m_config.end()) ? it->second : defaultValue;
}

int ConfigManager::getInt(const std::string& key, int defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        try {
            return std::stoi(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

float ConfigManager::getFloat(const std::string& key, float defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        try {
            return std::stof(it->second);
        } catch (...) {
            return defaultValue;
        }
    }
    return defaultValue;
}

bool ConfigManager::getBool(const std::string& key, bool defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        std::string value = it->second;
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return (value == "true" || value == "1" || value == "yes" || value == "on");
    }
    return defaultValue;
}

void ConfigManager::setString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = value;
}

void ConfigManager::setInt(const std::string& key, int value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = std::to_string(value);
}

void ConfigManager::setFloat(const std::string& key, float value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = std::to_string(value);
}

void ConfigManager::setBool(const std::string& key, bool value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = value ? "true" : "false";
}

bool ConfigManager::hasKey(const std::string& key) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config.find(key) != m_config.end();
}

std::string ConfigManager::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}
