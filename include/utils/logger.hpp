#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <iostream>

class Logger {
public:
    static void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.push_back(message);
        std::cout << message << std::endl;
    }

    static void error(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.push_back("ERROR: " + message);
        std::cerr << message << std::endl;
    }

    static std::vector<std::string> getLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_logs;
    }

    static void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }

private:
    static inline std::vector<std::string> m_logs;
    static inline std::mutex m_mutex;
};
