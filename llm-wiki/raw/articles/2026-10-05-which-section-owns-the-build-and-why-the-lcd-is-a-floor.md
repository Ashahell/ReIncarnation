# Which section owns the build, and why the LCD background is a floor (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-which-section-and-why-the-lcd-is-a-floor.md)
- Related: [the item cull that threw away 98 %](2026-10-05-the-item-cull-that-threw-away-98-percent.md), [the GAP counter was max where sum](2026-10-05-the-gap-counter-was-max-where-the-arithmetic-said-sum.md), [caches only skip work whose result is unchanged](2026-10-04-consultant-cache-refuted-and-a-live-bbox-bug.md), [a live bbox bug, fixed](2026-10-04-consultant-cache-refuted-and-a-live-bbox-bug.md)

The item cull was aimed using a **guess** about which section the lane repaints.
That guess had never been measured — the same mistake shape as the GAP's `max`, one
level up. So `bsec` was added before any further cut.

## `bsec=13` — TRANSPORT, 16 of 16 windows

```
   n | part_avg build rpl blt gap | bsec bsecsum
    8 |      176    55  32  69   18 |   13      447
   {'TRANSPORT': 16}
   window: bsec=13  TRANSPORT  bsecsum=447  of build_avg=55  box_bar=['0/0/0', '176/177/8', '0/0/0', '0/0/0']
```

**Essentially the entire build is the transport**, and `box_bar` reads `175/177/8`:
the Song Position box, 8 repaints per window. `bsec` prints **255** on idle windows,
so "nothing built" cannot be read as "section 0 built".

## So two commits were worth nothing here, and that is the finding

```
                build_avg   part_avg
    RIPP-CULL       56.2       177.5
    RIPP-DISC       55.4       175.9
```

**1.4 % on a ±2 µs spread.** The 303 strip seek and the `disc_grad` row clamp are
real on the host — SYNTH1 8.00 → 6.34, LEVI 42.0 → 30.4 — and they target SYNTH1 and
LEVI, **which this lane never paints.** Without `bsec` I would have read 56 → 55 as
confirmation. **The instrument cost one commit and prevented a false headline.**

## The composition of what is left

`ri_art_led_digits` is **not a 7-segment display**:

```c
    ri_art_lcd_bg(dl, x0, y0, x1, y1);
    ri_art_text_c(dl, cx, cy, ndig == 3 ? "888" : "88", C_SEG_DIM);
    ri_art_text_c(dl, cx, cy, b, C_SEG);
```

and `ri_art_lcd_bg` is **one rect per row** across the whole display. So the 97
survivors are **74 rects + 21 lines = the LCD background, plus 2 text commands**, and
the `"888"` ghost is a literal that never changes.

Against the real box (`404,38..467,70`, 64×33):

```
  unclipped     9.78 us over 829 commands  (11.8 ns/cmd)
  clipped       4.46 us -> 46.0 ns per SURVIVING command
  kept 97 of 829  (11.7% kept, 54.4% of the cost removed)
```

## Three cuts refuted by measurement, in one session

1. **Narrow the box to the changed digit.** Needs a monospaced face. `"  1"` measures
   **12 px** and `"  4"` measures **13 px** — identical structure, different widths.
   **The face is proportional; the cut is unsound.**
2. **Skip the static LCD background on a bar-only repaint.** Killed by the platform:
   `DoMethod(obj, MUIM_Draw, 0)` — **the message carries a literal 0**, and the
   invalidation region is only reachable by intersecting the object's own bounds
   with `l->ClipRegion->bounds`. So a widget is never told *what* changed. **The
   contents of a damage rect are undefined by contract**, and *"it should already be
   there"* is an assumption about a region the toolkit declared unreliable.
3. **Hoist the cull above the registry lookups.** Ratios, pre-hoist
   `0.3762, 0.3753, 0.3720`; post-hoist `0.4301, 0.3682, 0.3634, 0.4166`.
   Pre-hoist is tighter and slightly better.

## The floor

**~37 % of the build is the background plus the items the cull cannot skip, and it is
unreachable by every box-based cut** — because every `ri_art_lcd_bg` row is a
full-width rect, so its bbox is the whole display and it intersects *every* sub-box
of it. The only mechanism that could reach it is the one the contract forbids. **That
is a floor, not an unattributed remainder.**

## And the constant I invented

`bench_trbar` counted items by shape using a table **written from memory**. The real
enum is `KNOB 0, RECT 1, LED 2, LEGEND 3, DIVIDER 4, OPTION 5, STEPPER 6`. So the 5
items with no damage box, which I reported as OPTIONs, **are `RI_GEO_LEGEND`** — and
an earlier entry's confident "my legend hypothesis was wrong" was **wrong**: the
hypothesis was right and I refuted it with a fabricated constant. Counted correctly:
**14 KNOB, 3 RECT, 5 LEGEND, 8 OPTION, 1 other; 26 of 31 boxed.**

This is `RI_RAW_SPACE` (0x40, not 57) for the second time, and the first time it
**reversed a conclusion** rather than merely breaking a measurement.
