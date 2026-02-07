#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <Windows.h>

class Logger {
public:
    static void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.push_back(message);
        std::cout << message << std::endl;
        
        // Auto-save to file if enabled
        if (m_autoSaveEnabled) {
            appendToFile(message);
        }
    }

    static void error(const std::string& message) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string errorMsg = "ERROR: " + message;
        m_logs.push_back(errorMsg);
        std::cerr << errorMsg << std::endl;
        
        // Auto-save to file if enabled
        if (m_autoSaveEnabled) {
            appendToFile(errorMsg);
        }
    }

    static std::vector<std::string> getLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_logs;
    }

    static void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_logs.clear();
    }

    static std::string wstringToNarrow(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), NULL, 0, NULL, NULL);
        if (size_needed <= 0) {
            // Fallback to simple conversion
            std::string narrow;
            for (wchar_t wc : wstr) {
                narrow += static_cast<char>(wc);
            }
            return narrow;
        }
        
        std::string narrow(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.length(), &narrow[0], size_needed, NULL, NULL);
        return narrow;
    }

    static void saveToTimestampedFile() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logs.empty()) return;

        try {
            // If we're already auto-saving, just flush
            if (m_autoSaveEnabled && m_logFile.is_open()) {
                m_logFile.flush();
                std::cout << "Logs flushed to: " << m_logFilePath.string() << std::endl;
                return;
            }

            // Get executable directory
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            std::filesystem::path exePath(path);
            std::filesystem::path logDir = exePath.parent_path();

            // Generate filename: yyyy-mm-dd-HHMMSS.log
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm timeinfo;
            #ifdef _WIN32
            localtime_s(&timeinfo, &in_time_t);
            #else
            localtime_r(&in_time_t, &timeinfo);
            #endif
            std::stringstream ss;
            ss << std::put_time(&timeinfo, "%Y-%m-%d-%H%M%S");
            std::string filename = ss.str() + ".log";
            std::filesystem::path logPath = logDir / filename;

            std::ofstream outFile(logPath);
            if (outFile.is_open()) {
                for (const auto& line : m_logs) {
                    outFile << line << std::endl;
                }
                std::cout << "Logs saved to: " << logPath.string() << std::endl;
            } else {
                std::cerr << "Failed to open log file for writing: " << logPath.string() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error saving logs: " << e.what() << std::endl;
        }
    }
    
    static void enableAutoSave(bool enable = true) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (enable && !m_autoSaveEnabled) {
            // Open log file for continuous writing
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(NULL, path, MAX_PATH);
            std::filesystem::path exePath(path);
            std::filesystem::path logDir = exePath.parent_path();

            // Generate filename: yyyy-mm-dd-HHMMSS.log
            auto now = std::chrono::system_clock::now();
            auto in_time_t = std::chrono::system_clock::to_time_t(now);
            std::tm timeinfo;
            #ifdef _WIN32
            localtime_s(&timeinfo, &in_time_t);
            #else
            localtime_r(&in_time_t, &timeinfo);
            #endif
            std::stringstream ss;
            ss << std::put_time(&timeinfo, "%Y-%m-%d-%H%M%S");
            std::string filename = ss.str() + ".log";
            m_logFilePath = logDir / filename;
            
            m_logFile.open(m_logFilePath, std::ios::out | std::ios::app);
            if (m_logFile.is_open()) {
                m_autoSaveEnabled = true;
                std::cout << "Auto-save logging enabled: " << m_logFilePath.string() << std::endl;
                
                // Write existing logs
                for (const auto& line : m_logs) {
                    m_logFile << line << std::endl;
                }
                m_logFile.flush();
            }
        } else if (!enable && m_autoSaveEnabled) {
            if (m_logFile.is_open()) {
                m_logFile.flush();
                m_logFile.close();
            }
            m_autoSaveEnabled = false;
        }
    }

    static std::string getTimestampString() {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::tm timeinfo;
        #ifdef _WIN32
        localtime_s(&timeinfo, &in_time_t);
        #else
        localtime_r(&in_time_t, &timeinfo);
        #endif
        std::stringstream ss;
        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

private:
    static inline std::vector<std::string> m_logs;
    static inline std::mutex m_mutex;
    static inline bool m_autoSaveEnabled = false;
    static inline std::ofstream m_logFile;
    static inline std::filesystem::path m_logFilePath;
    
    static void appendToFile(const std::string& message) {
        // Assumes mutex is already locked by caller
        if (m_logFile.is_open()) {
            m_logFile << message << std::endl;
            m_logFile.flush(); // Flush immediately to ensure it's written
        }
    }
};
