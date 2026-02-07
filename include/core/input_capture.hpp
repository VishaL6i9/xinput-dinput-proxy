#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include "utils/logger.hpp"

// Windows headers
#include <windows.h>
#include <xinput.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <guiddef.h>

// GameInput API (for Windows 11)
#ifdef __cplusplus_winrt
#include <windows.gaming.input.h>
#endif

// Custom types
struct ControllerState {
    int userId;  // For XInput (0-3)
    DWORD xinputPacketNumber;
    XINPUT_STATE xinputState;
    
    // HID device info
    HANDLE hidHandle;
    std::wstring devicePath;
    std::wstring deviceInstanceId; // Unique system ID for HidHide
    std::wstring productName; // Friendly name
    bool isConnected;
    DWORD lastError; // Store API error code for debugging
    
    // Raw HID Data (Captured during poll)
    std::vector<USAGE> m_activeButtons;
    std::unordered_map<USAGE, LONG> m_hidValues;
    
    // For XInput source, we still keep the xinputState above.
    // XInput state is already captured in xinputState.Gamepad.
    
    // Processed state (maintained for backward compatibility if needed, 
    // but primary translation happens in TranslationLayer)
    struct {
        WORD wButtons;
        BYTE bLeftTrigger;
        BYTE bRightTrigger;
        SHORT sThumbLX;
        SHORT sThumbLY;
        SHORT sThumbRX;
        SHORT sThumbRY;
    } gamepad;
    
    // HID Data
    PHIDP_PREPARSED_DATA preparsedData;
    HIDP_CAPS caps;
    std::vector<HIDP_BUTTON_CAPS> buttonCaps;
    std::vector<HIDP_VALUE_CAPS> valueCaps;
    
    // Async I/O
    OVERLAPPED overlapped;
    bool isReadPending;
    static constexpr size_t INPUT_BUFFER_SIZE = 512; // Increased from 256 for exotic devices
    BYTE inputBuffer[INPUT_BUFFER_SIZE];
    
    // Timing
    uint64_t timestamp;
};

class InputCapture {
public:
    InputCapture();
    ~InputCapture();
    
    bool initialize();
    void shutdown();
    
    void update(double deltaTime);
    std::vector<ControllerState> getInputStates() const;
    
    // Device management
    void refreshDevices();
    int getConnectedDeviceCount() const;
    
    // Thread-safe state access
    void lockStates() const { m_statesMutex.lock(); }
    void unlockStates() const { m_statesMutex.unlock(); }
    
    // Output control
    void setVibration(int userId, float leftMotor, float rightMotor);

    // Debugging
    // void getStartupLogs() removed in favor of unified Logger

    // Device instance ID extraction
    static std::wstring extractDeviceInstanceId(const std::wstring& devicePath);

private:
    bool initializeXInput();
    bool initializeHID();
    void pollXInputControllers();
    void pollHIDControllers();

    mutable std::mutex m_statesMutex;
    std::vector<ControllerState> m_controllerStates;

    std::atomic<bool> m_running;
    std::unique_ptr<std::thread> m_pollingThread;

    // Timing
    uint64_t m_lastPollTime;

    // Device enumeration
    std::vector<std::wstring> m_hidDevicePaths;

    // HID Parsing helpers
    void parseHIDReport(ControllerState& state, PCHAR report, ULONG reportLength);
    void getHIDUsages(ControllerState& state, PCHAR report, ULONG reportLength);
    void getHIDValues(ControllerState& state, PCHAR report, ULONG reportLength);
};