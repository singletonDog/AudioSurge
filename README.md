# AudioSurge

一个轻量的 Windows 音频路由工具，支持将系统音频同时输出到多个设备，并为每个设备提供独立的音量控制和频段滤波器。

## 功能特性

- **多路输出**：将系统音频同时发送到多个输出设备（耳机、音箱等）
- **每设备独立控制**：
  - 系统音量和静音控制
  - 高通滤波器 (HPF)：过滤低频噪声
  - 低通滤波器 (LPF)：过滤高频噪声
- **频段双拖柄**：在单个控件中直观调整通过频段（20Hz ~ 20kHz）
- **系统托盘**：支持最小化到托盘，后台持续运行
- **开机自启**：支持静默后台启动
- **单实例**：再次启动时唤回已运行实例
- **深色主题**：采用深紫色主题的现代 UI

## 技术栈

- **框架**：Win32 API + WebView2
- **语言**：C++17
- **音频**：WASAPI（Loopback 捕获 + Render Client）
- **构建**：CMake + Ninja
- **编译器**：MinGW 64-bit

## 构建说明

### 依赖

- CMake 3.15+
- MinGW 64-bit（推荐 GCC 14+）
- WebView2 Runtime（已内置 `WebView2Loader.dll`）

### 构建命令

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

构建产物位于 `build/AudioSurge.exe`。

## 使用方法

1. **启动应用**：运行 `AudioSurge.exe`
2. **选择设备**：勾选要输出音频的设备
3. **调整参数**：
   - 使用音量滑块调整系统音量
   - 使用频段控件调整 HPF/LPF 滤波器
4. **启动路由**：点击"启动"按钮开始音频转发
5. **设为默认设备**：双击设备图标可将其设为系统默认输出设备（用于切换捕获源）

### 频段控件操作

- **拖动**：调整 HPF（左拖柄）和 LPF（右拖柄）
- **方向键**：精调 ±1Hz
- **Shift+方向键**：精调 ±100Hz
- **拉到端点**：对应侧滤波器关闭

## 高级功能

- **后台运行**：点击后台按钮隐藏窗口，音频继续转发
- **开机自启**：在设置面板开启，下次登录自动静默启动
- **系统托盘**：关闭窗口后最小化到托盘，右键菜单可恢复或退出

## 项目结构

```
d:\Project\audio
├── CMakeLists.txt          # 构建配置
├── assets/                 # 图标资源
│   ├── audiosurge.ico
│   └── audiosurge.png
├── tools/
│   └── embed_logo.py       # 图标嵌入脚本
├── include/                # WebView2 头文件
└── src/
    ├── gui_main.cpp        # 主窗口和应用协调
    ├── app.rc              # 资源文件
    ├── resource.h          # 资源定义
    ├── audio/              # 音频模块
    │   ├── audio_engine.h/cpp      # 音频引擎
    │   ├── audio_devices.h/cpp     # 设备枚举
    │   ├── wasapi_capture.h/cpp    # WASAPI 捕获
    │   ├── wasapi_output.h/cpp     # WASAPI 输出
    │   └── ring_buffer.h           # 环形缓冲区
    ├── system/             # 系统模块
    │   └── system_volume.h/cpp     # 系统音量管理
    └── common/             # 公共模块
        ├── app_common.h            # 公共工具
        ├── app_state.h/cpp         # 状态持久化
        └── embedded_ui.h           # 内嵌 UI（HTML/CSS/JS）
```

## 音频流程

```
Windows 默认输出设备 Loopback
    → WasapiCapture（捕获）
    → SharedAudioBuffer（环形缓冲区）
    → WasapiOutput（多设备渲染）
        → HPF/LPF 滤波
        → 音量控制
        → 输出到选中设备
```

## 许可证

MIT License