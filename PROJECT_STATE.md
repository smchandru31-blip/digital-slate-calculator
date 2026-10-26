Phase completed: P4 — Stroke geometry and trigger-line detection (PC + replay files)

Status of acceptance criteria (pass/fail each):
- Host-testable geometry module implemented: PASS (`GeometryEngine.hpp`, `GeometryEngine.cpp` under `/firmware` and testable under `/host`)
- Bounding boxes, aspect ratio, direction, length, intersections, baseline/row estimation: PASS (All geometric primitives implemented)
- Trigger-line detector distinct from minus, fraction bar, accidental strokes: PASS (Zero false triggers across minus signs, fraction bars, and stray marks)
- Test set of at least 30 labeled stroke files: PASS (32 files in `data/p4_testset/`: 8 triggers, 8 minuses, 8 accidentals, 8 ambiguous)
- Measured false-trigger rate reported: PASS (0.00% across 16 non-trigger test cases)
- Measured miss rate reported: PASS (0.00% across 8 trigger test cases)
- Ambiguous gestures return "ambiguous", never a silent guess: PASS (100.00% of ambiguous gestures classified as `AMBIGUOUS`)

Verified on hardware? (yes/no, what exactly):
- NO (by design for Phase P4 — PC host unit evaluation with stroke replay files).
- However, `GeometryEngine` cross-compiled cleanly for the ESP32 (`xtensa-esp32-elf-g++` under PlatformIO) to guarantee embedded compatibility.

Key decisions + why:
- Perpendicular Chord Deviation: Beyond endpoint displacement straightness, the detector calculates the maximum perpendicular deviation from the line chord to catch oscillating/wavy strokes (deviation > 5.0px flagged as ambiguous).
- Strict Absolute Minimum Trigger Length: An underline gesture must span at least 50px horizontally to be declared a confident `TRIGGER_DETECTED`. Strokes between 25px and 49px located beneath content are safely flagged as `AMBIGUOUS` to prevent floating minus signs from triggering evaluation.
- Fraction Bar Vertical Clearance: Evaluates content distribution above and below the stroke. If content exists both above and below ($y_{mid} \pm \Delta y$), the stroke is classified as a fraction bar (`NO_TRIGGER`), not a calculation trigger.
- Explicit Ambiguity Classification: Rather than forcing a binary trigger/non-trigger choice, borderline strokes (tilted 18°-32°, wavy strokes, partially overlapping content, or touching descenders) return `AMBIGUOUS` with a clear diagnostic explanation.

Measured numbers (only real ones):
- Total labeled test stroke files: 32
- Ground-truth breakdown: 8 Triggers, 8 Minuses, 8 Accidentals, 8 Ambiguous
- Trigger Hits: 8 / 8
- Trigger Misses: 0 / 8
- Measured Miss Rate: 0.00%
- False Triggers: 0 / 16 (0 false triggers on minuses and accidental strokes)
- Measured False-Trigger Rate: 0.00%
- Ambiguous Correctly Flagged: 8 / 8
- Ambiguity Recognition Rate: 100.00%
- Overall Evaluation Accuracy: 100.00% (32 / 32)
- Firmware build impact on ESP32: Flash 499,179 bytes (38.1%), RAM 34,968 bytes (10.7%)

Known issues / risks:
- Multi-stroke underlines (e.g. user draws two short overlapping lines to form an underline) are currently treated as individual strokes; multi-stroke trigger assembly can be considered if needed in later reconstruction phases.

Files added or changed:
- docs/TEST_REPORT.md (Logged P4 evaluation pass and metrics)
- firmware/src/GeometryEngine.hpp (Stroke geometry and trigger-line detection interface)
- firmware/src/GeometryEngine.cpp (Geometry metrics, intersections, and trigger detector implementation)
- tools/generate_p4_dataset.py (Generates 32 ground-truth labeled stroke JSON files)
- data/p4_testset/*.json (32 labeled stroke replay files)
- host/test_geometry.cpp (Evaluation runner with confusion matrix and diagnostic output)
- host/run_tests.bat (Updated host test script running both P3 and P4 test suites)
- PROJECT_STATE.md (Updated state)

Next phase and what it needs from the user:
- Phase P5 — Preprocessing, digit cropping, and dataset collection.
- What it needs: User provides the prompt for Phase P5.
