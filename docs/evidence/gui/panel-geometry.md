# Panel geometry (E1 layout, measured) — §12.10 G2

**Status:** 303, 808 and 909 sections measured 2026-09-25. Other sections pending their slices.
**Source:** ReBirth RB-338 2.0.1 Owner's Manual, synth-section figure on p. 153
(embedded image 366 × 115 px, extracted with `pdfimages`, viewed at 4× with a
10-px grid). Layout is measured, never pixel-copied (spec §1): artwork stays
clean-room.
**Code:** `gui/panelgeo.c` (units: Q = quarter figure pixel, 1464 × 460 Q),
test `tests/unit/t61_panelgeo.c`.

## Scale

- Base render scale 2 window px per figure px: 303 section = 732 × 230 px at
  1× (fits riqemu1 800×600 and the Dell 1366×768); 1.5× = 1098 px, 2× = 1464 px.
- The ReBirth screenshots' native pixel size is unknown (the manual figures are
  downscaled); the scale is E0 and changes in one constant.

## 303 section (Q units)

| Element | Centre (x, y) | Size | Note |
|---------|---------------|------|------|
| Waveform switch | 135, 62 | 180 × 36 | left of the first divider (x 262) |
| Knobs Tune, Cutoff, Reso, Env Mod, Decay, Accent | 352, 484, 615, 747, 878, 1010 @ y 110 | body Ø 84, tick ring Ø 120 | uniform pitch 131.5 Q = 32.9 figure px; TB-303 panel order |
| EDIT STEP display | 1356, 130 | 73 × 65 | right of divider x 1099 |
| Pitch Mode | 109, 311 | 53 × 22 | LED 108, 278 |
| Clear | 109, 402 | 62 × 28 | |
| White keys C D E F G A B C | 222, 307, 392, 476, 560, 645, 730, 812 @ y 385 | 34 × 60 | pitch ≈ 84.5 Q; LEDs @ y 338 |
| Black keys C# D# F# G# A# | 267, 349, 518, 602, 687 @ y 275 | 30 × 50 | LEDs @ y 238 |
| Note/Pause toggle | 1212, 252 | 60 × 27 | note LED 996, 250; pause LED 1152, 250 |
| Down, Up, Accent, Slide | 909, 1011, 1111, 1211 @ y 385 | 34 × 60 | LEDs @ y 338, legends @ y 305 |
| Back | 1345, 267 | 58 × 30 | |
| Step | 1349, 392 | 86 × 53 | |

Estimated measurement error ±2 figure px (±8 Q) from the downscaled figure;
the knob pitch is enforced uniform, which the figure supports (Tune→Accent
span 658 Q / 5 = 131.6 Q).

## UX facts from the same chapter (p. 18)

- Knobs: press and drag up/down; [Shift] for fine steps. (The owner-approved
  both-axes drag is a deliberate extension; vertical behaviour matches.)
- Faders: drag the handle up/down.
- Value displays: two arrow buttons change one step; holding repeats.
- Transport bar always at the top of the single Song window; sections scroll
  below it (p. 19).

## 808 section (Q units, p. 148 figure 368 × 117 px = 1472 × 468 Q)

| Element | Position | Size | Note |
|---------|----------|------|------|
| Columns AC BD SD LT MT HT RS CP CB CY OH CH | x = 92, 180, 267, 355, 442, 530, 617, 705, 793, 880, 968, 1056 | pitch 87.6 Q | panel order = TR-808 instrument order |
| LEVEL knobs (red) | every column @ y 62 | body Ø 40, ring 56 | legend @ y 18 |
| Parameter knobs (white) | BD/SD/LT/MT/HT/CY @ y 148; BD Decay, SD Snappy, CY Decay, OH Decay @ y 238 | body Ø 36 | always in their instrument's column |
| Sound switches LT MT HT RS CP | @ y 258 | 38 × 40 | alternate legends LC MC HC CL MA @ y 205 |
| Instrument legends (options) | every column @ y 312 | 70 × 32 | click = select instrument |
| Steps 1–16 | x = 92 … 1056, pitch 64.3 Q, @ y 400 | 50 × 76 | TR-808 colours in groups of four |
| Instrument Selection knob | 1295, 205 | Ø 104 (hit 110) | ring labels AC→CH clockwise from 208°, 28.2° apart, radius 100, 44 × 24 |

## 909 section (Q units, p. 151 figure 365 × 117 px = 1460 × 468 Q)

| Element | Position | Size | Note |
|---------|----------|------|------|
| Steps 1–16 | x = 148 + 84·k, y 378 | pitch 84 Q | numbered keys; lamp: low orange, high red, flam green |
| Knob rows | y 140 (LEVEL / TUNE) and y 238 (ATT / DEC / TONE / SNAP) | | every knob sits over its instrument's step(s), TR-909 grouping: BD 1–2, SD 3–4, LT 5–6, MT 7–8, HT 9–10, RS 11, CP 12, CH 13, OH 14, CC 15, RC 16 |
| Instrument legends (options) | y 322, over each group | | click = select instrument; no selector knob on the 909 |
| AC option / AC Level | option (60, 222); Level knob left column | | |
| Flam knob | 60, 335 | | |
| Flam button | 60, 405 | 40 × 40 | toggles the next step click to flam (p. 30) |
| Group bars (drawing only) | y 24–60 | | legends AC … RC in orange on dark |
