# Panel geometry — 909 (first panel, Module 2.9 partial)

**Date:** 2026-09-20 (E0 authored 2026-09-23, LOCKED to measured
2026-09-23 — see lock note). **Status:** LOCKED at TC-2.9.1
(device-measured, ±2 px contract below).
**Spec:** TC-2.9.1 (silhouette), TC-2.9.4 (zoom); code inventory
pinned by `tests/unit/t29_paneldefault.c` (909 exposes exactly
tune/level/decay/flamres at 0x0900..0x0903) and rects by
`t29_layout.c` — doc and code cite each other so neither drifts
silently.

## Lock note (E0→measured 2026-09-23)

The E0 draft assumed a 1024×768 canvas with the panel at
(64,96). Device measurement (Dell E6320, two independent runs
of the same binary, pixel-identical both times) showed Zune
MUI auto-layout instead: knob.mui renders its ~32 px intrinsic
(FixWidth honored nowhere measurable), cells added only slack,
and the window sizes to content (160×88 at screen origin — the
1024×768 request ignored). Rather than fight the toolkit for
the E0 numbers, the doc locks the measured compact panel: even
33 px pitch, aligned row, identical visuals — principled order,
not accident. The 1024-canvas full panel + ReBirth-style knob
art stay future work (M2.4); MUIC_Knob stock look claimed
as-is, never as art (never-pixel-copy policy holds).

## Canvas (measured)

Window 160×88 at screen origin (0,0). Knob row y = 52.

## Controls (centers, screen px @1x)

| Control | ID | Center (x,y) | Diam |
|---------|----|--------------|------|
| tune | 0x0900 | (30,52) | 32 |
| level | 0x0901 | (63,52) | 32 |
| decay | 0x0902 | (96,52) | 32 |
| flamres | 0x0903 | (129,52) | 32 |

Even 33 px pitch (30/63/96/129 — indicator bars agree with
bodies within 1 px); value-invariant body centers (pointers move
with value, bodies don't). Contract: every center within ±2 px
of the table on re-measurement (regression gate, not design —
the design is the even/aligned/consistent rule above).

**Spec:** TC-2.9.1 (silhouette), TC-2.9.4 (zoom); code inventory pinned
by `tests/unit/t29_paneldefault.c` (909 exposes exactly
tune/level/decay/flamres at 0x0900..0x0903 — doc and code cite each
other so neither drifts silently). Superseded E0 canvas/table
below struck for the record (1024-canvas premise refuted on
device — kept visible so the delta is auditable, not silent).

## Canvas (E0, SUPERSEDED)

Reference viewport 1024×768. Panel rect (64,96)-(960,672): 896×576,
aspect 896/576 = 1.5556 (contract: measured aspect within ±1%).
Title strip: top 32 px of the rect ("909", style-matched type, never
pixel-copied reference art).

## Controls (E0, SUPERSEDED — centers @1x, radius 28 px)

| Control | ID | Center (x,y) | Default |
|---------|----|--------------|---------|
| tune | 0x0900 | (176,320) | 64 |
| level | 0x0901 | (400,320) | 100 |
| decay | 0x0902 | (624,320) | 64 |
| flamres | 0x0903 | (848,320) | 64 |

Four equal 224 px cells (64+112+224k); value readout 16 px high,
centered 48 px below each knob center. Contract: every center within
±2 px of the table at 1024×768.

## Zoom rule (TC-2.9.4/TC-2.11)

Artwork authored at 2x, filtered down. Centers/sizes scale by the
zoom factor (`ri_zoom_factor`: 1x/1.5x/2x); knob DRAG travel stays
150 px of screen travel at every zoom (travel is input px, not layout
px — `ri_knob_drag_to_value` never sees the zoom level).
