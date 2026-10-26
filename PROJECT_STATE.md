Phase completed: P0 — Architecture review and plan

Status of acceptance criteria (pass/fail each):
- ARCHITECTURE.md exists: PASS
- Skeleton folders exist (/firmware, /host, /tools, /data, /docs): PASS
- PROJECT_STATE.md initialized: PASS

Verified on hardware? (yes/no, what exactly):
- No (Phase P0 is design & planning only; no hardware code executed).

Key decisions + why:
- Hybrid recognition pipeline selected: Rule-based geometric classification for arithmetic operators (+, -, *, /, parentheses, trigger line) combined with an int8-quantized compact neural network for isolated digits 0–9. This minimizes RAM usage (<30 KB tensor arena) and execution time (<20 ms inference per digit) while operating entirely within standard internal ESP32 SRAM without assuming external PSRAM.
- Stroke-vector representation over full bitmap processing: Real-time user touch is maintained as compact point arrays (x, y, timestamp), eliminating the need for full-resolution 320x240 frame buffers in internal RAM.
- Retained strict progressive phase order P1–P12: Isolates hardware pinout verification (P1) and pure-logic host unit testing (P3, P4) before on-device TinyML integration.

Measured numbers (only real ones):
- None (pure architecture phase; no benchmarked execution yet).

Known issues / risks:
- Display controller IC (ILI9341 vs ST7789) and pin mappings vary widely across CYD manufacturing revisions. Must be empirically verified via diagnostic firmware in P1.
- Resistive touch jitter and phantom lifts require robust filtering (median filtering + pressure thresholding) to avoid false stroke breaks.
- True trigger lines must be rigorously distinguished from minus signs and fraction bars using aspect ratio and vertical offset heuristics.

Files added or changed:
- GEMINI.md (Workspace constitution)
- docs/ARCHITECTURE.md (System architecture, risk mitigations, pipeline design)
- docs/HARDWARE.md (Hardware baseline & unknown registry for P1)
- docs/TEST_REPORT.md (Test tracking log)
- .gitignore
- firmware/.gitkeep
- host/.gitkeep
- tools/.gitkeep
- data/.gitkeep
- PROJECT_STATE.md (Project tracking state initialized)

Next phase and what it needs from the user:
- Phase P1 — Hardware diagnostic & pinout verification.
- User input needed:
  1. Board silkscreen text (e.g. `ESP32-2432S028R` or similar markings on back).
  2. Chip markings on ESP32 module can.
  3. Visual photos of front/back and ribbon connector if available.
  4. Platform preference / environment availability for flashing (PlatformIO vs Arduino IDE) and ability to capture serial monitor output at 115200 baud.
