# A LINE's bbox is ±1, and that was the two-day anomaly (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-the-bounded-scan-and-the-plus-one-line.md)
- Related: [the knob was O(r²)](2026-10-05-the-knob-was-o-of-r-squared.md), [the item cull that threw away 98 %](2026-10-05-the-item-cull-that-threw-away-98-percent.md), [a ratio, not microseconds](2026-10-05-a-ratio-not-microseconds-this-host-is-bimodal.md)

The clip-aware panel cut failed `t169` with `culled 237 commands, the clip alone
would keep 23`, and the number 23 was wrong — it was a stale object. On a clean tree
the same box reads:

```
unclipped 3009 | real clip keeps 239
   op   bbox-ok   hits_box
  RECT     2628        195
  LINE      362         44
  TEXT       19          0
  expect_count equivalent = 239   (bbox errors: 0)
```

Re-implementing the cut reproduced the failure as **237 vs 239** — **two commands
short.** Dumping both streams located them:

```
ref[1] op=2 0,21..731,21      <-- one row ABOVE the box, kept by the clip
got[1] op=2 0,23..731,23      <-- y=21 missing
```

## The cause is one comment in the platform

```c
    case RI_D_LINE:
        /* 1-px strokes on both backends; grow one for raster rounding. */
        *y0 = (c->y0 < c->y1 ? c->y0 : c->y1) - 1;
        *y1 = (c->y0 < c->y1 ? c->y1 : c->y0) + 1;
```

**A hairline at nominal `y` is kept by the clip whenever `y` is in `[cy0-1, cy1+1]`,
not `[cy0, cy1]`.** My clamp used the nominal line, so it dropped the row just above
and the row just below — exactly two commands, at `y=21` and `y=89`.

## The rule, and the three trims that produced three bugs

*When trimming a primitive, use the extent the **clip** uses, not the extent the
primitive nominally occupies.*

| trim | primitive | extent used | outcome |
|---|---|---|---|
| `ri_art_disc_grad` rows | one-pixel **rects** | exact | right first time |
| 303 keyboard strip seek | **rects**, variable stride | nominal | wrong — dropped the *straddling* strip |
| `ri_art_panel` hairlines | **lines** | nominal | wrong by **±1 per side** |

**A trim is only as correct as its model of what the clip considers inside — and that
is a property of the *command*, not of the drawing routine that emitted it.** `RECT`
and `LINE` differ by a pixel per side, and nothing at the call site distinguishes them.

## Result

Host: floor ratio **0.257 → 0.194**, the real box **2.79 → 2.63 µs**.
On target (`RIPP-PAN`): **build 43.0 → 32.7 µs (1.31×), part_avg 162.8 → 149.5**,
replay and blit unmoved, partition 17 rows / 0 mismatches, `xruns=0`.

## And a real API hazard found on the way

```
SYNTH1 unclipped 3009 | clipped to 143,22..209,88 -> 3248 kept
```

`3248 = 3009 + 239`. **`ri_dlist_set_clip` assigns `cx0..cy1` and `clip` and never
touches `dl->n`**, so setting a clip on a populated list appends. Every caller in the
tree initialises first, so it is a trap for the next caller rather than a live bug —
documented at the prototype because it is exactly the shape of thing that produces an
unaccountable count. **It is a candidate explanation for the anomaly and it is *not*
established**, because `t169` does initialise.
