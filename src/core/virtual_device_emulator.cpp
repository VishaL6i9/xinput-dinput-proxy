#include "core/virtual_device_emulator.hpp"
#include "utils/timing.hpp"
#include "utils/hidhide_controller.hpp"

#include <thread>
#include <sstream>
#include <iostream>
#include <iomanip>

// ViGEmClient.h is already included in the header with proper warning suppression

VirtualDeviceEmulator* VirtualDeviceEmulator::m_instance = nullptr;

void CALLBACK VirtualDeviceEmulator::x360Notification(
    PVIGEM_CLIENT client,
    PVIGEM_TARGET target,
    BYTE largeMotor,
    BYTE smallMotor,
    BYTE ledNumber,
    LPVOID userData
) {
    if (m_instance && m_instance->m_rumbleCallback) {
        int userId = static_cast<int>(reinterpret_cast<uintptr_t>(userData));
        float left = static_cast<float>(largeMotor) / 255.0f;
        float right = static_cast<float>(smallMotor) / 255.0f;
        m_instance->m_rumbleCallback(userId, left, right);
    }
}

VirtualDeviceEmulator::VirtualDeviceEmulator()
    : m_initialized(false),
      m_running(false),
      m_vigemClient(nullptr),
      m_hidHideController(nullptr),
      m_hidHideEnabled(false),
      m_rumbleEnabled(true),
      m_rumbleIntensity(1.0f) {
    m_instance = this;
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

int VirtualDeviceEmulator::createVirtualDevice(TranslatedState::TargetType type, int userId, const std::string& sourceName) {
    if (!m_initialized) {
        return -1;
    }
    
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    
    // Find an available ID
    int newId = 0;
    while (std::any_of(m_virtualDevices.begin(), m_virtualDevices.end(), [newId](const VirtualDevice& d) { return d.id == newId; })) {
        newId++;
    }
    
    // Create the virtual device target BEFORE adding to list (prevents race condition)
    void* target = nullptr;
    bool success = false;
    
    if (type == TranslatedState::TARGET_XINPUT) {
        target = createVirtualXInputDeviceTarget(userId);
        success = (target != nullptr);
    } else {
        target = createVirtualDInputDeviceTarget(userId);
        success = (target != nullptr);
    }
    
    if (!success || target == nullptr) {
        // Failed to create target - don't add to list
        return -1;
    }
    
    // Only add to list after successful target creation
    VirtualDevice newDevice;
    newDevice.id = newId;
    newDevice.type = type;
    newDevice.userId = userId;
    newDevice.sourceName = sourceName;
    newDevice.connected = true;
    newDevice.lastUpdate = TimingUtils::getPerformanceCounter();
    newDevice.target = target;
    
    m_virtualDevices.push_back(newDevice);
    
    // Call callback if set
    if (m_deviceCallback) {
        m_deviceCallback(newId, true);
    }
    
    return newId;
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
    
    // If testing manually, trigger the callback for all active virtual devices
    if (m_rumbleCallback) {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (const auto& device : m_virtualDevices) {
            float power = enabled ? m_rumbleIntensity : 0.0f;
            m_rumbleCallback(device.userId, power, power);
        }
    }
}

void VirtualDeviceEmulator::setRumbleIntensity(float intensity) {
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;
    m_rumbleIntensity = intensity;
    
    // If testing is currently active, update intensity immediately
    if (m_rumbleEnabled && m_rumbleCallback) {
        std::lock_guard<std::mutex> lock(m_devicesMutex);
        for (const auto& device : m_virtualDevices) {
            m_rumbleCallback(device.userId, intensity, intensity);
        }
    }
}

void VirtualDeviceEmulator::setRumbleCallback(RumbleCallback callback) {
    m_rumbleCallback = callback;
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
    // Initializing virtual devices is now handled dynamically by the main loop
    // when physical controllers are detected. At startup, we just need to confirm
    // that we are ready to create them.
    return true;
}

void* VirtualDeviceEmulator::createVirtualXInputDeviceTarget(int userId) {
    if (!m_vigemClient) {
        return nullptr;
    }

    // Create a new Xbox 360 controller target
    PVIGEM_TARGET x360Target = vigem_target_x360_alloc();
    if (!x360Target) {
        m_lastError = "vigem_target_x360_alloc failed (Out of memory).";
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        return nullptr;
    }

    VIGEM_ERROR error = vigem_target_add(static_cast<PVIGEM_CLIENT>(m_vigemClient), x360Target);
    if (!VIGEM_SUCCESS(error)) {
        std::stringstream ss;
        ss << "vigem_target_add failed (Error 0x" << std::hex << error << std::dec << ")";
        m_lastError = ss.str();
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        vigem_target_free(x360Target);
        return nullptr;
    }

    // Register rumble notification
    vigem_target_x360_register_notification(
        static_cast<PVIGEM_CLIENT>(m_vigemClient),
        x360Target,
        VirtualDeviceEmulator::x360Notification,
        reinterpret_cast<LPVOID>(static_cast<uintptr_t>(userId))
    );

    Logger::log("VirtualDeviceEmulator: Created XInput device for userId " + std::to_string(userId));
    return static_cast<void*>(x360Target);
}

void* VirtualDeviceEmulator::createVirtualDInputDeviceTarget(int userId) {
    if (!m_vigemClient) {
        return nullptr;
    }

    // Create a new DualShock 4 controller target
    PVIGEM_TARGET ds4Target = vigem_target_ds4_alloc();
    if (!ds4Target) {
        m_lastError = "vigem_target_ds4_alloc failed (Out of memory).";
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        return nullptr;
    }

    VIGEM_ERROR error = vigem_target_add(static_cast<PVIGEM_CLIENT>(m_vigemClient), ds4Target);
    if (!VIGEM_SUCCESS(error)) {
        std::stringstream ss;
        ss << "vigem_target_add failed (Error 0x" << std::hex << error << std::dec << ")";
        m_lastError = ss.str();
        Logger::error("VirtualDeviceEmulator: " + m_lastError);
        vigem_target_free(ds4Target);
        return nullptr;
    }

    Logger::log("VirtualDeviceEmulator: Created DInput (DS4) device for userId " + std::to_string(userId));
    return static_cast<void*>(ds4Target);
}

bool VirtualDeviceEmulator::sendToVirtualXInputDevice(int userId, const XINPUT_STATE& state) {
    // Find the target for this userId
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = std::find_if(m_virtualDevices.begin(), m_virtualDevices.end(),
                          [userId](const VirtualDevice& device) {
                              return device.userId == userId && device.type == TranslatedState::TARGET_XINPUT;
                          });
    
    if (it == m_virtualDevices.end() || !it->target || !it->connected) {
        return false;
    }
    
    // Verify ViGEm client is still valid
    if (!m_vigemClient) {
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
    
    // If update fails, mark device as disconnected to prevent further crashes
    if (!VIGEM_SUCCESS(error)) {
        it->connected = false;
        Logger::log("WARNING: X360 update failed for userId " + std::to_string(userId) + ", error: 0x" + std::to_string(error));
        return false;
    }
    
    return true;
}

bool VirtualDeviceEmulator::sendToVirtualDInputDevice(int userId, const TranslationLayer::DInputState& state) {
    // Find the target for this userId
    std::lock_guard<std::mutex> lock(m_devicesMutex);
    auto it = std::find_if(m_virtualDevices.begin(), m_virtualDevices.end(),
                          [userId](const VirtualDevice& device) {
                              return device.userId == userId && device.type == TranslatedState::TARGET_DINPUT;
                          });
    
    if (it == m_virtualDevices.end() || !it->target || !it->connected) {
        return false;
    }
    
    // Verify ViGEm client is still valid
    if (!m_vigemClient) {
        return false;
    }
    
    // Create a DS4_REPORT from the DInputState
    DS4_REPORT report;
    DS4_REPORT_INIT(&report);
    
    // Map XInput buttons to DS4 buttons using proper DS4 button flags
    USHORT ds4Buttons = 0;
    if (state.wButtons & XINPUT_GAMEPAD_BACK) ds4Buttons |= DS4_BUTTON_SHARE;
    if (state.wButtons & XINPUT_GAMEPAD_START) ds4Buttons |= DS4_BUTTON_OPTIONS;
    if (state.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) ds4Buttons |= DS4_BUTTON_THUMB_LEFT;
    if (state.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) ds4Buttons |= DS4_BUTTON_THUMB_RIGHT;
    if (state.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ds4Buttons |= DS4_BUTTON_SHOULDER_LEFT;
    if (state.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ds4Buttons |= DS4_BUTTON_SHOULDER_RIGHT;
    if (state.wButtons & XINPUT_GAMEPAD_A) ds4Buttons |= DS4_BUTTON_CROSS;
    if (state.wButtons & XINPUT_GAMEPAD_B) ds4Buttons |= DS4_BUTTON_CIRCLE;
    if (state.wButtons & XINPUT_GAMEPAD_X) ds4Buttons |= DS4_BUTTON_SQUARE;
    if (state.wButtons & XINPUT_GAMEPAD_Y) ds4Buttons |= DS4_BUTTON_TRIANGLE;
    
    report.wButtons = ds4Buttons;
    report.bTriggerL = state.bLeftTrigger;
    report.bTriggerR = state.bRightTrigger;
    
    // Set trigger buttons based on trigger values
    if (state.bLeftTrigger > 0) report.wButtons |= DS4_BUTTON_TRIGGER_LEFT;
    if (state.bRightTrigger > 0) report.wButtons |= DS4_BUTTON_TRIGGER_RIGHT;
    
    // Map D-Pad using DS4_SET_DPAD macro
    if (state.wButtons & XINPUT_GAMEPAD_DPAD_UP && state.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTHEAST);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT && state.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTHEAST);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_DOWN && state.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTHWEST);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_LEFT && state.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTHWEST);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NORTH);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_EAST);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_SOUTH);
    } else if (state.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_WEST);
    } else {
        DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NONE);
    }
    
    // DS4 report uses BYTE (0-255) for sticks, not SHORT (-32768 to 32767)
    // We need to normalize our LONG input (likely -32768..32767 usually) to 0..255
    // DS4 Y-axis convention: 0 = up, 255 = down (inverted from XInput where positive = up)
    auto longToByte = [](LONG val) -> BYTE {
        return static_cast<BYTE>(TranslationLayer::normalizeLong(val) * 127.5f + 127.5f);
    };
    
    auto longToByteInverted = [](LONG val) -> BYTE {
        // Invert the Y-axis: positive becomes low value (up), negative becomes high value (down)
        return static_cast<BYTE>(127.5f - TranslationLayer::normalizeLong(val) * 127.5f);
    };

    report.bThumbLX = longToByte(state.lX);
    report.bThumbLY = longToByteInverted(state.lY);  // Invert Y-axis
    report.bThumbRX = longToByte(state.lRx);
    report.bThumbRY = longToByteInverted(state.lRy); // Invert Y-axis

    // Submit the report to ViGEmBus
    VIGEM_ERROR error = vigem_target_ds4_update(static_cast<PVIGEM_CLIENT>(m_vigemClient), static_cast<PVIGEM_TARGET>(it->target), report);

    // If update fails, mark device as disconnected to prevent further crashes
    if (!VIGEM_SUCCESS(error)) {
        it->connected = false;
        Logger::log("WARNING: DS4 update failed for userId " + std::to_string(userId) + ", error: 0x" + std::to_string(error));
        return false;
    }

    return true;
}

void VirtualDeviceEmulator::enableHidHideIntegration(bool enable) {
    if (m_hidHideEnabled == enable) return;

    m_hidHideEnabled = enable;
    if (enable) {
        if (!m_hidHideController) {
            m_hidHideController = std::make_unique<HidHideController>();
        }
        connectHidHide();
    } else {
        if (m_hidHideController) {
            disconnectHidHide();
        }
    }
}

bool VirtualDeviceEmulator::connectHidHide() {
    if (!m_hidHideEnabled || !m_hidHideController) {
        return false;
    }
    
    bool result = m_hidHideController->connect();
    if (result) {
        Logger::log("Successfully connected to HidHide driver");
    } else {
        Logger::error("Failed to connect to HidHide driver. Is HidHide installed and running?");
    }
    return result;
}

void VirtualDeviceEmulator::disconnectHidHide() {
    if (m_hidHideController) {
        m_hidHideController->disconnect();
        Logger::log("Disconnected from HidHide driver");
    }
}

bool VirtualDeviceEmulator::addPhysicalDeviceToHidHideBlacklist(const std::wstring& deviceInstanceId) {
    if (!m_hidHideEnabled || !m_hidHideController) {
        return false;
    }
    
    bool result = m_hidHideController->addDeviceToBlacklist(deviceInstanceId);
    
    // Convert wide string to narrow string for logging explicitly to avoid C4244
    std::string narrowDeviceId;
    for (wchar_t wc : deviceInstanceId) {
        narrowDeviceId += static_cast<char>(wc);
    }

    if (result) {
        Logger::log("Added device to HidHide blacklist: " + narrowDeviceId);
    } else {
        Logger::error("Failed to add device to HidHide blacklist: " + narrowDeviceId);
    }
    return result;
}

bool VirtualDeviceEmulator::removePhysicalDeviceFromHidHideBlacklist(const std::wstring& deviceInstanceId) {
    if (!m_hidHideEnabled || !m_hidHideController) {
        return false;
    }
    
    bool result = m_hidHideController->removeDeviceFromBlacklist(deviceInstanceId);
    
    // Convert wide string to narrow string for logging explicitly to avoid C4244
    std::string narrowDeviceId;
    for (wchar_t wc : deviceInstanceId) {
        narrowDeviceId += static_cast<char>(wc);
    }

    if (result) {
        Logger::log("Removed device from HidHide blacklist: " + narrowDeviceId);
    } else {
        Logger::error("Failed to remove device from HidHide blacklist: " + narrowDeviceId);
    }
    return result;
}