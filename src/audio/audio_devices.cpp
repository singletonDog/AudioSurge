#include "audio_devices.h"
#include "app_common.h"
#include <audioclient.h>

static const PROPERTYKEY PKEY_DEVICE_FRIENDLYNAME_LOCAL = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
};

struct PolicyConfigDeviceShareMode;

struct IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(LPCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(LPCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(LPCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(LPCWSTR, INT, INT64*, INT64*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(LPCWSTR, INT64*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(LPCWSTR, PolicyConfigDeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(LPCWSTR, PolicyConfigDeviceShareMode*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(LPCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(LPCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(LPCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(LPCWSTR, INT) = 0;
};

static const GUID CLSID_PolicyConfigClient_Local =
    {0x870af99c, 0x171d, 0x4f9e, {0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9}};

static const GUID IID_IPolicyConfig_Local =
    {0xf8679f50, 0x850a, 0x41cf, {0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8}};

std::string GetDeviceId(IMMDevice* device) {
    std::string result;
    LPWSTR id = nullptr;
    if (SUCCEEDED(device->GetId(&id))) {
        result = WideToUtf8(id);
        CoTaskMemFree(id);
    }
    return result;
}

std::string GetDeviceFriendlyName(IMMDevice* device) {
    std::string result;
    ComPtr<IPropertyStore> props;
    if (FAILED(device->OpenPropertyStore(STGM_READ, props.put()))) return result;

    PROPVARIANT name;
    PropVariantInit(&name);
    if (SUCCEEDED(props->GetValue(PKEY_DEVICE_FRIENDLYNAME_LOCAL, &name)) && name.vt == VT_LPWSTR && name.pwszVal) {
        result = WideToUtf8(name.pwszVal);
    }
    PropVariantClear(&name);
    return result;
}

std::string GetDefaultRenderDeviceId() {
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.put()));
    if (FAILED(hr) || !enumerator) return {};

    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put())) || !device) return {};
    return GetDeviceId(device.get());
}

bool SetDefaultRenderDevice(const std::string& deviceId, std::string* errorMessage) {
    if (deviceId.empty()) {
        if (errorMessage) *errorMessage = "设备 ID 为空";
        return false;
    }

    std::wstring wideId = Utf8ToWide(deviceId);
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.put()));
    if (FAILED(hr) || !enumerator) {
        if (errorMessage) *errorMessage = HResultMessage("创建设备枚举器失败", hr);
        return false;
    }

    ComPtr<IMMDevice> device;
    hr = enumerator->GetDevice(wideId.c_str(), device.put());
    if (FAILED(hr) || !device) {
        if (errorMessage) *errorMessage = HResultMessage("找不到要切换的输出设备", hr);
        return false;
    }

    ComPtr<IPolicyConfig> policy;
    hr = CoCreateInstance(CLSID_PolicyConfigClient_Local, nullptr, CLSCTX_ALL,
                          IID_IPolicyConfig_Local, reinterpret_cast<void**>(policy.put()));
    if (FAILED(hr) || !policy) {
        if (errorMessage) *errorMessage = HResultMessage("打开系统默认设备切换接口失败", hr);
        return false;
    }

    ERole roles[] = { eConsole, eMultimedia, eCommunications };
    for (ERole role : roles) {
        hr = policy->SetDefaultEndpoint(wideId.c_str(), role);
        if (FAILED(hr)) {
            if (errorMessage) *errorMessage = HResultMessage("切换系统默认输出设备失败", hr);
            return false;
        }
    }
    return true;
}

std::vector<DeviceInfo> EnumerateRenderDevices() {
    std::vector<DeviceInfo> devices;
    ComPtr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator.put()));
    if (FAILED(hr) || !enumerator) return devices;

    std::string default_id;
    ComPtr<IMMDevice> default_device;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, default_device.put())) && default_device) {
        default_id = GetDeviceId(default_device.get());
    }

    ComPtr<IMMDeviceCollection> collection;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.put())) || !collection) {
        return devices;
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (FAILED(collection->Item(i, device.put())) || !device) continue;

        DeviceInfo info;
        info.id = GetDeviceId(device.get());
        info.name = GetDeviceFriendlyName(device.get());
        info.is_default = info.id == default_id;
        devices.push_back(info);
    }
    return devices;
}
