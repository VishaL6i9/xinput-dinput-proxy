cmake_minimum_required(VERSION 3.20)
project(xinput_dinput_proxy VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer Ninja generator if available
if(NOT CMAKE_GENERATOR)
    set(CMAKE_GENERATOR "Ninja" CACHE STRING "Generator to use" FORCE)
endif()

# Compiler-specific options
if(MSVC)
    # MSVC-specific flags
    add_compile_options(/EHsc)
    add_compile_options(/wd4005)  # Disable macro redefinition warnings
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    
    # Use Static Runtime (/MT) to match ViGEmClient.lib
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Performance flags only for Release builds
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /Ob2 /Oi /Ot /arch:AVX2)
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC/Clang flags for performance (Release only)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -march=native -mtune=native)
    endif()
endif()

# Add FTXUI as a subdirectory (assuming it's cloned in ext/)
# For now, we'll add it as an external dependency
include(FetchContent)

FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG        v5.0.0
)

FetchContent_MakeAvailable(ftxui)

# Add executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/core/input_capture.cpp
    src/core/translation_layer.cpp
    src/core/virtual_device_emulator.cpp
    src/core/device_manager.cpp
    src/ui/dashboard.cpp
    src/utils/timing.cpp
    src/utils/threading.cpp
    src/utils/hidhide_controller.cpp
    src/utils/config_manager.cpp
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ftxui::screen
    ftxui::dom
    ftxui::component
    dxguid.lib
    xinput.lib
    setupapi.lib
    cfgmgr32.lib
    ole32.lib
    uuid.lib
    hid.lib
    winmm.lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/ViGEmClient.lib
)

# Include ViGEmBus headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Define preprocessor macros for Windows 11
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

# Set Windows subsystem to console
set_target_properties(${PROJECT_NAME} PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE"
)

# Enforce Administrator privileges on Windows with MSVC
if(MSVC)
    set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY 
        LINK_FLAGS " /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

# Copy config.ini template to build directory if it doesn't exist
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config.ini
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/config.ini
    COMMENT "Copying config.ini template to build directory"
)

# Testing
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # Test for Config Manager
    add_executable(test_config_manager
        tests/test_config_manager.cpp
        src/utils/config_manager.cpp
    )
    target_include_directories(test_config_manager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    add_test(NAME ConfigManagerTest COMMAND test_config_manager)
    
    # Test for Translation Layer
    add_executable(test_translation_layer
        tests/test_translation_layer.cpp
        src/core/translation_layer.cpp
        src/utils/timing.cpp
    )
    target_include_directories(test_translation_layer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_translation_layer
        hid.lib
        winmm.lib
    )
    add_test(NAME TranslationLayerTest COMMAND test_translation_layer)
    
    # Test for Stick Drift Mitigation
    add_executable(test_stick_drift_mitigation
        tests/test_stick_drift_mitigation.cpp
        src/core/translation_layer.cpp
        src/utils/timing.cpp
    )
    target_include_directories(test_stick_drift_mitigation PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(test_stick_drift_mitigation
        hid.lib
        winmm.lib
    )
    add_test(NAME StickDriftMitigationTest COMMAND test_stick_drift_mitigation)
endif()
