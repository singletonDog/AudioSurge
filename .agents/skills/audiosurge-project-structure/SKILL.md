---
name: "audiosurge-project-structure"
description: "说明 AudioSurge 最新项目架构和关键文件。处理本仓库任务、询问结构、模块职责或功能修改位置时调用。"
---

# AudioSurge 项目结构

当用户询问本仓库的架构、代码位置、模块连接方式，或需要判断某个 AudioSurge 功能应该在哪里修改时，使用此 Skill，避免重复探索项目布局。

## 项目概览

AudioSurge 是一个 Windows C++17 桌面音频应用。

它使用：

- Win32 作为原生桌面窗口宿主。
- WebView2 作为 GUI 渲染器。
- `src/common/embedded_ui.h` 中的内嵌 HTML/CSS/JS 作为界面。
- WASAPI Loopback 捕获当前系统默认输出设备的混音。
- 默认捕获源始终跟随 Windows 当前默认输出设备。
- 提供手动设置系统默认输出设备的入口（双击设备图标），从而间接切换捕获源。
- WASAPI Render Client 将捕获到的音频输出到多个选中的输出设备。
- 一个无锁共享 float 音频环形缓冲区连接捕获线程和渲染线程。
- `SystemVolumeManager` 同步每个输出设备的系统音量和静音状态。
- `AppState` 将设备选中/滤波器状态和全局设置持久化到 exe 同目录 JSON 文件。
- 系统托盘图标：关闭窗口可最小化到托盘，托盘菜单可显示/退出。
- 开机自启动（HKCU Run 键 + `--autostart` 静默后台运行）。
- 命名 Mutex 单实例：再次启动时唤回已运行实例。
- 隐藏到托盘/后台时挂起（释放）WebView2 以降低内存占用，恢复时重建。
- 内嵌应用图标资源（`src/app.rc` + `assets/audiosurge.ico`）。
- CMake 构建 GUI 可执行程序（默认 Release，含体积优化与 DLL 拷贝）。

高层数据流：

```text
Windows 当前默认渲染设备 Loopback
  -> WasapiCapture
  -> SharedAudioBuffer
  -> WasapiOutput 渲染流
  -> 选中的输出设备

WebView2 UI
  -> JS postMessage
  -> App::onWebMessage
  -> AudioEngine / SystemVolumeManager / WasapiOutput
  -> 原生 JSON 消息回传给 JS
```

## 当前仓库目录

```text
d:\Project\audio
├─ CMakeLists.txt
├─ assets/
│  ├─ audiosurge.ico
│  └─ audiosurge.png
├─ tools/
│  └─ embed_logo.py
├─ include/
│  ├─ WebView2.h
│  ├─ WebView2EnvironmentOptions.h
│  └─ WebView2Loader.dll
└─ src/
   ├─ gui_main.cpp
   ├─ app.rc
   ├─ resource.h
   ├─ audio/
   │  ├─ audio_devices.h
   │  ├─ audio_devices.cpp
   │  ├─ audio_engine.h
   │  ├─ audio_engine.cpp
   │  ├─ ring_buffer.h
   │  ├─ wasapi_capture.h
   │  ├─ wasapi_capture.cpp
   │  ├─ wasapi_output.h
   │  └─ wasapi_output.cpp
   ├─ common/
   │  ├─ app_common.h
   │  ├─ app_state.h
   │  ├─ app_state.cpp
   │  └─ embedded_ui.h
   └─ system/
      ├─ system_volume.h
      └─ system_volume.cpp
```

生成目录通常为：

```text
build/
```

不要把生成目录当作源码事实来源。

注意：旧的 `main.cpp` 控制台入口已经删除，当前只有 GUI 目标。

## 构建配置

主构建文件：`CMakeLists.txt`。

当前可执行目标：

```cmake
add_executable(AudioSurge WIN32
    src/gui_main.cpp
    src/audio/audio_devices.cpp
    src/audio/audio_engine.cpp
    src/system/system_volume.cpp
    src/audio/wasapi_capture.cpp
    src/audio/wasapi_output.cpp
    src/common/app_state.cpp
    src/app.rc
)

target_include_directories(AudioSurge PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/audio
    ${CMAKE_SOURCE_DIR}/src/system
    ${CMAKE_SOURCE_DIR}/src/common
)
```

重要配置：

- C++ 标准：C++17。
- 默认构建类型：未指定时强制为 `Release`，便于体积/性能优化。
- GUI 入口：`src/gui_main.cpp` 中的 `wWinMain`。
- WebView2 头文件来自 `include/`。
- 资源编译：`src/app.rc` 内嵌 `assets/audiosurge.ico`（`IDI_APPICON`）；通过 `set_source_files_properties(... OBJECT_DEPENDS ...)` 让图标变化触发重编。
- 构建后 POST_BUILD 自动把 `include/WebView2Loader.dll` 拷到可执行文件输出目录，便于直接分发运行。
- 链接的 Windows 库：
  - `ole32`
  - `oleaut32`
  - `uuid`
  - `ksuser`
  - `user32`
  - `gdi32`
  - `dwmapi`
  - `advapi32`（注册表开机自启动）
- 编译定义：
  - `_WIN32_WINNT=0x0A00`
  - `UNICODE`
  - `_UNICODE`
- MinGW 构建时使用 `-municode`；Release 下额外启用体积优化（`-Os -ffunction-sections -fdata-sections` + `-Wl,--gc-sections -s`）。

推荐验证命令：

```powershell
cmake --build build
```

如果 `build/` 不存在：

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## 当前模块职责

### `src/gui_main.cpp`

这是主 GUI 应用协调文件。第二批架构清理后，它不再包含大块 UI 字符串、音频引擎实现、系统音量实现或设备枚举实现。

主要职责：

- 创建 Win32 窗口和自定义无边框窗口行为。
- 设置 DWM 深色模式、边框颜色、标题颜色和圆角偏好。
- 加载应用图标（`IDI_APPICON`）并设置窗口图标。
- 动态加载并初始化 WebView2。
- 处理 JS 与原生 C++ 的消息桥接。
- 协调 `AudioEngine`、`SystemVolumeManager`、`AppState` 和 WebView2 UI。
- 加载/保存本地持久化状态（设备启用/HPF/LPF、托盘与自启开关）。
- 注册默认输出设备变化监听。
- 管理系统托盘图标（`enableTray`/`disableTray`/`showTrayMenu`/`restoreFromTray`）。
- 管理开机自启动注册表项（`IsAutoStartEnabled`/`SetAutoStart`/`BuildAutoStartCommand`）。
- 命名 Mutex 单实例检测 + 广播唤起消息（`AudioSurgeShowInstanceMsg`）。
- 隐藏到托盘/后台时挂起 WebView2（`suspendWebView`），恢复时重建（`resumeWebView`）。
- `--autostart` 静默模式：不建 WebView，直接按保存配置后台转发（`autoStartForwarding`）。
- 使用 `WM_APP_VOLUME_CHANGED` 将系统音量回调派发回 UI 线程。
- 使用 `WM_APP_DEFAULT_DEVICE_CHANGED` 处理系统默认输出设备变化。
- 使用 `WM_APP_TRAYICON` 处理托盘图标鼠标事件。
- 使用 `WM_APP_SUSPEND_WEBVIEW` 延迟释放 WebView2，避免消息回调内重入。
- 处理窗口拖动、缩放、最小化、最大化、关闭。

重要类型/区域：

- `VolumeChangedEvent`：跨线程传递系统音量变化。
- `DefaultDeviceNotificationClient`：监听默认输出设备变化。
- `JsonVal`：用于解析 JS 消息的极简 JSON 解析器。
- `EnvCompletedHandler`、`CtrlCompletedHandler`、`MsgReceivedHandler`：WebView2 COM 回调辅助类。
- `App`：原生应用协调器（持有 `AppState appState_`、托盘 `NOTIFYICONDATAW`、`silent_` 等状态）。
- 自启动辅助：`kAutoStartRunKey`、`kAutoStartValueName`、`BuildAutoStartCommand`、`IsAutoStartEnabled`、`SetAutoStart`。
- 托盘常量：`kTrayIconId`、`kTrayMenuShow`、`kTrayMenuExit`。
- `g_showInstanceMsg`：单实例唤起用的注册窗口消息。
- `wWinMain`：GUI 入口点（含单实例 Mutex 与 `--autostart` 判断）。

JS 到 C++ 消息流：

```text
JS sendMsg({ type: ... })
  -> WebView2 WebMessageReceived
  -> App::onWebMessage
  -> AudioEngine / SystemVolumeManager / 窗口控制
```

C++ 到 JS 消息流：

```text
App::sendMessage(json)
App::executeScript("handleNativeMessage(...)")
  -> window.handleNativeMessage(data)
```

重要 JS 消息类型：

- `refresh_devices`：手动刷新输出设备列表（并同步运行状态、托盘、自启开关）。
- `start`：启动捕获和选中的输出设备（并把设备状态写入 `AppState` 持久化）。
- `stop`：停止音频运行时。
- `device_update`：更新某个设备的启用状态、HPF/LPF、系统音量/静音（并持久化）。
- `set_default_device`：将某设备设为 Windows 默认输出设备（双击设备图标触发），间接切换捕获源。
- `set_tray`：开启/关闭系统托盘功能（并持久化）。
- `set_autostart`：开启/关闭开机自启动（写 HKCU Run 键，并持久化）。
- `window_control`：最小化、最大化(`toggle_max`)、关闭、后台(`background`)、退出(`exit`)、拖动、边缘缩放(`resize_*`)。
- `debug`：JS 调试消息，生产环境静默忽略。

重要原生到 JS 的消息类型：

- `device_list`：活跃渲染设备列表，包含 id/name/isDefault/volume/muted，及持久化的 savedEnabled/savedHpf/savedLpf；顶层带 silent/running。
- `status`：运行时启动/停止状态。
- `window_state`：最大化/还原状态。
- `volume_changed`：外部系统端点音量或静音变化。
- `device_enabled`：原生侧修正某设备启用状态。
- `tray_state`：当前托盘开关状态。
- `autostart_state`：当前开机自启动状态。
- `info`：普通提示。
- `error`：toast 错误提示。

### `src/common/embedded_ui.h`

这个文件存放原来在 `src/gui_main.cpp` 中的内嵌 HTML/CSS/JS：

```cpp
static const char* kHtmlContent = R"html(... )html";
```

主要职责：

- 页面布局和视觉样式。
- 设备卡片渲染。
- 启动/停止按钮。
- 手动刷新设备按钮。
- 音量滑块、静音图标。
- 单个“通过频段”双拖柄控件（合并原 HPF/LPF 两条滑块）。
- 双击设备图标将其设为默认输出设备。
- 默认捕获源风险提示。
- 设置面板（齿轮按钮）：托盘开关、开机自启开关。
- 后台运行按钮（隐藏窗口并释放 WebView）。
- 从保存状态（savedEnabled/savedHpf/savedLpf）恢复设备选中与滤波器。
- JS 到原生的 `sendMsg` 协议。
- 原生到 JS 的 `handleNativeMessage` 协议。

修改 UI、布局、CSS、交互文案时，主要改这里。

重点查找：

- `renderDevices(list)`：设备卡片 DOM 结构（含频段双拖柄）；读取 `savedEnabled`/`savedHpf`/`savedLpf` 恢复状态。
- `getDevState(id)`：JS 状态序列化，从拖柄 `data-hz` 派生 `hpf`/`lpf`。
- `sendUpdate(id)` / `sendUpdateDebounced(id)`：JS 到原生的设备更新。
- `refreshDevices(silent)`：手动刷新设备。
- `posToFreq` / `freqToPos`：视觉位置与频率的对数互换。
- `setBandHz(card, input, isLow, newHz)`：写入拖柄频率并做交叉/边界约束。
- `updateBand(card)`：更新填充条、拖柄标签、频段状态文本。
- `applyNativeVolume(id, volume, muted)`：应用系统音量变化。
- `applyDeviceEnabled(id, enabled)`：应用原生侧设备启用状态修正。
- `applyTrayState(enabled)` / `applyAutoStartState(enabled)`：同步设置面板开关状态。
- 设置面板元素：`settingsBtn` / `settingsPanel` / 托盘开关 / 自启开关；`set_tray` / `set_autostart` 消息发送。
- 后台按钮：发送 `{type:'window_control', action:'background'}`。
- `handleNativeMessage(data)`：原生消息分发（含 `tray_state`/`autostart_state`）。

#### 频段双拖柄（HPF/LPF 合并控件）

- 两条独立的 HPF/LPF 滑块已合并为单个“通过频段”控件：左拖柄=低频边界(HPF)，右拖柄=高频边界(LPF)，中间高亮段为通过频段。
- 采用两个重叠的 `<input type=range>`（`.band-low` / `.band-high`），配合 `.band-track` / `.band-fill` 显示。
- 统一 20Hz~20000Hz 对数轴。视觉位置用 `BAND_STEPS`(1000) 分度，真实频率存于拖柄的 `data-hz` 属性，鼠标只做粗定位。
- 精调：拖柄聚焦后方向键 ±1Hz，Shift+方向键 ±100Hz（真正逐 Hz）。
- 端点即关闭：低拖柄拉到最左(20Hz)发送 `hpf=0`，高拖柄拉到最右(20000Hz)发送 `lpf=0`。
- 交叉保护：两边界保持最小频率比 `BAND_MIN_RATIO`，避免拉成陷波静音。
- 发送给原生的仍是 `hpf`/`lpf` 两个整数 Hz 字段，消息协议与后端不变。

### `src/audio/audio_engine.h` / `src/audio/audio_engine.cpp`

这个模块负责音频运行时生命周期，是 GUI 和底层 WASAPI 模块之间的协调层。

主要职责：

- 定义 `DeviceConfig`。
- 定义 `RuntimeUpdateResult`。
- 管理 `SharedAudioBuffer`、`WasapiCapture`、`WasapiOutput`。
- 启动/停止 AudioSurge 音频链路。
- 运行中启停单个输出设备。
- 更新每设备 HPF/LPF 参数。
- 默认输出设备变化时按当前配置重启音频链路，让捕获源跟随系统默认设备。
- 调用 `SystemVolumeManager` 填充设备音量/静音状态。
- 调用公共设备枚举模块获取设备列表。

主类：

```cpp
class AudioEngine
```

重要方法：

- `setVolumeManager(SystemVolumeManager*)`
- `start(const std::vector<DeviceConfig>& devices)`
- `stop()`
- `updateDevice(const DeviceConfig& config)`
- `restartForDefaultDeviceChange()`
- `enumerateDevices()`
- `isRunning()`
- `getLastError()`

运行中单设备启停逻辑在：

```cpp
AudioEngine::updateDevice
```

默认捕获设备跟随系统的重启逻辑在：

```cpp
AudioEngine::restartForDefaultDeviceChange
```

### `src/audio/audio_devices.h` / `src/audio/audio_devices.cpp`

这个模块集中处理 Windows 音频设备枚举，避免在多个文件里重复写 MMDevice 枚举逻辑。

主要职责：

- 枚举 active render devices。
- 获取默认 render device id。
- 设置系统默认 render device。
- 获取 `IMMDevice` 的设备 ID。
- 获取设备 friendly name。

重要函数：

```cpp
std::vector<DeviceInfo> EnumerateRenderDevices();
std::string GetDefaultRenderDeviceId();
bool SetDefaultRenderDevice(const std::string& deviceId, std::string* errorMessage = nullptr);
std::string GetDeviceId(IMMDevice* device);
std::string GetDeviceFriendlyName(IMMDevice* device);
```

`WasapiOutput::enumerateDevices()` 现在也复用这里：

```cpp
std::vector<DeviceInfo> WasapiOutput::enumerateDevices() {
    return EnumerateRenderDevices();
}
```

如果后续要改变设备名称读取、过滤设备、支持设备排序，应优先改这个模块。

### `src/system/system_volume.h` / `src/system/system_volume.cpp`

这个模块负责 Windows 系统端点音量和静音同步。

主要职责：

- 定义 `SystemVolumeState`。
- 读取指定设备系统音量/静音状态。
- 设置指定设备系统音量/静音状态。
- 注册每设备 `IAudioEndpointVolumeCallback`。
- 外部修改 Windows 音量/静音后，通过 callback 通知 `src/gui_main.cpp`。
- 跳过 AudioSurge 自己触发的音量事件，避免回环。
- 清理不存在设备的音量监听。

主类：

```cpp
class SystemVolumeManager
```

重要方法：

- `initialize(Callback callback)`
- `shutdown()`
- `getState(const std::string& deviceId)`
- `setState(const std::string& deviceId, int volume, bool muted)`
- `watchDevice(const std::string& deviceId)`
- `unwatchMissingDevices(const std::vector<DeviceInfo>& devices)`

系统音量同步路径：

AudioSurge UI 到 Windows：

```text
用户拖动 .vol-slider 或点击 .vol-icon
  -> JS sendUpdate
  -> device_update
  -> App::onWebMessage
  -> SystemVolumeManager::setState(deviceId, volume, muted)
```

Windows 到 AudioSurge UI：

```text
外部系统音量/静音变化
  -> IAudioEndpointVolumeCallback::OnNotify
  -> SystemVolumeManager callback
  -> PostMessageW(WM_APP_VOLUME_CHANGED)
  -> App::sendVolumeUpdate
  -> JS handleNativeMessage("volume_changed")
  -> applyNativeVolume
```

### `src/common/app_common.h`

这个文件放跨模块公共工具。

主要内容：

- `Utf8ToWide`
- `WideToUtf8`
- `EscapeJson`
- `ClampVolume`
- `ScalarToVolume`
- `VolumeToScalar`
- `IsSameGuid`
- `FormatHResult`
- `HResultMessage`
- `ComPtr<T>` 轻量 COM RAII 指针
- `kAudioSurgeVolumeContext`

`ComPtr<T>` 目前优先用于新拆分模块，例如：

- `src/audio/audio_devices.cpp`
- `src/system/system_volume.cpp`

旧的 WASAPI 捕获/输出模块仍有部分原始 COM 指针和显式 `Release`，后续可逐步迁移。

### `src/common/app_state.h` / `src/common/app_state.cpp`

这个模块负责应用本地状态持久化，数据以 JSON 存放在 exe 同目录的 `AudioSurge.state.json`（与 `WebView2Data` 相邻）。

主要职责：

- 定义 `DeviceState`（`enabled` / `hpf` / `lpf`）。
- 保存/读取每设备的启用状态与 HPF/LPF 频率。
- 保存/读取全局设置：托盘开关（`trayEnabled`）、开机自启开关（`autoStartEnabled`）。
- 内置极简 JSON 解析器（仅用于读取本模块自身写出的文件）。

主类：

```cpp
class AppState
```

重要方法：

- `load()` / `save()`
- `trayEnabled()` / `setTrayEnabled(bool)`
- `autoStartEnabled()` / `setAutoStartEnabled(bool)`
- `hasDevice(const std::string& id)`
- `getDevice(const std::string& id)` / `setDevice(const std::string& id, const DeviceState&)`
- `devices()`

使用要点：

- `src/gui_main.cpp` 在 `App::init` 中调用 `appState_.load()`，并在 `start` / `device_update` / `set_tray` / `set_autostart` 后调用 `save()`。
- `sendDeviceList` 会把持久化的设备状态作为 `savedEnabled`/`savedHpf`/`savedLpf` 附加到 `device_list` 消息，供 UI 恢复选中/滤波器状态。
- `--autostart` 静默模式下，`autoStartForwarding` 直接读取 `AppState` 里已启用的设备开始后台转发。

注意：`AppState` 内的 `autoStartEnabled` 与实际的 HKCU Run 注册表项分开存储；`set_autostart` 处理以注册表实际状态（`IsAutoStartEnabled`）为准回写。

### `src/app.rc` / `src/resource.h` / `assets/`

应用图标资源。

- `src/resource.h`：定义 `IDI_APPICON`（值 101）。
- `src/app.rc`：`IDI_APPICON ICON "../assets/audiosurge.ico"`。
- `assets/audiosurge.ico`：窗口标题栏、任务栏和托盘图标来源。
- `assets/audiosurge.png`：图标源图；`tools/embed_logo.py` 为相关的辅助脚本（生成/嵌入 logo）。

`src/gui_main.cpp` 通过 `LoadIconW(hInstance_, MAKEINTRESOURCEW(IDI_APPICON))` 加载图标用于窗口和托盘。

### `src/audio/wasapi_capture.h` / `src/audio/wasapi_capture.cpp`

这个模块实现从 Windows 当前默认输出设备进行 Loopback 捕获。

主要职责：

- 初始化 COM/MMDevice/WASAPI 捕获。
- 通过 `GetDefaultAudioEndpoint(eRender, eConsole)` 获取当前默认渲染端点。
- 激活 `IAudioClient` 和 `IAudioCaptureClient`。
- 使用 `AUDCLNT_STREAMFLAGS_LOOPBACK` 和事件回调初始化共享模式 Loopback 捕获。
- 必要时将捕获的 PCM 转换为 float。
- 将捕获到的 float 采样写入 `SharedAudioBuffer`。

主类：

```cpp
class WasapiCapture
```

重要方法：

- `initialize()`
- `start()`
- `stop()`
- `captureThread()`
- `convertIntToFloat()`
- `getSampleRate()`
- `getChannels()`

当前产品决策：不提供独立的捕获源下拉选择，捕获源始终跟随系统当前默认输出设备。用户可通过双击设备图标（`set_default_device` → `SetDefaultRenderDevice`）切换 Windows 默认输出设备来间接改变捕获源。默认设备变化由 `src/gui_main.cpp` 的通知 client 触发，最终调用 `AudioEngine::restartForDefaultDeviceChange()`。

### `src/audio/wasapi_output.h` / `src/audio/wasapi_output.cpp`

这个模块实现多设备音频输出渲染。

主要职责：

- 启动一个输出设备的 WASAPI render stream。
- 停止单个输出设备。
- 停止所有输出设备。
- 使用共享模式 WASAPI Render Client。
- 从 `SharedAudioBuffer` 读取音频。
- 应用 HPF/LPF 滤波器（单级 2 阶 Butterworth，-12dB/oct）。
- 对 HPF/LPF 频率做 C++ 层安全限制。
- 支持运行时更新每设备 HPF/LPF 参数。
- 保存最近一次错误信息，供 UI 展示。
- `enumerateDevices()` 复用 `audio_devices` 模块。

主要结构/类：

```cpp
struct DeviceInfo
class WasapiOutput
struct RenderStream
struct BiquadFilter
```

`DeviceInfo` 当前包含：

```cpp
std::string id;
std::string name;
bool is_default;
int volume;
bool muted;
```

重要方法：

- `initialize()`
- `setFormat(sample_rate, channels)`
- `enumerateDevices()`
- `startDevice(device_id, hpf_hz, lpf_hz, volume)`
- `stopDevice(device_id)`
- `isDeviceRunning(device_id)`
- `updateDevice(device_id, hpf_hz, lpf_hz, volume)`
- `createStream()`
- `renderThread()`
- `stopAll()`
- `getLastError()`

音频输出路径：

```text
renderThread
  -> 等待 WASAPI event
  -> 读取当前 hpf/lpf/volume 原子值
  -> 从 SharedAudioBuffer 读取采样
  -> 应用 HPF/LPF biquad 滤波器
  -> 应用音量/fade
  -> ReleaseBuffer 给 IAudioRenderClient
```

HPF/LPF 安全限制：

```text
0 = 关闭
非 0 时限制在 20Hz 到 sample_rate * 0.45
```

滤波器实现要点：

- `BiquadFilter` 为 RBJ cookbook 单级 2 阶 Butterworth（Q≈0.7071），Direct Form I。
- 每个流的每个声道各一个实例：`hpf_filters[MAX_CHANNELS]`、`lpf_filters[MAX_CHANNELS]`（非级联数组）。
- `renderThread` 中频率变化时只重算系数、保留滤波器历史状态，仅在从 OFF(0) 打开时 `reset()`，避免拖动时产生咔哒/爆音。

当前 UI 音量代表 Windows 系统端点音量，因此 AudioSurge 内部输出通常传入 100%，避免系统音量和内部音量双重缩放。

### `src/audio/ring_buffer.h`

这个文件实现捕获线程和输出线程之间的共享音频缓冲区。

主类：

```cpp
class SharedAudioBuffer
```

设计：

- 单写者：捕获线程。
- 多读者：每个输出设备一个渲染线程。
- 每个读者维护自己的 `read_pos`。
- 使用 atomic `write_pos_` 和 release/acquire 内存序。
- 如果读者落后太多，会跳到接近最新写入位置，避免播放过旧音频。
- 采样不足时填充静音。

这是性能敏感的实时音频代码。不要在实时路径里增加锁、堆分配、UI 调用或阻塞 I/O。

### `include/WebView2*.h`

这是随项目携带的 WebView2 SDK 头文件，供 `src/gui_main.cpp` 使用。

除非明确要升级 WebView2 SDK，否则不要编辑这些文件。

## 运行时架构

### GUI 启动

```text
wWinMain
  -> RegisterWindowMessageW("AudioSurgeShowInstanceMsg")
  -> CreateMutexW 单实例检测（已存在则广播唤起消息并退出）
  -> CoInitializeEx
  -> 解析命令行是否含 --autostart（silent）
  -> App::init(hInstance, silent)
       -> SetProcessDpiAwarenessContext
       -> RegisterClassW
       -> CreateWindowExW
       -> 设置窗口图标（IDI_APPICON）
       -> 初始化 SystemVolumeManager
       -> 注册默认设备变化监听
       -> appState_.load()
       -> 若 trayEnabled 则 enableTray()
       -> DWM 样式设置
       -> silent ? autoStartForwarding()（后台转发，不建 WebView）
                 : initWebView()
  -> App::run 消息循环（silent 时不 ShowWindow）
```

### WebView 启动

```text
App::initWebView
  -> LoadLibraryW("WebView2Loader.dll")
  -> createWebViewInstance()
       -> CreateCoreWebView2EnvironmentWithOptions
       -> CreateCoreWebView2Controller
       -> 配置透明背景和 settings
       -> NavigateToString(kHtmlContent)
```

从托盘/后台恢复时通过 `resumeWebView()` 复用 `createWebViewInstance()` 重建 WebView2。

### 音频启动

```text
JS 启动按钮
  -> message type "start"
  -> App::onWebMessage
  -> 解析选中的 DeviceConfig 列表
  -> AudioEngine::start
       -> SharedAudioBuffer
       -> WasapiCapture::initialize/start
       -> WasapiOutput::initialize/setFormat
       -> 对 enabled 设备调用 WasapiOutput::startDevice
```

### 运行中单设备启停

```text
用户勾选/取消勾选设备
  -> JS sendUpdate
  -> device_update
  -> App::onWebMessage
  -> AudioEngine::updateDevice
       -> enabled=true 且未运行：WasapiOutput::startDevice
       -> enabled=false 且正在运行：WasapiOutput::stopDevice
       -> 已运行：WasapiOutput::updateDevice
```

### 设备枚举

```text
JS refresh_devices
  -> App::onWebMessage
  -> AudioEngine::enumerateDevices
       -> EnumerateRenderDevices
       -> SystemVolumeManager 读取 volume/muted
       -> SystemVolumeManager watchDevice
       -> SystemVolumeManager unwatchMissingDevices
  -> App::sendDeviceList
  -> JS renderDevices
```

### 默认捕获设备跟随系统

```text
Windows 默认输出设备变化
  -> DefaultDeviceNotificationClient::OnDefaultDeviceChanged
  -> PostMessageW(WM_APP_DEFAULT_DEVICE_CHANGED)
  -> App 窗口消息处理
       -> AudioEngine::restartForDefaultDeviceChange
       -> AudioEngine::enumerateDevices
       -> App::sendDeviceList
       -> App::sendStatus / showInfo / sendError
```

当前策略：默认设备变化时按当前配置重启音频链路，因此切换瞬间可能有短暂停顿。

### 托盘与后台运行

```text
关闭按钮（trayEnabled=true）或后台按钮
  -> window_control (close / background)
  -> ShowWindow(SW_HIDE)
  -> PostMessageW(WM_APP_SUSPEND_WEBVIEW)
  -> App::suspendWebView（释放 WebView2 及浏览器子进程）
  -> SetProcessWorkingSetSize 回收工作集

托盘图标点击/双击
  -> WM_APP_TRAYICON
  -> App::restoreFromTray
       -> resumeWebView()（重建 WebView2）
       -> ShowWindow / SetForegroundWindow

托盘右键菜单
  -> App::showTrayMenu（显示窗口 / 退出）
```

注意：音频链路（AudioEngine）在挂起 WebView 期间继续运行，转发不中断。

### 单实例唤起

```text
再次启动 AudioSurge.exe
  -> CreateMutexW 检测到已存在实例
  -> PostMessageW(HWND_BROADCAST, AudioSurgeShowInstanceMsg)
  -> 已运行实例 WndProc 收到 g_showInstanceMsg
  -> 恢复并前置窗口
  -> 新进程退出
```

### 开机自启动（静默后台）

```text
系统登录 -> 运行 "AudioSurge.exe" --autostart
  -> wWinMain silent=true
  -> App::init 不创建 WebView
  -> autoStartForwarding()
       -> enumerateDevices
       -> 读取 AppState 中已启用设备
       -> 若有启用设备则 AudioEngine::start
```

## 常见修改位置

### 修改布局、样式或 UI 行为

编辑：

- `src/common/embedded_ui.h`

重点查找：

- `renderDevices(list)`：设备卡片 DOM 结构。
- `getDevState(id)`：JS 状态序列化。
- `sendUpdate(id)`：JS 到原生的设备更新。
- `refreshDevices(silent)`：刷新按钮行为。
- `handleNativeMessage(data)`：原生到 JS 的消息处理。

### 新增或修改 JS/原生通信协议

通常同时修改：

- `src/common/embedded_ui.h` 中 JS 发送/接收逻辑。
- `src/gui_main.cpp` 中的 `App::onWebMessage` 和原生发送辅助函数。

保持 JSON 字段名一致。原生生成字符串字段时使用 `EscapeJson`。

### 修改音频运行时生命周期

编辑：

- `src/audio/audio_engine.h`
- `src/audio/audio_engine.cpp`

例如：

- 改启动/停止流程。
- 改运行中设备启停策略。
- 改默认设备变化后的恢复方式。
- 改部分设备失败后的处理策略。

### 修改输出设备枚举、名称或默认设备判断

编辑：

- `src/audio/audio_devices.h`
- `src/audio/audio_devices.cpp`

`WasapiOutput` 和 `AudioEngine` 应复用这里，不要再新增一套 MMDevice 枚举代码。

### 修改系统音量/静音同步

编辑：

- `src/system/system_volume.h`
- `src/system/system_volume.cpp`

相关位置：

- `SystemVolumeManager::getState`
- `SystemVolumeManager::setState`
- `SystemVolumeManager::watchDevice`
- `EndpointVolumeCallback::OnNotify`
- `SystemVolumeManager::unwatchMissingDevices`

如果要改变 UI 如何响应音量变化，还要改：

- `src/gui_main.cpp` 的 `sendVolumeUpdate`
- `src/common/embedded_ui.h` 的 `applyNativeVolume`

### 增加音频 DSP 或修改滤波器

编辑：

- `src/audio/wasapi_output.cpp`
- `src/audio/wasapi_output.h`

重点位置：

- `BiquadFilter`
- `RenderStream` 状态
- `WasapiOutput::updateDevice`
- `WasapiOutput::renderThread`

不要在 `renderThread` 中加入锁、堆分配或阻塞操作。

### 修改捕获源策略

当前产品策略是不提供独立的捕获源选择，始终跟随系统默认输出设备；但已支持通过双击设备图标手动切换 Windows 默认输出设备来间接改变捕获源。

如果以后改成独立的捕获源选择，需要改：

- `src/audio/wasapi_capture.h/.cpp`：让 `WasapiCapture::initialize()` 支持指定 device id。
- `src/audio/audio_engine.h/.cpp`：保存并传递捕获源配置。
- `src/common/embedded_ui.h`：增加捕获源 UI。
- `src/audio/audio_devices.h/.cpp`：必要时提供捕获源枚举。

### 修改“设为默认输出设备”行为

编辑：

- `src/common/embedded_ui.h`：设备图标 `dblclick` 事件与 `set_default_device` 消息发送。
- `src/gui_main.cpp` 中 `onWebMessage` 的 `set_default_device` 分支。
- `src/audio/audio_devices.cpp` 中的 `SetDefaultRenderDevice`（内部使用未公开的 `IPolicyConfig` 接口设置默认端点）。

### 修改频段双拖柄（HPF/LPF 合并控件）

编辑：

- `src/common/embedded_ui.h`

重点位置：

- `posToFreq` / `freqToPos`：位置↔频率对数映射，`BAND` 频率范围、`BAND_STEPS` 分度。
- `setBandHz`：写入拖柄频率、交叉保护（`BAND_MIN_RATIO`）、端点=关闭。
- `updateBand`：填充条、拖柄标签、频段状态文本。
- `renderDevices` 中 `.band-slider` / `.band-low` / `.band-high` 结构与键盘微调事件（方向键 ±1Hz、Shift ±100Hz）。
- `getDevState`：从 `data-hz` 派生 `hpf`/`lpf`。

发送给原生的仍是 `hpf`/`lpf` 整数 Hz，后端无需改动。

### 修改默认设备变化处理

编辑：

- `src/gui_main.cpp` 中的 `DefaultDeviceNotificationClient`
- `src/gui_main.cpp` 中的 `WM_APP_DEFAULT_DEVICE_CHANGED` 分支
- `src/audio/audio_engine.cpp` 中的 `AudioEngine::restartForDefaultDeviceChange`

### 修改状态持久化（设备状态/设置）

编辑：

- `src/common/app_state.h` / `src/common/app_state.cpp`：`DeviceState`、字段读写、JSON 结构、文件路径。
- `src/gui_main.cpp`：`appState_.load()` 调用时机、`start`/`device_update`/`set_tray`/`set_autostart` 后的 `save()`、`sendDeviceList` 中 `savedEnabled`/`savedHpf`/`savedLpf` 附加逻辑。
- `src/common/embedded_ui.h`：`renderDevices` 中读取 saved* 恢复 UI 状态。

如果新增需要持久化的字段，通常要同时改 `app_state`、`sendDeviceList` 序列化和 UI 读取。

### 修改系统托盘 / 后台运行 / WebView 挂起恢复

编辑：

- `src/gui_main.cpp`：`enableTray` / `disableTray` / `showTrayMenu` / `restoreFromTray`、`suspendWebView` / `resumeWebView`、`WM_APP_TRAYICON` / `WM_APP_SUSPEND_WEBVIEW` 分支、`window_control` 的 `close`/`background`/`exit` 分支。
- `src/common/embedded_ui.h`：设置面板托盘开关、后台按钮、`set_tray` 消息与 `tray_state` 处理（`applyTrayState`）。
- 托盘常量：`kTrayIconId`、`kTrayMenuShow`、`kTrayMenuExit`。

### 修改开机自启动

编辑：

- `src/gui_main.cpp`：`kAutoStartRunKey`、`kAutoStartValueName`、`BuildAutoStartCommand`、`IsAutoStartEnabled`、`SetAutoStart`、`set_autostart` 分支、`--autostart` 判断与 `autoStartForwarding`。
- `src/common/embedded_ui.h`：设置面板自启开关、`set_autostart` 消息与 `autostart_state` 处理（`applyAutoStartState`）。
- 链接库需保留 `advapi32`（`CMakeLists.txt`）。

### 修改单实例唤起

编辑：

- `src/gui_main.cpp`：`wWinMain` 中的 `CreateMutexW`、`g_showInstanceMsg`（`RegisterWindowMessageW`）、WndProc 中 `g_showInstanceMsg` 分支。

### 修改应用图标

编辑：

- `assets/audiosurge.ico`（图标文件本体）。
- `src/app.rc` / `src/resource.h`（`IDI_APPICON` 引用）。
- `CMakeLists.txt` 中 `set_source_files_properties(... OBJECT_DEPENDS ...)`（图标变化触发重编）。

### 修改公共工具、HRESULT 或 COM RAII

编辑：

- `src/common/app_common.h`

其中：

- `FormatHResult` / `HResultMessage` 用于错误信息。
- `ComPtr<T>` 用于 COM 指针 RAII 管理。
- 字符串转换和 JSON 转义也在这里。

## 约定与限制

- 代码库使用 C++17。
- 新模块优先使用 `ComPtr<T>` 管理 COM 对象。
- 旧的 WASAPI 捕获/输出代码仍可能使用原始 COM 指针和显式 `Release`；如果修改这些路径，可逐步迁移到 RAII，但不要一次性无关大重构。
- 不要记录或暴露敏感信息。
- 未检查构建和项目约束前，不要引入外部依赖。
- 实时音频路径保持轻量：
  - 不加锁
  - 不分配堆内存
  - 不调用 UI
  - 不做阻塞 I/O
- WebView2 交互应在 UI 线程执行。
- 系统端点音量回调可能来自非 UI 线程；触碰 WebView2 前必须派发回 Win32 消息循环。
- 默认设备变化通知也通过窗口消息派发回 UI 线程。

## 验证方式

代码修改后，构建：

```powershell
cmake --build build
```

如果 `build/` 不存在：

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

GUI/音频修改的手动检查项：

- 应用能启动并渲染 WebView2 UI。
- 设备列表能显示。
- 手动刷新设备按钮正常。
- 启动/停止正常。
- 运行中勾选设备能启动该设备输出。
- 运行中取消勾选设备能停止该设备输出，且不影响其他设备。
- 输出设备能播放音频且无明显异常杂音。
- 频段双拖柄能运行时更新输出；左右拖柄可独立拖动且保持最小间隔不交叉。
- 拖柄聚焦后方向键 ±1Hz、Shift+方向键 ±100Hz 精调生效。
- 拖柄拉到两端时对应侧滤波关闭（状态显示“全频段”），拖动切换无咔哒/爆音。
- 每设备系统音量滑块与 Windows 音量一致。
- 外部 Windows 音量/静音变化能更新 AudioSurge UI。
- 系统默认输出设备变化后，AudioSurge 捕获源跟随系统切换。
- 双击设备图标能将其设为 Windows 默认输出设备。
- 默认捕获源风险提示仍正常。
- 窗口拖动、缩放、最小化、最大化、关闭仍正常。
- 设备选中/HPF/LPF 状态在重启应用后能从本地状态文件恢复。
- 设置面板托盘开关、开机自启开关能切换并持久化。
- 开启托盘后关闭窗口能最小化到托盘；托盘点击/双击/右键菜单能恢复或退出。
- 隐藏到托盘/后台后再次打开 exe 能唤回已运行实例（单实例）。
- 开机自启（`--autostart`）能静默后台按保存配置转发音频，不显示界面。
