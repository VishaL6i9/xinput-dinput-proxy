#include "core/translation_layer.hpp"
#include "utils/timing.hpp"

#include <algorithm>
#include <cmath>

TranslationLayer::TranslationLayer() 
    : m_xinputToDInputEnabled(true), 
      m_dinputToXInputEnabled(true),
      m_socdCleaningEnabled(true),
      m_socdMethod(0), // Last Win by default
      m_debouncingEnabled(true),
      m_debounceIntervalMs(50) {  // 50ms debounce by default
}

std::vector<TranslatedState> TranslationLayer::translate(const std::vector<ControllerState>& inputStates) {
    std::vector<TranslatedState> translatedStates;
    
    for (const auto& inputState : inputStates) {
        TranslatedState translatedState;
        
        if (inputState.xinputState.dwPacketNumber > 0 || inputState.userId >= 0) {
            // This appears to be an XInput device
            translatedState = convertXInputToStandard(inputState);
            translatedState.isXInputSource = true;
        } else if (!inputState.devicePath.empty()) {
            // This appears to be a HID device
            translatedState = convertHIDToStandard(inputState);
            translatedState.isXInputSource = false;
        } else {
            // Skip unrecognized input state
            continue;
        }
        
        // Apply SOCD cleaning if enabled
        if (m_socdCleaningEnabled) {
            applySOCDControl(translatedState.gamepad);
        }
        
        // Apply debouncing if enabled
        if (m_debouncingEnabled) {
            WORD cleanedButtons = translatedState.gamepad.wButtons;
            if (applyDebouncing(translatedState.sourceUserId, translatedState.gamepad.wButtons, cleanedButtons)) {
                translatedState.gamepad.wButtons = cleanedButtons;
            }
        }
        
        translatedStates.push_back(translatedState);
    }
    
    return translatedStates;
}

void TranslationLayer::setXInputToDInputMapping(bool enabled) {
    m_xinputToDInputEnabled = enabled;
}

void TranslationLayer::setDInputToXInputMapping(bool enabled) {
    m_dinputToXInputEnabled = enabled;
}

void TranslationLayer::setSOCDCleaningEnabled(bool enabled) {
    m_socdCleaningEnabled = enabled;
}

void TranslationLayer::setSOCDMethod(int method) {
    m_socdMethod = method;
}

void TranslationLayer::setDebouncingEnabled(bool enabled) {
    m_debouncingEnabled = enabled;
}

void TranslationLayer::setDebounceIntervalMs(int ms) {
    m_debounceIntervalMs = ms;
}

void TranslationLayer::applySOCDControl(TranslatedState::GamepadState& gamepad) {
    // SOCD stands for "Simultaneous Opposing Cardinal Directions"
    // This handles cases where both left and right (or up and down) are pressed simultaneously
    
    bool leftPressed = (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
    bool rightPressed = (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
    bool upPressed = (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
    bool downPressed = (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
    
    switch (m_socdMethod) {
        case 0: // Last Win - Prioritize the most recently pressed direction
        {
            // For horizontal movement
            if (leftPressed && rightPressed) {
                // Keep the one that was pressed last based on stick position
                if (gamepad.sThumbLX < 0) {
                    gamepad.wButtons &= ~XINPUT_GAMEPAD_DPAD_RIGHT;
                } else {
                    gamepad.wButtons &= ~XINPUT_GAMEPAD_DPAD_LEFT;
                }
            }
            
            // For vertical movement
            if (upPressed && downPressed) {
                // Keep the one that was pressed last based on stick position
                if (gamepad.sThumbLY > 0) {
                    gamepad.wButtons &= ~XINPUT_GAMEPAD_DPAD_DOWN;
                } else {
                    gamepad.wButtons &= ~XINPUT_GAMEPAD_DPAD_UP;
                }
            }
            break;
        }
        case 1: // First Win - Prioritize the first pressed direction
        {
            // For horizontal movement
            if (leftPressed && rightPressed) {
                // Keep both directions disabled (neutral)
                gamepad.wButtons &= ~(XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT);
            }
            
            // For vertical movement
            if (upPressed && downPressed) {
                // Keep both directions disabled (neutral)
                gamepad.wButtons &= ~(XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN);
            }
            break;
        }
        case 2: // Neutral - Always favor neutral state
        default:
        {
            // For horizontal movement
            if (leftPressed && rightPressed) {
                // Cancel both directions
                gamepad.wButtons &= ~(XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT);
            }
            
            // For vertical movement
            if (upPressed && downPressed) {
                // Cancel both directions
                gamepad.wButtons &= ~(XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN);
            }
            break;
        }
    }
}

bool TranslationLayer::applyDebouncing(int userId, WORD currentButtons, WORD& cleanedButtons) {
    // Ensure we have enough space in the timestamp vector
    if (userId >= static_cast<int>(m_lastButtonChangeTime.size())) {
        m_lastButtonChangeTime.resize(userId + 1, 0);
    }
    
    // Calculate time threshold in performance counter ticks
    uint64_t currentTime = TimingUtils::getPerformanceCounter();
    uint64_t timeThreshold = TimingUtils::microsecondsToCounter(m_debounceIntervalMs * 1000LL);
    
    // Simple debouncing: if button changed recently, ignore the change
    if ((currentTime - m_lastButtonChangeTime[userId]) < timeThreshold) {
        // Return false to indicate debouncing is active
        return false;
    }
    
    // Update the last change time
    m_lastButtonChangeTime[userId] = currentTime;
    cleanedButtons = currentButtons;
    return true;
}

TranslatedState TranslationLayer::convertXInputToStandard(const ControllerState& inputState) {
    TranslatedState state{};
    state.sourceUserId = inputState.userId;
    state.isXInputSource = true;
    state.timestamp = inputState.timestamp;
    
    // Copy gamepad state directly since XInput is our standard
    state.gamepad.wButtons = inputState.xinputState.Gamepad.wButtons;
    state.gamepad.bLeftTrigger = inputState.xinputState.Gamepad.bLeftTrigger;
    state.gamepad.bRightTrigger = inputState.xinputState.Gamepad.bRightTrigger;
    state.gamepad.sThumbLX = inputState.xinputState.Gamepad.sThumbLX;
    state.gamepad.sThumbLY = inputState.xinputState.Gamepad.sThumbLY;
    state.gamepad.sThumbRX = inputState.xinputState.Gamepad.sThumbRX;
    state.gamepad.sThumbRY = inputState.xinputState.Gamepad.sThumbRY;
    
    // Set target type based on configuration
    state.targetType = m_xinputToDInputEnabled ? TranslatedState::TARGET_DINPUT : TranslatedState::TARGET_XINPUT;
    
    return state;
}

TranslatedState TranslationLayer::convertHIDToStandard(const ControllerState& inputState) {
    TranslatedState state{};
    state.sourceUserId = -1; // HID devices don't have a user ID like XInput
    state.isXInputSource = false;
    state.timestamp = inputState.timestamp;
    
    // Map HID state to our standard gamepad format
    // This is a simplified mapping - actual implementation would need 
    // device-specific mapping based on the HID report descriptor
    state.gamepad.wButtons = inputState.gamepad.wButtons;
    state.gamepad.bLeftTrigger = inputState.gamepad.bLeftTrigger;
    state.gamepad.bRightTrigger = inputState.gamepad.bRightTrigger;
    state.gamepad.sThumbLX = inputState.gamepad.sThumbLX;
    state.gamepad.sThumbLY = inputState.gamepad.sThumbLY;
    state.gamepad.sThumbRX = inputState.gamepad.sThumbRX;
    state.gamepad.sThumbRY = inputState.gamepad.sThumbRY;
    
    // Set target type based on configuration
    state.targetType = m_dinputToXInputEnabled ? TranslatedState::TARGET_XINPUT : TranslatedState::TARGET_DINPUT;
    
    return state;
}

XINPUT_STATE TranslationLayer::translateToXInput(const TranslatedState& state) {
    XINPUT_STATE xinputState{};
    xinputState.dwPacketNumber = static_cast<DWORD>(state.timestamp);
    
    xinputState.Gamepad.wButtons = state.gamepad.wButtons;
    xinputState.Gamepad.bLeftTrigger = state.gamepad.bLeftTrigger;
    xinputState.Gamepad.bRightTrigger = state.gamepad.bRightTrigger;
    xinputState.Gamepad.sThumbLX = state.gamepad.sThumbLX;
    xinputState.Gamepad.sThumbLY = state.gamepad.sThumbLY;
    xinputState.Gamepad.sThumbRX = state.gamepad.sThumbRX;
    xinputState.Gamepad.sThumbRY = state.gamepad.sThumbRY;
    
    return xinputState;
}

TranslationLayer::DInputState TranslationLayer::translateToDInput(const TranslatedState& state) {
    DInputState dinputState{};
    
    // Map XInput-style buttons to DirectInput-style state
    // This is a simplified mapping - actual implementation would be more complex
    dinputState.wButtons = state.gamepad.wButtons;
    dinputState.bLeftTrigger = state.gamepad.bLeftTrigger;
    dinputState.bRightTrigger = state.gamepad.bRightTrigger;
    
    // Map thumbsticks to DirectInput axes
    // Use scaling helpers to convert from SHORT to LONG safely
    dinputState.lX = scaleShortToLong(state.gamepad.sThumbLX);
    dinputState.lY = scaleShortToLong(state.gamepad.sThumbLY);
    
    // Map triggers to Z/Rz axes
    // Triggers are 0-255, we want to map this to 0-65535 (LONG full range) or -32768 to 32767
    // Standard XInput trigger behavior is 0 (released) to 255 (pressed)
    // Let's map 0->0 and 255->65535 for DInput axes if they are unsigned, or center them.
    // However, most DInput implementations expect centered axes. 
    // For trigger-as-axis, let's map 0-255 => 0-65535 (Unsigned) or -32768-32767.
    // 
    // Implementing standard mapping: 0 -> -32768 (min), 255 -> 32767 (max)
    dinputState.lZ = static_cast<LONG>(state.gamepad.bLeftTrigger * 257) - 32768; 
    
    dinputState.lRx = scaleShortToLong(state.gamepad.sThumbRX);
    dinputState.lRy = scaleShortToLong(state.gamepad.sThumbRY);
    
    dinputState.lRz = static_cast<LONG>(state.gamepad.bRightTrigger * 257) - 32768;
    
    return dinputState;
}

SHORT TranslationLayer::scaleLongToShort(LONG value) {
    // scale 32-bit (assumed -32768 to 32767 range usually, effectively 16-bit range in 32-bit container) 
    // OR 0-65535 range. 
    // DInput standard is 0-65535 often, but can be formatted differently.
    // Assuming standard 16-bit signed range in 32-bit container for this context or full 32-bit range?
    // Let's assume standard DInput is 0..65535 
    
    // Convert 0..65535 to -32768..32767
    int32_t val = static_cast<int32_t>(value);
    
    // Clamp to 16-bit range
    if (val > 32767) val = 32767;
    if (val < -32768) val = -32768;
    
    return static_cast<SHORT>(val);
}

LONG TranslationLayer::scaleShortToLong(SHORT value) {
    // Convert -32768..32767 to DInput 0..65535 ?
    // Or keep it signed -32768..32767?
    // Most modern DInput usage expects 0..65535 for axes. 
    // But let's stick to 1:1 signed mapping if our DInputState structure expects signed LONGs
    // that mimic XInput behavior.
    
    // Actually, `lX`, `lY` etc are LONG. 
    // Let's return the value preserving the signed-ness but in 32-bit container
    return static_cast<LONG>(value);
}

float TranslationLayer::normalizeShort(SHORT value) {
    return std::max(-1.0f, std::min(1.0f, static_cast<float>(value) / 32767.0f));
}

float TranslationLayer::normalizeLong(LONG value) {
     return std::max(-1.0f, std::min(1.0f, static_cast<float>(value) / 32767.0f)); // Assuming signed 16-bit content
}

float TranslationLayer::normalizeByte(BYTE value) {
    return std::max(0.0f, std::min(1.0f, static_cast<float>(value) / 255.0f));
}