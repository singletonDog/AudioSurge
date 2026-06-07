#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <atomic>
#include "ring_buffer.h"

/**
 * WASAPI Loopback 音频捕获
 * 捕获系统默认输出设备的音频（所有应用的混音），统一转为 float 写入共享缓冲区。
 */
class WasapiCapture {
public:
    explicit WasapiCapture(SharedAudioBuffer& buffer);
    ~WasapiCapture();

    bool initialize();
    void start();
    void stop();

    int getSampleRate() const { return sample_rate_; }
    int getChannels() const { return channels_; }

private:
    void captureThread();
    bool isFloatFormat(const WAVEFORMATEX* format);
    void convertIntToFloat(const BYTE* data, float* output, size_t sample_count);

    SharedAudioBuffer& buffer_;

    IMMDeviceEnumerator* enumerator_ = nullptr;
    IMMDevice* device_ = nullptr;
    IAudioClient* audio_client_ = nullptr;
    IAudioCaptureClient* capture_client_ = nullptr;

    std::thread thread_;
    std::atomic<bool> running_{false};

    int sample_rate_ = 44100;
    int channels_ = 2;
    bool is_float_format_ = true;
    int bytes_per_sample_ = 4;
};