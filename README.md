<div align="center">

# XInput-DirectInput Proxy for Windows 11

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows_11-blue.svg)](https://www.microsoft.com/windows)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![Status](https://img.shields.io/badge/status-active-brightgreen.svg)]()

**High-performance, user-mode controller emulation bridging the gap between XInput and DirectInput APIs through robust HID parsing and Virtual Device handling.**

</div>

---

## Overview

The **XInput-DirectInput Proxy** is a specialized tool designed to solve compatibility issues between modern Gamepads (XInput) and legacy applications or peripherals (DirectInput/HID) on Windows 11. 

By leveraging **low-latency HID parsing** and the **ViGEmBus** kernel-mode driver for virtual device emulation, this proxy enables:
*   Using generic HID controllers in XInput-only games.
*   Using Xbox controllers in legacy DirectInput games.
*   Advanced input manipulation like SOCD cleaning and debouncing for competitive gaming.

Unlike basic wrappers, this project parses raw HID reports directly from the Windows API, ensuring maximum compatibility and minimal latency.

## Key Features

*   **Sub-millisecond Latency:** Optimized polling loop using `QueryPerformanceCounter` targeting 1000Hz+ refresh rates.
*   **Universal Translation:**
    *   **HID to XInput:** Translates generic joystick/gamepad inputs to standard X360 instructions.
    *   **XInput to DirectInput:** Maps Xbox inputs to standard DirectInput axes and buttons (DualShock 4 emulation).
*   **Advanced HID Parsing:** Uses Windows `HidP_` APIs to correctly interpret buttons and axes from any HID-compliant device, regardless of vendor.
*   **Smart Input Processing:**
    *   **SOCD Cleaning:** Configurable resolution for Simultaneous Opposing Cardinal Directions (Last-Win, First-Win, Neutral).
    *   **Anti-Deadzone & Scaling:** Mathematical scaling for 8-bit, 16-bit, and 32-bit axis data to prevent truncation.
    *   **Debouncing:** Logic to filter out mechanical switch noise.
*   **Interactive Dashboard:** Real-time CLI dashboard (built with FTXUI) with:
    *   Live controller detection and connection status
    *   Input test panel with button press tracking (green = pressed, blue = tested, dim = untested)
    *   Real-time performance metrics (FPS, latency, frame time)
    *   Interactive configuration controls (SOCD, debouncing, HidHide, translation toggle)
    *   Rumble/vibration testing with intensity control
*   **Configuration System:** INI-based settings with runtime updates and persistence
*   **Crash-Resistant Logging:** Continuous auto-save logging that survives crashes for debugging
*   **HidHide Integration:** Automatic physical device hiding to prevent double-input issues
*   **Dynamic Device Management:** Automatically creates and destroys virtual devices as physical controllers are plugged in or removed
*   **Duplicate Prevention:** Smart device tracking prevents multiple HID interfaces from the same controller filling all slots

## Architecture

The system is built on a modular four-layer architecture designed for separation of concerns and speed:

1.  **Input Capture Layer:** 
    *   Polls physical XInput devices via `XInputGetState`.
    *   Reads raw HID reports using Windows HID API (`HidD_GetPreparsedData`, `HidP_GetUsages`).
2.  **Translation Layer:**
    *   Normalizes disparate input formats into a standardized `TranslatedState`.
    *   Applies SOCD cleaning, deadzones, and remapping logic.
3.  **Emulation Layer (Output):**
    *   Interfaces with **ViGEmBus** to spawn virtual Xbox 360 or DualShock 4 controllers.
    *   Injects translated state into the OS kernel.
4.  **Presentation Layer:**
    *   Renders the CLI dashboard on a separate thread to ensure input processing never stalls.

## Getting Started

### Prerequisites

*   **OS:** Windows 10 or 11 (64-bit)
*   **Drivers:** 
    *   **ViGEmBus Driver** (REQUIRED): [Community Fork](https://github.com/nefarius/ViGEmBus/releases) - The original ViGEmBus was retired in 2023. Use the community-maintained fork.
    *   **HidHide Driver** (OPTIONAL): [Download](https://github.com/nefarius/HidHide/releases) - Required only if you want to hide physical devices from games.
*   **Build Tools:** Visual Studio 2022 with C++ Desktop Development workload.
*   **Dependencies:** CMake (3.20+) and Ninja (recommended).
*   **Permissions:** Administrator privileges required for driver access.

> **Warning:** Without ViGEmBus, the application will run in "Input Test Mode" where you can see controller inputs but cannot create virtual devices. HidHide is optional but recommended for best compatibility.

### Building the Project

This project includes automated scripts for easy building.

**1. Clone the repository**
```bash
git clone https://github.com/VishaL6i9/xinput-dinput-proxy.git
cd xinput-dinput-proxy
```

**2. Run the Build Script**
Simply execute the PowerShell script. It will detect your Visual Studio installation, configure CMake with Ninja, and build the project.
```powershell
.\build.ps1
```

**Alternative: Manual Build**
```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
ninja
```

### Usage

1.  **Install ViGEmBus driver** (if not already installed):
    - Download from [ViGEmBus Community Fork](https://github.com/nefarius/ViGEmBus/releases)
    - Run installer as administrator
    - Restart your computer

2.  **(Optional) Install HidHide driver** for device masking:
    - Download from [HidHide Releases](https://github.com/nefarius/HidHide/releases)
    - Run installer as administrator
    - Restart your computer

3.  **Run the proxy executable** as administrator:
    ```bash
    .\build\xinput_dinput_proxy.exe
    ```

4.  The interactive dashboard will launch, showing:
    - Connected controllers with real-time status
    - Input test panel for verifying button presses
    - Performance metrics (FPS, latency)
    - Configuration controls

5.  **Test your controller inputs:**
    - Press buttons to see them light up green
    - Successfully tested buttons turn blue (checklist style)
    - Verify all buttons, triggers, and analog sticks work

6.  **Configure settings** using the interactive controls:
    - Enable/disable translation layer
    - Toggle HidHide device hiding
    - Configure SOCD cleaning method
    - Enable debouncing if needed
    - Test rumble/vibration

7.  Connect your physical controller. The proxy will automatically detect it and create a corresponding virtual device.

8.  Launch your game. The game should detect the virtual controller.

9.  Press `Ctrl+C` or use the "Exit Application" button to stop the service.

> **Tip:** All settings are saved to `config.ini` and persist between sessions. You can also edit the config file directly.

> **Note:** If ViGEmBus is not installed, the application will run in **Input Test Mode**. You can still test controller inputs but virtual devices won't be created.

> **Game Compatibility:** For games to pick up the *virtual* controller instead of the physical one, enable HidHide in the dashboard. The proxy handles device hiding automatically.

## Performance

| Metric | Target | Actual |
| :--- | :--- | :--- |
| **Polling Rate** | > 1000 Hz | **~1000 Hz** (1ms stable) |
| **Input Latency** | < 1 ms | **~0.4 ms** (Process only) |
| **CPU Usage** | < 1% | **< 0.5%** (Ryzen 5 5600X) |
| **Memory Footprint** | Low | **~12 MB** |

## Contributing

Contributions are welcome! Please follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) where possible.

1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feat/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'feat: Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feat/AmazingFeature`)
5.  Open a Pull Request

### Development Setup

```bash
# Clone the repository
git clone https://github.com/VishaL6i9/xinput-dinput-proxy.git
cd xinput-dinput-proxy

# Build the project
.\build.ps1

# Run tests
cd build
ctest --output-on-failure
```

## Documentation

- [Configuration Reference](config.ini) - All available settings and their defaults
- Settings are automatically saved and loaded from `config.ini` in the executable directory
- Logs are saved with timestamps (e.g., `2026-02-07-173551.log`) for crash debugging

## Known Issues

1. **ViGEmBus Dependency**: The original ViGEmBus driver was retired in 2023. This project requires the community-maintained fork. Future versions may migrate to alternative solutions.

2. **Administrator Privileges**: Required for ViGEmBus and HidHide driver access. The application will not function properly without elevated permissions.

3. **Controller Limit**: Maximum 4 XInput controllers supported (Windows limitation). Additional HID devices can be detected but won't be assigned to XInput slots.

4. **Bluetooth Latency**: Bluetooth controllers have higher latency than USB. For competitive gaming, USB connection is recommended.

5. **Multiple HID Interfaces**: Xbox controllers expose multiple HID interfaces (IG_01, IG_02, etc.). The proxy now correctly handles this and only creates one virtual device per physical controller.

## License

Distributed under the MIT License. See `LICENSE` for more information.

---

<div align="center">
    <b>Built as a personal side project.</b>
</div>