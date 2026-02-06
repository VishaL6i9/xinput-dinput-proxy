#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include "core/input_capture.hpp"

class Dashboard {
public:
    Dashboard();
    ~Dashboard();
    
    void run();
    void stop();
    
    // Update statistics displayed on the dashboard
    void updateStats(uint64_t frameCount, double deltaTime, const std::vector<ControllerState>& states);
    
    // Set status messages
    void setStatusMessage(const std::string& message);
    void setViGEmAvailable(bool available);
    
private:
    void initializeUI();
    void updateUI();
    ftxui::Element renderMainScreen();
    ftxui::Element renderControllersPanel();
    ftxui::Element renderMappingsPanel();
    ftxui::Element renderPerformancePanel();
    ftxui::Element renderStatusPanel();
    
    std::atomic<bool> m_running;
    std::unique_ptr<std::thread> m_uiThread;
    
    // FTXUI components
    ftxui::ScreenInteractive m_screen;
    ftxui::Component m_mainContainer;
    
    // Statistics
    uint64_t m_frameCount;
    double m_deltaTime;
    std::vector<ControllerState> m_controllerStates;
    std::string m_statusMessage;
    bool m_vigemAvailable = false;
    
    // Timing
    uint64_t m_lastUpdateTime;
    
    // Mutex for thread-safe updates
    mutable std::mutex m_statsMutex;
};