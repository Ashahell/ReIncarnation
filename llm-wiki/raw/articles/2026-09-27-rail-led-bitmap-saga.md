# Rail LED dots: bitmap saga (Dell 2026-09-27, open)

- Source: ReIncarnation session, 2026-09-27 (Dell E6320 lane, owner eye + status-log forensics)
- Collected: 2026-09-27
- Published: 2026-09-27
- Code: `app/riapp.c` (`rail_leds_make/drop/show`, `tab_rail`), commits
  `4a3fe6e` … `9ac03e2` (current deployed)

## Goal (owner-specified)

Each device-rail chip = labeled button + LED dot. Dot must be: circle
(not triangle), bright green when active — exactly `C_MIX_GREEN`
`0x38E040` (pixel-verified off the Pattern-303A header LED,
greenest-pixel scan `56,224,64`) — dim when inactive, no visible box
around it (must sit clean on the rail grey).

## What was tried, what failed, why

1. **MUIO_Checkmark chips** (state in widget). Failed: Zune renders the
   mark with NO label text — four naked checks. Rejected (labels required).
2. **Button + MUII_TapeRecord image** (red-dot expectation, Selected
   mirror). Failed: this Zune theme paints it a dim BLACK TRIANGLE —
   wrong color AND wrong shape. Stock images are theme-owned; rejected.
3. **Button + MUII_TapePlay image** (green-triangle expectation).
   Superseded before device verdict by (4); theme risk untested.
4. **Self-drawn 18px bitmaps via Bitmap.mui** (current). Four sub-failures,
   each diagnosed from the lane:
   - (a) Dots absent, `rail LEDs unavailable`: `extern IntuitionBase`
     is NULL in the app (nothing opens it; DOSBase comes from custom
     startup). Fixed: local `OpenLibrary("intuition.library")`.
   - (b) Still absent, `rc=15`: `AllocBitMap(..., friend=0)` returns
     NULL on this AROS. Fixed: friend-free `InitBitMap` + chip
     `AllocRaster` planes.
   - (c) **Guru at launch in `Graphics AreaEllipse`**: area ops need
     `AreaInfo` + `TmpRas`, which a bare `InitRastPort` does not
     provide. Fixed: same disc as 15 integer `RectFill` spans.
   - (d) Dots absent but NO failure line: Bitmap.mui objects created
     imageless pre-bitmaps keep zero size. Fixed: build bitmaps first,
     attach at creation (`MUIA_Bitmap_Bitmap` + Width/Height).
   - (e) Dots show GREEN (chain works) but frozen + grey boxes:
     `MUIA_Bitmap_Transparent, 0L` disables transparency (grey box =
     pen 0 opaque); bitmap swap needs `MUI_Redraw(DRAWOBJECT)`.
     Fixed in `9ac03e2`.
   - (f) **NOW: boxes persist with `Transparent, 1L`.** So 1L is not
     pen-0-transparent on this Bitmap.mui. Unresolved at write time.

## Readings for the next attempt

- `MUIA_Bitmap_Transparent` semantics on Zune Bitmap.mui: boolean?
  pen number? (If pen number: 1L made pen 1 transparent while the box
  is pen 0 — consistent with (f). Check the class source, not guesses.)
- `MUIA_Bitmap_UseFriend` (V11 BOOL) + `MUIA_Bitmap_MappingTable` /
  `SourceColors`: pens were obtained on the screen colormap while the
  bitmap carries none — mapping may be required for correct display.
- Durable alternative (already on record in the tracker): draw the rail
  chips through the canvas display-list path (fixed palette, proven
  LEDs everywhere else) and delete the bitmap code instead of feeding
  it. Real work (new controls), kills the class; offered, not ordered.

## Current device state

Build `9ac03e2` runs clean (no guru, no failure line); green dots show
but boxed; toggle refresh believed fixed (redraw added) awaiting owner
re-test. Proof open.
