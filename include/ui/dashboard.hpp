#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <set>

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include "core/input_capture.hpp"
#include "core/virtual_device_emulator.hpp"
#include "core/translation_layer.hpp"

class Dashboard {
public:
    Dashboard();
    ~Dashboard();
    
    void run();
    void stop();
    
    // Set dependencies (with null checks)
    void setEmulator(VirtualDeviceEmulator* emulator) { 
        if (emulator) m_emulator = emulator; 
    }
    void setTranslationLayer(TranslationLayer* layer) { 
        if (layer) m_translationLayer = layer; 
    }
    
    // Update statistics displayed on the dashboard
    void updateStats(uint64_t frameCount, double deltaTime, const std::vector<ControllerState>& states);
    
    // Set status messages
    void setStatusMessage(const std::string& message);
    void setViGEmAvailable(bool available);
    
    // Load settings from config
    void loadSettings(bool translationEnabled, bool hidHideEnabled, bool socdEnabled, int socdMethod, bool debouncingEnabled);
    
    // Interactive State Getters
    bool isTranslationEnabled() const { std::lock_guard<std::mutex> lock(m_statsMutex); return m_translationEnabled; }
    bool isHidHideEnabled() const { std::lock_guard<std::mutex> lock(m_statsMutex); return m_hidHideEnabled; }
    
    // Device refresh control
    bool isRefreshRequested() const { std::lock_guard<std::mutex> lock(m_statsMutex); return m_refreshRequested; }
    void clearRefreshRequest() { std::lock_guard<std::mutex> lock(m_statsMutex); m_refreshRequested = false; }
    
private:
    void initializeUI();
    void updateUI();
    ftxui::Element renderMainScreen();
    ftxui::Element renderControllersPanel();
    ftxui::Element renderMappingsPanel();
    ftxui::Element renderPerformancePanel();
    ftxui::Element renderStatusPanel();
    ftxui::Element renderInteractiveControls();
    ftxui::Element renderRumblePanel();
    ftxui::Element renderInputTestPanel();
    
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
    bool m_vigemAvailable;
    VirtualDeviceEmulator* m_emulator;
    TranslationLayer* m_translationLayer;
    
    // Interactive State
    int m_selectedSocd;
    int m_selectedTargetType;
    bool m_socdEnabled;
    bool m_debouncingEnabled;
    bool m_hidHideEnabled;
    bool m_translationEnabled;
    float m_rumbleIntensity;
    bool m_rumbleTesting;
    bool m_lastRumbleTesting;
    std::string m_rumbleBtnLabel;
    bool m_refreshRequested;
    
    // Timing
    uint64_t m_lastUpdateTime;
    
    // Mutex for thread-safe updates
    mutable std::mutex m_statsMutex;
    
    // Button press tracking for input test panel
    std::set<WORD> m_pressedButtons; // Tracks which buttons have been pressed at least once
    
    // UI Labels (must be persistent for FTXUI)
    std::vector<std::string> m_socdLabels;
    std::vector<std::string> m_targetLabels;
};