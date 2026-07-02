#include "audio_engine.h"
#include "audio_devices.h"
#include <algorithm>
#include <windows.h>

std::string DeviceLabel(const std::string& name, const std::string& id) {
    return name.empty() ? id : name;
}

AudioEngine::~AudioEngine() {
    stop();
}

void AudioEngine::setVolumeManager(SystemVolumeManager* volumeManager) {
    volumeManager_ = volumeManager;
}

bool AudioEngine::start(const std::vector<DeviceConfig>& devices) {
    if (running_) stop();
    last_error_.clear();

    buffer_ = new SharedAudioBuffer(480000);
    capture_ = new WasapiCapture(*buffer_);
    if (!capture_->initialize()) {
        last_error_ = "初始化默认捕获设备失败";
        delete capture_; capture_ = nullptr;
        delete buffer_; buffer_ = nullptr;
        return false;
    }

    output_ = new WasapiOutput(*buffer_);
    if (!output_->initialize()) {
        last_error_ = "初始化输出模块失败";
        delete output_; output_ = nullptr;
        delete capture_; capture_ = nullptr;
        delete buffer_; buffer_ = nullptr;
        return false;
    }

    output_->setFormat(capture_->getSampleRate(), capture_->getChannels());
    capture_->start();
    Sleep(50);

    int ok = 0;
    std::vector<std::string> errors;
    for (auto& dc : devices) {
        if (dc.enabled) {
            if (output_->startDevice(dc.id, dc.hpf_hz, dc.lpf_hz, 100)) {
                ++ok;
            } else {
                errors.push_back(DeviceLabel(dc.name, dc.id) + "：" + output_->getLastError());
            }
        }
    }
    if (!errors.empty()) {
        last_error_ = errors.front();
    }
    if (ok == 0) {
        if (last_error_.empty()) last_error_ = "没有可用的输出设备启动成功";
        capture_->stop();
        delete output_; output_ = nullptr;
        delete capture_; capture_ = nullptr;
        delete buffer_; buffer_ = nullptr;
        return false;
    }
    running_ = true;
    configs_ = devices;
    return true;
}

void AudioEngine::stop() {
    stopRuntime(true);
}

RuntimeUpdateResult AudioEngine::updateDevice(const DeviceConfig& config) {
    RuntimeUpdateResult result;
    auto it = findConfig(config.id);
    bool wasEnabled = it != configs_.end() && it->enabled;

    if (!running_ || !output_) {
        if (it != configs_.end()) {
            it->enabled = config.enabled;
            it->hpf_hz = config.hpf_hz;
            it->lpf_hz = config.lpf_hz;
            it->volume = config.volume;
        }
        return result;
    }

    if (config.enabled) {
        if (wasEnabled && output_->isDeviceRunning(config.id)) {
            output_->updateDevice(config.id, config.hpf_hz, config.lpf_hz, 100);
        } else if (output_->startDevice(config.id, config.hpf_hz, config.lpf_hz, 100)) {
            result.device_started = true;
        } else {
            result.ok = false;
            result.error = DeviceLabel(config.name, config.id) + "：" + output_->getLastError();
        }
    } else if (wasEnabled || output_->isDeviceRunning(config.id)) {
        output_->stopDevice(config.id);
        result.device_stopped = true;
    }

    if (it != configs_.end()) {
        it->name = config.name;
        it->enabled = result.ok ? config.enabled : false;
        it->hpf_hz = config.hpf_hz;
        it->lpf_hz = config.lpf_hz;
        it->volume = config.volume;
    } else {
        DeviceConfig next = config;
        if (!result.ok) next.enabled = false;
        configs_.push_back(next);
    }
    return result;
}

bool AudioEngine::restartForDefaultDeviceChange() {
    if (!running_) return true;
    std::vector<DeviceConfig> saved = configs_;
    stopRuntime(false);
    return start(saved);
}

std::vector<DeviceInfo> AudioEngine::enumerateDevices() {
    std::vector<DeviceInfo> devices = EnumerateRenderDevices();
    if (volumeManager_) {
        for (auto& info : devices) {
            SystemVolumeState volumeState = volumeManager_->getState(info.id);
            info.volume = volumeState.volume;
            info.muted = volumeState.muted;
            volumeManager_->watchDevice(info.id);
        }
        volumeManager_->unwatchMissingDevices(devices);
    }
    return devices;
}

std::vector<DeviceConfig>::iterator AudioEngine::findConfig(const std::string& id) {
    return std::find_if(configs_.begin(), configs_.end(), [&](const DeviceConfig& c) { return c.id == id; });
}

void AudioEngine::stopRuntime(bool clearConfigs) {
    if (!running_ && !output_ && !capture_ && !buffer_) return;
    running_ = false;
    if (output_)  { output_->stopAll();  delete output_;  output_  = nullptr; }
    if (capture_) { capture_->stop();    delete capture_; capture_ = nullptr; }
    if (buffer_)  { delete buffer_; buffer_ = nullptr; }
    if (clearConfigs) configs_.clear();
}
