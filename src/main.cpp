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
#include <csignal>
#include <atomic>

// Global flag for signal handling
std::atomic<bool> g_running(true);

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\nInterrupt signal (" << signum << ") received. Stopping..." << std::endl;
        g_running = false;
    }
}

int main() {
    std::cout << "XInput-DirectInput Proxy for Windows 11" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Initialize timing utilities
    TimingUtils::initialize();

    // Create input capture module
    auto inputCapture = std::make_unique<InputCapture>();
    
    // Create translation layer
    auto translationLayer = std::make_unique<TranslationLayer>();
    
    // Create virtual device emulator
    auto virtualDeviceEmulator = std::make_unique<VirtualDeviceEmulator>();
    
    // Create dashboard UI
    auto dashboard = std::make_unique<Dashboard>();

    // Initialize modules
    if (!inputCapture->initialize()) {
        return -1;
    }

    if (!virtualDeviceEmulator->initialize()) {
        dashboard->setViGEmAvailable(false);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    } else {
        dashboard->setViGEmAvailable(true);
    }

    std::cout << "Initialization successful!" << std::endl;
    std::cout << "Starting proxy service..." << std::endl;

    // Start dashboard in a separate thread
    std::thread dashboardThread([&dashboard]() {
        dashboard->run();
    });

    // Register signal handler
    signal(SIGINT, signalHandler);

    // Main proxy loop
    uint64_t frameCount = 0;
    auto lastTime = TimingUtils::getPerformanceCounter();

    while (g_running) {
        auto currentTime = TimingUtils::getPerformanceCounter();
        double deltaTime = TimingUtils::counterToMicroseconds(currentTime - lastTime);
        
        // Capture input from physical controllers
        inputCapture->update(deltaTime);
        
        // Get captured input states
        auto inputStates = inputCapture->getInputStates();
        
        // Translate input states
        auto translatedStates = translationLayer->translate(inputStates);
        
        // Send translated input to virtual devices
        virtualDeviceEmulator->sendInput(translatedStates);
        
        // Update dashboard with current stats
        dashboard->updateStats(frameCount++, deltaTime, inputStates);
        
        // Calculate sleep time to maintain desired polling frequency
        // Target ~1000 Hz polling rate (1ms interval)
        const double targetIntervalMicroseconds = 1000.0; // 1ms = 1000 microseconds
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
    dashboard->stop();
    if (dashboardThread.joinable()) {
        dashboardThread.join();
    }

    std::cout << "Proxy service stopped." << std::endl;
    std::cout << "\n--- Startup Log Summary ---" << std::endl;
    
    for (const auto& log : Logger::getLogs()) {
        std::cout << log << std::endl;
    }
    
    std::cout << "---------------------------" << std::endl;

    return 0;
}