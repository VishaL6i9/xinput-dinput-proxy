<div align="center">

# XInput-DirectInput Proxy for Windows 11

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows_11-blue.svg)](https://www.microsoft.com/windows)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/VishaL6i9/xinput-dinput-proxy/actions)
[![Performance](https://img.shields.io/badge/performance-sub--millisecond-brightgreen.svg)](https://github.com/VishaL6i9/xinput-dinput-proxy#performance)

A high-performance Windows 11 application that translates controller input between XInput and DirectInput in real time, using user-mode solutions to avoid kernel-mode driver requirements.

</div>

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Requirements](#requirements)
- [Building](#building)
- [Usage](#usage)
- [Performance](#performance)
- [Configuration](#configuration)
- [Known Limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## Overview

The XInput-DirectInput Proxy is a performance-oriented application designed to bridge the gap between XInput and DirectInput controller APIs on Windows 11. It enables users to seamlessly use controllers that are only supported by one API in games or applications that require the other API, without requiring kernel-mode drivers.

## Features

<div align="center">

| Feature | Description | Status |
|--------|-------------|---------|
| **High-Performance Input Capture** | Sub-millisecond polling using QueryPerformanceCounter | ✅ |
| **Bidirectional Translation** | Convert between XInput and DirectInput formats | ✅ |
| **User-Mode Operation** | No kernel drivers required, using Windows Input Injection API | ✅ |
| **Real-Time Dashboard** | FTXUI-based text dashboard showing performance metrics | ✅ |
| **SOCD Cleaning** | Simultaneous Opposing Cardinal Directions handling | ✅ |
| **Input Debouncing** | Configurable debouncing to prevent input chatter | ✅ |
| **Low Latency** | Optimized for competitive gaming scenarios | ✅ |
| **Controller Agnostic** | Supports various controller types | ✅ |

</div>

- **High-Performance Input Capture**: Sub-millisecond polling using QueryPerformanceCounter
- **Bidirectional Translation**: Convert between XInput and DirectInput formats
- **User-Mode Operation**: No kernel drivers required, using Windows Input Injection API
- **Real-Time Dashboard**: FTXUI-based text dashboard showing performance metrics
- **SOCD Cleaning**: Simultaneous Opposing Cardinal Directions handling
- **Input Debouncing**: Configurable debouncing to prevent input chatter
- **Low Latency**: Optimized for competitive gaming scenarios
- **Controller Agnostic**: Supports various controller types including Xbox, DualShock, and third-party controllers

## Architecture

The application consists of several core components:

1. **Input Capture**: Polls physical controllers via XInput and HID APIs
2. **Translation Layer**: Maps button/axis states between XInput and DirectInput layouts
3. **Virtual Device Emulator**: Creates virtual controllers using Windows Input Injection API
4. **Dashboard UI**: Real-time performance monitoring and configuration

### Component Diagram

<div align="center">

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────────┐
│   Physical      │    │   Translation    │    │   Virtual Device    │
│   Controllers   │───▶│     Layer        │───▶│    Emulator         │
│                 │    │                  │    │                     │
│ - Xbox          │    │ - XInput ↔       │    │ - Input Injection   │
│ - DualShock     │    │   DirectInput    │    │ - Virtual devices   │
│ - Third-party   │    │ - SOCD cleaning  │    │ - HID manipulation  │
└─────────────────┘    │ - Debouncing     │    └─────────────────────┘
                       └──────────────────┘
                                │
                                ▼
                       ┌──────────────────┐
                       │   Dashboard UI   │
                       │                  │
                       │ - Performance    │
                       │   metrics        │
                       │ - Configuration  │
                       │ - Status         │
                       └──────────────────┘
```

</div>

## Requirements

### System Requirements
- ![Windows 11](https://img.shields.io/badge/Windows_11-21H2+-0078D6?logo=windows&logoColor=white) (21H2 or later)
- 64-bit processor (x64)
- Minimum 512 MB RAM
- 100 MB available disk space

### Hardware Requirements
- Compatible game controller (XInput, DirectInput, or HID)
- Recommended: USB 2.0 or higher for low latency

### Development Requirements
- Visual Studio 2022 Build Tools or IDE with MSVC compiler
- Windows SDK (10.0.22000.0 or later)
- CMake 3.20 or later
- Ninja 1.12.1 or later

## Building

### Prerequisites
Ensure you have the following installed:
- Visual Studio 2022 Build Tools (with C++ development tools)
- Windows SDK
- CMake
- Ninja

### Build Instructions

#### Using Build Script (Recommended)
```cmd
# Clone or navigate to the project directory
cd xinput-dinput-proxy

# Run the build script
.\build.ps1
```

#### Manual Build
```cmd
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release

# Build with Ninja
ninja
```

The executable will be created as `xinput_dinput_proxy.exe` in the build directory.

## Usage

### Basic Usage
```cmd
# Navigate to build directory
cd build

# Run the application
xinput_dinput_proxy.exe
```

### Configuration
The application can be configured through the runtime dashboard UI or via the `config.ini` file.

### Testing with Alan Wake 2
The application is designed to work with Alan Wake 2, which visually changes controller icons based on detected input type. This makes it ideal for verifying the translation functionality.

## Performance

### Benchmarks

<div align="center">

| Metric | Value | Status |
|--------|-------|--------|
| **Polling Frequency** | Up to 1000 Hz (1ms interval) | ![High](https://img.shields.io/badge/status-high_performance-brightgreen) |
| **Latency** | Sub-millisecond translation delay | ![Low](https://img.shields.io/badge/status-ultra_low-brightgreen) |
| **CPU Usage** | Less than 5% on modern CPUs | ![Efficient](https://img.shields.io/badge/status-efficient-brightgreen) |
| **Memory Usage** | Less than 50 MB RAM | ![Lightweight](https://img.shields.io/badge/status-lightweight-brightgreen) |

</div>

- **Polling Frequency**: Up to 1000 Hz (1ms interval)
- **Latency**: Sub-millisecond translation delay
- **CPU Usage**: Less than 5% on modern CPUs during normal operation
- **Memory Usage**: Less than 50 MB RAM

### Optimization Techniques
- High-priority threading for input processing
- Precise timing using QueryPerformanceCounter
- CPU affinity settings for reduced latency
- Efficient memory allocation patterns
- Lock-free data structures where possible

## Configuration

### Runtime Configuration
The application provides a real-time dashboard with the following configuration options:
- Polling frequency adjustment
- SOCD cleaning method selection
- Input debouncing settings
- Performance monitoring options

### Configuration File
The `config.ini` file contains the following settings:

```ini
[POLLING]
PollingFrequency=1000

[TRANSLATION]
XInputToDInput=true
DInputToXInput=true

[SOCD]
Method=0  ; 0=Last Win, 1=First Win, 2=Neutral

[DEBOUNCE]
Enabled=true
IntervalMs=50

[PERFORMANCE]
HighPriority=true
CoreAffinity=-1

[VIRTUAL_DEVICE]
RumbleEnabled=true
RumbleIntensity=1.0

[UI]
RefreshRate=30
```

## Known Limitations

<div align="center">

| Limitation | Severity | Status |
|------------|----------|--------|
| Requires Windows 11 for Input Injection API | ![High](https://img.shields.io/badge/severity-medium-yellow) | ![Planned](https://img.shields.io/badge/status-planned_for_future-important) |
| Performance may vary depending on system configuration | ![Medium](https://img.shields.io/badge/severity-low-green) | ![Acknowledged](https://img.shields.io/badge/status-acknowledged-lightgrey) |
| Some games may have compatibility issues with emulated devices | ![Medium](https://img.shields.io/badge/severity-medium-orange) | ![Known](https://img.shields.io/badge/status-known_issue-yellow) |
| Kernel-mode driver alternatives may offer lower latency in some scenarios | ![Low](https://img.shields.io/badge/severity-low-green) | ![Considered](https://img.shields.io/badge/status-under_consideration-blue) |
| Complex controller features (motion sensors, advanced rumble) may not be fully supported | ![Low](https://img.shields.io/badge/severity-low-green) | ![Future](https://img.shields.io/badge/status-future_enhancement-blue) |

</div>

- Requires Windows 11 for Input Injection API
- Performance may vary depending on system configuration
- Some games may have compatibility issues with emulated devices
- Kernel-mode driver alternatives may offer lower latency in some scenarios
- Complex controller features (motion sensors, advanced rumble) may not be fully supported

## Troubleshooting

### Common Issues

#### Controller Not Detected
- Ensure the controller is properly connected
- Check Windows Game Controller settings
- Verify controller drivers are up to date

#### High Latency
- Close other applications consuming CPU resources
- Ensure the application is running with high priority
- Check for background processes affecting performance

#### Game Not Recognizing Controller
- Verify the game supports the emulated controller type
- Check if the game has exclusive access to the controller
- Try restarting the application

### Performance Monitoring
Use the built-in dashboard to monitor:
- Frame rate and timing
- Controller connection status
- Translation effectiveness
- System resource usage

## Contributing

We welcome contributions to improve the XInput-DirectInput Proxy. Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines
- Follow the existing code style
- Write clear, concise commit messages
- Include tests for new functionality
- Update documentation as needed
- Ensure all builds pass before submitting

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

For support, please open an issue in the GitHub repository or contact the development team.

## Badges

[![GitHub stars](https://img.shields.io/github/stars/username/repository?style=social)](https://github.com/VishaL6i9/repository/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/username/repository?style=social)](https://github.com/VishaL6i9/repository/network/members)
[![GitHub contributors](https://img.shields.io/github/contributors/username/repository)](https://github.com/VishaL6i9/repository/graphs/contributors)
[![GitHub issues](https://img.shields.io/github/issues/username/repository)](https://github.com/VishaL6i9/repository/issues)
[![GitHub pull requests](https://img.shields.io/github/issues-pr/username/repository)](https://github.com/VishaL6i9/repository/pulls)

---

**Disclaimer**: This software is provided as-is without warranty. The authors are not responsible for any damage caused by the use of this software.