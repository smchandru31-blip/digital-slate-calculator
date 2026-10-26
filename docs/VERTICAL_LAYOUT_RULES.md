# Vertical (Column) Expression Layout Rules & Ambiguity Protocol

## 1. Overview
In addition to standard horizontal infix expressions (e.g. `23 + 12 = 35`), the Digital Slate Calculator supports vertical (column-format) arithmetic:
```
    2 3
  + 1 0
  -----  <-- Horizontal trigger line
    3 3  <-- Evaluated result displayed beneath trigger line
```
Because vertical layout relies on 2D spatial arrangement rather than explicit horizontal reading order, strict geometric rules are required to ensure robustness, avoid silent miscalculations, and reject ambiguous or cramped input.

---

## 2. Structural Layout Definitions

### 2.1 Coordinate System & Geometry
- **Display Resolution**: 320 (width, X: 0..319) × 240 (height, Y: 0..239).
- **Trigger Line**: The bottom-most deliberate horizontal stroke ($L_{\text{trigger}}$) beneath all content strokes ($y_{\text{trigger}} > y_{\text{content, max}}$).
- **Content Area**: All strokes above the trigger line ($y < y_{\text{trigger}}$).

### 2.2 Row Grouping
Strokes are segmented into recognized glyphs (digits, operators) using the hybrid segmentation pipeline from Phase P8.
Glyphs are grouped into vertical rows based on their vertical bounding box projection $[y_{\text{min}}, y_{\text{max}}]$ and vertical center $y_{\text{mid}} = \frac{y_{\text{min}} + y_{\text{max}}}{2}$.
- **Row Separation Threshold**: Two rows $R_i$ and $R_{i+1}$ are distinct if their vertical centers are separated by at least $0.65 \times H_{\text{glyph, avg}}$ and the vertical overlap between the bounding boxes does not exceed $25\%$ of row height.
- **Cramped / Overlap Condition**: If vertical bounding boxes overlap by $> 35\%$, rows cannot be reliably separated. The layout is rejected as `AMBIGUOUS_CRAMPED_ROWS`.
- **Supported Row Count**: 2 operands (Row 0: Top Operand, Row 1: Bottom Operand). If $\ge 3$ stacked rows are detected, multi-row column addition is validated or flagged if heights are erratic.

---

## 3. Place-Value & Column Alignment Rules

In handwritten column arithmetic, place values align from the **right** (units / ones place):
```
      Col 2   Col 1   Col 0
    [Hundreds][Tens]  [Units]
               2       3
       +       1       0
```

### 3.1 Right-Alignment (Units Column Anchor)
1. Let $x_{\text{max}}(R_0)$ be the right edge of the rightmost digit of Row 0.
2. Let $x_{\text{max}}(R_1)$ be the right edge of the rightmost digit of Row 1.
3. **Right-Alignment Tolerance**:
   $$\Delta x_{\text{units}} = |x_{\text{max}}(R_0) - x_{\text{max}}(R_1)| \le \max(18\text{ px}, 0.75 \times W_{\text{digit}})$$
   If $\Delta x_{\text{units}}$ exceeds this tolerance, check column placement of individual digits. If the units digit of one row aligns midway between two digits of the other row, reject as `AMBIGUOUS_PLACE_VALUE_ALIGNMENT`.

### 3.2 Multi-Digit Place-Value Mapping (e.g. 128 over +47)
- Digits within each row are grouped into a number by horizontal spacing ($< 32\text{ px}$).
- The rightmost digit of each row represents $10^0$ (units).
- Digits to the left represent $10^1$ (tens), $10^2$ (hundreds), etc.
- Column sanity check: Each digit in Row 1 must have its horizontal center $x_{\text{mid}}$ either:
  1. Roughly vertically aligned with a corresponding place-value digit in Row 0 ($\pm 14\text{ px}$), OR
  2. Shifted strictly leftward for higher place values (e.g., Row 1 having more digits than Row 0).

---

## 4. Operator Detection & Conservative Inference Rules

### 4.1 Explicit Operator Detection
- An operator symbol (`+` or `-`) located in Row 1 (or between Row 0 and Row 1) to the left of all digits ($x_{\text{max}}(\text{op}) < x_{\text{min}}(\text{digits})$) is identified as the explicit operation.
- **Explicit Plus (`+`)**: Detected via perpendicular intersecting bars or cross geometry.
- **Explicit Minus (`-`)**: Detected via single horizontal bar located to the left of the bottom operand.
- Supported operations:
  - Multi-digit Addition (`+`)
  - Multi-digit Subtraction (`-`) — only with explicit `-` operator.

### 4.2 Conservative Operator Inference (No Explicit Operator)
Per the Project Constitution:
> *"Vertical layout: never assume two stacked numbers mean addition. Infer an operator only when layout evidence is sufficient; otherwise ask the user."*

When no explicit operator (`+` or `-`) is drawn to the left:
1. **Inference Conditions for Addition (`+`)**:
   - Both rows contain exclusively valid digits ($0..9$) with zero operator glyphs.
   - Rows are separated by a clean, uniform vertical gap ($4\text{ px} \le \text{gap} \le 30\text{ px}$).
   - The units column is strictly right-aligned ($\Delta x_{\text{units}} \le 16\text{ px}$).
   - Digits in both rows exhibit consistent heights ($\text{ratio} \in [0.70, 1.40]$).
   - The bottom trigger line spans beneath the entire width of both operands ($W_{\text{trigger}} \ge 0.85 \times \max(W_{R0}, W_{R1})$).
2. **Rejection as `AMBIGUOUS_OPERATOR` (Ask User)**:
   - If any of the strict inference conditions fail (e.g., misaligned right margin, unequal digit scales, wide row gap, or ambiguous intent), the engine **REJECTS** automatic calculation and returns `VerticalStatus::AMBIGUOUS_OPERATOR`.
   - The UI displays an amber prompt: `"Confirm Operator: [+] or [-]?"` rather than making a silent guess.
3. **Never Infer Subtraction**: Subtraction is **never** inferred without an explicit minus sign.

---

## 5. Ambiguity & Rejection Protocol

The system returns an explicit status code for all non-ideal layouts:

| Status Code | Description | Action |
| :--- | :--- | :--- |
| `VERTICAL_OK` | Clean vertical layout, unambiguous alignment and operator | Evaluates expression and displays result below trigger |
| `AMBIGUOUS_OPERATOR` | 2 stacked rows with no operator and imperfect alignment | Prompts user to select `[+]` or `[-]` |
| `AMBIGUOUS_PLACE_VALUE` | Units column or internal digit columns misaligned / skewed | Highlights misaligned column; prompts user to rewrite |
| `AMBIGUOUS_CRAMPED_ROWS`| Vertical bounding boxes overlap ($>25\%$) | Highlights overlapping glyphs; prompts user to rewrite |
| `INCONSISTENT_ROW_COUNT`| Stacked content does not form coherent rows | Rejects; shows error banner |
| `OPERATOR_COLLISION` | Operator collides or touches digit strokes | Rejects; flags touching glyphs |

---

## 6. Mathematical Evaluation

Once validated, the vertical layout is mapped to a canonical mathematical expression:
$$\text{Row}_0 \quad \text{Op} \quad \text{Row}_1$$
- E.g. `23` over `+10` $\rightarrow$ `23 + 10` $\rightarrow$ Answer: `33`.
- E.g. `128` over `+47` $\rightarrow$ `128 + 47` $\rightarrow$ Answer: `175`.
- E.g. `45` over `-12` $\rightarrow$ `45 - 12` $\rightarrow$ Answer: `33`.
- E.g. `23` over `10` (unambiguous right alignment) $\rightarrow$ Inferred `23 + 10` $\rightarrow$ Answer: `33`.
The token stream is passed to the P3 [CalcEngine](file:///d:/calculator/firmware/src/CalcEngine.hpp) recursive descent parser for computation.
