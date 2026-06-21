#include "wasapi_output.h"
#include <iostream>
#include <cstring>
#include <cmath>

// 设备友好名称属性键
static const PROPERTYKEY PKEY_DEVICE_FRIENDLYNAME = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
};

// ============ BiquadFilter ============

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
        std::cerr << "[错误] CoInitialize 失败" << std::endl;
        return false;
    }
    // 创建设备枚举器
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr)) {
        std::cerr << "[错误] 创建设备枚举器失败" << std::endl;
        return false;
    }
    return true;
}

void WasapiOutput::setFormat(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;
}

std::string WasapiOutput::getDeviceName(IMMDevice* device) {
    std::string result;
    IPropertyStore* props = nullptr;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &props))) return result;
    PROPVARIANT name;
    PropVariantInit(&name);
    if (SUCCEEDED(props->GetValue(PKEY_DEVICE_FRIENDLYNAME, &name))) {
        std::wstring ws(name.pwszVal);
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            result.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &result[0], len, nullptr, nullptr);
        }
        PropVariantClear(&name);
    }
    props->Release();
    return result;
}

std::string WasapiOutput::getDeviceId(IMMDevice* device) {
    std::string result;
    LPWSTR id = nullptr;
    if (SUCCEEDED(device->GetId(&id))) {
        std::wstring ws(id);
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            result.resize(len - 1);
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &result[0], len, nullptr, nullptr);
        }
        CoTaskMemFree(id);
    }
    return result;
}

std::vector<DeviceInfo> WasapiOutput::enumerateDevices() {
    std::vector<DeviceInfo> devices;
    if (!enumerator_) return devices;

    // 获取默认输出设备ID
    std::string default_id;
    IMMDevice* default_dev = nullptr;
    if (SUCCEEDED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &default_dev))) {
        default_id = getDeviceId(default_dev);
        default_dev->Release();
    }

    // 枚举所有活跃的渲染设备
    IMMDeviceCollection* collection = nullptr;
    if (FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection)))
        return devices;
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device))) continue;
        DeviceInfo info;
        info.id = getDeviceId(device);
        info.name = getDeviceName(device);
        info.is_default = (info.id == default_id);
        devices.push_back(info);
        device->Release();
    }
    collection->Release();
    return devices;
}

bool WasapiOutput::startDevice(const std::string& device_id,
                               float hpf_hz, float lpf_hz, int volume) {
    if (!running_) running_ = true;

    // UTF-8 设备ID -> wstring
    int wlen = MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, nullptr, 0);
    std::wstring wid(wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, device_id.c_str(), -1, &wid[0], wlen);

    IMMDevice* device = nullptr;
    if (FAILED(enumerator_->GetDevice(wid.c_str(), &device))) {
        std::cerr << "[错误] 找不到设备: " << device_id << std::endl;
        return false;
    }
    bool ok = createStream(device, device_id, (int)hpf_hz, (int)lpf_hz, volume);
    if (!ok) device->Release();
    return ok;
}

void WasapiOutput::updateDevice(const std::string& device_id,
                                float hpf_hz, float lpf_hz, int volume) {
    for (auto* s : streams_) {
        if (s->device_id == device_id) {
            s->hpf_hz.store((int)hpf_hz, std::memory_order_release);
            s->lpf_hz.store((int)lpf_hz, std::memory_order_release);
            s->volume.store(volume, std::memory_order_release);
            break;
        }
    }
}

bool WasapiOutput::createStream(IMMDevice* device, const std::string& id,
                                int hpf_hz, int lpf_hz, int volume) {
    if (channels_ > RenderStream::MAX_CHANNELS) {
        std::cerr << "[错误] 声道数 " << channels_ << " 超过最大值 "
                  << RenderStream::MAX_CHANNELS << std::endl;
        return false;
    }

    // 激活音频客户端
    IAudioClient* audio_client = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client);
    if (FAILED(hr)) {
        std::cerr << "[错误] 激活音频客户端失败" << std::endl;
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
        std::cerr << "[错误] 初始化音频客户端失败" << std::endl;
        audio_client->Release();
        CloseHandle(event);
        return false;
    }

    // 绑定事件句柄
    hr = audio_client->SetEventHandle(event);
    if (FAILED(hr)) {
        std::cerr << "[错误] 设置事件句柄失败" << std::endl;
        audio_client->Release();
        CloseHandle(event);
        return false;
    }

    // 获取渲染客户端
    IAudioRenderClient* render_client = nullptr;
    hr = audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&render_client);
    if (FAILED(hr)) {
        std::cerr << "[错误] 获取渲染客户端失败" << std::endl;
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

    while (running_) {
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
                for (int s = 0; s < 2; ++s)
                    for (int ch = 0; ch < channels_; ++ch) {
                        stream->hpf_filters[s][ch].reset();
                        stream->hpf_filters[s][ch].setHPF((float)hpf, (float)sample_rate_);
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
                for (int s = 0; s < 2; ++s)
                    for (int ch = 0; ch < channels_; ++ch) {
                        stream->lpf_filters[s][ch].reset();
                        stream->lpf_filters[s][ch].setLPF((float)lpf, (float)sample_rate_);
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
        if (s->thread.joinable()) s->thread.join();
        if (s->render_client) s->render_client->Release();
        if (s->audio_client) s->audio_client->Release();
        if (s->device) s->device->Release();
        if (s->event) CloseHandle(s->event);
        delete s;
    }
    streams_.clear();
}
