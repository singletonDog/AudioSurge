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
.settings-btn{left:32px;cursor:default}
.settings-btn:hover{color:var(--text2);background:rgba(37,37,73,0.72);border-color:rgba(109,59,255,0.35);transform:none}
.refresh-btn{right:32px}
.refresh-icon{display:inline-block;line-height:1}
.refresh-btn.refreshing .refresh-icon{animation:spin 0.8s linear infinite}
@keyframes spin{from{transform:rotate(0deg)}to{transform:rotate(360deg)}}
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
    <button class="corner-btn settings-btn" id="settingsBtn" title="设置（暂未开放）" aria-label="设置（暂未开放）">&#9881;</button>
    <button class="action-btn start" id="actionBtn">&#9654; 启动 AudioFlux</button>
    <div class="status-row">
      <span class="status-dot-small stopped" id="statusDot"></span>
      <span class="status-text stopped" id="statusText">已停止</span>
    </div>
    <button class="corner-btn refresh-btn" id="refreshBtn" title="刷新设备" aria-label="刷新设备"><span class="refresh-icon">&#8635;</span></button>
  </div>
</div>
<script>
(function(){
  var isRunning = false;
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
      var enabled = dev.isDefault ? false : (prev ? prev.enabled : true);
      var lowHz = !dev.isDefault && prev && prev.hpf > 0 ? prev.hpf : BAND.min;
      var highHz = !dev.isDefault && prev && prev.lpf > 0 ? prev.lpf : BAND.max;
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
            '<input type="range" class="vol-slider slider-main" min="0" max="100" value="' + volume + '">' +
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

  function refreshDevices(silent) {
    setRefreshState(!silent);
    sendMsg({ type: 'refresh_devices', silent: !!silent });
  }

  document.getElementById('refreshBtn').addEventListener('click', function() {
    refreshDevices(false);
  });

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
          setRefreshState(false);
          if (!data.silent) showToast('设备列表已刷新', 'info');
          break;
        case 'status':
          updateStatus(data.running);
          break;
        case 'window_state':
          updateMaxButton(data.maximized);
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

