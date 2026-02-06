#pragma once

#include <windows.h>
#include <cstdint>

class TimingUtils {
public:
    // Initialize timing utilities
    static bool initialize();
    
    // Get high-resolution performance counter value
    static uint64_t getPerformanceCounter();
    
    // Convert performance counter difference to microseconds
    static double counterToMicroseconds(uint64_t counterDiff);
    
    // Convert microseconds to performance counter ticks
    static uint64_t microsecondsToCounter(int64_t microseconds);
    
    // Convert performance counter difference to milliseconds
    static double counterToMilliseconds(uint64_t counterDiff);
    
    // Get the performance frequency (ticks per second)
    static uint64_t getPerformanceFrequency();
    
private:
    static bool s_initialized;
    static LARGE_INTEGER s_frequency;
};