/**
 * @file test_stick_drift_mitigation.cpp
 * @brief Automated tests for stick drift mitigation feature
 */

#include "core/translation_layer.hpp"
#include <iostream>
#include <cmath>
#include <cassert>
#include <vector>

// Test helper to create a controller state with specific stick values
ControllerState createTestState(SHORT lx, SHORT ly, SHORT rx, SHORT ry) {
    ControllerState state{};
    state.userId = 0;
    state.timestamp = 0;
    state.xinputState.dwPacketNumber = 1;
    state.xinputState.Gamepad.sThumbLX = lx;
    state.xinputState.Gamepad.sThumbLY = ly;
    state.xinputState.Gamepad.sThumbRX = rx;
    state.xinputState.Gamepad.sThumbRY = ry;
    state.xinputState.Gamepad.wButtons = 0;
    state.xinputState.Gamepad.bLeftTrigger = 0;
    state.xinputState.Gamepad.bRightTrigger = 0;
    return state;
}

// Test helper to check if stick is centered (within tolerance)
bool isStickCentered(SHORT x, SHORT y, SHORT tolerance = 100) {
    return std::abs(x) <= tolerance && std::abs(y) <= tolerance;
}

// Test helper to calculate magnitude
float calculateMagnitude(SHORT x, SHORT y) {
    float fx = static_cast<float>(x) / 32767.0f;
    float fy = static_cast<float>(y) / 32767.0f;
    return std::sqrt(fx * fx + fy * fy);
}

void testDeadzoneZerosSmallInputs() {
    std::cout << "Test: Deadzone zeros out small inputs (stick drift)... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    layer.setRightStickDeadzone(0.15f);
    
    // Simulate small drift values (about 10% magnitude)
    std::vector<ControllerState> inputs = {
        createTestState(3000, 2000, -2500, 1500)  // Small drift on both sticks
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    assert(isStickCentered(results[0].gamepad.sThumbLX, results[0].gamepad.sThumbLY));
    assert(isStickCentered(results[0].gamepad.sThumbRX, results[0].gamepad.sThumbRY));
    
    std::cout << "PASSED\n";
}

void testDeadzonePreservesLargeInputs() {
    std::cout << "Test: Deadzone preserves large inputs... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    
    // Full stick deflection
    std::vector<ControllerState> inputs = {
        createTestState(32767, 0, 0, -32767)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    // Should be close to full deflection after scaling
    assert(std::abs(results[0].gamepad.sThumbLX) > 30000);
    assert(std::abs(results[0].gamepad.sThumbRY) > 30000);
    
    std::cout << "PASSED\n";
}

void testScaledRadialDeadzone() {
    std::cout << "Test: Scaled radial deadzone maintains smooth transition... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.2f);
    
    // Test input just above deadzone (25% magnitude)
    SHORT testX = static_cast<SHORT>(32767 * 0.25f * 0.707f);  // 45 degree angle
    SHORT testY = static_cast<SHORT>(32767 * 0.25f * 0.707f);
    
    std::vector<ControllerState> inputs = {
        createTestState(testX, testY, 0, 0)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    // Output should be non-zero but scaled down
    float outputMag = calculateMagnitude(results[0].gamepad.sThumbLX, results[0].gamepad.sThumbLY);
    assert(outputMag > 0.0f);
    assert(outputMag < 0.25f);  // Should be less than input due to scaling
    
    std::cout << "PASSED\n";
}

void testDeadzoneDisabled() {
    std::cout << "Test: Deadzone can be disabled... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(false);
    
    // Small drift values
    std::vector<ControllerState> inputs = {
        createTestState(3000, 2000, 0, 0)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    // Should pass through unchanged when disabled
    assert(results[0].gamepad.sThumbLX == 3000);
    assert(results[0].gamepad.sThumbLY == 2000);
    
    std::cout << "PASSED\n";
}

void testIndependentStickDeadzones() {
    std::cout << "Test: Left and right stick have independent deadzones... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.1f);
    layer.setRightStickDeadzone(0.3f);
    
    // Same magnitude input on both sticks (20%)
    SHORT testVal = static_cast<SHORT>(32767 * 0.2f);
    std::vector<ControllerState> inputs = {
        createTestState(testVal, 0, testVal, 0)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    // Left stick (10% deadzone) should pass through
    assert(std::abs(results[0].gamepad.sThumbLX) > 1000);
    // Right stick (30% deadzone) should be zeroed
    assert(isStickCentered(results[0].gamepad.sThumbRX, results[0].gamepad.sThumbRY));
    
    std::cout << "PASSED\n";
}

void testAntiDeadzone() {
    std::cout << "Test: Anti-deadzone adds minimum output... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.2f);
    layer.setLeftStickAntiDeadzone(0.15f);
    
    // Input just above deadzone (25% magnitude)
    SHORT testX = static_cast<SHORT>(32767 * 0.25f);
    std::vector<ControllerState> inputs = {
        createTestState(testX, 0, 0, 0)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    float outputMag = calculateMagnitude(results[0].gamepad.sThumbLX, results[0].gamepad.sThumbLY);
    // With anti-deadzone, output should be at least 15%
    assert(outputMag >= 0.14f);
    
    std::cout << "PASSED\n";
}

void testDirectionPreservation() {
    std::cout << "Test: Deadzone preserves stick direction... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    
    // Test various angles
    std::vector<std::pair<SHORT, SHORT>> testAngles = {
        {20000, 20000},   // 45 degrees
        {25000, -10000},  // ~-22 degrees
        {-15000, 20000},  // ~127 degrees
        {-20000, -20000}  // -135 degrees
    };
    
    for (const auto& [x, y] : testAngles) {
        std::vector<ControllerState> inputs = {
            createTestState(x, y, 0, 0)
        };
        
        auto results = layer.translate(inputs);
        assert(results.size() == 1);
        
        // Calculate input and output angles
        float inputAngle = std::atan2(y, x);
        float outputAngle = std::atan2(results[0].gamepad.sThumbLY, results[0].gamepad.sThumbLX);
        
        // Angles should be very close (within 1 degree)
        float angleDiff = std::abs(inputAngle - outputAngle);
        assert(angleDiff < 0.02f || angleDiff > 6.26f);  // Account for wrap-around
    }
    
    std::cout << "PASSED\n";
}

void testFullRangeOutput() {
    std::cout << "Test: Full stick deflection produces near-maximum output... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    
    // Maximum deflection in all cardinal directions
    std::vector<std::pair<SHORT, SHORT>> maxInputs = {
        {32767, 0},      // Right
        {0, 32767},      // Up
        {-32768, 0},     // Left
        {0, -32768}      // Down
    };
    
    for (const auto& [x, y] : maxInputs) {
        std::vector<ControllerState> inputs = {
            createTestState(x, y, 0, 0)
        };
        
        auto results = layer.translate(inputs);
        assert(results.size() == 1);
        
        float outputMag = calculateMagnitude(results[0].gamepad.sThumbLX, results[0].gamepad.sThumbLY);
        // Should be close to 1.0 (allowing for rounding)
        assert(outputMag > 0.95f);
    }
    
    std::cout << "PASSED\n";
}

void testBothSticksSimultaneously() {
    std::cout << "Test: Both sticks processed independently... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    layer.setRightStickDeadzone(0.15f);
    
    // Left stick with drift, right stick with full deflection
    std::vector<ControllerState> inputs = {
        createTestState(2000, 1500, 32767, 0)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    // Left stick should be zeroed
    assert(isStickCentered(results[0].gamepad.sThumbLX, results[0].gamepad.sThumbLY));
    // Right stick should be near maximum
    assert(std::abs(results[0].gamepad.sThumbRX) > 30000);
    
    std::cout << "PASSED\n";
}

void testEdgeCaseZeroInput() {
    std::cout << "Test: Zero input remains zero... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    
    std::vector<ControllerState> inputs = {
        createTestState(0, 0, 0, 0)
    };
    
    auto results = layer.translate(inputs);
    
    assert(results.size() == 1);
    assert(results[0].gamepad.sThumbLX == 0);
    assert(results[0].gamepad.sThumbLY == 0);
    assert(results[0].gamepad.sThumbRX == 0);
    assert(results[0].gamepad.sThumbRY == 0);
    
    std::cout << "PASSED\n";
}

void testDeadzoneValueClamping() {
    std::cout << "Test: Deadzone values are clamped to valid range... ";
    
    TranslationLayer layer;
    
    // Try to set invalid values
    layer.setLeftStickDeadzone(-0.5f);
    assert(layer.getLeftStickDeadzone() == 0.0f);
    
    layer.setLeftStickDeadzone(1.5f);
    assert(layer.getLeftStickDeadzone() == 1.0f);
    
    layer.setRightStickDeadzone(0.25f);
    assert(layer.getRightStickDeadzone() == 0.25f);
    
    std::cout << "PASSED\n";
}

void testXInputAndDInputBothSupported() {
    std::cout << "Test: Deadzone works for both XInput and DInput sources... ";
    
    TranslationLayer layer;
    layer.setStickDeadzoneEnabled(true);
    layer.setLeftStickDeadzone(0.15f);
    
    // Test XInput source
    ControllerState xinputState = createTestState(3000, 2000, 0, 0);
    std::vector<ControllerState> xinputInputs = {xinputState};
    auto xinputResults = layer.translate(xinputInputs);
    assert(xinputResults.size() == 1);
    assert(isStickCentered(xinputResults[0].gamepad.sThumbLX, xinputResults[0].gamepad.sThumbLY));
    
    // Test HID/DInput source (simulated)
    ControllerState dinputState{};
    dinputState.userId = -1;
    dinputState.devicePath = L"\\\\?\\hid#test";
    dinputState.productName = L"Test Controller";
    dinputState.timestamp = 0;
    dinputState.m_hidValues[0x30] = 128 + 23;  // Small X drift
    dinputState.m_hidValues[0x31] = 128 + 15;  // Small Y drift
    
    std::vector<ControllerState> dinputInputs = {dinputState};
    auto dinputResults = layer.translate(dinputInputs);
    assert(dinputResults.size() == 1);
    assert(isStickCentered(dinputResults[0].gamepad.sThumbLX, dinputResults[0].gamepad.sThumbLY));
    
    std::cout << "PASSED\n";
}

int main() {
    std::cout << "=== Stick Drift Mitigation Tests ===\n\n";
    
    try {
        testDeadzoneZerosSmallInputs();
        testDeadzonePreservesLargeInputs();
        testScaledRadialDeadzone();
        testDeadzoneDisabled();
        testIndependentStickDeadzones();
        testAntiDeadzone();
        testDirectionPreservation();
        testFullRangeOutput();
        testBothSticksSimultaneously();
        testEdgeCaseZeroInput();
        testDeadzoneValueClamping();
        testXInputAndDInputBothSupported();
        
        std::cout << "\n=== All tests PASSED ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest FAILED with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "\nTest FAILED with unknown exception\n";
        return 1;
    }
}
