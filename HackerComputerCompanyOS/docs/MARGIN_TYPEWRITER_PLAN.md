# Margin + typewriter layout plan

## Goal

Fix **typewriter + Letter margins** so the **top margin band** (paper-colored strip and status toast) sits **immediately above the first visible text row**, not glued to the top of the paper rectangle. Keep **per-side margin** fields in layout (partially present today) and align **PDF** with **X11** naming.

## Code areas

### 1. `linux-typewrite-x11/src/main_x11.c`

- Remove remaining **`lay->margin_px`** uses (`ViewLayout` has **`margin_*_px` only**; current code still references `margin_px` in footer, toast, and gutter clamp — **compile break**).
- Add **`typewriter_first_visible_sy(cursor_y, view_rows)`**: first screen row `sy` where **`typewriter_buf_row_for_sy(sy, …) >= 0`**.
- **`render()`**
  - After filling the paper, if **`page_margins && typewriter_mode`**: fill **`[paper_y, text_y0)`** at full paper width with **`outer_bg`** so the old fixed top strip is not stuck paper-colored.
  - After drawing text rows and **before** cursor/rule (so the cursor stays on top): if **`page_margins && typewriter_mode`**: compute **`y_first = text_y0 + first_sy * cell_h`**, **`y0 = max(paper_y, y_first - margin_top_px)`**, then **`fill_rect`** **`paper_bg`** on **`[y0, y_first)`** at full paper width.
- **`draw_toast()`**: extend with **`const TwCore *tw`**, **`page_margins`**, **`typewriter_mode`** (or pass a precomputed band). For **top-band** toasts, baseline uses the **same `y0` / `y_first`** as the floating margin, not **`paper_y` / `text_y0`**. Footer toasts unchanged.
- **`footer_layout`**: use **`margin_right_px`** (not `margin_px`).
- Toast / gutter horizontal insets: **`margin_left_px` / `margin_right_px`** instead of `margin_px`.
- Replace **`toast_top_baseline_y`** with a small **`toast_baseline_in_band(band_top_y, first_text_row_top_y, font)`**, called with either the fixed band (non-typewriter) or the floating band (typewriter).

### 2. `linux-typewrite-x11/src/pdf_export.c`

- **`PdfLayout`**: mirror X11 with **`margin_top_px` … `margin_right_px`** (all equal to today’s single margin for PDF).
- **`pdf_compute_layout`**, footer, gutter clamp: use **`margin_left_px` / `margin_right_px`**. PDF has **no** typewriter transform (unchanged).

### 3. `linux-typewrite/src/tw_paper.h`

- No logic change required for this fix. Optional: remove **`(void)TW_PAPER_HEIGHT_IN`** in `main_x11.c` if you prefer a comment-only reference to nominal page size.

### 4. Build

- **`make -C linux-typewrite-x11 clean all`** (and **`i686` / `portable`** if shipping tarballs).

## Out of scope for this pass

- JSON / settings for **independent** L/R/T/B ( **`TwPaperMargins`** in `tw_paper.h` is the hook; UI stays **symmetric** pixels until configured).
- UEFI parity unless explicitly requested.

## Context

- Document model: **`tw_doc.h`** + **`tw_paper.h`** (pages, lines/cells, nominal Letter, margins as a concept).
