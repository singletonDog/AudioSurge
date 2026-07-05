#pragma once

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
.title-logo{width:24px;height:24px;display:flex;align-items:center;justify-content:center;filter:drop-shadow(0 0 6px rgba(109,59,255,0.6))}
.title-logo img{width:100%;height:100%;object-fit:contain;display:block}
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
  color:var(--accent2);flex-shrink:0;cursor:pointer;user-select:none;
  box-shadow:inset 0 2px 4px rgba(0,0,0,0.4), 0 2px 6px rgba(0,0,0,0.25);
  transition:transform 0.15s, border-color 0.15s, box-shadow 0.15s;
}
.dev-icon:hover{transform:scale(1.06);border-color:rgba(157,114,255,0.55);box-shadow:inset 0 2px 4px rgba(0,0,0,0.4), 0 0 14px rgba(157,114,255,0.24)}

/* Device Info */
.dev-info{flex:0 1 auto;min-width:0;max-width:240px}
.dev-name{font-size:17px;font-weight:600;letter-spacing:0.2px;margin-bottom:3px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.dev-status{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--text2)}
.status-dot{width:7px;height:7px;border-radius:50%;background:var(--green);box-shadow:0 0 6px rgba(76,175,80,0.55)}
.status-dot.grey{background:var(--text4);box-shadow:none}

/* Volume Area - flex:1 占据所有剩余空间，滑块紧接设备名 */
.vol-area{display:flex;align-items:center;gap:12px;flex:1;min-width:0;margin-left:8px}
.vol-icon{font-size:16px;color:var(--text2);flex-shrink:0;cursor:pointer;user-select:none;transition:color 0.15s, transform 0.15s}
.vol-icon:hover{color:var(--accent2);transform:scale(1.08)}
.vol-icon.muted{color:var(--red)}
.vol-slider{flex:1;min-width:0}
/* 音量滑块仿频段滑块：轨道透明，用独立 .vol-track/.vol-fill 显示灰底与紫色填充 */
.vol-slider-wrap{position:relative;flex:1;min-width:0;height:20px;display:flex;align-items:center}
.vol-track{position:absolute;top:50%;left:0;right:0;height:5px;transform:translateY(-50%);border-radius:3px;background:#2a2a4a;pointer-events:none}
.vol-fill{position:absolute;top:50%;left:0;height:5px;transform:translateY(-50%);border-radius:3px;background:linear-gradient(90deg,#5a2bff 0%,#9d72ff 100%);pointer-events:none}
.vol-slider-wrap input[type=range]{position:absolute;top:0;left:0;width:100%;height:20px;margin:0;background:transparent !important}
.vol-slider-wrap input[type=range]::-webkit-slider-runnable-track{height:20px;background:transparent}
.vol-slider-wrap input[type=range]::-webkit-slider-thumb{margin-top:0}
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

/* Band (dual-thumb) Slider */
.band-slider{position:relative;flex:1;height:20px}
.band-track{position:absolute;top:50%;left:0;right:0;height:5px;transform:translateY(-50%);border-radius:3px;background:#2a2a4a}
.band-fill{position:absolute;top:50%;height:5px;transform:translateY(-50%);border-radius:3px;background:linear-gradient(90deg,#5a2bff 0%,#9d72ff 100%);pointer-events:none}
.band-slider input[type=range]{position:absolute;top:0;left:0;width:100%;height:20px;margin:0;background:none !important;pointer-events:none}
.band-slider input[type=range]::-webkit-slider-runnable-track{height:20px;background:transparent}
.band-slider input[type=range]::-webkit-slider-thumb{pointer-events:auto;margin-top:0}
.band-slider input.band-low{z-index:3}
.band-slider input.band-high{z-index:4}
.band-below{position:absolute;top:20px;transform:translateX(-50%);font-size:10px;color:var(--text2);font-weight:600;white-space:nowrap;pointer-events:none;transition:left 0.05s}

.bottom-bar{flex-shrink:0;padding:20px 32px 28px;display:flex;flex-direction:column;align-items:center;gap:12px;position:relative}
.corner-btn{position:absolute;bottom:28px;width:44px;height:44px;border:1px solid rgba(109,59,255,0.35);border-radius:14px;background:rgba(37,37,73,0.72);color:var(--text2);font-size:20px;font-weight:600;cursor:pointer;transition:all 0.15s;display:flex;align-items:center;justify-content:center;backdrop-filter:blur(10px)}
.corner-btn:hover{border-color:rgba(157,114,255,0.65);color:#fff;background:rgba(109,59,255,0.24);transform:translateY(-1px)}
.corner-btn:disabled{opacity:0.55;cursor:not-allowed;transform:none}
.settings-btn{left:32px}
.refresh-btn{right:32px}
.refresh-icon{display:inline-block;line-height:1}
.refresh-btn.refreshing .refresh-icon{animation:spin 0.8s linear infinite}
@keyframes spin{from{transform:rotate(0deg)}to{transform:rotate(360deg)}}

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

/* Settings floating panel */
.settings-panel{position:absolute;left:32px;bottom:84px;width:260px;padding:18px 20px;
  background:rgba(28,28,58,0.98);border:1px solid rgba(109,59,255,0.4);border-radius:16px;
  box-shadow:0 12px 40px rgba(0,0,0,0.45),0 2px 8px rgba(109,59,255,0.25);
  backdrop-filter:blur(16px);z-index:9998;display:none;animation:panelIn 0.18s ease}
.settings-panel.open{display:block}
@keyframes panelIn{from{transform:translateY(8px);opacity:0}to{transform:translateY(0);opacity:1}}
.settings-panel .panel-title{font-size:14px;font-weight:700;color:var(--text1);margin-bottom:16px;letter-spacing:0.3px}
.settings-row{display:flex;align-items:center;justify-content:space-between;gap:12px}
.settings-row .row-label{font-size:13px;color:var(--text2)}
.switch{position:relative;display:inline-block;width:44px;height:24px;flex-shrink:0}
.switch input{opacity:0;width:0;height:0}
.switch .track{position:absolute;inset:0;cursor:pointer;background:rgba(120,120,150,0.4);border-radius:24px;transition:0.2s}
.switch .track::before{content:"";position:absolute;height:18px;width:18px;left:3px;top:3px;background:#fff;border-radius:50%;transition:0.2s}
.switch input:checked + .track{background:linear-gradient(135deg,var(--accent) 0%,var(--accent2) 100%)}
.switch input:checked + .track::before{transform:translateX(20px)}

/* 关闭确认模态框 */
.modal-mask{position:fixed;inset:0;background:rgba(8,8,20,0.55);backdrop-filter:blur(4px);z-index:10000;display:flex;align-items:center;justify-content:center;animation:fadeIn 0.15s ease}
@keyframes fadeIn{from{opacity:0}to{opacity:1}}
.modal-box{width:320px;padding:26px 26px 22px;border-radius:18px;border:1px solid rgba(109,59,255,0.4);
  background:linear-gradient(145deg,rgba(37,37,73,0.98),rgba(26,26,58,0.98));
  box-shadow:0 18px 50px rgba(0,0,0,0.5),0 2px 10px rgba(109,59,255,0.3);animation:panelIn 0.18s ease}
.modal-title{font-size:16px;font-weight:700;color:var(--text1);margin-bottom:12px;letter-spacing:0.3px}
.modal-msg{font-size:13px;color:var(--text2);line-height:1.6;margin-bottom:22px}
.modal-actions{display:flex;gap:12px}
.modal-btn{flex:1;padding:11px 0;border:1px solid rgba(109,59,255,0.4);border-radius:12px;
  background:rgba(109,59,255,0.16);color:#fff;font-size:14px;font-weight:600;cursor:pointer;transition:all 0.15s}
.modal-btn:hover{background:rgba(109,59,255,0.32);border-color:rgba(157,114,255,0.7);transform:translateY(-1px)}
.modal-btn.danger{background:rgba(230,57,70,0.16);border-color:rgba(230,57,70,0.45)}
.modal-btn.danger:hover{background:rgba(230,57,70,0.32);border-color:rgba(255,107,107,0.7)}
</style>
</head>
<body>
<div class="app">
  <div class="title-bar">
    <div class="title-logo"><img src="data:image/png;base64,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" alt="AudioFlux"></div>
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
    <button class="corner-btn settings-btn" id="settingsBtn" title="设置" aria-label="设置">&#9881;</button>
    <div class="settings-panel" id="settingsPanel">
      <div class="panel-title">设置</div>
      <div class="settings-row">
        <span class="row-label">系统托盘图标</span>
        <label class="switch">
          <input type="checkbox" id="trayToggle" checked>
          <span class="track"></span>
        </label>
      </div>
    </div>
    <div class="status-row">
      <span class="status-dot-small stopped" id="statusDot"></span>
      <span class="status-text stopped" id="statusText">正在启动...</span>
    </div>
    <button class="corner-btn refresh-btn" id="refreshBtn" title="刷新设备" aria-label="刷新设备"><span class="refresh-icon">&#8635;</span></button>
  </div>
</div>
<script>
(function(){
  var isRunning = false;
  var pendingStart = false;
  var forceRestart = false;
  var devices = [];
  var stateById = {};

  // 频段双拖柄：统一 20Hz~20000Hz 对数轴
  // 视觉位置用 0~BAND_STEPS 对数分度；真实频率存在拖柄的 data-hz 上，支持逐 1Hz 精调
  var BAND = { min: 20, max: 20000 };
  var BAND_STEPS = 1000;      // 视觉位置分度(越大鼠标拖动越顺滑)
  var BAND_MIN_RATIO = 1.1;   // 两边界最小频率比，防止交叉成陷波
  function posToFreq(pos) {
    pos = Math.max(0, Math.min(BAND_STEPS, pos));
    return BAND.min * Math.pow(BAND.max / BAND.min, pos / BAND_STEPS);
  }
  function freqToPos(hz) {
    if (hz <= BAND.min) return 0;
    if (hz >= BAND.max) return BAND_STEPS;
    return Math.round(BAND_STEPS * Math.log(hz / BAND.min) / Math.log(BAND.max / BAND.min));
  }
  function clampHz(hz) {
    return Math.max(BAND.min, Math.min(BAND.max, Math.round(hz)));
  }
  // 从拖柄 data-hz 派生 hpf/lpf：低边界拖到最左(20Hz)=关闭高通；高边界拖到最右(20000Hz)=关闭低通
  function lowHzToHpf(hz) { return hz <= BAND.min ? 0 : Math.round(hz); }
  function highHzToLpf(hz) { return hz >= BAND.max ? 0 : Math.round(hz); }
  function fmtFreq(hz) {
    if (hz <= 0) return 'OFF';
    return Math.round(hz) + ' Hz';
  }
  // 写入某个拖柄的频率(带约束)，同步更新视觉位置与整个频段显示
  function setBandHz(card, input, isLow, newHz) {
    var low = card.querySelector('.band-low');
    var high = card.querySelector('.band-high');
    if (!low || !high) return;
    newHz = clampHz(newHz);
    var otherHz = parseFloat((isLow ? high : low).dataset.hz);
    if (isLow) {
      if (newHz > otherHz / BAND_MIN_RATIO) newHz = Math.round(otherHz / BAND_MIN_RATIO);
    } else {
      if (newHz < otherHz * BAND_MIN_RATIO) newHz = Math.round(otherHz * BAND_MIN_RATIO);
    }
    newHz = clampHz(newHz);
    input.dataset.hz = newHz;
    input.value = freqToPos(newHz);
    updateBand(card);
  }

  // 动态设置滑块的填充宽度和下方值标签位置
  function updateSlider(el) {
    var mn = parseFloat(el.min), mx = parseFloat(el.max), v = parseFloat(el.value);
    var pct = ((v - mn) / (mx - mn)) * 100;
    // 仿频段滑块：填充由独立的 .vol-fill 显示，input 轨道透明
    var wrap = el.closest('.vol-slider-wrap');
    if (wrap) {
      var fill = wrap.querySelector('.vol-fill');
      if (fill) fill.style.width = pct + '%';
    }
    // 更新下方值标签位置
    var card = el.closest('.device-card');
    if (!card) return;
    var label = card.querySelector('.' + el.dataset.labelTarget);
    if (label) {
      label.style.left = pct + '%';
    }
  }

  // 更新频段双拖柄：填充条位置、拖柄标签、状态文本；真值取自 data-hz
  function updateBand(card) {
    var low = card.querySelector('.band-low');
    var high = card.querySelector('.band-high');
    if (!low || !high) return;
    var loHz = parseFloat(low.dataset.hz);
    var hiHz = parseFloat(high.dataset.hz);
    var loPct = freqToPos(loHz) / BAND_STEPS * 100;
    var hiPct = freqToPos(hiHz) / BAND_STEPS * 100;
    var fill = card.querySelector('.band-fill');
    if (fill) {
      fill.style.left = loPct + '%';
      fill.style.width = (hiPct - loPct) + '%';
    }
    var hpf = lowHzToHpf(loHz);
    var lpf = highHzToLpf(hiHz);
    var loBelow = card.querySelector('.band-low-below');
    var hiBelow = card.querySelector('.band-high-below');
    if (loBelow) {
      loBelow.textContent = fmtFreq(hpf);
      loBelow.style.left = loPct + '%';
      loBelow.style.opacity = hpf > 0 ? '1' : '0';
    }
    if (hiBelow) {
      hiBelow.textContent = fmtFreq(lpf);
      hiBelow.style.left = hiPct + '%';
      hiBelow.style.opacity = lpf > 0 ? '1' : '0';
    }
    var state = card.querySelector('.band-val');
    if (state) {
      if (hpf <= 0 && lpf <= 0) {
        state.textContent = '全频段';
        state.className = 'filter-state band-val off';
      } else if (hpf > 0 && lpf <= 0) {
        state.textContent = '\u2265 ' + fmtFreq(hpf);
        state.className = 'filter-state band-val';
      } else if (hpf <= 0 && lpf > 0) {
        state.textContent = '\u2264 ' + fmtFreq(lpf);
        state.className = 'filter-state band-val';
      } else {
        state.textContent = Math.round(hpf) + ' \u2013 ' + fmtFreq(lpf);
        state.className = 'filter-state band-val';
      }
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
    var previous = {};
    devices.forEach(function(d) {
      var s = getDevState(d.id);
      if (s) previous[d.id] = s;
    });
    devices = list;
    stateById = {};
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
      var volume = typeof dev.volume === 'number' ? dev.volume : 100;
      var muted = !!dev.muted;
      var prev = previous[dev.id];
      var savedEnabled = typeof dev.savedEnabled === 'boolean' ? dev.savedEnabled : true;
      var savedHpf = typeof dev.savedHpf === 'number' ? dev.savedHpf : 0;
      var savedLpf = typeof dev.savedLpf === 'number' ? dev.savedLpf : 0;
      var enabled = dev.isDefault ? false : (prev ? prev.enabled : savedEnabled);
      var lowHz = !dev.isDefault && prev && prev.hpf > 0 ? prev.hpf
                : (!dev.isDefault && savedHpf > 0 ? savedHpf : BAND.min);
      var highHz = !dev.isDefault && prev && prev.lpf > 0 ? prev.lpf
                 : (!dev.isDefault && savedLpf > 0 ? savedLpf : BAND.max);
      var lowPos = freqToPos(lowHz);
      var highPos = freqToPos(highHz);
      card.dataset.muted = muted ? 'true' : 'false';
      var statusText = dev.isDefault ? '默认捕获源' : '已连接';
      var enableHtml = dev.isDefault ? '' : '<label class="cb-wrap"><input type="checkbox" class="dev-enable" ' + (enabled ? 'checked' : '') + '><span class="cb-custom"></span></label>';
      var filterHtml = dev.isDefault ? '' :
        '<div class="filter-row">' +
          '<div class="filter-head">' +
            '<div class="filter-title">通过频段<span class="abbr">(HPF~LPF)</span></div>' +
            '<div class="filter-state band-val off">全频段</div>' +
          '</div>' +
          '<div class="slider-wrap">' +
            '<span class="slider-label">20Hz</span>' +
            '<div class="band-slider">' +
              '<div class="band-track"></div>' +
              '<div class="band-fill"></div>' +
              '<input type="range" class="band-low" min="0" max="' + BAND_STEPS + '" value="' + lowPos + '" data-hz="' + lowHz + '" title="拖动粗调，聚焦后方向键±1Hz，Shift+方向键±100Hz">' +
              '<input type="range" class="band-high" min="0" max="' + BAND_STEPS + '" value="' + highPos + '" data-hz="' + highHz + '" title="拖动粗调，聚焦后方向键±1Hz，Shift+方向键±100Hz">' +
              '<span class="band-below band-low-below" style="left:0%;opacity:0">OFF</span>' +
              '<span class="band-below band-high-below" style="left:100%;opacity:0">OFF</span>' +
            '</div>' +
            '<span class="slider-label right">20000Hz</span>' +
          '</div>' +
        '</div>';
      card.innerHTML =
        '<div class="card-header">' +
          enableHtml +
          '<div class="dev-icon">&#128266;</div>' +
          '<div class="dev-info">' +
            '<div class="dev-name">' + escHtml(dev.name) + '</div>' +
            '<div class="dev-status"><span class="status-dot"></span><span>' + statusText + '</span></div>' +
          '</div>' +
          '<div class="vol-area">' +
            '<span class="vol-icon">&#128266;</span>' +
            '<div class="vol-slider-wrap">' +
              '<div class="vol-track"></div>' +
              '<div class="vol-fill"></div>' +
              '<input type="range" class="vol-slider slider-main" min="0" max="100" value="' + volume + '">' +
            '</div>' +
            '<span class="vol-pct">' + volume + '%</span>' +
          '</div>' +
        '</div>' + filterHtml;
      c.appendChild(card);

      var volSlider = card.querySelector('.vol-slider');
      var bandLow = card.querySelector('.band-low');
      var bandHigh = card.querySelector('.band-high');
      updateSlider(volSlider);
      if (bandLow && bandHigh) updateBand(card);
      updateMuteUi(card);

      volSlider.addEventListener('input', function() {
        setVolumeUi(card, parseInt(this.value), card.dataset.muted === 'true', false);
        sendUpdateDebounced(dev.id);
      });
      if (bandLow && bandHigh) {
        // 鼠标拖动：视觉位置(对数)转成频率，经 setBandHz 约束后回写 data-hz
        bandLow.addEventListener('input', function() {
          setBandHz(card, this, true, posToFreq(parseInt(this.value)));
          sendUpdateDebounced(dev.id);
        });
        bandHigh.addEventListener('input', function() {
          setBandHz(card, this, false, posToFreq(parseInt(this.value)));
          sendUpdateDebounced(dev.id);
        });
        // 键盘微调：方向键 ±1Hz，Shift+方向键 ±100Hz（真正逐 Hz 精调）
        function bandKey(input, isLow) {
          input.addEventListener('keydown', function(e) {
            var dir = (e.key === 'ArrowRight' || e.key === 'ArrowUp') ? 1
                    : (e.key === 'ArrowLeft' || e.key === 'ArrowDown') ? -1 : 0;
            if (!dir) return;
            e.preventDefault();
            var step = e.shiftKey ? 100 : 1;
            var cur = parseFloat(input.dataset.hz);
            setBandHz(card, input, isLow, cur + dir * step);
            sendUpdateDebounced(dev.id);
          });
        }
        bandKey(bandLow, true);
        bandKey(bandHigh, false);
      }
      var enableBox = card.querySelector('.dev-enable');
      if (enableBox) {
        enableBox.addEventListener('change', function() {
          sendUpdate(dev.id);
        });
      }
      card.querySelector('.dev-icon').addEventListener('dblclick', function() {
        if (dev.isDefault) {
          showToast('该设备已经是默认捕获源', 'info');
          return;
        }
        sendMsg({ type: 'set_default_device', deviceId: dev.id, name: dev.name });
      });
      card.querySelector('.vol-icon').addEventListener('click', function() {
        setVolumeUi(card, parseInt(card.querySelector('.vol-slider').value), !(card.dataset.muted === 'true'), false);
        sendUpdate(dev.id);
      });
    });
  }

  function updateMuteUi(card) {
    var icon = card.querySelector('.vol-icon');
    var muted = card.dataset.muted === 'true';
    icon.innerHTML = muted ? '&#128263;' : '&#128266;';
    icon.className = 'vol-icon' + (muted ? ' muted' : '');
    icon.title = muted ? '取消静音' : '静音';
  }

  function setVolumeUi(card, volume, muted, fromNative) {
    volume = Math.max(0, Math.min(100, parseInt(volume) || 0));
    card.dataset.muted = muted ? 'true' : 'false';
    var slider = card.querySelector('.vol-slider');
    slider.value = volume;
    card.querySelector('.vol-pct').textContent = volume + '%';
    updateSlider(slider);
    updateMuteUi(card);
  }

  function applyNativeVolume(id, volume, muted) {
    var card = document.querySelector('[data-device-id="' + id + '"]');
    if (!card) return;
    setVolumeUi(card, volume, muted, true);
  }

  function applyDeviceEnabled(id, enabled) {
    var card = document.querySelector('[data-device-id="' + id + '"]');
    if (!card) return;
    var enableBox = card.querySelector('.dev-enable');
    if (!enableBox) return;
    enableBox.checked = !!enabled;
  }

  function setRefreshState(refreshing) {
    var btn = document.getElementById('refreshBtn');
    if (!btn) return;
    btn.disabled = !!refreshing;
    btn.title = refreshing ? '刷新中...' : '刷新设备';
    btn.setAttribute('aria-label', btn.title);
    btn.className = 'corner-btn refresh-btn' + (refreshing ? ' refreshing' : '');
  }

  function escHtml(s) {
    var d = document.createElement('div'); d.textContent = s; return d.innerHTML;
  }

  function getDevState(id) {
    var card = document.querySelector('[data-device-id="' + id + '"]');
    if (!card) return null;
    var dev = devices.find(function(d) { return d.id === id; }) || {};
    var isDefault = !!dev.isDefault;
    var enableBox = card.querySelector('.dev-enable');
    var bandLow = card.querySelector('.band-low');
    var bandHigh = card.querySelector('.band-high');
    return {
      id: id,
      name: dev.name || card.querySelector('.dev-name').textContent,
      enabled: isDefault ? false : !!(enableBox && enableBox.checked),
      volume: parseInt(card.querySelector('.vol-slider').value),
      muted: card.dataset.muted === 'true',
      hpf: isDefault || !bandLow ? 0 : lowHzToHpf(parseFloat(bandLow.dataset.hz)),
      lpf: isDefault || !bandHigh ? 0 : highHzToLpf(parseFloat(bandHigh.dataset.hz))
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
    sendMsg({ type: 'device_update', deviceId: s.id, name: s.name, enabled: s.enabled, volume: s.volume, muted: s.muted, hpf: s.hpf, lpf: s.lpf });
  }

  function refreshDevices(silent, force) {
    setRefreshState(!silent);
    pendingStart = true;
    forceRestart = !!force;
    sendMsg({ type: 'refresh_devices', silent: !!silent });
  }

  function startAudio() {
    var devs = [];
    devices.forEach(function(d) {
      var s = getDevState(d.id);
      if (s) devs.push(s);
    });
    sendMsg({ type: 'start', devices: devs });
  }

  document.getElementById('refreshBtn').addEventListener('click', function() {
    refreshDevices(false, true);
  });

  function updateStatus(running) {
    isRunning = running;
    var dot = document.getElementById('statusDot');
    var txt = document.getElementById('statusText');
    if (running) {
      dot.className = 'status-dot-small running';
      txt.textContent = '运行中';
      txt.className = 'status-text running';
    } else {
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
          setRefreshState(false);
          if (!data.silent) showToast('设备列表已刷新', 'info');
          if (pendingStart) {
            pendingStart = false;
            if (forceRestart || !data.running) {
              startAudio();
            }
            forceRestart = false;
          }
          break;
        case 'status':
          updateStatus(data.running);
          break;
        case 'window_state':
          updateMaxButton(data.maximized);
          break;
        case 'tray_state':
          applyTrayState(data.enabled);
          break;
        case 'volume_changed':
          applyNativeVolume(data.deviceId, data.volume, data.muted);
          break;
        case 'device_enabled':
          applyDeviceEnabled(data.deviceId, data.enabled);
          break;
        case 'info':
          showToast(data.message || '', 'info');
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
  if (closeBtn) closeBtn.addEventListener('click', function() {
    var trayOn = document.getElementById('trayToggle');
    if (trayOn && trayOn.checked) {
      // 托盘开启：直接隐藏到托盘
      sendMsg({type:'window_control', action:'close'});
    } else {
      // 托盘关闭：询问后台运行还是退出
      showCloseDialog();
    }
  });

  // 关闭确认对话框：左“后台运行”，右“退出程序”
  function showCloseDialog() {
    if (document.getElementById('closeDialogMask')) return;
    var mask = document.createElement('div');
    mask.id = 'closeDialogMask';
    mask.className = 'modal-mask';
    mask.innerHTML =
      '<div class="modal-box">' +
        '<div class="modal-title">关闭 AudioFlux</div>' +
        '<div class="modal-msg">您希望让程序在后台继续运行，还是退出程序？</div>' +
        '<div class="modal-actions">' +
          '<button class="modal-btn" id="closeDlgBackground">后台运行</button>' +
          '<button class="modal-btn danger" id="closeDlgExit">退出程序</button>' +
        '</div>' +
      '</div>';
    document.body.appendChild(mask);
    function closeDlg() { mask.remove(); }
    mask.addEventListener('click', function(e) { if (e.target === mask) closeDlg(); });
    document.getElementById('closeDlgBackground').addEventListener('click', function() {
      closeDlg();
      sendMsg({type:'window_control', action:'background'});
    });
    document.getElementById('closeDlgExit').addEventListener('click', function() {
      sendMsg({type:'window_control', action:'exit'});
    });
  }

  // 设置浮动面板与系统托盘开关
  var settingsBtn = document.getElementById('settingsBtn');
  var settingsPanel = document.getElementById('settingsPanel');
  var trayToggle = document.getElementById('trayToggle');

  function applyTrayState(enabled) {
    if (trayToggle) trayToggle.checked = !!enabled;
  }

  if (settingsBtn && settingsPanel) {
    settingsBtn.addEventListener('click', function(e) {
      e.stopPropagation();
      settingsPanel.classList.toggle('open');
    });
    settingsPanel.addEventListener('click', function(e) { e.stopPropagation(); });
    document.addEventListener('click', function() {
      settingsPanel.classList.remove('open');
    });
  }

  if (trayToggle) {
    trayToggle.addEventListener('change', function() {
      sendMsg({type:'set_tray', enabled: trayToggle.checked});
    });
  }

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
    if (e.target.closest('input, button, label, .cb-wrap, .dev-icon, .vol-slider, .vol-icon, .band-slider, .band-low, .band-high')) return;
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
    refreshDevices(true);
  }, 500);
})();
</script>
</body>
</html>)html";

