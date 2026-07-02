#pragma once

#include "system_volume.h"
#include "wasapi_capture.h"
#include "wasapi_output.h"
#include <string>
#include <vector>

struct DeviceConfig {
    std::string id;
    std::string name;
    bool enabled = true;
    float hpf_hz = 0;
    float lpf_hz = 0;
    int volume = 100;
};

struct RuntimeUpdateResult {
    bool ok = true;
    bool device_started = false;
    bool device_stopped = false;
    std::string error;
};

std::string DeviceLabel(const std::string& name, const std::string& id);

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine();

    void setVolumeManager(SystemVolumeManager* volumeManager);
    bool start(const std::vector<DeviceConfig>& devices);
    void stop();
    RuntimeUpdateResult updateDevice(const DeviceConfig& config);
    bool restartForDefaultDeviceChange();
    std::vector<DeviceInfo> enumerateDevices();
    bool isRunning() const { return running_; }
    const std::string& getLastError() const { return last_error_; }

private:
    std::vector<DeviceConfig>::iterator findConfig(const std::string& id);
    void stopRuntime(bool clearConfigs);

    SharedAudioBuffer* buffer_ = nullptr;
    WasapiCapture* capture_ = nullptr;
    WasapiOutput* output_ = nullptr;
    bool running_ = false;
    std::vector<DeviceConfig> configs_;
    std::string last_error_;
    SystemVolumeManager* volumeManager_ = nullptr;
};
