#include <cassert>
#include <iostream>
#include <cmath>
#include "../include/core/translation_layer.hpp"

// Simple test framework
#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "..."; \
    test_##name(); \
    std::cout << " PASSED\n"; \
} while(0)

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))
#define ASSERT_NEAR(a, b, epsilon) assert(std::abs((a) - (b)) < (epsilon))

TEST(ScaleLongToShort) {
    // Test boundary values
    ASSERT_EQ(TranslationLayer::scaleLongToShort(0), 0);
    ASSERT_EQ(TranslationLayer::scaleLongToShort(32767), 32767);
    ASSERT_EQ(TranslationLayer::scaleLongToShort(-32768), -32768);
    
    // Test scaling
    ASSERT_EQ(TranslationLayer::scaleLongToShort(65535), 32767);
    ASSERT_EQ(TranslationLayer::scaleLongToShort(-65536), -32768);
}

TEST(ScaleShortToLong) {
    // Test boundary values
    ASSERT_EQ(TranslationLayer::scaleShortToLong(0), 0);
    ASSERT_EQ(TranslationLayer::scaleShortToLong(32767), 65535);
    ASSERT_EQ(TranslationLayer::scaleShortToLong(-32768), -65536);
}

TEST(NormalizeShort) {
    ASSERT_NEAR(TranslationLayer::normalizeShort(0), 0.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeShort(32767), 1.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeShort(-32768), -1.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeShort(16384), 0.5f, 0.01f);
}

TEST(NormalizeLong) {
    ASSERT_NEAR(TranslationLayer::normalizeLong(0), 0.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeLong(65535), 1.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeLong(-65536), -1.0f, 0.001f);
}

TEST(NormalizeByte) {
    ASSERT_NEAR(TranslationLayer::normalizeByte(0), 0.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeByte(255), 1.0f, 0.001f);
    ASSERT_NEAR(TranslationLayer::normalizeByte(128), 0.5f, 0.01f);
}

TEST(SOCDCleaningEnabled) {
    TranslationLayer layer;
    layer.setSOCDCleaningEnabled(true);
    layer.setSOCDMethod(2); // Neutral
    
    // Create a state with opposing inputs
    std::vector<ControllerState> inputs(1);
    inputs[0].userId = 0;
    inputs[0].isConnected = true;
    inputs[0].xinputState.dwPacketNumber = 1;
    inputs[0].xinputState.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
    
    auto translated = layer.translate(inputs);
    
    ASSERT_EQ(translated.size(), 1);
    // With neutral SOCD, opposing directions should cancel out
    ASSERT_FALSE(translated[0].gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
    ASSERT_FALSE(translated[0].gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
}

TEST(SOCDCleaningDisabled) {
    TranslationLayer layer;
    layer.setSOCDCleaningEnabled(false);
    
    // Create a state with opposing inputs
    std::vector<ControllerState> inputs(1);
    inputs[0].userId = 0;
    inputs[0].isConnected = true;
    inputs[0].xinputState.dwPacketNumber = 1;
    inputs[0].xinputState.Gamepad.wButtons = XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
    
    auto translated = layer.translate(inputs);
    
    ASSERT_EQ(translated.size(), 1);
    // With SOCD disabled, both directions should remain
    ASSERT_TRUE(translated[0].gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
    ASSERT_TRUE(translated[0].gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
}

TEST(TranslationMappingFlags) {
    TranslationLayer layer;
    
    layer.setXInputToDInputMapping(true);
    ASSERT_TRUE(layer.isXInputToDInputEnabled());
    
    layer.setXInputToDInputMapping(false);
    ASSERT_FALSE(layer.isXInputToDInputEnabled());
    
    layer.setDInputToXInputMapping(true);
    ASSERT_TRUE(layer.isDInputToXInputEnabled());
    
    layer.setDInputToXInputMapping(false);
    ASSERT_FALSE(layer.isDInputToXInputEnabled());
}

TEST(EmptyInputHandling) {
    TranslationLayer layer;
    std::vector<ControllerState> inputs;
    
    auto translated = layer.translate(inputs);
    ASSERT_EQ(translated.size(), 0);
}

TEST(MultipleControllersTranslation) {
    TranslationLayer layer;
    
    std::vector<ControllerState> inputs(3);
    for (int i = 0; i < 3; i++) {
        inputs[i].userId = i;
        inputs[i].isConnected = true;
        inputs[i].xinputState.dwPacketNumber = 1;
        inputs[i].xinputState.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    }
    
    auto translated = layer.translate(inputs);
    ASSERT_EQ(translated.size(), 3);
    
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(translated[i].sourceUserId, i);
        ASSERT_TRUE(translated[i].gamepad.wButtons & XINPUT_GAMEPAD_A);
    }
}

int main() {
    std::cout << "Running Translation Layer Tests\n";
    std::cout << "================================\n\n";
    
    try {
        RUN_TEST(ScaleLongToShort);
        RUN_TEST(ScaleShortToLong);
        RUN_TEST(NormalizeShort);
        RUN_TEST(NormalizeLong);
        RUN_TEST(NormalizeByte);
        RUN_TEST(SOCDCleaningEnabled);
        RUN_TEST(SOCDCleaningDisabled);
        RUN_TEST(TranslationMappingFlags);
        RUN_TEST(EmptyInputHandling);
        RUN_TEST(MultipleControllersTranslation);
        
        std::cout << "\n================================\n";
        std::cout << "All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << "\n";
        return 1;
    }
}
