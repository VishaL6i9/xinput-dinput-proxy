# PowerShell script to build the project using Visual Studio environment
Write-Host "Setting up build environment for XInput-DInput Proxy..." -ForegroundColor Green

# Try to find Visual Studio installation
$vsInstallerPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (Test-Path $vsInstallerPath) {
    $vsPath = & $vsInstallerPath -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($vsPath) {
        $vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        
        if (Test-Path $vcvarsPath) {
            Write-Host "Found Visual Studio at: $vsPath" -ForegroundColor Yellow
            Write-Host "Setting up Visual Studio environment..." -ForegroundColor Cyan
            
            # Create a temporary batch file to set up the environment and run cmake
            $tempBat = [System.IO.Path]::GetTempFileName()
            $tempBat = [System.IO.Path]::ChangeExtension($tempBat, ".bat")
            
            # Create the batch file content with proper error handling
            $batchContent = "@echo off`r`n" +
                           "setlocal enabledelayedexpansion`r`n`r`n" +
                           "REM Set up the Visual Studio environment`r`n" +
                           "call `"$vcvarsPath`"`r`n`r`n" +
                           "if errorlevel 1 (`r`n" +
                           "    echo Failed to set up Visual Studio environment`r`n" +
                           "    pause`r`n" +
                           "    exit /b 1`r`n" +
                           ")`r`n`r`n" +
                           "echo Visual Studio environment set up successfully`r`n`r`n" +
                           "REM Create build directory if it doesn't exist`r`n" +
                           "if not exist `"build`" mkdir build`r`n`r`n" +
                           "REM Change to build directory`r`n" +
                           "cd build`r`n`r`n" +
                           "echo Generating build files with CMake using Ninja...`r`n" +
                           "cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release`r`n`r`n" +
                           "if errorlevel 1 (`r`n" +
                           "    echo CMake configuration failed!`r`n" +
                           "    pause`r`n" +
                           "    exit /b 1`r`n" +
                           ")`r`n`r`n" +
                           "echo Building with Ninja...`r`n" +
                           "ninja`r`n`r`n" +
                           "if errorlevel 1 (`r`n" +
                           "    echo Build failed!`r`n" +
                           "    pause`r`n" +
                           "    exit /b 1`r`n" +
                           ")`r`n`r`n" +
                           "echo.`r`n" +
                           "echo Build completed successfully!`r`n" +
                           "echo Executable is located at: $(Join-Path (Get-Location) `"build\xinput_dinput_proxy.exe`")`r`n" +
                           "echo.`r`n" +
                           "pause`r`n"
            
            [System.IO.File]::WriteAllText($tempBat, $batchContent)
            
            Write-Host "Running build with Visual Studio environment..." -ForegroundColor Cyan
            cmd /c $tempBat
            
            # Clean up
            Remove-Item $tempBat
        } else {
            Write-Host "Could not find VC environment script at: $vcvarsPath" -ForegroundColor Red
        }
    } else {
        Write-Host "Could not find Visual Studio with C++ tools." -ForegroundColor Red
        Write-Host "Please make sure Visual Studio with C++ development tools is installed." -ForegroundColor Yellow
    }
} else {
    Write-Host "Could not find Visual Studio installer at: $vsInstallerPath" -ForegroundColor Red
    Write-Host "Please make sure Visual Studio is installed." -ForegroundColor Yellow
}