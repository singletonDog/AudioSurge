#pragma once

#include "app_common.h"
#include "wasapi_output.h"
#include <endpointvolume.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

struct SystemVolumeState {
    int volume = 100;
    bool muted = false;
};

class SystemVolumeManager {
public:
    using Callback = std::function<void(const std::string&, int, bool)>;

    SystemVolumeManager() = default;
    ~SystemVolumeManager();

    bool initialize(Callback callback);
    void shutdown();
    SystemVolumeState getState(const std::string& deviceId);
    bool setState(const std::string& deviceId, int volume, bool muted);
    void watchDevice(const std::string& deviceId);
    void unwatchMissingDevices(const std::vector<DeviceInfo>& devices);

private:
    class EndpointVolumeCallback;

    struct EndpointWatch {
        std::string device_id;
        ComPtr<IAudioEndpointVolume> endpoint;
        EndpointVolumeCallback* callback = nullptr;
    };

    ComPtr<IAudioEndpointVolume> getEndpoint(const std::string& deviceId);
    void notify(const std::string& deviceId, int volume, bool muted);
    void releaseWatch(EndpointWatch* watch);

    ComPtr<IMMDeviceEnumerator> enumerator_;
    std::map<std::string, EndpointWatch*> watches_;
    Callback callback_;
};
