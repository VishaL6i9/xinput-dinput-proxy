#pragma once

#include <thread>
#include <windows.h>

class ThreadingUtils {
public:
    // Set the current thread to high priority for real-time processing
    static bool setCurrentThreadToHighPriority();
    
    // Set the current thread to time-critical priority for real-time processing
    static bool setCurrentThreadToTimeCriticalPriority();
    
    // Set a specific thread to high priority
    static bool setThreadToHighPriority(std::thread& thread);
    
    // Set a specific thread to time-critical priority
    static bool setThreadToTimeCriticalPriority(std::thread& thread);
    
    // Pin the current thread to a specific CPU core
    static bool setCurrentThreadAffinity(int coreId);
    
    // Pin a specific thread to a specific CPU core
    static bool setThreadAffinity(std::thread& thread, int coreId);
    
    // Get the number of logical processor cores
    static int getLogicalCoreCount();
    
private:
    static int s_logicalCoreCount;
};