#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include "ring_buffer.h"

// Windows 10+ 自动格式转换标志
#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

/**
 * WASAPI 多设备输出
 * 将共享缓冲区中的音频数据同时输出到多个设备。
 * 使用 AUTOCONVERTPCM 让 Windows 内核自动处理采样率/格式转换。
 * 自动跳过默认设备以避免回声反馈。
 */
class WasapiOutput {
public:
    explicit WasapiOutput(SharedAudioBuffer& buffer);
    ~WasapiOutput();

    bool initialize(int sample_rate, int channels);
    bool startAll();
    void stopAll();

private:
    struct RenderStream {
        IMMDevice* device = nullptr;
        IAudioClient* audio_client = nullptr;
        IAudioRenderClient* render_client = nullptr;
        std::thread thread;
        size_t read_pos = 0;          // 独立读取位置
        UINT32 buffer_frames = 0;     // WASAPI 缓冲区帧数
    };

    void renderThread(RenderStream* stream);
    bool createStream(IMMDevice* device, const std::string& name);
    std::string getDeviceName(IMMDevice* device);

    SharedAudioBuffer& buffer_;
    std::vector<RenderStream*> streams_;
    IMMDeviceEnumerator* enumerator_ = nullptr;
    int sample_rate_ = 44100;
    int channels_ = 2;
    std::atomic<bool> running_{false};
};