<#
.SYNOPSIS
    Build script for XInput-DirectInput Proxy using Visual Studio environment

.DESCRIPTION
    This script automatically detects Visual Studio installation, sets up the
    build environment, and compiles the project using CMake and Ninja.

.PARAMETER Clean
    Clean the build directory before building

.PARAMETER BuildType
    Build configuration type (Release or Debug). Default: Release

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Clean
    .\build.ps1 -BuildType Debug
#>

param(
    [switch]$Clean,
    [ValidateSet("Release", "Debug")]
    [string]$BuildType = "Release"
)

# Configuration
$ErrorActionPreference = "Stop"
$ProjectName = "XInput-DirectInput Proxy"
$BuildDir = "build"
$ExeName = "xinput_dinput_proxy.exe"

# Helper function for colored output
function Write-Status {
    param([string]$Message, [string]$Type = "Info")
    
    switch ($Type) {
        "Success" { Write-Host "✓ $Message" -ForegroundColor Green }
        "Error"   { Write-Host "✗ $Message" -ForegroundColor Red }
        "Warning" { Write-Host "⚠ $Message" -ForegroundColor Yellow }
        "Info"    { Write-Host "→ $Message" -ForegroundColor Cyan }
        default   { Write-Host $Message }
    }
}

# Print header
Write-Host "`n========================================" -ForegroundColor Magenta
Write-Host "  $ProjectName Build Script" -ForegroundColor Magenta
Write-Host "========================================`n" -ForegroundColor Magenta

# Clean build directory if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Status "Cleaning build directory..." "Info"
    Remove-Item -Recurse -Force $BuildDir
    Write-Status "Build directory cleaned" "Success"
}

# Find Visual Studio installation
Write-Status "Detecting Visual Studio installation..." "Info"
$vsInstallerPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vsInstallerPath)) {
    Write-Status "Visual Studio installer not found at: $vsInstallerPath" "Error"
    Write-Host "`nPlease install Visual Studio 2022 with C++ Desktop Development workload." -ForegroundColor Yellow
    exit 1
}

# Get Visual Studio installation path
$vsPath = & $vsInstallerPath -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath 2>$null

if (-not $vsPath) {
    Write-Status "Visual Studio with C++ tools not found" "Error"
    Write-Host "`nPlease install Visual Studio 2022 with C++ Desktop Development workload." -ForegroundColor Yellow
    exit 1
}

$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"

if (-not (Test-Path $vcvarsPath)) {
    Write-Status "VC environment script not found at: $vcvarsPath" "Error"
    exit 1
}

Write-Status "Found Visual Studio at: $vsPath" "Success"

# Check for Ninja
Write-Status "Checking for Ninja build system..." "Info"
$ninjaCheck = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninjaCheck) {
    Write-Status "Ninja not found in PATH" "Warning"
    Write-Host "  Ninja will be used from Visual Studio installation" -ForegroundColor Gray
}

# Create build directory
if (-not (Test-Path $BuildDir)) {
    Write-Status "Creating build directory..." "Info"
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Create temporary batch file for build
$tempBat = [System.IO.Path]::GetTempFileName()
$tempBat = [System.IO.Path]::ChangeExtension($tempBat, ".bat")

# Get absolute path to build directory
$buildDirAbsolute = Join-Path $PSScriptRoot $BuildDir

$batchContent = @"
@echo off
setlocal enabledelayedexpansion

REM Set up Visual Studio environment
call "$vcvarsPath" >nul
if errorlevel 1 (
    echo ERROR: Failed to set up Visual Studio environment
    exit /b 1
)

REM Change to build directory
if not exist "$buildDirAbsolute" mkdir "$buildDirAbsolute"
cd /d "$buildDirAbsolute"
if errorlevel 1 (
    echo ERROR: Failed to change to build directory
    exit /b 1
)

REM Run CMake
echo Running CMake configuration...
cmake .. -GNinja -DCMAKE_BUILD_TYPE=$BuildType
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    exit /b 1
)

REM Build with Ninja
echo.
echo Building project...
ninja
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

exit /b 0
"@

[System.IO.File]::WriteAllText($tempBat, $batchContent)

try {
    # Run CMake configuration
    Write-Status "Configuring project with CMake ($BuildType)..." "Info"
    Write-Host ""
    
    # Execute the batch file and capture output in real-time
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $psi.Arguments = "/c `"$tempBat`""
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    
    # Event handlers for real-time output
    $outputHandler = {
        if (-not [string]::IsNullOrEmpty($EventArgs.Data)) {
            Write-Host $EventArgs.Data
        }
    }
    
    $errorHandler = {
        if (-not [string]::IsNullOrEmpty($EventArgs.Data)) {
            Write-Host $EventArgs.Data -ForegroundColor Red
        }
    }
    
    Register-ObjectEvent -InputObject $process -EventName OutputDataReceived -Action $outputHandler | Out-Null
    Register-ObjectEvent -InputObject $process -EventName ErrorDataReceived -Action $errorHandler | Out-Null
    
    $process.Start() | Out-Null
    $process.BeginOutputReadLine()
    $process.BeginErrorReadLine()
    $process.WaitForExit()
    
    # Clean up event handlers
    Get-EventSubscriber | Where-Object { $_.SourceObject -eq $process } | Unregister-Event
    
    if ($process.ExitCode -ne 0) {
        Write-Host ""
        Write-Status "Build process failed with exit code $($process.ExitCode)" "Error"
        exit 1
    }
    
    # Check if executable was created
    $exePath = Join-Path $PSScriptRoot (Join-Path $BuildDir $ExeName)
    if (Test-Path $exePath) {
        Write-Host "`n========================================" -ForegroundColor Green
        Write-Status "Build completed successfully!" "Success"
        Write-Host "========================================" -ForegroundColor Green
        Write-Host "`nExecutable location:" -ForegroundColor Cyan
        Write-Host "  $exePath" -ForegroundColor White
        Write-Host "`nTo run the application:" -ForegroundColor Cyan
        Write-Host "  .\$BuildDir\$ExeName" -ForegroundColor White
        Write-Host ""
    } else {
        Write-Status "Build completed but executable not found" "Warning"
    }
    
} catch {
    Write-Status "An error occurred during build: $_" "Error"
    exit 1
} finally {
    # Clean up temporary batch file
    if (Test-Path $tempBat) {
        Remove-Item $tempBat -Force
    }
}