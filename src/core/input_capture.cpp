#include "core/input_capture.hpp"
#include "utils/timing.hpp"

#include <thread>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <objbase.h>
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <cfgmgr32.h>

InputCapture::InputCapture() 
    : m_running(false), 
      m_lastPollTime(0),
      m_loggingEnabled(false),
      m_logFilePath("controller_input_log.csv"),
      m_logStartTime(0),
      m_logSampleCount(0) {
    // Initialize COM for HID operations
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

InputCapture::~InputCapture() {
    shutdown();
    
    // Close log file if open
    if (m_logFile.is_open()) {
        m_logFile.close();
        Logger::log("Input logging stopped. Total samples: " + std::to_string(m_logSampleCount));
    }
    
    CoUninitialize();
}

bool InputCapture::initialize() {
    if (!initializeXInput()) {
        std::cerr << "InputCapture: initializeXInput failed" << std::endl;
        return false;
    }
    
    if (!initializeHID()) {
        Logger::error("InputCapture: initializeHID failed");
        return false;
    }

    Logger::log("InputCapture: Initialized with " + std::to_string(m_controllerStates.size()) + " controller slots.");
    
    // Start polling thread with high priority
    m_running = true;
    /*
    m_pollingThread = std::make_unique<std::thread>([this]() {
        // Set thread priority to highest for real-time input processing
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        
        while (m_running) {
            update(0.0); // Delta time will be calculated internally
            // Small sleep to prevent 100% CPU usage in case of issues
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    */
    
    return true;
}

void InputCapture::shutdown() {
    m_running = false;
    
    if (m_pollingThread && m_pollingThread->joinable()) {
        m_pollingThread->join();
    }
}

void InputCapture::update(double deltaTime) {
    pollXInputControllers();
    pollHIDControllers();
    
    // Log input states if logging is enabled
    if (m_loggingEnabled) {
        std::lock_guard<std::mutex> lock(m_statesMutex);
        for (const auto& state : m_controllerStates) {
            if (state.isConnected) {
                logInputState(state);
            }
        }
    }
    
    m_lastPollTime = TimingUtils::getPerformanceCounter();
}

std::vector<ControllerState> InputCapture::getInputStates() const {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    return m_controllerStates;
}

void InputCapture::refreshDevices() {
    // Refresh HID device list
    initializeHID();
}

int InputCapture::getConnectedDeviceCount() const {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    return static_cast<int>(m_controllerStates.size());
}

bool InputCapture::initializeXInput() {
    // Test XInput availability by querying the first controller
    XINPUT_STATE state;
    DWORD result = XInputGetState(0, &state);

    // Even if no controller is connected, XInput is available if we don't get ERROR_DEVICE_NOT_CONNECTED
    if (result != ERROR_SUCCESS && result != ERROR_DEVICE_NOT_CONNECTED) {
        std::cerr << "InputCapture: XInput not available. Error code: " << result << std::endl;
        return false;
    }
    
    // Pre-populate controller states for XInput controllers
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE initialState;
        DWORD res = XInputGetState(i, &initialState);
        
        ControllerState ctrlState{};
        ctrlState.userId = static_cast<int>(i);
        ctrlState.xinputState = initialState;
        ctrlState.isConnected = false; // Initially disconnected until matched with verified HID
        ctrlState.lastError = res;
        ctrlState.timestamp = TimingUtils::getPerformanceCounter();
        
        {
            std::lock_guard<std::mutex> lock(m_statesMutex);
            m_controllerStates.push_back(ctrlState);
        }
    }
    
    Logger::log("InputCapture: XInput initialized.");
    return true;
}

bool InputCapture::initializeHID() {
    // Enumerate HID devices
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    std::vector<std::wstring> newDevicePaths;
    
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &hidGuid, i, &deviceInterfaceData); ++i) {
        // Get required buffer size
        SP_DEVICE_INTERFACE_DETAIL_DATA_W* deviceDetailData = nullptr;
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr);
        
        if (requiredSize == 0) continue;
        
        // Allocate buffer
        deviceDetailData = static_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(malloc(requiredSize));
        deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        
        // Get device path
        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        if (SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &deviceInterfaceData, deviceDetailData, requiredSize, nullptr, &devInfoData)) {
            std::wstring devicePath = deviceDetailData->DevicePath;
            newDevicePaths.push_back(devicePath);
            
            // Get Device Instance ID
            wchar_t instanceId[MAX_DEVICE_ID_LEN];
            std::wstring actualInstanceId;
            if (SetupDiGetDeviceInstanceIdW(deviceInfoSet, &devInfoData, instanceId, MAX_DEVICE_ID_LEN, nullptr)) {
                actualInstanceId = instanceId;
            }

            // Diagnostic Logging
            Logger::log("InputCapture: Enumerating HID Device. Instance ID: " + Logger::wstringToNarrow(actualInstanceId));

            // FILTER: Ignore ViGEm virtual devices to prevent feedback loops
            // Multiple detection methods for robustness:
            // 1. Check DEVPKEY_Device_UINumber property (ViGEm devices have this set)
            // 2. Check device instance ID for known ViGEm patterns
            // 3. Fallback: Allow device if property check fails (avoid false positives)
            bool isVirtualViGEm = false;
            
            // Method 1: Check for ViGEm patterns in instance ID (fast, reliable)
            if (actualInstanceId.find(L"VID_044F&PID_B326") != std::wstring::npos ||  // ViGEm Xbox 360
                actualInstanceId.find(L"VID_054C&PID_05C4") != std::wstring::npos) {  // ViGEm DS4
                isVirtualViGEm = true;
                Logger::log("InputCapture: Blocked virtual ViGEm device (pattern match): " + Logger::wstringToNarrow(actualInstanceId));
            }
            
            // Method 2: Check DEVPKEY_Device_UINumber property (more thorough)
            if (!isVirtualViGEm) {
                HDEVINFO propDeviceInfoSet = SetupDiGetClassDevsW(&hidGuid, actualInstanceId.c_str(), 0, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
                if (propDeviceInfoSet != INVALID_HANDLE_VALUE) {
                    SP_DEVINFO_DATA devInfoData = {sizeof(SP_DEVINFO_DATA)};
                    if (SetupDiEnumDeviceInfo(propDeviceInfoSet, 0, &devInfoData)) {
                        // Check DEVPKEY_Device_UINumber property
                        DEVPROPKEY uiNumberKey = {
                            {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}},
                            18  // pid for UINumber
                        };
                        
                        DEVPROPTYPE propertyType;
                        BYTE buffer[256];
                        DWORD requiredSize = 0;
                        
                        if (SetupDiGetDevicePropertyW(propDeviceInfoSet, &devInfoData, &uiNumberKey,
                            &propertyType, buffer, sizeof(buffer), &requiredSize, 0)) {
                            // Property exists and has a value - likely a virtual device
                            // But only block if we also see ViGEm-like characteristics
                            isVirtualViGEm = true;
                            Logger::log("InputCapture: Blocked virtual ViGEm device (property check): " + Logger::wstringToNarrow(actualInstanceId));
                        }
                    }
                    SetupDiDestroyDeviceInfoList(propDeviceInfoSet);
                }
                // If property check fails, assume it's a real device (avoid false positives)
            }

            if (isVirtualViGEm) {
                free(deviceDetailData);
                continue; 
            }

            // Check if this device is already in our states list
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(m_statesMutex);
                
                // Identify if this is an XInput device
                bool isXInput = (devicePath.find(L"IG_") != std::wstring::npos || 
                                 actualInstanceId.find(L"IG_") != std::wstring::npos);

                // Match by Instance ID (Stable) instead of Device Path (Transient)
                for (auto& state : m_controllerStates) {
                    if (!state.deviceInstanceId.empty() && state.deviceInstanceId == actualInstanceId) {
                        state.devicePath = devicePath; // Update path in case it changed
                        state.isConnected = true; 
                        found = true;
                        break;
                    }
                }
                
                if (!found && isXInput) {
                    // Extract VID/PID only (before the backslash and &IG_ suffix)
                    // Example: HID\VID_045E&PID_028E&IG_01\8&F746FFA&0&0000 -> HID\VID_045E&PID_028E
                    // Example: HID\VID_045E&PID_028E&IG_03\3&29329EE&0&0000 -> HID\VID_045E&PID_028E
                    std::wstring baseDeviceId = actualInstanceId;
                    
                    // First, get everything before the backslash (removes serial number)
                    size_t backslashPos = baseDeviceId.find(L'\\');
                    if (backslashPos != std::wstring::npos) {
                        baseDeviceId = baseDeviceId.substr(0, backslashPos);
                    }
                    
                    // Then remove &IG_XX suffix
                    size_t igPos = baseDeviceId.find(L"&IG_");
                    if (igPos != std::wstring::npos) {
                        baseDeviceId = baseDeviceId.substr(0, igPos);
                    }
                    
                    // Validate that we extracted a non-empty base ID
                    if (baseDeviceId.empty()) {
                        Logger::log("InputCapture: Warning - Failed to extract base device ID from: " + Logger::wstringToNarrow(actualInstanceId));
                        found = true; // Skip this device to avoid issues
                        continue;
                    }
                    
                    // Check if we've already assigned ANY interface from this physical device
                    bool alreadyAssigned = false;
                    for (const auto& state : m_controllerStates) {
                        if (state.userId >= 0 && !state.deviceInstanceId.empty()) {
                            std::wstring existingBaseId = state.deviceInstanceId;
                            
                            // Extract VID/PID from existing device
                            size_t existingBackslashPos = existingBaseId.find(L'\\');
                            if (existingBackslashPos != std::wstring::npos) {
                                existingBaseId = existingBaseId.substr(0, existingBackslashPos);
                            }
                            
                            size_t existingIgPos = existingBaseId.find(L"&IG_");
                            if (existingIgPos != std::wstring::npos) {
                                existingBaseId = existingBaseId.substr(0, existingIgPos);
                            }
                            
                            // Validate extracted ID before comparison
                            if (existingBaseId.empty()) {
                                continue; // Skip invalid entries
                            }
                            
                            if (existingBaseId == baseDeviceId) {
                                alreadyAssigned = true;
                                found = true; // Mark as found so we don't add it again
                                break;
                            }
                        }
                    }
                    
                    // Only assign if this physical device hasn't been assigned yet
                    if (!alreadyAssigned) {
                        // Try to assign this Instance ID to an XInput slot (User 0-3)
                        for (auto& state : m_controllerStates) {
                            if (state.userId >= 0 && state.deviceInstanceId.empty()) {
                                state.deviceInstanceId = actualInstanceId;
                                state.devicePath = devicePath;
                                state.isConnected = true; // MARK CONNECTED ONLY WHEN MATCHED
                                
                                // Also try to get the product name for this XInput device
                                HANDLE h = CreateFileW(devicePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
                                if (h != INVALID_HANDLE_VALUE) {
                                    wchar_t productBuffer[128];
                                    if (HidD_GetProductString(h, productBuffer, sizeof(productBuffer))) {
                                        state.productName = productBuffer;
                                    }
                                    CloseHandle(h);
                                }
                                
                                Logger::log("InputCapture: Matched XInput device to User " + std::to_string(state.userId) + ": " + Logger::wstringToNarrow(state.productName));
                                
                                found = true;
                                break;
                            }
                        }
                    }
                }

                if (!found) {
                    // One last check: Ensure this device isn't already added as a generic HID
                    for (auto& state : m_controllerStates) {
                        if (state.userId < 0 && state.deviceInstanceId == actualInstanceId) {
                            state.devicePath = devicePath;
                            state.isConnected = true;
                            found = true;
                            break;
                        }
                    }
                }

                if (!found) {
                    // Add new controller state for this HID device
                    ControllerState newState{};
                    newState.userId = -1; // HID device
                    newState.devicePath = devicePath;
                    newState.deviceInstanceId = actualInstanceId;
                    newState.isConnected = true;
                    newState.timestamp = TimingUtils::getPerformanceCounter();
                    
                    // Attempt to open the device
                    HANDLE deviceHandle = CreateFileW(
                        deviceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, // Use Overlapped I/O for non-blocking reads
                        nullptr
                    );
                    
                    if (deviceHandle != INVALID_HANDLE_VALUE) {
                        newState.hidHandle = deviceHandle;
                        newState.isConnected = true;
                        
                        // Initialize Overlapped structure
                        memset(&newState.overlapped, 0, sizeof(OVERLAPPED));
                        newState.overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
                        newState.isReadPending = false;
                        
                        // Get Product Name
                        wchar_t productBuffer[128];
                        if (HidD_GetProductString(deviceHandle, productBuffer, sizeof(productBuffer))) {
                            newState.productName = productBuffer;
                        } else {
                            newState.productName = L"Unknown HID Device";
                        }
                        
                        // Get Preparsed Data
                        PHIDP_PREPARSED_DATA preparsedData;
                        if (HidD_GetPreparsedData(deviceHandle, &preparsedData)) {
                             newState.preparsedData = preparsedData;
                             HidP_GetCaps(preparsedData, &newState.caps);
                             
                             // Get Button Caps
                             USHORT capsLength = newState.caps.NumberInputButtonCaps;
                             if (capsLength > 0) {
                                 newState.buttonCaps.resize(capsLength);
                                 HidP_GetButtonCaps(HidP_Input, newState.buttonCaps.data(), &capsLength, preparsedData);
                             }
                             
                             // Get Value Caps (Axes)
                             capsLength = newState.caps.NumberInputValueCaps;
                             if (capsLength > 0) {
                                 newState.valueCaps.resize(capsLength);
                                 HidP_GetValueCaps(HidP_Input, newState.valueCaps.data(), &capsLength, preparsedData);
                             }
                             
                             // Diagnostic Logging
                             HIDD_ATTRIBUTES attributes;
                             attributes.Size = sizeof(HIDD_ATTRIBUTES);
                             if (HidD_GetAttributes(deviceHandle, &attributes)) {
                                 std::stringstream ss;
                                 ss << "InputCapture: HID Attributes - VendorID: 0x" << std::hex << attributes.VendorID 
                                    << ", ProductID: 0x" << attributes.ProductID 
                                    << ", Version: 0x" << attributes.VersionNumber << std::dec;
                                 Logger::log(ss.str());
                             }
                             
                             std::stringstream ss;
                             ss << "InputCapture: HID Capabilities - UsagePage: 0x" << std::hex << newState.caps.UsagePage 
                                << ", Usage: 0x" << newState.caps.Usage << std::dec
                                << ", Buttons: " << newState.caps.NumberInputButtonCaps
                                << ", Axes: " << newState.caps.NumberInputValueCaps;
                             Logger::log(ss.str());

                             // Filter for Gamepads/Joysticks only
                             // Usage Page 0x01 (Generic Desktop), Usage 0x04 (Joystick) or 0x05 (Gamepad)
                             if (newState.caps.UsagePage == 0x01 && (newState.caps.Usage == 0x04 || newState.caps.Usage == 0x05)) {
                                 std::wstring pName = newState.productName;
                                 std::string pNameStr;
                                 for (wchar_t wc : pName) {
                                     pNameStr += static_cast<char>(wc);
                                 }
                                 
                                 // Check if this device is already in the list (by deviceInstanceId)
                                 bool alreadyExists = false;
                                 {
                                     std::lock_guard<std::mutex> lock(m_statesMutex);
                                     for (const auto& existing : m_controllerStates) {
                                         if (existing.deviceInstanceId == newState.deviceInstanceId && !newState.deviceInstanceId.empty()) {
                                             alreadyExists = true;
                                             break;
                                         }
                                     }
                                 }
                                 
                                 if (!alreadyExists) {
                                     Logger::log("InputCapture: HID Device Found: " + pNameStr);
                                     std::lock_guard<std::mutex> lock(m_statesMutex);
                                     m_controllerStates.push_back(newState);
                                 } else {
                                     // Device already exists, close handles
                                     if (newState.overlapped.hEvent) CloseHandle(newState.overlapped.hEvent);
                                     CloseHandle(newState.hidHandle);
                                     if (newState.preparsedData) HidD_FreePreparsedData(newState.preparsedData);
                                 }
                             } else {
                                 // Close handle if not a relevant device
                                 if (newState.overlapped.hEvent) CloseHandle(newState.overlapped.hEvent);
                                 CloseHandle(newState.hidHandle);
                                 if (newState.preparsedData) HidD_FreePreparsedData(newState.preparsedData);
                             }
                        } else {
                            // Failed to get preparsed data
                             if (newState.overlapped.hEvent) CloseHandle(newState.overlapped.hEvent);
                             CloseHandle(newState.hidHandle);
                        }
                    } else {
                        // Failed to open device
                        // newState.isConnected = false;
                    }
                }
            }
        }
        
        free(deviceDetailData);
    }
    
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    
    m_hidDevicePaths = newDevicePaths;
    return true;
}

void InputCapture::pollXInputControllers() {
    for (size_t i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(static_cast<DWORD>(i), &state);
        
        {
            std::lock_guard<std::mutex> lock(m_statesMutex);
            if (i < m_controllerStates.size()) {
                // Only process XInput data if this slot has been matched to a physical HID device
                // This prevents duplicate interfaces from the same controller filling multiple slots
                if (!m_controllerStates[i].deviceInstanceId.empty()) {
                    // Log first successful poll for debugging
                    static bool firstPollLogged[XUSER_MAX_COUNT] = {false, false, false, false};
                    if (result == ERROR_SUCCESS && !firstPollLogged[i]) {
                        Logger::log("InputCapture: First successful XInput poll for User " + std::to_string(i) + 
                                   ", PacketNumber: " + std::to_string(state.dwPacketNumber));
                        firstPollLogged[i] = true;
                    }
                    
                    m_controllerStates[i].xinputState = state;
                    m_controllerStates[i].xinputPacketNumber = state.dwPacketNumber;
                    
                    // Mark as connected only if XInput poll succeeds AND device is matched
                    if (result == ERROR_SUCCESS) {
                        m_controllerStates[i].isConnected = true;
                    } else {
                        m_controllerStates[i].isConnected = false;
                        m_controllerStates[i].deviceInstanceId = L""; // Clear so it can be re-matched
                    }
                    m_controllerStates[i].lastError = result;
                } else {
                    // No HID device matched to this XInput slot - mark as disconnected
                    m_controllerStates[i].isConnected = false;
                    m_controllerStates[i].lastError = ERROR_DEVICE_NOT_CONNECTED;
                }
                
                m_controllerStates[i].timestamp = TimingUtils::getPerformanceCounter();
            }
        }
    }
}

void InputCapture::pollHIDControllers() {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    
    for (auto& state : m_controllerStates) {
        if (state.hidHandle != INVALID_HANDLE_VALUE && state.hidHandle != nullptr && state.userId < 0) { // Only poll HID, not pure XInput
            
            // If no read is pending, start one
            if (!state.isReadPending) {
                // Reset event
                ResetEvent(state.overlapped.hEvent);
                
                DWORD bytesRead = 0;
                BOOL success = ReadFile(state.hidHandle, state.inputBuffer, sizeof(state.inputBuffer), &bytesRead, &state.overlapped);
                
                if (success) {
                    // Immediate success (cached data?)
                    state.isConnected = true;
                    state.timestamp = TimingUtils::getPerformanceCounter();
                    parseHIDReport(state, reinterpret_cast<PCHAR>(state.inputBuffer), bytesRead);
                } else {
                    DWORD error = GetLastError();
                    if (error == ERROR_IO_PENDING) {
                        state.isReadPending = true;
                    } else {
                        // Read failed
                        // Check availability or ignore
                        if (error == ERROR_DEVICE_NOT_CONNECTED) {
                             state.isConnected = false;
                             state.lastError = error;
                        }
                    }
                }
            } else {
                // Read IS pending, check if it completed
                DWORD bytesTransferred = 0;
                if (GetOverlappedResult(state.hidHandle, &state.overlapped, &bytesTransferred, FALSE)) {
                    // Completed!
                    state.isReadPending = false;
                    state.isConnected = true;
                    state.timestamp = TimingUtils::getPerformanceCounter();
                    
                    if (bytesTransferred > 0) {
                        parseHIDReport(state, reinterpret_cast<PCHAR>(state.inputBuffer), bytesTransferred);
                    }
                } else {
                    // Not done or error
                    DWORD error = GetLastError();
                    if (error == ERROR_IO_INCOMPLETE) {
                        // Still pending - this is normal, continue waiting
                        // Don't mark as disconnected
                    } else if (error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_BAD_COMMAND) {
                        // Device actually disconnected
                        state.isReadPending = false;
                        state.isConnected = false;
                        state.lastError = error;
                    } else {
                        // Other transient error - retry on next poll
                        state.isReadPending = false;
                        // Don't mark as disconnected for transient errors
                        state.lastError = error;
                    }
                }
            }
        }
    }
}

void InputCapture::parseHIDReport(ControllerState& state, PCHAR report, ULONG reportLength) {
    if (!state.preparsedData) return;

    HIDP_CAPS caps = state.caps;
    
    // Parse Buttons
    getHIDUsages(state, report, reportLength);

    // Parse Values (Axes)
    getHIDValues(state, report, reportLength);
}

void InputCapture::getHIDUsages(ControllerState& state, PCHAR report, ULONG reportLength) {
    ULONG usageLength = state.caps.NumberInputButtonCaps;
    if (usageLength == 0) return;

    std::vector<USAGE> usages(usageLength);
    NTSTATUS status = HidP_GetUsages(
        HidP_Input, 
        state.caps.UsagePage, 
        0, 
        usages.data(), 
        &usageLength, 
        state.preparsedData, 
        report, 
        reportLength
    );

    if (status == HIDP_STATUS_SUCCESS) {
        state.m_activeButtons.clear();
        state.m_activeButtons.assign(usages.begin(), usages.begin() + usageLength);
    }
}

void InputCapture::getHIDValues(ControllerState& state, PCHAR report, ULONG reportLength) {
    for (const auto& cap : state.valueCaps) {
        ULONG value;
        NTSTATUS status = HidP_GetUsageValue(
            HidP_Input, 
            cap.UsagePage, 
            0, 
            cap.Range.UsageMin, 
            &value, 
            state.preparsedData, 
            report, 
            reportLength
        );
        
        if (status == HIDP_STATUS_SUCCESS) {
             state.m_hidValues[cap.Range.UsageMin] = static_cast<LONG>(value);
             // Map axes generic desktop page
             if (cap.UsagePage == 0x01) {
                 switch (cap.Range.UsageMin) {
                     case 0x30: // X
                        state.gamepad.sThumbLX = static_cast<SHORT>(value - 32768); 
                        break;
                     case 0x31: // Y
                        state.gamepad.sThumbLY = static_cast<SHORT>(32768 - value); // Invert Y
                        break;
                     case 0x32: // Z (often Right X or Trigger)
                        state.gamepad.sThumbRX = static_cast<SHORT>(value - 32768);
                        break;
                     case 0x35: // Rz (often Right Y)
                        state.gamepad.sThumbRY = static_cast<SHORT>(32768 - value);
                        break;
                 }
             }
        }
    }
}

void InputCapture::setVibration(int userId, float leftMotor, float rightMotor) {
    if (userId < 0 || userId > 3) return;
    
    XINPUT_VIBRATION vibration;
    vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535);
    vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535);
    XInputSetState(userId, &vibration);
}

std::wstring InputCapture::extractDeviceInstanceId(const std::wstring& devicePath) {
    // Extract the device instance ID from the device path
    // Device paths typically look like: \\?\HID#VID_XXXX&PID_XXXX#XXXXXXXX#{...}
    size_t start = devicePath.find(L"HID#");
    if (start != std::wstring::npos) {
        size_t end = devicePath.find(L'#', start + 4);
        if (end != std::wstring::npos) {
            size_t nextEnd = devicePath.find(L'#', end + 1);
            if (nextEnd != std::wstring::npos) {
                return devicePath.substr(start, nextEnd - start);
            }
        }
    }
    return L"";
}

void InputCapture::enableInputLogging(bool enabled) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    
    if (enabled && !m_loggingEnabled) {
        // Start logging
        m_logFile.open(m_logFilePath, std::ios::out | std::ios::trunc);
        if (!m_logFile.is_open()) {
            Logger::error("Failed to open log file: " + m_logFilePath);
            return;
        }
        
        // Write CSV header
        m_logFile << "Timestamp_ms,Controller_ID,Controller_Name,";
        m_logFile << "LX_Raw,LY_Raw,RX_Raw,RY_Raw,";
        m_logFile << "LX_Normalized,LY_Normalized,RX_Normalized,RY_Normalized,";
        m_logFile << "LT,RT,Buttons_Hex,";
        m_logFile << "Packet_Number,Is_Connected,Error_Code\n";
        
        m_logStartTime = TimingUtils::getPerformanceCounter();
        m_logSampleCount = 0;
        m_loggingEnabled = true;
        
        Logger::log("Input logging started: " + m_logFilePath);
    } else if (!enabled && m_loggingEnabled) {
        // Stop logging
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
        m_loggingEnabled = false;
        
        Logger::log("Input logging stopped. Total samples: " + std::to_string(m_logSampleCount));
    }
}

void InputCapture::setLogFilePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    
    if (m_loggingEnabled) {
        Logger::error("Cannot change log file path while logging is active");
        return;
    }
    
    m_logFilePath = path;
}

void InputCapture::logInputState(const ControllerState& state) {
    if (!m_loggingEnabled || !m_logFile.is_open()) {
        return;
    }
    
    // Calculate elapsed time in milliseconds
    uint64_t currentTime = TimingUtils::getPerformanceCounter();
    double elapsedMs = TimingUtils::counterToMicroseconds(currentTime - m_logStartTime) / 1000.0;
    
    // Convert controller name from wstring to string
    std::string controllerName;
    for (wchar_t wc : state.productName) {
        controllerName += static_cast<char>(wc);
    }
    if (controllerName.empty()) {
        controllerName = (state.userId >= 0) ? "XInput_Controller" : "HID_Device";
    }
    
    // Get stick values
    SHORT lx = state.xinputState.Gamepad.sThumbLX;
    SHORT ly = state.xinputState.Gamepad.sThumbLY;
    SHORT rx = state.xinputState.Gamepad.sThumbRX;
    SHORT ry = state.xinputState.Gamepad.sThumbRY;
    
    // Normalize to -1.0 to 1.0 range
    float lx_norm = lx / 32767.0f;
    float ly_norm = ly / 32767.0f;
    float rx_norm = rx / 32767.0f;
    float ry_norm = ry / 32767.0f;
    
    // Write CSV row
    m_logFile << std::fixed << std::setprecision(3) << elapsedMs << ",";
    m_logFile << state.userId << ",";
    m_logFile << "\"" << controllerName << "\",";
    m_logFile << lx << "," << ly << "," << rx << "," << ry << ",";
    m_logFile << std::setprecision(6) << lx_norm << "," << ly_norm << "," << rx_norm << "," << ry_norm << ",";
    m_logFile << static_cast<int>(state.xinputState.Gamepad.bLeftTrigger) << ",";
    m_logFile << static_cast<int>(state.xinputState.Gamepad.bRightTrigger) << ",";
    m_logFile << "0x" << std::hex << state.xinputState.Gamepad.wButtons << std::dec << ",";
    m_logFile << state.xinputState.dwPacketNumber << ",";
    m_logFile << (state.isConnected ? "1" : "0") << ",";
    m_logFile << state.lastError << "\n";
    
    m_logSampleCount++;
    
    // Flush every 100 samples to ensure data is written
    if (m_logSampleCount % 100 == 0) {
        m_logFile.flush();
    }
}
