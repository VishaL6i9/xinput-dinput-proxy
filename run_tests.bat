@echo off
REM Automated test runner for XInput-DInput Translation Layer
REM This script builds and runs all tests

echo ================================================
echo XInput-DInput Translation Layer Test Suite
echo ================================================
echo.

REM Check if build directory exists
if not exist "build" (
    echo Creating build directory...
    mkdir build
)

cd build

echo.
echo [1/3] Configuring CMake...
cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    cd ..
    exit /b 1
)

echo.
echo [2/3] Building tests...
cmake --build . --target test_translation_layer
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    cd ..
    exit /b 1
)

echo.
echo [3/3] Running tests...
echo ================================================
echo.

REM Run the translation layer tests
if exist "tests\test_translation_layer.exe" (
    tests\test_translation_layer.exe
    set TEST_RESULT=%ERRORLEVEL%
) else if exist "test_translation_layer.exe" (
    test_translation_layer.exe
    set TEST_RESULT=%ERRORLEVEL%
) else (
    echo ERROR: Test executable not found!
    cd ..
    exit /b 1
)

cd ..

echo.
echo ================================================
if %TEST_RESULT% EQU 0 (
    echo Test suite completed successfully!
    echo All technical debt fixes verified.
) else (
    echo Test suite failed!
    echo Please review the output above.
)
echo ================================================

exit /b %TEST_RESULT%
