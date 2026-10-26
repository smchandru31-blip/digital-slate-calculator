"""
Dataset Collector Tool for Digital Slate Calculator
Connects over USB-UART to the ESP32 (or receives synthetic/simulator input),
prompts for writer metadata, collects labeled handwriting samples,
and saves them to /data/dataset/raw/<writer_id>/.
"""

import sys
import os
import time
import json
import argparse
from pathlib import Path
from datetime import datetime, timezone

try:
    import serial
except ImportError:
    serial = None

REPO_ROOT = Path(__file__).resolve().parent.parent
RAW_DATASET_DIR = REPO_ROOT / "data" / "dataset" / "raw"

CLASSES = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+", "-", "*", "/"]

def compute_bounding_box(strokes):
    x_min, y_min = 9999, 9999
    x_max, y_max = -9999, -9999
    for s in strokes:
        for p in s.get("points", []):
            x = p.get("x", 0)
            y = p.get("y", 0)
            if x < x_min: x_min = x
            if x > x_max: x_max = x
            if y < y_min: y_min = y
            if y > y_max: y_max = y
    if x_min == 9999:
        return {"x_min": 0, "y_min": 0, "x_max": 0, "y_max": 0, "width": 0, "height": 0}
    return {
        "x_min": x_min,
        "y_min": y_min,
        "x_max": x_max,
        "y_max": y_max,
        "width": x_max - x_min + 1,
        "height": y_max - y_min + 1
    }

def save_sample(sample_doc, base_dir=RAW_DATASET_DIR):
    writer_id = sample_doc.get("writer_id", "W01")
    label = sample_doc.get("label", "unknown")
    timestamp = sample_doc.get("timestamp_ms", int(time.time() * 1000))

    # Clean label string for filesystem
    label_clean = label
    if label == "+": label_clean = "plus"
    elif label == "-": label_clean = "minus"
    elif label == "*": label_clean = "mul"
    elif label == "/": label_clean = "div"

    writer_folder = base_dir / writer_id
    writer_folder.mkdir(parents=True, exist_ok=True)

    filename = f"{label_clean}_{writer_id}_{timestamp}.json"
    filepath = writer_folder / filename

    # Enrich metadata
    if "bbox" not in sample_doc:
        sample_doc["bbox"] = compute_bounding_box(sample_doc.get("strokes", []))
    sample_doc["saved_at_iso"] = datetime.now(timezone.utc).isoformat()

    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(sample_doc, f, indent=2)

    return filepath

def print_collection_stats(base_dir=RAW_DATASET_DIR):
    print("\n================ DATASET COLLECTION SUMMARY ================")
    if not base_dir.exists():
        print("No samples collected yet.")
        print("============================================================\n")
        return

    writer_counts = {}
    class_counts = {c: 0 for c in CLASSES}
    total_samples = 0

    for json_file in base_dir.rglob("*.json"):
        try:
            with open(json_file, "r", encoding="utf-8") as f:
                doc = json.load(f)
            w = doc.get("writer_id", "Unknown")
            lbl = doc.get("label", "?")
            writer_counts[w] = writer_counts.get(w, 0) + 1
            if lbl in class_counts:
                class_counts[lbl] += 1
            total_samples += 1
        except Exception:
            continue

    print(f"Total Samples Collected: {total_samples}")
    print("\n--- Samples by Writer ---")
    for w, count in sorted(writer_counts.items()):
        print(f"  Writer {w:6s}: {count:4d} samples")

    print("\n--- Samples by Class ---")
    line = ""
    for idx, c in enumerate(CLASSES):
        line += f"  '{c}': {class_counts[c]:3d} |"
        if (idx + 1) % 4 == 0 or idx == len(CLASSES) - 1:
            print(line)
            line = ""
    print("============================================================\n")

def run_serial_collector(port="COM21", baud=115200, writer="W01", device="stylus"):
    if serial is None:
        print("[ERROR] pyserial is not installed. Install with 'pip install pyserial'")
        return

    print(f"Connecting to {port} @ {baud}...")
    try:
        ser = serial.Serial(port, baud, timeout=1.0)
    except Exception as e:
        print(f"[ERROR] Could not open {port}: {e}")
        return

    time.sleep(1.5)
    ser.reset_input_buffer()

    print(f"Initializing collection mode on device for Writer={writer}, Device={device}...")
    # Send writer and device settings
    ser.write(f"w {writer}\n".encode('utf-8'))
    time.sleep(0.1)
    ser.write(f"d {device}\n".encode('utf-8'))
    time.sleep(0.1)
    ser.write(b"m\n") # Enable collection mode
    ser.flush()

    print("\n[READY] Listening for labeled samples from Digital Slate...")
    print("Draw prompted symbol on board, then tap [SAVE & NEXT] (or press 's'/'n').")
    print("Commands: 's' = Save & Next, 'r' = Redo, 'k' = Skip, 'q' = Quit.\n")

    capturing = False
    json_lines = []

    try:
        while True:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if not line:
                continue

            if "--- BEGIN LABELED SAMPLE ---" in line:
                capturing = True
                json_lines = []
                continue
            elif "--- END LABELED SAMPLE ---" in line:
                capturing = False
                if json_lines:
                    json_str = "\n".join(json_lines).strip()
                    try:
                        sample = json.loads(json_str)
                        path = save_sample(sample)
                        print(f"[SAVED] Sample #{sample.get('sample_index')}: Label='{sample.get('label')}' "
                              f"({sample.get('total_points')} pts) -> {path.name}")
                        print_collection_stats()
                    except json.JSONDecodeError as err:
                        print(f"[ERROR] JSON parse failed: {err}")
                continue
            elif capturing:
                json_lines.append(line)
            else:
                if not line.startswith("---"):
                    print(f"[ESP32] {line}")
    except KeyboardInterrupt:
        print("\nStopping collector.")
    finally:
        ser.close()

def generate_synthetic_samples(count=10, writer="W01", device="stylus"):
    print(f"Generating {count} verified end-to-end labeled samples for Writer={writer}...")
    sample_templates = [
        ("0", [(60, 50), (45, 75), (60, 100), (75, 75), (60, 50)]),
        ("1", [(60, 50), (60, 100)]),
        ("2", [(45, 60), (55, 50), (75, 55), (75, 70), (45, 100), (75, 100)]),
        ("3", [(45, 50), (75, 50), (60, 72), (75, 85), (45, 100)]),
        ("4", [(70, 50), (45, 80), (80, 80), (70, 50), (70, 100)]),
        ("5", [(75, 50), (45, 50), (45, 72), (75, 80), (45, 100)]),
        ("6", [(70, 50), (45, 75), (60, 100), (75, 85), (45, 75)]),
        ("7", [(45, 50), (75, 50), (55, 100)]),
        ("8", [(60, 50), (45, 62), (75, 88), (60, 100), (45, 88), (75, 62), (60, 50)]),
        ("9", [(75, 75), (60, 50), (45, 62), (60, 75), (75, 75), (55, 100)]),
        ("+", [(45, 75), (75, 75)], [(60, 60), (60, 90)]),
        ("-", [(45, 75), (75, 75)])
    ]

    saved_paths = []
    base_time = int(time.time() * 1000)

    for i in range(count):
        idx = i % len(sample_templates)
        tmpl = sample_templates[idx]
        label = tmpl[0]

        strokes_data = []
        t_clock = base_time + (i * 5000)
        total_pts = 0

        # Handle multi-stroke templates like '+'
        stroke_defs = tmpl[1:] if isinstance(tmpl[1], list) and isinstance(tmpl[1][0], tuple) else [tmpl[1]]
        if len(tmpl) > 2 and isinstance(tmpl[2], list):
            stroke_defs = [tmpl[1], tmpl[2]]

        for s_idx, pts in enumerate(stroke_defs):
            s_pts = []
            for p in pts:
                s_pts.append({"x": p[0], "y": p[1], "t": t_clock})
                t_clock += 15
            t_clock += 80
            strokes_data.append({
                "stroke_index": s_idx,
                "point_count": len(s_pts),
                "points": s_pts
            })
            total_pts += len(s_pts)

        doc = {
            "format": "digital_slate_v1",
            "type": "labeled_sample",
            "label": label,
            "prompt": label,
            "sample_index": i + 1,
            "writer_id": writer,
            "input_device": device,
            "timestamp_ms": t_clock,
            "stroke_count": len(strokes_data),
            "total_points": total_pts,
            "strokes": strokes_data
        }

        path = save_sample(doc)
        saved_paths.append(path)
        print(f"[{i+1}/{count}] Saved labeled sample '{label}': {path.name}")

    print_collection_stats()
    return saved_paths

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Digital Slate Dataset Collector")
    parser.add_argument("--port", default="COM21", help="Serial COM port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--writer", default="W01", help="Writer identifier (e.g. W01, W02)")
    parser.add_argument("--device", default="stylus", choices=["stylus", "finger"], help="Input method")
    parser.add_argument("--stats", action="store_true", help="Display dataset collection statistics")
    parser.add_argument("--synthetic", type=int, default=0, help="Generate N verified synthetic samples for pipeline verification")

    args = parser.parse_args()

    if args.stats:
        print_collection_stats()
    elif args.synthetic > 0:
        generate_synthetic_samples(count=args.synthetic, writer=args.writer, device=args.device)
    else:
        run_serial_collector(port=args.port, baud=args.baud, writer=args.writer, device=args.device)
