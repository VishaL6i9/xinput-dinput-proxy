#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#include <winioctl.h>  // For CTL_CODE
#include <vector>
#include <string>

// Define the actual HidHide IOCTL codes based on the official HidHide driver
// HidHide v1.5 uses these specific IOCTL codes
#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0x0000
#endif

// HidHide IOCTL codes
// Note: Using DS4Windows-compatible codes (older HidHide API)
// These work with HidHide v1.x
#define IOCTL_GET_WHITELIST   0x80016000
#define IOCTL_SET_WHITELIST   0x80016004
#define IOCTL_GET_BLACKLIST   0x80016008
#define IOCTL_SET_BLACKLIST   0x8001600C
#define IOCTL_GET_ACTIVE      0x80016010
#define IOCTL_SET_ACTIVE      0x80016014
#define IOCTL_GET_WLINVERSE   0x80016018
#define IOCTL_SET_WLINVERSE   0x8001601C

// Structure for device instance path list
// We'll handle this as raw bytes in the implementation

class HidHideController {
public:
    HidHideController();
    ~HidHideController();

    bool connect();
    void disconnect();

    // Device hiding functions
    bool addDeviceToBlacklist(const std::wstring& devicePath);
    bool removeDeviceFromBlacklist(const std::wstring& devicePath);
    bool clearBlacklist();
    std::vector<std::wstring> getBlacklist();

    // Whitelist functions (applications that can see devices)
    bool addProcessToWhitelist(const std::wstring& processPath);
    bool removeProcessFromWhitelist(const std::wstring& processPath);
    bool clearWhitelist();
    std::vector<std::wstring> getWhitelist();
    bool addSelfToWhitelist(); // Add current application to whitelist

    // Driver activation
    bool setActive(bool active);
    bool isActive();

    // Inverse mode (when enabled, only whitelisted apps see devices)
    bool setInverseMode(bool inverse);
    bool getInverseMode();

    // Utility functions
    static std::vector<std::wstring> enumerateHidDevices();
    static std::wstring getDeviceInstanceId(const std::wstring& devicePath);

private:
    HANDLE m_driverHandle;
    bool m_connected;

    // Helper functions
    bool sendIoctl(DWORD ioctlCode, LPVOID inBuffer = nullptr, DWORD inSize = 0, 
                   LPVOID outBuffer = nullptr, DWORD outSize = 0);
};