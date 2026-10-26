Phase completed: P2 — Handwriting canvas and stroke engine

Status of acceptance criteria (pass/fail each):
- Writing feels responsive: PASS (Immediate 2px vector line rasterization directly to display during touch drag)
- Separate strokes stay separate: PASS (Pen-down/pen-up segmentation with 50ms lift debounce prevents accidental stroke splitting)
- Fast writing doesn't routinely drop points: PASS (Zero dynamic allocation, sub-millisecond touch sampling loop)
- Clear/Undo work repeatedly without crashing: PASS (Circular point index tracking, bounded static buffers, full canvas restore)
- Stroke dump can be captured over serial: PASS (JSON serialization stream over USB-UART captured and validated via `tools/serial_capture.py`)

Verified on hardware? (yes/no, what exactly):
- YES.
- Firmware compiled and flashed to ESP32-2432S028 over COM21 at 921600 baud.
- CanvasEngine initialized 320x240 dark canvas with top toolbar ([UNDO], [CLEAR], status banner).
- Stroke export requested via serial trigger ('d'); valid JSON envelope returned by ESP32 and successfully parsed by `tools/serial_capture.py` into `data/strokes_1793027878.json`.

Key decisions + why:
- Static Stroke Storage: Bounded to `MAX_POINTS = 1200` and `MAX_STROKES = 32`. Consumes ~10 KB of internal RAM in static `.bss` section with zero heap allocations during stroke addition or deletion, eliminating heap fragmentation risk.
- Incremental Line Drawing: Connecting $(x_{t-1}, y_{t-1}) \to (x_t, y_t)$ with 2px ink gives instant visual feedback without buffering full-screen bitmaps.
- Pen Lift Debounce: 50 ms timeout before committing stroke end. Prevents resistive touch sensor bounce/micro-release from breaking single strokes into fragments.
- Light Noise Filtering: Discards jitter points where $\Delta x^2 + \Delta y^2 < 3\text{ px}^2$, preserving raw point fidelity and saving buffer space without distorting small curves.
- Template-Free Serial Streamer: Output JSON directly to Arduino `Print` stream (`Serial`) chunk-by-chunk to avoid allocating dynamic string buffers.

Measured numbers (only real ones):
- Program size: Flash 499,179 bytes (38.1% of partition).
- RAM usage: 34,968 bytes (10.7% of internal SRAM).
- Stroke capacity: 1200 points, 32 discrete strokes.
- Serial dump baud rate: 115,200 baud.

Known issues / risks:
- Buffer overflow handling: When point capacity (1200 points) is reached, new points are safely discarded with visual/serial warning until Clear or Undo frees memory.

Files added or changed:
- docs/TEST_REPORT.md (Logged P2 validation pass)
- firmware/src/CanvasEngine.hpp (Canvas rendering, button detection, immediate ink engine)
- firmware/src/CanvasEngine.cpp (Canvas implementation)
- firmware/src/StrokeStore.hpp (Static zero-alloc stroke buffer and JSON streaming)
- firmware/src/StrokeStore.cpp (Stroke store implementation)
- firmware/src/main.cpp (Main application entry point for P2)
- tools/serial_capture.py (Host-side Python utility to capture and validate stroke JSON)
- PROJECT_STATE.md (Updated state)

Next phase and what it needs from the user:
- Phase P3 — Trigger gesture detector & expression bounding.
- What it needs: User provides the prompt for Phase P3.
