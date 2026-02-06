#include "utils/timing.hpp"

bool TimingUtils::s_initialized = false;
LARGE_INTEGER TimingUtils::s_frequency = {};

bool TimingUtils::initialize() {
    if (s_initialized) {
        return true;
    }
    
    // Get the performance frequency (ticks per second)
    if (!QueryPerformanceFrequency(&s_frequency)) {
        return false;
    }
    
    s_initialized = true;
    return true;
}

uint64_t TimingUtils::getPerformanceCounter() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return static_cast<uint64_t>(counter.QuadPart);
}

double TimingUtils::counterToMicroseconds(uint64_t counterDiff) {
    if (!s_initialized) {
        initialize();
    }
    
    // Convert to microseconds: (counterDiff / frequency) * 1,000,000
    return (static_cast<double>(counterDiff) * 1000000.0) / static_cast<double>(s_frequency.QuadPart);
}

uint64_t TimingUtils::microsecondsToCounter(int64_t microseconds) {
    if (!s_initialized) {
        initialize();
    }
    
    // Convert microseconds to counter ticks: (microseconds * frequency) / 1,000,000
    return (static_cast<uint64_t>(microseconds) * static_cast<uint64_t>(s_frequency.QuadPart)) / 1000000ULL;
}

double TimingUtils::counterToMilliseconds(uint64_t counterDiff) {
    if (!s_initialized) {
        initialize();
    }
    
    // Convert to milliseconds: (counterDiff / frequency) * 1,000
    return (static_cast<double>(counterDiff) * 1000.0) / static_cast<double>(s_frequency.QuadPart);
}

uint64_t TimingUtils::getPerformanceFrequency() {
    if (!s_initialized) {
        initialize();
    }
    
    return static_cast<uint64_t>(s_frequency.QuadPart);
}