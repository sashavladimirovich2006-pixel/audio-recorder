#include "audio_recorder.h"
#include <cstring>
#include <cmath>
#include <algorithm>

AudioRecorder::AudioRecorder()
    : stream_(nullptr)
    , outputFile_(nullptr)
    , isRecording_(false)
    , currentLevel_(0.0f)
    , currentDeviceIndex_(-1)
    , sampleRate_(44100.0)
    , channels_(1)
    , totalFramesRecorded_(0)
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        lastError_ = Pa_GetErrorText(err);
    }
}

AudioRecorder::~AudioRecorder() {
    stopRecording();
    Pa_Terminate();
}

std::vector<AudioRecorder::AudioDevice> AudioRecorder::getInputDevices() {
    std::vector<AudioDevice> devices;
    
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        lastError_ = "Error getting device count";
        return devices;
    }

    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo->maxInputChannels > 0) {
            AudioDevice device;
            device.index = i;
            device.name = deviceInfo->name;
            device.maxInputChannels = deviceInfo->maxInputChannels;
            device.defaultSampleRate = deviceInfo->defaultSampleRate;
            devices.push_back(device);
        }
    }

    return devices;
}

bool AudioRecorder::setInputDevice(int deviceIndex) {
    if (isRecording_) {
        lastError_ = "Cannot change device while recording";
        return false;
    }

    const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
    if (!deviceInfo) {
        lastError_ = "Invalid device index";
        return false;
    }

    if (deviceInfo->maxInputChannels <= 0) {
        lastError_ = "Device has no input channels";
        return false;
    }

    currentDeviceIndex_ = deviceIndex;
    return true;
}

bool AudioRecorder::startRecording(const std::string& filename) {
    if (isRecording_) {
        lastError_ = "Already recording";
        return false;
    }

    if (currentDeviceIndex_ < 0) {
        // Use default input device
        currentDeviceIndex_ = Pa_GetDefaultInputDevice();
        if (currentDeviceIndex_ == paNoDevice) {
            lastError_ = "No default input device available";
            return false;
        }
    }

    // Open output file
    outputFile_ = fopen(filename.c_str(), "wb");
    if (!outputFile_) {
        lastError_ = "Failed to open output file";
        return false;
    }

    currentFilename_ = filename;
    totalFramesRecorded_ = 0;
    recordedData_.clear();

    // Write placeholder WAV header
    writeWavHeader(outputFile_);

    // Setup stream parameters
    PaStreamParameters inputParameters;
    inputParameters.device = currentDeviceIndex_;
    inputParameters.channelCount = channels_;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo(currentDeviceIndex_)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    // Open stream
    PaError err = Pa_OpenStream(
        &stream_,
        &inputParameters,
        nullptr,
        sampleRate_,
        256,
        paClipOff,
        recordCallback,
        this
    );

    if (err != paNoError) {
        lastError_ = Pa_GetErrorText(err);
        fclose(outputFile_);
        outputFile_ = nullptr;
        return false;
    }

    // Start stream
    err = Pa_StartStream(stream_);
    if (err != paNoError) {
        lastError_ = Pa_GetErrorText(err);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        fclose(outputFile_);
        outputFile_ = nullptr;
        return false;
    }

    isRecording_ = true;
    return true;
}

void AudioRecorder::stopRecording() {
    if (!isRecording_) {
        return;
    }

    isRecording_ = false;

    if (stream_) {
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }

    if (outputFile_) {
        updateWavHeader(outputFile_);
        fclose(outputFile_);
        outputFile_ = nullptr;
    }

    currentLevel_ = 0.0f;
}

int AudioRecorder::recordCallback(const void* inputBuffer, void* outputBuffer,
                                  unsigned long framesPerBuffer,
                                  const PaStreamCallbackTimeInfo* timeInfo,
                                  PaStreamCallbackFlags statusFlags,
                                  void* userData) {
    AudioRecorder* recorder = static_cast<AudioRecorder*>(userData);
    const float* in = static_cast<const float*>(inputBuffer);

    if (in && recorder->isRecording_) {
        recorder->processAudioData(in, framesPerBuffer);
    }

    return paContinue;
}

void AudioRecorder::processAudioData(const float* inputBuffer, unsigned long framesPerBuffer) {
    // Calculate audio level (RMS)
    float sum = 0.0f;
    size_t totalSamples = framesPerBuffer * channels_;
    
    for (size_t i = 0; i < totalSamples; i++) {
        sum += inputBuffer[i] * inputBuffer[i];
    }
    
    float rms = std::sqrt(sum / totalSamples);
    currentLevel_ = std::min(1.0f, rms * 10.0f); // Scale for better visualization

    if (levelCallback_) {
        levelCallback_(currentLevel_);
    }

    // Write to file
    if (outputFile_) {
        // Convert float to 16-bit PCM
        std::vector<int16_t> pcmData(totalSamples);
        for (size_t i = 0; i < totalSamples; i++) {
            float sample = std::max(-1.0f, std::min(1.0f, inputBuffer[i]));
            pcmData[i] = static_cast<int16_t>(sample * 32767.0f);
        }

        fwrite(pcmData.data(), sizeof(int16_t), totalSamples, outputFile_);
        totalFramesRecorded_ += framesPerBuffer;
    }
}

bool AudioRecorder::writeWavHeader(FILE* file) {
    // WAV header structure
    struct WavHeader {
        char riff[4] = {'R', 'I', 'F', 'F'};
        uint32_t fileSize = 0;
        char wave[4] = {'W', 'A', 'V', 'E'};
        char fmt[4] = {'f', 'm', 't', ' '};
        uint32_t fmtSize = 16;
        uint16_t audioFormat = 1; // PCM
        uint16_t numChannels = 1;
        uint32_t sampleRate = 44100;
        uint32_t byteRate = 0;
        uint16_t blockAlign = 0;
        uint16_t bitsPerSample = 16;
        char data[4] = {'d', 'a', 't', 'a'};
        uint32_t dataSize = 0;
    };

    WavHeader header;
    header.numChannels = channels_;
    header.sampleRate = static_cast<uint32_t>(sampleRate_);
    header.bitsPerSample = 16;
    header.byteRate = header.sampleRate * header.numChannels * header.bitsPerSample / 8;
    header.blockAlign = header.numChannels * header.bitsPerSample / 8;

    fwrite(&header, sizeof(WavHeader), 1, file);
    return true;
}

bool AudioRecorder::updateWavHeader(FILE* file) {
    if (!file) return false;

    uint32_t dataSize = totalFramesRecorded_ * channels_ * sizeof(int16_t);
    uint32_t fileSize = dataSize + 36;

    // Update file size
    fseek(file, 4, SEEK_SET);
    fwrite(&fileSize, sizeof(uint32_t), 1, file);

    // Update data size
    fseek(file, 40, SEEK_SET);
    fwrite(&dataSize, sizeof(uint32_t), 1, file);

    return true;
}
