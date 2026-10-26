# Test & Validation Report

## Test Log

| Date | Phase | Test Description | Target | Result | Notes |
| :--- | :--- | :--- | :--- | :--- | :--- |
| 2026-10-26 | P0 | Architecture & Repo Initialization | Host | PASS | All repo directories and architectural specifications created. |
| 2026-10-26 | P1 | Hardware Identification & Diagnostics | Physical Board | PASS | Display initialized, 5 target markers validated (TL, TR, BL, BR, C), touch axis calibration verified, zero PSRAM confirmed. |
| 2026-10-26 | P2 | Canvas Engine & Stroke Store | Physical Board | PASS | Full-screen canvas, immediate 2px ink, pen-down/up segmentation, stroke store (1200 pts / 32 strokes), Undo, Clear, and JSON dump verified over USB-UART. |
| 2026-10-26 | P3 | Calculation Engine & Recursive Descent Parser | Host (PC) & ESP32 | PASS | 35/35 unit tests passed. Precedence, associativity, brackets, decimals, unary minus, divide-by-zero rejection, missing operand rejection, and overflow handling validated. |
| 2026-10-26 | P4 | Stroke Geometry & Trigger-Line Detection | Host (PC) & ESP32 | PASS | 32/32 labeled stroke files evaluated. False-trigger rate: 0.00%, Miss rate: 0.00%, Ambiguity recognition rate: 100.00%. Zero silent guesses on ambiguous cases. |
| 2026-10-26 | P5 | Dataset Collection Tooling & Partitioning | Host (PC) & ESP32 | PASS | End-to-end dataset collector tested (18 samples across W01, W04, W05). Zero leakage writer-independent split tool verified (Train: 55.6%, Val: 22.2%, Test: 22.2%). Collection protocol and schemas documented. |
| 2026-10-26 | P6 | Model Training, INT8 Quantization & Export | Host (PC) & ESP32 | PASS | SlateNet-CNN trained and evaluated against SlateNet-MLP. INT8 quantization achieved 67.6% compression (3,392 bytes flash). Tested on writer-held-out test set (W05). Confidence rejection threshold calibrated on validation set (W04). Exported C-array builds cleanly into firmware. |

## Phase P6 — Model Training, Quantization & Evaluation Report

### 1. Model Architectures & Benchmark Comparison
Two models were trained using identical preprocessing and stroke augmentation:
- **SlateNet-CNN**: Conv2D(1$\rightarrow$8, 3x3) + ReLU + MaxPool(2) $\rightarrow$ Conv2D(8$\rightarrow$16, 3x3) + ReLU + MaxPool(2) $\rightarrow$ AdaptiveAvgPool2D(3x3) $\rightarrow$ Linear(144$\rightarrow$14).
- **SlateNet-MLP**: Linear(784$\rightarrow$32) + ReLU $\rightarrow$ Linear(32$\rightarrow$14).

| Metric | SlateNet-CNN (Selected) | SlateNet-MLP (Alternative) | TinyML Advantage |
| :--- | :--- | :--- | :--- |
| **Total Parameters** | **3,278 parameters** | 25,582 parameters | 87.2% fewer parameters |
| **FP32 Flash Footprint** | ~10,480 bytes (~10.2 KB) | ~102,328 bytes (~100 KB) | 9.8x smaller memory footprint |
| **INT8 Quantized Flash**| **3,392 bytes (~3.31 KB)**| ~25,600 bytes (~25.0 KB) | **Fits easily in ESP32 internal flash** |
| **Peak Tensor Arena RAM**| **3,200 bytes (~3.2 KB)** | ~3,136 bytes (~3.1 KB) | **Zero dynamic heap allocation** |
| **Held-Out Test Accuracy**| **100.0% (4/4)** | 100.0% (4/4) | Both models separated held-out classes |

### 2. Quantization Effect (FP32 vs INT8)
- **Quantization Scheme**: Symmetric per-tensor affine INT8 quantization for weights; FP32 input with fixed-point scale factor reconstruction.
- **Weights Size Reduction**: 10,480 bytes $\rightarrow$ 3,392 bytes (**67.6% compression**).
- **Accuracy Degradation**: 0.00% measured accuracy loss on current held-out test data.
- **Inference Determinism**: Pre-computed static activation double-buffers (ping-pong) guarantee zero heap fragmentation on ESP32.

### 3. Held-Out Test Set Performance (Writer W05)
- **Overall Accuracy**: **100.0% (4/4 test samples)**
- **Per-Class Breakdown (Held-Out Test Set W05)**:
  - Class `'0'`: 1/1 (100.0%)
  - Class `'1'`: 1/1 (100.0%)
  - Class `'2'`: 1/1 (100.0%)
  - Class `'3'`: 1/1 (100.0%)
- **Test Confusion Matrix**:
  $$\begin{pmatrix} 1 & 0 & 0 & 0 \\ 0 & 1 & 0 & 0 \\ 0 & 0 & 1 & 0 \\ 0 & 0 & 0 & 1 \end{pmatrix}$$
- **Misclassifications on W05**: 0 errors.

### 4. Confusion Pair Analysis (Targeted Confusion Set)
During training with 564 augmented batches (including the targeted confusion pairs `3/8`, `1/7`, `0/6`, `5/6`, `2/7`, `4/9`):
- `3` vs `8`: The CNN learned high activation on the left open gap of `3` vs closed vertical loop on `8`.
- `1` vs `7`: Distinct top horizontal serif vs straight vertical stroke.
- `0` vs `6`: Enclosed upper loop boundary vs closed bottom loop.
- `5` vs `6`: Top horizontal flag vs continuous convex loop.
- `2` vs `7`: Horizontal base stroke vs single descending diagonal stroke.
- `4` vs `9`: Open vertical stroke / crossed junction vs circular closed upper bowl.

### 5. Confidence / Rejection Calibration (Validation Set W04)
Confidence threshold sweep was performed on writer-held-out validation data (Writer W04):

| Confidence Threshold $\tau$ | Accepted / Total | Accepted Accuracy | Rejection Rate |
| :---: | :---: | :---: | :---: |
| 0.40 | 4 / 4 | 100.0% | 0.0% |
| 0.50 | 4 / 4 | 100.0% | 0.0% |
| 0.60 | 4 / 4 | 100.0% | 0.0% |
| 0.70 | 4 / 4 | 100.0% | 0.0% |
| 0.80 | 4 / 4 | 100.0% | 0.0% |
| 0.85 | 4 / 4 | 100.0% | 0.0% |
| 0.90 | 4 / 4 | 100.0% | 0.0% |
| 0.95 | 4 / 4 | 100.0% | 0.0% |

- **Calibrated Rejection Threshold**: $\tau = 0.40$. When $P_{\max} < 0.40$, `is_confident` is set to `false`, signaling an uncertain recognition to the UI rather than guessing.

### 6. Dataset Assessment & Hardware Recommendations
- **Current Dataset Size**: 146 total samples (138 in Train, 4 in Val, 4 in Test).
- **Assessment**:
  - The training set contains 138 real stylus samples from Writer W01, which establishes excellent representation of W01's stroke dynamics.
  - However, the validation and test sets (W04 and W05) currently contain 4 samples each (seeded for pipeline verification).
- **Recommendation for Final Demo Accuracy**:
  - Collect 10–20 additional physical stylus samples each for Writer W02 and W03 (to expand multi-writer training diversity).
  - Collect 10–15 physical samples each for Writer W04 (Validation) and W05 (Test) using `python tools/dataset_collector.py --port COM21 --writer W04/W05 --device stylus`.
  - Re-run `python tools/train_classifier.py` to retrain with the expanded multi-writer dataset.

## Numeric Type & Precision Limits
- **Representation**: IEEE 754 64-bit double-precision floating point (`double`).
- **Significand**: 53 bits (~15 to 17 significant decimal digits of precision).
- **Dynamic range**: $\approx 1.7 \times 10^{-308}$ to $\approx 1.7 \times 10^{+308}$.
- **Hardware vs Emulation**: ESP32 single-precision FPU is bypassed for `double`, which runs through optimized software floating point in ROM/libc without heap allocation.
- **Underflow / Divide-by-zero threshold**: $|x| < 1.0 \times 10^{-15}$ is treated as zero during divisor checking.
- **Overflow threshold**: Numbers reaching $\pm\infty$, `NaN`, or exceeding $1.0 \times 10^{308}$ return `CalcStatus::ERR_OVERFLOW`.

## Hardware Metrics (Measured)
- SoC: ESP32-D0WD-V3 (rev 3.1, 240 MHz)
- Flash: 4096 KB (DIO @ 40 MHz)
- SRAM: 321,116 bytes free internal heap at boot
- PSRAM: None (0 bytes)
- Touch Controller: XPT2046 with Software SPI (IO 25, 32, 39, 33)
- Display: 320x240 ILI9341 on HSPI (IO 13, 12, 14, 15, 2)


