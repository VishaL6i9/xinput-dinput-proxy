#include "utils/hidhide_controller.hpp"
#include "utils/logger.hpp"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <iomanip>

HidHideController::HidHideController() : m_driverHandle(INVALID_HANDLE_VALUE), m_connected(false) {
}

HidHideController::~HidHideController() {
    disconnect();
}

bool HidHideController::connect() {
    if (m_connected) {
        return true;
    }

    // Try to open the HidHide control device
    m_driverHandle = CreateFileW(
        L"\\\\.\\HidHide",  // Standard HidHide device name
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (m_driverHandle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        // HidHide might not be installed or running
        if (err == ERROR_FILE_NOT_FOUND) {
            Logger::log("WARNING: HidHide driver not found. Is it installed?");
        } else if (err == ERROR_ACCESS_DENIED) {
            Logger::error("ERROR: Access denied to HidHide driver. Please run as Administrator.");
        } else {
            Logger::error("ERROR: Could not connect to HidHide driver. Error: " + std::to_string(err));
        }
        return false;
    }

    m_connected = true;
    Logger::log("Successfully connected to HidHide driver");
    
    // Log IOCTL codes for debugging
    std::stringstream ss;
    ss << "Debug: IOCTL_GET_BLACKLIST = 0x" << std::hex << IOCTL_GET_BLACKLIST;
    Logger::log(ss.str());
    ss.str("");
    ss << "Debug: IOCTL_SET_BLACKLIST = 0x" << std::hex << IOCTL_SET_BLACKLIST;
    Logger::log(ss.str());
    ss.str("");
    ss << "Debug: IOCTL_GET_ACTIVE = 0x" << std::hex << IOCTL_GET_ACTIVE;
    Logger::log(ss.str());
    
    // Try to check if driver is active
    BOOL active = FALSE;
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        IOCTL_GET_ACTIVE,
        nullptr, 0,
        &active, sizeof(active),
        &bytesReturned,
        NULL
    );
    
    if (result) {
        Logger::log("Debug: HidHide active state: " + std::string(active ? "ACTIVE" : "INACTIVE"));
        Logger::log("Debug: Bytes returned: " + std::to_string(bytesReturned));
    } else {
        DWORD err = GetLastError();
        Logger::log("Debug: Could not query HidHide active state. Error: " + std::to_string(err));
    }
    
    // Check inverse mode
    BOOL inverse = FALSE;
    bytesReturned = 0;
    result = DeviceIoControl(
        m_driverHandle,
        IOCTL_GET_WLINVERSE,
        nullptr, 0,
        &inverse, sizeof(inverse),
        &bytesReturned,
        NULL
    );
    
    if (result) {
        Logger::log("Debug: HidHide inverse mode: " + std::string(inverse ? "ENABLED (whitelist mode)" : "DISABLED (blacklist mode)"));
        if (inverse) {
            Logger::log("WARNING: HidHide is in whitelist mode. This application needs to be added to the whitelist in HidHide Configuration Client.");
        }
    } else {
        DWORD err = GetLastError();
        Logger::log("Debug: Could not query HidHide inverse mode. Error: " + std::to_string(err));
    }
    
    return true;
}

void HidHideController::disconnect() {
    if (m_connected && m_driverHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_driverHandle);
        m_driverHandle = INVALID_HANDLE_VALUE;
        m_connected = false;
        Logger::log("Disconnected from HidHide driver");
    }
}

bool HidHideController::sendIoctl(DWORD ioctlCode, LPVOID inBuffer, DWORD inSize, LPVOID outBuffer, DWORD outSize) {
    if (!m_connected || m_driverHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        ioctlCode,
        inBuffer, inSize,
        outBuffer, outSize,
        &bytesReturned,
        NULL
    );

    return result == TRUE;
}

bool HidHideController::addDeviceToBlacklist(const std::wstring& devicePath) {
    if (!m_connected) {
        return false;
    }

    // First get the current blacklist
    std::vector<std::wstring> currentList = getBlacklist();
    
    // If getBlacklist failed (HidHide not working), silently fail
    static bool hidHideFailed = false;
    if (currentList.empty() && !hidHideFailed) {
        // First failure - getBlacklist already logged the error
        hidHideFailed = true;
        return false;
    }
    if (hidHideFailed) {
        return false; // Silently fail on subsequent attempts
    }
    
    // Check if device is already in the list
    if (std::find(currentList.begin(), currentList.end(), devicePath) != currentList.end()) {
        // Device already in blacklist, no need to log
        return true; // Already added
    }

    // Add the new device to the list
    currentList.push_back(devicePath);

    // Prepare the buffer for the new list
    // HidHide expects a multi-string format: count (ULONG) followed by null-terminated wide strings
    size_t totalSize = sizeof(ULONG); // Count
    for (const auto& path : currentList) {
        totalSize += (path.length() + 1) * sizeof(WCHAR); // Include null terminator
    }
    totalSize += sizeof(WCHAR); // Extra null terminator at the end

    std::vector<BYTE> buffer(totalSize, 0);
    ULONG* countPtr = reinterpret_cast<ULONG*>(buffer.data());
    *countPtr = static_cast<ULONG>(currentList.size());

    WCHAR* pathPtr = reinterpret_cast<WCHAR*>(buffer.data() + sizeof(ULONG));
    for (const auto& path : currentList) {
        wcscpy_s(pathPtr, path.length() + 1, path.c_str());
        pathPtr += path.length() + 1;
    }
    // pathPtr now points to the final null terminator (already zeroed)

    Logger::log("Debug: Attempting to set blacklist with " + std::to_string(currentList.size()) + " devices");
    Logger::log("Debug: Buffer size: " + std::to_string(buffer.size()) + " bytes");
    
    bool result = sendIoctl(IOCTL_SET_BLACKLIST, buffer.data(), static_cast<DWORD>(buffer.size()));
    
    if (!result) {
        DWORD err = GetLastError();
        // Only log error once per session
        static bool setBlacklistErrorLogged = false;
        if (!setBlacklistErrorLogged) {
            Logger::error("Failed to add device to HidHide blacklist. Error: " + std::to_string(err));
            Logger::error("Device path: " + Logger::wstringToNarrow(devicePath));
            setBlacklistErrorLogged = true;
        }
    } else {
        Logger::log("Successfully added device to HidHide blacklist: " + Logger::wstringToNarrow(devicePath));
    }

    return result;
}

bool HidHideController::removeDeviceFromBlacklist(const std::wstring& devicePath) {
    if (!m_connected) {
        return false;
    }

    // Get the current blacklist
    std::vector<std::wstring> currentList = getBlacklist();
    
    // Remove the specified device
    auto it = std::find(currentList.begin(), currentList.end(), devicePath);
    if (it != currentList.end()) {
        currentList.erase(it);
    } else {
        // Device not in list, nothing to remove
        return true;
    }

    // Prepare the buffer for the updated list
    size_t totalSize = sizeof(ULONG); // Count
    for (const auto& path : currentList) {
        totalSize += (path.length() + 1) * sizeof(WCHAR); // Include null terminator
    }

    std::vector<BYTE> buffer(totalSize);
    ULONG* countPtr = reinterpret_cast<ULONG*>(buffer.data());
    *countPtr = static_cast<ULONG>(currentList.size());

    WCHAR* pathPtr = reinterpret_cast<WCHAR*>(buffer.data() + sizeof(ULONG));
    for (const auto& path : currentList) {
        wcscpy_s(pathPtr, path.length() + 1, path.c_str());
        pathPtr += path.length() + 1;
    }

    bool result = sendIoctl(IOCTL_SET_BLACKLIST, buffer.data(), static_cast<DWORD>(buffer.size()));
    
    if (result) {
        Logger::log("Removed device from HidHide blacklist: " + Logger::wstringToNarrow(devicePath));
    } else {
        Logger::error("Failed to remove device from HidHide blacklist: " + Logger::wstringToNarrow(devicePath));
    }

    return result;
}

bool HidHideController::clearBlacklist() {
    if (!m_connected) {
        return false;
    }

    // Create an empty list
    ULONG count = 0;
    bool result = sendIoctl(IOCTL_SET_BLACKLIST, &count, sizeof(count));
    
    if (result) {
        Logger::log("Cleared HidHide blacklist");
    } else {
        Logger::error("Failed to clear HidHide blacklist");
    }

    return result;
}

std::vector<std::wstring> HidHideController::getBlacklist() {
    if (!m_connected) {
        return {};
    }

    // Try to get the list with a buffer
    std::vector<BYTE> sizeBuffer(4096); // Start with 4KB buffer
    
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        IOCTL_GET_BLACKLIST,
        nullptr, 0,
        sizeBuffer.data(), static_cast<DWORD>(sizeBuffer.size()),
        &bytesReturned,
        NULL
    );

    if (result != TRUE) {
        DWORD error = GetLastError();
        static bool errorLogged = false;
        if (!errorLogged) {
            Logger::error("Failed to get HidHide blacklist. Error: " + std::to_string(error));
            Logger::error("HidHide may not be properly installed or configured. Device hiding will not work.");
            Logger::log("Debug: Driver handle valid: " + std::string(m_driverHandle != INVALID_HANDLE_VALUE ? "YES" : "NO"));
            Logger::log("Debug: IOCTL code: 0x" + std::to_string(IOCTL_GET_BLACKLIST));
            Logger::log("Debug: Buffer size: " + std::to_string(sizeBuffer.size()));
            errorLogged = true;
        }
        return {};
    }

    // Parse the returned data
    std::vector<std::wstring> devices;
    if (bytesReturned >= sizeof(ULONG)) {
        ULONG* countPtr = reinterpret_cast<ULONG*>(sizeBuffer.data());
        ULONG count = *countPtr;
        
        if (count > 0) {
            WCHAR* pathPtr = reinterpret_cast<WCHAR*>(sizeBuffer.data() + sizeof(ULONG));
            
            for (ULONG i = 0; i < count; ++i) {
                // Find the end of the current string (null terminator)
                WCHAR* currentPos = pathPtr;
                
                // Calculate the length of the string
                size_t len = 0;
                while (currentPos[len] != L'\0') {
                    len++;
                }
                
                // Create the string from the character array
                std::wstring path(currentPos, len);
                
                devices.push_back(path);
                
                // Move pointer past the current string and its null terminator
                pathPtr = currentPos + len + 1;
            }
        }
    }

    return devices;
}

bool HidHideController::addProcessToWhitelist(const std::wstring& processPath) {
    if (!m_connected) {
        return false;
    }

    // First get the current whitelist
    std::vector<std::wstring> currentList = getWhitelist();
    
    // Check if process is already in the list
    if (std::find(currentList.begin(), currentList.end(), processPath) != currentList.end()) {
        Logger::log("Process already in HidHide whitelist: " + Logger::wstringToNarrow(processPath));
        return true; // Already added
    }

    // Add the new process to the list
    currentList.push_back(processPath);

    // Prepare the buffer for the new list
    size_t totalSize = sizeof(ULONG); // Count
    for (const auto& path : currentList) {
        totalSize += (path.length() + 1) * sizeof(WCHAR); // Include null terminator
    }

    std::vector<BYTE> buffer(totalSize);
    ULONG* countPtr = reinterpret_cast<ULONG*>(buffer.data());
    *countPtr = static_cast<ULONG>(currentList.size());

    WCHAR* pathPtr = reinterpret_cast<WCHAR*>(buffer.data() + sizeof(ULONG));
    for (const auto& path : currentList) {
        wcscpy_s(pathPtr, path.length() + 1, path.c_str());
        pathPtr += path.length() + 1;
    }

    bool result = sendIoctl(IOCTL_SET_WHITELIST, buffer.data(), static_cast<DWORD>(buffer.size()));
    
    if (result) {
        Logger::log("Added process to HidHide whitelist: " + Logger::wstringToNarrow(processPath));
    } else {
        Logger::error("Failed to add process to HidHide whitelist: " + Logger::wstringToNarrow(processPath));
    }

    return result;
}

bool HidHideController::removeProcessFromWhitelist(const std::wstring& processPath) {
    if (!m_connected) {
        return false;
    }

    // Get the current whitelist
    std::vector<std::wstring> currentList = getWhitelist();
    
    // Remove the specified process
    auto it = std::find(currentList.begin(), currentList.end(), processPath);
    if (it != currentList.end()) {
        currentList.erase(it);
    } else {
        // Process not in list, nothing to remove
        return true;
    }

    // Prepare the buffer for the updated list
    size_t totalSize = sizeof(ULONG); // Count
    for (const auto& path : currentList) {
        totalSize += (path.length() + 1) * sizeof(WCHAR); // Include null terminator
    }

    std::vector<BYTE> buffer(totalSize);
    ULONG* countPtr = reinterpret_cast<ULONG*>(buffer.data());
    *countPtr = static_cast<ULONG>(currentList.size());

    WCHAR* pathPtr = reinterpret_cast<WCHAR*>(buffer.data() + sizeof(ULONG));
    for (const auto& path : currentList) {
        wcscpy_s(pathPtr, path.length() + 1, path.c_str());
        pathPtr += path.length() + 1;
    }

    bool result = sendIoctl(IOCTL_SET_WHITELIST, buffer.data(), static_cast<DWORD>(buffer.size()));
    
    if (result) {
        Logger::log("Removed process from HidHide whitelist: " + Logger::wstringToNarrow(processPath));
    } else {
        Logger::error("Failed to remove process from HidHide whitelist: " + Logger::wstringToNarrow(processPath));
    }

    return result;
}

bool HidHideController::clearWhitelist() {
    if (!m_connected) {
        return false;
    }

    // Create an empty list
    ULONG count = 0;
    bool result = sendIoctl(IOCTL_SET_WHITELIST, &count, sizeof(count));
    
    if (result) {
        Logger::log("Cleared HidHide whitelist");
    } else {
        Logger::error("Failed to clear HidHide whitelist");
    }

    return result;
}

std::vector<std::wstring> HidHideController::getWhitelist() {
    if (!m_connected) {
        return {};
    }

    // Get the whitelist
    std::vector<BYTE> sizeBuffer(4096); // Start with 4KB buffer
    
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        IOCTL_GET_WHITELIST,
        nullptr, 0,
        sizeBuffer.data(), static_cast<DWORD>(sizeBuffer.size()),
        &bytesReturned,
        NULL
    );

    if (result != TRUE) {
        Logger::error("Failed to get HidHide whitelist");
        return {};
    }

    // Parse the returned data
    std::vector<std::wstring> processes;
    if (bytesReturned >= sizeof(ULONG)) {
        ULONG* countPtr = reinterpret_cast<ULONG*>(sizeBuffer.data());
        ULONG count = *countPtr;

        if (count > 0) {
            WCHAR* pathPtr = reinterpret_cast<WCHAR*>(sizeBuffer.data() + sizeof(ULONG));

            for (ULONG i = 0; i < count; ++i) {
                // Find the end of the current string (null terminator)
                WCHAR* currentPos = pathPtr;
                
                // Calculate the length of the string
                size_t len = 0;
                while (currentPos[len] != L'\0') {
                    len++;
                }
                
                // Create the string from the character array
                std::wstring path(currentPos, len);
                
                processes.push_back(path);
                
                // Move pointer past the current string and its null terminator
                pathPtr = currentPos + len + 1;
            }
        }
    }

    return processes;
}

bool HidHideController::setActive(bool active) {
    if (!m_connected) {
        return false;
    }

    ULONG state = active ? 1 : 0;
    bool result = sendIoctl(IOCTL_SET_ACTIVE, &state, sizeof(state));
    
    if (result) {
        Logger::log(active ? "HidHide driver activated" : "HidHide driver deactivated");
    } else {
        Logger::error(active ? "Failed to activate HidHide driver" : "Failed to deactivate HidHide driver");
    }

    return result;
}

bool HidHideController::isActive() {
    if (!m_connected) {
        return false;
    }

    ULONG state = 0;
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        IOCTL_GET_ACTIVE,
        nullptr, 0,
        &state, sizeof(state),
        &bytesReturned,
        NULL
    );

    if (result != TRUE) {
        Logger::error("Failed to get HidHide active state");
        return false;
    }

    return state != 0;
}

bool HidHideController::setInverseMode(bool inverse) {
    if (!m_connected) {
        return false;
    }

    ULONG state = inverse ? 1 : 0;
    bool result = sendIoctl(IOCTL_SET_WLINVERSE, &state, sizeof(state));
    
    if (result) {
        Logger::log(inverse ? "HidHide inverse mode enabled" : "HidHide inverse mode disabled");
    } else {
        Logger::error(inverse ? "Failed to enable HidHide inverse mode" : "Failed to disable HidHide inverse mode");
    }

    return result;
}

bool HidHideController::getInverseMode() {
    if (!m_connected) {
        return false;
    }

    ULONG state = 0;
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        m_driverHandle,
        IOCTL_GET_WLINVERSE,
        nullptr, 0,
        &state, sizeof(state),
        &bytesReturned,
        NULL
    );

    if (result != TRUE) {
        Logger::error("Failed to get HidHide inverse mode state");
        return false;
    }

    return state != 0;
}

std::vector<std::wstring> HidHideController::enumerateHidDevices() {
    std::vector<std::wstring> devicePaths;
    
    // Setup API to enumerate HID devices
    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&GUID_DEVCLASS_HIDCLASS, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        return devicePaths;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVCLASS_HIDCLASS, i, &deviceInterfaceData); i++) {
        // Get required buffer size
        SP_DEVINFO_DATA devInfoData = {sizeof(SP_DEVINFO_DATA)};
        DWORD requiredSize = 0;

        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, &devInfoData);

        if (requiredSize > 0) {
            std::vector<BYTE> detailBuffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA>(detailBuffer.data());
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

            if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                // Cast to the wide version of the structure to access the wide string
                PSP_DEVICE_INTERFACE_DETAIL_DATA_W wideDetailData = 
                    reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailData);
                devicePaths.push_back(wideDetailData->DevicePath);
            }
        }
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return devicePaths;
}

std::wstring HidHideController::getDeviceInstanceId(const std::wstring& devicePath) {
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