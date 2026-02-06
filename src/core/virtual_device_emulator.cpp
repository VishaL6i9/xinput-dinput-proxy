#include "core/virtual_device_emulator.hpp"
#include "utils/timing.hpp"

#include <thread>
#include <sstream>

VirtualDeviceEmulator::VirtualDeviceEmulator() 
    : m_initialized(false), 
      m_running(false),
      m_inputInjector(nullptr),
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
    
    if (!initializeVirtualDevices()) {
        return false;
    }
    
    m_running = true;
    m_initialized = true;
    
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
            destroyVirtualDevice(device.id);
        }
        m_virtualDevices.clear();
    }
    
    m_initialized = false;
}

bool VirtualDeviceEmulator::sendInput(const std::vector<TranslatedState>& translatedStates) {
    if (!m_initialized) {
        return false;
    }
    
    // Add states to injection queue
    {
        std::lock_guard<std::mutex> lock(m_injectionQueueMutex);
        m_injectionQueue.insert(m_injectionQueue.end(), translatedStates.begin(), translatedStates.end());
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
        // Actually destroy the device based on its type
        if (it->type == TranslatedState::TARGET_XINPUT) {
            // TODO: Implement actual destruction of virtual XInput device
        } else {
            // TODO: Implement actual destruction of virtual DInput device
        }
        
        m_virtualDevices.erase(it);
        
        // Call callback if set
        if (m_deviceCallback) {
            m_deviceCallback(deviceId, false);
        }
        
        return true;
    }
    
    return false;
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
    // NOTE: This is a placeholder implementation
    // Actual Windows Input Injection API usage would require:
    // 1. Proper COM initialization
    // 2. Creating InputInjector object
    // 3. Configuring for gamepad injection
    
    // For now, we'll simulate the initialization
    // In a real implementation, we would:
    /*
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return false;
    }
    
    // Create InputInjector
    // Windows::UI::Input::Preview::Injection::InputInjector^ injector = 
    //     Windows::UI::Input::Preview::Injection::InputInjector::TryCreate();
    // if (!injector) {
    //     return false;
    // }
    */
    
    // Since we can't directly use Windows Runtime C++ APIs without proper setup,
    // we'll use a placeholder approach for now
    m_inputInjector = static_cast<void*>(const_cast<char*>("PLACEHOLDER")); // Placeholder
    
    return true;
}

bool VirtualDeviceEmulator::initializeVirtualDevices() {
    // Initialize virtual devices based on configuration
    // For now, create a default virtual device
    return createVirtualDevice(TranslatedState::TARGET_XINPUT, 0);
}

bool VirtualDeviceEmulator::createVirtualXInputDevice(int userId) {
    // In a real implementation, this would create a virtual XInput device
    // using the Input Injection API or a user-mode driver approach
    
    // Placeholder implementation
    return true;
}

bool VirtualDeviceEmulator::createVirtualDInputDevice(int userId) {
    // In a real implementation, this would create a virtual DirectInput device
    // using the Input Injection API or a user-mode driver approach
    
    // Placeholder implementation
    return true;
}

bool VirtualDeviceEmulator::sendToVirtualXInputDevice(int userId, const XINPUT_STATE& state) {
    // In a real implementation, this would inject the XINPUT_STATE to a virtual device
    // using the Input Injection API
    
    // Placeholder implementation
    // Would use InputInjector to send gamepad input
    return true;
}

bool VirtualDeviceEmulator::sendToVirtualDInputDevice(int userId, const TranslationLayer::DInputState& state) {
    // In a real implementation, this would inject the DInput state to a virtual device
    // using the Input Injection API
    
    // Placeholder implementation
    // Would use InputInjector to send DirectInput-compatible input
    return true;
}