// Simple test to verify the build system works
#include <iostream>
#include <windows.h>

int main() {
    std::cout << "Build environment test successful!" << std::endl;
    std::cout << "Windows version: ";
    
    // Get Windows version information
    OSVERSIONINFOW osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOW));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);
    
    if (GetVersionExW(&osvi)) {
        std::wcout << osvi.dwMajorVersion << "." << osvi.dwMinorVersion 
                   << " (Build " << osvi.dwBuildNumber << ")" << std::endl;
    } else {
        std::cout << "Unable to get version" << std::endl;
    }
    
    std::cout << "This confirms that:" << std::endl;
    std::cout << "- C++ compiler is working" << std::endl;
    std::cout << "- Windows SDK is available" << std::endl;
    std::cout << "- CMake+Ninja build system is functional" << std::endl;
    
    return 0;
}