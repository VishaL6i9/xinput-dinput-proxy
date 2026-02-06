#include "ui/dashboard.hpp"
#include "utils/timing.hpp"

#include <iomanip>
#include <sstream>

Dashboard::Dashboard() 
    : m_running(false), 
      m_frameCount(0), 
      m_deltaTime(0.0), 
      m_statusMessage("Initializing..."),
      m_lastUpdateTime(0),
      m_screen(ftxui::ScreenInteractive::Fullscreen()) {
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
    
    // Create a renderer component for the dashboard
    auto renderer = ftxui::Renderer([&]() {
        return renderMainScreen();
    });
    
    // Main UI loop
    m_screen.Loop(renderer);
}

void Dashboard::stop() {
    m_running = false;
    
    m_screen.ExitLoopClosure()();
    
    if (m_uiThread && m_uiThread->joinable()) {
        m_uiThread->join();
    }
}

void Dashboard::updateStats(uint64_t frameCount, double deltaTime, const std::vector<ControllerState>& states) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_frameCount = frameCount;
    m_deltaTime = deltaTime;
    m_controllerStates = states;
    m_lastUpdateTime = TimingUtils::getPerformanceCounter();
}

void Dashboard::setStatusMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_statusMessage = message;
}

void Dashboard::initializeUI() {
    // Create the main container
    m_mainContainer = ftxui::Container::Vertical({
        // This will be populated in updateUI
    });
}

void Dashboard::updateUI() {
    // The UI is updated through the renderer in the run() method
    // This method is kept for compatibility but doesn't need to do anything
    // since the renderer handles the drawing
}

ftxui::Element Dashboard::renderMainScreen() {
    return ftxui:: vbox({
        ftxui::text("XInput-DirectInput Proxy Dashboard") | ftxui::bold | ftxui::center,
        ftxui::separator(),
        ftxui::hbox({
            renderControllersPanel() | ftxui::flex,
            renderMappingsPanel() | ftxui::flex,
        }),
        ftxui::separator(),
        ftxui::hbox({
            renderPerformancePanel() | ftxui::flex,
            renderStatusPanel() | ftxui::flex,
        }),
        ftxui::separator(),
        ftxui::text("Press 'q' to quit") | ftxui::center
    });
}

ftxui::Element Dashboard::renderControllersPanel() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    
    std::stringstream ss;
    ss << "Connected Controllers: " << m_controllerStates.size();
    
    ftxui::Elements children;
    children.push_back(ftxui::text(ss.str()));
    children.push_back(ftxui::separator());
    
    for (const auto& state : m_controllerStates) {
        std::string type = (state.userId >= 0) ? "XInput (" + std::to_string(state.userId) + ")" : "HID Input";
        std::string status = state.isConnected ? "Connected" : "Disconnected";
        
        std::stringstream info;
        info << "- " << type << ": " << status;
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
    // Placeholder for mapping information
    auto mappingInfo = ftxui::vbox({
        ftxui::text("Physical -> Virtual Mapping:"),
        ftxui::text("Kreo Mirage -> Virtual Xbox 360"),
        ftxui::text("Xbox Wireless -> Virtual DS4"),
        ftxui::text("Active Profiles: 1")
    }) | ftxui::border;
    
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
    
    auto statusInfo = ftxui::vbox({
        ftxui::text("Status: " + m_statusMessage),
        ftxui::text("Service: Running"),
        ftxui::text("Mode: XInput <-> DInput"),
        ftxui::text("SOCD: Last Win"),
        ftxui::text("Debouncing: Enabled")
    }) | ftxui::border;
    
    return ftxui::vbox({
        ftxui::text("Status") | ftxui::bold,
        statusInfo
    });
}