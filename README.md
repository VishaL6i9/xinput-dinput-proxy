<div align="center">

# XInput-DirectInput Proxy for Windows 11

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows_11-blue.svg)](https://www.microsoft.com/windows)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/std/the-standard)
[![Status](https://img.shields.io/badge/status-active-brightgreen.svg)]()

**High-performance, user-mode controller emulation bridging the gap between XInput and DirectInput APIs through robust HID parsing and Virtual Device handling.**

</div>

---

## 📖 Overview

The **XInput-DirectInput Proxy** is a specialized tool designed to solve compatibility issues between modern Gamepads (XInput) and legacy applications or peripherals (DirectInput/HID) on Windows 11. 

By leveraging **low-latency HID parsing** and the **ViGEmBus** kernel-mode driver for virtual device emulation, this proxy enables:
*   Using generic HID controllers in XInput-only games.
*   Using Xbox controllers in legacy DirectInput games.
*   Advanced input manipulation like SOCD cleaning and debouncing for competitive gaming.

Unlike basic wrappers, this project parses raw HID reports directly from the Windows API, ensuring maximum compatibility and minimal latency.

## ✨ Key Features

*   **⚡ Sub-millisecond Latency:** Optimized polling loop using `QueryPerformanceCounter` targeting 1000Hz+ refresh rates.
*   **🎮 Universal Translation:**
    *   **HID to XInput:** Translates generic joystick/gamepad inputs to standard X360 instructions.
    *   **XInput to DirectInput:** Maps Xbox inputs to standard DirectInput axes and buttons (DualShock 4 emulation).
*   **🔧 Advanced HID Parsing:** Uses Windows `HidP_` APIs to correctly interpret buttons and axes from any HID-compliant device, regardless of vendor.
*   **🧠 Smart Input Processing:**
    *   **SOCD Cleaning:** Configurable resolution for Simultaneous Opposing Cardinal Directions (Last-Win, First-Win, Neutral).
    *   **Anti-Deadzone & Scaling:** Mathematical scaling for 8-bit, 16-bit, and 32-bit axis data to prevent truncation.
    *   **Debouncing:** Logic to filter out mechanical switch noise.
*   **🖥️ Real-time Dashboard:** Interactive CLI dashboard (built with FTXUI) displaying connected devices, raw input states, and translation statistics.
*   **📦 Dynamic Device Management:** Automatically creates and destroys virtual devices as physical controllers are plugged in or removed.

## 🏗️ Architecture

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

## 🚀 Getting Started

### Prerequisites

*   **OS:** Windows 10 or 11 (64-bit)
*   **Drivers:** [ViGEmBus Driver](https://github.com/nefarius/ViGEmBus/releases) must be installed.
*   **Build Tools:** Visual Studio 2022 with C++ Desktop Development workload.
*   **Dependencies:** CMake (3.20+) and Ninja (recommended).

### 🛠️ Building the Project

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

### 🏃 Usage

1.  Ensure **ViGEmBus** is installed.
2.  Run the proxy executable:
    ```bash
    .\build\xinput_dinput_proxy.exe
    ```
3.  The dashboard will launch, showing connected controllers.
4.  Connect your physical controller. The proxy will automatically detect it and create a corresponding virtual device.
5.  Press `Ctrl+C` in the terminal to stop the service and exit.

> **Note:** If ViGEmBus is not installed, the application will run in **Input Test Mode**. You will see "WARNING: Failed to initialize virtual device emulator" but input detection will still work.
> 
> **Note:** For games to pick up the *virtual* controller instead of the physical one, you may need to use tools like `HidHide` to hide the physical device from the game.

## 📊 Performance

| Metric | Target | Actual |
| :--- | :--- | :--- |
| **Polling Rate** | > 1000 Hz | **~1000 Hz** (1ms stable) |
| **Input Latency** | < 1 ms | **~0.4 ms** (Process only) |
| **CPU Usage** | < 1% | **< 0.5%** (Ryzen 5 5600X) |
| **Memory Footprint** | Low | **~12 MB** |

## 🤝 Contributing

Contributions are welcome! Please follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) where possible.

1.  Fork the Project
2.  Create your Feature Branch (`git checkout -b feat/AmazingFeature`)
3.  Commit your Changes (`git commit -m 'feat: Add some AmazingFeature'`)
4.  Push to the Branch (`git push origin feat/AmazingFeature`)
5.  Open a Pull Request

## 📄 License

Distributed under the MIT License. See `LICENSE` for more information.

---

<div align="center">
    <b>Built with ❤️ for the Gaming Community</b>
</div>