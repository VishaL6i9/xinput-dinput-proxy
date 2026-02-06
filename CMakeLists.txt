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
    # MSVC-specific flags for performance
    add_compile_options(/O2 /Ob2 /Oi /Ot /arch:AVX2 /EHsc)
    add_compile_options(/wd4005)  # Disable macro redefinition warnings
    add_compile_definitions(NOMINMAX)
    add_compile_definitions(WIN32_LEAN_AND_MEAN)
    
    # Use Static Runtime (/MT) to match ViGEmClient.lib
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC/Clang flags for performance
    add_compile_options(-O3 -march=native -mtune=native)
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
    src/ui/dashboard.cpp
    src/utils/timing.cpp
    src/utils/threading.cpp
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