#include "core/virtual_device_emulator.hpp"
#include "utils/timing.hpp"

#include <thread>
#include <sstream>
#include <iostream>
#include <iomanip>

// Include ViGEmBus headers
extern "C" {
#pragma warning(push)
#pragma warning(disable: 4828)
#include "ViGEmClient.h"
#pragma warning(pop)
}

VirtualDeviceEmulator::VirtualDeviceEmulator() 
    : m_initialized(false), 
      m_running(false),
      m_vigemClient(nullptr),
      m_rumbleEnabled(true),
      m_rumbleIntensity(1.0f) {
}

VirtualDeviceEmulator::~VirtualDeviceEmulator() {
    shutdown();
}

bool VirtualDeviceEmulator::initialize() {
    if (m_initialized) {
        return true;
    }
    
    if (!initializeInputInjection()) {
        return false;
    }

    m_initialized = true;
    
    if (!initializeVirtualDevices()) {
        m_initialized = false;
        return false;
    }
    
    m_running = true;
    
    // Start injection thread with high priority
    m_injectionThread = std::make_unique<std::thread>([this]() {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        
        while (m_running) {
            // Process injection queue
            {
                std::lock_guard<std::mutex> lock(m_injectionQueueMutex);
                if (!m_injectionQueue.empty()) {
                    for (const auto& state : m_injectionQueue) {
                        // Send the translated state to the appropriate virtual device
                        if (state.targetType == TranslatedState::TARGET_XINPUT) {
                            auto xinputState = TranslationLayer().translateToXInput(state);
                            sendToVirtualXInputDevice(state.sourceUserId, xinputState);
                        } else {
                            auto dinputState = TranslationLayer().translateToDInput(state);
                            sendToVirtualDInputDevice(state.sourceUserId, dinputState);
                        }
                    }
                    m_injectionQueue.clear();
                }
            }
            
            // Small sleep to prevent 100% CPU usage
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
    return true;
}

void VirtualDeviceEmulator::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_running = false;

    if (m_injectionThread && m_injectionThread->joinable()) {
        m_injectionThread->join();
    }

    // Destroy all virtual devices
    {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (auto& device : m_virtualDevices) {
            destroyVirtualDeviceInternal(device);
        }
        m_virtualDevices.clear();
    }

    // Disconnect from ViGEmBus
    if (m_vigemClient) {
        auto client = static_cast<PVIGEM_CLIENT>(m_vigemClient);
        vigem_disconnect(client);
        vigem_free(client);
        m_vigemClient = nullptr;
    }

    m_initialized = false;
}

bool VirtualDeviceEmulator::sendInput(const std::vector<TranslatedState>& translatedStates) {
    if (!m_initialized) {
        return false;
    }
    
    // Process each translated state immediately if possible, otherwise queue
    for (const auto& state : translatedStates) {
        if (state.targetType == TranslatedState::TARGET_XINPUT) {
            auto xinputState = TranslationLayer().translateToXInput(state);
            if (!sendToVirtualXInputDevice(state.sourceUserId, xinputState)) {
                // If immediate send fails, add to queue for retry
                std::lock_guard<std::mutex> lock(m_injectionQueueMutex);
                m_injectionQueue.push_back(state);
            }
        } else {
            auto dinputState = TranslationLayer().translateToDInput(state);
            if (!sendToVirtualDInputDevice(state.sourceUserId, dinputState)) {
                // If immediate send fails, add to queue for retry
                std::lock_guard<std::mutex> lock(m_injectionQueueMutex);
                m_injectionQueue.push_back(state);
            }
        }
    }
    
    return true;
}

bool VirtualDeviceEmulator::createVirtualDevice(TranslatedState::TargetType type, int userId) {
    if (!m_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    
    // Find an available ID
    int newId = 0;
    for (const auto& device : m_virtualDevices) {
        if (device.id >= newId) {
            newId = device.id + 1;
        }
    }
    
    VirtualDevice newDevice;
    newDevice.id = newId;
    newDevice.type = type;
    newDevice.userId = userId;
    newDevice.connected = false;
    newDevice.lastUpdate = 0;
    
    bool success = false;
    if (type == TranslatedState::TARGET_XINPUT) {
        success = createVirtualXInputDevice(userId);
    } else {
        success = createVirtualDInputDevice(userId);
    }
    
    if (success) {
        newDevice.connected = true;
        m_virtualDevices.push_back(newDevice);
        
        // Call callback if set
        if (m_deviceCallback) {
            m_deviceCallback(newId, true);
        }
        
        return true;
    }
    
    return false;
}

bool VirtualDeviceEmulator::destroyVirtualDevice(int deviceId) {
    if (!m_initialized) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    
    auto it = std::find_if(m_virtualDevices.begin(), m_virtualDevices.end(),
                          [deviceId](const VirtualDevice& device) {
                              return device.id == deviceId;
                          });
    
    if (it != m_virtualDevices.end()) {
        destroyVirtualDeviceInternal(*it);
        m_virtualDevices.erase(it);
        
        // Call callback if set
        if (m_deviceCallback) {
            m_deviceCallback(deviceId, false);
        }
        
        return true;
    }
    
    return false;
}

void VirtualDeviceEmulator::destroyVirtualDeviceInternal(VirtualDevice& device) {
    if (device.target && m_vigemClient) {
        PVIGEM_CLIENT client = reinterpret_cast<PVIGEM_CLIENT>(m_vigemClient);
        PVIGEM_TARGET target = reinterpret_cast<PVIGEM_TARGET>(device.target);
        
        if (device.type == TranslatedState::TARGET_XINPUT) {
            vigem_target_x360_unregister_notification(target);
            vigem_target_remove(client, target);
            vigem_target_free(target);
        } else {
            vigem_target_ds4_unregister_notification(target);
            vigem_target_remove(client, target);
            vigem_target_free(target);
        }
        device.target = nullptr;
    }
}

int VirtualDeviceEmulator::getVirtualDeviceCount() const {
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    return static_cast<int>(m_virtualDevices.size());
}

void VirtualDeviceEmulator::setRumbleEnabled(bool enabled) {
    m_rumbleEnabled = enabled;
}

void VirtualDeviceEmulator::setRumbleIntensity(float intensity) {
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    m_rumbleIntensity = intensity;
}

void VirtualDeviceEmulator::setDeviceConnectCallback(DeviceCallback callback) {
    m_deviceCallback = callback;
}

bool VirtualDeviceEmulator::initializeInputInjection() {
    // Initialize ViGEmBus client
    auto client = vigem_alloc();
    if (!client) {
        m_lastError = "vigem_alloc failed (Memory/DLL issue).";
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        return false;
    }
    m_vigemClient = static_cast<void*>(client);

    VIGEM_ERROR vigemError = vigem_connect(client);
    if (!VIGEM_SUCCESS(vigemError)) {
        std::stringstream ss;
        ss << "vigem_connect failed (Error 0x" << std::hex << vigemError << std::dec << "). Driver missing?";
        m_lastError = ss.str();
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        vigem_free(client);
        m_vigemClient = nullptr;
        return false;
    }

    Logger::log("VirtualDeviceEmulator: Connected to ViGEmBus driver successfully.");

    return true;
}

bool VirtualDeviceEmulator::initializeVirtualDevices() {
    // Initialize virtual devices based on configuration
    // For now, create a default virtual device
    return createVirtualDevice(TranslatedState::TARGET_XINPUT, 0);
}

bool VirtualDeviceEmulator::createVirtualXInputDevice(int userId) {
    if (!m_vigemClient) {
        return false;
    }

    // Create a new Xbox 360 controller target
    PVIGEM_TARGET x360Target = vigem_target_x360_alloc();
    if (!x360Target) {
        m_lastError = "vigem_target_x360_alloc failed (Out of memory).";
        return false;
    }

    VIGEM_ERROR error = vigem_target_add(static_cast<PVIGEM_CLIENT>(m_vigemClient), x360Target);
    if (!VIGEM_SUCCESS(error)) {
        std::stringstream ss;
        ss << "vigem_target_add failed (Error 0x" << std::hex << error << std::dec << ")";
        m_lastError = ss.str();
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        vigem_target_free(x360Target);
        return false;
    }

    // Store the target in our device mapping for this userId
    VirtualDevice newDevice;
    newDevice.id = userId; // Use userId as ID for simplicity
    newDevice.type = TranslatedState::TARGET_XINPUT;
    newDevice.userId = userId;
    newDevice.connected = true;
    newDevice.lastUpdate = TimingUtils::getPerformanceCounter();
    newDevice.target = x360Target;
    
    m_virtualDevices.push_back(newDevice);

    return true;
}

bool VirtualDeviceEmulator::createVirtualDInputDevice(int userId) {
    if (!m_vigemClient) {
        return false;
    }

    // Create a new DualShock 4 controller target
    PVIGEM_TARGET ds4Target = vigem_target_ds4_alloc();
    if (!ds4Target) {
        m_lastError = "vigem_target_ds4_alloc failed (Out of memory).";
        return false;
    }

    VIGEM_ERROR error = vigem_target_add(static_cast<PVIGEM_CLIENT>(m_vigemClient), ds4Target);
    if (!VIGEM_SUCCESS(error)) {
        std::stringstream ss;
        ss << "vigem_target_add failed (Error 0x" << std::hex << error << std::dec << ")";
        m_lastError = ss.str();
        vigem_target_free(ds4Target);
        return false;
    }

    // Store the target in our device mapping for this userId
    VirtualDevice newDevice;
    newDevice.id = userId; // Use userId as ID for simplicity
    newDevice.type = TranslatedState::TARGET_DINPUT;
    newDevice.userId = userId;
    newDevice.connected = true;
    newDevice.lastUpdate = TimingUtils::getPerformanceCounter();
    newDevice.target = ds4Target;
    
    m_virtualDevices.push_back(newDevice);

    return true;
}

bool VirtualDeviceEmulator::sendToVirtualXInputDevice(int userId, const XINPUT_STATE& state) {
    // Find the target for this userId
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = std::find_if(m_virtualDevices.begin(), m_virtualDevices.end(),
                          [userId](const VirtualDevice& device) {
                              return device.userId == userId && device.type == TranslatedState::TARGET_XINPUT;
                          });
    
    if (it == m_virtualDevices.end() || !it->target) {
        return false;
    }
    
    // Create an XUSB_REPORT from the XINPUT_STATE
    XUSB_REPORT report;
    XUSB_REPORT_INIT(&report);
    
    // Map the XINPUT_STATE to XUSB_REPORT
    report.wButtons = state.Gamepad.wButtons;
    report.bLeftTrigger = state.Gamepad.bLeftTrigger;
    report.bRightTrigger = state.Gamepad.bRightTrigger;
    report.sThumbLX = state.Gamepad.sThumbLX;
    report.sThumbLY = state.Gamepad.sThumbLY;
    report.sThumbRX = state.Gamepad.sThumbRX;
    report.sThumbRY = state.Gamepad.sThumbRY;
    
    // Submit the report to ViGEmBus
    VIGEM_ERROR error = vigem_target_x360_update(static_cast<PVIGEM_CLIENT>(m_vigemClient), static_cast<PVIGEM_TARGET>(it->target), report);
    
    return VIGEM_SUCCESS(error);
}

bool VirtualDeviceEmulator::sendToVirtualDInputDevice(int userId, const TranslationLayer::DInputState& state) {
    // Find the target for this userId
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = std::find_if(m_virtualDevices.begin(), m_virtualDevices.end(),
                          [userId](const VirtualDevice& device) {
                              return device.userId == userId && device.type == TranslatedState::TARGET_DINPUT;
                          });
    
    if (it == m_virtualDevices.end() || !it->target) {
        return false;
    }
    
    // Create a DS4_REPORT from the DInputState
    DS4_REPORT report;
    DS4_REPORT_INIT(&report);
    
    // Map the DInputState to DS4_REPORT
    report.wButtons = state.wButtons;
    report.bTriggerL = state.bLeftTrigger;
    report.bTriggerR = state.bRightTrigger;
    
    // DS4 report uses BYTE (0-255) for sticks, not SHORT (-32768 to 32767)
    // We need to normalize our LONG input (likely -32768..32767 usually) to 0..255
    auto longToByte = [](LONG val) -> BYTE {
        return static_cast<BYTE>(TranslationLayer::normalizeLong(val) * 127.5f + 127.5f);
    };

    report.bThumbLX = longToByte(state.lX);
    report.bThumbLY = longToByte(state.lY);
    report.bThumbRX = longToByte(state.lRx);
    report.bThumbRY = longToByte(state.lRy);
    
    // Submit the report to ViGEmBus
    VIGEM_ERROR error = vigem_target_ds4_update(static_cast<PVIGEM_CLIENT>(m_vigemClient), static_cast<PVIGEM_TARGET>(it->target), report);
    
    return VIGEM_SUCCESS(error);
}