# A ratio, not microseconds: this host's timings are bimodal (2026-10-05)

- Source: ReIncarnation session, 2026-10-05 (opencode lane, host benches, ABIv11)
- Collected: 2026-10-05
- Published: 2026-10-05
- Raw: [verbatim](../evidence/2026-10-05-which-section-and-why-the-lcd-is-a-floor.md)
- Related: [which section owns the build](2026-10-05-which-section-owns-the-build-and-why-the-lcd-is-a-floor.md), [a host bench and the overload guard](2026-10-04-host-bench-and-the-overload-guard.md)

I proposed hoisting the item cull above `ri_ctlreg_find` and `ri_sui_value`, so a
rejected item would not pay a registry search and a value read. It looked free: the
cull's justification is unchanged either way.

**The first measurement read as a regression** — the disjoint-clip build went
3.62 → 4.50 µs — and I nearly reverted the idea on that basis.

## The regression was an artefact

Six runs of the same bench, same binary, same tree:

```
  unclipped 10.17 us   disjoint 4.53 us
  unclipped 10.41 us   disjoint 4.45 us
  unclipped  9.68 us   disjoint 3.59 us
  unclipped 10.53 us   disjoint 4.52 us
  unclipped  9.55 us   disjoint 3.64 us
```

**Two modes, ~25 % apart, and `unclipped` tracks `disjoint` exactly.** That is CPU
frequency, not the change. **Comparing absolute microseconds across runs on this
machine measures the governor.**

## So the bench prints a within-process ratio

Two builds in the same process share the frequency; two runs do not.

```
PRE-HOIST   floor/unclipped = 0.3762, 0.3753, 0.3720
POST-HOIST  floor/unclipped = 0.4301, 0.3682, 0.3634, 0.4166
```

Pre-hoist is tighter and slightly better, so the hoist was reverted. And there is a
mechanism for why it cannot help: the old code called `ri_geo_item_box` for every
item anyway — **it is the cull's own test** — so hoisting saves only the two lookups,
which on a registry this size are evidently near-free, while making
`ri_geo_item_box` run for items that `if (!d) continue;` would have skipped.

## What this host's numbers are still good for

The bench remains valid **within** a run and **across** runs for *ratios and for
counts*. `kept 97 of 829`, `544` survivors matching the unclipped reference, and
`46.0 ns per SURVIVING command` are all exact-control quantities — a fixed number of
commands and a fixed number of repetitions — and none of them moved by 25 %.

**A bench that prints absolute microseconds across runs invites exactly the false
regression I read. That is an instrument lying in the most expensive way available,
because it looks like a result.**
