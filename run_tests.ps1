<#
.SYNOPSIS
    Test runner for XInput-DInput Translation Layer

.DESCRIPTION
    Builds and runs automated tests using Visual Studio environment
#>

param(
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$BuildDir = "build"

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

Write-Host "`n========================================" -ForegroundColor Magenta
Write-Host "  Translation Layer Test Suite" -ForegroundColor Magenta
Write-Host "========================================`n" -ForegroundColor Magenta

# Clean if requested
if ($Clean -and (Test-Path $BuildDir)) {
    Write-Status "Cleaning build directory..." "Info"
    Remove-Item -Recurse -Force $BuildDir
}

# Find Visual Studio
Write-Status "Detecting Visual Studio..." "Info"
$vsInstallerPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vsInstallerPath)) {
    Write-Status "Visual Studio installer not found" "Error"
    exit 1
}

$vsPath = & $vsInstallerPath -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath 2>$null

if (-not $vsPath) {
    Write-Status "Visual Studio with C++ tools not found" "Error"
    exit 1
}

$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvarsPath)) {
    Write-Status "VC environment script not found" "Error"
    exit 1
}

Write-Status "Found Visual Studio" "Success"

# Create build directory
if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Create temporary batch file
$tempBat = [System.IO.Path]::GetTempFileName()
$tempBat = [System.IO.Path]::ChangeExtension($tempBat, ".bat")
$buildDirAbsolute = Join-Path $PSScriptRoot $BuildDir

$batchContent = @"
@echo off
call "$vcvarsPath" >nul
if errorlevel 1 exit /b 1

if not exist "$buildDirAbsolute" mkdir "$buildDirAbsolute"
cd /d "$buildDirAbsolute"
if errorlevel 1 exit /b 1

echo Configuring tests...
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
if errorlevel 1 exit /b 1

echo.
echo Building tests...
ninja test_translation_layer
if errorlevel 1 exit /b 1

echo.
echo Running tests...
echo ========================================
test_translation_layer.exe
exit /b %ERRORLEVEL%
"@

[System.IO.File]::WriteAllText($tempBat, $batchContent)

try {
    Write-Status "Building and running tests..." "Info"
    Write-Host ""
    
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "cmd.exe"
    $psi.Arguments = "/c `"$tempBat`""
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true
    
    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $psi
    
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
    
    Get-EventSubscriber | Where-Object { $_.SourceObject -eq $process } | Unregister-Event
    
    Write-Host ""
    if ($process.ExitCode -eq 0) {
        Write-Host "========================================" -ForegroundColor Green
        Write-Status "All tests passed!" "Success"
        Write-Host "========================================" -ForegroundColor Green
    } else {
        Write-Host "========================================" -ForegroundColor Red
        Write-Status "Tests failed!" "Error"
        Write-Host "========================================" -ForegroundColor Red
    }
    
    exit $process.ExitCode
    
} catch {
    Write-Status "Error: $_" "Error"
    exit 1
} finally {
    if (Test-Path $tempBat) {
        Remove-Item $tempBat -Force
    }
}
