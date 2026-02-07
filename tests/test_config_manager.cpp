#include <cassert>
#include <iostream>
#include <fstream>
#include <filesystem>
#include "../include/utils/config_manager.hpp"

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::cout << "Running " #name "..."; \
    test_##name(); \
    std::cout << " PASSED\n"; \
} while(0)

#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_TRUE(x) assert(x)
#define ASSERT_FALSE(x) assert(!(x))

TEST(GetSetString) {
    ConfigManager& config = ConfigManager::getInstance();
    
    config.setString("test_key", "test_value");
    ASSERT_EQ(config.getString("test_key"), "test_value");
    ASSERT_EQ(config.getString("nonexistent", "default"), "default");
}

TEST(GetSetInt) {
    ConfigManager& config = ConfigManager::getInstance();
    
    config.setInt("test_int", 42);
    ASSERT_EQ(config.getInt("test_int"), 42);
    ASSERT_EQ(config.getInt("nonexistent", 99), 99);
}

TEST(GetSetFloat) {
    ConfigManager& config = ConfigManager::getInstance();
    
    config.setFloat("test_float", 3.14f);
    float value = config.getFloat("test_float");
    ASSERT_TRUE(std::abs(value - 3.14f) < 0.001f);
    ASSERT_EQ(config.getFloat("nonexistent", 1.5f), 1.5f);
}

TEST(GetSetBool) {
    ConfigManager& config = ConfigManager::getInstance();
    
    config.setBool("test_bool_true", true);
    config.setBool("test_bool_false", false);
    
    ASSERT_TRUE(config.getBool("test_bool_true"));
    ASSERT_FALSE(config.getBool("test_bool_false"));
    ASSERT_TRUE(config.getBool("nonexistent", true));
}

TEST(HasKey) {
    ConfigManager& config = ConfigManager::getInstance();
    
    config.setString("existing_key", "value");
    ASSERT_TRUE(config.hasKey("existing_key"));
    ASSERT_FALSE(config.hasKey("nonexistent_key"));
}

TEST(SaveAndLoad) {
    ConfigManager& config = ConfigManager::getInstance();
    const std::string testFile = "test_config.ini";
    
    // Set some values
    config.setString("string_key", "test_string");
    config.setInt("int_key", 123);
    config.setFloat("float_key", 2.5f);
    config.setBool("bool_key", true);
    
    // Save
    ASSERT_TRUE(config.save(testFile));
    
    // Clear and reload
    ConfigManager& config2 = ConfigManager::getInstance();
    ASSERT_TRUE(config2.load(testFile));
    
    // Verify values
    ASSERT_EQ(config2.getString("string_key"), "test_string");
    ASSERT_EQ(config2.getInt("int_key"), 123);
    ASSERT_TRUE(std::abs(config2.getFloat("float_key") - 2.5f) < 0.001f);
    ASSERT_TRUE(config2.getBool("bool_key"));
    
    // Cleanup
    std::filesystem::remove(testFile);
}

int main() {
    std::cout << "Running Config Manager Tests\n";
    std::cout << "=============================\n\n";
    
    try {
        RUN_TEST(GetSetString);
        RUN_TEST(GetSetInt);
        RUN_TEST(GetSetFloat);
        RUN_TEST(GetSetBool);
        RUN_TEST(HasKey);
        RUN_TEST(SaveAndLoad);
        
        std::cout << "\n=============================\n";
        std::cout << "All tests passed!\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\nTest failed with exception: " << e.what() << "\n";
        return 1;
    }
}
