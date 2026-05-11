#include "gui.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <cstring>
#include <chrono>
#include <sstream>
#include <iomanip>

GUI::GUI()
    : window_(nullptr)
    , recorder_(nullptr)
    , selectedDeviceIndex_(-1)
    , showSettings_(false)
    , recordingTime_(0.0f)
    , levelHistoryIndex_(0)
    , sampleRateIndex_(1)
    , channelsIndex_(0)
{
    std::memset(filenameBuffer_, 0, sizeof(filenameBuffer_));
    std::strcpy(filenameBuffer_, "recording.wav");
    std::memset(levelHistory_, 0, sizeof(levelHistory_));
}

GUI::~GUI() {
    shutdown();
}

bool GUI::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        return false;
    }

    // GL 3.3 + GLSL 330
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window
    window_ = glfwCreateWindow(800, 600, "Audio Recorder", nullptr, nullptr);
    if (!window_) {
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Apply custom style
    applyCustomStyle();

    // Initialize audio recorder
    recorder_ = std::make_unique<AudioRecorder>();
    devices_ = recorder_->getInputDevices();

    if (!devices_.empty()) {
        selectedDeviceIndex_ = 0;
        recorder_->setInputDevice(devices_[0].index);
    }

    // Set level callback
    recorder_->setLevelCallback([this](float level) {
        levelHistory_[levelHistoryIndex_] = level;
        levelHistoryIndex_ = (levelHistoryIndex_ + 1) % 100;
    });

    return true;
}

void GUI::run() {
    auto lastTime = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        // Update recording time
        if (recorder_->isRecording()) {
            auto currentTime = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            recordingTime_ += deltaTime;
            lastTime = currentTime;
        } else {
            recordingTime_ = 0.0f;
            lastTime = std::chrono::steady_clock::now();
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderUI();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.15f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
}

void GUI::shutdown() {
    if (recorder_) {
        recorder_->stopRecording();
        recorder_.reset();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }

    glfwTerminate();
}

void GUI::renderUI() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("MainWindow", nullptr, window_flags);

    // Header
    ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
    ImGui::Indent(20);
    ImGui::Text("🎙️ Audio Recorder");
    ImGui::Unindent(20);
    ImGui::PopFont();

    ImGui::Separator();
    ImGui::Spacing();

    // Main content area
    ImGui::BeginChild("Content", ImVec2(0, -50), false);
    
    renderDeviceSelector();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    renderRecordingControls();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    renderAudioLevelMeter();
    
    ImGui::EndChild();

    // Footer with settings button
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Indent(20);
    if (ImGui::Button("⚙️ Settings", ImVec2(120, 30))) {
        showSettings_ = !showSettings_;
    }
    ImGui::Unindent(20);

    // Settings window
    if (showSettings_) {
        renderSettings();
    }

    ImGui::End();
}

void GUI::renderDeviceSelector() {
    ImGui::Indent(20);
    ImGui::Text("Input Device");
    ImGui::SetNextItemWidth(400);

    if (ImGui::BeginCombo("##device", selectedDeviceIndex_ >= 0 && selectedDeviceIndex_ < devices_.size() 
                                      ? devices_[selectedDeviceIndex_].name.c_str() 
                                      : "No device selected")) {
        for (size_t i = 0; i < devices_.size(); i++) {
            bool isSelected = (selectedDeviceIndex_ == static_cast<int>(i));
            if (ImGui::Selectable(devices_[i].name.c_str(), isSelected)) {
                selectedDeviceIndex_ = static_cast<int>(i);
                if (!recorder_->isRecording()) {
                    recorder_->setInputDevice(devices_[i].index);
                }
            }
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::Unindent(20);
}

void GUI::renderRecordingControls() {
    ImGui::Indent(20);
    
    // Filename input
    ImGui::Text("Output File");
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("##filename", filenameBuffer_, sizeof(filenameBuffer_));

    ImGui::Spacing();
    ImGui::Spacing();

    // Recording controls
    bool isRecording = recorder_->isRecording();
    
    if (isRecording) {
        // Recording time display
        int minutes = static_cast<int>(recordingTime_) / 60;
        int seconds = static_cast<int>(recordingTime_) % 60;
        std::stringstream ss;
        ss << "⏺️ Recording: " << std::setfill('0') << std::setw(2) << minutes 
           << ":" << std::setw(2) << seconds;
        
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("%s", ss.str().c_str());
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        
        if (ImGui::Button("⏹️ Stop Recording", ImVec2(200, 40))) {
            recorder_->stopRecording();
        }
    } else {
        if (ImGui::Button("⏺️ Start Recording", ImVec2(200, 40))) {
            outputFilename_ = filenameBuffer_;
            if (recorder_->startRecording(outputFilename_)) {
                recordingTime_ = 0.0f;
            }
        }
    }

    // Error display
    std::string error = recorder_->getLastError();
    if (!error.empty()) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("Error: %s", error.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::Unindent(20);
}

void GUI::renderAudioLevelMeter() {
    ImGui::Indent(20);
    ImGui::Text("Audio Level");
    
    float currentLevel = recorder_->getCurrentLevel();
    
    // Level bar
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.3f, 1.0f));
    ImGui::ProgressBar(currentLevel, ImVec2(400, 30), "");
    ImGui::PopStyleColor();

    // Level history graph
    ImGui::Spacing();
    ImGui::Text("Level History");
    ImGui::PlotLines("##levelhistory", levelHistory_, 100, 0, nullptr, 0.0f, 1.0f, ImVec2(400, 80));
    
    ImGui::Unindent(20);
}

void GUI::renderSettings() {
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
    ImGui::Begin("Settings", &showSettings_);

    ImGui::Text("Audio Settings");
    ImGui::Separator();
    ImGui::Spacing();

    // Sample rate
    ImGui::Text("Sample Rate");
    if (ImGui::Combo("##samplerate", &sampleRateIndex_, sampleRates_, 4)) {
        if (!recorder_->isRecording()) {
            recorder_->setSampleRate(sampleRateValues_[sampleRateIndex_]);
        }
    }

    ImGui::Spacing();

    // Channels
    ImGui::Text("Channels");
    if (ImGui::Combo("##channels", &channelsIndex_, channelOptions_, 2)) {
        if (!recorder_->isRecording()) {
            recorder_->setChannels(channelsIndex_ + 1);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Note: Settings can only be changed when not recording.");

    ImGui::End();
}

void GUI::applyCustomStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Colors
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.50f, 0.80f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.55f, 0.85f, 0.80f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.60f, 0.90f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.50f, 0.80f, 0.80f);
    colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.25f, 0.75f, 0.35f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.25f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 0.55f, 0.85f, 0.35f);

    // Rounding
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    
    // Spacing
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(8, 8);
}
