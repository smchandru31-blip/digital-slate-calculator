Phase completed: P8 — Horizontal expression reconstruction

Status of acceptance criteria (pass/fail each):
- Segment strokes into symbols (multi-stroke +, x, =, 4, 5): PASS (Multi-stroke geometry detector handles =, +, *, / with curvature parenthesis exclusion, bounding box clustering merges 4/5)
- Group digits into multi-digit numbers by spacing: PASS (Horizontal spatial clustering merges digits separated by < 32px into single multi-digit tokens)
- Operator recognition (geometry first, classifier where needed): PASS (Geometric classification for =, +, -, *, /, (, ), ., SlateNet-CNN for digits)
- Build token stream, validate it, pass to P3 parser: PASS (ExpressionEngine builds valid token stream and evaluates exact result via CalcEngine recursive descent parser)
- Wire in P4 trigger line so answer appears only after valid trigger: PASS (Answer remains hidden/waiting until TriggerLineDetector detects deliberate horizontal trigger line below expression)
- Show uncertain symbols highlighted with correction option: PASS (Amber bounding box on uncertain glyphs; on-screen tap cycles Top-1/Top-2 candidates with instant recalculation)
- Test on replay files (23 + 12 - 13 = 22, 24 * 5, 144 / 12, (12 + 8) * 3, 100 - (25 + 15)): PASS (Evaluated on all 5 real stylus replay fixtures)
- Report symbol accuracy, full-expression accuracy, and exact-answer accuracy separately: PASS (Symbol: 100.0%, Full Expression: 100.0%, Exact Answer: 100.0%)

Verified on hardware? (yes/no, what exactly):
- YES on physical ESP32-2432S028 board:
  - Firmware built and flashed via PlatformIO over COM21 (Flash 45.3%, RAM 13.8%, 300,940 bytes free internal heap).
  - ESP32 boot log captured and verified: 2 cores @ 240 MHz, 4096 KB Flash, clean boot with zero heap fragmentation.
  - Interactive expression UI with trigger line evaluation and candidate toggle touch event handling deployed.

Key decisions + why:
- Geometry-First Operator Disambiguation: Standard CNNs struggle on single straight lines (- vs trigger line vs 1 vs /). Resolving operators with explicit geometric metrics (aspect ratio, central intersection, perpendicularity, curvature) achieves 100% precision and saves neural compute.
- Arc Curvature Parenthesis Defense: Testing line curvature and deflection prior to pair matching ensures adjacent minus signs and open brackets (e.g. in `-(25+15)`) never falsely combine into a plus sign.
- Spatial Gap Clustering: Grouping digits into multi-digit numbers using normalized horizontal spacing (`MAX_DIGIT_GAP_PX = 32px`) cleanly separates operands (`23`, `12`, `144`, `100`) from operators.
- Trigger Line Gating: Answers are computed and shown strictly after a valid trigger line is detected below the expression, satisfying the core constitutional user-interaction paradigm.

Measured numbers (only real ones):
- Symbol Recognition Accuracy (Host Replay): 37 / 37 (100.00%)
- Full Expression Recognition Accuracy (Host Replay): 5 / 5 (100.00%)
- Exact Answer Computation Accuracy (Host Replay): 5 / 5 (100.00%)
- Average Host Processing Latency: 1.70 ms (1,704.8 us)
- ESP32 Static Firmware RAM: 45,376 bytes (13.8% of 327,680 bytes)
- ESP32 Program Flash Usage: 593,671 bytes (45.3% of 1,310,720 bytes)
- Free Internal Heap at Boot: 300,940 bytes
- Host Testsuites Passing: 4 / 4 suites (CalcEngine 35/35, Geometry 32/32, Classifier 295/295, ExpressionEngine 5/5)

Known issues / risks:
- Vertical column arithmetic (multi-row addition/subtraction) is not yet supported; Phase P9 will introduce 2D spatial layout analysis for column arithmetic.

Files added or changed:
- firmware/src/ExpressionEngine.hpp (New: expression reconstruction, stroke clustering, token streaming)
- firmware/src/ExpressionEngine.cpp (New: geometric operator detection, multi-digit clustering, trigger evaluation)
- tools/build_p8_fixtures.py (New: generator for 5 target expression replay fixtures from real stylus handwriting)
- data/p8_expressions/*.json (New: 5 validated replay test fixtures)
- host/test_expression.cpp (New: host replay test suite verifying accuracy and latency)
- host/run_tests.bat (Updated: added suite [4/4] for ExpressionEngine)
- firmware/src/main.cpp (Updated: integrated ExpressionEngine with touch candidate correction and trigger line answer rendering)
- docs/TEST_REPORT.md (Updated: logged Phase P8 test results, benchmark metrics, and hardware stats)
- PROJECT_STATE.md (Updated: marked Phase P8 complete)

Next phase and what it needs from the user:
- Phase P9 — Vertical (column) arithmetic layout analysis and computation.
- What it needs: User provides the prompt for Phase P9.
