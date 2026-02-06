#include "core/input_capture.hpp"
#include "utils/timing.hpp"

#include <thread>
#include <algorithm>
#include <sstream>
#include <objbase.h>
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>

InputCapture::InputCapture() 
    : m_running(false), m_lastPollTime(0) {
    // Initialize COM for HID operations
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

InputCapture::~InputCapture() {
    shutdown();
    CoUninitialize();
}

bool InputCapture::initialize() {
    if (!initializeXInput()) {
        return false;
    }
    
    if (!initializeHID()) {
        return false;
    }
    
    // Start polling thread with high priority
    m_running = true;
    m_pollingThread = std::make_unique<std::thread>([this]() {
        // Set thread priority to highest for real-time input processing
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
        
        while (m_running) {
            update(0.0); // Delta time will be calculated internally
            // Small sleep to prevent 100% CPU usage in case of issues
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });
    
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
        return false;
    }
    
    // Pre-populate controller states for XInput controllers
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
        XINPUT_STATE initialState;
        DWORD res = XInputGetState(i, &initialState);
        
        ControllerState ctrlState{};
        ctrlState.userId = static_cast<int>(i);
        ctrlState.xinputState = initialState;
        ctrlState.isConnected = (res == ERROR_SUCCESS);
        ctrlState.timestamp = TimingUtils::getPerformanceCounter();
        
        {
            std::lock_guard<std::mutex> lock(m_statesMutex);
            m_controllerStates.push_back(ctrlState);
        }
    }
    
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
        if (SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &deviceInterfaceData, deviceDetailData, requiredSize, nullptr, nullptr)) {
            newDevicePaths.push_back(deviceDetailData->DevicePath);
            
            // Check if this device is already in our states list
            bool found = false;
            {
                std::lock_guard<std::mutex> lock(m_statesMutex);
                for (auto& state : m_controllerStates) {
                    if (state.devicePath == deviceDetailData->DevicePath) {
                        found = true;
                        break;
                    }
                }
                
                if (!found) {
                    // Add new controller state for this HID device
                    ControllerState newState{};
                    newState.devicePath = deviceDetailData->DevicePath;
                    newState.isConnected = true;
                    newState.timestamp = TimingUtils::getPerformanceCounter();
                    
                    // Attempt to open the device
                    HANDLE deviceHandle = CreateFileW(
                        deviceDetailData->DevicePath,
                        GENERIC_READ | GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr,
                        OPEN_EXISTING,
                        0,
                        nullptr
                    );
                    
                    if (deviceHandle != INVALID_HANDLE_VALUE) {
                        newState.hidHandle = deviceHandle;
                        newState.isConnected = true;
                    } else {
                        newState.isConnected = false;
                    }
                    
                    m_controllerStates.push_back(newState);
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
                m_controllerStates[i].xinputState = state;
                m_controllerStates[i].xinputPacketNumber = state.dwPacketNumber;
                m_controllerStates[i].isConnected = (result == ERROR_SUCCESS);
                m_controllerStates[i].timestamp = TimingUtils::getPerformanceCounter();
            }
        }
    }
}

void InputCapture::pollHIDControllers() {
    std::lock_guard<std::mutex> lock(m_statesMutex);
    
    for (auto& state : m_controllerStates) {
        if (state.hidHandle != INVALID_HANDLE_VALUE && state.hidHandle != nullptr) {
            // Prepare buffer for HID input report
            BYTE buffer[256];
            ULONG bytesRead = 0;
            
            // Try to read the input report
            BOOL success = ReadFile(state.hidHandle, buffer, sizeof(buffer), reinterpret_cast<LPDWORD>(&bytesRead), nullptr);
            
            if (success && bytesRead > 0) {
                state.isConnected = true;
                state.timestamp = TimingUtils::getPerformanceCounter();
                
                // Parse the HID report based on the device's capabilities
                // This is a simplified approach - in a real implementation, 
                // we'd need to parse the device's report descriptor
                if (bytesRead >= sizeof(XINPUT_GAMEPAD)) {
                    // Map HID report to gamepad structure
                    // This is a simplified mapping - actual implementation would need 
                    // device-specific parsing based on report descriptor
                    memcpy(&state.gamepad, buffer, std::min(sizeof(state.gamepad), static_cast<size_t>(bytesRead)));
                }
            } else {
                // Check if device is still connected by trying to get attributes
                HIDD_ATTRIBUTES attributes;
                attributes.Size = sizeof(HIDD_ATTRIBUTES);
                if (!HidD_GetAttributes(state.hidHandle, &attributes)) {
                    state.isConnected = false;
                    CloseHandle(state.hidHandle);
                    state.hidHandle = INVALID_HANDLE_VALUE;
                }
            }
        }
    }
}