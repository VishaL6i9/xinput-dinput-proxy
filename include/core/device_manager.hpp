#pragma once

#include <set>
#include <map>
#include <string>
#include <memory>
#include "core/input_capture.hpp"
#include "core/translation_layer.hpp"
#include "core/virtual_device_emulator.hpp"

/**
 * @brief Manages the lifecycle of physical and virtual controller devices
 * 
 * This class handles:
 * - Physical device detection and HidHide integration
 * - Virtual device creation and destruction based on translation settings
 * - Device state tracking and synchronization
 */
class DeviceManager {
public:
    // Timing constants for device scanning
    static constexpr double SCAN_INTERVAL_NO_CONTROLLERS_US = 5000000.0;  // 5 seconds
    static constexpr double SCAN_INTERVAL_WITH_CONTROLLERS_US = 30000000.0; // 30 seconds

    DeviceManager(
        VirtualDeviceEmulator* emulator,
        TranslationLayer* translationLayer
    );
    ~DeviceManager() = default;

    /**
     * @brief Process connected physical devices and manage virtual device lifecycle
     * 
     * @param inputStates Current state of all physical controllers
     * @param translationEnabled Whether translation is currently active
     * @param hidHideEnabled Whether HidHide device masking is enabled
     */
    void processDevices(
        const std::vector<ControllerState>& inputStates,
        bool translationEnabled,
        bool hidHideEnabled
    );

    /**
     * @brief Clean up all virtual devices and unhide physical devices
     */
    void cleanup();

    /**
     * @brief Get the count of currently hidden physical devices
     */
    size_t getHiddenDeviceCount() const { return m_hiddenDeviceIds.size(); }

    /**
     * @brief Get the count of active virtual XInput devices
     */
    size_t getVirtualXInputDeviceCount() const { return m_activeVirtualXInputDevices.size(); }

    /**
     * @brief Get the count of active virtual DirectInput devices
     */
    size_t getVirtualDInputDeviceCount() const { return m_activeVirtualDInputDevices.size(); }

private:
    /**
     * @brief Hide a physical device using HidHide
     * 
     * @param state Controller state containing device instance ID
     * @return true if device was successfully hidden or already hidden
     */
    bool hidePhysicalDevice(const ControllerState& state);

    /**
     * @brief Create virtual devices for a connected physical controller
     * 
     * @param state Controller state
     * @param translationEnabled Whether translation is active
     */
    void createVirtualDevicesForController(
        const ControllerState& state,
        bool translationEnabled
    );

    /**
     * @brief Destroy virtual devices for a disconnected controller
     * 
     * @param userId User ID of the disconnected controller
     */
    void destroyVirtualDevicesForController(int userId);

    VirtualDeviceEmulator* m_emulator;
    TranslationLayer* m_translationLayer;

    // Track physical devices that have been hidden or failed to hide
    std::set<std::wstring> m_hiddenDeviceIds;
    std::set<std::wstring> m_failedToHideDeviceIds;

    // Track active virtual devices by userId
    std::map<int, int> m_activeVirtualXInputDevices; // userId -> virtualId
    std::map<int, int> m_activeVirtualDInputDevices;  // userId -> virtualId
};
