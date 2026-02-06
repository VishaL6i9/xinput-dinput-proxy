#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>

#include "core/translation_layer.hpp"

class VirtualDeviceEmulator {
public:
    VirtualDeviceEmulator();
    ~VirtualDeviceEmulator();
    
    bool initialize();
    void shutdown();
    
    // Send translated input to virtual devices
    bool sendInput(const std::vector<TranslatedState>& translatedStates);
    
    // Device management
    bool createVirtualDevice(TranslatedState::TargetType type, int userId = -1);
    bool destroyVirtualDevice(int deviceId);
    int getVirtualDeviceCount() const;
    
    // Configuration
    void setRumbleEnabled(bool enabled);
    void setRumbleIntensity(float intensity); // 0.0 to 1.0
    
    // Callback for device connection events
    using DeviceCallback = std::function<void(int deviceId, bool connected)>;
    void setDeviceConnectCallback(DeviceCallback callback);
    
private:
    bool initializeInputInjection();
    bool initializeVirtualDevices();
    
    // Methods for creating different types of virtual devices
    bool createVirtualXInputDevice(int userId);
    bool createVirtualDInputDevice(int userId);
    
    // Methods for sending input to different device types
    bool sendToVirtualXInputDevice(int userId, const XINPUT_STATE& state);
    bool sendToVirtualDInputDevice(int userId, const TranslationLayer::DInputState& state);
    
    std::atomic<bool> m_initialized;
    std::atomic<bool> m_running;
    
    // Virtual device tracking
    struct VirtualDevice {
        int id;
        TranslatedState::TargetType type;
        int userId;
        bool connected;
        uint64_t lastUpdate;
    };
    
    mutable std::mutex m_devicesMutex;
    std::vector<VirtualDevice> m_virtualDevices;
    
    // Input injection components (Windows 11 API)
    void* m_inputInjector;  // Placeholder for InputInjector object
    
    // Configuration
    bool m_rumbleEnabled;
    float m_rumbleIntensity;
    
    // Callbacks
    DeviceCallback m_deviceCallback;
    
    // Threading for input injection
    std::unique_ptr<std::thread> m_injectionThread;
    std::mutex m_injectionQueueMutex;
    std::vector<TranslatedState> m_injectionQueue;
};