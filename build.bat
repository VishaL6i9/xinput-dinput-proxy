@echo off
setlocal enabledelayedexpansion

echo Setting up build environment for XInput-DInput Proxy...

REM Try to find Visual Studio installation
set "VSFOUND=false"
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSINSTALLDIR=%%i"
)

if not defined VSINSTALLDIR (
    echo Error: Could not find Visual Studio with C++ tools installed.
    echo Please make sure Visual Studio with C++ development tools is installed.
    pause
    exit /b 1
)

echo Found Visual Studio at: !VSINSTALLDIR!

REM Set up the Visual Studio environment
call "!VSINSTALLDIR!\VC\Auxiliary\Build\vcvars64.bat"

REM Create build directory if it doesn't exist
if not exist "build" mkdir build

REM Change to build directory
cd build

echo Generating build files with CMake using Ninja...
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    pause
    exit /b %ERRORLEVEL%
)

echo Building with Ninja...
ninja

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
echo Executable is located at: build\xinput_dinput_proxy.exe
echo.
echo You can run the application with: build\xinput_dinput_proxy.exe
pause