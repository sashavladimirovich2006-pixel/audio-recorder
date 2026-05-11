#pragma once

#include <portaudio.h>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <functional>

class AudioRecorder {
public:
    struct AudioDevice {
        int index;
        std::string name;
        int maxInputChannels;
        double defaultSampleRate;
    };

    AudioRecorder();
    ~AudioRecorder();

    // Device management
    std::vector<AudioDevice> getInputDevices();
    bool setInputDevice(int deviceIndex);
    int getCurrentDeviceIndex() const { return currentDeviceIndex_; }

    // Recording control
    bool startRecording(const std::string& filename);
    void stopRecording();
    bool isRecording() const { return isRecording_; }

    // Audio level monitoring
    float getCurrentLevel() const { return currentLevel_; }
    void setLevelCallback(std::function<void(float)> callback) { levelCallback_ = callback; }

    // Settings
    void setSampleRate(double sampleRate) { sampleRate_ = sampleRate; }
    double getSampleRate() const { return sampleRate_; }
    
    void setChannels(int channels) { channels_ = channels; }
    int getChannels() const { return channels_; }

    // Error handling
    std::string getLastError() const { return lastError_; }

private:
    static int recordCallback(const void* inputBuffer, void* outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void* userData);

    void processAudioData(const float* inputBuffer, unsigned long framesPerBuffer);
    bool writeWavHeader(FILE* file);
    bool updateWavHeader(FILE* file);

    PaStream* stream_;
    FILE* outputFile_;
    
    std::atomic<bool> isRecording_;
    std::atomic<float> currentLevel_;
    
    int currentDeviceIndex_;
    double sampleRate_;
    int channels_;
    
    std::vector<float> recordedData_;
    std::string currentFilename_;
    std::string lastError_;
    
    std::function<void(float)> levelCallback_;
    
    size_t totalFramesRecorded_;
};
