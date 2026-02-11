/**
 * @file translation_layer.hpp
 * @brief Input translation and normalization layer
 * 
 * This module translates between different controller input formats (XInput, DirectInput, HID)
 * and applies advanced input processing like SOCD cleaning and debouncing.
 */
#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <array>
#include "core/input_capture.hpp"

/**
 * @struct TranslatedState
 * @brief Standardized controller state after translation
 * 
 * This structure represents a controller's state in a normalized format
 * that can be sent to either XInput or DirectInput virtual devices.
 */
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

/**
 * @class TranslationLayer
 * @brief Bidirectional input translation between XInput and DirectInput formats
 * 
 * Features:
 * - XInput to DirectInput translation (Xbox → DualShock 4)
 * - DirectInput to XInput translation (Generic HID → Xbox 360)
 * - SOCD (Simultaneous Opposing Cardinal Directions) cleaning
 * - Input debouncing for mechanical switch noise filtering
 * - Device-specific profiles for optimal compatibility
 * - Safe axis scaling to prevent truncation errors
 */
class TranslationLayer {
public:
    TranslationLayer();
    ~TranslationLayer() = default;
    
    // Translate input states from source format to target format
    std::vector<TranslatedState> translate(const std::vector<ControllerState>& inputStates);
    
    // Configure translation mappings
    void setXInputToDInputMapping(bool enabled);
    void setDInputToXInputMapping(bool enabled);
    bool isXInputToDInputEnabled() const { return m_xinputToDInputEnabled; }
    bool isDInputToXInputEnabled() const { return m_dinputToXInputEnabled; }
    
    // Set SOCD cleaning options
    void setSOCDCleaningEnabled(bool enabled);
    void setSOCDMethod(int method); // 0: Last Win, 1: First Win, 2: Neutral
    
    // Set input debouncing options
    void setDebouncingEnabled(bool enabled);
    void setDebounceIntervalMs(int ms);
    
    // Translate standardized state to XInput format
    XINPUT_STATE translateToXInput(const TranslatedState& state);
    
    // Comprehensive DirectInput state (similar to DIJOYSTATE2)
    struct DInputState {
        LONG lX;            // X-axis (usually Left Stick X)
        LONG lY;            // Y-axis (usually Left Stick Y)
        LONG lZ;            // Z-axis (usually Left Trigger or Right Stick X)
        LONG lRx;           // R-axis (usually Right Stick X)
        LONG lRy;           // U-axis (usually Right Stick Y)
        LONG lRz;           // V-axis (usually Right Trigger)
        LONG rglSlider[2];  // Extra sliders
        DWORD rgdwPOV[4];   // POV hats (in hundredths of degrees, -1 for centered)
        BYTE rgbButtons[128]; // Max 128 buttons
        
        // Legacy fields for our internal simplified mapping compatibility
        WORD wButtons;
        BYTE bLeftTrigger;
        BYTE bRightTrigger;
    };
    DInputState translateToDInput(const TranslatedState& state);

private:
    struct HIDMappingProfile {
        std::wstring productName;
        std::unordered_map<USAGE, WORD> buttonMap;
        std::unordered_map<USAGE, int> axisMap; // Index into gamepad axes
    };
    std::unordered_map<std::wstring, HIDMappingProfile> m_deviceProfiles;
    
    void initializeProfiles();
    bool m_xinputToDInputEnabled;
    bool m_dinputToXInputEnabled;
    bool m_socdCleaningEnabled;
    int m_socdMethod;  // 0: Last Win, 1: First Win, 2: Neutral
    bool m_debouncingEnabled;
    int m_debounceIntervalMs;

    // Internal state for debouncing (fixed size to prevent unbounded growth)
    static constexpr size_t MAX_CONTROLLERS = 16;
    std::array<uint64_t, MAX_CONTROLLERS> m_lastButtonChangeTime;

    // Apply SOCD cleaning to a gamepad state
    void applySOCDControl(TranslatedState::GamepadState& gamepad);

    // Apply debouncing to a gamepad state
    bool applyDebouncing(int userId, WORD currentButtons, WORD& cleanedButtons);

    // Convert XInput state to standardized format
    TranslatedState convertXInputToStandard(const ControllerState& inputState);

    // Convert HID state to standardized format
    TranslatedState convertHIDToStandard(const ControllerState& inputState);

public:
    // Helpers for safe scaling
    static SHORT scaleLongToShort(LONG value);
    static LONG scaleShortToLong(SHORT value);
    static float normalizeShort(SHORT value);
    static float normalizeLong(LONG value);
    static float normalizeByte(BYTE value);
};