#include "system_volume.h"

class SystemVolumeManager::EndpointVolumeCallback : public IAudioEndpointVolumeCallback {
public:
    EndpointVolumeCallback(SystemVolumeManager* owner, const std::string& deviceId)
        : owner_(owner), device_id_(deviceId) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAudioEndpointVolumeCallback)) {
            *ppv = static_cast<IAudioEndpointVolumeCallback*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG r = InterlockedDecrement(&ref_);
        if (r == 0) delete this;
        return r;
    }

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data) override {
        if (!data || !owner_) return S_OK;
        if (IsSameGuid(data->guidEventContext, kAudioFluxVolumeContext)) return S_OK;
        owner_->notify(device_id_, ScalarToVolume(data->fMasterVolume), data->bMuted != FALSE);
        return S_OK;
    }

private:
    SystemVolumeManager* owner_ = nullptr;
    std::string device_id_;
    ULONG ref_ = 1;
};

SystemVolumeManager::~SystemVolumeManager() {
    shutdown();
}

bool SystemVolumeManager::initialize(Callback callback) {
    callback_ = std::move(callback);
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator_.put()));
    return SUCCEEDED(hr) && enumerator_;
}

void SystemVolumeManager::shutdown() {
    for (auto& kv : watches_) {
        releaseWatch(kv.second);
    }
    watches_.clear();
    enumerator_.reset();
    callback_ = nullptr;
}

SystemVolumeState SystemVolumeManager::getState(const std::string& deviceId) {
    SystemVolumeState state;
    auto endpoint = getEndpoint(deviceId);
    if (!endpoint) return state;

    float scalar = 1.0f;
    BOOL muted = FALSE;
    if (SUCCEEDED(endpoint->GetMasterVolumeLevelScalar(&scalar))) {
        state.volume = ScalarToVolume(scalar);
    }
    if (SUCCEEDED(endpoint->GetMute(&muted))) {
        state.muted = muted != FALSE;
    }
    return state;
}

bool SystemVolumeManager::setState(const std::string& deviceId, int volume, bool muted) {
    auto endpoint = getEndpoint(deviceId);
    if (!endpoint) return false;

    HRESULT hr1 = endpoint->SetMasterVolumeLevelScalar(VolumeToScalar(volume), &kAudioFluxVolumeContext);
    HRESULT hr2 = endpoint->SetMute(muted ? TRUE : FALSE, &kAudioFluxVolumeContext);
    return SUCCEEDED(hr1) && SUCCEEDED(hr2);
}

void SystemVolumeManager::watchDevice(const std::string& deviceId) {
    if (!enumerator_ || watches_.find(deviceId) != watches_.end()) return;

    auto endpoint = getEndpoint(deviceId);
    if (!endpoint) return;

    auto* watch = new EndpointWatch();
    watch->device_id = deviceId;
    watch->endpoint = std::move(endpoint);
    watch->callback = new EndpointVolumeCallback(this, deviceId);

    HRESULT hr = watch->endpoint->RegisterControlChangeNotify(watch->callback);
    if (FAILED(hr)) {
        releaseWatch(watch);
        return;
    }
    watches_[deviceId] = watch;
}

void SystemVolumeManager::unwatchMissingDevices(const std::vector<DeviceInfo>& devices) {
    std::vector<std::string> removeIds;
    for (auto& kv : watches_) {
        bool found = false;
        for (auto& d : devices) {
            if (d.id == kv.first) {
                found = true;
                break;
            }
        }
        if (!found) removeIds.push_back(kv.first);
    }
    for (auto& id : removeIds) {
        auto it = watches_.find(id);
        if (it == watches_.end()) continue;
        releaseWatch(it->second);
        watches_.erase(it);
    }
}

ComPtr<IAudioEndpointVolume> SystemVolumeManager::getEndpoint(const std::string& deviceId) {
    ComPtr<IAudioEndpointVolume> endpoint;
    if (!enumerator_) return endpoint;

    std::wstring wid = Utf8ToWide(deviceId);
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator_->GetDevice(wid.c_str(), device.put())) || !device) return endpoint;

    device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                     reinterpret_cast<void**>(endpoint.put()));
    return endpoint;
}

void SystemVolumeManager::notify(const std::string& deviceId, int volume, bool muted) {
    if (callback_) callback_(deviceId, volume, muted);
}

void SystemVolumeManager::releaseWatch(EndpointWatch* watch) {
    if (!watch) return;
    if (watch->endpoint && watch->callback) {
        watch->endpoint->UnregisterControlChangeNotify(watch->callback);
    }
    if (watch->callback) watch->callback->Release();
    delete watch;
}
