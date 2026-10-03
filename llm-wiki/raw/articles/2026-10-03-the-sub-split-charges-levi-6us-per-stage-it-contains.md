# The sub-split charges LEVI 6 µs per stage it contains, and the real residual is ~8 µs (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, riqemu1, ABIv1)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [verbatim, four runs](../evidence/2026-10-03-levi-residual-is-the-instrumentation.md)
- Related: [the LEVI sub-split, re-measured](../evidence/2026-10-03-levi-subsplit-corrected-with-a-control.md), [five sections, one culprit](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md), [proving the VM window is actually visible](2026-10-03-proving-the-vm-window-is-actually-visible.md)

This answers the question the last record left open: **where does LEVI's
unattributed time go?** Into the sub-split itself.

## The residual was the instrument

```
levi 213 - (arp 6 + seq 6 + voice 152 + mix 6 + tempo 6) = 37 us
```

Two explanations fit that equally well — LEVI has real uncovered work, or **the
split charges LEVI the cost of every stage pair it contains**, and that lands in
the enclosing stage's interval rather than in any child's reading. One table
cannot separate them.

So a **second control** was added: `lev-probe2`, an empty open/close pair placed
**inside** SLEVI, where `lev-probe` already sat outside it. The same pair, measured
from both sides of the boundary.

| run | inner pairs | levi | lev-voice | residual |
|---|---|---|---|---|
| 2 | 5 | 213 | 152 | **37** |
| 3 | 5 | 238 | 176 | **38** |
| 4 | **6** | 263 | 189 | **44** |

Runs 2 and 3 share a pair count and give 37 and 38. Run 4 adds one pair and the
residual becomes 44 — **+6 µs, exactly one stage pair.** The residual tracks the
*number of stages*, not the music: `lev-voice` moved 152 → 176 → 189 across the
three (different playback windows) while the residual barely budged.

Both controls read 6 µs, inside and outside:

```
lev-probe   (outside SLEVI) : 6 us
lev-probe2  (inside SLEVI)  : 6 us
```

## The arithmetic

```
per-pair cost, from either control     : 6 us
6 inner stages x 6                     = 36 us of split overhead
residual with 6 pairs (run 4)          : 44 us  ->  44 - 36 = 8 us
residual with 5 pairs (runs 2, 3)      : 37, 38  ->  ~7-8 us
```

**About 83 % of the residual was the split measuring itself. The genuinely
unattributed part is ~8 µs, stable at 7, 8, 8 across three runs — roughly 3 % of
LEVI.**

The slope is exactly one pair per stage; the intercept barely moves. That is what
makes this a measurement rather than a plausible story.

## Every enclosing stage has been paying this tax

SLEVI's reported average **includes the cost of its own sub-split**:

```
run 4:  levi 263 - (6 pairs x 6) = 227 us of real LEVI work
        lev-voice 189 - 6        = 183 us of real voice rendering
        reported share : 263 / 538 = 48.9 %
        corrected     : 227 / 538 = 42.2 %
```

**And this is not confined to LEVI.** Any wrapper containing N instrumented
children reads about N × 6 µs high. That is `block` (six always-stages plus five
sections), each section leaf, and SLEVI. **Every section and block figure in every
table recorded so far carries the same tax, and it was never subtracted.**

That is the most transferable thing here. It does not invalidate those tables —
the tax is 6 µs per stage, small against a 351–538 µs block — but a reader
comparing a leaf against its wrapper was comparing numbers that differ by the
wrapper's own child count.

## Where LEVI actually stands

| | share of block |
|---|---|
| first riqemu1 run (the song's first 32 s, no Levi sounding) | 16.2 % |
| **properly powered runs, reported** | **42.7 – 48.9 %** |
| **properly powered runs, corrected for the split's own cost** | **42.2 %** |
| Dell, 2026-09-02 | ~30 % |

`lev-voice` alone is 183 µs corrected, so **the Levi voices are the single
largest item in the block by a wide margin** — larger than 303a and 303b
together in the corrected run.

## Method

- **Put a control on both sides of a boundary.** One probe inside a region and
  one outside turns "is this residual real?" into a subtraction.
- **Vary the instrumentation, not just the workload.** The residual's slope with
  respect to *stage count* is what identifies it; no single table can.
- **Check whether a wrapper is inflated by its own children** before trusting a
  leaf-against-wrapper comparison.

## Cross-lane: the floor, and so the tax, is lane-dependent

The Dell repeat of this same split landed the same day, and its control reads
**4 µs where riqemu1's reads 6**:

```
lev-voice  avg=1042 us      91 % of levi
lev-probe  avg=4    us      THE FLOOR CONTROL
```

So **the per-pair cost is not a constant — it is a property of the machine.** That
refines this record in a way worth stating plainly: the "6 µs per stage" figure is
**riqemu1-specific**, and the tax is roughly **4 µs per stage on the Dell**.

Two consequences, in opposite directions:

- **riqemu1's LEVI figure needs the correction most.** Its reported 48.9 % is
  really 42.2 %, a 6.7-point correction, because its pair cost is the higher of
  the two.
- **The Dell's 64 % is barely inflated.** Six inner stages at 4 µs is about 24 µs
  against a much larger block, so the headline survives essentially intact. **The
  child-count tax is not what explains the 16.2 % / 64 % gap** — that remains a
  configuration and song difference, as the Dell record itself concludes.

And the two lanes now **converge on the same shape**, which is the useful part:

| | riqemu1 (corrected) | Dell |
|---|---|---|
| LEVI share of block | 42.2 % | 64 % |
| `lev-voice` share of LEVI | 183 / 227 = **81 %** | **91 %** |
| control / pair cost | 6 µs | 4 µs |

**`levi_voice_render_sum_stereo` is the target on both machines**, and it is a
*render* rather than the sequencer, arpeggiator or bus mix — which is the same
conclusion the two lanes reached independently, from opposite starting points
(riqemu1's intro-window 16.2 % and the Dell's 64 %).

## Open

- **The remaining ~8 µs inside LEVI.** Small and stable, but still unattributed.
- **Correcting the older tables** for the child-count tax has not been done, and
  the 2026-09-02 Dell figures were taken before any of this was known.
- **Sub-6 µs resolution** is unchanged: both controls read 6, which is also the
  floor, so "cheap" and "never ran" remain indistinguishable on those rows.