# Render-stage close-out: three floors and my own clock (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-the-bounded-scan-and-the-plus-one-line.md)
- Related: [which section owns the build, and why the LCD is a floor](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md), [the bounded scan](2026-10-05-the-bounded-scan-that-needed-no-ordering-assumption.md), [a line bbox is ±1](2026-10-05-a-line-bbox-is-plus-one-and-that-was-the-anomaly.md), [the GAP counter was max where sum](2026-10-05-the-gap-counter-was-max-where-the-arithmetic-said-sum.md)

The render-stage work is finished as far as the caller can take it. Not because the
ideas ran out — because what remains has a mechanism.

## The final budget

`RIPP-PAN`, 15 quiet windows:

```
  part_avg 149.5 | build 32.7 | replay 32.9 | blit 69.0 | gap 14.1
```

**149.5 µs = 69 blit (46 %) + 33 build (22 %) + 33 replay (22 %) + 14 gap (10 %).**
From **232.6 µs** when the gap counter was first corrected: **1.56×**, and
`build_avg` **112 → 32.7 µs (3.4×)**, with `xruns=0` and `wake_max=46 µs` throughout.

**~100 of the remaining 149 µs is established as floors.**

## The blit: 46 %, and no caller-side lever

69 µs for 64×33×4 B = **8.4 KB, about 122 MB/s — roughly 10× slower than the pixels
justify.** Traced end to end in the AROS tree:

```
BltBitMapRastPort -> OBTAIN_HIDD_BM/RELEASE -> do_render_with_gc
  -> LockLayerRom(L) -> GetRPClipRectangleForLayer -> walk L->ClipRect
  -> bitmap_render -> HIDD_BM_CopyMemBox32
```

And `blt_avg` is **constant to within a microsecond across every window of every run**
— 68.8, 68.9, 69.0 — with `blt_min` never resetting. **So the cost is call-path, not
pixels**, which the consultant's earlier "*under 3 % of the blit's 69 µs is pixel
movement*" already implied.

Three attacks have failed: direct painting (net zero, because `do_render_with_gc`
branches on `rp->Layer` so the **window** path pays `LockLayerRom` *per drawing
primitive* while the offscreen path skips it), narrowing the box (a full-width row
intersects every sub-box), fewer blits (already one per repaint).

**The only remaining lever is an AROS-side change** — a `COPY`-minterm fast path in
`HIDD_BM_CopyMemBox32`, or caching `GetRPClipRectangleForLayer`. **A different
repository, on the hot path of every window on the machine, and not a change this
lane should make quietly.** An owner decision with a named file and a named function.

## The gap: 10 %, and it is mine

`draw_frame` sets `timed = 1` whenever the EClock opened, with no gate, so **every
partial repaint in the shipped binary pays the clock reads.** `ReadEClock` on AROS
x86-64 is not an rdtsc:

```c
    outb(CH0|ACCESS_LATCH, PIT_CONTROL);   /* Latch the current time value */
    time = ch_read(PIT_CH0);               /* Read out current 16-bit time */
```

**8254 PIT channel 0, by port I/O, ~2.2 µs per call.** And **there is no cheaper
public monotonic source in this AROS** — no `ReadNanoseconds`, no exposed TSC
counter. AROS uses raw `rdtsc` internally (`rom/graphics/gfxfuncsupport.c`) but does
not publish it. **So the win is fewer reads, not cheaper ones** — the opposite of what
I assumed when I asked.

One of the eight was a **pure duplicate** (the read ending the replay span and the
read starting the blit span were two calls of the same instant), and sharing the
sample is *more* accurate, since two reads of one instant can differ by the PIT's own
advance. `RIPP-CLK`: **gap 18.0 → 16.7 µs**, part_avg 167.0 → 162.8.

## The one open decision, stated as a decision

**Six reads remain: ~13 µs, 9 % of a repaint.** Keeping only the total and the build
span is 4 reads, ~9 µs, 6 %. The cost is specific: **the on-target partition invariant
goes with it**, because two components always sum. That invariant is what caught the
`max`-where-`sum` bug on a live 119 µs phantom; it is now pinned at the source by
`t168`, so the on-target copy is redundant — **but it is the copy that caught a real
one, and trading a standing check for 6 % is a judgement about risk, not about code.**

## Closed, and not worth having

- **a display-list cache** — hit rate 0 %; `RI_STALE_BAR` means identical rects carry different pixels
- **a narrowed per-character box** — the legend face is proportional: `"  1"` is 12 px, `"  4"` is 13 px
- **a static/live split** — `DoMethod(obj, MUIM_Draw, 0)`, so the damage region is undefined by contract
- **a per-section binary search** — the ids invert inside a section too; LEVI is ordered by sub-panel

## The session's real record

Six refuted cuts. **Six instrument bugs, each producing a false conclusion:**

1. a `max` where `sum` was documented — the 119 µs phantom
2. a dead clock pair re-timing the replay
3. a geometry-shape enum written from memory — reversed a published conclusion
4. a probe comparing a `reg_id` against an index — `prev = i` instead of `prev = d->reg_id`
5. a stale binary read as "the optimisation does nothing"
6. a stale build read as "it does not reproduce"

**Five of the six are the same failure: an instrument reporting a property the code did
not have, believed because it printed a clean answer.** The sixth was a harness
reporting a stale artefact. **The wins came mostly from distrusting a number, and the
losses were all a number believed too early.**
