#include "core/device_manager.hpp"
#include "utils/logger.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <set>

DeviceManager::DeviceManager(
    VirtualDeviceEmulator* emulator,
    TranslationLayer* translationLayer
) : m_emulator(emulator),
    m_translationLayer(translationLayer) {
}

void DeviceManager::processDevices(
    const std::vector<ControllerState>& inputStates,
    bool translationEnabled,
    bool hidHideEnabled
) {
    for (const auto& state : inputStates) {
        if (state.isConnected) {
            // Only hide DInput devices (userId < 0) when translating to XInput
            // XInput devices (userId >= 0) cannot be hidden via HidHide as they use XInput API
            bool shouldHide = hidHideEnabled && 
                            m_emulator->isHidHideIntegrationEnabled() &&
                            state.userId < 0 &&  // DInput device
                            m_translationLayer->isDInputToXInputEnabled();  // Translating to XInput
            
            if (shouldHide) {
                bool wasHidden = hidePhysicalDevice(state);
                
                // If we just hid the device, give Windows a moment to process it
                // This prevents games from detecting both devices simultaneously
                if (wasHidden && m_hiddenDeviceIds.find(state.deviceInstanceId) != m_hiddenDeviceIds.end()) {
                    // Check if this is the first time we're hiding this device
                    static std::set<std::wstring> devicesHiddenThisSession;
                    if (devicesHiddenThisSession.find(state.deviceInstanceId) == devicesHiddenThisSession.end()) {
                        devicesHiddenThisSession.insert(state.deviceInstanceId);
                        Logger::log("Waiting for HidHide to take effect before creating virtual device...");
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }
            }

            // Create virtual devices if translation is enabled
            if (translationEnabled) {
                createVirtualDevicesForController(state, translationEnabled);
            }
        } else {
            // Handle disconnection
            destroyVirtualDevicesForController(state.userId);
        }
    }
}

bool DeviceManager::hidePhysicalDevice(const ControllerState& state) {
    // Skip if device ID is empty or already processed
    if (state.deviceInstanceId.empty()) {
        return false;
    }

    // Check if this is an XInput device (userId >= 0)
    if (state.userId >= 0) {
        // Log once per device that XInput devices cannot be hidden
        static std::set<std::wstring> xInputWarningLogged;
        if (xInputWarningLogged.find(state.deviceInstanceId) == xInputWarningLogged.end()) {
            xInputWarningLogged.insert(state.deviceInstanceId);
            Logger::log("INFO: XInput device cannot be hidden via HidHide (XInput API bypasses HID layer)");
            Logger::log("      Device: " + Logger::wstringToNarrow(state.deviceInstanceId));
        }
        return false;
    }

    // Already hidden
    if (m_hiddenDeviceIds.find(state.deviceInstanceId) != m_hiddenDeviceIds.end()) {
        return true;
    }

    // Previously failed to hide
    if (m_failedToHideDeviceIds.find(state.deviceInstanceId) != m_failedToHideDeviceIds.end()) {
        return false;
    }

    // Attempt to hide the device
    if (m_emulator->addPhysicalDeviceToHidHideBlacklist(state.deviceInstanceId)) {
        m_hiddenDeviceIds.insert(state.deviceInstanceId);
        std::cout << "Hidden physical device: " 
                  << Logger::wstringToNarrow(state.deviceInstanceId) << std::endl;
        return true;
    } else {
        m_failedToHideDeviceIds.insert(state.deviceInstanceId);
        return false;
    }
}

void DeviceManager::createVirtualDevicesForController(
    const ControllerState& state,
    bool translationEnabled
) {
    // DualShock 4 Emulation (XInput -> DInput)
    if (m_translationLayer->isXInputToDInputEnabled()) {
        if (m_activeVirtualDInputDevices.find(state.userId) == m_activeVirtualDInputDevices.end()) {
            std::string sourceName = Logger::wstringToNarrow(state.productName);
            if (sourceName.empty()) {
                sourceName = "Xbox 360 Controller (User " + std::to_string(state.userId) + ")";
            }

            int virtualId = m_emulator->createVirtualDevice(
                TranslatedState::TARGET_DINPUT,
                state.userId,
                sourceName
            );

            if (virtualId >= 0) {
                m_activeVirtualDInputDevices[state.userId] = virtualId;
                std::cout << "Created virtual DS4 for " << sourceName << std::endl;
                Logger::log("Created virtual DS4 (type=TARGET_DINPUT) for userId=" 
                           + std::to_string(state.userId));
            }
        }
    }

    // Xbox 360 Emulation (DInput -> XInput)
    if (m_translationLayer->isDInputToXInputEnabled()) {
        if (m_activeVirtualXInputDevices.find(state.userId) == m_activeVirtualXInputDevices.end()) {
            std::string sourceName = Logger::wstringToNarrow(state.productName);
            if (sourceName.empty()) {
                sourceName = "HID Device";
            }

            int virtualId = m_emulator->createVirtualDevice(
                TranslatedState::TARGET_XINPUT,
                state.userId,
                sourceName
            );

            if (virtualId >= 0) {
                m_activeVirtualXInputDevices[state.userId] = virtualId;
                std::cout << "Created virtual Xbox 360 for " << sourceName << std::endl;
                Logger::log("Created virtual Xbox 360 (type=TARGET_XINPUT) for userId=" 
                           + std::to_string(state.userId));
            }
        }
    }
}

void DeviceManager::destroyVirtualDevicesForController(int userId) {
    // Destroy virtual XInput device
    if (m_activeVirtualXInputDevices.count(userId)) {
        m_emulator->destroyVirtualDevice(m_activeVirtualXInputDevices[userId]);
        m_activeVirtualXInputDevices.erase(userId);
        std::cout << "Destroyed virtual Xbox 360 for User " << userId << std::endl;
        Logger::log("Destroyed virtual Xbox 360 for userId=" + std::to_string(userId));
    }

    // Destroy virtual DirectInput device
    if (m_activeVirtualDInputDevices.count(userId)) {
        m_emulator->destroyVirtualDevice(m_activeVirtualDInputDevices[userId]);
        m_activeVirtualDInputDevices.erase(userId);
        std::cout << "Destroyed virtual DS4 for User " << userId << std::endl;
        Logger::log("Destroyed virtual DS4 for userId=" + std::to_string(userId));
    }
}

void DeviceManager::cleanup() {
    // Unhide all physical devices
    if (m_emulator->isHidHideIntegrationEnabled()) {
        for (const auto& deviceId : m_hiddenDeviceIds) {
            m_emulator->removePhysicalDeviceFromHidHideBlacklist(deviceId);
            std::cout << "Unhidden physical device: " 
                      << Logger::wstringToNarrow(deviceId) << std::endl;
        }
        m_hiddenDeviceIds.clear();
        m_emulator->disconnectHidHide();
    }

    // Destroy all virtual devices
    for (const auto& [userId, virtualId] : m_activeVirtualXInputDevices) {
        m_emulator->destroyVirtualDevice(virtualId);
    }
    m_activeVirtualXInputDevices.clear();

    for (const auto& [userId, virtualId] : m_activeVirtualDInputDevices) {
        m_emulator->destroyVirtualDevice(virtualId);
    }
    m_activeVirtualDInputDevices.clear();
}
