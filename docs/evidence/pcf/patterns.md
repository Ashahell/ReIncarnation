# Appendix-D pattern ledger (E1, closed OPEN-04)

**Source:** ReBirth RB-338 Owner's Manual (this PDF copy: ReBirth 1.0
`pdf.book`, FrameMaker 5.5, 232 pages), Appendix D "PCF Pattern Diagrams",
pp. 205–220 (PDF index 205–220). E1 public documentation.
**Method:** mechanical extraction, no hand transcription —
`bbox` (pdftotext) for step-label geometry + pattern titles,
raster column profiles (pdftoppm PGM 150 DPI, stdlib parser) for bar
heights and loop triangles, overlay review (measured boxes drawn back
over the manual pages). Scripts (scratch, rerunnable):
`/home/miller/Work/ri_build/pcf_extract.py` (survey),
`/home/miller/Work/ri_build/pcf_decode.py` (ledger + verify),
`/home/miller/Work/ri_build/pcf_overlay.py` (visual proof).
Machine ledger: `reference/pcf-patterns.bin` (magic PCFP, version 1,
55 rows of res/length/vel[32]); JSON twin `reference/pcf-patterns.json`.

## Census (corrects the review's "54 patterns (0–53)")

55 patterns numbered 0–54. Resolution from the axis label grammar
(mechanical, no section assumptions): 39 two-bar-16 (1–16 twice) → 16th;
15 straight-32 (1–32) → 32nd; 1 quad-8 (1–8 ×4, pattern 53) → 8th.
Assignment: 0–33 → 16th; 34–47 → 32nd; 48–51, 54 → 16th; 52 → 32nd;
53 → 8th. Cross-checked against the section headers (exact agreement).

## Lengths

End-marker tip, rounded to steps. 7 patterns carry no end marker
(0, 1, 6, 21, 34, 42, 44 — visually verified empty bands): loop runs
the full displayed width (32), flagged here for ReBirth behavior check.
Text cross-checks: pattern 3 = 12 steps (manual prose p.206),
pattern 40 = 28 steps (prose p.215). No hit at/after any end marker
(mechanical check over all 55).

## Velocities

Bar height over the global max (pattern 44 step 1, 184 px) × 127.
773 hits; the common full height (179 px, 213 hits) lands at 124.
Ghost notes resolve (e.g. pattern 40's 8 px ticks). One 2 px speck
(pattern 15 step 1) kept as velocity 1 — visible on overlay, honest
to the source. No attack markings found in any diagram (attack stays
the fast default; review's "attack flag where marked" finds nothing).
No y-axis exists in the diagrams: absolute (not per-pattern) scaling
is the documented reading (single drawing routine); pattern 44's
tallest bar anchors 127.

## Per-pattern table (res, length, hits, maxvel)

| pat | res | len | hits | maxv | pat | res | len | hits | maxv |
|-----|-----|-----|------|------|-----|-----|-----|------|------|
| 0 | 16th | 32 | 1 | 124 | 28 | 16th | 16 | 8 | 121 |
| 1 | 16th | 32 | 2 | 124 | 29 | 16th | 16 | 5 | 117 |
| 2 | 16th | 16 | 4 | 124 | 30 | 16th | 16 | 8 | 115 |
| 3 | 16th | 12 | 4 | 124 | 31 | 16th | 16 | 8 | 121 |
| 4 | 16th | 16 | 8 | 124 | 32 | 16th | 27 | 18 | 117 |
| 5 | 16th | 16 | 14 | 124 | 33 | 16th | 10 | 5 | 93 |
| 6 | 16th | 32 | 6 | 124 | 34 | 32nd | 32 | 21 | 124 |
| 7 | 16th | 16 | 8 | 124 | 35 | 32nd | 32 | 28 | 124 |
| 8 | 16th | 16 | 6 | 124 | 36 | 32nd | 32 | 28 | 124 |
| 9 | 16th | 16 | 6 | 124 | 37 | 32nd | 32 | 28 | 124 |
| 10 | 16th | 16 | 8 | 124 | 38 | 32nd | 32 | 28 | 124 |
| 11 | 16th | 7 | 3 | 124 | 39 | 32nd | 32 | 28 | 124 |
| 12 | 16th | 16 | 15 | 111 | 40 | 32nd | 28 | 27 | 124 |
| 13 | 16th | 32 | 29 | 117 | 41 | 32nd | 32 | 28 | 124 |
| 14 | 16th | 19 | 19 | 113 | 42 | 32nd | 32 | 21 | 99 |
| 15 | 16th | 29 | 29 | 113 | 43 | 32nd | 32 | 26 | 123 |
| 16 | 16th | 4 | 4 | 113 | 44 | 32nd | 32 | 21 | 127 |
| 17 | 16th | 3 | 3 | 113 | 45 | 32nd | 32 | 27 | 113 |
| 18 | 16th | 32 | 8 | 122 | 46 | 32nd | 32 | 30 | 113 |
| 19 | 16th | 30 | 10 | 122 | 47 | 32nd | 32 | 26 | 113 |
| 20 | 16th | 30 | 15 | 118 | 48 | 16th | 3 | 3 | 110 |
| 21 | 16th | 32 | 29 | 118 | 49 | 16th | 5 | 5 | 110 |
| 22 | 16th | 16 | 6 | 122 | 50 | 16th | 7 | 7 | 110 |
| 23 | 16th | 16 | 7 | 117 | 51 | 16th | 9 | 9 | 110 |
| 24 | 16th | 16 | 8 | 121 | 52 | 32nd | 11 | 11 | 113 |
| 25 | 16th | 16 | 10 | 119 | 53 | 8th | 32 | 32 | 99 |
| 26 | 16th | 16 | 11 | 117 | 54 | 16th | 4 | 4 | 86 |
| 27 | 16th | 16 | 10 | 123 | | | | | |

(Full per-step velocities: `reference/pcf-patterns.json`.)
