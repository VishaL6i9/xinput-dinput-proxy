#include "ui/dashboard.hpp"
#include "utils/timing.hpp"

#include <iomanip>
#include <sstream>

Dashboard::Dashboard() 
    : m_running(false), 
      m_frameCount(0), 
      m_deltaTime(0), 
      m_statusMessage("Initializing..."),
      m_vigemAvailable(false),
      m_emulator(nullptr),
      m_translationLayer(nullptr),
      m_selectedSocd(2), // Neutral
      m_selectedTargetType(0), // XInput
      m_socdEnabled(false),
      m_debouncingEnabled(false),
      m_hidHideEnabled(false),
      m_translationEnabled(false),
      m_rumbleIntensity(0.0f),
      m_rumbleTesting(false),
      m_lastRumbleTesting(false),
      m_refreshRequested(false),
      m_stickDeadzoneEnabled(true),
      m_leftStickDeadzone(0.15f),
      m_rightStickDeadzone(0.15f),
      m_leftStickAntiDeadzone(0.0f),
      m_rightStickAntiDeadzone(0.0f),
      m_screen(ftxui::ScreenInteractive::Fullscreen()) {
    TimingUtils::initialize();
    m_lastUpdateTime = TimingUtils::getPerformanceCounter();
}

Dashboard::~Dashboard() {
    stop();
}

void Dashboard::run() {
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    // Initialize UI components
    initializeUI();
    
    // Create a renderer that combines static content and interactive components
    auto renderer = ftxui::Renderer(m_mainContainer, [&]() {
        try {
            updateUI(); // Dynamic sync
            return renderMainScreen();
        } catch (const std::exception& e) {
            Logger::error("Dashboard rendering exception: " + std::string(e.what()));
            return ftxui::text("ERROR: " + std::string(e.what())) | ftxui::color(ftxui::Color::Red);
        } catch (...) {
            Logger::error("Dashboard rendering unknown exception");
            return ftxui::text("ERROR: Unknown exception in rendering") | ftxui::color(ftxui::Color::Red);
        }
    });
    
    // Main UI loop
    m_screen.Loop(renderer);
}

void Dashboard::stop() {
    m_running = false;
    m_rumbleTesting = false;
    if (m_emulator) {
        m_emulator->setRumbleEnabled(false);
    }
    m_screen.ExitLoopClosure()();
}

void Dashboard::setStatusMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_statusMessage = message;
    m_screen.Post(ftxui::Event::Custom);
}

void Dashboard::updateStats(uint64_t frameCount, double deltaTime, const std::vector<ControllerState>& states) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_frameCount = frameCount;
    m_deltaTime = deltaTime;
    m_controllerStates = states;
    m_lastUpdateTime = TimingUtils::getPerformanceCounter();
    m_screen.Post(ftxui::Event::Custom);
}

void Dashboard::setViGEmAvailable(bool available) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_vigemAvailable = available;
    m_screen.Post(ftxui::Event::Custom);
}

void Dashboard::loadSettings(bool translationEnabled, bool hidHideEnabled, bool socdEnabled, int socdMethod, bool debouncingEnabled, int targetType) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_translationEnabled = translationEnabled;
    m_hidHideEnabled = hidHideEnabled;
    m_socdEnabled = socdEnabled;
    m_selectedSocd = socdMethod;
    m_debouncingEnabled = debouncingEnabled;
    m_selectedTargetType = targetType;
    
    // Load stick drift settings from translation layer if available
    if (m_translationLayer) {
        m_stickDeadzoneEnabled = true; // Will be synced from translation layer
        m_leftStickDeadzone = m_translationLayer->getLeftStickDeadzone();
        m_rightStickDeadzone = m_translationLayer->getRightStickDeadzone();
    }
}

void Dashboard::initializeUI() {
    using namespace ftxui;

    // Initialize labels
    m_socdLabels = {"Last Win", "First Win", "Neutral"};
    m_targetLabels = {"Xbox 360", "DualShock 4", "Combined"};

    // 1. SOCD Control
    auto socd_options = RadioboxOption::Simple();
    auto socd_radio = Radiobox(&m_socdLabels, &m_selectedSocd, socd_options);
    auto socd_toggle = Checkbox("Enable SOCD Cleaning", &m_socdEnabled);
    
    // 2. Target Emulation
    auto target_options = RadioboxOption::Simple();
    auto target_radio = Radiobox(&m_targetLabels, &m_selectedTargetType, target_options);
    
    // 3. System Options
    auto debounce_toggle = Checkbox("Enable Debouncing", &m_debouncingEnabled);
    auto hidhide_toggle = Checkbox("Enable HidHide", &m_hidHideEnabled);
    auto translation_toggle = Checkbox("Enable Translation Layer", &m_translationEnabled);
    
    // 3.5. Stick Drift Mitigation
    auto stick_deadzone_toggle = Checkbox("Enable Stick Drift Mitigation", &m_stickDeadzoneEnabled);
    auto left_stick_slider = Slider("Left Stick DZ", &m_leftStickDeadzone, 0.0f, 0.5f, 0.01f);
    auto right_stick_slider = Slider("Right Stick DZ", &m_rightStickDeadzone, 0.0f, 0.5f, 0.01f);
    auto left_anti_slider = Slider("Left Anti-DZ", &m_leftStickAntiDeadzone, 0.0f, 0.3f, 0.01f);
    auto right_anti_slider = Slider("Right Anti-DZ", &m_rightStickAntiDeadzone, 0.0f, 0.3f, 0.01f);

    // 4. Rumble Test with preset buttons
    m_rumbleBtnLabel = "START Rumble";
    auto rumble_slider = Slider("", &m_rumbleIntensity, 0.0f, 1.0f, 0.01f);
    
    auto rumble_btn = Button(&m_rumbleBtnLabel, [&] {
        m_rumbleTesting = !m_rumbleTesting;
        if (m_emulator) {
            m_emulator->setRumbleEnabled(m_rumbleTesting);
            if (m_rumbleTesting) {
                m_emulator->setRumbleIntensity(m_rumbleIntensity);
            }
            
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_statusMessage = m_rumbleTesting ? "Vibration testing ACTIVE" : "Vibration stopped";
        }
    });

    // Preset intensity buttons
    auto preset_25_btn = Button("25%", [&] {
        m_rumbleIntensity = 0.25f;
        if (m_rumbleTesting && m_emulator) {
            m_emulator->setRumbleIntensity(0.25f);
        }
    });
    
    auto preset_50_btn = Button("50%", [&] {
        m_rumbleIntensity = 0.5f;
        if (m_rumbleTesting && m_emulator) {
            m_emulator->setRumbleIntensity(0.5f);
        }
    });
    
    auto preset_75_btn = Button("75%", [&] {
        m_rumbleIntensity = 0.75f;
        if (m_rumbleTesting && m_emulator) {
            m_emulator->setRumbleIntensity(0.75f);
        }
    });
    
    auto preset_100_btn = Button("100%", [&] {
        m_rumbleIntensity = 1.0f;
        if (m_rumbleTesting && m_emulator) {
            m_emulator->setRumbleIntensity(1.0f);
        }
    });

    // 5. Device Management
    auto refresh_devices_btn = Button("Refresh Devices", [&] {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_refreshRequested = true;
        m_statusMessage = "Device refresh requested...";
    });

    // 6. Container Assembly
    m_mainContainer = Container::Vertical({
        socd_toggle,
        socd_radio,
        Renderer([&] { return separator(); }),
        target_radio,
        Renderer([&] { return separator(); }),
        debounce_toggle,
        hidhide_toggle,
        translation_toggle,
        Renderer([&] { return separator(); }),
        stick_deadzone_toggle,
        left_stick_slider,
        right_stick_slider,
        left_anti_slider,
        right_anti_slider,
        Renderer([&] { return separator(); }),
        rumble_slider,
        rumble_btn,
        preset_25_btn,
        preset_50_btn,
        preset_75_btn,
        preset_100_btn,
        Renderer([&] { return separator(); }),
        refresh_devices_btn,
        Button("Exit Application", [&] { stop(); })
    });
}

void Dashboard::updateUI() {
    // Sync UI labels
    m_rumbleBtnLabel = m_rumbleTesting ? "STOP Rumble" : "START Rumble";

    // Sync UI state to core modules
    if (m_translationLayer) {
        m_translationLayer->setSOCDCleaningEnabled(m_socdEnabled);
        m_translationLayer->setSOCDMethod(m_selectedSocd);
        m_translationLayer->setDebouncingEnabled(m_debouncingEnabled);
        
        // Sync stick drift mitigation settings
        m_translationLayer->setStickDeadzoneEnabled(m_stickDeadzoneEnabled);
        m_translationLayer->setLeftStickDeadzone(m_leftStickDeadzone);
        m_translationLayer->setRightStickDeadzone(m_rightStickDeadzone);
        m_translationLayer->setLeftStickAntiDeadzone(m_leftStickAntiDeadzone);
        m_translationLayer->setRightStickAntiDeadzone(m_rightStickAntiDeadzone);
        
        // Target type determines which translation direction is enabled:
        // 0 = Xbox 360: Enable DInput->XInput (convert generic HID to Xbox)
        // 1 = DualShock 4: Enable XInput->DInput (convert Xbox to DS4)
        // 2 = Combined: Enable both directions
        bool xiToDi = (m_selectedTargetType == 1 || m_selectedTargetType == 2);
        bool diToXi = (m_selectedTargetType == 0 || m_selectedTargetType == 2);
        m_translationLayer->setXInputToDInputMapping(xiToDi);
        m_translationLayer->setDInputToXInputMapping(diToXi);
    }
    
    if (m_emulator) {
        m_emulator->enableHidHideIntegration(m_hidHideEnabled);
        
        // Only update rumble if the state changed or intensity is being tweaked
        if (m_rumbleTesting != m_lastRumbleTesting) {
            m_emulator->setRumbleEnabled(m_rumbleTesting);
            m_lastRumbleTesting = m_rumbleTesting;
        }
        
        // Always sync intensity if testing is active (setRumbleIntensity handles its own optimization)
        if (m_rumbleTesting) {
            m_emulator->setRumbleIntensity(m_rumbleIntensity);
        }
    }
}

ftxui::Element Dashboard::renderMainScreen() {
    return ftxui::vbox({
        ftxui::text("XInput-DirectInput Proxy Dashboard - Interactive Test Mode") | ftxui::bold | ftxui::center | ftxui::color(ftxui::Color::Cyan),
        ftxui::separator(),
        ftxui::hbox({
            renderControllersPanel() | ftxui::flex,
            renderMappingsPanel() | ftxui::flex,
        }),
        ftxui::separator(),
        ftxui::hbox({
            renderInteractiveControls() | ftxui::flex,
            renderRumblePanel() | ftxui::flex,
        }),
        ftxui::separator(),
        ftxui::hbox({
            renderPerformancePanel() | ftxui::flex,
            renderStatusPanel() | ftxui::flex,
        }),
        ftxui::separator(),
        renderInputTestPanel(),
        ftxui::separator(),
        ftxui::text("Navigate with Arrows/Tab, Space/Enter to select") | ftxui::center | ftxui::dim
    });
}

ftxui::Element Dashboard::renderInteractiveControls() {
    return ftxui::vbox({
        ftxui::text("Interactive Configuration") | ftxui::bold,
        ftxui::vbox({
            ftxui::text("SOCD Mode:") | ftxui::color(ftxui::Color::Yellow),
            m_mainContainer->ChildAt(1)->Render(), // socd_radio (index 1)
            ftxui::separator(),
            ftxui::text("Target Emulation:") | ftxui::color(ftxui::Color::Yellow),
            m_mainContainer->ChildAt(3)->Render(), // target_radio (index 3)
            ftxui::separator(),
            m_mainContainer->ChildAt(0)->Render(), // socd_toggle (index 0)
            m_mainContainer->ChildAt(5)->Render(), // debounce_toggle (index 5)
            m_mainContainer->ChildAt(6)->Render(), // hidhide_toggle (index 6)
            ftxui::separator(),
            ftxui::text("Stick Drift Mitigation:") | ftxui::color(ftxui::Color::Yellow),
            m_mainContainer->ChildAt(9)->Render(),  // stick_deadzone_toggle (index 9)
            ftxui::hbox({
                ftxui::text("L: " + std::to_string(static_cast<int>(m_leftStickDeadzone * 100)) + "% "),
                m_mainContainer->ChildAt(10)->Render() | ftxui::flex, // left_stick_slider
            }),
            ftxui::hbox({
                ftxui::text("R: " + std::to_string(static_cast<int>(m_rightStickDeadzone * 100)) + "% "),
                m_mainContainer->ChildAt(11)->Render() | ftxui::flex, // right_stick_slider
            }),
            ftxui::hbox({
                ftxui::text("L Anti: " + std::to_string(static_cast<int>(m_leftStickAntiDeadzone * 100)) + "% "),
                m_mainContainer->ChildAt(12)->Render() | ftxui::flex, // left_anti_slider
            }),
            ftxui::hbox({
                ftxui::text("R Anti: " + std::to_string(static_cast<int>(m_rightStickAntiDeadzone * 100)) + "% "),
                m_mainContainer->ChildAt(13)->Render() | ftxui::flex, // right_anti_slider
            }),
        }) | ftxui::border
    });
}

ftxui::Element Dashboard::renderRumblePanel() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // Format intensity as percentage
    int intensityPercent = static_cast<int>(m_rumbleIntensity * 100);
    
    return ftxui::vbox({
        ftxui::text("Functionality Tests") | ftxui::bold,
        ftxui::vbox({
            ftxui::text("Vibration/Rumble Test:") | ftxui::color(ftxui::Color::Yellow),
            ftxui::hbox({
                ftxui::text("Intensity: " + std::to_string(intensityPercent) + "% "),
                m_mainContainer->ChildAt(15)->Render() | ftxui::flex, // rumble_slider (index 15)
            }),
            ftxui::hbox({
                m_mainContainer->ChildAt(16)->Render(),  // rumble_btn (START/STOP) (index 16)
                ftxui::text(" "),
                m_mainContainer->ChildAt(17)->Render(), // preset_25_btn (index 17)
                ftxui::text(" "),
                m_mainContainer->ChildAt(18)->Render(), // preset_50_btn (index 18)
                ftxui::text(" "),
                m_mainContainer->ChildAt(19)->Render(), // preset_75_btn (index 19)
                ftxui::text(" "),
                m_mainContainer->ChildAt(20)->Render(), // preset_100_btn (index 20)
            }),
            ftxui::separator(),
            ftxui::text("Device Management:") | ftxui::color(ftxui::Color::Yellow),
            m_mainContainer->ChildAt(22)->Render(), // refresh_devices_btn (index 22)
            ftxui::separator(),
            ftxui::text("Status: " + m_statusMessage) | ftxui::dim,
            ftxui::filler(),
            m_mainContainer->ChildAt(23)->Render(), // exit_btn (index 23)
        }) | ftxui::border
    });
}

ftxui::Element Dashboard::renderControllersPanel() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    // Count only connected controllers
    int connectedCount = 0;
    for (const auto& state : m_controllerStates) {
        if (state.isConnected) connectedCount++;
    }
    
    std::stringstream ss;
    ss << "Connected Controllers: " << connectedCount;
    
    ftxui::Elements children;
    children.push_back(ftxui::text(ss.str()));
    children.push_back(ftxui::separator());
    
    for (const auto& state : m_controllerStates) {
        std::string displayName;
        
        // Convert wide string product name to narrow string
        std::string productName;
        for (wchar_t wc : state.productName) {
            productName += static_cast<char>(wc);
        }
        
        if (state.userId >= 0) {
            // XInput device
            std::string name = productName.empty() ? "Xbox 360 Controller" : productName;
            displayName = name + " (User " + std::to_string(state.userId) + ")";
        } else {
            // Pure HID device
            displayName = productName.empty() ? "HID Input Device" : productName;
        }
        
        std::string status = state.isConnected ? "Connected" : "Disconnected";
        
        std::stringstream info;
        info << "- " << displayName << ": " << status;
        
        if (!state.isConnected && state.userId >= 0) {
            info << " (Err: " << state.lastError << ")";
        }
        
        children.push_back(ftxui::text(info.str()));
    }
    
    if (m_controllerStates.empty()) {
        children.push_back(ftxui::text("No controllers detected"));
    }
    
    auto controllerList = ftxui::vbox(std::move(children)) | ftxui::border;
    
    return ftxui::vbox({
        ftxui::text("Controllers") | ftxui::bold,
        controllerList
    });
}

ftxui::Element Dashboard::renderMappingsPanel() {
    std::vector<ftxui::Element> mappingElements;
    mappingElements.push_back(ftxui::text("Physical -> Virtual Translation:"));
    
    if (m_emulator) {
        auto virtualDevices = m_emulator->getVirtualDevices();
        if (virtualDevices.empty()) {
            mappingElements.push_back(ftxui::text("No active mappings") | ftxui::color(ftxui::Color::Yellow));
        } else {
            for (const auto& dev : virtualDevices) {
                std::string target = (dev.type == TranslatedState::TARGET_XINPUT) ? "Xbox 360" : "DS4";
                std::string line = dev.sourceName + " -> Virtual " + target;
                mappingElements.push_back(ftxui::text(line) | ftxui::color(ftxui::Color::Green));
            }
        }
    } else {
        mappingElements.push_back(ftxui::text("Emulator not connected") | ftxui::color(ftxui::Color::Red));
    }
    
    auto mappingInfo = ftxui::vbox(std::move(mappingElements)) | ftxui::border;
    
    return ftxui::vbox({
        ftxui::text("Mappings") | ftxui::bold,
        mappingInfo
    });
}

ftxui::Element Dashboard::renderPerformancePanel() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    double currentTime = TimingUtils::counterToMicroseconds(TimingUtils::getPerformanceCounter());
    double fps = (m_deltaTime > 0) ? (1000000.0 / m_deltaTime) : 0.0;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Frame Rate: " << fps << " FPS\n";
    ss << "Avg Frame Time: " << m_deltaTime << " μs\n";
    ss << "Total Frames: " << m_frameCount << "\n";
    ss << "Latency Estimate: <1ms";
    
    auto perfInfo = ftxui::vbox({
        ftxui::text(ss.str())
    }) | ftxui::border;
    
    return ftxui::vbox({
        ftxui::text("Performance") | ftxui::bold,
        perfInfo
    });
}

ftxui::Element Dashboard::renderStatusPanel() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    std::string socdStr = "Disabled";
    if (m_socdEnabled) {
        if (m_selectedSocd == 0) socdStr = "Last Win";
        else if (m_selectedSocd == 1) socdStr = "First Win";
        else socdStr = "Neutral";
    }

    std::string modeStr = "Xbox 360";
    if (m_selectedTargetType == 1) modeStr = "DualShock 4";
    else if (m_selectedTargetType == 2) modeStr = "Combined";
    
    std::string stickDriftStr = m_stickDeadzoneEnabled 
        ? "L:" + std::to_string(static_cast<int>(m_leftStickDeadzone * 100)) + "% R:" + std::to_string(static_cast<int>(m_rightStickDeadzone * 100)) + "%"
        : "Disabled";

    auto statusInfo = ftxui::vbox({
        ftxui::text("Service: Running") | ftxui::color(ftxui::Color::Green),
        ftxui::text(std::string("ViGEmBus: ") + (m_vigemAvailable ? "Connected" : "Not Found (Input Test Mode)")),
        ftxui::text("Emulation: " + modeStr),
        ftxui::text("SOCD: " + socdStr),
        ftxui::text(std::string("Debouncing: ") + (m_debouncingEnabled ? "Enabled" : "Disabled")),
        ftxui::text("Stick Drift Fix: " + stickDriftStr),
        ftxui::text(std::string("HidHide: ") + (m_hidHideEnabled ? "Active" : "Inactive"))
    }) | ftxui::border;
    
    return ftxui::vbox({
        ftxui::text("System Status") | ftxui::bold,
        statusInfo
    });
}

ftxui::Element Dashboard::renderInputTestPanel() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    if (m_controllerStates.empty()) {
        return ftxui::vbox({
            ftxui::text("Input Test (No controller detected)") | ftxui::bold,
            ftxui::text("Connect a controller to see raw input data") | ftxui::dim
        }) | ftxui::border;
    }

    // Find first connected controller with valid XInput data
    const ControllerState* activeState = nullptr;
    for (const auto& state : m_controllerStates) {
        // Only use XInput controllers (userId >= 0) that are connected
        if (state.userId >= 0 && state.lastError == ERROR_SUCCESS) {
            activeState = &state;
            break;
        }
    }
    
    // If no connected XInput controller found, show waiting message
    if (!activeState) {
        return ftxui::vbox({
            ftxui::text("Input Test (Waiting for XInput controller)") | ftxui::bold,
            ftxui::text("Connect an Xbox controller to see input data") | ftxui::dim
        }) | ftxui::border;
    }
    
    // Read from xinputState.Gamepad (the actual XInput data)
    const auto& gamepad = activeState->xinputState.Gamepad;
    
    // Track which buttons have been pressed
    if (gamepad.wButtons & XINPUT_GAMEPAD_A) m_pressedButtons.insert(XINPUT_GAMEPAD_A);
    if (gamepad.wButtons & XINPUT_GAMEPAD_B) m_pressedButtons.insert(XINPUT_GAMEPAD_B);
    if (gamepad.wButtons & XINPUT_GAMEPAD_X) m_pressedButtons.insert(XINPUT_GAMEPAD_X);
    if (gamepad.wButtons & XINPUT_GAMEPAD_Y) m_pressedButtons.insert(XINPUT_GAMEPAD_Y);
    if (gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) m_pressedButtons.insert(XINPUT_GAMEPAD_LEFT_SHOULDER);
    if (gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) m_pressedButtons.insert(XINPUT_GAMEPAD_RIGHT_SHOULDER);
    if (gamepad.wButtons & XINPUT_GAMEPAD_BACK) m_pressedButtons.insert(XINPUT_GAMEPAD_BACK);
    if (gamepad.wButtons & XINPUT_GAMEPAD_START) m_pressedButtons.insert(XINPUT_GAMEPAD_START);
    if (gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) m_pressedButtons.insert(XINPUT_GAMEPAD_LEFT_THUMB);
    if (gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) m_pressedButtons.insert(XINPUT_GAMEPAD_RIGHT_THUMB);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) m_pressedButtons.insert(XINPUT_GAMEPAD_DPAD_UP);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) m_pressedButtons.insert(XINPUT_GAMEPAD_DPAD_DOWN);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) m_pressedButtons.insert(XINPUT_GAMEPAD_DPAD_LEFT);
    if (gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) m_pressedButtons.insert(XINPUT_GAMEPAD_DPAD_RIGHT);
    
    auto renderButton = [&](const std::string& name, WORD bit) {
        bool pressed = (gamepad.wButtons & bit) != 0;
        bool everPressed = m_pressedButtons.count(bit) > 0;
        
        if (pressed) {
            // Currently pressed: bold green
            return ftxui::text(name) | ftxui::bold | ftxui::color(ftxui::Color::Green);
        } else if (everPressed) {
            // Was pressed before: blue (checklist style)
            return ftxui::text(name) | ftxui::color(ftxui::Color::Blue);
        } else {
            // Never pressed: dim
            return ftxui::text(name) | ftxui::dim;
        }
    };

    auto renderTrigger = [&](const std::string& name, BYTE value) {
        float percent = value / 255.0f;
        return ftxui::hbox({
            ftxui::text(name + ": "),
            ftxui::gauge(percent) | ftxui::flex,
            ftxui::text(" " + std::to_string(value))
        });
    };

    auto renderStick = [&](const std::string& name, SHORT x, SHORT y) {
        float fx = x / 32768.0f;
        float fy = y / 32768.0f;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << "(" << fx << ", " << fy << ")";
        return ftxui::text(name + ": " + ss.str());
    };

    return ftxui::vbox({
        ftxui::text(std::string("Raw XInput Test - ") + (m_translationEnabled ? "Active" : "BYPASSED")) | ftxui::bold | ftxui::color(m_translationEnabled ? ftxui::Color::White : ftxui::Color::Yellow),
        ftxui::hbox({
            // Columns for buttons
            ftxui::vbox({
                renderButton("A", XINPUT_GAMEPAD_A),
                renderButton("B", XINPUT_GAMEPAD_B),
                renderButton("X", XINPUT_GAMEPAD_X),
                renderButton("Y", XINPUT_GAMEPAD_Y),
            }) | ftxui::flex,
            ftxui::vbox({
                renderButton("L_SHOULDER", XINPUT_GAMEPAD_LEFT_SHOULDER),
                renderButton("R_SHOULDER", XINPUT_GAMEPAD_RIGHT_SHOULDER),
                renderButton("L_THUMB", XINPUT_GAMEPAD_LEFT_THUMB),
                renderButton("R_THUMB", XINPUT_GAMEPAD_RIGHT_THUMB),
            }) | ftxui::flex,
            ftxui::vbox({
                renderButton("BACK", XINPUT_GAMEPAD_BACK),
                renderButton("START", XINPUT_GAMEPAD_START),
                renderButton("UP", XINPUT_GAMEPAD_DPAD_UP),
                renderButton("DOWN", XINPUT_GAMEPAD_DPAD_DOWN),
            }) | ftxui::flex,
            ftxui::vbox({
                renderButton("LEFT", XINPUT_GAMEPAD_DPAD_LEFT),
                renderButton("RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT),
                ftxui::text("") | ftxui::dim,
                ftxui::text("") | ftxui::dim,
            }) | ftxui::flex,
        }),
        ftxui::separator(),
        ftxui::vbox({
            renderTrigger("LT", gamepad.bLeftTrigger),
            renderTrigger("RT", gamepad.bRightTrigger),
            renderStick("Left Stick", gamepad.sThumbLX, gamepad.sThumbLY),
            renderStick("Right Stick", gamepad.sThumbRX, gamepad.sThumbRY),
        })
    }) | ftxui::border;
}