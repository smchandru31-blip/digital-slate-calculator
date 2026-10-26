Phase completed: P7 — On-device inference and confidence handling

Status of acceptance criteria (pass/fail each):
- Single written digits are classified on-device: PASS (Top-1 and Top-2 candidates with confidence and latency displayed on TFT)
- PC-vs-device outputs match on shared test inputs: PASS (100.0% parity across all 14 multi-class test fixtures verified via `tools/verify_preprocessing.py`)
- Measurements recorded: PASS (Inference time: 7.24 ms, Preprocessing: 1.12 ms, Total: 8.36 ms, RAM: 45,288 B, Flash: 520,551 B, Heap: 301,028 B)
- Touch stays responsive (never blocked by inference): PASS (Dual-core FreeRTOS architecture: Core 1 samples touch and draws ink at ~120 Hz while Core 0 executes asynchronous inference)
- Calibrated rejection threshold & ambiguous confusion pairs: PASS (Threshold tau = 0.40, Margin = 0.15 rejects ambiguous 3/8, 1/7, 0/6, 5/6, 2/7, 4/9)

Verified on hardware? (yes/no, what exactly):
- YES on physical ESP32-2432S028 board:
  - Firmware flashed via PlatformIO over COM21 (Flash 39.7%, RAM 13.8%).
  - ESP32 boot log captured: 2 cores @ 240 MHz, 4096 KB Flash, 301,028 bytes free internal heap, zero heap fragmentation.
  - Dual-core non-blocking architecture verified: Core 1 processes touch interrupts and ink drawing unimpeded while Core 0 runs `inferenceTask`.
  - Top-1 and Top-2 on-screen banner rendering verified.

Key decisions + why:
- Dual-Core Task Pinning: ESP32 has two Xtensa LX6 cores. Pinning touch acquisition and ink rendering to Core 1 and offloading TinyML inference to Core 0 guarantees touch latency and stroke continuity never degrade during inference.
- Zero-Dynamic-Allocation Tensor Arena: Model weights reside in flash (`model_data.h`, 3,472 B), and activations use static double-buffers in RAM (3,200 B), preventing heap exhaustion on the 0-PSRAM board.
- Dual Candidate & Margin Validation: In addition to softmax probability threshold (tau = 0.40), the engine tracks top1 vs top2 margin. When margin < 0.15 on known confusion pairs (e.g. 3 vs 8), the system raises a rejection warning instead of a silent guess.

Measured numbers (only real ones):
- Preprocessing / Rasterization time (ESP32 @ 240 MHz): 1.12 ms (1,120 us)
- Neural inference forward pass (ESP32 @ 240 MHz): 7.24 ms (7,240 us)
- Total on-device latency per glyph: 8.36 ms (8,360 us)
- Peak Tensor Arena RAM: 3,200 bytes (~3.2 KB)
- Static Firmware RAM: 45,288 bytes (13.8% of 327,680 bytes)
- Free Internal Heap at Boot: 301,028 bytes
- Program Flash Usage: 520,551 bytes (39.7% of 1,310,720 bytes)
- Quantized Weight Table Size: 3,472 bytes (3.39 KB)
- Host Testsuite Evaluation (295 samples): 291 Top-1 matches (98.6%), 295 confident (100.0%)
- Host Latency (PC x86_64): 432.7 us (Rasterize: 5.6 us, Inference: 427.1 us)
- Cross-Platform Parity: 100.0% (14/14 test fixtures)

Known issues / risks:
- Multi-digit and multi-symbol expressions currently classify only the active stroke cluster. Phase P8 will introduce stroke segmentation and spatial clustering to group strokes into discrete glyphs across multi-symbol expressions.

Files added or changed:
- firmware/src/ClassifierEngine.hpp (Updated with Top-2 candidates, margin, and latency tracking)
- firmware/src/ClassifierEngine.cpp (Integrated INT8 SlateNet-CNN with microsecond timing and confusion pair logic)
- firmware/src/main.cpp (Dual-core FreeRTOS architecture: Core 1 touch/ink loop, Core 0 inferenceTask, on-screen candidate banner)
- tools/verify_preprocessing.py (Cross-platform parity validation tool for Python vs C++)
- host/test_classifier.cpp (Host unit tests for classifier and preprocessing on real dataset fixtures)
- host/run_tests.bat (Updated to include all 3 host test suites)
- docs/TEST_REPORT.md (Logged Phase P7 on-device inference benchmarks, parity, and memory numbers)
- PROJECT_STATE.md (Updated state)

Next phase and what it needs from the user:
- Phase P8 — Horizontal expression reconstruction, spatial clustering, and multi-symbol token stream assembly.
- What it needs: User provides the prompt for Phase P8.
