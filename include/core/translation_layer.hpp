#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include "core/input_capture.hpp"

// Structure to hold translated input state
struct TranslatedState {
    int sourceUserId;  // Original controller ID
    bool isXInputSource;  // True if source was XInput, false if HID
    
    // Translated gamepad state (standardized format)
    struct GamepadState {
        WORD wButtons;
        BYTE bLeftTrigger;
        BYTE bRightTrigger;
        SHORT sThumbLX;
        SHORT sThumbLY;
        SHORT sThumbRX;
        SHORT sThumbRY;
    } gamepad;
    
    // Timestamp of when this state was translated
    uint64_t timestamp;
    
    // Type of target device (XInput or DirectInput)
    enum TargetType {
        TARGET_XINPUT,
        TARGET_DINPUT
    } targetType;
};

class TranslationLayer {
public:
    TranslationLayer();
    ~TranslationLayer() = default;
    
    // Translate input states from source format to target format
    std::vector<TranslatedState> translate(const std::vector<ControllerState>& inputStates);
    
    // Configure translation mappings
    void setXInputToDInputMapping(bool enabled);
    void setDInputToXInputMapping(bool enabled);
    
    // Set SOCD cleaning options
    void setSOCDCleaningEnabled(bool enabled);
    void setSOCDMethod(int method); // 0: Last Win, 1: First Win, 2: Neutral
    
    // Set input debouncing options
    void setDebouncingEnabled(bool enabled);
    void setDebounceIntervalMs(int ms);
    
    // Translate standardized state to XInput format
    XINPUT_STATE translateToXInput(const TranslatedState& state);
    
    // Translate standardized state to DirectInput format
    // Note: DirectInput state representation would be more complex
    // This is a simplified placeholder
    struct DInputState {
        WORD wButtons;
        BYTE bLeftTrigger;
        BYTE bRightTrigger;
        LONG lX;
        LONG lY;
        LONG lZ;
        LONG lRx;
        LONG lRy;
        LONG lRz;
    };
    DInputState translateToDInput(const TranslatedState& state);

private:
    bool m_xinputToDInputEnabled;
    bool m_dinputToXInputEnabled;
    bool m_socdCleaningEnabled;
    int m_socdMethod;  // 0: Last Win, 1: First Win, 2: Neutral
    bool m_debouncingEnabled;
    int m_debounceIntervalMs;

    // Internal state for debouncing
    std::vector<uint64_t> m_lastButtonChangeTime;

    // Apply SOCD cleaning to a gamepad state
    void applySOCDControl(TranslatedState::GamepadState& gamepad);

    // Apply debouncing to a gamepad state
    bool applyDebouncing(int userId, WORD currentButtons, WORD& cleanedButtons);

    // Convert XInput state to standardized format
    TranslatedState convertXInputToStandard(const ControllerState& inputState);

    // Convert HID state to standardized format
    TranslatedState convertHIDToStandard(const ControllerState& inputState);
};