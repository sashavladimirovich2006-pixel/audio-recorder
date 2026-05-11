#pragma once

#include "audio_recorder.h"
#include <memory>
#include <string>

struct GLFWwindow;

class GUI {
public:
    GUI();
    ~GUI();

    bool initialize();
    void run();
    void shutdown();

private:
    void renderUI();
    void renderDeviceSelector();
    void renderRecordingControls();
    void renderAudioLevelMeter();
    void renderSettings();
    
    void applyCustomStyle();
    
    GLFWwindow* window_;
    std::unique_ptr<AudioRecorder> recorder_;
    
    // UI state
    std::vector<AudioRecorder::AudioDevice> devices_;
    int selectedDeviceIndex_;
    bool showSettings_;
    
    // Recording state
    std::string outputFilename_;
    char filenameBuffer_[256];
    float recordingTime_;
    
    // Audio level visualization
    float levelHistory_[100];
    int levelHistoryIndex_;
    
    // Settings
    int sampleRateIndex_;
    int channelsIndex_;
    
    const char* sampleRates_[4] = {"22050 Hz", "44100 Hz", "48000 Hz", "96000 Hz"};
    const int sampleRateValues_[4] = {22050, 44100, 48000, 96000};
    const char* channelOptions_[2] = {"Mono", "Stereo"};
};
