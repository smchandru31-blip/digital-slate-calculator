Phase completed: P9 — Vertical (column) expression reconstruction

Status of acceptance criteria (pass/fail each):
- Row grouping and operand grouping: PASS (Clustering digits into top and bottom operands via vertical mid-point thresholding and stroke metric projection)
- Place-value column alignment (right-aligned digits): PASS (Validating units place right alignment within tolerance, verifying multi-digit place value alignment)
- Explicit operator detection (+, -): PASS (Explicit '+' or '-' detected on the left of row 1; subtraction supported only when explicit minus is present)
- Conservative operator inference (+ only, never -): PASS (Addition inferred only when strict right-alignment <= 16px, uniform vertical gap, matching digit heights, and trigger coverage are satisfied)
- Rejection of ambiguous, misaligned, or cramped layouts (no silent guessing): PASS (Zero silent guesses across all test layouts; correctly flagged and rejected AMBIGUOUS_OPERATOR, AMBIGUOUS_PLACE_VALUE, and AMBIGUOUS_CRAMPED_ROWS)
- Document layout rules: PASS (Created comprehensive docs/VERTICAL_LAYOUT_RULES.md with coordinate definitions, tolerances, and ambiguity state protocol)
- Test with varied layouts (23 over +10 = 33, 128 over +47 = 175, 45 over -12 = 33, inferred 23 over 10 = 33, misaligned, cramped, ambiguous): PASS (100% status accuracy, 100% valid evaluation accuracy, 100% ambiguity rejection recall, 0 silent guess violations)

Verified on hardware? (yes/no, what exactly):
- YES on physical ESP32-2432S028 board:
  - Firmware built and flashed via PlatformIO over COM21 (Flash 65.9%, RAM 16.1%, 286,724 bytes free internal heap).
  - ESP32 boot log captured and verified: 2 cores @ 240 MHz, 4096 KB Flash, clean boot with zero heap fragmentation.
  - Interactive dual-mode firmware deployed supporting seamless auto-detection of horizontal expressions and vertical column layouts with trigger underline computation.

Key decisions + why:
- Zero Silent Guessing Policy: Per the Project Constitution, two stacked numbers are never blindly assumed to be addition. Addition is inferred only when all spatial alignment criteria are strictly satisfied; subtraction is never inferred without an explicit minus. Any layout defect prompts the user rather than risking wrong answers.
- Shared Symbol Pipeline: Reused the hybrid segmentation and SlateNet-CNN classifier from ExpressionEngine, ensuring unified single-digit classification and multi-stroke digit merging across both horizontal and vertical modes.
- Top-Bar Protection for Multi-Stroke Digits: Disambiguating horizontal strokes by inspecting vertical proximity to underlying strokes prevents top bars of digits '5' and '7' from being falsely consumed as arithmetic minus signs.
- Right-Aligned Units Anchor: Column place values are referenced to the units place ($10^0$) at the rightmost bounding edge. Leftward digits are mapped into increasing powers of ten ($10^1, 10^2, \dots$) with internal column alignment checks.

Measured numbers (only real ones):
- Total Vertical Layout Fixtures Evaluated: 7
- Layout Status Classification Accuracy: 7 / 7 (100.00%)
- Valid Layout Evaluation Accuracy: 4 / 4 (100.00%)
- Ambiguity Rejection Recall: 3 / 3 (100.00%)
- Silent Guess Violations: 0 (Requirement: exactly 0)
- Average Host Processing Latency: 2.15 ms (2,152.86 us)
- ESP32 Static Firmware RAM: 52,660 bytes (16.1% of 327,680 bytes)
- ESP32 Program Flash Usage: 863,623 bytes (65.9% of 1,310,720 bytes)
- Free Internal Heap at Boot: 286,724 bytes
- All 5 Host Testsuites Passing: CalcEngine 35/35, Geometry 32/32, Classifier 295/295, ExpressionEngine 5/5, VerticalEngine 7/7 (100% pass)

Known issues / risks:
- Multi-row addition ($\ge 3$ stacked rows) is currently treated conservatively; 2 operands are fully supported.
- Final user-interface styling, prompt overlays for ambiguity resolution, and full standalone experience will be completed in Phase P10.

Files added or changed:
- docs/VERTICAL_LAYOUT_RULES.md (New: full specification of vertical layout geometry, tolerances, inference rules, and ambiguity protocol)
- firmware/src/VerticalEngine.hpp (New: VerticalEngine class, VerticalStatus enum, VerticalRow and VerticalResult structures)
- firmware/src/VerticalEngine.cpp (New: row clustering, place-value alignment check, conservative inference, trigger line handling)
- firmware/src/ExpressionEngine.hpp (Updated: exposed segmentAndRecognizeSymbols for module reuse)
- firmware/src/ExpressionEngine.cpp (Updated: straightness requirement for '+', digit top-bar preservation against '-' collision, tightened multi-stroke proximity)
- tools/build_p9_fixtures.py (New: test fixture generator for 7 column layout expressions from real stylus samples)
- data/p9_vertical/*.json (New: 7 validated vertical expression fixtures)
- host/test_vertical.cpp (New: unit and metric evaluation test harness for vertical layouts)
- host/run_tests.bat (Updated: added test suite [5/5] for VerticalEngine)
- firmware/src/main.cpp (Updated: integrated VerticalEngine alongside ExpressionEngine with real-time UI banners and Serial telemetry)
- docs/TEST_REPORT.md (Updated: added Phase P9 section with measured metrics and zero silent guess proof)
- PROJECT_STATE.md (Updated: marked Phase P9 complete)

Next phase and what it needs from the user:
- Phase P10 — UI Polish, Integration, and Standalone Operation.
- What it needs: User provides the prompt for Phase P10.
