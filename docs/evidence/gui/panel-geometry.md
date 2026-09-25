# Panel geometry (E1 layout, measured) — §12.10 G2

**Status:** every panel section measured 2026-09-25 (303, 808, 909, mixers, Master, FX units, Transport, Pattern sections). Other sections pending their slices.
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

## Section mixer (Q units, p. 157 figure 71 × 116 px = 284 × 464 Q; same for Synth 1/2, 808, 909)

| Element | Centre (x, y) | Size | Note |
|---------|---------------|------|------|
| Title bar "MIX" | 142, 45 | 264 × 72 | olive |
| On/Off (mute) lamp button | 49, 46 | 32 × 32 | green = sounding; not automated (p. 56) |
| Output meter | 235, 45 | 24 × 44 | 4 segments |
| Pan knob | 80, 132 | body Ø 45, ticks Ø 70 | L / R marks, legend y 215 |
| Dist / PCF / Comp rockers | 200 @ y 96 / 168 / 241 | 45 × 22 | red LED at x 245; legends y 131 / 205 / 272 |
| Volume fader | 75, 345 | 60 × 200 (travel + cap) | scale lines x 32–118 |
| Delay knob | 205, 352 | body Ø 45, ticks Ø 70 | "0" / "10" marks, legend y 432 |

## Master (Q units, p. 23 figure 83 × 98 px = 332 × 392 Q)

| Element | Centre (x, y) | Size | Note |
|---------|---------------|------|------|
| Title bar "MASTER" | 170, 45 | 293 × 50 | |
| Meter L / R | 98 / 238, 209 | 38 × 192 | 12 segments, clip lamp on top; scale CLIP −6 −12 −24 −36 both sides |
| Level fader | 165, 208 | 58 × 200 | not automated (p. 24) |
| Comp rocker | 202, 345 | 45 × 22 | LED x 245; legend at x 125 |

**Insert routing (behaviour, `gui/sectmix.c`, t66).** The four mixers and the Master share one board. PCF: one section at a time, and switching it on elsewhere turns the old LED off (p. 62 step 16, p. 70). Comp: one section or the Master (p. 67). Dist follows the engine's one-owner routing (`engine/fx/route.h`). The manual contradicts itself on Dist: p. 59 says "four distortion units… all sections can use the distortion", but p. 157 says "One section at a time". This is left open for the owner; changing it touches only `unit_of()` plus the engine route.

## FX units (Q units)

Common header on all four: an on/off (bypass) lamp at (50, 45) 30 × 30, a navy title bar at y 17–70, and an input meter at (288, 45) 20 × 45 with 3 segments. Dist sits 3 Q higher (y 42).

| Unit (figure) | Element | Centre (x, y) | Size | Note |
|---------------|---------|---------------|------|------|
| PCF (p. 159, 83 × 106 px = 332 × 424 Q) | Pattern LED display | 79, 122 | 78 × 65 | 0..53; arrows up (141, 105) / down (141, 139) 38 × 30 |
| | Mode lever | 219, 124 | 28 × 62 | BP up, LP down |
| | Freq / Q / Amt / Dec sliders | 50 / 126 / 202 / 278, 294 | 58 × 158 | sliders, not knobs (figure; p. 160 "the Amount slider") → registry kind FADER |
| Delay (p. 161, 83 × 94 px = 332 × 376 Q) | Steps LED display | 76, 122 | 72 × 65 | 1..32; arrows as PCF |
| | 16th / 8th-triplet lever | 219, 122 | 28 × 65 | triplet up, 16th down |
| | Pan / F.Back knobs | 88 / 248, 270 | Ø 48, ticks 80 | L R / 0 10 marks |
| Dist (p. 163, 84 × 66 px = 336 × 264 Q) | Amount / Shape knobs | 88 / 250, 150 | Ø 48, ticks 80 | 0 10 marks |
| Comp (p. 164, 83 × 94 px = 332 × 376 Q) | Level Reduction LED row | 160, 119 | 250 × 20 | 9 LEDs, "0" in the middle; reduction lights leftward (p. 164) |
| | Ratio / Thres knobs | 88 / 248, 265 | Ø 48, ticks 80 | 0 10 marks |

Value displays use the new `RI_GEO_STEPPER` item (the owning SELECTOR's up/down arrows). One click steps by one and stops at the ends, and holding repeats after about 0.4 s (p. 18).

## Pattern section (Q units, p. 147 figure 71 × 116 px = 284 × 464 Q; same for all four sections)

| Element | Centre (x, y) | Size | Note |
|---------|---------------|------|------|
| Section on/off lamp | 42, 45 | 30 × 30 | on a maroon "PATTERN" title bar; off = "the same as an empty Pattern" |
| Pattern buttons 1–4 / 5–8 | x 50, 110, 170, 230 @ y 122 / 182 | 55 × 55 | selected = lit |
| Bank buttons A–D | same x @ y 288 | 55 × 55 | "BANK" legend (60, 240); a Bank click only arms the bank (p. 147) |
| Shuffle button | 50, 408 | 55 × 55 | legend (70, 360) |
| Steps display | 180, 411 | 70 × 58 | 1..16, arrows (244, 395 / 430) 32 × 28; per-pattern length |

## Transport (Q units, p. 144 figure 421 × 52 px = 1684 × 208 Q)

| Element | Centre (x, y) | Size | Note |
|---------|---------------|------|------|
| Shuffle knob | 80, 95 | Ø 50, ticks 90 | 0 / 10 marks; not automated (p. 72) |
| Sync / MIDI LEDs | 188 / 318, 40 | 12 | Sync: red downbeat, green other beats (p. 145) |
| Tempo display | 232, 142 | 115 × 65 | 20..500, arrows at x 312 |
| Pattern / Song lever | 690, 45 | 24 × 40 | Pattern LED 655, Song LED 725 |
| Play Stop Rewind FF Record | x 450, 578, 706, 834, 962 @ y 137 | 120 × 76 | one framed row |
| Bar display | 1139, 143 | 110 × 70 | arrows at x 1222 |
| Loop lever + LED | 1274 / 1300, 45 | 24 × 40 | "LOOP" title with rules |
| Loop Start / Length displays | 1369 / 1544, 143 | 110 × 70 | arrows at x 1452 / 1629 |

At 1× the Transport is 842 px wide, which doesn't fit an 800-px screen. For those screens there is a compact zoom (`RI_GEO_ZOOM_COMPACT` = 0.75×, 632 px). Transport legends next to the mode lever are anchored to the lever's sides so they stay clear at any font width.

**Transport laws are the engine's** (`engine/seq/transport.h`, opencode §12.9a). The panel only routes clicks. Two points differ from the manual, and both are recorded here for the §12.9 owner rather than changed from the GUI side:
1. With the song stopped, the engine's first Stop click only arms the stop sequence. The manual (p. 145) says it moves the position to the Loop Start.
2. The manual's exception is not modelled. It says that when the position is already before the Left Locator, Stop goes to the song start.
