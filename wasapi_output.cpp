#include "wasapi_output.h"
#include <iostream>
#include <cstring>

// PKEY_Device_FriendlyName
static const PROPERTYKEY PKEY_DEVICE_FRIENDLYNAME = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
};

WasapiOutput::WasapiOutput(SharedAudioBuffer& buffer) : buffer_(buffer) {}

WasapiOutput::~WasapiOutput() {
    stopAll();
    if (enumerator_) enumerator_->Release();
    CoUninitialize();
}

bool WasapiOutput::initialize(int sample_rate, int channels) {
    sample_rate_ = sample_rate;
    channels_ = channels;

    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "[错误] CoInitialize 失败" << std::endl;
        return false;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator), (void**)&enumerator_);
    if (FAILED(hr)) { std::cerr << "[错误] 创建设备枚举器失败" << std::endl; return false; }
    return true;
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

bool WasapiOutput::startAll() {
    // 获取默认设备 ID（跳过它以避免回声反馈）
    std::wstring default_id;
    IMMDevice* default_dev = nullptr;
    if (SUCCEEDED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, &default_dev))) {
        LPWSTR id = nullptr;
        default_dev->GetId(&id);
        default_id = id;
        CoTaskMemFree(id);
        default_dev->Release();
    }

    // 枚举所有活跃输出设备
    IMMDeviceCollection* collection = nullptr;
    if (FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
        std::cerr << "[错误] 枚举设备失败" << std::endl;
        return false;
    }

    UINT count = 0;
    collection->GetCount(&count);

    std::cout << "\n[信息] 正在启动输出设备..." << std::endl;
    running_ = true;
    int success = 0;

    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device))) continue;

        // 检查是否为默认设备
        LPWSTR id = nullptr;
        device->GetId(&id);
        bool is_default = (default_id == id);
        CoTaskMemFree(id);

        std::string name = getDeviceName(device);
        if (is_default) {
            std::cout << "[跳过] " << name << " (默认设备)" << std::endl;
            device->Release();
            continue;
        }

        if (createStream(device, name)) {
            success++;
        } else {
            device->Release();
        }
    }

    collection->Release();

    if (success == 0) {
        std::cerr << "[错误] 没有成功启动任何设备" << std::endl;
        running_ = false;
        return false;
    }

    std::cout << "[信息] 成功启动 " << success << " 个设备" << std::endl;
    return true;
}

bool WasapiOutput::createStream(IMMDevice* device, const std::string& name) {
    // 激活音频客户端
    IAudioClient* audio_client = nullptr;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&audio_client);
    if (FAILED(hr)) { std::cerr << "[警告] 无法激活设备 " << name << std::endl; return false; }

    // 构造与捕获格式一致的 float32 格式，Windows 内核自动转换到设备原生格式
    WAVEFORMATEX format = {};
    format.wFormatTag = 3;  // WAVE_FORMAT_IEEE_FLOAT
    format.nChannels = channels_;
    format.nSamplesPerSec = sample_rate_;
    format.wBitsPerSample = 32;
    format.nBlockAlign = format.nChannels * 4;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    // AUTOCONVERTPCM + SRC_DEFAULT_QUALITY：Windows 内核自动处理采样率转换
    hr = audio_client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                   1000000, 0, &format, nullptr);
    if (FAILED(hr)) {
        std::cerr << "[警告] 无法初始化设备 " << name << std::endl;
        audio_client->Release();
        return false;
    }

    // 获取渲染客户端
    IAudioRenderClient* render_client = nullptr;
    hr = audio_client->GetService(__uuidof(IAudioRenderClient), (void**)&render_client);
    if (FAILED(hr)) { audio_client->Release(); return false; }

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
    stream->device = device;
    stream->audio_client = audio_client;
    stream->render_client = render_client;
    stream->read_pos = buffer_.getWritePos();
    stream->buffer_frames = buffer_frames;
    stream->thread = std::thread(&WasapiOutput::renderThread, this, stream);

    streams_.push_back(stream);
    std::cout << "[成功] " << name << std::endl;
    return true;
}

void WasapiOutput::renderThread(RenderStream* stream) {
    CoInitialize(nullptr);
    stream->audio_client->Start();

    while (running_) {
        UINT32 padding = 0;
        if (FAILED(stream->audio_client->GetCurrentPadding(&padding))) break;

        UINT32 available = stream->buffer_frames - padding;
        if (available == 0) { Sleep(1); continue; }

        BYTE* data = nullptr;
        if (FAILED(stream->render_client->GetBuffer(available, &data))) break;

        // 从共享缓冲区非阻塞读取，每个流独立 read_pos
        buffer_.read(reinterpret_cast<float*>(data), available * channels_, stream->read_pos);

        if (FAILED(stream->render_client->ReleaseBuffer(available, 0))) break;
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
        delete s;
    }
    streams_.clear();
    std::cout << "[信息] 所有输出设备已停止" << std::endl;
}