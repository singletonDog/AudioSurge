#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include "ring_buffer.h"

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif
#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

struct DeviceInfo {
    std::string id;
    std::string name;
    bool is_default;
    int volume = 100;
    bool muted = false;
};

/**
 * WASAPI 多设备音频输出
 * 从共享环形缓冲区读取数据，同时渲染到多个输出设备。
 * 每个设备独立线程，支持运行时 HPF/LPF/音量 调整。
 */
class WasapiOutput {
public:
    explicit WasapiOutput(SharedAudioBuffer& buffer);
    ~WasapiOutput();

    bool initialize();
    void setFormat(int sample_rate, int channels);
    std::vector<DeviceInfo> enumerateDevices();

    // hpf_hz: 0=关闭, >0=高通截止频率
    // lpf_hz: 0=关闭, >0=低通截止频率
    // volume: 0-100
    bool startDevice(const std::string& device_id,
                     float hpf_hz, float lpf_hz, int volume);
    bool stopDevice(const std::string& device_id);
    bool isDeviceRunning(const std::string& device_id) const;
    const std::string& getLastError() const { return last_error_; }

    // 运行时更新单个设备参数（线程安全，渲染线程自动检测变化并重算系数）
    void updateDevice(const std::string& device_id,
                      float hpf_hz, float lpf_hz, int volume);

    void stopAll();

private:
    // Biquad 滤波器 (Robert Bristow-Johnson cookbook)
    struct BiquadFilter {
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

        void setHPF(float fc, float fs);   // 2阶 Butterworth 高通 (-12dB/oct)
        void setLPF(float fc, float fs);   // 2阶 Butterworth 低通 (-12dB/oct)
        void reset();                      // 清零状态
        float process(float x);            // Direct Form I
    };

    struct RenderStream {
        static constexpr int MAX_CHANNELS = 8;

        std::string device_id;
        IMMDevice* device = nullptr;
        IAudioClient* audio_client = nullptr;
        IAudioRenderClient* render_client = nullptr;
        HANDLE event = nullptr;
        std::thread thread;
        std::atomic<bool> running{false};
        size_t read_pos = 0;
        UINT32 buffer_frames = 0;

        std::atomic<int> hpf_hz{0};     // 0=关闭, >0=高通截止频率(Hz)
        std::atomic<int> lpf_hz{0};     // 0=关闭, >0=低通截止频率(Hz)
        std::atomic<int> volume{100};   // 0-100

        // 音量平滑过渡：当前音量缩放因子，渲染线程专用（无需原子）
        float current_vol_scale = 1.0f;

        // HPF/LPF 单级 2阶 biquad (-12dB/oct) [channel]
        BiquadFilter hpf_filters[MAX_CHANNELS];
        BiquadFilter lpf_filters[MAX_CHANNELS];
    };

    void renderThread(RenderStream* stream);
    bool createStream(IMMDevice* device, const std::string& id,
                      int hpf_hz, int lpf_hz, int volume);

    SharedAudioBuffer& buffer_;
    std::vector<RenderStream*> streams_;
    IMMDeviceEnumerator* enumerator_ = nullptr;
    std::string last_error_;
    int sample_rate_ = 44100;
    int channels_ = 2;
    std::atomic<bool> running_{false};
};
