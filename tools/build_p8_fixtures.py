#!/usr/bin/env python3
"""
Build Horizontal Expression Test Fixtures for Phase P8.
Composes real stylus stroke recordings from Writer W01 and geometric operators
into full multi-symbol horizontal expressions with trigger lines.
"""

import json
import os
import glob
import math

def load_stroke_sample(pattern):
    files = glob.glob(pattern)
    if not files:
        raise FileNotFoundError(f"No file matching {pattern}")
    with open(files[0], "r") as f:
        data = json.load(f)
    return data["strokes"]

def shift_strokes(strokes, offset_x, offset_y=0):
    shifted = []
    for s in strokes:
        pts = []
        for p in s["points"]:
            pts.append({
                "x": p["x"] + offset_x,
                "y": p["y"] + offset_y,
                "t": p.get("t", 0)
            })
        shifted.append({
            "stroke_index": len(shifted),
            "point_count": len(pts),
            "points": pts
        })
    return shifted

def normalize_glyph_strokes(strokes, target_h=45):
    # Find bounding box
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

def make_parenthesis(is_open=True, x_start=0, y_top=55, h=50, w=14):
    pts = []
    num_pts = 10
    for i in range(num_pts):
        prog = i / (num_pts - 1)
        y = int(y_top + prog * h)
        # Parabolic arc
        # at prog=0 and prog=1, offset=0; at prog=0.5, offset = w
        arc = 4 * prog * (1.0 - prog)
        if is_open:
            # '(' bows to the left (smaller x)
            x = int(x_start + w - arc * w)
        else:
            # ')' bows to the right (larger x)
            x = int(x_start + arc * w)
        pts.append({"x": x, "y": y, "t": int(prog * 150)})
    return [{"points": pts}]

def make_equals(x_start=0, y_mid=80, w=20, gap=10):
    pts1 = [{"x": x_start + int(i * w / 6), "y": y_mid - gap // 2, "t": i * 15} for i in range(7)]
    pts2 = [{"x": x_start + int(i * w / 6), "y": y_mid + gap // 2, "t": 120 + i * 15} for i in range(7)]
    return [{"points": pts1}, {"points": pts2}]

def make_trigger_line(x_start=20, x_end=300, y=140):
    pts = []
    num_pts = 20
    for i in range(num_pts):
        prog = i / (num_pts - 1)
        pts.append({
            "x": int(x_start + prog * (x_end - x_start)),
            "y": int(y + (i % 2)),
            "t": 1000 + i * 15
        })
    return [{"points": pts}]

def build_fixture(components, target_expr, target_val, out_path, desc=""):
    composed_strokes = []
    cur_x = 25
    base_y = 60

    for comp_type, glyph in components:
        if comp_type == "digit":
            raw_s = load_stroke_sample(f"data/dataset/raw/W01/{glyph}_W01_*.json")
            norm_s = normalize_glyph_strokes(raw_s, target_h=42)
            shifted = shift_strokes(norm_s, cur_x, base_y)
            composed_strokes.extend(shifted)
            # Find width
            w = max([p["x"] for s in shifted for p in s["points"]]) - min([p["x"] for s in shifted for p in s["points"]])
            cur_x += w + 12
        elif comp_type == "plus":
            raw_s = load_stroke_sample("data/dataset/raw/W01/plus_W01_*.json")
            norm_s = normalize_glyph_strokes(raw_s, target_h=30)
            shifted = shift_strokes(norm_s, cur_x, base_y + 6)
            composed_strokes.extend(shifted)
            cur_x = max([p["x"] for s in shifted for p in s["points"]]) + 14
        elif comp_type == "minus":
            raw_s = load_stroke_sample("data/dataset/raw/W01/minus_W01_*.json")
            norm_s = normalize_glyph_strokes(raw_s, target_h=6)
            shifted = shift_strokes(norm_s, cur_x, base_y + 18)
            composed_strokes.extend(shifted)
            cur_x = max([p["x"] for s in shifted for p in s["points"]]) + 14
        elif comp_type == "mul":
            raw_s = load_stroke_sample("data/dataset/raw/W01/mul_W01_*.json")
            norm_s = normalize_glyph_strokes(raw_s, target_h=28)
            shifted = shift_strokes(norm_s, cur_x, base_y + 8)
            composed_strokes.extend(shifted)
            cur_x = max([p["x"] for s in shifted for p in s["points"]]) + 14
        elif comp_type == "div":
            raw_s = load_stroke_sample("data/dataset/raw/W01/div_W01_*.json")
            norm_s = normalize_glyph_strokes(raw_s, target_h=30)
            shifted = shift_strokes(norm_s, cur_x, base_y + 6)
            composed_strokes.extend(shifted)
            cur_x = max([p["x"] for s in shifted for p in s["points"]]) + 14
        elif comp_type == "paren_open":
            p_s = make_parenthesis(is_open=True, x_start=cur_x, y_top=base_y - 4, h=50, w=14)
            composed_strokes.extend(p_s)
            cur_x = max([p["x"] for s in p_s for p in s["points"]]) + 14
        elif comp_type == "paren_close":
            p_s = make_parenthesis(is_open=False, x_start=cur_x, y_top=base_y - 4, h=50, w=14)
            composed_strokes.extend(p_s)
            cur_x = max([p["x"] for s in p_s for p in s["points"]]) + 14
        elif comp_type == "equals":
            eq_s = make_equals(x_start=cur_x, y_mid=base_y + 20, w=22, gap=8)
            composed_strokes.extend(eq_s)
            cur_x = max([p["x"] for s in eq_s for p in s["points"]]) + 14

    # Add trigger line beneath content
    all_x = [p["x"] for s in composed_strokes for p in s["points"]]
    all_y = [p["y"] for s in composed_strokes for p in s["points"]]
    trig_x0 = max(15, min(all_x) - 10)
    trig_x1 = min(305, max(all_x) + 10)
    trig_y = max(all_y) + 18

    trig_s = make_trigger_line(x_start=trig_x0, x_end=trig_x1, y=trig_y)
    composed_strokes.extend(trig_s)

    # Re-index strokes
    indexed_strokes = []
    total_pts = 0
    for idx, s in enumerate(composed_strokes):
        pts = s["points"]
        total_pts += len(pts)
        indexed_strokes.append({
            "stroke_index": idx,
            "point_count": len(pts),
            "points": pts
        })

    fixture_doc = {
        "format": "digital_slate_v1",
        "description": desc,
        "target_expression": target_expr,
        "target_value": target_val,
        "stroke_count": len(indexed_strokes),
        "total_points": total_pts,
        "strokes": indexed_strokes
    }

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(fixture_doc, f, indent=2)
    print(f"[OK] Generated {out_path}: '{target_expr}' = {target_val} ({len(indexed_strokes)} strokes, {total_pts} pts)")

def main():
    fixtures = [
        (
            [("digit", "2"), ("digit", "3"), ("plus", "+"), ("digit", "1"), ("digit", "2"),
             ("minus", "-"), ("digit", "1"), ("digit", "3"), ("equals", "=")],
            "23+12-13",
            22.0,
            "data/p8_expressions/expr_01_addition_subtraction.json",
            "23 + 12 - 13 = 22"
        ),
        (
            [("digit", "2"), ("digit", "4"), ("mul", "*"), ("digit", "5")],
            "24*5",
            120.0,
            "data/p8_expressions/expr_02_multiplication.json",
            "24 * 5 = 120"
        ),
        (
            [("digit", "1"), ("digit", "4"), ("digit", "4"), ("div", "/"), ("digit", "1"), ("digit", "2")],
            "144/12",
            12.0,
            "data/p8_expressions/expr_03_division.json",
            "144 / 12 = 12"
        ),
        (
            [("paren_open", "("), ("digit", "1"), ("digit", "2"), ("plus", "+"), ("digit", "8"),
             ("paren_close", ")"), ("mul", "*"), ("digit", "3")],
            "(12+8)*3",
            60.0,
            "data/p8_expressions/expr_04_parentheses_mul.json",
            "(12 + 8) * 3 = 60"
        ),
        (
            [("digit", "1"), ("digit", "0"), ("digit", "0"), ("minus", "-"),
             ("paren_open", "("), ("digit", "2"), ("digit", "5"), ("plus", "+"),
             ("digit", "1"), ("digit", "5"), ("paren_close", ")")],
            "100-(25+15)",
            60.0,
            "data/p8_expressions/expr_05_nested_subtraction.json",
            "100 - (25 + 15) = 60"
        )
    ]

    for comps, expr, val, path, desc in fixtures:
        build_fixture(comps, expr, val, path, desc)

if __name__ == "__main__":
    main()
