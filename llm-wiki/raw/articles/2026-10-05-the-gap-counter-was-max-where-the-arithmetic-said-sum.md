# The GAP counter was `max` where the arithmetic said `sum` (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-gap-accounting-and-the-item-cull.md)
- Related: [the item cull and what it bought](2026-10-05-the-item-cull-that-threw-away-98-percent.md), [which section, and why the LCD is a floor](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md), [a measure-first method, four bugs deep](2026-09-24-a-measure-first-method-four-bugs-deep.md)

For two sessions the largest single term in a routine repaint was reported as a
**~119 µs GAP** — "36 % of every quiet partial, ~104 µs of it real unattributed
time". It did not exist.

## The bug, in three lines

`dp_gap` was `us - max(build, replay, blit)`. **The phases are disjoint intervals**,
so `us - max` equals *the other two phases* plus the true residue. The counter did
not lose 105 µs to the scheduler — it booked the replay and the blit into a bucket
labelled "no phase".

## My own log line refuted it, and I did not look

Every mean on the `RIAPP draw:` line shares one denominator, so they must partition:

```
  buffered      233 == 113 + 34 + 70 + gap  =>  gap = 16     (logged: 121)
  direct-paint  238 == 117 + 105 + 0 + gap  =>  gap = 16     (logged: 119)
```

`121 - 16 = 105 = 34 + 70`, exactly, in both configurations. **Checkable by hand from
any line already in the log.**

## I had the right number a day earlier, then implemented a different formula

The 2026-10-04 record hand-summed the same fields and recorded **17 µs / 7.2 %**,
then justified the instrumentation on it. The 2026-10-05 headline directly
**contradicted my own entry from the day before.**

Corrected on target (`RIPP-SUM`, mixed build 1013880 B, r12moves=42):

```
  quiet windows: part_avg 232.6 | gap_avg 17.6 us  (was reported as 121)
```

## And the "it is not preemption because it is flat" argument was an identity

`us - max` is flat **exactly when the other phases are flat**, and the build is the
largest phase in every logged window — so the gap's flatness carried zero evidential
weight.

`t168_gap_accounting` pins this directly: hold `us` and the largest phase fixed,
move a non-largest one, and the `max`-gap **cannot move** while the `sum`-gap **must**.

## What the corrected budget looks like

A quiet partial is **233 µs = 112 build + 33 replay + 69 blit + 18 unattributed**,
and **~14 of that 18 is my own eight `ReadEClock` calls** at ~2.2 µs each
(`ReadEClock` on AROS x86-64 reads 8254 PIT channel 0 by port I/O). So the
uninstrumented structural floor is **~2 µs**, and the largest real term was the
**build at 112 µs** — which is what the next article is about.

**The standing rule, and the fourth time this shape appeared:** `build_max ≈
part_max` true of one row and not the other; `boxrep`'s "MUI coalesces" premise; a
dead clock pair re-timing the replay; and now `max` where `sum` was documented.
**Every one was an instrument disagreeing with its own documentation, and every time
the documentation was right.** The rule that catches all four: *assert that your
components account for your total.*
