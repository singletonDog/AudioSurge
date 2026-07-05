#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <WebView2.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <utility>
#include "app_common.h"
#include "audio_devices.h"
#include "audio_engine.h"
#include "embedded_ui.h"
#include "resource.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

static constexpr COLORREF kWindowBgColor = RGB(18, 18, 43);
static constexpr UINT WM_APP_VOLUME_CHANGED = WM_APP + 1;
static constexpr UINT WM_APP_DEFAULT_DEVICE_CHANGED = WM_APP + 2;
static constexpr UINT WM_APP_TRAYICON = WM_APP + 3;
static constexpr UINT WM_APP_SUSPEND_WEBVIEW = WM_APP + 4;
static constexpr UINT kTrayIconId = 1;
static constexpr UINT kTrayMenuShow = 40001;
static constexpr UINT kTrayMenuExit = 40002;
static UINT g_showInstanceMsg = 0;

struct VolumeChangedEvent {
    std::string device_id;
    int volume = 100;
    bool muted = false;
};

class DefaultDeviceNotificationClient : public IMMNotificationClient {
public:
    using Callback = std::function<void()>;

    explicit DefaultDeviceNotificationClient(Callback cb) : callback_(std::move(cb)) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient)) {
            *ppv = static_cast<IMMNotificationClient*>(this);
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

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && role == eConsole && callback_) callback_();
        return S_OK;
    }

private:
    Callback callback_;
    ULONG ref_ = 1;
};

// ============================================================================
//  Minimal JSON parser
// ============================================================================

class JsonVal {
public:
    enum Type { NUL, BOOL, NUMBER, STRING, ARRAY, OBJECT };

    JsonVal() : type_(NUL) {}
    explicit JsonVal(bool v) : type_(BOOL), b_(v) {}
    explicit JsonVal(double v) : type_(NUMBER), n_(v) {}
    explicit JsonVal(const std::string& v) : type_(STRING), s_(v) {}
    explicit JsonVal(const std::vector<JsonVal>& v) : type_(ARRAY), a_(v) {}
    explicit JsonVal(const std::map<std::string, JsonVal>& v) : type_(OBJECT), o_(v) {}

    Type type() const { return type_; }
    bool isNull()   const { return type_ == NUL; }
    bool isBool()   const { return type_ == BOOL; }
    bool isNumber() const { return type_ == NUMBER; }
    bool isString() const { return type_ == STRING; }
    bool isArray()  const { return type_ == ARRAY; }
    bool isObject() const { return type_ == OBJECT; }

    bool   asBool()   const { return b_; }
    double asNumber() const { return n_; }
    int    asInt()    const { return static_cast<int>(n_); }
    const std::string& asString() const { return s_; }
    const std::vector<JsonVal>& asArray() const { return a_; }
    const std::map<std::string, JsonVal>& asObject() const { return o_; }

    size_t size() const {
        return type_ == ARRAY ? a_.size() : type_ == OBJECT ? o_.size() : 0;
    }

    const JsonVal& operator[](const std::string& key) const {
        static const JsonVal nullVal;
        auto it = o_.find(key);
        return it != o_.end() ? it->second : nullVal;
    }

    const JsonVal& operator[](size_t idx) const {
        static const JsonVal nullVal;
        return idx < a_.size() ? a_[idx] : nullVal;
    }

    static JsonVal parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }

private:
    Type type_ = NUL;
    bool b_ = false;
    double n_ = 0;
    std::string s_;
    std::vector<JsonVal> a_;
    std::map<std::string, JsonVal> o_;

    static void skipWs(const std::string& s, size_t& p) {
        while (p < s.size() && (s[p]==' '||s[p]=='\t'||s[p]=='\n'||s[p]=='\r')) ++p;
    }

    static JsonVal parseValue(const std::string& s, size_t& p) {
        skipWs(s, p);
        if (p >= s.size()) return JsonVal();
        char c = s[p];
        if (c == '"') return parseStr(s, p);
        if (c == '{') return parseObj(s, p);
        if (c == '[') return parseArr(s, p);
        if (c == 't' || c == 'f') return parseBool(s, p);
        if (c == 'n') return parseNull(s, p);
        return parseNum(s, p);
    }

    static JsonVal parseStr(const std::string& s, size_t& p) {
        ++p;
        std::string r;
        while (p < s.size() && s[p] != '"') {
            if (s[p] == '\\' && p + 1 < s.size()) {
                ++p;
                switch (s[p]) {
                    case '"':  r += '"'; break;
                    case '\\': r += '\\'; break;
                    case 'n':  r += '\n'; break;
                    case 'r':  r += '\r'; break;
                    case 't':  r += '\t'; break;
                    case '/':  r += '/'; break;
                    default:   r += s[p];
                }
            } else {
                r += s[p];
            }
            ++p;
        }
        if (p < s.size()) ++p;
        return JsonVal(r);
    }

    static JsonVal parseNum(const std::string& s, size_t& p) {
        size_t st = p;
        if (s[p] == '-') ++p;
        while (p < s.size() && isdigit((unsigned char)s[p])) ++p;
        if (p < s.size() && s[p] == '.') { ++p; while (p < s.size() && isdigit((unsigned char)s[p])) ++p; }
        if (p < s.size() && (s[p]=='e'||s[p]=='E')) { ++p; if (p<s.size()&&(s[p]=='+'||s[p]=='-')) ++p; while (p<s.size()&&isdigit((unsigned char)s[p])) ++p; }
        return JsonVal(std::stod(s.substr(st, p - st)));
    }

    static JsonVal parseBool(const std::string& s, size_t& p) {
        if (s.compare(p, 4, "true") == 0)  { p += 4; return JsonVal(true); }
        if (s.compare(p, 5, "false") == 0) { p += 5; return JsonVal(false); }
        return JsonVal();
    }

    static JsonVal parseNull(const std::string& s, size_t& p) {
        if (s.compare(p, 4, "null") == 0) { p += 4; return JsonVal(); }
        return JsonVal();
    }

    static JsonVal parseArr(const std::string& s, size_t& p) {
        ++p;
        std::vector<JsonVal> arr;
        skipWs(s, p);
        if (p < s.size() && s[p] == ']') { ++p; return JsonVal(arr); }
        while (p < s.size()) {
            arr.push_back(parseValue(s, p));
            skipWs(s, p);
            if (p < s.size() && s[p] == ',') { ++p; continue; }
            if (p < s.size() && s[p] == ']') { ++p; break; }
            break;
        }
        return JsonVal(arr);
    }

    static JsonVal parseObj(const std::string& s, size_t& p) {
        ++p;
        std::map<std::string, JsonVal> obj;
        skipWs(s, p);
        if (p < s.size() && s[p] == '}') { ++p; return JsonVal(obj); }
        while (p < s.size()) {
            skipWs(s, p);
            auto key = parseStr(s, p);
            skipWs(s, p);
            if (p < s.size() && s[p] == ':') ++p;
            skipWs(s, p);
            auto val = parseValue(s, p);
            obj[key.asString()] = val;
            skipWs(s, p);
            if (p < s.size() && s[p] == ',') { ++p; continue; }
            if (p < s.size() && s[p] == '}') { ++p; break; }
            break;
        }
        return JsonVal(obj);
    }
};

// ============================================================================
//  WebView2 COM handler implementations (MinGW-compatible)
// ============================================================================

class EnvCompletedHandler : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    using Callback = std::function<void(HRESULT, ICoreWebView2Environment*)>;
    EnvCompletedHandler(Callback cb) : cb_(std::move(cb)), ref_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override  { ULONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return r; }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment* env) override {
        cb_(hr, env);
        return S_OK;
    }
private:
    Callback cb_;
    ULONG ref_;
};

class CtrlCompletedHandler : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    using Callback = std::function<void(HRESULT, ICoreWebView2Controller*)>;
    CtrlCompletedHandler(Callback cb) : cb_(std::move(cb)), ref_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override  { ULONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return r; }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Controller* ctrl) override {
        cb_(hr, ctrl);
        return S_OK;
    }
private:
    Callback cb_;
    ULONG ref_;
};

class MsgReceivedHandler : public ICoreWebView2WebMessageReceivedEventHandler {
public:
    using Callback = std::function<void(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs*)>;
    MsgReceivedHandler(Callback cb) : cb_(std::move(cb)), ref_(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *ppv = this; AddRef(); return S_OK;
        }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override  { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override  { ULONG r = InterlockedDecrement(&ref_); if (r == 0) delete this; return r; }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        cb_(sender, args);
        return S_OK;
    }
private:
    Callback cb_;
    ULONG ref_;
};

// ============================================================================
//  Application
// ============================================================================

class App {
public:
    bool init(HINSTANCE hInst) {
        hInstance_ = hInst;

        // DPI awareness
        HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
        if (hUser32) {
            typedef BOOL(WINAPI* PFN_SetDpi)(HANDLE);
            auto fn = (PFN_SetDpi)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
            if (fn) fn((HANDLE)-4);
        }

        // Register window class
        HICON appIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APPICON));
        WNDCLASSW wc = {};
        wc.lpfnWndProc = &App::StaticWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"AudioFluxWnd";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.hIcon = appIcon;
        RegisterClassW(&wc);

        // WS_POPUP + WS_THICKFRAME: 无可见边框，但保留系统缩放能力
        RECT wr = { 0, 0, 1000, 900 };
        AdjustWindowRect(&wr, WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU, FALSE);
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int winW = wr.right - wr.left;
        int winH = wr.bottom - wr.top;
        int x = (screenW - winW) / 2;
        int y = (screenH - winH) / 2;

        hwnd_ = CreateWindowExW(0, L"AudioFluxWnd", L"AudioFlux",
            WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
            x, y, winW, winH, nullptr, nullptr, hInst, this);
        if (!hwnd_) return false;

        if (appIcon) {
            SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(appIcon));
            SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(appIcon));
        }

        engine_.setVolumeManager(&volumeManager_);
        volumeManager_.initialize([this](const std::string& deviceId, int volume, bool muted) {
            auto* event = new VolumeChangedEvent();
            event->device_id = deviceId;
            event->volume = volume;
            event->muted = muted;
            PostMessageW(hwnd_, WM_APP_VOLUME_CHANGED, 0, reinterpret_cast<LPARAM>(event));
        });
        initializeDeviceNotifications();
        enableTray();

        // 标准无边框窗口做法：保留系统阴影和 Aero Snap，但不把 DWM frame 扩进客户区
        BOOL darkMode = TRUE;
        DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
        COLORREF captionColor = kWindowBgColor;
        DwmSetWindowAttribute(hwnd_, DWMWA_CAPTION_COLOR, &captionColor, sizeof(captionColor));
        COLORREF borderColor = kWindowBgColor;
        DwmSetWindowAttribute(hwnd_, DWMWA_BORDER_COLOR, &borderColor, sizeof(borderColor));
        int cornerPreference = DWMWCP_ROUND;
        DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
        MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(hwnd_, &margins);

        SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        return initWebView();
    }

    int run() {
        ShowWindow(hwnd_, SW_SHOW);
        UpdateWindow(hwnd_);
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return (int)msg.wParam;
    }

private:
    HINSTANCE hInstance_ = nullptr;
    HWND hwnd_ = nullptr;
    ICoreWebView2* webView_ = nullptr;
    ICoreWebView2Controller* controller_ = nullptr;
    EventRegistrationToken msgToken_ = {};
    HMODULE wv2Dll_ = nullptr;
    AudioEngine engine_;
    SystemVolumeManager volumeManager_;
    IMMDeviceEnumerator* deviceEnumerator_ = nullptr;
    DefaultDeviceNotificationClient* defaultDeviceClient_ = nullptr;
    NOTIFYICONDATAW nid_ = {};
    bool trayEnabled_ = false;
    bool webViewCreating_ = false;

    void initializeDeviceNotifications() {
        if (deviceEnumerator_) return;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&deviceEnumerator_));
        if (FAILED(hr) || !deviceEnumerator_) return;
        defaultDeviceClient_ = new DefaultDeviceNotificationClient([this]() {
            PostMessageW(hwnd_, WM_APP_DEFAULT_DEVICE_CHANGED, 0, 0);
        });
        if (FAILED(deviceEnumerator_->RegisterEndpointNotificationCallback(defaultDeviceClient_))) {
            defaultDeviceClient_->Release();
            defaultDeviceClient_ = nullptr;
        }
    }

    void enableTray() {
        if (trayEnabled_) return;
        nid_ = {};
        nid_.cbSize = sizeof(NOTIFYICONDATAW);
        nid_.hWnd = hwnd_;
        nid_.uID = kTrayIconId;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = WM_APP_TRAYICON;
        nid_.hIcon = LoadIconW(hInstance_, MAKEINTRESOURCEW(IDI_APPICON));
        wcscpy_s(nid_.szTip, L"AudioFlux");
        if (Shell_NotifyIconW(NIM_ADD, &nid_)) {
            trayEnabled_ = true;
        }
    }

    void disableTray() {
        if (!trayEnabled_) return;
        Shell_NotifyIconW(NIM_DELETE, &nid_);
        trayEnabled_ = false;
    }

    void restoreFromTray() {
        resumeWebView();
        ShowWindow(hwnd_, SW_SHOW);
        ShowWindow(hwnd_, SW_RESTORE);
        SetForegroundWindow(hwnd_);
        if (controller_) {
            RECT bounds;
            GetClientRect(hwnd_, &bounds);
            controller_->put_Bounds(bounds);
        }
    }

    void showTrayMenu() {
        POINT pt;
        GetCursorPos(&pt);
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        AppendMenuW(menu, MF_STRING, kTrayMenuShow, L"显示窗口");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kTrayMenuExit, L"退出");
        // TrackPopupMenu 需要前台窗口才能正确关闭菜单
        SetForegroundWindow(hwnd_);
        UINT cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                                  pt.x, pt.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
        if (cmd == kTrayMenuShow) {
            restoreFromTray();
        } else if (cmd == kTrayMenuExit) {
            DestroyWindow(hwnd_);
        }
    }

    bool initWebView() {
        // Load WebView2Loader.dll dynamically
        wv2Dll_ = LoadLibraryW(L"WebView2Loader.dll");
        if (!wv2Dll_) {
            MessageBoxW(hwnd_, L"WebView2Loader.dll not found.\nPlease ensure WebView2 Runtime is installed.",
                L"Error", MB_OK | MB_ICONERROR);
            return false;
        }
        return createWebViewInstance();
    }

    bool createWebViewInstance() {
        if (!wv2Dll_) return false;
        if (webViewCreating_ || webView_) return true;

        typedef HRESULT(STDMETHODCALLTYPE* PFN_CreateEnv)(
            PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
            ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
        auto pCreateEnv = (PFN_CreateEnv)GetProcAddress(wv2Dll_, "CreateCoreWebView2EnvironmentWithOptions");
        if (!pCreateEnv) {
            MessageBoxW(hwnd_, L"Failed to get CreateCoreWebView2EnvironmentWithOptions.",
                L"Error", MB_OK | MB_ICONERROR);
            return false;
        }

        webViewCreating_ = true;

        // User data directory
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring userDataDir = exePath;
        auto pos = userDataDir.find_last_of(L"\\/");
        userDataDir = userDataDir.substr(0, pos + 1) + L"WebView2Data";

        // Create environment
        auto envHandler = new EnvCompletedHandler(
            [this](HRESULT hr, ICoreWebView2Environment* env) {
                if (FAILED(hr) || !env) {
                    webViewCreating_ = false;
                    MessageBoxW(hwnd_, L"Failed to create WebView2 environment.",
                        L"Error", MB_OK | MB_ICONERROR);
                    return;
                }
                // Create controller
                env->CreateCoreWebView2Controller(hwnd_,
                    new CtrlCompletedHandler(
                        [this](HRESULT hr2, ICoreWebView2Controller* ctrl) {
                            if (FAILED(hr2) || !ctrl) {
                                webViewCreating_ = false;
                                MessageBoxW(hwnd_, L"Failed to create WebView2 controller.",
                                    L"Error", MB_OK | MB_ICONERROR);
                                return;
                            }
                            controller_ = ctrl;
                            ctrl->AddRef();

                            ICoreWebView2Controller2* controller2 = nullptr;
                            if (SUCCEEDED(ctrl->QueryInterface(IID_ICoreWebView2Controller2, reinterpret_cast<void**>(&controller2))) && controller2) {
                                COREWEBVIEW2_COLOR bg = { 0, 0, 0, 0 };
                                controller2->put_DefaultBackgroundColor(bg);
                                controller2->Release();
                            }

                            // Get ICoreWebView2
                            HRESULT hr3 = ctrl->get_CoreWebView2(&webView_);
                            if (FAILED(hr3) || !webView_) {
                                webViewCreating_ = false;
                                MessageBoxW(hwnd_, L"Failed to get CoreWebView2.",
                                    L"Error", MB_OK | MB_ICONERROR);
                                return;
                            }

                            // Set bounds
                            RECT bounds;
                            GetClientRect(hwnd_, &bounds);
                            controller_->put_Bounds(bounds);

                            // Register message handler
                            auto msgHandler = new MsgReceivedHandler(
                                [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) {
                                    onWebMessage(args);
                                });
                            webView_->add_WebMessageReceived(msgHandler, &msgToken_);
                            msgHandler->Release();

                            // 启用 DevTools 和消息
                            ICoreWebView2Settings* settings = nullptr;
                            if (SUCCEEDED(webView_->get_Settings(&settings))) {
                                settings->put_IsScriptEnabled(TRUE);
                                settings->put_AreDefaultScriptDialogsEnabled(TRUE);
                                settings->put_IsWebMessageEnabled(TRUE);
                                settings->put_AreDevToolsEnabled(TRUE);
                                settings->Release();
                            }

                            // Navigate to embedded HTML
                            std::wstring htmlW = Utf8ToWide(kHtmlContent);
                            webView_->NavigateToString(htmlW.c_str());
                            webViewCreating_ = false;
                        }));
            });

        HRESULT hr = pCreateEnv(nullptr, userDataDir.c_str(), nullptr, envHandler);
        if (FAILED(hr)) {
            webViewCreating_ = false;
            return false;
        }
        return true;
    }

    // 释放 WebView2（含浏览器子进程），保留已加载的 loader DLL
    void suspendWebView() {
        if (webView_ && msgToken_.value != 0) {
            webView_->remove_WebMessageReceived(msgToken_);
            msgToken_ = {};
        }
        if (controller_) {
            controller_->Close();
            controller_->Release();
            controller_ = nullptr;
        }
        if (webView_) {
            webView_->Release();
            webView_ = nullptr;
        }
    }

    // 从托盘恢复时重建 WebView2
    void resumeWebView() {
        if (webView_ || webViewCreating_) return;
        createWebViewInstance();
    }

    void onWebMessage(ICoreWebView2WebMessageReceivedEventArgs* args) {
        LPWSTR rawMsg = nullptr;
        HRESULT hr = args->get_WebMessageAsJson(&rawMsg);
        if (FAILED(hr) || !rawMsg) {
            return;
        }
        std::string json = WideToUtf8(rawMsg);
        CoTaskMemFree(rawMsg);

        // get_WebMessageAsJson 返回 JSON 字符串。
        // JS 调用 postMessage(obj) 时，WebView2 自动将对象序列化为 JSON。
        // 如果 JS 调用 postMessage(JSON.stringify(obj))，则外层会多一层引号。
        JsonVal outer = JsonVal::parse(json);
        std::string innerJson;
        if (outer.isString()) {
            // JS 传了 JSON.stringify(obj)，需要二次解析
            innerJson = outer.asString();
        } else if (outer.isObject()) {
            // JS 直接传了对象，这就是最终结果
            innerJson = json;
        } else {
            return;
        }

        JsonVal msg = JsonVal::parse(innerJson);
        if (!msg.isObject()) return;

        std::string type = msg["type"].asString();

        if (type == "refresh_devices") {
            auto devs = engine_.enumerateDevices();
            sendDeviceList(devs, msg["silent"].asBool());
            // WebView 可能是从托盘恢复后重建的，同步运行状态与托盘开关
            sendStatus(engine_.isRunning());
            sendTrayState(trayEnabled_);
        }
        else if (type == "start") {
            std::vector<DeviceConfig> configs;
            std::string defaultDeviceId = GetDefaultRenderDeviceId();
            auto& arr = msg["devices"].asArray();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto& d = arr[i];
                DeviceConfig dc;
                dc.id = d["id"].asString();
                dc.name = d["name"].asString();
                dc.enabled = dc.id != defaultDeviceId && d["enabled"].asBool();
                dc.hpf_hz = dc.id == defaultDeviceId ? 0.0f : (float)d["hpf"].asNumber();
                dc.lpf_hz = dc.id == defaultDeviceId ? 0.0f : (float)d["lpf"].asNumber();
                dc.volume = d["volume"].asInt();
                configs.push_back(dc);
            }
            if (!engine_.start(configs)) {
                std::string message = engine_.getLastError().empty() ? "启动失败，请检查音频设备" : engine_.getLastError();
                sendError(message);
            } else {
                sendStatus(true);
            }
        }
        else if (type == "stop") {
            engine_.stop();
            sendStatus(false);
        }
        else if (type == "device_update") {
            DeviceConfig dc;
            dc.id = msg["deviceId"].asString();
            dc.name = msg["name"].asString();
            dc.enabled = msg["enabled"].asBool();
            dc.hpf_hz = (float)msg["hpf"].asNumber();
            dc.lpf_hz = (float)msg["lpf"].asNumber();
            dc.volume = msg["volume"].asInt();
            bool muted = msg["muted"].asBool();
            if (!volumeManager_.setState(dc.id, dc.volume, muted)) {
                sendError("系统音量设置失败");
            }
            RuntimeUpdateResult result = engine_.updateDevice(dc);
            if (!result.ok) {
                sendError(result.error.empty() ? "输出设备更新失败" : result.error);
                sendDeviceEnabled(dc.id, false);
            } else if (result.device_started) {
                showInfo(DeviceLabel(dc.name, dc.id) + " 已启用");
            } else if (result.device_stopped) {
                showInfo(DeviceLabel(dc.name, dc.id) + " 已停止");
            }
        }
        else if (type == "set_default_device") {
            std::string deviceId = msg["deviceId"].asString();
            std::string name = msg["name"].asString();
            std::string error;
            if (!SetDefaultRenderDevice(deviceId, &error)) {
                sendError(error.empty() ? "切换默认输出设备失败" : error);
            } else {
                showInfo(DeviceLabel(name, deviceId) + " 已设为默认输出设备");
                auto devs = engine_.enumerateDevices();
                sendDeviceList(devs, true);
            }
        }
        else if (type == "set_tray") {
            bool enabled = msg["enabled"].asBool();
            if (enabled) {
                enableTray();
            } else {
                disableTray();
            }
            sendTrayState(trayEnabled_);
        }
        else if (type == "window_control") {
            std::string action = msg["action"].asString();
            if (action == "minimize") {
                ShowWindow(hwnd_, SW_MINIMIZE);
            } else if (action == "toggle_max") {
                if (IsZoomed(hwnd_)) {
                    ShowWindow(hwnd_, SW_RESTORE);
                } else {
                    ShowWindow(hwnd_, SW_MAXIMIZE);
                }
            } else if (action == "close") {
                if (trayEnabled_) {
                    ShowWindow(hwnd_, SW_HIDE);
                    // 延迟到消息循环再销毁 WebView，避免在其消息回调内重入释放
                    PostMessageW(hwnd_, WM_APP_SUSPEND_WEBVIEW, 0, 0);
                } else {
                    SendMessageW(hwnd_, WM_CLOSE, 0, 0);
                }
            } else if (action == "drag") {
                ReleaseCapture();
                SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            // 边缘缩放：JS 检测鼠标在边缘，C++ 端发起系统缩放
            else if (action == "resize_l")  { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTLEFT, 0); }
            else if (action == "resize_r")  { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTRIGHT, 0); }
            else if (action == "resize_t")  { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTTOP, 0); }
            else if (action == "resize_b")  { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTBOTTOM, 0); }
            else if (action == "resize_tl") { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTTOPLEFT, 0); }
            else if (action == "resize_tr") { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTTOPRIGHT, 0); }
            else if (action == "resize_bl") { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTBOTTOMLEFT, 0); }
            else if (action == "resize_br") { ReleaseCapture(); SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTBOTTOMRIGHT, 0); }
        }
        else if (type == "debug") {
            // Debug messages from JS are silently ignored in production
        }
    }

    void sendDeviceList(const std::vector<DeviceInfo>& devs, bool silent = false) {
        std::string json = "{\"type\":\"device_list\",\"silent\":";
        json += silent ? "true" : "false";
        json += ",\"devices\":[";
        for (size_t i = 0; i < devs.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"id\":\"" + EscapeJson(devs[i].id) +
                    "\",\"name\":\"" + EscapeJson(devs[i].name) +
                    "\",\"isDefault\":" + (devs[i].is_default ? "true" : "false") +
                    ",\"volume\":" + std::to_string(ClampVolume(devs[i].volume)) +
                    ",\"muted\":" + (devs[i].muted ? "true" : "false") + "}";
        }
        json += "]}";

        // 用 ExecuteScript 直接调用 JS 函数（最可靠的方式）
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);

        // 也用 PostWebMessageAsJson 发送
        sendMessage(json);
    }

    void sendStatus(bool running) {
        std::string json = std::string("{\"type\":\"status\",\"running\":") + (running ? "true" : "false") + "}";
        sendMessage(json);
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);
    }
    void sendTrayState(bool enabled) {
        std::string json = std::string("{\"type\":\"tray_state\",\"enabled\":") + (enabled ? "true" : "false") + "}";
        sendMessage(json);
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);
    }

    void sendVolumeUpdate(const std::string& deviceId, int volume, bool muted) {
        std::string json = "{\"type\":\"volume_changed\",\"deviceId\":\"" + EscapeJson(deviceId) +
                           "\",\"volume\":" + std::to_string(ClampVolume(volume)) +
                           ",\"muted\":" + (muted ? "true" : "false") + "}";
        sendMessage(json);
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);
    }

    void sendDeviceEnabled(const std::string& deviceId, bool enabled) {
        std::string json = "{\"type\":\"device_enabled\",\"deviceId\":\"" + EscapeJson(deviceId) +
                           "\",\"enabled\":" + (enabled ? "true" : "false") + "}";
        sendMessage(json);
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);
    }

    void sendError(const std::string& message) {
        std::string json = "{\"type\":\"error\",\"message\":\"" + EscapeJson(message) + "\"}";
        sendMessage(json);
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);
    }

    void showInfo(const std::string& message) {
        std::string json = "{\"type\":\"info\",\"message\":\"" + EscapeJson(message) + "\"}";
        sendMessage(json);
        std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                            + Utf8ToWide(json) + L");}";
        executeScript(script);
    }

    void sendMessage(const std::string& json) {
        if (!webView_) return;
        std::wstring wjson = Utf8ToWide(json);
        webView_->PostWebMessageAsJson(wjson.c_str());
    }

    // 通过 ExecuteScript 调用 JS（必须在 UI 线程）
    void executeScript(const std::wstring& script) {
        if (!webView_) return;
        webView_->ExecuteScript(script.c_str(), nullptr);
    }

    void cleanup() {
        disableTray();
        if (deviceEnumerator_ && defaultDeviceClient_) {
            deviceEnumerator_->UnregisterEndpointNotificationCallback(defaultDeviceClient_);
        }
        if (defaultDeviceClient_) {
            defaultDeviceClient_->Release();
            defaultDeviceClient_ = nullptr;
        }
        if (deviceEnumerator_) {
            deviceEnumerator_->Release();
            deviceEnumerator_ = nullptr;
        }
        volumeManager_.shutdown();
        engine_.stop();
        suspendWebView();
        if (wv2Dll_) {
            FreeLibrary(wv2Dll_);
            wv2Dll_ = nullptr;
        }
    }

    static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        App* app = nullptr;
        if (msg == WM_NCCREATE) {
            auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            app = reinterpret_cast<App*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        } else {
            app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        // 另一个实例请求唤起窗口
        if (msg == g_showInstanceMsg && g_showInstanceMsg != 0 && app) {
            app->restoreFromTray();
            return 0;
        }

        switch (msg) {
        case WM_NCCALCSIZE: {
            if (wParam == TRUE) return 0;
            break;
        }

        case WM_GETMINMAXINFO: {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
            mmi->ptMinTrackSize.x = 700;
            mmi->ptMinTrackSize.y = 500;
            return 0;
        }

        case WM_ERASEBKGND: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            HBRUSH brush = CreateSolidBrush(RGB(18, 18, 43));
            FillRect(reinterpret_cast<HDC>(wParam), &rc, brush);
            DeleteObject(brush);
            return 1;
        }

        case WM_APP_VOLUME_CHANGED: {
            if (!app) break;
            auto* event = reinterpret_cast<VolumeChangedEvent*>(lParam);
            if (event) {
                app->sendVolumeUpdate(event->device_id, event->volume, event->muted);
                delete event;
            }
            return 0;
        }

        case WM_APP_TRAYICON: {
            if (!app) break;
            if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
                app->restoreFromTray();
            } else if (LOWORD(lParam) == WM_RBUTTONUP) {
                app->showTrayMenu();
            }
            return 0;
        }

        case WM_APP_SUSPEND_WEBVIEW: {
            if (!app) break;
            // 仅当窗口仍处于隐藏（在托盘）时才释放，避免刚恢复又被销毁
            if (!IsWindowVisible(hwnd)) {
                app->suspendWebView();
            }
            return 0;
        }

        case WM_APP_DEFAULT_DEVICE_CHANGED: {
            if (!app) break;
            bool wasRunning = app->engine_.isRunning();
            if (wasRunning && !app->engine_.restartForDefaultDeviceChange()) {
                std::string message = app->engine_.getLastError().empty() ? "默认捕获设备切换失败" : app->engine_.getLastError();
                app->sendError(message);
            } else if (wasRunning) {
                app->sendStatus(true);
                app->showInfo("默认捕获设备已跟随系统切换");
            }
            auto devs = app->engine_.enumerateDevices();
            app->sendDeviceList(devs, true);
            return 0;
        }

        case WM_SIZE: {
            if (!app) break;
            if (wParam == SIZE_MINIMIZED) return 0;
            if (app->controller_) {
                RECT bounds;
                GetClientRect(hwnd, &bounds);
                if (bounds.right > 0 && bounds.bottom > 0) {
                    app->controller_->put_Bounds(bounds);
                }
            }
            // 通知 JS 窗口状态变化（最大化/还原）
            bool maximized = IsZoomed(hwnd);
            std::string json = std::string("{\"type\":\"window_state\",\"maximized\":")
                             + (maximized ? "true" : "false") + "}";
            app->sendMessage(json);
            std::wstring script = L"if(typeof handleNativeMessage==='function'){handleNativeMessage("
                                + Utf8ToWide(json) + L");}";
            app->executeScript(script);
            return 0;
        }

        case WM_DESTROY:
            if (app) app->cleanup();
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
};

// ============================================================================
//  Entry point
// ============================================================================

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    // 注册全局唤起消息，用于让已运行实例显示到前台
    g_showInstanceMsg = RegisterWindowMessageW(L"AudioFluxShowInstanceMsg");

    // 单实例：命名 Mutex 检测是否已有实例在运行
    HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, L"AudioFlux_SingleInstance_Mutex");
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        // 已有实例：广播唤起消息让其显示，然后退出本进程
        if (g_showInstanceMsg) {
            PostMessageW(HWND_BROADCAST, g_showInstanceMsg, 0, 0);
        }
        CloseHandle(instanceMutex);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    App app;
    if (!app.init(hInstance)) {
        CoUninitialize();
        if (instanceMutex) CloseHandle(instanceMutex);
        return 1;
    }

    int ret = app.run();
    CoUninitialize();
    if (instanceMutex) CloseHandle(instanceMutex);
    return ret;
}



