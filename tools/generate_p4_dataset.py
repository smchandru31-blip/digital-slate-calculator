"""
Dataset Generator for Phase P4: Stroke Geometry & Trigger-Line Detection
Generates 32 precisely calibrated, ground-truth-labeled stroke files:
- 8 TRIGGER files (clean trigger lines below expressions)
- 8 MINUS files (minus signs inside expressions, unary minus, fraction bars)
- 8 ACCIDENTAL files (taps, palm dots, vertical scratches, diagonal slashes)
- 8 AMBIGUOUS files (tilted lines 22-26 deg, overlapping bottom, wavy strokes, short lines)
"""

import os
import json
import math
from pathlib import Path

def create_stroke(points, start_time=1000, dt=15):
    stroke_pts = []
    t = start_time
    for pt in points:
        stroke_pts.append({"x": int(pt[0]), "y": int(pt[1]), "t": t})
        t += dt
    return stroke_pts

def line_points(x0, y0, x1, y1, steps=10):
    pts = []
    for i in range(steps + 1):
        f = i / float(steps)
        pts.append((x0 + (x1 - x0) * f, y0 + (y1 - y0) * f))
    return pts

def digit_1(x, y):
    return line_points(x, y, x, y + 50, steps=8)

def digit_2(x, y):
    # Arc and base
    pts = [(x, y + 10), (x + 10, y), (x + 25, y + 5), (x + 25, y + 20), (x, y + 50), (x + 25, y + 50)]
    return pts

def digit_3(x, y):
    return [(x, y), (x + 25, y), (x + 12, y + 25), (x + 25, y + 35), (x, y + 50)]

def digit_plus(x, y):
    h = line_points(x, y + 25, x + 25, y + 25, steps=5)
    v = line_points(x + 12, y + 12, x + 12, y + 37, steps=5)
    return [h, v]

def save_dataset_file(folder, filename, label, description, stroke_list):
    strokes_json = []
    total_pts = 0
    t_clock = 500
    for idx, stroke_pts in enumerate(stroke_list):
        s_data = []
        for pt in stroke_pts:
            s_data.append({"x": int(pt[0]), "y": int(pt[1]), "t": t_clock})
            t_clock += 12
        t_clock += 80  # pen-up delay
        strokes_json.append({
            "stroke_index": idx,
            "point_count": len(s_data),
            "points": s_data
        })
        total_pts += len(s_data)

    doc = {
        "format": "digital_slate_v1",
        "label": label,
        "description": description,
        "stroke_count": len(strokes_json),
        "total_points": total_pts,
        "strokes": strokes_json
    }

    filepath = folder / filename
    with open(filepath, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)

def generate_all():
    out_dir = Path(__file__).resolve().parent.parent / "data" / "p4_testset"
    out_dir.mkdir(parents=True, exist_ok=True)
    print(f"Generating 32 labeled test stroke files in {out_dir}...")

    # =========================================================================
    # 1. TRIGGER LINES (8 files) - Expected Verdict: TRIGGER_DETECTED
    # =========================================================================

    # 1.1 "12 + 34" with clean horizontal trigger beneath
    s1 = [digit_1(50, 60), digit_2(70, 60)]
    s1 += digit_plus(105, 60)
    s1 += [digit_3(140, 60), digit_1(175, 60)]
    trigger1 = line_points(45, 130, 205, 132, steps=18)
    save_dataset_file(out_dir, "trigger_01_standard_horizontal.json", "TRIGGER",
                      "Standard expression 12+34 with trigger line beneath", s1 + [trigger1])

    # 1.2 "23 * 4" with long trigger line
    s2 = [digit_2(60, 50), digit_3(95, 50), digit_1(140, 50)]
    trigger2 = line_points(50, 120, 210, 122, steps=20)
    save_dataset_file(out_dir, "trigger_02_long_line.json", "TRIGGER",
                      "Expression with wide trigger line spanning beyond content", s2 + [trigger2])

    # 1.3 Column addition layout: 12 above 34 with bottom sum line
    s3 = [digit_1(80, 40), digit_2(100, 40),  # top: 12
          digit_3(80, 100), digit_1(100, 100)] # bottom: 31
    trigger3 = line_points(70, 160, 135, 160, steps=12) # column line
    save_dataset_file(out_dir, "trigger_03_column_sum_line.json", "TRIGGER",
                      "Column addition format with underline beneath column", s3 + [trigger3])

    # 1.4 Trigger line with slight natural hand tilt (+6 degrees)
    s4 = [digit_1(60, 60), digit_2(80, 60), digit_3(110, 60)]
    trigger4 = line_points(55, 125, 160, 136, steps=15)
    save_dataset_file(out_dir, "trigger_04_slight_tilt_pos.json", "TRIGGER",
                      "Trigger line with natural slight positive tilt (+6 deg)", s4 + [trigger4])

    # 1.5 Trigger line with slight hand tilt (-5 degrees)
    s5 = [digit_2(50, 60), digit_3(80, 60)]
    trigger5 = line_points(45, 135, 130, 127, steps=14)
    save_dataset_file(out_dir, "trigger_05_slight_tilt_neg.json", "TRIGGER",
                      "Trigger line with natural slight negative tilt (-5 deg)", s5 + [trigger5])

    # 1.6 Single digit "7" with horizontal trigger line beneath
    s6 = [line_points(80, 50, 110, 50, steps=6) + line_points(110, 50, 90, 100, steps=8)]
    trigger6 = line_points(60, 120, 130, 120, steps=14)
    save_dataset_file(out_dir, "trigger_06_single_digit.json", "TRIGGER",
                      "Single digit 7 with standard trigger underline", s6 + [trigger6])

    # 1.7 Right-to-left stroke direction (user drew line right-to-left)
    s7 = [digit_1(50, 60), digit_3(80, 60)]
    trigger7 = line_points(130, 130, 45, 130, steps=15) # R to L
    save_dataset_file(out_dir, "trigger_07_right_to_left.json", "TRIGGER",
                      "Trigger line drawn right to left", s7 + [trigger7])

    # 1.8 Generous vertical clearance (drawn 30px below baseline)
    s8 = [digit_2(60, 50), digit_1(90, 50)]
    trigger8 = line_points(50, 140, 130, 140, steps=15)
    save_dataset_file(out_dir, "trigger_08_generous_clearance.json", "TRIGGER",
                      "Trigger line with generous 30px clearance beneath content", s8 + [trigger8])

    # =========================================================================
    # 2. MINUS SIGNS & OPERATORS (8 files) - Expected Verdict: NO_TRIGGER
    # =========================================================================

    # 2.1 Standard binary minus inside expression: "12 - 3"
    s9 = [digit_1(50, 60), digit_2(70, 60)]
    minus1 = line_points(105, 85, 125, 85, steps=5) # minus inside row
    s9_tail = [digit_3(135, 60)]
    save_dataset_file(out_dir, "minus_01_binary_minus.json", "MINUS",
                      "Binary minus sign inside expression 12 - 3", s9 + s9_tail + [minus1])

    # 2.2 Leading unary minus: "-25"
    unary_minus = line_points(40, 85, 58, 85, steps=4)
    s10 = [digit_2(65, 60), digit_3(95, 60)]
    save_dataset_file(out_dir, "minus_02_unary_minus.json", "MINUS",
                      "Leading unary minus preceding digits -23", s10 + [unary_minus])

    # 2.3 Short subtraction bar in "5 - 2"
    s11 = [digit_1(50, 60), digit_2(90, 60)]
    minus3 = line_points(68, 85, 82, 85, steps=3)
    save_dataset_file(out_dir, "minus_03_short_minus.json", "MINUS",
                      "Short subtraction operator bar between 1 and 2", s11 + [minus3])

    # 2.4 Fraction bar: content both above and below: "12 / 34"
    s12_top = [digit_1(80, 40), digit_2(100, 40)]
    s12_bot = [digit_3(80, 120), digit_1(100, 120)]
    frac_bar = line_points(65, 95, 125, 95, steps=12)
    save_dataset_file(out_dir, "minus_04_fraction_bar.json", "MINUS",
                      "Fraction bar with content both above and below", s12_top + s12_bot + [frac_bar])

    # 2.5 Equals sign top bar (first stroke of '=')
    s13 = [digit_1(50, 60), digit_2(75, 60)]
    eq_bar1 = line_points(105, 75, 130, 75, steps=5)
    save_dataset_file(out_dir, "minus_05_equals_bar.json", "MINUS",
                      "Single horizontal bar representing part of equals sign", s13 + [eq_bar1])

    # 2.6 Multi-digit expression with multiple minuses: "10 - 5 - 2"
    s14 = [digit_1(40, 60), digit_2(60, 60), digit_3(110, 60), digit_1(160, 60)]
    minus_mid = line_points(82, 85, 98, 85, steps=4)
    save_dataset_file(out_dir, "minus_06_minus_in_long_expr.json", "MINUS",
                      "Minus sign centered horizontally and vertically in expression", s14 + [minus_mid])

    # 2.7 Minus with slight tilt inside expression row
    s15 = [digit_2(50, 60), digit_3(95, 60)]
    tilted_minus = line_points(70, 82, 88, 87, steps=5)
    save_dataset_file(out_dir, "minus_07_tilted_minus.json", "MINUS",
                      "Minus sign with slight natural tilt inside expression row", s15 + [tilted_minus])

    # 2.8 Decimal point + minus sign: "3.5 - 1"
    s16 = [digit_3(40, 60), [(58, 108), (59, 108)], digit_1(100, 60)]
    minus_dec = line_points(72, 85, 90, 85, steps=4)
    save_dataset_file(out_dir, "minus_08_minus_after_decimal.json", "MINUS",
                      "Minus sign placed between decimal number and next digit", s16 + [minus_dec])

    # =========================================================================
    # 3. ACCIDENTAL STROKES (8 files) - Expected Verdict: NO_TRIGGER
    # =========================================================================

    # 3.1 Accidental stylus tap / dot (3 points)
    s17 = [digit_1(50, 60), digit_2(75, 60)]
    tap_dot = [(110, 130), (111, 131), (110, 131)]
    save_dataset_file(out_dir, "accidental_01_tap_dot.json", "ACCIDENTAL",
                      "Accidental stylus contact dot beneath expression", s17 + [tap_dot])

    # 3.2 Stray vertical scratch
    s18 = [digit_2(50, 60), digit_3(85, 60)]
    scratch_v = line_points(120, 70, 122, 135, steps=8)
    save_dataset_file(out_dir, "accidental_02_vertical_scratch.json", "ACCIDENTAL",
                      "Stray vertical stroke from palm contact or stylus drag", s18 + [scratch_v])

    # 3.3 Diagonal slash / stray mark (45 degrees)
    s19 = [digit_1(50, 60), digit_1(70, 60)]
    slash = line_points(90, 140, 140, 90, steps=10)
    save_dataset_file(out_dir, "accidental_03_diagonal_slash.json", "ACCIDENTAL",
                      "Diagonal slash mark (45 degrees)", s19 + [slash])

    # 3.4 Curved hook / looped scribble
    s20 = [digit_3(50, 60)]
    curl = [(70, 130), (90, 120), (110, 135), (95, 150), (80, 140)]
    save_dataset_file(out_dir, "accidental_04_curved_scribble.json", "ACCIDENTAL",
                      "Curved scribble beneath digit (low straightness)", s20 + [curl])

    # 3.5 Strike-through line cutting across digits
    s21 = [digit_1(50, 60), digit_2(75, 60), digit_3(105, 60)]
    strikethrough = line_points(40, 85, 135, 85, steps=15) # crosses all 3 digits
    save_dataset_file(out_dir, "accidental_05_strikethrough.json", "ACCIDENTAL",
                      "Horizontal line striking directly through digits", s21 + [strikethrough])

    # 3.6 Micro-stroke / jitter (length 8px)
    s22 = [digit_2(50, 60)]
    jitter = line_points(80, 120, 86, 122, steps=3)
    save_dataset_file(out_dir, "accidental_06_micro_jitter.json", "ACCIDENTAL",
                      "Tiny 6px micro-jitter stroke", s22 + [jitter])

    # 3.7 Steep diagonal scratch (60 degrees)
    s23 = [digit_1(50, 60), digit_3(80, 60)]
    steep = line_points(110, 150, 135, 95, steps=8)
    save_dataset_file(out_dir, "accidental_07_steep_scratch.json", "ACCIDENTAL",
                      "Steep diagonal scratch mark", s23 + [steep])

    # 3.8 Tiny tick mark
    s24 = [digit_2(60, 50)]
    tick = [(70, 120), (75, 128), (85, 115)]
    save_dataset_file(out_dir, "accidental_08_tick_mark.json", "ACCIDENTAL",
                      "Checkmark/tick gesture", s24 + [tick])

    # =========================================================================
    # 4. AMBIGUOUS CASES (8 files) - Expected Verdict: AMBIGUOUS
    # =========================================================================

    # 4.1 Significant tilt (24 degrees)
    s25 = [digit_1(50, 60), digit_2(75, 60), digit_3(105, 60)]
    tilt_line = line_points(45, 120, 140, 158, steps=15) # angle ~22-24 deg
    save_dataset_file(out_dir, "ambiguous_01_tilted_24deg.json", "AMBIGUOUS",
                      "Line beneath content with borderline tilt (24 degrees)", s25 + [tilt_line])

    # 4.2 Vertical overlap: line top partially overlaps digit bottom by 10px
    s26 = [digit_1(50, 60), digit_2(75, 60)] # bottom at y=110
    overlap_line = line_points(45, 102, 120, 102, steps=12) # y=102 overlaps by 8px
    save_dataset_file(out_dir, "ambiguous_02_vertical_overlap.json", "AMBIGUOUS",
                      "Line partially overlapping bottom of digits (neither clearly below nor inside)", s26 + [overlap_line])

    # 4.3 Borderline length below content (38px wide below 90px expression)
    s27 = [digit_1(50, 60), digit_2(80, 60), digit_3(110, 60)]
    short_line = line_points(70, 130, 108, 130, steps=8) # 38px
    save_dataset_file(out_dir, "ambiguous_03_borderline_short.json", "AMBIGUOUS",
                      "Short 38px line below content (could be floating minus or short trigger)", s27 + [short_line])

    # 4.4 Wavy line with moderate straightness (0.74)
    s28 = [digit_2(50, 60), digit_3(85, 60)]
    wavy_line = [(45, 130), (65, 122), (85, 138), (105, 124), (125, 136), (145, 130)]
    save_dataset_file(out_dir, "ambiguous_04_wavy_stroke.json", "AMBIGUOUS",
                      "Wavy horizontal stroke below content", s28 + [wavy_line])

    # 4.5 Line slightly touches the bottom pixel of digit 2 (single descender contact)
    s29 = [digit_1(50, 60), digit_2(75, 60)] # digit 2 bottom at (75, 110)
    touching_line = line_points(45, 110, 125, 110, steps=12) # passes through (75, 110)
    save_dataset_file(out_dir, "ambiguous_05_descender_contact.json", "AMBIGUOUS",
                      "Line slightly intersecting bottom edge of digit stroke", s29 + [touching_line])

    # 4.6 Borderline negative tilt (-22 degrees)
    s30 = [digit_2(50, 60), digit_1(80, 60)]
    tilt_neg = line_points(45, 148, 125, 118, steps=12) # ~ -21 deg
    save_dataset_file(out_dir, "ambiguous_06_tilted_neg_22deg.json", "AMBIGUOUS",
                      "Line with borderline negative tilt (-22 degrees)", s30 + [tilt_neg])

    # 4.7 Curled end: straight line with hooked tail
    s31 = [digit_1(50, 60), digit_3(80, 60)]
    hooked_line = line_points(45, 130, 115, 130, steps=10) + [(118, 124), (114, 118), (108, 122)]
    save_dataset_file(out_dir, "ambiguous_07_hooked_line.json", "AMBIGUOUS",
                      "Horizontal line with curled hook at end", s31 + [hooked_line])

    # 4.8 Short line placed just below single digit (32px wide)
    s32 = [digit_3(60, 60)]
    short_below_single = line_points(55, 125, 87, 125, steps=7) # 32px
    save_dataset_file(out_dir, "ambiguous_08_short_below_digit.json", "AMBIGUOUS",
                      "Short line 32px below single digit", s32 + [short_below_single])

    print("Successfully generated all 32 labeled test stroke files.")

if __name__ == "__main__":
    generate_all()
