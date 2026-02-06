#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

#include "core/input_capture.hpp"
#include "core/translation_layer.hpp"
#include "core/virtual_device_emulator.hpp"
#include "ui/dashboard.hpp"
#include "utils/timing.hpp"

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
        std::cerr << "Failed to initialize input capture module" << std::endl;
        return -1;
    }

    if (!virtualDeviceEmulator->initialize()) {
        std::cerr << "Failed to initialize virtual device emulator" << std::endl;
        return -1;
    }

    std::cout << "Initialization successful!" << std::endl;
    std::cout << "Starting proxy service..." << std::endl;

    // Start dashboard in a separate thread
    std::thread dashboardThread([&dashboard]() {
        dashboard->run();
    });

    // Main proxy loop
    bool running = true;
    uint64_t frameCount = 0;
    auto lastTime = TimingUtils::getPerformanceCounter();

    while (running) {
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
        dashboard->updateStats(frameCount++, deltaTime, inputStates.size());
        
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
    return 0;
}