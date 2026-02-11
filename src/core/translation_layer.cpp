#include "core/translation_layer.hpp"
#include "utils/timing.hpp"

#include <algorithm>
#include <cmath>

// Include Windows headers for USAGE and other types
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <hidusage.h>

TranslationLayer::TranslationLayer() 
    : m_xinputToDInputEnabled(true), 
      m_dinputToXInputEnabled(true),
      m_socdCleaningEnabled(true),
      m_socdMethod(2), // Neutral
      m_debouncingEnabled(false),
      m_debounceIntervalMs(10),
      m_lastButtonChangeTime{} {  // Initialize array to zeros
    initializeProfiles();
}

void TranslationLayer::initializeProfiles() {
    // DS4 / DualSense Profile
    HIDMappingProfile ds4;
    ds4.productName = L"Wireless Controller"; // Sony's standard name
    ds4.buttonMap[1] = XINPUT_GAMEPAD_X; // Square
    ds4.buttonMap[2] = XINPUT_GAMEPAD_A; // Cross
    ds4.buttonMap[3] = XINPUT_GAMEPAD_B; // Circle
    ds4.buttonMap[4] = XINPUT_GAMEPAD_Y; // Triangle
    ds4.buttonMap[5] = XINPUT_GAMEPAD_LEFT_SHOULDER;
    ds4.buttonMap[6] = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    ds4.buttonMap[9] = XINPUT_GAMEPAD_BACK;   // Share
    ds4.buttonMap[10] = XINPUT_GAMEPAD_START; // Options
    ds4.buttonMap[11] = XINPUT_GAMEPAD_LEFT_THUMB;
    ds4.buttonMap[12] = XINPUT_GAMEPAD_RIGHT_THUMB;
    
    m_deviceProfiles[ds4.productName] = ds4;
}
/**
 * @brief Translates input states from various controller formats to a standardized format
 * 
 * This is the main translation function that processes all connected controllers.
 * It handles both XInput and HID devices, applying SOCD cleaning and debouncing as configured.
 * 
 * @param inputStates Vector of raw controller states from InputCapture
 * @return Vector of translated states ready for virtual device emulation
 */
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

/**
 * @brief Applies SOCD (Simultaneous Opposing Cardinal Directions) cleaning
 * 
 * SOCD cleaning resolves conflicts when opposing directions are pressed simultaneously
 * (e.g., left+right or up+down). This is critical for competitive gaming to prevent
 * unintended behavior.
 * 
 * Three methods are supported:
 * - Method 0 (Last Win): The most recently pressed direction takes priority
 * - Method 1 (First Win): The first pressed direction takes priority
 * - Method 2 (Neutral): Both directions cancel out, resulting in neutral position
 * 
 * @param gamepad Reference to gamepad state to be modified in-place
 */
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

/**
 * @brief Applies input debouncing to filter mechanical switch noise
 * 
 * Debouncing prevents rapid button state changes caused by mechanical switch bounce.
 * If a button state changes within the debounce interval, the change is ignored.
 * 
 * @param userId Controller user ID (0-15)
 * @param currentButtons Current button state
 * @param cleanedButtons Output parameter for debounced button state
 * @return true if input should be processed, false if debouncing is active
 */
bool TranslationLayer::applyDebouncing(int userId, WORD currentButtons, WORD& cleanedButtons) {
    // Bounds check for fixed-size array
    if (userId < 0 || userId >= static_cast<int>(MAX_CONTROLLERS)) {
        cleanedButtons = currentButtons;
        return true; // Allow input for out-of-bounds IDs
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

/**
 * @brief Converts XInput controller state to standardized format
 * 
 * XInput is already in our standard format, so this is mostly a direct copy
 * with metadata population.
 * 
 * @param inputState Raw XInput controller state
 * @return Standardized translated state
 */
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

/**
 * @brief Converts HID device state to standardized XInput-like format
 * 
 * This function handles the complex task of translating raw HID reports into
 * a standardized gamepad format. It supports:
 * - Device-specific profiles (e.g., DualShock 4, DualSense)
 * - Generic HID gamepad fallback mapping
 * - Axis normalization and inversion where needed
 * 
 * HID devices report axes in various ranges (0-255, 0-65535, etc.) and may
 * have inverted Y-axes. This function normalizes everything to XInput conventions:
 * - Axes: -32768 to 32767 (signed 16-bit)
 * - Triggers: 0 to 255 (unsigned 8-bit)
 * - Positive Y = up, Negative Y = down
 * 
 * @param inputState Raw HID controller state with parsed HID values
 * @return Standardized translated state
 */
TranslatedState TranslationLayer::convertHIDToStandard(const ControllerState& inputState) {
    TranslatedState state{};
    state.sourceUserId = -1;
    state.isXInputSource = false;
    state.timestamp = inputState.timestamp;
    
    // Default values
    state.gamepad.wButtons = 0;
    state.gamepad.bLeftTrigger = 0;
    state.gamepad.bRightTrigger = 0;
    state.gamepad.sThumbLX = 0;
    state.gamepad.sThumbLY = 0;
    state.gamepad.sThumbRX = 0;
    state.gamepad.sThumbRY = 0;

    // 1. Check for device-specific profile
    auto it = m_deviceProfiles.find(inputState.productName);
    if (it != m_deviceProfiles.end()) {
        const auto& profile = it->second;
        
        // Map Buttons
        for (USAGE usage : inputState.m_activeButtons) {
            auto btnIt = profile.buttonMap.find(usage);
            if (btnIt != profile.buttonMap.end()) {
                state.gamepad.wButtons |= btnIt->second;
            }
        }
        
        // Map Axes (Specific logic for DS4 etc.)
        if (inputState.productName == L"Wireless Controller") {
            // DS4 axes: 0x30=LX, 0x31=LY, 0x32=RX, 0x35=RY (usually)
            // Values are 0-255, center at 128
            // XInput convention: positive Y = up, negative Y = down
            // DS4 HID: 0 = up, 255 = down (inverted from XInput)
            for (const auto& [usage, value] : inputState.m_hidValues) {
                switch (usage) {
                    case 0x30: state.gamepad.sThumbLX = static_cast<SHORT>((value - 128) * 256); break;
                    case 0x31: state.gamepad.sThumbLY = static_cast<SHORT>((128 - value) * 256); break; // Invert: 0->up, 255->down
                    case 0x32: state.gamepad.sThumbRX = static_cast<SHORT>((value - 128) * 256); break;
                    case 0x35: state.gamepad.sThumbRY = static_cast<SHORT>((128 - value) * 256); break; // Invert: 0->up, 255->down
                }
            }
        }
    } else {
        // 2. Fallback to robust generic mapping
        // Standardize Buttons (1-based index to standard bits)
        for (USAGE usage : inputState.m_activeButtons) {
            if (usage >= 1 && usage <= 16) {
                // Map common button indices to something sensible
                if (usage == 1) state.gamepad.wButtons |= XINPUT_GAMEPAD_A;
                else if (usage == 2) state.gamepad.wButtons |= XINPUT_GAMEPAD_B;
                else if (usage == 3) state.gamepad.wButtons |= XINPUT_GAMEPAD_X;
                else if (usage == 4) state.gamepad.wButtons |= XINPUT_GAMEPAD_Y;
            }
        }
        
        // Standardize Axes (Generic Desktop Page 0x01)
        for (const auto& [usage, value] : inputState.m_hidValues) {
            // value is likely 0-65535 or 0-1023 etc. 
            // We'll normalize based on the reported caps if we had them here, 
            // but for now let's assume standard 0-65535 for generic.
            switch (usage) {
                case 0x30: state.gamepad.sThumbLX = static_cast<SHORT>(value - 32768); break;
                case 0x31: state.gamepad.sThumbLY = static_cast<SHORT>(32767 - value); break;
                case 0x32: state.gamepad.sThumbRX = static_cast<SHORT>(value - 32768); break;
                case 0x35: state.gamepad.sThumbRY = static_cast<SHORT>(32767 - value); break;
                case 0x33: state.gamepad.bLeftTrigger = static_cast<BYTE>(value / 256); break;
                case 0x34: state.gamepad.bRightTrigger = static_cast<BYTE>(value / 256); break;
            }
        }
    }

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
    
    // 1. Map Thumbsticks to X, Y, Rx, Ry
    dinputState.lX = scaleShortToLong(state.gamepad.sThumbLX);
    dinputState.lY = scaleShortToLong(state.gamepad.sThumbLY);
    dinputState.lRx = scaleShortToLong(state.gamepad.sThumbRX);
    dinputState.lRy = scaleShortToLong(state.gamepad.sThumbRY);
    
    // 2. Map Triggers to Z and Rz
    // XInput ranges: 0 to 255
    // DInput ranges: -32768 to 32767
    dinputState.lZ = static_cast<LONG>(state.gamepad.bLeftTrigger * 257) - 32768;
    dinputState.lRz = static_cast<LONG>(state.gamepad.bRightTrigger * 257) - 32768;
    
    // 3. Map Buttons
    // XInput buttons (WORD) -> DInput buttons (128-byte array)
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_A)              dinputState.rgbButtons[0] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_B)              dinputState.rgbButtons[1] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_X)              dinputState.rgbButtons[2] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_Y)              dinputState.rgbButtons[3] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)   dinputState.rgbButtons[4] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)  dinputState.rgbButtons[5] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_BACK)           dinputState.rgbButtons[6] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_START)          dinputState.rgbButtons[7] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)     dinputState.rgbButtons[8] = 0x80;
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)    dinputState.rgbButtons[9] = 0x80;
    
    // 4. Map D-Pad to POV
    // POV is in hundredths of degrees: North=0, East=9000, South=18000, West=27000, Release=-1
    dinputState.rgdwPOV[0] = static_cast<DWORD>(-1);
    if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) {
        if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) dinputState.rgdwPOV[0] = 4500;
        else if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) dinputState.rgdwPOV[0] = 31500;
        else dinputState.rgdwPOV[0] = 0;
    } else if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) {
        if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) dinputState.rgdwPOV[0] = 13500;
        else if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) dinputState.rgdwPOV[0] = 22500;
        else dinputState.rgdwPOV[0] = 18000;
    } else if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) {
        dinputState.rgdwPOV[0] = 9000;
    } else if (state.gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) {
        dinputState.rgdwPOV[0] = 27000;
    }
    
    // Backward compatibility fields
    dinputState.wButtons = state.gamepad.wButtons;
    dinputState.bLeftTrigger = state.gamepad.bLeftTrigger;
    dinputState.bRightTrigger = state.gamepad.bRightTrigger;
    
    return dinputState;
}

/**
 * @brief Scales a 16-bit signed value to 32-bit signed (for DInput compatibility)
 * 
 * DInput uses LONG (32-bit) for axis values, but the effective range is often
 * still 16-bit. This function safely converts SHORT to LONG while preserving
 * the signed nature of the value.
 * 
 * @param value 16-bit signed input value (-32768 to 32767)
 * @return 32-bit signed output value
 */
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

/**
 * @brief Scales a 32-bit signed value to 16-bit signed (for XInput compatibility)
 * 
 * Converts DInput's 32-bit LONG axis values to XInput's 16-bit SHORT format.
 * Values are clamped to prevent overflow.
 * 
 * @param value 32-bit signed input value
 * @return 16-bit signed output value (-32768 to 32767)
 */
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