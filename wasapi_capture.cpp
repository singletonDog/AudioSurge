#include "wasapi_capture.h"
#include <iostream>
#include <cstring>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "ksuser.lib")

// IEEE Float 子格式 GUID，用于检测 WAVEFORMATEXTENSIBLE 是否为 float 格式
static const GUID SUBTYPE_IEEE_FLOAT = 
    {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

WasapiCapture::WasapiCapture(SharedAudioBuffer& buffer) : buffer_(buffer) {}

WasapiCapture::~WasapiCapture() {
    stop();
    if (capture_client_) capture_client_->Release();
    if (audio_client_) audio_client_->Release();
    if (device_) device_->Release();
    if (enumerator_) enumerator_->Release();
    if (event_) CloseHandle(event_);
    CoUninitialize();
}

bool WasapiCapture::isFloatFormat(const WAVEFORMATEX* format) {
    if (format->wFormatTag == 3) return true;  // WAVE_FORMAT_IEEE_FLOAT
    if (format->wFormatTag == 0xFFFE) {        // WAVE_FORMAT_EXTENSIBLE
        auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return ext->SubFormat == SUBTYPE_IEEE_FLOAT;
    }
    return false;
}

bool WasapiCapture::initialize() {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "[错误] CoInitialize 失败" << std::endl;
        return false;
    }

    // 创建设备枚举器
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr)) { std::cerr << "[错误] 创建设备枚举器失败" << std::endl; return false; }

    // 获取默认输出设备
    hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr)) { std::cerr << "[错误] 获取默认输出设备失败" << std::endl; return false; }

    // 激活音频客户端
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client_);
    if (FAILED(hr)) { std::cerr << "[错误] 激活音频客户端失败" << std::endl; return false; }

    // 获取系统混音格式
    WAVEFORMATEX* format = nullptr;
    hr = audio_client_->GetMixFormat(&format);
    if (FAILED(hr)) { std::cerr << "[错误] 获取音频格式失败" << std::endl; return false; }

    sample_rate_ = format->nSamplesPerSec;
    channels_ = format->nChannels;
    bytes_per_sample_ = format->wBitsPerSample / 8;
    is_float_format_ = isFloatFormat(format);

    std::cout << "[信息] 捕获格式: " << sample_rate_ << "Hz, " << channels_ << "ch, "
              << format->wBitsPerSample << "bit, "
              << (is_float_format_ ? "Float" : "PCM") << std::endl;

    // 初始化 Loopback + 事件驱动模式（缓冲区 10ms）
    event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    hr = audio_client_->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                    100000, 0, format, nullptr);
    CoTaskMemFree(format);
    if (FAILED(hr)) { std::cerr << "[错误] 初始化 Loopback 失败" << std::endl; return false; }

    // 绑定事件：缓冲区有数据时自动触发，无需轮询
    hr = audio_client_->SetEventHandle(event_);
    if (FAILED(hr)) { std::cerr << "[错误] 设置事件句柄失败" << std::endl; return false; }

    // 获取捕获客户端
    hr = audio_client_->GetService(__uuidof(IAudioCaptureClient), (void**)&capture_client_);
    if (FAILED(hr)) { std::cerr << "[错误] 获取捕获客户端失败" << std::endl; return false; }

    std::cout << "[信息] WASAPI Loopback 初始化成功" << std::endl;
    return true;
}

void WasapiCapture::convertIntToFloat(const BYTE* data, float* output, size_t count) {
    if (bytes_per_sample_ == 2) {
        auto* p = reinterpret_cast<const int16_t*>(data);
        for (size_t i = 0; i < count; ++i)
            output[i] = static_cast<float>(p[i]) / 32768.0f;
    } else if (bytes_per_sample_ == 3) {
        for (size_t i = 0; i < count; ++i) {
            int32_t v = 0;
            memcpy(&v, data + i * 3, 3);
            if (v & 0x800000) v |= 0xFF000000;
            output[i] = static_cast<float>(v) / 8388608.0f;
        }
    } else if (bytes_per_sample_ == 4) {
        auto* p = reinterpret_cast<const int32_t*>(data);
        for (size_t i = 0; i < count; ++i)
            output[i] = static_cast<float>(p[i]) / 2147483648.0f;
    }
}

void WasapiCapture::captureThread() {
    audio_client_->Start();
    std::cout << "[信息] WASAPI Loopback 捕获已启动 (事件驱动)" << std::endl;

    std::vector<float> convert_buf;

    while (running_) {
        // 等待事件：缓冲区有数据时立即唤醒，无数据时挂起（零 CPU 开销）
        DWORD wait = WaitForSingleObject(event_, 100);
        if (wait == WAIT_TIMEOUT) continue;

        UINT32 packet_len = 0;
        capture_client_->GetNextPacketSize(&packet_len);

        while (packet_len != 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;

            if (FAILED(capture_client_->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;

            size_t samples = frames * channels_;
            if (convert_buf.size() < samples) convert_buf.resize(samples);

            if (data && !(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                if (is_float_format_) {
                    buffer_.write(reinterpret_cast<const float*>(data), samples);
                } else {
                    convertIntToFloat(data, convert_buf.data(), samples);
                    buffer_.write(convert_buf.data(), samples);
                }
            } else {
                std::fill(convert_buf.begin(), convert_buf.begin() + samples, 0.0f);
                buffer_.write(convert_buf.data(), samples);
            }

            capture_client_->ReleaseBuffer(frames);
            if (FAILED(capture_client_->GetNextPacketSize(&packet_len))) break;
        }
    }

    audio_client_->Stop();
    std::cout << "[信息] WASAPI Loopback 捕获已停止" << std::endl;
}

void WasapiCapture::start() {
    if (running_) return;
    running_ = true;
    thread_ = std::thread(&WasapiCapture::captureThread, this);
}

void WasapiCapture::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
}