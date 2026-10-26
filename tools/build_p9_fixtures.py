#!/usr/bin/env python3
"""
Build Vertical Expression Test Fixtures for Phase P9.
Generates realistic column-arithmetic test fixtures from real stylus handwriting (W01):
1. 23 over +10 = 33 (Clean multi-digit addition)
2. 128 over +47 = 175 (Multi-digit place-value column alignment)
3. 45 over -12 = 33 (Vertical subtraction with explicit minus)
4. 23 over 10 = 33 (Conservative addition inference with no operator)
5. 128 over +47 misaligned (Units place shifted under tens/hundreds -> Rejected AMBIGUOUS_PLACE_VALUE)
6. 23 over +10 cramped (Rows vertically collide > 35% -> Rejected AMBIGUOUS_CRAMPED_ROWS)
7. 23 over 10 ambiguous (Stacked without operator, misaligned units & irregular scale -> Rejected AMBIGUOUS_OPERATOR)
"""

import json
import os
import glob
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = REPO_ROOT / "data"
OUT_DIR = DATA_DIR / "p9_vertical"
OUT_DIR.mkdir(parents=True, exist_ok=True)

def load_stroke_sample(digit_char):
    patterns = [
        str(DATA_DIR / f"dataset/raw/W01/{digit_char}_W01_*.json"),
        str(DATA_DIR / f"dataset/splits/**/{digit_char}_W01_*.json"),
        str(DATA_DIR / f"**/{digit_char}_W01_*.json")
    ]
    files = []
    for pat in patterns:
        files = glob.glob(pat, recursive=True)
        if files:
            break
    if not files:
        raise FileNotFoundError(f"No file matching digit {digit_char} in patterns: {patterns}")
    with open(files[0], "r") as f:
        data = json.load(f)
    return data["strokes"]

def normalize_glyph(strokes, target_h=38):
    all_x = [p["x"] for s in strokes for p in s["points"]]
    all_y = [p["y"] for s in strokes for p in s["points"]]
    min_x, max_x = min(all_x), max(all_x)
    min_y, max_y = min(all_y), max(all_y)
    h = max(1, max_y - min_y)
    scale = target_h / h

    norm_strokes = []
    for s in strokes:
        pts = []
        for p in s["points"]:
            pts.append({
                "x": int((p["x"] - min_x) * scale),
                "y": int((p["y"] - min_y) * scale),
                "t": p.get("t", 0)
            })
        norm_strokes.append({"points": pts})
    return norm_strokes

def shift_glyph(strokes, offset_x, offset_y):
    shifted = []
    for s in strokes:
        pts = []
        for p in s["points"]:
            pts.append({
                "x": p["x"] + offset_x,
                "y": p["y"] + offset_y,
                "t": p.get("t", 0)
            })
        shifted.append({"points": pts})
    return shifted

def make_plus_operator(x_center, y_center, size=20):
    half = size // 2
    # Horizontal bar
    pts_h = [{"x": x_center - half + i, "y": y_center, "t": i * 15} for i in range(size + 1)]
    # Vertical bar
    pts_v = [{"x": x_center, "y": y_center - half + i, "t": (size + 1 + i) * 15} for i in range(size + 1)]
    return [{"points": pts_h}, {"points": pts_v}]

def make_minus_operator(x_center, y_center, width=22):
    half = width // 2
    pts = [{"x": x_center - half + i, "y": y_center, "t": i * 15} for i in range(width + 1)]
    return [{"points": pts}]

def make_trigger_line(x_start, x_end, y_pos):
    pts = []
    steps = max(20, (x_end - x_start) // 2)
    for i in range(steps + 1):
        frac = i / steps
        pts.append({
            "x": int(x_start + frac * (x_end - x_start)),
            "y": y_pos,
            "t": i * 10
        })
    return {"points": pts}

def compose_number_row(num_str, x_right, y_top, digit_h=38, digit_gap=6):
    norm_digits = [normalize_glyph(load_stroke_sample(ch), target_h=digit_h) for ch in num_str]
    widths = []
    for nd in norm_digits:
        all_x = [p["x"] for s in nd for p in s["points"]]
        widths.append(max(all_x) - min(all_x) + 1)

    total_w = sum(widths) + (len(widths) - 1) * digit_gap
    cur_x = x_right - total_w

    row_strokes = []
    for i, nd in enumerate(norm_digits):
        row_strokes.extend(shift_glyph(nd, cur_x, y_top))
        cur_x += widths[i] + digit_gap

    return row_strokes, total_w

# --- Fixture 1: 23 over +10 = 33 ---
def build_fixture_1():
    row0, w0 = compose_number_row("23", x_right=180, y_top=40)
    op = make_plus_operator(x_center=95, y_center=98, size=18)
    row1, w1 = compose_number_row("10", x_right=180, y_top=82)
    trig = make_trigger_line(x_start=80, x_end=195, y_pos=130)

    strokes = row0 + op + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "23 over +10 = 33",
        "expected_status": "VERTICAL_OK",
        "expected_expression": "23+10",
        "expected_value": 33.0,
        "is_inferred": False,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_01_23_over_plus_10.json", "w") as f:
        json.dump(out, f, indent=2)

# --- Fixture 2: 128 over +47 = 175 (Multi-digit column place values) ---
def build_fixture_2():
    row0, w0 = compose_number_row("128", x_right=200, y_top=38)
    op = make_plus_operator(x_center=110, y_center=98, size=18)
    row1, w1 = compose_number_row("47", x_right=198, y_top=82)
    trig = make_trigger_line(x_start=95, x_end=215, y_pos=130)

    strokes = row0 + op + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "128 over +47 = 175",
        "expected_status": "VERTICAL_OK",
        "expected_expression": "128+47",
        "expected_value": 175.0,
        "is_inferred": False,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_02_128_over_plus_47.json", "w") as f:
        json.dump(out, f, indent=2)

# --- Fixture 3: 45 over -12 = 33 (Vertical subtraction) ---
def build_fixture_3():
    row0, w0 = compose_number_row("45", x_right=180, y_top=40)
    op = make_minus_operator(x_center=95, y_center=100, width=22)
    row1, w1 = compose_number_row("12", x_right=180, y_top=82)
    trig = make_trigger_line(x_start=80, x_end=195, y_pos=130)

    strokes = row0 + op + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "45 over -12 = 33",
        "expected_status": "VERTICAL_OK",
        "expected_expression": "45-12",
        "expected_value": 33.0,
        "is_inferred": False,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_03_45_over_minus_12.json", "w") as f:
        json.dump(out, f, indent=2)

# --- Fixture 4: 23 over 10 = 33 (Inferred addition with no operator) ---
def build_fixture_4():
    row0, w0 = compose_number_row("23", x_right=180, y_top=40)
    row1, w1 = compose_number_row("10", x_right=182, y_top=84)
    trig = make_trigger_line(x_start=125, x_end=195, y_pos=132)

    strokes = row0 + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "23 over 10 = 33 (Inferred +)",
        "expected_status": "VERTICAL_OK",
        "expected_expression": "23+10",
        "expected_value": 33.0,
        "is_inferred": True,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_04_inferred_addition_23_over_10.json", "w") as f:
        json.dump(out, f, indent=2)

# --- Fixture 5: Misaligned place value (47 left-aligned under 128) -> AMBIGUOUS_PLACE_VALUE ---
def build_fixture_5():
    row0, w0 = compose_number_row("128", x_right=200, y_top=38)
    op = make_plus_operator(x_center=80, y_center=98, size=18)
    # Left-aligned: x_right is only 165 instead of 200 (shifted by 35px, so 7 lands under 2)
    row1, w1 = compose_number_row("47", x_right=165, y_top=82)
    trig = make_trigger_line(x_start=70, x_end=215, y_pos=130)

    strokes = row0 + op + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "128 over +47 (Misaligned Place Value)",
        "expected_status": "AMBIGUOUS_PLACE_VALUE",
        "expected_expression": "",
        "expected_value": 0.0,
        "is_inferred": False,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_05_misaligned_place_value.json", "w") as f:
        json.dump(out, f, indent=2)

# --- Fixture 6: Cramped/overlapping rows -> AMBIGUOUS_CRAMPED_ROWS ---
def build_fixture_6():
    row0, w0 = compose_number_row("23", x_right=180, y_top=40)
    op = make_plus_operator(x_center=115, y_center=85, size=18)
    # Row 1 is placed at y_top=66, overlapping with Row 0 (which ends at y=78) by 12px
    row1, w1 = compose_number_row("10", x_right=180, y_top=66)
    trig = make_trigger_line(x_start=100, x_end=195, y_pos=118)

    strokes = row0 + op + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "23 over +10 (Cramped / Overlapping Rows)",
        "expected_status": "AMBIGUOUS_CRAMPED_ROWS",
        "expected_expression": "",
        "expected_value": 0.0,
        "is_inferred": False,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_06_cramped_rows.json", "w") as f:
        json.dump(out, f, indent=2)

# --- Fixture 7: Ambiguous operator stacked -> AMBIGUOUS_OPERATOR (Prompt User) ---
def build_fixture_7():
    # Row 0: large height (46px), Row 1: small height (24px) - unequal scale ratio = 1.91
    # Right-aligned units (x_right=180 for both), missing operator, trigger covers operand
    row0, w0 = compose_number_row("23", x_right=180, y_top=35, digit_h=46)
    row1, w1 = compose_number_row("10", x_right=180, y_top=90, digit_h=24)
    trig = make_trigger_line(x_start=100, x_end=195, y_pos=128)

    strokes = row0 + row1 + [trig]
    out = {
        "format": "digital_slate_v1",
        "description": "23 over 10 (Ambiguous Operator / Ask User)",
        "expected_status": "AMBIGUOUS_OPERATOR",
        "expected_expression": "",
        "expected_value": 0.0,
        "is_inferred": False,
        "stroke_count": len(strokes),
        "strokes": [{"stroke_index": i, "points": s["points"]} for i, s in enumerate(strokes)]
    }
    with open(OUT_DIR / "vert_07_ambiguous_operator.json", "w") as f:
        json.dump(out, f, indent=2)

def main():
    print(f"Generating Phase P9 Vertical Expression fixtures into {OUT_DIR}...")
    build_fixture_1()
    build_fixture_2()
    build_fixture_3()
    build_fixture_4()
    build_fixture_5()
    build_fixture_6()
    build_fixture_7()
    print("Done! Generated 7 vertical expression fixtures.")

if __name__ == "__main__":
    main()
