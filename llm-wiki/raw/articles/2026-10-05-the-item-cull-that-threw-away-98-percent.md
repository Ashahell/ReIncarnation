# The item cull that threw away 98 % of the build (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-gap-accounting-and-the-item-cull.md)
- Related: [the GAP counter was max where sum](2026-10-05-the-gap-counter-was-max-where-the-arithmetic-said-sum.md), [which section owns the build](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md), [bounding the damage-box build](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md)

With the GAP corrected (previous article) the build became the largest real term at
**112 µs of 233**, and both other terms had already been measured as not-a-lever.
The bug was in the *design*, and it was measurable before any code changed.

## A 64×16 damage box keeps 2 %

`bench_build` prints, per section, what the whole section emits against what a small
damage box keeps:

```
SYNTH1          69    3009    566   2443     2957     19      0     46.27      9.86
808             95    3605    174   3431     3591     51      0     51.85      7.99
909             87    4967    364   4603     4920     67      0     75.87     16.20
LEVI           108    3031    916   2115     2912    212      0     69.11     43.28
TOTAL items 685 | built 26766 | kept 544 (2.0% kept) | us whole 413.5 | us clipped 155.6
```

**The clip drops commands at *push* time, so it saved the replay and never the
drawing.** Every item in the section was still resolved, measured and emitted.

## And it is the item loop, not the background

808 emits **3431 of 3605** commands from 95 items; 909 emits **4603 of 4967** from 87;
SYNTH1 **2443 of 3009** from 69 — **70–93 % of a build, at 35–53 commands per item**.

## The cut

Skip an item whose own `ri_geo_item_box` is disjoint from the clip. Exact, not
approximate: such an item can only have produced commands the clipped replay was
going to drop. Two deliberate non-culls: a shape with **no** declared box is drawn
(*"no box" is not "no pixels"*), and the section background is left alone.

**One formula, two callers.** The box rule moved out of `ri_geo_bbox`'s union loop
into `ri_geo_item_box`, which `ri_geo_bbox` now unions. Had the cull carried its own
copy, the parity test would have been testing the copy.

## On target (`RIPP-CULL`, mixed build 1014432 B, r12moves=42)

```
ITEM-CULL quiet windows (n=11):
  part_avg 177.5 | build 56.2 | replay 34.3 | blit 68.9 | gap 17.6
  BEFORE (2026-10-05, buffered+sum): part_avg 232.6 | build 112 | replay 33 | blit 69 | gap 17.6
```

**Replay, blit and gap do not move — which is the claim.** `xruns=0`, `wake_max=47 µs`.

## `t169` proves safety and deliberately does NOT prove presence

With the cull removed every assertion still passes — the push-time clip produces the
same list, which *is* the equivalence. So the test is the **safety** proof
(over-culling puts a hole on screen; under-culling is merely slow) and `bench_build`
is the **win**. **A test claiming both would be claiming the cull by asserting that
it happened.**

## It caught two of my own bugs, and I had a gap in it

- The 303 strip seek asked for the first strip whose **top** is below the box,
  dropping the strip that **straddles** the edge: `culled 67 commands, the clip alone
  would keep 70`. **My comment already said "seek to the first strip whose BOTTOM edge
  reaches the box's top" and the arithmetic did the opposite.** The comment was the
  specification and the code did not match it.
- Dropping `ox`/`oy` from the cull's test **survived everything**, because production
  only sets a clip where `ox = oy = 0`. **"Equivalent in production" is not "tested";
  the difference is a future refactor.** A third arm at a non-zero origin killed it.

**And a mutant that silently did not apply:** two `str.replace` attempts matched an
8-space indent where the line had 12, so nothing was mutated and the "SURVIVED" was
the clean build passing. **A build that succeeds is not proof that anything was
mutated** — the converse of the wiki's older "a build that fails is not a kill", and
worse because it looks like rigour.
