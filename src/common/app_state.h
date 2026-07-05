#pragma once

#include <string>
#include <map>

struct DeviceState {
    bool enabled = true;
    int hpf = 0;
    int lpf = 0;
};

// 应用本地状态持久化：设备选中/滤波器状态 + 全局设置。
// 数据以 JSON 存放在 exe 同目录（与 WebView2Data 相邻）。
class AppState {
public:
    void load();
    void save() const;

    bool trayEnabled() const { return tray_enabled_; }
    void setTrayEnabled(bool enabled);

    bool hasDevice(const std::string& id) const;
    DeviceState getDevice(const std::string& id) const;
    void setDevice(const std::string& id, const DeviceState& state);

private:
    std::wstring filePath() const;

    bool tray_enabled_ = true;
    std::map<std::string, DeviceState> devices_;
    bool loaded_ = false;
};
