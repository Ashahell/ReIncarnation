# Panel geometry — 909 (first panel, Module 2.9 partial)

**Date:** 2026-09-23. **Status:** E0 HYPOTHESIS (locks at the on-device
silhouette pass, TC-2.9.1). Numbers below are owned design decisions
in the P-18 style: the MCC shells implement them, the device pass
measures against them (±2 px centers, ±1% proportions).

**Spec:** TC-2.9.1 (silhouette), TC-2.9.4 (zoom); code inventory pinned
by `tests/unit/t29_paneldefault.c` (909 exposes exactly
tune/level/decay/flamres at 0x0900..0x0903 — doc and code cite each
other so neither drifts silently).

## Canvas

Reference viewport 1024×768. Panel rect (64,96)-(960,672): 896×576,
aspect 896/576 = 1.5556 (contract: measured aspect within ±1%).
Title strip: top 32 px of the rect ("909", style-matched type, never
pixel-copied reference art).

## Controls (centers @1x, radius 28 px)

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
