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
// HidHide uses FILE_DEVICE_UNKNOWN (0x22) with custom function codes
#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0x0000
#endif

// Official HidHide IOCTL codes (from HidHide v1.2+)
// These use FILE_ANY_ACCESS instead of FILE_READ_DATA
#define IOCTL_GET_WHITELIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_WHITELIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_BLACKLIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_BLACKLIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_ACTIVE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_ACTIVE      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_WLINVERSE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SET_WLINVERSE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)

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