#include "audio_devices.h"
#include "app_common.h"
#include <propsys.h>

static const PROPERTYKEY PKEY_DEVICE_FRIENDLYNAME_LOCAL = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
};

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
