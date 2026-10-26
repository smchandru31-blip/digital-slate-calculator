"""
Digital Slate Interactive PC Simulator
Simulates the ESP32-2432S028 touchscreen canvas, stroke engine, and trigger detector on PC.
Runs a local server with zero third-party dependencies (pure standard library).
"""

import os
import sys
import json
import time
import webbrowser
from pathlib import Path
from http.server import HTTPServer, SimpleHTTPRequestHandler

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "data"
TESTSET_DIR = DATA_DIR / "p4_testset"

HTML_TEMPLATE = r"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Digital Slate Calculator - PC Simulator</title>
<style>
  :root {
    --bg-dark: #0f1117;
    --panel-bg: #1a1d27;
    --canvas-bg: #050608;
    --accent-cyan: #00e5ff;
    --accent-gold: #ffd600;
    --accent-orange: #ff9100;
    --accent-green: #00e676;
    --accent-red: #ff1744;
    --text-primary: #f0f2f5;
    --text-muted: #8a90a2;
    --border-color: #2b3042;
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
    font-size: 24px;
    font-weight: 700;
    letter-spacing: 0.5px;
    color: var(--accent-cyan);
    margin-bottom: 4px;
  }
  header p {
    font-size: 13px;
    color: var(--text-muted);
  }
  .main-layout {
    display: flex;
    gap: 24px;
    max-width: 1000px;
    width: 100%;
    align-items: flex-start;
  }
  .device-frame {
    background: var(--panel-bg);
    border: 3px solid var(--border-color);
    border-radius: 14px;
    padding: 16px;
    box-shadow: 0 16px 36px rgba(0, 0, 0, 0.6);
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
    color: var(--text-muted);
    margin-bottom: 10px;
    padding: 0 4px;
  }
  .bezel {
    border: 4px solid #000;
    border-radius: 6px;
    overflow: hidden;
    background: var(--canvas-bg);
    position: relative;
    width: 640px; /* 320 * 2 */
    height: 480px; /* 240 * 2 */
    cursor: crosshair;
    user-select: none;
  }
  canvas {
    width: 640px;
    height: 480px;
    display: block;
  }
  .overlay-bar {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    height: 52px;
    background: rgba(18, 22, 34, 0.92);
    border-bottom: 2px solid var(--border-color);
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0 12px;
    pointer-events: none;
  }
  .screen-btn {
    pointer-events: auto;
    background: #252a3d;
    color: var(--text-primary);
    border: 1px solid #3d4563;
    border-radius: 6px;
    padding: 6px 14px;
    font-size: 12px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.15s ease;
  }
  .screen-btn:hover {
    background: #343c57;
    border-color: var(--accent-cyan);
    color: #fff;
  }
  .screen-btn.active {
    background: var(--accent-cyan);
    color: #000;
  }
  .status-badge {
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.5px;
    padding: 4px 12px;
    border-radius: 12px;
    background: #141824;
    border: 1px solid var(--border-color);
    color: var(--text-muted);
  }
  .status-badge.trigger {
    background: rgba(0, 230, 118, 0.15);
    border-color: var(--accent-green);
    color: var(--accent-green);
  }
  .status-badge.ambiguous {
    background: rgba(255, 145, 0, 0.15);
    border-color: var(--accent-orange);
    color: var(--accent-orange);
  }
  .sidebar {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 16px;
  }
  .card {
    background: var(--panel-bg);
    border: 1px solid var(--border-color);
    border-radius: 10px;
    padding: 16px;
  }
  .card h3 {
    font-size: 14px;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.8px;
    color: var(--accent-cyan);
    margin-bottom: 12px;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .metric-row {
    display: flex;
    justify-content: space-between;
    font-size: 13px;
    padding: 6px 0;
    border-bottom: 1px solid rgba(255, 255, 255, 0.05);
  }
  .metric-row:last-child { border-bottom: none; }
  .metric-label { color: var(--text-muted); }
  .metric-value { font-weight: 600; font-family: monospace; }
  .verdict-box {
    margin-top: 10px;
    padding: 12px;
    border-radius: 8px;
    background: #12151e;
    border-left: 4px solid var(--border-color);
    font-size: 13px;
  }
  .verdict-box.trigger { border-left-color: var(--accent-green); }
  .verdict-box.ambiguous { border-left-color: var(--accent-orange); }
  .verdict-box.no_trigger { border-left-color: var(--text-muted); }
  .verdict-title { font-weight: 700; margin-bottom: 4px; }
  .verdict-desc { color: var(--text-muted); font-size: 12px; }
  .action-btn {
    width: 100%;
    padding: 10px;
    margin-top: 8px;
    background: #23283b;
    border: 1px solid #3a4261;
    border-radius: 6px;
    color: #fff;
    font-weight: 600;
    font-size: 13px;
    cursor: pointer;
    transition: all 0.2s ease;
  }
  .action-btn:hover {
    background: #2f364f;
    border-color: var(--accent-cyan);
  }
  .action-btn.primary {
    background: var(--accent-cyan);
    color: #000;
    border-color: var(--accent-cyan);
  }
  .action-btn.primary:hover {
    background: #33ecff;
  }
  select {
    width: 100%;
    padding: 8px 10px;
    background: #12151e;
    color: #fff;
    border: 1px solid var(--border-color);
    border-radius: 6px;
    font-size: 13px;
    margin-bottom: 8px;
  }
</style>
</head>
<body>

<header>
  <h1>Digital Slate Calculator — PC Simulator</h1>
  <p>Interactive hardware-accurate simulator for ESP32-2432S028 (320x240 native scaled 2x)</p>
</header>

<div class="main-layout">
  <!-- Touchscreen Simulation Frame -->
  <div class="device-frame">
    <div class="device-header">
      <span>GUITION ESP32-2432S028 (CYD)</span>
      <span id="coordDisplay">X: 0, Y: 0</span>
    </div>
    <div class="bezel" id="bezel">
      <canvas id="slateCanvas" width="320" height="240"></canvas>
      <div class="overlay-bar">
        <button class="screen-btn" id="btnUndo">UNDO</button>
        <div class="status-badge" id="screenBadge">READY</div>
        <button class="screen-btn" id="btnClear">CLEAR</button>
      </div>
    </div>
    <div style="margin-top: 10px; font-size: 11px; color: var(--text-muted); text-align: center;">
      Draw expressions with your mouse or stylus, then underline to trigger!
    </div>
  </div>

  <!-- Real-time Diagnostics Sidebar -->
  <div class="sidebar">
    <!-- Trigger Geometry Inspector -->
    <div class="card">
      <h3>Geometry & Trigger Detection</h3>
      <div class="metric-row">
        <span class="metric-label">Strokes Stored:</span>
        <span class="metric-value" id="valStrokeCount">0 / 32</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Total Points:</span>
        <span class="metric-value" id="valPointCount">0 / 1200</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Last Stroke Width:</span>
        <span class="metric-value" id="valCandWidth">0 px</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Angle from Horiz.:</span>
        <span class="metric-value" id="valAngle">0.0°</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Straightness Ratio:</span>
        <span class="metric-value" id="valStraightness">0.00</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Perp. Chord Deviation:</span>
        <span class="metric-value" id="valChordDev">0.0 px</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Baseline Clearance:</span>
        <span class="metric-value" id="valClearance">0 px</span>
      </div>

      <div class="verdict-box" id="verdictBox">
        <div class="verdict-title" id="verdictTitle">NO TRIGGER</div>
        <div class="verdict-desc" id="verdictDesc">Awaiting handwritten expression and underline gesture.</div>
      </div>
    </div>

    <!-- Replay Testset Files -->
    <div class="card">
      <h3>Replay P4 Testset</h3>
      <select id="selTestset">
        <option value="">-- Select a test stroke file to replay --</option>
      </select>
      <button class="action-btn" id="btnReplay">Replay Selected File</button>
      <button class="action-btn primary" id="btnSaveStroke">Save Drawn Stroke to /data</button>
    </div>
  </div>
</div>

<script>
// Canvas dimensions
const NATIVE_W = 320;
const NATIVE_H = 240;
const canvas = document.getElementById('slateCanvas');
const ctx = canvas.getContext('2d');

let strokes = []; // Array of arrays of {x, y, t}
let currentStroke = [];
let isPenDown = false;
let liftTimer = null;
const LIFT_DEBOUNCE_MS = 50;

function clearCanvas() {
  ctx.fillStyle = '#050608';
  ctx.fillRect(0, 0, NATIVE_W, NATIVE_H);
}

function renderAllStrokes(triggerIndex = -1, isAmbiguous = false) {
  clearCanvas();
  for (let sIdx = 0; sIdx < strokes.length; ++sIdx) {
    const pts = strokes[sIdx];
    if (pts.length < 1) continue;

    if (sIdx === triggerIndex) {
      ctx.strokeStyle = isAmbiguous ? '#ff9100' : '#00e676'; // Gold/Green for trigger, Orange for ambiguous
      ctx.lineWidth = 3;
    } else {
      ctx.strokeStyle = '#00e5ff'; // Bright cyan
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
}

function analyzeGeometry() {
  const countSpan = document.getElementById('valStrokeCount');
  const ptSpan = document.getElementById('valPointCount');
  const widthSpan = document.getElementById('valCandWidth');
  const angleSpan = document.getElementById('valAngle');
  const straightSpan = document.getElementById('valStraightness');
  const chordSpan = document.getElementById('valChordDev');
  const clearSpan = document.getElementById('valClearance');
  const verdictBox = document.getElementById('verdictBox');
  const verdictTitle = document.getElementById('verdictTitle');
  const verdictDesc = document.getElementById('verdictDesc');
  const screenBadge = document.getElementById('screenBadge');

  let totalPts = 0;
  for (const s of strokes) totalPts += s.length;
  countSpan.innerText = `${strokes.length} / 32`;
  ptSpan.innerText = `${totalPts} / 1200`;

  if (strokes.length < 2) {
    verdictBox.className = 'verdict-box no_trigger';
    verdictTitle.innerText = 'NO TRIGGER';
    verdictDesc.innerText = strokes.length === 0 ? 'Slate is clear.' : 'Need content + underline gesture.';
    screenBadge.className = 'status-badge';
    screenBadge.innerText = 'READY';
    widthSpan.innerText = '0 px';
    angleSpan.innerText = '0.0°';
    straightSpan.innerText = '0.00';
    chordSpan.innerText = '0.0 px';
    clearSpan.innerText = '0 px';
    renderAllStrokes();
    return;
  }

  // Last stroke is candidate
  const candIdx = strokes.length - 1;
  const cand = strokes[candIdx];
  if (cand.length < 2) return;

  // Content bounding box
  let cXmin = 9999, cYmin = 9999, cXmax = -9999, cYmax = -9999;
  for (let i = 0; i < candIdx; ++i) {
    for (const p of strokes[i]) {
      if (p.x < cXmin) cXmin = p.x;
      if (p.x > cXmax) cXmax = p.x;
      if (p.y < cYmin) cYmin = p.y;
      if (p.y > cYmax) cYmax = p.y;
    }
  }

  // Candidate metrics
  let kXmin = 9999, kYmin = 9999, kXmax = -9999, kYmax = -9999;
  let pathLen = 0;
  for (let i = 0; i < cand.length; ++i) {
    const p = cand[i];
    if (p.x < kXmin) kXmin = p.x;
    if (p.x > kXmax) kXmax = p.x;
    if (p.y < kYmin) kYmin = p.y;
    if (p.y > kYmax) kYmax = p.y;
    if (i > 0) {
      const dx = p.x - cand[i-1].x;
      const dy = p.y - cand[i-1].y;
      pathLen += Math.sqrt(dx*dx + dy*dy);
    }
  }

  const candWidth = kXmax - kXmin + 1;
  const candHeight = kYmax - kYmin + 1;
  const dx = cand[cand.length - 1].x - cand[0].x;
  const dy = cand[cand.length - 1].y - cand[0].y;
  const disp = Math.sqrt(dx*dx + dy*dy);
  const straightness = pathLen > 0 ? Math.min(1.0, disp / pathLen) : 0;
  const angleDeg = Math.atan2(dy, dx) * 180 / Math.PI;
  const absAngle = Math.abs(angleDeg);
  const angleFromHoriz = Math.min(absAngle, 180 - absAngle);

  // Perpendicular chord deviation
  let maxChordDev = 0;
  if (disp > 5) {
    const x1 = cand[0].x, y1 = cand[0].y;
    const x2 = cand[cand.length - 1].x, y2 = cand[cand.length - 1].y;
    for (const p of cand) {
      const d = Math.abs((y2 - y1)*p.x - (x2 - x1)*p.y + x2*y1 - y2*x1) / disp;
      if (d > maxChordDev) maxChordDev = d;
    }
  }

  const clearance = kYmin - cYmax;

  widthSpan.innerText = `${candWidth} px`;
  angleSpan.innerText = `${angleFromHoriz.toFixed(1)}°`;
  straightSpan.innerText = straightness.toFixed(2);
  chordSpan.innerText = `${maxChordDev.toFixed(1)} px`;
  clearSpan.innerText = `${clearance} px`;

  // Decision logic matching GeometryEngine.cpp
  let verdict = 'NO_TRIGGER';
  let reason = '';

  if (pathLen < 25 || candWidth < 20) {
    verdict = 'NO_TRIGGER';
    reason = 'Too short (accidental dot or stray stroke)';
  } else if (straightness < 0.65) {
    verdict = 'NO_TRIGGER';
    reason = 'Curved stroke (not a line)';
  } else if (angleFromHoriz > 32) {
    verdict = 'NO_TRIGGER';
    reason = 'Steeply angled stroke (vertical/diagonal)';
  } else if (kYmax <= cYmax && kYmin >= cYmin && candWidth < 45) {
    verdict = 'NO_TRIGGER';
    reason = 'Minus sign inside expression row';
  } else if (angleFromHoriz > 18 && angleFromHoriz <= 32) {
    verdict = 'AMBIGUOUS';
    reason = 'Ambiguous: Tilted horizontal stroke (18-32°)';
  } else if (cYmax - kYmin > 4 && cYmax - kYmin <= (cYmax - cYmin)*0.55) {
    verdict = 'AMBIGUOUS';
    reason = 'Ambiguous: Partially overlaps bottom of written content';
  } else if (maxChordDev > 5.0 || (straightness >= 0.65 && straightness < 0.80)) {
    verdict = 'AMBIGUOUS';
    reason = 'Ambiguous: Stroke is wavy/curled';
  } else if (kYmin >= cYmax - 5 && candWidth < 50) {
    verdict = 'AMBIGUOUS';
    reason = 'Ambiguous: Short line below content (< 50px)';
  } else if (kYmin >= cYmax - 5 && candWidth >= 50 && straightness >= 0.80 && maxChordDev <= 5.0 && angleFromHoriz <= 18) {
    verdict = 'TRIGGER_DETECTED';
    reason = 'Clear horizontal trigger line below content';
  } else {
    verdict = 'NO_TRIGGER';
    reason = 'Does not meet trigger criteria';
  }

  if (verdict === 'TRIGGER_DETECTED') {
    verdictBox.className = 'verdict-box trigger';
    verdictTitle.innerText = 'TRIGGER DETECTED';
    verdictDesc.innerText = reason;
    screenBadge.className = 'status-badge trigger';
    screenBadge.innerText = 'TRIGGER DETECTED';
    renderAllStrokes(candIdx, false);
  } else if (verdict === 'AMBIGUOUS') {
    verdictBox.className = 'verdict-box ambiguous';
    verdictTitle.innerText = 'AMBIGUOUS GESTURE';
    verdictDesc.innerText = reason;
    screenBadge.className = 'status-badge ambiguous';
    screenBadge.innerText = 'AMBIGUOUS';
    renderAllStrokes(candIdx, true);
  } else {
    verdictBox.className = 'verdict-box no_trigger';
    verdictTitle.innerText = 'NO TRIGGER';
    verdictDesc.innerText = reason;
    screenBadge.className = 'status-badge';
    screenBadge.innerText = 'WRITING';
    renderAllStrokes();
  }
}

// Touch & Mouse Handling
function getCanvasCoords(e) {
  const rect = canvas.getBoundingClientRect();
  const clientX = e.touches ? e.touches[0].clientX : e.clientX;
  const clientY = e.touches ? e.touches[0].clientY : e.clientY;
  const scaleX = NATIVE_W / rect.width;
  const scaleY = NATIVE_H / rect.height;
  return {
    x: Math.round((clientX - rect.left) * scaleX),
    y: Math.round((clientY - rect.top) * scaleY)
  };
}

canvas.addEventListener('mousedown', (e) => {
  if (liftTimer) { clearTimeout(liftTimer); liftTimer = null; }
  isPenDown = true;
  const pt = getCanvasCoords(e);
  pt.t = Date.now();
  currentStroke = [pt];
  strokes.push(currentStroke);
  analyzeGeometry();
});

window.addEventListener('mousemove', (e) => {
  const pt = getCanvasCoords(e);
  document.getElementById('coordDisplay').innerText = `X: ${pt.x}, Y: ${pt.y}`;
  if (!isPenDown || currentStroke.length === 0) return;

  const lastPt = currentStroke[currentStroke.length - 1];
  const dx = pt.x - lastPt.x;
  const dy = pt.y - lastPt.y;
  if (dx*dx + dy*dy >= 3) {
    pt.t = Date.now();
    currentStroke.push(pt);
    ctx.strokeStyle = '#00e5ff';
    ctx.lineWidth = 2;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.moveTo(lastPt.x, lastPt.y);
    ctx.lineTo(pt.x, pt.y);
    ctx.stroke();
  }
});

window.addEventListener('mouseup', () => {
  if (!isPenDown) return;
  isPenDown = false;
  liftTimer = setTimeout(() => {
    analyzeGeometry();
    liftTimer = null;
  }, LIFT_DEBOUNCE_MS);
});

// Buttons
document.getElementById('btnClear').addEventListener('click', () => {
  strokes = [];
  currentStroke = [];
  clearCanvas();
  analyzeGeometry();
});

document.getElementById('btnUndo').addEventListener('click', () => {
  if (strokes.length > 0) {
    strokes.pop();
    currentStroke = [];
    analyzeGeometry();
  }
});

// Populate Testset Select
fetch('/api/testset')
  .then(res => res.json())
  .then(files => {
    const sel = document.getElementById('selTestset');
    for (const f of files) {
      const opt = document.createElement('option');
      opt.value = f;
      opt.innerText = f;
      sel.appendChild(opt);
    }
  });

document.getElementById('btnReplay').addEventListener('click', () => {
  const sel = document.getElementById('selTestset');
  if (!sel.value) return;
  fetch('/api/load?file=' + encodeURIComponent(sel.value))
    .then(res => res.json())
    .then(data => {
      strokes = [];
      for (const s of data.strokes) {
        strokes.push(s.points);
      }
      analyzeGeometry();
    });
});

document.getElementById('btnSaveStroke').addEventListener('click', () => {
  if (strokes.length === 0) {
    alert('Draw something first!');
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
            files = sorted([f.name for f in TESTSET_DIR.glob("*.json")]) if TESTSET_DIR.exists() else []
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(json.dumps(files).encode("utf-8"))
        elif self.path.startswith("/api/load?file="):
            fname = self.path.split("file=")[1]
            fpath = TESTSET_DIR / fname
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
        if self.path == "/api/save":
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
