"""
Digital Slate Interactive PC Simulator (Phases P1 to P8)
Simulates the ESP32-2432S028 touchscreen slate, stroke engine, TinyML classifier,
expression reconstruction, and recursive descent calculation engine on PC.
Runs a local web server with zero third-party dependencies (pure Python standard library).
"""

import os
import sys
import json
import time
import subprocess
import webbrowser
from pathlib import Path
from http.server import HTTPServer, SimpleHTTPRequestHandler

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "data"
P8_EXPR_DIR = DATA_DIR / "p8_expressions"
P4_TESTSET_DIR = DATA_DIR / "p4_testset"
HOST_EVAL_EXE = REPO_ROOT / "host" / "test_expression.exe"

HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Digital Slate Calculator - Interactive PC Simulator</title>
<style>
  :root {
    --bg-dark: #0a0d14;
    --panel-bg: #141824;
    --card-bg: #1c2233;
    --canvas-bg: #000000;
    --accent-cyan: #00e5ff;
    --accent-gold: #ffd600;
    --accent-orange: #ff9100;
    --accent-green: #00e676;
    --accent-red: #ff1744;
    --text-primary: #f0f4fc;
    --text-muted: #8b95ad;
    --border-color: #273048;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg-dark);
    color: var(--text-primary);
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
    padding: 24px;
    min-height: 100vh;
  }
  header {
    text-align: center;
    margin-bottom: 20px;
  }
  header h1 {
    font-size: 26px;
    font-weight: 700;
    letter-spacing: 0.5px;
    color: var(--accent-cyan);
    margin-bottom: 6px;
  }
  header p {
    font-size: 14px;
    color: var(--text-muted);
  }
  .main-layout {
    display: flex;
    gap: 28px;
    max-width: 1100px;
    width: 100%;
    align-items: flex-start;
  }
  .device-frame {
    background: var(--panel-bg);
    border: 3px solid var(--border-color);
    border-radius: 16px;
    padding: 18px;
    box-shadow: 0 20px 48px rgba(0, 0, 0, 0.7);
    display: flex;
    flex-direction: column;
    align-items: center;
  }
  .device-header {
    width: 100%;
    display: flex;
    justify-content: space-between;
    align-items: center;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.5px;
    color: var(--text-muted);
    margin-bottom: 12px;
    padding: 0 4px;
  }
  .bezel {
    border: 5px solid #000;
    border-radius: 8px;
    overflow: hidden;
    background: var(--canvas-bg);
    position: relative;
    width: 640px; /* 320 * 2 */
    height: 480px; /* 240 * 2 */
    cursor: crosshair;
    user-select: none;
    box-shadow: inset 0 0 12px rgba(0,0,0,0.8);
  }
  canvas {
    width: 640px;
    height: 480px;
    display: block;
    image-rendering: pixelated;
  }
  .overlay-bar {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 38px;
    background: rgba(10, 15, 25, 0.85);
    backdrop-filter: blur(4px);
    border-bottom: 1px solid var(--border-color);
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 12px;
    z-index: 10;
  }
  .screen-btn {
    background: #1f2738;
    color: var(--text-primary);
    border: 1px solid #37435f;
    border-radius: 4px;
    padding: 4px 10px;
    font-size: 11px;
    font-weight: 700;
    cursor: pointer;
    transition: all 0.15s ease;
  }
  .screen-btn:hover {
    background: #2a354c;
    border-color: var(--accent-cyan);
  }
  .status-badge {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.5px;
    padding: 3px 8px;
    border-radius: 12px;
    background: #1e2638;
    color: var(--accent-cyan);
    border: 1px solid #2d3a54;
  }
  .status-badge.waiting { background: rgba(255, 214, 0, 0.15); color: var(--accent-gold); border-color: rgba(255, 214, 0, 0.4); }
  .status-badge.recognized { background: rgba(0, 230, 118, 0.15); color: var(--accent-green); border-color: rgba(0, 230, 118, 0.4); }
  .status-badge.error { background: rgba(255, 23, 68, 0.15); color: var(--accent-red); border-color: rgba(255, 23, 68, 0.4); }

  .answer-overlay {
    position: absolute;
    bottom: 12px;
    right: 14px;
    background: rgba(0, 230, 118, 0.12);
    border: 2px solid var(--accent-green);
    border-radius: 8px;
    padding: 6px 14px;
    color: var(--accent-green);
    font-family: 'Consolas', 'Courier New', monospace;
    font-size: 26px;
    font-weight: 800;
    text-shadow: 0 0 10px rgba(0, 230, 118, 0.5);
    display: none;
    z-index: 10;
  }
  .sidebar {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .card {
    background: var(--card-bg);
    border: 1px solid var(--border-color);
    border-radius: 12px;
    padding: 16px;
    box-shadow: 0 4px 16px rgba(0,0,0,0.3);
  }
  .card h3 {
    font-size: 14px;
    font-weight: 700;
    color: var(--accent-cyan);
    margin-bottom: 12px;
    text-transform: uppercase;
    letter-spacing: 0.5px;
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .metric-row {
    display: flex;
    justify-content: space-between;
    font-size: 13px;
    padding: 5px 0;
    border-bottom: 1px solid rgba(255, 255, 255, 0.05);
  }
  .metric-label { color: var(--text-muted); }
  .metric-value { font-weight: 600; font-family: monospace; color: var(--text-primary); }
  .metric-value.highlight { color: var(--accent-green); font-weight: 700; }
  .metric-value.warn { color: var(--accent-orange); }

  .expr-display {
    background: #0d111a;
    border: 1px solid #273048;
    border-radius: 8px;
    padding: 12px;
    font-family: 'Consolas', monospace;
    font-size: 18px;
    color: var(--accent-gold);
    text-align: center;
    margin: 8px 0;
    min-height: 44px;
    display: flex;
    align-items: center;
    justify-content: center;
  }
  .result-banner {
    background: rgba(0, 230, 118, 0.08);
    border: 1px solid rgba(0, 230, 118, 0.3);
    border-radius: 8px;
    padding: 12px;
    text-align: center;
    margin-top: 8px;
  }
  .result-banner .val {
    font-size: 28px;
    font-weight: 800;
    color: var(--accent-green);
    font-family: 'Consolas', monospace;
  }
  .result-banner .lbl {
    font-size: 11px;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.5px;
  }

  select {
    width: 100%;
    background: #0d111a;
    border: 1px solid var(--border-color);
    color: var(--text-primary);
    padding: 9px 12px;
    border-radius: 6px;
    font-size: 13px;
    margin-bottom: 10px;
    outline: none;
  }
  select:focus { border-color: var(--accent-cyan); }
  .action-btn {
    width: 100%;
    background: #252e42;
    color: var(--text-primary);
    border: 1px solid #3d4a68;
    border-radius: 6px;
    padding: 10px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s ease;
    margin-bottom: 8px;
  }
  .action-btn:hover {
    background: #2f3a53;
    border-color: var(--accent-cyan);
    color: #fff;
  }
  .action-btn.primary {
    background: linear-gradient(135deg, #00b0ff, #0081cb);
    border: none;
    color: #fff;
  }
  .action-btn.primary:hover {
    filter: brightness(1.15);
  }
  .tokens-list {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    margin-top: 8px;
  }
  .token-chip {
    background: #222b3d;
    border: 1px solid #35425e;
    border-radius: 4px;
    padding: 2px 8px;
    font-size: 12px;
    font-family: monospace;
    color: var(--accent-cyan);
  }
  .token-chip.operator { color: var(--accent-gold); border-color: rgba(255, 214, 0, 0.4); }
  .token-chip.uncertain { color: var(--accent-orange); border-color: rgba(255, 145, 0, 0.5); }
</style>
</head>
<body>

<header>
  <h1>Digital Slate Calculator — Live PC Simulation</h1>
  <p>Hardware-accurate simulation of ESP32 TinyML handwriting recognition, geometry, and calculation engine</p>
</header>

<div class="main-layout">
  <!-- Touchscreen Simulation Frame -->
  <div class="device-frame">
    <div class="device-header">
      <span>GUITION ESP32-2432S028 (CYD 320x240 TFT)</span>
      <span id="coordDisplay">X: 0, Y: 0</span>
    </div>
    <div class="bezel" id="bezel">
      <canvas id="slateCanvas" width="320" height="240"></canvas>
      <div class="overlay-bar">
        <button class="screen-btn" id="btnUndo">UNDO</button>
        <div class="status-badge" id="screenBadge">READY</div>
        <button class="screen-btn" id="btnClear">CLEAR</button>
      </div>
      <div class="answer-overlay" id="answerOverlay">= 0</div>
    </div>
    <div style="margin-top: 12px; font-size: 12px; color: var(--text-muted); text-align: center;">
      Write expression with mouse or stylus, then <strong>underline</strong> with horizontal stroke to trigger answer!
    </div>
  </div>

  <!-- Real-time Diagnostics Sidebar -->
  <div class="sidebar">
    <!-- Expression Engine Card -->
    <div class="card">
      <h3>Expression Recognition & Calculation</h3>
      <div class="expr-display" id="exprDisplay">-- Draw Expression --</div>

      <div class="result-banner" id="resultBanner">
        <div class="lbl">Exact Computed Answer</div>
        <div class="val" id="valAnswer">--</div>
      </div>

      <div style="margin-top: 12px;">
        <div class="metric-row">
          <span class="metric-label">Status:</span>
          <span class="metric-value highlight" id="valStatus">READY</span>
        </div>
        <div class="metric-row">
          <span class="metric-label">Trigger Line:</span>
          <span class="metric-value" id="valTriggerDetected">None</span>
        </div>
        <div class="metric-row">
          <span class="metric-label">Total Strokes:</span>
          <span class="metric-value" id="valStrokeCount">0 / 32</span>
        </div>
        <div class="metric-row">
          <span class="metric-label">Pipeline Latency:</span>
          <span class="metric-value highlight" id="valLatency">--</span>
        </div>
        <div class="metric-row">
          <span class="metric-label">Uncertain Symbols:</span>
          <span class="metric-value" id="valUncertain">0</span>
        </div>
      </div>

      <div style="margin-top: 10px;">
        <span style="font-size: 11px; color: var(--text-muted); text-transform: uppercase;">Segmented Symbols:</span>
        <div class="tokens-list" id="tokensList"></div>
      </div>
    </div>

    <!-- Replay Expression Files -->
    <div class="card">
      <h3>Replay Recorded Expressions</h3>
      <select id="selExpression">
        <option value="">-- Select an expression to replay --</option>
      </select>
      <button class="action-btn primary" id="btnReplay">Replay Selected Expression</button>
      <button class="action-btn" id="btnSaveStroke">Save Drawn Expression to /data</button>
    </div>
  </div>
</div>

<script>
const NATIVE_W = 320;
const NATIVE_H = 240;
const canvas = document.getElementById('slateCanvas');
const ctx = canvas.getContext('2d');

let strokes = []; // Array of arrays of {x, y, t}
let currentStroke = [];
let isPenDown = false;
let latestEvalResult = null;
let animationTimer = null;

function clearCanvas() {
  ctx.fillStyle = '#000000';
  ctx.fillRect(0, 0, NATIVE_W, NATIVE_H);
}

function renderCanvas() {
  clearCanvas();

  const triggerIdx = (latestEvalResult && latestEvalResult.trigger_index !== undefined) 
                     ? latestEvalResult.trigger_index : -1;

  // 1. Draw strokes
  for (let sIdx = 0; sIdx < strokes.length; ++sIdx) {
    const pts = strokes[sIdx];
    if (pts.length < 1) continue;

    if (sIdx === triggerIdx) {
      ctx.strokeStyle = '#00e676'; // Emerald green for trigger underline
      ctx.lineWidth = 3;
    } else {
      ctx.strokeStyle = '#ffd600'; // Warm yellow for handwriting ink
      ctx.lineWidth = 2;
    }
    ctx.lineCap = 'round';
    ctx.lineJoin = 'round';

    ctx.beginPath();
    ctx.moveTo(pts[0].x, pts[0].y);
    for (let i = 1; i < pts.length; ++i) {
      ctx.lineTo(pts[i].x, pts[i].y);
    }
    ctx.stroke();
  }

  // 2. Draw recognized symbol bounding boxes & labels
  if (latestEvalResult && latestEvalResult.symbols) {
    for (const sym of latestEvalResult.symbols) {
      const isUncertain = !sym.is_confident;
      ctx.strokeStyle = isUncertain ? 'rgba(255, 145, 0, 0.8)' : 'rgba(0, 229, 255, 0.4)';
      ctx.lineWidth = 1;
      const bw = sym.xMax - sym.xMin + 1;
      const bh = sym.yMax - sym.yMin + 1;
      ctx.strokeRect(sym.xMin - 1, sym.yMin - 1, bw + 2, bh + 2);

      // Label badge
      ctx.fillStyle = isUncertain ? 'rgba(255, 145, 0, 0.9)' : 'rgba(0, 229, 255, 0.8)';
      ctx.fillRect(sym.xMin - 1, Math.max(0, sym.yMin - 13), 16, 12);
      ctx.fillStyle = '#000000';
      ctx.font = 'bold 9px monospace';
      ctx.fillText(sym.label, sym.xMin + 3, Math.max(9, sym.yMin - 3));
    }
  }

  // 3. Draw on-screen answer beneath trigger line if evaluated
  if (latestEvalResult && latestEvalResult.status === 'EXPRESSION_RECOGNIZED' && latestEvalResult.calc_is_ok) {
    const overlay = document.getElementById('answerOverlay');
    overlay.innerText = '= ' + latestEvalResult.formatted_answer;
    overlay.style.display = 'block';
  } else {
    document.getElementById('answerOverlay').style.display = 'none';
  }
}

function evaluateCurrentStrokes() {
  if (strokes.length === 0) {
    latestEvalResult = null;
    updateUIWithResult(null);
    renderCanvas();
    return;
  }

  const payload = {
    format: "digital_slate_v1",
    stroke_count: strokes.length,
    total_points: strokes.reduce((acc, s) => acc + s.length, 0),
    strokes: strokes.map((s, idx) => ({ stroke_index: idx, point_count: s.length, points: s }))
  };

  fetch('/api/evaluate', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  })
  .then(res => res.json())
  .then(data => {
    latestEvalResult = data;
    updateUIWithResult(data);
    renderCanvas();
  })
  .catch(err => {
    console.error('Evaluation error:', err);
  });
}

function updateUIWithResult(res) {
  const exprDisplay = document.getElementById('exprDisplay');
  const valAnswer = document.getElementById('valAnswer');
  const valStatus = document.getElementById('valStatus');
  const valTriggerDetected = document.getElementById('valTriggerDetected');
  const valStrokeCount = document.getElementById('valStrokeCount');
  const valLatency = document.getElementById('valLatency');
  const valUncertain = document.getElementById('valUncertain');
  const screenBadge = document.getElementById('screenBadge');
  const tokensList = document.getElementById('tokensList');

  valStrokeCount.innerText = `${strokes.length} / 32`;

  if (!res || strokes.length === 0) {
    exprDisplay.innerText = '-- Draw Expression --';
    valAnswer.innerText = '--';
    valStatus.innerText = 'READY';
    valStatus.className = 'metric-value';
    valTriggerDetected.innerText = 'None';
    valLatency.innerText = '--';
    valUncertain.innerText = '0';
    screenBadge.className = 'status-badge';
    screenBadge.innerText = 'READY';
    tokensList.innerHTML = '';
    return;
  }

  exprDisplay.innerText = res.expression || '(no symbols recognized)';
  valLatency.innerText = `${(res.latency_us / 1000.0).toFixed(2)} ms (${res.latency_us} us)`;
  valUncertain.innerText = res.uncertain_count || 0;

  if (res.status === 'EXPRESSION_RECOGNIZED') {
    valStatus.innerText = 'EXPRESSION RECOGNIZED';
    valStatus.className = 'metric-value highlight';
    valTriggerDetected.innerText = `Stroke #${res.trigger_index} (Valid Underline)`;
    valTriggerDetected.className = 'metric-value highlight';
    screenBadge.className = 'status-badge recognized';
    screenBadge.innerText = 'ANSWER READY';

    if (res.calc_is_ok) {
      valAnswer.innerText = res.formatted_answer;
    } else {
      valAnswer.innerText = 'ERROR';
    }
  } else if (res.status === 'WAITING_FOR_TRIGGER') {
    valStatus.innerText = 'WAITING FOR TRIGGER';
    valStatus.className = 'metric-value warn';
    valTriggerDetected.innerText = 'Awaiting horizontal underline';
    valTriggerDetected.className = 'metric-value warn';
    screenBadge.className = 'status-badge waiting';
    screenBadge.innerText = 'WAITING TRIGGER';
    valAnswer.innerText = '(Underline to calculate)';
  } else if (res.status === 'AMBIGUOUS_TRIGGER') {
    valStatus.innerText = 'AMBIGUOUS TRIGGER';
    valStatus.className = 'metric-value warn';
    valTriggerDetected.innerText = 'Borderline trigger gesture';
    screenBadge.className = 'status-badge waiting';
    screenBadge.innerText = 'AMBIGUOUS';
    valAnswer.innerText = '(Draw clear underline)';
  } else {
    valStatus.innerText = res.status || 'ERROR';
    valStatus.className = 'metric-value';
    screenBadge.className = 'status-badge error';
    screenBadge.innerText = 'ERROR';
    valAnswer.innerText = 'ERROR';
  }

  // Tokens List
  tokensList.innerHTML = '';
  if (res.symbols) {
    for (const sym of res.symbols) {
      const chip = document.createElement('div');
      chip.className = 'token-chip' + (sym.is_operator ? ' operator' : '') + (!sym.is_confident ? ' uncertain' : '');
      chip.innerText = `${sym.label} (${Math.round(sym.conf * 100)}%)`;
      tokensList.appendChild(chip);
    }
  }
}

// Mouse / Stylus Event Handlers
function getCanvasCoords(evt) {
  const rect = canvas.getBoundingClientRect();
  const scaleX = NATIVE_W / rect.width;
  const scaleY = NATIVE_H / rect.height;
  const clientX = evt.touches ? evt.touches[0].clientX : evt.clientX;
  const clientY = evt.touches ? evt.touches[0].clientY : evt.clientY;
  return {
    x: Math.max(0, Math.min(NATIVE_W - 1, Math.round((clientX - rect.left) * scaleX))),
    y: Math.max(0, Math.min(NATIVE_H - 1, Math.round((clientY - rect.top) * scaleY))),
    t: Date.now()
  };
}

function handlePointerDown(evt) {
  evt.preventDefault();
  isPenDown = true;
  const pt = getCanvasCoords(evt);
  currentStroke = [pt];
  strokes.push(currentStroke);
  renderCanvas();
}

function handlePointerMove(evt) {
  const pt = getCanvasCoords(evt);
  document.getElementById('coordDisplay').innerText = `X: ${pt.x}, Y: ${pt.y}`;
  if (!isPenDown) return;
  evt.preventDefault();

  const lastPt = currentStroke[currentStroke.length - 1];
  const dist = Math.hypot(pt.x - lastPt.x, pt.y - lastPt.y);
  if (dist >= 1.5) {
    currentStroke.push(pt);
    renderCanvas();
  }
}

function handlePointerUp(evt) {
  if (!isPenDown) return;
  isPenDown = false;
  evaluateCurrentStrokes();
}

canvas.addEventListener('mousedown', handlePointerDown);
window.addEventListener('mousemove', handlePointerMove);
window.addEventListener('mouseup', handlePointerUp);

canvas.addEventListener('touchstart', handlePointerDown, { passive: false });
window.addEventListener('touchmove', handlePointerMove, { passive: false });
window.addEventListener('touchend', handlePointerUp, { passive: false });

// Controls
document.getElementById('btnClear').addEventListener('click', () => {
  if (animationTimer) clearInterval(animationTimer);
  strokes = [];
  latestEvalResult = null;
  updateUIWithResult(null);
  renderCanvas();
});

document.getElementById('btnUndo').addEventListener('click', () => {
  if (animationTimer) clearInterval(animationTimer);
  if (strokes.length > 0) {
    strokes.pop();
    evaluateCurrentStrokes();
  }
});

// Load expression catalog
fetch('/api/testset')
  .then(res => res.json())
  .then(items => {
    const sel = document.getElementById('selExpression');
    sel.innerHTML = '<option value="">-- Select an expression to replay --</option>';
    for (const it of items) {
      const opt = document.createElement('option');
      opt.value = it.file;
      opt.innerText = `[${it.group}] ${it.name}`;
      sel.appendChild(opt);
    }
  });

document.getElementById('btnReplay').addEventListener('click', () => {
  const sel = document.getElementById('selExpression');
  if (!sel.value) return;
  if (animationTimer) clearInterval(animationTimer);

  fetch('/api/load?file=' + encodeURIComponent(sel.value))
    .then(res => res.json())
    .then(data => {
      strokes = [];
      latestEvalResult = null;
      renderCanvas();

      // Animate strokes writing
      const incomingStrokes = data.strokes.map(s => s.points);
      let strokeIdx = 0;
      let pointIdx = 0;
      let animStrokes = [];

      animationTimer = setInterval(() => {
        if (strokeIdx >= incomingStrokes.length) {
          clearInterval(animationTimer);
          animationTimer = null;
          strokes = incomingStrokes;
          evaluateCurrentStrokes();
          return;
        }

        if (pointIdx === 0) {
          animStrokes.push([]);
        }

        const pts = incomingStrokes[strokeIdx];
        const step = Math.max(1, Math.floor(pts.length / 8));
        const endPt = Math.min(pts.length, pointIdx + step);

        for (let i = pointIdx; i < endPt; ++i) {
          animStrokes[animStrokes.length - 1].push(pts[i]);
        }
        pointIdx = endPt;

        strokes = animStrokes;
        renderCanvas();

        if (pointIdx >= pts.length) {
          strokeIdx++;
          pointIdx = 0;
        }
      }, 25);
    });
});

document.getElementById('btnSaveStroke').addEventListener('click', () => {
  if (strokes.length === 0) {
    alert('Draw an expression first!');
    return;
  }
  const payload = {
    format: "digital_slate_v1",
    stroke_count: strokes.length,
    total_points: strokes.reduce((acc, s) => acc + s.length, 0),
    strokes: strokes.map((s, idx) => ({ stroke_index: idx, point_count: s.length, points: s }))
  };
  fetch('/api/save', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload)
  })
  .then(r => r.json())
  .then(res => {
    alert('Saved stroke recording to: ' + res.path);
  });
});

clearCanvas();
</script>
</body>
</html>
"""

class SlateSimulatorHandler(SimpleHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/" or self.path.startswith("/?"):
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(HTML_TEMPLATE.encode("utf-8"))
        elif self.path == "/api/testset":
            items = []
            # 1. P8 Target expressions
            if P8_EXPR_DIR.exists():
                for f in sorted(P8_EXPR_DIR.glob("*.json")):
                    display_name = f.stem.replace("expr_", "").replace("_", " ")
                    try:
                        with open(f, "r", encoding="utf-8") as jf:
                            d = json.load(jf)
                            if "description" in d:
                                display_name = d["description"]
                    except Exception:
                        pass
                    items.append({"group": "Phase P8 Expression", "file": f"p8:{f.name}", "name": display_name})

            # 2. P4 Testset
            if P4_TESTSET_DIR.exists():
                for f in sorted(P4_TESTSET_DIR.glob("*.json")):
                    items.append({"group": "P4 Geometry", "file": f"p4:{f.name}", "name": f.name})

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(items).encode("utf-8"))

        elif self.path.startswith("/api/load?file="):
            param = self.path.split("file=")[1]
            if param.startswith("p8:"):
                fpath = P8_EXPR_DIR / param[3:]
            elif param.startswith("p4:"):
                fpath = P4_TESTSET_DIR / param[3:]
            else:
                fpath = P8_EXPR_DIR / param

            if fpath.exists():
                with open(fpath, "r", encoding="utf-8") as f:
                    data = f.read()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.end_headers()
                self.wfile.write(data.encode("utf-8"))
            else:
                self.send_response(404)
                self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()

    def do_POST(self):
        if self.path == "/api/evaluate":
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length).decode('utf-8')
            parsed = json.loads(body)

            temp_file = DATA_DIR / "temp_sim_eval.json"
            DATA_DIR.mkdir(parents=True, exist_ok=True)
            with open(temp_file, "w", encoding="utf-8") as f:
                json.dump(parsed, f)

            if HOST_EVAL_EXE.exists():
                try:
                    p = subprocess.run([str(HOST_EVAL_EXE), "--eval", str(temp_file)],
                                       capture_output=True, text=True, timeout=5)
                    stdout = p.stdout.strip()
                    # Parse json from stdout
                    brace_pos = stdout.find("{")
                    if brace_pos != -1:
                        eval_json = stdout[brace_pos:]
                        self.send_response(200)
                        self.send_header("Content-Type", "application/json")
                        self.end_headers()
                        self.wfile.write(eval_json.encode("utf-8"))
                        return
                except Exception as e:
                    print("Eval error:", e)

            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"error": "Evaluation engine failed"}).encode("utf-8"))

        elif self.path == "/api/save":
            content_length = int(self.headers.get('Content-Length', 0))
            body = self.rfile.read(content_length).decode('utf-8')
            parsed = json.loads(body)
            timestamp = int(time.time())
            out_file = DATA_DIR / f"sim_recording_{timestamp}.json"
            DATA_DIR.mkdir(parents=True, exist_ok=True)
            with open(out_file, "w", encoding="utf-8") as f:
                json.dump(parsed, f, indent=2)

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps({"status": "ok", "path": str(out_file)}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

def run_server(port=8080):
    server = HTTPServer(("127.0.0.1", port), SlateSimulatorHandler)
    url = f"http://127.0.0.1:{port}"
    print(f"\n=======================================================")
    print(f"  DIGITAL SLATE CALCULATOR - PC SIMULATOR RUNNING")
    print(f"=======================================================")
    print(f"Simulator Web URL : {url}")
    print(f"Touchscreen Bezel : 320x240 (Scaled 2x on screen)")
    print(f"Pipeline Backend  : host/test_expression.exe (P1..P8)")
    print(f"Press Ctrl+C in this terminal to stop the simulator.")
    print(f"=======================================================\n")
    try:
        webbrowser.open(url)
    except Exception:
        pass
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping simulator.")
        server.server_close()

if __name__ == "__main__":
    run_server(port=8080)
