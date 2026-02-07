#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "core/input_capture.hpp"
#include "core/translation_layer.hpp"
#include "core/virtual_device_emulator.hpp"
#include "ui/dashboard.hpp"
#include "utils/timing.hpp"
#include "utils/logger.hpp"
#include "utils/config_manager.hpp"
#include <csignal>
#include <atomic>
#include <set>
#include <map>
#include <shlobj.h> // For IsUserAnAdmin

// Global flag for signal handling
std::atomic<bool> g_running(true);

// Windows Console Control Handler
BOOL WINAPI consoleHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            std::cout << "\nShutdown event received. Stopping..." << std::endl;
            g_running = false;
            return TRUE;
        default:
            return FALSE;
    }
}

int main() {
    std::cout << "XInput-DirectInput Proxy for Windows 11" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Enable auto-save logging immediately
    Logger::enableAutoSave(true);

    // Load configuration
    ConfigManager& config = ConfigManager::getInstance();
    config.load();

    // System Audit
    Logger::log("System Audit:");
    Logger::log("  - Admin Privileges: " + std::string(IsUserAnAdmin() ? "YES" : "NO"));
    Logger::log("  - Timestamp: " + Logger::getTimestampString());
    
    if (!IsUserAnAdmin()) {
        Logger::log("WARNING: Running without administrator privileges. Some features may not work.");
        Logger::log("         Please run as administrator for full functionality.");
    }

    // Initialize timing utilities
    TimingUtils::initialize();

    // Create input capture module
    auto inputCapture = std::make_unique<InputCapture>();
    
    // Create translation layer
    auto translationLayer = std::make_unique<TranslationLayer>();
    
    // Load translation settings from config
    translationLayer->setXInputToDInputMapping(config.getBool("xinput_to_dinput", true));
    translationLayer->setDInputToXInputMapping(config.getBool("dinput_to_xinput", true));
    translationLayer->setSOCDCleaningEnabled(config.getBool("socd_enabled", true));
    translationLayer->setSOCDMethod(config.getInt("socd_method", 2));
    translationLayer->setDebouncingEnabled(config.getBool("debouncing_enabled", false));
    translationLayer->setDebounceIntervalMs(config.getInt("debounce_interval_ms", 10));
    
    // Create virtual device emulator
    auto virtualDeviceEmulator = std::make_unique<VirtualDeviceEmulator>();
    
    // Load emulator settings from config
    virtualDeviceEmulator->setRumbleEnabled(config.getBool("rumble_enabled", true));
    virtualDeviceEmulator->setRumbleIntensity(config.getFloat("rumble_intensity", 1.0f));
    
    // Create dashboard UI
    auto dashboard = std::make_unique<Dashboard>();
    dashboard->setEmulator(virtualDeviceEmulator.get());
    dashboard->setTranslationLayer(translationLayer.get());
    
    // Load dashboard settings from config
    // Calculate target type: 0=Xbox360, 1=DS4, 2=Combined
    int targetType = 0; // Default to Xbox 360
    bool xiToDi = config.getBool("xinput_to_dinput", true);
    bool diToXi = config.getBool("dinput_to_xinput", false);
    
    if (xiToDi && diToXi) {
        targetType = 2; // Combined
    } else if (xiToDi) {
        targetType = 1; // DualShock 4
    } else if (diToXi) {
        targetType = 0; // Xbox 360
    }
    
    dashboard->loadSettings(
        config.getBool("translation_enabled", true),
        config.getBool("hidhide_enabled", true),
        config.getBool("socd_enabled", true),
        config.getInt("socd_method", 2),
        config.getBool("debouncing_enabled", false),
        targetType
    );

    // Initialize modules
    if (!inputCapture->initialize()) {
        return -1;
    }

    // Enable and initialize HidHide integration
    bool hidHideEnabled = config.getBool("hidhide_enabled", true);
    virtualDeviceEmulator->enableHidHideIntegration(hidHideEnabled);
    if (hidHideEnabled && !virtualDeviceEmulator->connectHidHide()) {
        std::cout << "WARNING: HidHide driver not available. Physical devices will not be hidden." << std::endl;
        Logger::log("HidHide driver not found or failed to connect");
    }

    if (!virtualDeviceEmulator->initialize()) {
        dashboard->setViGEmAvailable(false);
        Logger::log("WARNING: ViGEmBus driver not available. Running in input test mode only.");
        Logger::log("         Install ViGEmBus from: https://github.com/nefarius/ViGEmBus/releases");
    } else {
        dashboard->setViGEmAvailable(true);
        Logger::log("ViGEmBus initialized successfully");
    }

    // Connect Rumble Passthrough
    virtualDeviceEmulator->setRumbleCallback([&inputCapture](int userId, float left, float right) {
        inputCapture->setVibration(userId, left, right);
    });

    std::cout << "Initialization successful!" << std::endl;
    std::cout << "Starting proxy service..." << std::endl;

    // Start dashboard in a separate thread
    std::thread dashboardThread([&dashboard]() {
        dashboard->run();
    });

    // Register console control handler
    SetConsoleCtrlHandler(consoleHandler, TRUE);

    // Track physical devices that have been hidden or failed to hide
    std::set<std::wstring> hiddenDeviceIds;
    std::set<std::wstring> failedToHideDeviceIds;
    
    // Track active virtual devices by userId
    std::map<int, int> activeVirtualXInputDevices; // userId -> virtualId
    std::map<int, int> activeVirtualDInputDevices;  // userId -> virtualId

    // Main proxy loop
    uint64_t frameCount = 0;
    auto lastTime = TimingUtils::getPerformanceCounter();
    
    // Get polling frequency from config
    int pollingFrequency = config.getInt("polling_frequency", 1000);
    const double targetIntervalMicroseconds = 1000000.0 / pollingFrequency; // Convert Hz to microseconds

    while (g_running) {
        auto currentTime = TimingUtils::getPerformanceCounter();
        double deltaTime = TimingUtils::counterToMicroseconds(currentTime - lastTime);

        // Capture input from physical controllers
        inputCapture->update(deltaTime);

        // Get captured input states (thread-safe copy)
        auto inputStates = inputCapture->getInputStates();

        // Check for new physical devices and manage virtual devices
        for (const auto& state : inputStates) {
            if (state.isConnected) {
                // 1. Conditionally apply HidHide logic
                if (dashboard->isHidHideEnabled() && virtualDeviceEmulator->isHidHideIntegrationEnabled()) {
                    if (!state.deviceInstanceId.empty() && 
                        hiddenDeviceIds.find(state.deviceInstanceId) == hiddenDeviceIds.end() &&
                        failedToHideDeviceIds.find(state.deviceInstanceId) == failedToHideDeviceIds.end()) {
                        if (virtualDeviceEmulator->addPhysicalDeviceToHidHideBlacklist(state.deviceInstanceId)) {
                            hiddenDeviceIds.insert(state.deviceInstanceId);
                            std::cout << "Hidden physical device: " << Logger::wstringToNarrow(state.deviceInstanceId) << std::endl;
                        } else {
                            failedToHideDeviceIds.insert(state.deviceInstanceId);
                        }
                    }
                }

                // 2. Dynamic Virtual Device Creation (ONLY IF TRANSLATION IS ENABLED)
                if (dashboard->isTranslationEnabled()) {
                    // DualShock 4 Emulation (XInput -> DInput)
                    if (translationLayer->isXInputToDInputEnabled()) {
                        if (activeVirtualDInputDevices.find(state.userId) == activeVirtualDInputDevices.end()) {
                            std::string sourceName = Logger::wstringToNarrow(state.productName);
                            if (sourceName.empty()) sourceName = "Xbox 360 Controller (User " + std::to_string(state.userId) + ")";
                            
                            int virtualId = virtualDeviceEmulator->createVirtualDevice(TranslatedState::TARGET_DINPUT, state.userId, sourceName);
                            if (virtualId >= 0) {
                                activeVirtualDInputDevices[state.userId] = virtualId;
                                std::cout << "Created virtual DS4 for " << sourceName << std::endl;
                                Logger::log("DEBUG: Created virtual DS4 (type=TARGET_DINPUT=1) for userId=" + std::to_string(state.userId));
                            }
                        }
                    }
                    
                    // Xbox 360 Emulation (DInput -> XInput)
                    if (translationLayer->isDInputToXInputEnabled()) {
                        if (activeVirtualXInputDevices.find(state.userId) == activeVirtualXInputDevices.end()) {
                            std::string sourceName = Logger::wstringToNarrow(state.productName);
                            if (sourceName.empty()) sourceName = "HID Device";
                            
                            int virtualId = virtualDeviceEmulator->createVirtualDevice(TranslatedState::TARGET_XINPUT, state.userId, sourceName);
                            if (virtualId >= 0) {
                                activeVirtualXInputDevices[state.userId] = virtualId;
                                std::cout << "Created virtual Xbox 360 for " << sourceName << std::endl;
                            }
                        }
                    }
                }
            } else {
                // Handle Disconnection
                if (activeVirtualXInputDevices.count(state.userId)) {
                    virtualDeviceEmulator->destroyVirtualDevice(activeVirtualXInputDevices[state.userId]);
                    activeVirtualXInputDevices.erase(state.userId);
                    std::cout << "Destroyed virtual Xbox 360 for User " << state.userId << std::endl;
                }
                if (activeVirtualDInputDevices.count(state.userId)) {
                    virtualDeviceEmulator->destroyVirtualDevice(activeVirtualDInputDevices[state.userId]);
                    activeVirtualDInputDevices.erase(state.userId);
                    std::cout << "Destroyed virtual DS4 for User " << state.userId << std::endl;
                }
            }
        }

        // 3. Conditional Translation and Emulation
        if (dashboard->isTranslationEnabled()) {
            std::vector<TranslatedState> translatedStates = translationLayer->translate(inputStates);
            virtualDeviceEmulator->sendInput(translatedStates);
        }

        // Update dashboard with current stats
        dashboard->updateStats(frameCount++, deltaTime, inputStates);

        // Adaptive device refresh based on connected controller count
        static uint64_t lastRefreshTime = currentTime;
        int connectedCount = 0;
        for (const auto& state : inputStates) {
            if (state.isConnected) connectedCount++;
        }
        
        // Determine refresh interval based on controller count
        double refreshIntervalMicroseconds;
        if (connectedCount == 0) {
            refreshIntervalMicroseconds = 5000000.0; // 5 seconds when no controllers
        } else {
            refreshIntervalMicroseconds = 30000000.0; // 30 seconds when controllers connected
        }
        
        // Check if manual refresh was requested
        if (dashboard->isRefreshRequested()) {
            inputCapture->refreshDevices();
            lastRefreshTime = currentTime;
            dashboard->clearRefreshRequest();
            Logger::log("Manual device refresh triggered");
        } else if (TimingUtils::counterToMicroseconds(currentTime - lastRefreshTime) > refreshIntervalMicroseconds) {
            inputCapture->refreshDevices();
            lastRefreshTime = currentTime;
        }

        // Calculate sleep time to maintain desired polling frequency
        // Target polling rate from config (default 1000 Hz = 1ms interval)
        double elapsedMicroseconds = TimingUtils::counterToMicroseconds(
            TimingUtils::getPerformanceCounter() - currentTime
        );

        if (elapsedMicroseconds < targetIntervalMicroseconds) {
            double sleepMicroseconds = targetIntervalMicroseconds - elapsedMicroseconds;
            std::this_thread::sleep_for(std::chrono::microseconds(static_cast<long long>(sleepMicroseconds)));
        }

        lastTime = TimingUtils::getPerformanceCounter();
    }

    // Cleanup
    // Unhide all devices before exiting
    if (virtualDeviceEmulator->isHidHideIntegrationEnabled()) {
        for (const auto& deviceId : hiddenDeviceIds) {
            virtualDeviceEmulator->removePhysicalDeviceFromHidHideBlacklist(deviceId);
            std::cout << "Unhidden physical device: " << Logger::wstringToNarrow(deviceId) << std::endl;
        }
        virtualDeviceEmulator->disconnectHidHide();
    }

    dashboard->stop();
    if (dashboardThread.joinable()) {
        dashboardThread.join();
    }

    std::cout << "Proxy service stopped." << std::endl;
    
    // Save configuration
    config.setBool("translation_enabled", dashboard->isTranslationEnabled());
    config.setBool("hidhide_enabled", dashboard->isHidHideEnabled());
    config.save();
    
    // Save session logs to file if enabled
    if (config.getBool("save_logs_on_exit", true)) {
        Logger::saveToTimestampedFile();
    }

    return 0;
}