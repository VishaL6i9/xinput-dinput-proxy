/**
 * @file virtual_device_emulator.hpp
 * @brief Virtual controller device emulation using ViGEmBus
 * 
 * This module creates and manages virtual Xbox 360 and DualShock 4 controllers
 * using the ViGEmBus kernel driver. It also integrates with HidHide for
 * physical device masking.
 */
#pragma once

#include <vector>
#include <windows.h>
#include <xinput.h>

extern "C" {
#pragma warning(push)
#pragma warning(disable: 4828)
#include "ViGEmClient.h"
#pragma warning(pop)
}
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include "utils/logger.hpp"

#include "core/translation_layer.hpp"

// Forward declaration for ViGEmBus
// Use void* to avoid including ViGEm headers or getting into typedef conflicts
// The implementation file will cast these to the real types

// Forward declaration for HidHide controller
class HidHideController;

/**
 * @class VirtualDeviceEmulator
 * @brief Manages virtual controller devices via ViGEmBus driver
 * 
 * Features:
 * - Dynamic virtual device creation/destruction
 * - Support for Xbox 360 and DualShock 4 emulation
 * - Rumble/vibration passthrough from games to physical controllers
 * - HidHide integration for physical device masking
 * - Thread-safe device management
 * - Automatic cleanup on shutdown
 */
class VirtualDeviceEmulator {
public:
    VirtualDeviceEmulator();
    ~VirtualDeviceEmulator();

    bool initialize();
    void shutdown();

    // Send translated input to virtual devices
    bool sendInput(const std::vector<TranslatedState>& translatedStates);

    // Device management
    int createVirtualDevice(TranslatedState::TargetType type, int userId = -1, const std::string& sourceName = "standard input");
    bool destroyVirtualDevice(int deviceId);
    int getVirtualDeviceCount() const;

    // Configuration
    void setRumbleEnabled(bool enabled);
    void setRumbleIntensity(float intensity); // 0.0 to 1.0

    // HidHide integration
    void enableHidHideIntegration(bool enable);
    bool isHidHideIntegrationEnabled() const { return m_hidHideEnabled; }
    bool connectHidHide();
    void disconnectHidHide();
    bool addPhysicalDeviceToHidHideBlacklist(const std::wstring& deviceInstanceId);
    bool removePhysicalDeviceFromHidHideBlacklist(const std::wstring& deviceInstanceId);

    // Callback for device connection events
    using DeviceCallback = std::function<void(int deviceId, bool connected)>;
    void setDeviceConnectCallback(DeviceCallback callback);
    
    // Callback for rumble events
    using RumbleCallback = std::function<void(int userId, float leftMotor, float rightMotor)>;
    void setRumbleCallback(RumbleCallback callback);

    // Debugging
    std::string getLastError() const { return m_lastError; }

    // Virtual device tracking info
    struct VirtualDevice {
        int id;
        TranslatedState::TargetType type;
        int userId;
        std::string sourceName;
        bool connected;
        uint64_t lastUpdate;
        void* target;  // ViGEmBus target handle (PVIGEM_TARGET)
    };
    std::vector<VirtualDevice> getVirtualDevices() const {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        return m_virtualDevices;
    }

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

    void destroyVirtualDeviceInternal(VirtualDevice& device);

    mutable std::mutex m_devicesMutex;
    std::vector<VirtualDevice> m_virtualDevices;

    // ViGEmBus components
    void* m_vigemClient;  // ViGEmBus client handle (PVIGEM_CLIENT)

    // HidHide components
    std::unique_ptr<HidHideController> m_hidHideController;
    bool m_hidHideEnabled;

    // Configuration
    bool m_rumbleEnabled;
    float m_rumbleIntensity;

    // Callbacks
    DeviceCallback m_deviceCallback;
    RumbleCallback m_rumbleCallback;
    static VirtualDeviceEmulator* m_instance;
    static void CALLBACK x360Notification(
        PVIGEM_CLIENT client,
        PVIGEM_TARGET target,
        BYTE largeMotor,
        BYTE smallMotor,
        BYTE ledNumber,
        LPVOID userData
    );

    // Threading for input injection
    std::unique_ptr<std::thread> m_injectionThread;
    std::mutex m_injectionQueueMutex;
    std::vector<TranslatedState> m_injectionQueue;

    // Error tracking
    std::string m_lastError;
};