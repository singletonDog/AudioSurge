#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <propsys.h>
#include <WebView2.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <cmath>
#include <algorithm>
#include <sstream>
#include "ring_buffer.h"
#include "wasapi_capture.h"
#include "wasapi_output.h"

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

// PKEY_Device_FriendlyName - 避免依赖 functiondiscoverykeys_devpkey.h
static const PROPERTYKEY PKEY_DEVICE_FRIENDLYNAME = {
    {0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14
};

// ============================================================================
//  Utility
// ============================================================================

static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(n - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], n);
    return ws;
}

static std::string WideToUtf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::string EscapeJson(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:   r += c;
        }
    }
    return r;
}

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
//  Embedded HTML / CSS / JS
// ============================================================================

static const char* kHtmlContent = R"html(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<style>
*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}
:root{
  --bg:#12122b;
  --bg-deep:#0a0a1e;
  --card:#1a1a3a;
  --card-bright:#252549;
  --card-border:#3a3a5e;
  --accent:#6d3bff;
  --accent2:#9d72ff;
  --accent3:#4a1fbf;
  --accent-glow:rgba(109,59,255,0.45);
  --text:#f0eef7;
  --text2:#a8a5c0;
  --text3:#6e6b87;
  --text4:#4a4865;
  --green:#4caf50;
  --red:#ff4757;
  --track:#252549;
  --track-dark:#1a1a35;
  --radius-lg:20px;
  --radius-md:14px;
  --radius-sm:8px;
}
html,body{height:100%;overflow:hidden;font-family:'Segoe UI','PingFang SC','Microsoft YaHei',system-ui,sans-serif;color:var(--text);background:transparent}
body{
  border-radius:var(--radius-lg);
  overflow:hidden;
  background:
    radial-gradient(ellipse at 15% 20%, rgba(109,59,255,0.18) 0%, transparent 50%),
    radial-gradient(ellipse at 85% 80%, rgba(157,114,255,0.12) 0%, transparent 45%),
    linear-gradient(135deg, #12122b 0%, #1a1a3e 50%, #15152e 100%);
  background-attachment:fixed;
}
body::before{
  content:'';position:fixed;inset:0;pointer-events:none;
  border-radius:inherit;
  background-image:
    linear-gradient(120deg, rgba(109,59,255,0.04) 0%, transparent 40%),
    linear-gradient(240deg, rgba(157,114,255,0.03) 0%, transparent 40%);
  background-size:100% 100%;
}
.app{display:flex;flex-direction:column;height:100vh;position:relative;z-index:1}

/* Custom Title Bar - 40px high, matches WM_NCHITTEST drag area */
.title-bar{height:40px;flex-shrink:0;display:flex;align-items:center;padding:0 20px;gap:12px;position:relative;user-select:none;
  background:linear-gradient(180deg, rgba(30,25,60,0.85) 0%, rgba(20,18,45,0.85) 100%);
  border-bottom:1px solid rgba(109,59,255,0.25);
}
.title-logo{width:24px;height:24px;display:flex;align-items:center;justify-content:center;color:var(--accent2);font-size:18px;filter:drop-shadow(0 0 6px rgba(109,59,255,0.6))}
.title-text{font-size:15px;font-weight:700;background:linear-gradient(135deg,#d0c2ff 0%, #a080ff 60%, var(--accent2) 100%);-webkit-background-clip:text;-webkit-text-fill-color:transparent;letter-spacing:0.3px}
.title-spacer{flex:1}
/* Window Control Buttons - right 140px area matches WM_NCHITTEST BUTTON_RIGHT_MARGIN */
.win-ctrl{display:flex;align-items:center;gap:2px;height:40px;-webkit-app-region:no-drag}
.win-btn{width:44px;height:30px;border:none;background:transparent;color:#b0acd0;font-size:12px;cursor:pointer;border-radius:6px;display:flex;align-items:center;justify-content:center;transition:all 0.15s;font-family:'Segoe UI',system-ui,sans-serif;font-weight:500}
.win-btn:hover{background:rgba(109,59,255,0.2);color:#fff}
.win-btn.close:hover{background:rgba(230,57,70,0.85);color:#fff;box-shadow:0 0 12px rgba(230,57,70,0.4)}
.win-btn svg{width:12px;height:12px}

/* Device Scroll Area */
.device-scroll{flex:1;overflow-y:auto;padding:8px 40px 8px;scrollbar-width:thin;scrollbar-color:#3a3a5e transparent}
.device-scroll::-webkit-scrollbar{width:8px}
.device-scroll::-webkit-scrollbar-track{background:transparent;margin:8px 0}
.device-scroll::-webkit-scrollbar-thumb{background:#3a3a5e;border-radius:4px}
.device-scroll::-webkit-scrollbar-thumb:hover{background:#4a4a6e}

.empty-state{display:flex;flex-direction:column;align-items:center;justify-content:center;padding:80px 0;color:var(--text3)}
.empty-state .empty-icon{font-size:56px;margin-bottom:18px;opacity:0.45;color:var(--accent2)}
.empty-state p{font-size:14px;color:var(--text2)}

/* Device Card */
.device-card{
  background:
    linear-gradient(145deg, rgba(37,37,73,0.9) 0%, rgba(26,26,58,0.95) 100%);
  border:1px solid rgba(90,85,130,0.35);
  border-radius:var(--radius-lg);
  padding:24px 28px;margin-bottom:18px;
  box-shadow:
    0 4px 24px rgba(0,0,0,0.35),
    inset 0 1px 0 rgba(157,114,255,0.08);
  transition:border-color 0.25s, box-shadow 0.25s, transform 0.25s;
}
.device-card:hover{
  border-color:rgba(109,59,255,0.5);
  box-shadow:
    0 6px 32px rgba(109,59,255,0.15),
    0 4px 24px rgba(0,0,0,0.4),
    inset 0 1px 0 rgba(157,114,255,0.12);
}

/* Card Header */
.card-header{display:flex;align-items:center;gap:16px}

/* Custom Checkbox */
.cb-wrap{position:relative;display:inline-flex;align-items:center;cursor:pointer;flex-shrink:0}
.cb-wrap input{position:absolute;opacity:0;width:0;height:0}
.cb-custom{
  width:24px;height:24px;border-radius:7px;
  background:linear-gradient(145deg,#2a2a4a,#1e1e3e);
  border:1.5px solid #4a4865;
  display:flex;align-items:center;justify-content:center;
  transition:all 0.2s ease;
  box-shadow:inset 0 1px 2px rgba(0,0,0,0.3);
}
.cb-wrap input:checked+.cb-custom{
  background:linear-gradient(145deg,var(--accent) 0%,var(--accent2) 100%);
  border-color:var(--accent2);
  box-shadow:0 0 12px var(--accent-glow), inset 0 1px 0 rgba(255,255,255,0.15);
}
.cb-wrap input:checked+.cb-custom::after{content:'\2713';color:#fff;font-size:14px;font-weight:700;text-shadow:0 1px 2px rgba(0,0,0,0.3)}

/* Device Icon */
.dev-icon{
  width:44px;height:44px;border-radius:50%;
  background:radial-gradient(circle at 30% 30%, #3a355e 0%, #252342 60%, #1a1832 100%);
  border:1px solid rgba(157,114,255,0.2);
  display:flex;align-items:center;justify-content:center;font-size:20px;
  color:var(--accent2);flex-shrink:0;
  box-shadow:inset 0 2px 4px rgba(0,0,0,0.4), 0 2px 6px rgba(0,0,0,0.25);
}

/* Device Info */
.dev-info{flex:0 1 auto;min-width:0;max-width:240px}
.dev-name{font-size:17px;font-weight:600;letter-spacing:0.2px;margin-bottom:3px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev-status{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--text2)}
.status-dot{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 6px rgba(76,175,80,0.55)}
.status-dot.grey{background:var(--text4);box-shadow:none}

/* Volume Area - flex:1 占据所有剩余空间，滑块紧接设备名 */
.vol-area{display:flex;align-items:center;gap:12px;flex:1;min-width:0;margin-left:8px}
.vol-icon{font-size:16px;color:var(--text2);flex-shrink:0}
.vol-slider{flex:1;min-width:0}
.vol-pct{font-size:15px;font-weight:700;color:var(--accent2);min-width:52px;text-align:right;letter-spacing:0.3px;flex-shrink:0}

/* Filter Rows */
.filter-row{margin-top:20px}
.filter-row:first-of-type{margin-top:22px}
.filter-head{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px}
.filter-title{font-size:13px;font-weight:600;letter-spacing:0.2px}
.filter-title .abbr{color:var(--text3);font-weight:500;font-size:12px;margin-left:3px;letter-spacing:0.5px}
.filter-state{font-size:12px;font-weight:700;color:var(--accent2);letter-spacing:0.3px}
.filter-state.off{color:var(--text3);font-weight:600}

/* Slider Container */
.slider-wrap{display:flex;align-items:center;gap:12px}
.slider-label{font-size:10px;color:var(--text3);flex-shrink:0;min-width:38px;font-weight:500;letter-spacing:0.3px}
.slider-label.right{text-align:right}
.slider-main{flex:1;position:relative}
.slider-value-below{position:absolute;top:12px;transform:translateX(-50%);font-size:10px;color:var(--text2);font-weight:600;white-space:nowrap;pointer-events:none;transition:left 0.05s}

/* Range Slider */
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:5px;border-radius:3px;background:transparent;outline:none;cursor:pointer;position:relative}
/* Track - use linear gradient for filled portion, JS sets the color stops dynamically */
input[type=range]::-webkit-slider-runnable-track{height:5px;border-radius:3px;background:linear-gradient(90deg, #2a2a4a 0%, #2a2a4a 100%)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:20px;height:20px;border-radius:50%;background:linear-gradient(145deg,#fff 0%,#e6e6ff 100%);border:none;cursor:pointer;margin-top:-8px;box-shadow:0 0 0 3px var(--accent), 0 0 12px var(--accent-glow), 0 2px 6px rgba(0,0,0,0.3);transition:transform 0.12s, box-shadow 0.15s}
input[type=range]::-webkit-slider-thumb:hover{transform:scale(1.1);box-shadow:0 0 0 3px var(--accent2), 0 0 18px var(--accent-glow), 0 3px 8px rgba(0,0,0,0.35)}
input[type=range]:disabled{cursor:not-allowed}
input[type=range]:disabled::-webkit-slider-thumb{background:var(--text3);box-shadow:0 0 0 2px #5a5875, 0 1px 3px rgba(0,0,0,0.3);opacity:0.65}
input[type=range]:disabled::-webkit-slider-runnable-track{background:#222242;opacity:0.6}

/* Bottom Bar */
.bottom-bar{flex-shrink:0;padding:20px 32px 28px;display:flex;flex-direction:column;align-items:center;gap:12px}
.action-btn{
  min-width:340px;padding:16px 32px;border:none;border-radius:18px;
  font-size:17px;font-weight:700;cursor:pointer;transition:all 0.2s;
  letter-spacing:1px;color:#fff;
  background:linear-gradient(135deg, #5a2bff 0%, var(--accent) 40%, var(--accent2) 100%);
  box-shadow:
    0 6px 28px rgba(109,59,255,0.45),
    0 2px 8px rgba(109,59,255,0.3),
    inset 0 1px 0 rgba(255,255,255,0.15);
}
.action-btn:hover{
  box-shadow:
    0 8px 38px rgba(109,59,255,0.6),
    0 3px 12px rgba(109,59,255,0.4),
    inset 0 1px 0 rgba(255,255,255,0.2);
  transform:translateY(-1px);
}
.action-btn:active{transform:translateY(0);box-shadow:0 3px 18px rgba(109,59,255,0.45), inset 0 1px 0 rgba(255,255,255,0.1)}
.action-btn.stop{background:linear-gradient(135deg,#c02030 0%,#e63946 40%,#ff6b6b 100%);box-shadow:0 6px 28px rgba(230,57,70,0.4),0 2px 8px rgba(230,57,70,0.3),inset 0 1px 0 rgba(255,255,255,0.15)}
.action-btn.stop:hover{box-shadow:0 8px 38px rgba(230,57,70,0.55),0 3px 12px rgba(230,57,70,0.4),inset 0 1px 0 rgba(255,255,255,0.2)}

.status-row{display:flex;align-items:center;gap:8px;font-size:13px}
.status-dot-small{width:8px;height:8px;border-radius:50%;transition:all 0.3s}
.status-dot-small.running{background:var(--green);box-shadow:0 0 8px rgba(76,175,80,0.5)}
.status-dot-small.stopped{background:var(--text4)}
.status-text{color:var(--text2);letter-spacing:0.3px}
.status-text.running{color:var(--green);font-weight:600}

/* Toast */
.toast{position:fixed;top:20px;right:20px;padding:12px 24px;border-radius:var(--radius-md);color:#fff;font-size:13px;z-index:9999;animation:slideIn 0.3s ease;pointer-events:none;font-weight:500;letter-spacing:0.3px;backdrop-filter:blur(10px)}
.toast.error{background:rgba(230,57,70,0.92);box-shadow:0 4px 20px rgba(230,57,70,0.4), 0 2px 6px rgba(0,0,0,0.3)}
.toast.info{background:rgba(109,59,255,0.92);box-shadow:0 4px 20px rgba(109,59,255,0.4), 0 2px 6px rgba(0,0,0,0.3)}
@keyframes slideIn{from{transform:translateX(100%);opacity:0}to{transform:translateX(0);opacity:1}}
@keyframes fadeOut{from{opacity:1}to{opacity:0}}
</style>
</head>
<body>
<div class="app">
  <div class="title-bar">
    <div class="title-logo">&#9835;</div>
    <div class="title-text">AudioFlux</div>
    <div class="title-spacer"></div>
    <div class="win-ctrl">
      <button class="win-btn min" id="winMin" title="最小化">
        <svg viewBox="0 0 12 12"><line x1="2" y1="6" x2="10" y2="6" stroke="currentColor" stroke-width="1.2"/></svg>
      </button>
      <button class="win-btn max" id="winMax" title="最大化">
        <svg viewBox="0 0 12 12"><rect x="2" y="2" width="8" height="8" fill="none" stroke="currentColor" stroke-width="1.2"/></svg>
      </button>
      <button class="win-btn close" id="winClose" title="关闭">
        <svg viewBox="0 0 12 12"><line x1="2.5" y1="2.5" x2="9.5" y2="9.5" stroke="currentColor" stroke-width="1.2"/><line x1="9.5" y1="2.5" x2="2.5" y2="9.5" stroke="currentColor" stroke-width="1.2"/></svg>
      </button>
    </div>
  </div>
  <main class="device-scroll" id="deviceContainer">
    <div class="empty-state" id="emptyState">
      <div class="empty-icon">&#128266;</div>
      <p>正在检测音频设备...</p>
    </div>
  </main>
  <div class="bottom-bar">
    <button class="action-btn start" id="actionBtn">&#9654; 启动 AudioFlux</button>
    <div class="status-row">
      <span class="status-dot-small stopped" id="statusDot"></span>
      <span class="status-text stopped" id="statusText">已停止</span>
    </div>
  </div>
</div>
<script>
(function(){
  var isRunning = false;
  var devices = [];

  // 滑块位置 (1-100) 映射到频率 (20Hz-20kHz)，对数刻度；0=关闭
  function sliderToFreq(pos) {
    if (pos <= 0) return 0;
    return 20 * Math.pow(1000, (pos - 1) / 99);
  }
  function freqToSlider(hz) {
    if (hz <= 0) return 0;
    return Math.round(1 + 99 * Math.log10(hz / 20) / 3);
  }
  function fmtFreq(hz) {
    if (hz <= 0) return 'OFF';
    if (hz < 1000) return Math.round(hz) + ' Hz';
    return (hz / 1000).toFixed(1) + ' kHz';
  }

  // 动态设置滑块的渐变填充和下方值标签位置
  function updateSlider(el) {
    var mn = parseFloat(el.min), mx = parseFloat(el.max), v = parseFloat(el.value);
    var pct = ((v - mn) / (mx - mn)) * 100;
    el.style.background = 'transparent';
    // WebKit track 通过 element style 改 runnable-track 需要伪元素，我们用 JS设置渐变样式在主元素的 background linear-gradient 上
    // 因为 ::-webkit-slider-runnable-track 无法直接设置，改用一种技巧：给input本身设置渐变背景
    el.style.setProperty('background', 'linear-gradient(90deg, #5a2bff 0%, #9d72ff ' + pct + '%, #2a2a4a ' + pct + '%, #1a1a35 100%)', 'important');
    // 更新下方值标签位置
    var card = el.closest('.device-card');
    if (!card) return;
    var label = card.querySelector('.' + el.dataset.labelTarget);
    if (label) {
      var rect = el.getBoundingClientRect();
      var px = pct;
      label.style.left = px + '%';
    }
  }

  function showToast(msg, type) {
    var t = document.createElement('div');
    t.className = 'toast ' + (type || 'info');
    t.textContent = msg;
    document.body.appendChild(t);
    setTimeout(function(){ t.style.animation='fadeOut .3s ease'; setTimeout(function(){ t.remove(); },300); }, 2500);
  }

  function renderDevices(list) {
    devices = list;
    var c = document.getElementById('deviceContainer');
    c.innerHTML = '';
    if (!list || list.length === 0) {
      c.innerHTML = '<div class="empty-state"><div class="empty-icon">&#128266;</div><p>未检测到音频输出设备</p></div>';
      return;
    }
    list.forEach(function(dev) {
      var card = document.createElement('div');
      card.className = 'device-card';
      card.dataset.deviceId = dev.id;
      var statusText = dev.isDefault ? '默认设备' : '已连接';
      card.innerHTML =
        '<div class="card-header">' +
          '<label class="cb-wrap"><input type="checkbox" class="dev-enable" ' + (dev.isDefault ? '' : 'checked') + '><span class="cb-custom"></span></label>' +
          '<div class="dev-icon">&#128266;</div>' +
          '<div class="dev-info">' +
            '<div class="dev-name">' + escHtml(dev.name) + '</div>' +
            '<div class="dev-status"><span class="status-dot"></span><span>' + statusText + '</span></div>' +
          '</div>' +
          '<div class="vol-area">' +
            '<span class="vol-icon">&#128266;</span>' +
            '<input type="range" class="vol-slider slider-main" min="0" max="100" value="100">' +
            '<span class="vol-pct">100%</span>' +
          '</div>' +
        '</div>' +
        '<div class="filter-row">' +
          '<div class="filter-head">' +
            '<div class="filter-title">高通滤波器<span class="abbr">(HPF)</span></div>' +
            '<div class="filter-state hpf-val off">OFF</div>' +
          '</div>' +
          '<div class="slider-wrap">' +
            '<span class="slider-label">20Hz</span>' +
            '<div class="slider-main">' +
              '<input type="range" class="hpf-slider" min="0" max="100" value="0" data-label-target="hpf-under">' +
              '<span class="slider-value-below hpf-under" style="left:0%;opacity:0">0Hz</span>' +
            '</div>' +
            '<span class="slider-label right">20kHz</span>' +
          '</div>' +
        '</div>' +
        '<div class="filter-row">' +
          '<div class="filter-head">' +
            '<div class="filter-title">低通滤波器<span class="abbr">(LPF)</span></div>' +
            '<div class="filter-state lpf-val off">OFF</div>' +
          '</div>' +
          '<div class="slider-wrap">' +
            '<span class="slider-label">20Hz</span>' +
            '<div class="slider-main">' +
              '<input type="range" class="lpf-slider" min="0" max="100" value="0" data-label-target="lpf-under">' +
              '<span class="slider-value-below lpf-under" style="left:0%;opacity:0">0Hz</span>' +
            '</div>' +
            '<span class="slider-label right">20kHz</span>' +
          '</div>' +
        '</div>';
      c.appendChild(card);

      var volSlider = card.querySelector('.vol-slider');
      var hpfSlider = card.querySelector('.hpf-slider');
      var lpfSlider = card.querySelector('.lpf-slider');
      updateSlider(volSlider);
      updateSlider(hpfSlider);
      updateSlider(lpfSlider);

      volSlider.addEventListener('input', function() {
        card.querySelector('.vol-pct').textContent = this.value + '%';
        updateSlider(this);
        sendUpdateDebounced(dev.id);
      });
      hpfSlider.addEventListener('input', function() {
        var f = sliderToFreq(parseInt(this.value));
        var el = card.querySelector('.hpf-val');
        var under = card.querySelector('.hpf-under');
        el.textContent = fmtFreq(f);
        el.className = 'filter-state hpf-val' + (f > 0 ? '' : ' off');
        if (f > 0) {
          under.textContent = fmtFreq(f);
          under.style.opacity = '1';
        } else {
          under.style.opacity = '0';
        }
        updateSlider(this);
        sendUpdateDebounced(dev.id);
      });
      lpfSlider.addEventListener('input', function() {
        var f = sliderToFreq(parseInt(this.value));
        var el = card.querySelector('.lpf-val');
        var under = card.querySelector('.lpf-under');
        el.textContent = fmtFreq(f);
        el.className = 'filter-state lpf-val' + (f > 0 ? '' : ' off');
        if (f > 0) {
          under.textContent = fmtFreq(f);
          under.style.opacity = '1';
        } else {
          under.style.opacity = '0';
        }
        updateSlider(this);
        sendUpdateDebounced(dev.id);
      });
      card.querySelector('.dev-enable').addEventListener('change', function() {
        if (this.checked && dev.isDefault) {
          showToast('警告：默认输出设备是音频捕获源，启用可能产生反馈啸叫！', 'error');
        }
        sendUpdate(dev.id);
      });
    });
  }

  function escHtml(s) {
    var d = document.createElement('div'); d.textContent = s; return d.innerHTML;
  }

  function getDevState(id) {
    var card = document.querySelector('[data-device-id="' + id + '"]');
    if (!card) return null;
    return {
      id: id,
      enabled: card.querySelector('.dev-enable').checked,
      volume: parseInt(card.querySelector('.vol-slider').value),
      hpf: Math.round(sliderToFreq(parseInt(card.querySelector('.hpf-slider').value))),
      lpf: Math.round(sliderToFreq(parseInt(card.querySelector('.lpf-slider').value)))
    };
  }

  function sendMsg(obj) {
    window.chrome.webview.postMessage(obj);
  }

  // 节流：避免拖动滑块时每帧都发送消息，减少C++端处理压力
  var updateTimers = {};
  function sendUpdateDebounced(id) {
    if (updateTimers[id]) clearTimeout(updateTimers[id]);
    updateTimers[id] = setTimeout(function() {
      sendUpdate(id);
      delete updateTimers[id];
    }, 50);
  }
  function sendUpdate(id) {
    var s = getDevState(id);
    if (!s) return;
    sendMsg({ type: 'device_update', deviceId: s.id, enabled: s.enabled, volume: s.volume, hpf: s.hpf, lpf: s.lpf });
  }

  document.getElementById('actionBtn').addEventListener('click', function() {
    if (isRunning) {
      sendMsg({ type: 'stop' });
    } else {
      var devs = [];
      devices.forEach(function(d) {
        var s = getDevState(d.id);
        if (s) devs.push(s);
      });
      sendMsg({ type: 'start', devices: devs });
    }
  });

  function updateStatus(running) {
    isRunning = running;
    var btn = document.getElementById('actionBtn');
    var dot = document.getElementById('statusDot');
    var txt = document.getElementById('statusText');
    if (running) {
      btn.innerHTML = '&#9632; 停止 AudioFlux';
      btn.className = 'action-btn stop';
      dot.className = 'status-dot-small running';
      txt.textContent = '运行中';
      txt.className = 'status-text running';
    } else {
      btn.innerHTML = '&#9654; 启动 AudioFlux';
      btn.className = 'action-btn start';
      dot.className = 'status-dot-small stopped';
      txt.textContent = '已停止';
      txt.className = 'status-text stopped';
    }
  }

  function updateMaxButton(maximized) {
    var btn = document.getElementById('winMax');
    if (!btn) return;
    if (maximized) {
      btn.title = '还原';
      btn.innerHTML = '<svg viewBox="0 0 12 12"><rect x="3" y="1" width="7" height="7" fill="none" stroke="currentColor" stroke-width="1.2"/><rect x="1" y="3" width="7" height="7" fill="var(--bg)" stroke="currentColor" stroke-width="1.2"/></svg>';
    } else {
      btn.title = '最大化';
      btn.innerHTML = '<svg viewBox="0 0 12 12"><rect x="2" y="2" width="8" height="8" fill="none" stroke="currentColor" stroke-width="1.2"/></svg>';
    }
  }

  // 全局消息处理函数，供 C++ ExecuteScript 直接调用
  window.handleNativeMessage = function(data) {
    try {
      switch (data.type) {
        case 'device_list':
          renderDevices(data.devices);
          break;
        case 'status':
          updateStatus(data.running);
          break;
        case 'window_state':
          updateMaxButton(data.maximized);
          break;
        case 'error':
          showToast(data.message, 'error');
          break;
      }
    } catch(e) {
      window.chrome.webview.postMessage({type:'debug', message:'handleNativeMessage ERROR: ' + e.toString()});
    }
  };

  window.chrome.webview.addEventListener('message', function(e) {
    var data;
    try {
      // PostWebMessageAsJson 发送的消息，e.data 可能是字符串也可能是已解析的对象
      if (typeof e.data === 'string') {
        data = JSON.parse(e.data);
      } else {
        data = e.data;
      }
    } catch(err) {
      console.error('Failed to parse message:', err, e.data);
      return;
    }
    handleNativeMessage(data);
  });

  // 窗口控制按钮
  var minBtn = document.getElementById('winMin');
  var maxBtn = document.getElementById('winMax');
  var closeBtn = document.getElementById('winClose');
  if (minBtn) minBtn.addEventListener('click', function() { sendMsg({type:'window_control',action:'minimize'}); });
  if (maxBtn) maxBtn.addEventListener('click', function() { sendMsg({type:'window_control',action:'toggle_max'}); });
  if (closeBtn) closeBtn.addEventListener('click', function() { sendMsg({type:'window_control',action:'close'}); });

  // 窗口边缘缩放 + 标题栏拖动
  var EDGE = 6;
  var TITLEBAR_H = 40;
  var BTN_RIGHT = 140;

  function hitTest(x, y) {
    var w = window.innerWidth, h = window.innerHeight;
    if (x < EDGE && y < EDGE) return 'resize_tl';
    if (x >= w - EDGE && y < EDGE) return 'resize_tr';
    if (x < EDGE && y >= h - EDGE) return 'resize_bl';
    if (x >= w - EDGE && y >= h - EDGE) return 'resize_br';
    if (y < EDGE) return 'resize_t';
    if (y >= h - EDGE) return 'resize_b';
    if (x < EDGE) return 'resize_l';
    if (x >= w - EDGE) return 'resize_r';
    if (y < TITLEBAR_H && x < w - BTN_RIGHT) return 'drag';
    return null;
  }

  var cursorMap = {
    resize_t: 'ns-resize', resize_b: 'ns-resize',
    resize_l: 'ew-resize', resize_r: 'ew-resize',
    resize_tl: 'nwse-resize', resize_br: 'nwse-resize',
    resize_tr: 'nesw-resize', resize_bl: 'nesw-resize',
    drag: 'default'
  };

  // 鼠标移动时更新光标样式
  document.addEventListener('mousemove', function(e) {
    var action = hitTest(e.clientX, e.clientY);
    document.body.style.cursor = (action && cursorMap[action]) || '';
  });

  // 鼠标按下时发起缩放或拖动
  document.addEventListener('mousedown', function(e) {
    if (e.target.closest('input, button, label, .cb-wrap, .vol-slider, .hpf-slider, .lpf-slider')) return;
    var action = hitTest(e.clientX, e.clientY);
    if (action) {
      e.preventDefault();
      sendMsg({type:'window_control', action:action});
    }
  });

  // 双击标题栏切换最大化
  var titleBar = document.querySelector('.title-bar');
  if (titleBar) {
    titleBar.addEventListener('dblclick', function(e) {
      if (e.target.closest('.win-ctrl')) return;
      sendMsg({type:'window_control', action:'toggle_max'});
    });
  }

  // 延迟发送 refresh_devices，确保页面完全加载
  setTimeout(function() {
    sendMsg({ type: 'refresh_devices' });
  }, 500);
})();
</script>
</body>
</html>)html";

// ============================================================================
//  AudioEngine
// ============================================================================

struct DeviceConfig {
    std::string id;
    bool enabled = true;
    float hpf_hz = 0;
    float lpf_hz = 0;
    int volume = 100;
};

class AudioEngine {
public:
    AudioEngine() = default;
    ~AudioEngine() { stop(); }

    bool start(const std::vector<DeviceConfig>& devices) {
        if (running_) stop();

        buffer_ = new SharedAudioBuffer(480000);
        capture_ = new WasapiCapture(*buffer_);
        if (!capture_->initialize()) {
            delete capture_; capture_ = nullptr;
            delete buffer_; buffer_ = nullptr;
            return false;
        }

        output_ = new WasapiOutput(*buffer_);
        if (!output_->initialize()) {
            delete output_; output_ = nullptr;
            delete capture_; capture_ = nullptr;
            delete buffer_; buffer_ = nullptr;
            return false;
        }

        output_->setFormat(capture_->getSampleRate(), capture_->getChannels());
        capture_->start();
        Sleep(50);

        int ok = 0;
        for (auto& dc : devices) {
            if (dc.enabled) {
                if (output_->startDevice(dc.id, dc.hpf_hz, dc.lpf_hz, dc.volume))
                    ++ok;
            }
        }
        if (ok == 0) {
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

    void stop() {
        if (!running_) return;
        running_ = false;
        if (output_)  { output_->stopAll();  delete output_;  output_  = nullptr; }
        if (capture_) { capture_->stop();    delete capture_; capture_ = nullptr; }
        if (buffer_)  { delete buffer_; buffer_ = nullptr; }
        configs_.clear();
    }

    void updateDevice(const std::string& deviceId, float hpf, float lpf, int volume) {
        if (!running_ || !output_) return;
        output_->updateDevice(deviceId, hpf, lpf, volume);
        for (auto& c : configs_) {
            if (c.id == deviceId) {
                c.hpf_hz = hpf; c.lpf_hz = lpf; c.volume = volume;
                break;
            }
        }
    }

    std::vector<DeviceInfo> enumerateDevices() {
        // 不能创建临时 WasapiOutput（它的析构会 CoUninitialize 破坏主线程 COM）
        // 直接用 COM API 枚举
        IMMDeviceEnumerator* enumerator = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                      __uuidof(IMMDeviceEnumerator), (void**)&enumerator);
        if (FAILED(hr) || !enumerator) return {};

        // 获取默认设备 ID
        std::string default_id;
        IMMDevice* default_dev = nullptr;
        if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &default_dev))) {
            LPWSTR did = nullptr;
            if (SUCCEEDED(default_dev->GetId(&did))) {
                default_id = WideToUtf8(did);
                CoTaskMemFree(did);
            }
            default_dev->Release();
        }

        // 枚举所有活跃输出设备
        std::vector<DeviceInfo> devices;
        IMMDeviceCollection* collection = nullptr;
        if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &collection))) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice* device = nullptr;
                if (FAILED(collection->Item(i, &device))) continue;

                DeviceInfo info;
                // ID
                LPWSTR id = nullptr;
                if (SUCCEEDED(device->GetId(&id))) {
                    info.id = WideToUtf8(id);
                    CoTaskMemFree(id);
                }
                // 名称
                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                    PROPVARIANT name;
                    PropVariantInit(&name);
                    if (SUCCEEDED(props->GetValue(PKEY_DEVICE_FRIENDLYNAME, &name))) {
                        info.name = WideToUtf8(name.pwszVal);
                        PropVariantClear(&name);
                    }
                    props->Release();
                }
                info.is_default = (info.id == default_id);
                devices.push_back(info);
                device->Release();
            }
            collection->Release();
        }
        enumerator->Release();
        return devices;
    }

    bool isRunning() const { return running_; }

private:
    SharedAudioBuffer* buffer_ = nullptr;
    WasapiCapture*     capture_ = nullptr;
    WasapiOutput*      output_ = nullptr;
    bool running_ = false;
    std::vector<DeviceConfig> configs_;
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
        WNDCLASSW wc = {};
        wc.lpfnWndProc = &App::StaticWndProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"AudioFluxWnd";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        RegisterClassW(&wc);

        // WS_POPUP + WS_THICKFRAME: 无可见边框，但保留系统缩放能力
        RECT wr = { 0, 0, 1000, 1200 };
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

    bool initWebView() {
        // Load WebView2Loader.dll dynamically
        wv2Dll_ = LoadLibraryW(L"WebView2Loader.dll");
        if (!wv2Dll_) {
            MessageBoxW(hwnd_, L"WebView2Loader.dll not found.\nPlease ensure WebView2 Runtime is installed.",
                L"Error", MB_OK | MB_ICONERROR);
            return false;
        }

        typedef HRESULT(STDMETHODCALLTYPE* PFN_CreateEnv)(
            PCWSTR, PCWSTR, ICoreWebView2EnvironmentOptions*,
            ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
        auto pCreateEnv = (PFN_CreateEnv)GetProcAddress(wv2Dll_, "CreateCoreWebView2EnvironmentWithOptions");
        if (!pCreateEnv) {
            MessageBoxW(hwnd_, L"Failed to get CreateCoreWebView2EnvironmentWithOptions.",
                L"Error", MB_OK | MB_ICONERROR);
            return false;
        }

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
                    MessageBoxW(hwnd_, L"Failed to create WebView2 environment.",
                        L"Error", MB_OK | MB_ICONERROR);
                    return;
                }
                // Create controller
                env->CreateCoreWebView2Controller(hwnd_,
                    new CtrlCompletedHandler(
                        [this](HRESULT hr2, ICoreWebView2Controller* ctrl) {
                            if (FAILED(hr2) || !ctrl) {
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
                        }));
            });

        pCreateEnv(nullptr, userDataDir.c_str(), nullptr, envHandler);
        return true;
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
            sendDeviceList(devs);
        }
        else if (type == "start") {
            std::vector<DeviceConfig> configs;
            auto& arr = msg["devices"].asArray();
            for (size_t i = 0; i < arr.size(); ++i) {
                auto& d = arr[i];
                DeviceConfig dc;
                dc.id = d["id"].asString();
                dc.enabled = d["enabled"].asBool();
                dc.hpf_hz = (float)d["hpf"].asNumber();
                dc.lpf_hz = (float)d["lpf"].asNumber();
                dc.volume = d["volume"].asInt();
                configs.push_back(dc);
            }
            if (!engine_.start(configs)) {
                sendMessage("{\"type\":\"error\",\"message\":\"启动失败，请检查音频设备\"}");
            } else {
                sendStatus(true);
            }
        }
        else if (type == "stop") {
            engine_.stop();
            sendStatus(false);
        }
        else if (type == "device_update") {
            std::string devId = msg["deviceId"].asString();
            float hpf = (float)msg["hpf"].asNumber();
            float lpf = (float)msg["lpf"].asNumber();
            int vol = msg["volume"].asInt();
            engine_.updateDevice(devId, hpf, lpf, vol);
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
                SendMessageW(hwnd_, WM_CLOSE, 0, 0);
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

    void sendDeviceList(const std::vector<DeviceInfo>& devs) {
        std::string json = "{\"type\":\"device_list\",\"devices\":[";
        for (size_t i = 0; i < devs.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"id\":\"" + EscapeJson(devs[i].id) +
                    "\",\"name\":\"" + EscapeJson(devs[i].name) +
                    "\",\"isDefault\":" + (devs[i].is_default ? "true" : "false") + "}";
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
        engine_.stop();
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
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    App app;
    if (!app.init(hInstance)) {
        CoUninitialize();
        return 1;
    }

    int ret = app.run();
    CoUninitialize();
    return ret;
}
