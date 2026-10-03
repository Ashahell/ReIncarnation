# On the Dell, LEVI is 64 % of the block and 91 % of it is one call: `levi_voice_render_sum_stereo`

- Source: ReIncarnation session, 2026-10-03 (opencode lane; the like-for-like repeat this lane's own cascade recorded as owed)
- Collected: 2026-10-03
- Published: 2026-10-03
- Prior: [five sections, one culprit: LEVI is 32 percent](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md) (whose standing gap this closes), [The LEVI sub-split is measured](2026-10-03-the-levi-sub-split-measured-arp-seq-voice-and-mix-are-all-at-the-floor.md) (the riqemu1 half), [LEVI sub-split corrected, with a control](../evidence/2026-10-03-levi-subsplit-corrected-with-a-control.md) (the `lev-probe` floor control — an evidence record, not an article)
- Evidence: [`docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`](../../../docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md) §"The like-for-like LEVI repeat, ON THE DELL"
- Binary: `RIAPP-h` 1,099,200 B, `r12moves=41`, HEAD `84f46cc`, `-O0`, ABIv11

## What was owed

The 2026-10-02 record left this open:

> **Not yet actionable:** LEVI's own block is five calls … timed together. Which
> of those carries the 398 µs is not established, and the obvious next split.

A cascade in this lane later restated it more sharply, after the first split
came back from **riqemu1** with all four internals at the timer floor and 54 % of
LEVI unattributed:

> *"why do the Dell and riqemu1 distribute LEVI differently, and which is right
> for the Dell?"* — and **the like-for-like repeat is owed on the Dell**, because
> every audio measurement in this lane is a Dell measurement.

That is this record. `84f46cc` supplied the fifth sub-stage (`levi_set_tempo`)
and a **control**: `RI_ENGINE_ST_LEVPROBE`, an empty T/E pair that measures the
per-pair floor directly rather than leaving it to be inferred.

## The measurement

The Knife, 104 bars at 124 BPM, 4,344 playing buffers, `-O0`, live AHI
(`mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us`):

```
block      avg=1767 us      levi   avg=1139 us  (64 % of block)
lev-arp    avg=4    us      AT FLOOR
lev-seq    avg=4    us      AT FLOOR
lev-voice  avg=1042 us      91 % of levi
lev-mix    avg=58   us      5 % of levi
lev-tempo  avg=4    us      AT FLOOR
lev-probe  avg=4    us      THE FLOOR CONTROL
303a 185   303b 183   808 88   909 77
hb: xruns=1586 render_max=8868 us wake_max=5819 us overloads=6 load=1193/1000
```

## The answer, and it is the opposite of riqemu1's

| | riqemu1 (ABIv1) | **Dell (ABIv11)** |
|---|---|---|
| LEVI share of block | 16.2 % | **64 %** |
| `lev-voice` | 8 µs (floor) | **1,042 µs — 91 % of LEVI** |
| `lev-mix` | 6 µs (floor) | 58 µs — 5 % |
| `lev-arp` / `lev-seq` / `lev-tempo` | floor | floor |
| internals vs LEVI | 26 of 57 µs (46 %) | **1,116 of 1,139 µs (98 %)** |
| **unattributed** | **31 µs = 54 %** | **23 µs = 2 %** |

**On the Dell, `levi_voice_render_sum_stereo` carries the cost** — 91 % of LEVI,
and LEVI is 64 % of the block. The 2026-10-02 record's "LEVI is 32 %, the
outlier" was directionally right and, on this song at this configuration, much
too low.

riqemu1's "54 % of LEVI unattributed" is therefore **not a mystery about LEVI**.
It is what the same code looks like when `levi_voice_render_sum_stereo` is itself
at the floor there — the lane measures something real, just not the same thing.

## The control closes this lane's own floor caveat

This lane recorded, in the correction article, that the sub-stage rows carry
**no `n`**, so a floor reading cannot be told apart from one that never ran, and
flagged three of the four Dell `lev-*` rows as unquotable for that reason.

`lev-probe` answers it directly: **an empty T/E pair reads 4 µs — exactly what
`lev-arp`, `lev-seq` and `lev-tempo` read.** The three at-floor rows are
*measured as doing nothing*, not merely too cheap to resolve. That closes the
caveat for this configuration, and it is the right way to close it: a control
that reads the floor beats an argument about what the floor means.

## What this means

The render-stage question has an answer at last, on the machine that matters:

> **`levi_voice_render_sum_stereo` is the target.** Not LEVI as a bucket, not the
> arp/seq/mix split, and not a timer floor artefact.

Two cautions stated rather than buried:

- **One song, one configuration.** The Knife, `-O0`, The Knife's section
  enablement. The 2026-09-02 Dell figure had LEVI at ~32 %; this one has it at
  64 %. Both are single-song, single-configuration readings, and the sibling's
  record already notes that song and enablement move these proportions. **A
  second song on the Dell would tell us whether 64 % is "LEVI on The Knife" or
  "LEVI on this guest".**
- **`lev-voice` is `levi_voice_render_sum_stereo`**, so the target is a *render*,
  not a DSP kernel, and it is timed as one block. Splitting *it* is the same kind
  of step this record just performed on LEVI, and the same control discipline
  would apply.

## Method findings

- **A cross-lane discrepancy is settled by repeating on the machine whose numbers
  you are actually reasoning about.** riqemu1 and the Dell were never in
  conflict; they were measuring different configurations, and only the Dell
  reading bears on this lane's conclusions.
- **A control row earns its keep by making a caveat unnecessary.** `lev-probe`
  costs one empty timer pair and retires an entire class of "is that floor or is
  that nothing?" reasoning.
- **`64 % of the block` beside `16.2 %` is a configuration difference, not a
  contradiction** — and the way to tell is the `lev-voice` row, not the total.
  One row at 1,042 µs explains both figures; the totals alone would have invited
  a false refutation in either direction.