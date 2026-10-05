# The bounded scan that needed no ordering assumption (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, Dell E6320, ABIv11, mixed build)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-the-bounded-scan-and-the-plus-one-line.md)
- Related: [which section owns the build, and why the LCD is a floor](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md), [render-stage close-out](2026-10-05-render-stage-close-out-three-floors-and-my-own-clock.md), [the item cull that threw away 98 %](2026-10-05-the-item-cull-that-threw-away-98-percent.md)

The clipped transport build is ~37 % "floor" — background plus the items the item
cull deliberately cannot skip. I guessed the registry lookups. Measured inside one
process:

```
31x ri_ctlreg_find()  1.971 us | disjoint-clip build 4.513 us  => 44%
31x ri_ctlreg_find()  2.015 us | disjoint-clip build 4.468 us  => 45%
```

**45 % of the floor was one linear scan.**

## The cost is positional, which is the whole problem

`RI_CTLREG_N = 485`, and the lookup was a linear scan, so a repaint paid a depth
proportional to where a control sat in the table:

```
transport items 31 | scan depth: total 6949 -> mean 224.2 entries (of 485)
```

Per section: SYNTH1 mean depth 16, 808 82, 909 128, **TRANSPORT and everything
after it 192–215**. **The controls the lane actually repaints are the most expensive
in the table, purely by position.**

## Three approaches; the third was correct

| approach | verdict |
|---|---|
| global binary search | **refuted** — ids invert at section boundaries (`first inversion at 190: 2048 after 5127`) |
| binary search inside a per-section range | **refuted** — ids invert *inside* a section too |
| **bounded linear scan of the section's run** | **ships** |

The second was the one that looked safest — literal bounds, no mutable state, 6
comparisons instead of 224 — and it failed:

```
reg_id 4833: binary search gave NULL, linear scan gave an entry
```

The cause:

```
table[445] reg_id=4835 sec=18 idx=227
table[446] reg_id=4822 sec=18 idx=214
```

**LEVI's run is ordered by sub-panel, each sub-panel ascending in `reg_id`, the
sub-panels themselves not in idx order.** So the table is grouped by section and then
by sub-panel, and **neither grouping ascends**. A bounded *linear* scan does not care
about order at all; contiguity is the only property it needs, and `t170` verifies
that against the table for all 21 sections.

**The boring answer was available from the first measurement. Two entries were spent
proving the clever ones wrong first.**

## Result

```
31x ri_ctlreg_find()  0.069 us | disjoint-clip build 2.014 us  => 3%
```

**A 30× cut on the lookup, and the floor's share falls from 45 % to 3 %.** On target
(`RIPP-SCAN`): **build 55.4 → 45.9 µs, part_avg 175.9 → 167.0**, replay and blit
unmoved, partition invariant 34 rows / 0 mismatches, `xruns=0`.

**The host predicted 1.62× and the Dell delivered 1.21×.** The host cut is arithmetic
(224 comparisons to 15), so it should transfer. It does not: at `-O2` the scan is
branch-bound, while at `-O0` on a 2010-era mobile chip the 485-entry table is **cold
cache** and the win is bounded by memory, not comparisons. **`829 → 97 kept` and
`224 → 15 comparisons` are both exact, and neither is a time.**

## And the measurement that misled me, twice

The probe that "proved" every section ascends stored `prev = i` — **the index, not the
reg_id** — so it compared a `reg_id` against an index and reported `ascending: yes`
for all 21 sections. I believed it, wrote the range search on it, installed it, and
reverted it.

Then, after that revert, I probed again and concluded the failure **"does not
reproduce."** It did not reproduce **because the offending code was gone.** The
original reading of `23` rather than `239` was likewise a stale `art_shared.o`.
**"I reverted it and it went away" is an observation of one tree; "it does not happen"
is a claim about the code. Only one of them is evidence, and I made the second from
the first twice in one entry.**
