# Handwriting Dataset Collection Protocol & Specification

## 1. Objective
Establish a rigorous, standardized protocol for collecting handwritten digits (0–9) and arithmetic operators (+, −, ×, ÷) directly from the Guition ESP32-2432S028 2.8" touchscreen board and PC slate simulator. The resulting dataset will be used to train and evaluate the compact quantized TinyML classifier.

---

## 2. Target Classes & Sampling Quotas

| Class Type | Symbols | Target Samples / Class | Notes |
| :--- | :--- | :--- | :--- |
| **Digits** | `0, 1, 2, 3, 4, 5, 6, 7, 8, 9` | 50 samples / class | Standard base dataset |
| **Operators** | `+, −, ×, ÷` | 50 samples / class | Standard arithmetic operators |
| **Confusion Pairs (Targeted)** | `3 / 8`, `1 / 7`, `0 / 6`, `5 / 6`, `2 / 7`, `4 / 9` | +25% extra samples | Specific fine-tuning quota |

- **Minimum Total Target**: $\approx 700$ labeled samples across all classes.

---

## 3. Writer Distribution & Split Strategy

To prevent data leakage and ensure realistic model generalization, all splits must be **strictly writer-independent** (no single writer appears in more than one partition):

| Partition | Writers Assigned | Target Ratio | Purpose |
| :--- | :--- | :--- | :--- |
| **Train Set** | `W01`, `W02`, `W03` | ~60% (~420 samples) | Model weight optimization and loss minimization |
| **Validation Set** | `W04` | ~20% (~140 samples) | Hyperparameter tuning, early stopping, quantization calibration |
| **Test Set** | `W05` | ~20% (~140 samples) | Final unbiased benchmark; never touched during training |

---

## 4. Input Modality Ratio (Stylus vs Finger)

The target hardware is a resistive touch panel (XPT2046) requiring positive point contact:
- **Stylus (80%)**: Primary usage mode. Uses a plastic stylus or pen cap. Produces sharp, localized point streams with high point density.
- **Finger / Fingernail (20%)**: Secondary usage mode. Uses a fingernail or firm finger press. Captures broader, slightly lower precision trajectories and slight contact drift.

---

## 5. Sample Storage & JSON Schema

Each recorded sample is stored in a standalone JSON file under `/data/dataset/raw/<writer_id>/<label>_<writer_id>_<timestamp>.json`:

```json
{
  "format": "digital_slate_v1",
  "type": "labeled_sample",
  "label": "7",
  "prompt": "7",
  "sample_index": 12,
  "writer_id": "W01",
  "input_device": "stylus",
  "timestamp_ms": 1729904512,
  "saved_at_iso": "2026-10-26T22:25:00.000000Z",
  "bbox": {
    "x_min": 45,
    "y_min": 50,
    "x_max": 75,
    "y_max": 100,
    "width": 31,
    "height": 51
  },
  "stroke_count": 1,
  "total_points": 18,
  "strokes": [
    {
      "stroke_index": 0,
      "point_count": 18,
      "points": [
        {"x": 45, "y": 50, "t": 1729904512},
        {"x": 52, "y": 50, "t": 1729904524},
        {"x": 75, "y": 50, "t": 1729904536},
        {"x": 55, "y": 100, "t": 1729904548}
      ]
    }
  ]
}
```

---

## 6. Collection Execution Procedure

### Method A: On-Board Collection Mode (ESP32 via Serial)
1. Flash firmware with collection mode enabled to `COM21`.
2. Launch host collector tool:
   ```cmd
   python tools/dataset_collector.py --port COM21 --writer W01 --device stylus
   ```
3. The board displays: `PROMPT: WRITE '7' [#1] W01-stylus`.
4. Draw the prompted digit or operator on the touchscreen.
5. Tap **`[SAVE & NEXT]`** on the bottom-right of the display (or send `'s'` via serial). The sample is transmitted, saved to `/data/dataset/raw/W01/`, and the board advances to the next symbol.
6. Tap **`[REDO]`** if the stroke was drawn incorrectly, or **`[SKIP]`** to move to the next prompt.

### Method B: PC Slate Simulator
1. Launch simulator: `python tools/simulator.py`.
2. Open browser at `http://127.0.0.1:8080`.
3. Draw strokes on the touchscreen canvas.
4. Click **"Save Drawn Stroke to /data"**.

---

## 7. Automated Dataset Splitting

After collecting raw samples, run the writer-based split script:
```cmd
python tools/split_dataset.py
```
This partitions files from `data/dataset/raw/` into:
- `data/dataset/train/` (W01, W02, W03)
- `data/dataset/val/`   (W04)
- `data/dataset/test/`  (W05)
and outputs a class distribution and balance report.
