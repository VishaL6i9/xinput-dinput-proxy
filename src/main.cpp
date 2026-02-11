#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "core/input_capture.hpp"
#include "core/translation_layer.hpp"
#include "core/virtual_device_emulator.hpp"
#include "core/device_manager.hpp"
#include "ui/dashboard.hpp"
#include "utils/timing.hpp"
#include "utils/logger.hpp"
#include "utils/config_manager.hpp"
#include <csignal>
#include <atomic>
#include <shlobj.h> // For IsUserAnAdmin

// Configuration constants
namespace Config {
    constexpr int DEFAULT_POLLING_FREQUENCY_HZ = 1000;
    constexpr double MICROSECONDS_PER_SECOND = 1000000.0;
}

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
    
    // Load stick drift mitigation settings
    translationLayer->setStickDeadzoneEnabled(config.getBool("stick_deadzone_enabled", true));
    translationLayer->setLeftStickDeadzone(config.getFloat("left_stick_deadzone", 0.15f));
    translationLayer->setRightStickDeadzone(config.getFloat("right_stick_deadzone", 0.15f));
    translationLayer->setLeftStickAntiDeadzone(config.getFloat("left_stick_anti_deadzone", 0.0f));
    translationLayer->setRightStickAntiDeadzone(config.getFloat("right_stick_anti_deadzone", 0.0f));
    
    // Create virtual device emulator
    auto virtualDeviceEmulator = std::make_unique<VirtualDeviceEmulator>();
    
    // Load emulator settings from config
    virtualDeviceEmulator->setRumbleEnabled(config.getBool("rumble_enabled", true));
    virtualDeviceEmulator->setRumbleIntensity(config.getFloat("rumble_intensity", 1.0f));
    
    // Create device manager
    auto deviceManager = std::make_unique<DeviceManager>(
        virtualDeviceEmulator.get(),
        translationLayer.get()
    );
    
    // Create dashboard UI
    auto dashboard = std::make_unique<Dashboard>();
    dashboard->setEmulator(virtualDeviceEmulator.get());
    dashboard->setTranslationLayer(translationLayer.get());
    dashboard->setInputCapture(inputCapture.get());
    
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
    
    // Sync stick drift settings to dashboard (read from translation layer)
    if (translationLayer) {
        // Dashboard will read these values from translation layer in updateUI()
    }

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

    // Main proxy loop
    uint64_t frameCount = 0;
    auto lastTime = TimingUtils::getPerformanceCounter();
    
    // Get polling frequency from config
    int pollingFrequency = config.getInt("polling_frequency", Config::DEFAULT_POLLING_FREQUENCY_HZ);
    const double targetIntervalMicroseconds = Config::MICROSECONDS_PER_SECOND / pollingFrequency;
    
    // Device refresh timing
    uint64_t lastRefreshTime = lastTime;

    while (g_running) {
        auto currentTime = TimingUtils::getPerformanceCounter();
        double deltaTime = TimingUtils::counterToMicroseconds(currentTime - lastTime);

        // Capture input from physical controllers
        inputCapture->update(deltaTime);

        // Get captured input states (thread-safe copy)
        auto inputStates = inputCapture->getInputStates();

        // Process device connections/disconnections and manage virtual devices
        deviceManager->processDevices(
            inputStates,
            dashboard->isTranslationEnabled(),
            dashboard->isHidHideEnabled()
        );

        // Translate and send input if translation is enabled
        if (dashboard->isTranslationEnabled()) {
            std::vector<TranslatedState> translatedStates = translationLayer->translate(inputStates);
            virtualDeviceEmulator->sendInput(translatedStates);
        }

        // Update dashboard with current stats
        dashboard->updateStats(frameCount++, deltaTime, inputStates);

        // Adaptive device refresh based on connected controller count
        int connectedCount = 0;
        for (const auto& state : inputStates) {
            if (state.isConnected) connectedCount++;
        }
        
        // Determine refresh interval based on controller count
        double refreshIntervalMicroseconds = (connectedCount == 0) 
            ? DeviceManager::SCAN_INTERVAL_NO_CONTROLLERS_US
            : DeviceManager::SCAN_INTERVAL_WITH_CONTROLLERS_US;
        
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
    deviceManager->cleanup();

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