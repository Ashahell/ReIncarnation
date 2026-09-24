# Panel geometry — 909 (first panel, Module 2.9 partial)

**Date:** 2026-09-20 (E0 authored 2026-09-23, compact lock
2026-09-23, v2 art lock below). **Status:** LOCKED at TC-2.9.1
(device-measured, ±2 px contract below).
**Spec:** TC-2.9.1 (silhouette), TC-2.9.4 (zoom); code inventory
pinned by `tests/unit/t29_paneldefault.c`, rects by
`t29_layout.c` — doc and code cite each other so neither drifts
silently.

## v2 art lock (2026-09-23)

Real knob art (52 px bodies) does not fit the compact 33 px
pitch — measured overlapping on device. Hardware ratio
(TR-09 photo: ~74 px pitch on ~55 px bodies = 1.35) gives
pitch 70 for 52 px bodies: centers (40,110,180,250 @ y52),
window 296×96 at screen origin. Same even/aligned/consistent
rules, re-measured on device below. The 33-pitch compact
numbers stay struck through for the audit trail.
**Spec:** TC-2.9.1 (silhouette), TC-2.9.4 (zoom); code inventory
pinned by `tests/unit/t29_paneldefault.c` (909 exposes exactly
tune/level/decay/flamres at 0x0900..0x0903) and rects by
`t29_layout.c` — doc and code cite each other so neither drifts
silently.

## Lock note (E0→measured 2026-09-23; SUPERSEDED by v2 lock above 2026-09-23/24)

The E0 draft assumed a 1024×768 canvas with the panel at
(64,96). Device measurement (Dell E6320, two independent runs
of the same binary, pixel-identical both times) showed Zune
MUI auto-layout instead: knob.mui renders its ~32 px intrinsic
(FixWidth honored nowhere measurable), cells added only slack,
and the window sizes to content (160×88 at screen origin — the
1024×768 request ignored). ~~Rather than fight the toolkit for
the E0 numbers, the doc locks the measured compact panel: even
33 px pitch, aligned row, identical visuals~~ — SUPERSEDED:
real 52 px art bodies overlap at 33 px pitch (proven on device),
so the lock moved to even 70 px pitch (v2 section above).
~~The 1024-canvas full panel + ReBirth-style knob
art stay future work (M2.4); MUIC_Knob stock look claimed
as-is, never as art~~ — SUPERSEDED: custom RKnB class renders
measured 909 art on device (m22), MUIC_Knob retired; the
never-pixel-copy policy holds unchanged.

## Controls (centers, screen px @1x)

| Control | ID | Center (x,y) | Diam |
|---------|----|--------------|------|
| tune | 0x0900 | (40,52) | 52 |
| level | 0x0901 | (110,52) | 52 |
| decay | 0x0902 | (180,52) | 52 |
| flamres | 0x0903 | (250,52) | 52 |

Even 70 px pitch (40/110/180/250 — hardware 1.35 diameter ratio
on the 52 px art bodies); value-invariant body centers; 80 px
frames blit at center−40. Contract: every center within ±2 px
of the table on re-measurement (regression gate).

## Superseded compact lock (33 px pitch, MUIC era)

Centers (30,63,96,129 @ y52) in a 160×88 window — held for the
~32 px MUIC visuals. Refuted as an art basis by the 52 px
measured bodies (overlap at 33 px pitch, proven on device
2026-09-23). Kept visible so the delta is auditable.

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
