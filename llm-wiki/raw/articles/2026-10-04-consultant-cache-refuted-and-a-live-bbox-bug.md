# The cache is refuted, `build_max ≈ part_max` is a misread, and there is a live bbox bug (2026-10-04)

- Source: third-party consultant session + main-session verification, 2026-10-04
- Collected: 2026-10-04
- Published: 2026-10-04
- Raw: [verbatim](../evidence/2026-10-04-consultant-review-and-bbox-bug.md)
- Related: [host bench and the overload guard](2026-10-04-host-bench-and-the-overload-guard.md), [the Dell lane's two silent traps](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)

I asked the consultant how to proceed on the 1.36 s stall. It refuted my plan
**using my own first `boxrep` reading**, and every finding checked out when I
re-verified it independently. Two defects came out of the exchange: one in my
conclusion, one in the code.

## The cache is dead, and `boxrep` already said so

`boxrep=3/0/130` means `dpr_new=0` — every invalidation repeated the previous
rect. But `RI_STALE_BAR` originates only from `RI_PANEL_CH_FOLLOW`, set when
`ri_str_follow()` returns non-zero, i.e. **only when the song bar advanced**. So
every one of those "repeats" carried a *different bar number*: identical rect,
different pixels.

**A cache can only skip work whose result is unchanged, and here the result
changes by construction on every call.** Host-measured at `-O0` — the stall's own
flag — a clipped build is **24 µs** against the Dell's quiet `build_max=116`.
25 draws × 24 µs = **0.6 ms of a 1.87 s window.**

Not built. There was nothing to win.

## `build_max ≈ part_max` was a coincidence in the wrong row

```
part_max=490609  build_max=490523   <- 99.8 % build
part_max=1364359 build_max=132813   <-  9.7 % build
```

It holds in one motivating row and **fails by 10× in the other**. `dp_build_max`
resets per window, so 132813 µs bounds *any* build in that window — including the
1.36 s draw. **≥1.23 s (90 %) of the worst draw was replay+blit, not build.** Two
different failures had been averaged into one story, and the one with 2505 xruns
behind it was never a build problem.

**The lesson, which now heads the wiki's method notes: check a conclusion against
the tree before building on it.**

## The free discriminator, and what it said

The cheapest experiment was to re-read a log I already had:

```
xruns=0     wake_max=34    load=611  overloads=0
xruns=908   wake_max=5822  load=953  overloads=6
xruns=2505  wake_max=5822  load=609  overloads=17
```

`wake_max` jumps **34 µs → 5822 µs** in the same window as the stall. Identical
code doing 24 µs of work that took 1.36 s is a 55 000× ratio — only the *absence
of the CPU* produces that. This is the **third** instance of a signature this
wiki already named twice ("`-O2` tab cost is preemption of prebuilt MUI code"; the
governor dropping the render task below the UI at `-O0`).

**The BAR path is a witness to the stall, not its cause.** It fires at the bar
rate on its own (~0.83 Hz), so **tab switching was the wrong stimulus** and `-O0`
was the wrong arm — and "the stall did not reproduce on the mixed build" is
near-neutral evidence rather than evidence against. At `-O2` the same event would
last about a third as long and probably be attributed to a different box.

## A real, user-visible bug — fixed

All four `ri_geo_bbox` call sites passed a hardcoded zoom of **0** while the
canvases are not at 0 — the transport is unconditionally `RI_GEO_ZOOM_COMPACT`:

```
asked (zoom 0)      : 540,52..622,92
real  (compact = 3) : 404,38..467,70
DISJOINT
```

A bbox in the wrong coordinate space is not rejected — it is clamped into
something that looks valid and repaints the wrong art. So the path whose entire
job is to make the Song Position display follow the song **was repainting a region
that does not contain it**, and `box_bar` had never once measured the Song
Position.

All four sites now pass the canvas's own zoom, and `t166_bbox_zoom` pins it as
pure geometry — the concrete regression, plus the rule across every zoom,
including that each wrong zoom *fails* to cover the compact bar.

**Filed separately from the stall, as it deserves: this one changes what the user
sees.**

## Two corrections to my own instrumentation

- **`dpr_run_now` is now reset with its siblings.** My defence of leaving it
  running ("a window-spanning burst would be hidden") was already satisfied by
  `dpr_run_max`, which is never reset. Worse, zeroing the siblings meant each
  window's first invalidation was miscounted as a **repeat** when it matched the
  previous window's last — spurious `dpr_rep` landing in the **quiet** windows,
  biasing the ratio toward "cache it".
- **`boxrep` is a caller-spam counter, not a cache oracle.** An identical rect
  does not mean identical work: two requests for the same rectangle can bracket a
  state change. I had been treating it as the oracle, which is a category error.

## The gap that hid all of this

`blit_max` is written **only on the full-draw path**; the partial path returns
without ever timing its blit. The one number that would have split a
90 %-not-build event from a build event **was never collected** — and that is what
would have caught this without a consultant.

## Open

- **Replay vs blit split on the partial path** — the top instrumentation item,
  ~8 lines, no Dell session needed, and it makes any recurrence self-explaining.
- **The 1.36 s stall itself** is now attributed to preemption rather than to our
  code, and the lever is the yield policy (a trigger on GUI latency rather than
  2 s of continuous over-budget audio) — **an owner decision.**
- **`AllocBitMap` churn** (`draw_frame:190-201`) is the only allocation in the
  whole GUI draw path; `allocs=` is already on the heartbeat and was never quoted
  for the expensive row.

## Method

- **Ask when the next step is expensive and a cheaper discriminator exists.** The
  consultant's best experiment was a grep of a log I already held.
- **"It repeats" is not "it is the same".** The rect repeating said nothing about
  the work, and reading it as a cache oracle was the error that would have cost
  the most.
- **Two numbers that agree in one sample and disagree in another are not a
  finding.** `build_max ≈ part_max` looked conclusive and was true of the wrong
  row.

## See Also

- [host bench and the overload guard](2026-10-04-host-bench-and-the-overload-guard.md) — where the 1.36 s stall was first attributed to the display-list build
- [the guest crashed in utility.library](2026-10-04-the-guest-crashed-in-utility-library.md) — the AHI contention that made a run lie