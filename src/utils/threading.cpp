#include "utils/threading.hpp"

int ThreadingUtils::s_logicalCoreCount = 0;

bool ThreadingUtils::setCurrentThreadToHighPriority() {
    return SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST) != 0;
}

bool ThreadingUtils::setCurrentThreadToTimeCriticalPriority() {
    return SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) != 0;
}

bool ThreadingUtils::setThreadToHighPriority(std::thread& thread) {
    if (!thread.joinable()) {
        return false;
    }
    
    return SetThreadPriority(thread.native_handle(), THREAD_PRIORITY_HIGHEST) != 0;
}

bool ThreadingUtils::setThreadToTimeCriticalPriority(std::thread& thread) {
    if (!thread.joinable()) {
        return false;
    }
    
    return SetThreadPriority(thread.native_handle(), THREAD_PRIORITY_TIME_CRITICAL) != 0;
}

bool ThreadingUtils::setCurrentThreadAffinity(int coreId) {
    int coreCount = getLogicalCoreCount();
    if (coreId < 0 || coreId >= coreCount) {
        return false;
    }
    
    DWORD_PTR affinityMask = static_cast<DWORD_PTR>(1) << coreId;
    DWORD_PTR result = SetThreadAffinityMask(GetCurrentThread(), affinityMask);
    
    return result != 0;
}

bool ThreadingUtils::setThreadAffinity(std::thread& thread, int coreId) {
    if (!thread.joinable()) {
        return false;
    }
    
    int coreCount = getLogicalCoreCount();
    if (coreId < 0 || coreId >= coreCount) {
        return false;
    }
    
    DWORD_PTR affinityMask = static_cast<DWORD_PTR>(1) << coreId;
    DWORD_PTR result = SetThreadAffinityMask(thread.native_handle(), affinityMask);
    
    return result != 0;
}

int ThreadingUtils::getLogicalCoreCount() {
    if (s_logicalCoreCount == 0) {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        s_logicalCoreCount = static_cast<int>(sysInfo.dwNumberOfProcessors);
    }
    
    return s_logicalCoreCount;
}