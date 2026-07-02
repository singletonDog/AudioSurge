#include "wasapi_output.h"
#include "audio_devices.h"
#include "app_common.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

// ============ BiquadFilter ============

static float ClampFilterHz(int hz, int sample_rate) {
    if (hz <= 0) return 0.0f;
    float max_hz = (std::max)(20.0f, sample_rate * 0.45f);
    return (std::max)(20.0f, (std::min)((float)hz, max_hz));
}

void WasapiOutput::BiquadFilter::setHPF(float fc, float fs) {
    // RBJ cookbook: 2阶 Butterworth 高通
    float K = tanf(3.14159265f * fc / fs);
    float Q = 0.7071f;  // Butterworth Q值
    float norm = 1.0f / (1.0f + K / Q + K * K);
    b0 = norm;
    b1 = -2.0f * norm;
    b2 = norm;
    a1 = 2.0f * (K * K - 1.0f) * norm;
    a2 = (1.0f - K / Q + K * K) * norm;
}

void WasapiOutput::BiquadFilter::setLPF(float fc, float fs) {
    // RBJ cookbook: 2阶 Butterworth 低通
    float K = tanf(3.14159265f * fc / fs);
    float Q = 0.7071f;
    float norm = 1.0f / (1.0f + K / Q + K * K);
    b0 = K * K * norm;
    b1 = 2.0f * K * K * norm;
    b2 = K * K * norm;
    a1 = 2.0f * (K * K - 1.0f) * norm;
    a2 = (1.0f - K / Q + K * K) * norm;
}

void WasapiOutput::BiquadFilter::reset() {
    x1 = x2 = y1 = y2 = 0.0f;
}

float WasapiOutput::BiquadFilter::process(float x) {
    // Direct Form I: y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2
    float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
}

// ============ WasapiOutput ============

WasapiOutput::WasapiOutput(SharedAudioBuffer& buffer) : buffer_(buffer) {}

WasapiOutput::~WasapiOutput() {
    stopAll();
    if (enumerator_) enumerator_->Release();
    enumerator_ = nullptr;
    // 不调用 CoUninitialize - COM 由调用者管理
}

bool WasapiOutput::initialize() {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        last_error_ = HResultMessage("CoInitialize 失败", hr);
        std::cerr << "[错误] " << last_error_ << std::endl;
        return false;
    }
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr)) {
        last_error_ = HResultMessage("创建设备枚举器失败", hr);
        std::cerr << "[错误] " << last_error_ << std::endl;
        return false;
    }
    return true;
}

void WasapiOutput::setFormat(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;
}

std::vector<DeviceInfo> WasapiOutput::enumerateDevices() {
    return EnumerateRenderDevices();
}

bool WasapiOutput::startDevice(const std::string& device_id,
                               float hpf_hz, float lpf_hz, int volume) {
    last_error_.clear();
    if (!enumerator_) {
        last_error_ = "输出设备枚举器未初始化";
        return false;
    }
    if (!running_) running_ = true;
    if (isDeviceRunning(device_id)) {
        updateDevice(device_id, hpf_hz, lpf_hz, volume);
        return true;
    }

    int wlen = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        last_error_ = "设备 ID 转换失败";
        return false;
    }
    std::wstring wid(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, &wid[0], wlen);

    IMMDevice* device = nullptr;
    HRESULT hr = enumerator_->GetDevice(wid.c_str(), &device);
    if (FAILED(hr) || !device) {
        last_error_ = HResultMessage("找不到输出设备", hr);
        std::cerr << "[错误] 找不到设备: " << device_id << std::endl;
        return false;
    }
    bool ok = createStream(device, device_id, (int)hpf_hz, (int)lpf_hz, volume);
    if (!ok) device->Release();
    return ok;
}

bool WasapiOutput::isDeviceRunning(const std::string& device_id) const {
    for (auto* s : streams_) {
        if (s->device_id == device_id) return true;
    }
    return false;
}

bool WasapiOutput::stopDevice(const std::string& device_id) {
    for (auto it = streams_.begin(); it != streams_.end(); ++it) {
        RenderStream* s = *it;
        if (s->device_id != device_id) continue;
        s->running.store(false, std::memory_order_release);
        if (s->event) SetEvent(s->event);
        if (s->thread.joinable()) s->thread.join();
        if (s->render_client) s->render_client->Release();
        if (s->audio_client) s->audio_client->Release();
        if (s->device) s->device->Release();
        if (s->event) CloseHandle(s->event);
        delete s;
        streams_.erase(it);
        if (streams_.empty()) running_ = false;
        return true;
    }
    return false;
}

void WasapiOutput::updateDevice(const std::string& device_id,
                                float hpf_hz, float lpf_hz, int volume) {
    int hpf = (int)ClampFilterHz((int)hpf_hz, sample_rate_);
    int lpf = (int)ClampFilterHz((int)lpf_hz, sample_rate_);
    for (auto* s : streams_) {
        if (s->device_id == device_id) {
            s->hpf_hz.store(hpf, std::memory_order_release);
            s->lpf_hz.store(lpf, std::memory_order_release);
            s->volume.store(volume, std::memory_order_release);
            break;
        }
    }
}

bool WasapiOutput::createStream(IMMDevice* device, const std::string& id,
                                int hpf_hz, int lpf_hz, int volume) {
    last_error_.clear();
    hpf_hz = (int)ClampFilterHz(hpf_hz, sample_rate_);
    lpf_hz = (int)ClampFilterHz(lpf_hz, sample_rate_);
    if (channels_ > RenderStream::MAX_CHANNELS) {
        last_error_ = "输出声道数超过支持上限";
        std::cerr << "[错误] 声道数 " << channels_ << " 超过最大值 "
                  << RenderStream::MAX_CHANNELS << std::endl;
        return false;
    }

    // 激活音频客户端
    IAudioClient* audio_client = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client);
    if (FAILED(hr)) {
        last_error_ = HResultMessage("激活输出音频客户端失败", hr);
        std::cerr << "[错误] " << last_error_ << std::endl;
        return false;
    }

    // 构建 float32 PCM 格式
    WAVEFORMATEX format = {};
    format.wFormatTag = 3;  // WAVE_FORMAT_IEEE_FLOAT
    format.nChannels = (WORD)channels_;
    format.nSamplesPerSec = (DWORD)sample_rate_;
    format.wBitsPerSample = 32;
    format.nBlockAlign = format.nChannels * 4;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    // 初始化：共享模式 + 自动格式转换 + SRC高质量 + 事件驱动
    HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                   AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY |
                                   AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                   100000, 0, &format, nullptr);
    if (FAILED(hr)) {
        last_error_ = HResultMessage("初始化输出音频客户端失败", hr);
        std::cerr << "[错误] " << last_error_ << std::endl;
        audio_client->Release();
        CloseHandle(event);
        return false;
    }

    // 绑定事件句柄
    hr = audio_client->SetEventHandle(event);
    if (FAILED(hr)) {
        last_error_ = HResultMessage("设置输出设备事件句柄失败", hr);
        std::cerr << "[错误] " << last_error_ << std::endl;
        audio_client->Release();
        CloseHandle(event);
        return false;
    }

    // 获取渲染客户端
    IAudioRenderClient* render_client = nullptr;
    hr = audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&render_client);
    if (FAILED(hr)) {
        last_error_ = HResultMessage("获取输出渲染客户端失败", hr);
        std::cerr << "[错误] " << last_error_ << std::endl;
        audio_client->Release();
        CloseHandle(event);
        return false;
    }

    // 获取缓冲区大小并预填充静音
    UINT32 buffer_frames = 0;
    audio_client->GetBufferSize(&buffer_frames);
    BYTE* data = nullptr;
    if (SUCCEEDED(render_client->GetBuffer(buffer_frames, &data))) {
        memset(data, 0, buffer_frames * channels_ * sizeof(float));
        render_client->ReleaseBuffer(buffer_frames, 0);
    }

    // 创建渲染流
    auto* stream = new RenderStream();
    stream->device_id = id;
    stream->device = device;
    stream->audio_client = audio_client;
    stream->render_client = render_client;
    stream->event = event;
    stream->read_pos = buffer_.getWritePos();  // 从当前写入位置开始读
    stream->buffer_frames = buffer_frames;
    stream->hpf_hz.store(hpf_hz, std::memory_order_relaxed);
    stream->lpf_hz.store(lpf_hz, std::memory_order_relaxed);
    stream->volume.store(volume, std::memory_order_relaxed);
    stream->current_vol_scale = volume / 100.0f;  // 初始化为当前音量
    stream->running.store(true, std::memory_order_relaxed);

    // 设置初始滤波器系数
    if (hpf_hz > 0) {
        for (int s = 0; s < 2; ++s)
            for (int ch = 0; ch < channels_; ++ch)
                stream->hpf_filters[s][ch].setHPF((float)hpf_hz, (float)sample_rate_);
    }
    if (lpf_hz > 0) {
        for (int s = 0; s < 2; ++s)
            for (int ch = 0; ch < channels_; ++ch)
                stream->lpf_filters[s][ch].setLPF((float)lpf_hz, (float)sample_rate_);
    }

    // 启动渲染线程
    stream->thread = std::thread(&WasapiOutput::renderThread, this, stream);
    streams_.push_back(stream);
    return true;
}

void WasapiOutput::renderThread(RenderStream* stream) {
    CoInitialize(nullptr);
    stream->audio_client->Start();

    // 缓存上次的参数值，用于检测变化
    int prev_hpf = stream->hpf_hz.load(std::memory_order_acquire);
    int prev_lpf = stream->lpf_hz.load(std::memory_order_acquire);

    // 音量平滑过渡：约10ms的ramp时间
    const float vol_ramp_rate = 1.0f / (sample_rate_ * 0.01f);

    // 启动时从0淡入，避免初始咔嗒声
    stream->current_vol_scale = 0.0f;
    bool fading_in = true;

    while (running_ && stream->running.load(std::memory_order_acquire)) {
        // 等待渲染缓冲区事件
        DWORD wait = WaitForSingleObject(stream->event, 100);
        if (wait == WAIT_TIMEOUT) continue;

        // 读取当前参数
        int hpf = stream->hpf_hz.load(std::memory_order_acquire);
        int lpf = stream->lpf_hz.load(std::memory_order_acquire);
        int vol = stream->volume.load(std::memory_order_acquire);
        float target_vol_scale = vol / 100.0f;

        // 参数变化时重新计算 biquad 系数并重置状态
        if (hpf != prev_hpf) {
            if (hpf > 0) {
                float safe_hpf = ClampFilterHz(hpf, sample_rate_);
                for (int s = 0; s < 2; ++s)
                    for (int ch = 0; ch < channels_; ++ch) {
                        stream->hpf_filters[s][ch].reset();
                        stream->hpf_filters[s][ch].setHPF(safe_hpf, (float)sample_rate_);
                    }
            } else {
                for (int s = 0; s < 2; ++s)
                    for (int ch = 0; ch < channels_; ++ch)
                        stream->hpf_filters[s][ch].reset();
            }
            prev_hpf = hpf;
        }
        if (lpf != prev_lpf) {
            if (lpf > 0) {
                float safe_lpf = ClampFilterHz(lpf, sample_rate_);
                for (int s = 0; s < 2; ++s)
                    for (int ch = 0; ch < channels_; ++ch) {
                        stream->lpf_filters[s][ch].reset();
                        stream->lpf_filters[s][ch].setLPF(safe_lpf, (float)sample_rate_);
                    }
            } else {
                for (int s = 0; s < 2; ++s)
                    for (int ch = 0; ch < channels_; ++ch)
                        stream->lpf_filters[s][ch].reset();
            }
            prev_lpf = lpf;
        }

        // 计算可用帧数
        UINT32 padding = 0;
        if (FAILED(stream->audio_client->GetCurrentPadding(&padding))) break;
        UINT32 available = stream->buffer_frames - padding;
        if (available == 0) continue;

        // 获取渲染缓冲区
        BYTE* data = nullptr;
        if (FAILED(stream->render_client->GetBuffer(available, &data))) break;

        // 从共享缓冲区读取音频数据
        bool jumped = false;
        buffer_.read(reinterpret_cast<float*>(data), available * channels_, stream->read_pos, &jumped);

        // 应用滤波器和音量（带平滑过渡）
        float* samples = reinterpret_cast<float*>(data);
        for (UINT32 i = 0; i < available; ++i) {
            // 平滑音量过渡
            if (stream->current_vol_scale < target_vol_scale) {
                stream->current_vol_scale = (std::min)(stream->current_vol_scale + vol_ramp_rate, target_vol_scale);
            } else if (stream->current_vol_scale > target_vol_scale) {
                stream->current_vol_scale = (std::max)(stream->current_vol_scale - vol_ramp_rate, target_vol_scale);
            }

            // 缓冲区跳跃时前50帧淡入
            float fade = 1.0f;
            if (jumped && i < 50) {
                fade = (float)i / 50.0f;
            }

            float vol_scale = stream->current_vol_scale * fade;

            for (int ch = 0; ch < channels_; ++ch) {
                size_t idx = i * channels_ + ch;
                float v = samples[idx];

                // HPF: 2级级联 = 4阶 Butterworth
                if (hpf > 0) {
                    v = stream->hpf_filters[0][ch].process(v);
                    v = stream->hpf_filters[1][ch].process(v);
                }
                // LPF: 2级级联 = 4阶 Butterworth
                if (lpf > 0) {
                    v = stream->lpf_filters[0][ch].process(v);
                    v = stream->lpf_filters[1][ch].process(v);
                }
                // 音量缩放
                v *= vol_scale;
                samples[idx] = v;
            }
        }

        fading_in = false;

        // 释放缓冲区
        stream->render_client->ReleaseBuffer(available, 0);
    }

    stream->audio_client->Stop();
    CoUninitialize();
}

void WasapiOutput::stopAll() {
    running_ = false;
    for (auto* s : streams_) {
        s->running.store(false, std::memory_order_release);
        if (s->event) SetEvent(s->event);
    }
    for (auto* s : streams_) {
        if (s->thread.joinable()) s->thread.join();
        if (s->render_client) s->render_client->Release();
        if (s->audio_client) s->audio_client->Release();
        if (s->device) s->device->Release();
        if (s->event) CloseHandle(s->event);
        delete s;
    }
    streams_.clear();
}
