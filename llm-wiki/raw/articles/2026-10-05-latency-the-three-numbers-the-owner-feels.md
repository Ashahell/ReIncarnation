# Latency: the three numbers the owner feels, and what they say (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-latency-the-three-numbers-and-two-instrument-defects.md)
- Related: [render-stage close-out](2026-10-05-render-stage-close-out-three-floors-and-my-own-clock.md), [the gap counter was max where sum](2026-10-05-the-gap-counter-was-max-where-the-arithmetic-said-sum.md), [a line bbox is ±1](2026-10-05-a-line-bbox-is-plus-one-and-that-was-the-anomaly.md)

The owner closed the box-repaint path — 150 µs at ~9–10 advances/s is ~1.4 ms of
GUI time per second, 0.15 % of a core, invisible in use — and redirected the work to
latency. Three numbers, from the GUI task's own clock, each with a self-check.

## The three numbers, mixed build on the Dell

```
ticks=162 late=157 late_max=2867 us hist=61/101/0/0 hs=162
input=300 in_max=2655 us in_avg=40 us ihist=298/2/0/0 is=300
span=16405418 expect=16200000 drift=205418 latesum=205418 SELFCHK=ok
```

| metric | result |
|---|---|
| **input → repaint** | max **2.7–3.8 ms**, mean 40–52 µs |
| **tick lateness** | 157 of 162 ticks >0.5 ms late, max 2.9 ms, 205 ms of lateness in 16.4 s = **1.25 %** |
| **five-tab cycle** | **165489 µs** total, per tab 29 / 32 / 24 / 47 / 33 ms |

**The owner's judgement is confirmed by the first number:** a click is on screen in
well under 4 ms at worst. The damage path was never the felt lag.

**The loop is slightly starved, not starved** — 1.25 % behind schedule. That is the
first number on this lane about **scheduling** rather than drawing.

## The tab switch is the lag, and it is 18× a full repaint

```
  click -> repaint      in_max  2655-3836 us, in_avg 40-52 us
  full repaint          full_avg 2291 us
  tab switch            TAB page=4 us=41608      -> 18.2x a full repaint
```

A tab switch should cost roughly **one** full repaint — of the newly-shown page.
It costs eighteen. The likely cause is the cascade in `tab_switch`: one
`MUIA_Group_ActivePage` plus **five** `MUIA_RArt_Active` tab-button redraws, each
doing a whole-area draw, which is 5–6 draws ≈ 24–47 ms.

**And the per-tab spread is the tell:** 24 ms to 47 ms. Equal work should cost equal
time, so **the cost is content-dependent, not fixed overhead** — consistent with
per-tab full draws whose cost tracks what is on that tab.

## A measurement caveat that must be carried forward

**Five of six tab clicks did not register** — only `page=4` reached the app. The
recorded click map (`y=166`, `x=56/122/180/238/288`) is stale or the layout differs.

**So the per-tab figures rest on one complete cycle plus one isolated switch.** The
tab totals are sound (they come from `tab_switch`'s own per-switch EClock), but
**the click map must be re-derived from a capture before per-tab cost is measured
properly** — and that is the same class of bug as the bbox-zoom one: **a coordinate
space that was right once and was never re-verified.**

Also worth recording: **`TAB` events live in `RIAPP-EV.LOG`, not `RIAPP.LOG`.** Grepping
the main log for tab activity finds nothing, which reads as "the clicks missed" and
is indistinguishable from the real reason they missed.

## The self-check missed two defects on its first real run

```
late_max=2805650769 us ... span=2820903567 us drift=2806003567 SELFCHK=ok
```

**`late_max` was 2.8 BILLION microseconds** — the previous-tick stamp is a `static`
initialised to zero, so the first interval was measured **from boot**. And **`cyc=0`
sat beside five non-zero `per_tab` values**, because `per_tab` held the last cycle's
numbers while the cycle counter had been reset.

**The lesson is about the check, not the fields.** `SELFCHK=ok` was *true* on every
quantity it tested: the buckets summed correctly, and the arithmetic was internally
consistent. **The lie lived in a field nothing was checking.** After the fixes the
identity is exact:

```
drift=205418 us latesum=205418 us SELFCHK=ok
```

A third gap was caught before it shipped: `per_tab` came from `cyc_tab`, which
`lat_report` does not reset, while `cyc_n` is. That asymmetry is visible only because
`cyc=0` and non-zero `per_tab` cannot both be true — **which is why the report prints
`sumtab` beside `per_tab`: a redundant field kept purely so a reader can spot a
broken one.**

**The fix was not more checking. It was making every published number either an
identity or a guarded mean, so there is nothing left to assert.** *A self-check covers
the quantities it sums; it does not cover the ones it does not.*
