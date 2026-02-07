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

// Define the actual HidHide IOCTL codes based on the real driver values
// Since FILE_DEVICE_UNKNOWN might not be defined in all contexts, define it if needed
#ifndef FILE_DEVICE_UNKNOWN
#define FILE_DEVICE_UNKNOWN 0x00000022
#endif

#ifndef METHOD_BUFFERED
#define METHOD_BUFFERED 0
#endif

#ifndef FILE_READ_DATA
#define FILE_READ_DATA 0x0001
#endif

#ifndef FILE_ANY_ACCESS
#define FILE_ANY_ACCESS 0x0000
#endif

#define IOCTL_GET_WHITELIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 2048, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_WHITELIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 2049, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_BLACKLIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 2050, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_BLACKLIST   CTL_CODE(FILE_DEVICE_UNKNOWN, 2051, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_ACTIVE      CTL_CODE(FILE_DEVICE_UNKNOWN, 2052, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_ACTIVE      CTL_CODE(FILE_DEVICE_UNKNOWN, 2053, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_GET_WLINVERSE   CTL_CODE(FILE_DEVICE_UNKNOWN, 2054, METHOD_BUFFERED, FILE_READ_DATA)
#define IOCTL_SET_WLINVERSE   CTL_CODE(FILE_DEVICE_UNKNOWN, 2055, METHOD_BUFFERED, FILE_READ_DATA)

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