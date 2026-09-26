# Zoom finish G8.2 — device proof (riqemu1, 2026-09-26)

`RISECT <section> [mod=<name>] [zoom=0..3]` (zoom switch added in this
commit; canvases fixed at their creation zoom, skin cache rebuilt per
zoom change). Agent session 26, 1280x1024.

## Matrix (`img/2026-09-26-zoom-*.png`)

- 303 Classic z0–z3, 808 Classic z0–z3 (all four zooms each)
- mix Classic z0+z2, fx Classic z0+z2, tr Classic z0+z2
- 808 under 808-RI z0–z3 (skin zoom-cache proof at every factor)

Stored subset: 303 z0+z2, 808 z0+z2, mix z2, tr z0, 808skin z2+z3
(the rest stay in scratch; all viewed by the author).

## Verdict

- **Crisp at every zoom.** Procedural art is resolution-independent by
  construction; skin parts downscale once per zoom change from the 2x
  masters through the host-pinned box filter (t75 exact pixels, t76 sizes
  32/48/64/24 + solid exact at every factor). No shimmer, no resample
  halos on device at 1x/1.5x/2x/0.75x, Classic or skinned.
- **Compact (0.75x) legend crowding.** Fonts do not scale with zoom (known
  AROS lesson): at 0.75x adjacent legends touch (`LEVELLEVEL…`). The
  geometry is correct (controls hit-test right — same PX() both paths);
  only inter-legend spacing collapses. Accepted consequence of the
  800-px-screen zoom; flagged for the owner pass, not a defect.
- **150 px screen travel at every zoom (P-18).** Structural: the canvas
  accumulates raw `MouseX/MouseY` deltas (screen pixels —
  `rsection.mcc.c` mousemove handler) and the pure law maps 150 px to
  full 0..127 with no zoom input (`t1_knob` ±5% band; t76 pins 150 px →
  full and 75 px → half through the same call the canvas makes).
  A true drag needs a mouse, which no lane injects — the drag row stays
  for the human pass in acceptance.md.

## Zoom-factor single source

`ri_skin_zoom_num()` (new, t76) returns the 4/6/8/3-over-8 series; t76
pins it value-for-value AND against `ri_geo_px` (`px*8 == q*num` on
multiples of 8). The loader scales masters by it — geometry and skins
cannot drift apart without a test failing first (mutant 4→5 killed).

## Zoom policy on small screens (DECIDED 2026-09-26, owner)

Cap the zoom at the largest factor whose width fits the screen (1x on
the Dell's 1366 px: 303/808/909 clip ~75-80 px per side at 2x); a
vertical window scrollbar is acceptable when the panel is taller than
the screen.
