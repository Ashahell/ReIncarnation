# Five sections, one culprit: LEVI is 32 % of the render at `-O0` and 28 % at `-O2`, and owns the worst tail (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane)
- Collected: 2026-10-02
- Published: 2026-10-02
- Prior: [2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md) (the `VOICES` bucket this splits), [2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md), [2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md)
- Commit: `ce4ed05` (the split), `7863e2b` (the r12moves finding). Raw logs: `~/Work/vms/ri-p9/logs/stg-2026-10-02/` (`sec0`, `seco2`)
- Grounding: checked mechanically on 2026-10-02. Every integer and decimal literal in this record was grepped against the cited raw logs. What is verbatim in the logs: all buffer/xrun/`render_max`/`render_total`/`wake_*` figures and all per-stage `avg`/`max`/`n` values. What is **derived** and therefore shown with its components: µs/buffer (= µs/block × blocks-per-buffer, both printed), shares (%), blocks-per-buffer (= dstg n ÷ playing, both printed), speedups and spreads (ratios of two printed figures), and the idle fractions (printed buffer counts and `period`).

## Why the split was placed inside the enable test

`VOICES` was 92.4 % of the render and not yet actionable — one bucket over five
engines. Each section now has its own stage, and **each stage sits inside its own
`if (e->sections & ...)`.** That placement is the whole point: with the five timed
as one bucket, a section that renders while *disabled* still produces a number, and
a cheap section is indistinguishable from one that never ran. Gating them makes
`n=0` mean "never ran".

In these runs all five are enabled (`snd=0/0/0/0` means all four tracks are
sounding pattern slot 0 — slot 0 is a valid index, not an absence), so all five
report.

## The result (`sec0`, `-O0`)

```
RIAPP closed: buffers=38038 xruns=3678 render_max=6960 us render_total=175982 ms
period=5333 us wake_max=5821 us wake_total=51089 ms wake_n=38036
stg_total_avg=5071 us stg_dsp_avg=4993 us stg_evt_avg=31 us stg_playing=34567
RIAPP dstg n=135134
RIAPP dstg zero   avg=5 us max=36 us
RIAPP dstg delay  avg=7 us max=29 us
RIAPP dstg comp   avg=4 us max=26 us
RIAPP dstg master avg=4 us max=27 us
RIAPP dstg meter  avg=5 us max=35 us
RIAPP dstg limit  avg=5 us max=29 us
RIAPP dstg 303a   avg=185 us max=234 us
RIAPP dstg 303b   avg=178 us max=224 us
RIAPP dstg 808    avg=198 us max=265 us
RIAPP dstg 909    avg=174 us max=316 us
RIAPP dstg levi   avg=398 us max=773 us
RIAPP dstg block  avg=1220 us max=1723 us
```

Blocks per buffer `135134 / 33186 = 4.072`. Per buffer, and as a share of the
5071 µs total:

| section | µs/block | **µs/buffer** | share | max µs/buffer | max/avg |
|---|---|---|---|---|---|
| **LEVI** | 398 | **1621** | **32.0 %** | 3149 | **1.94** |
| 808 | 198 | 806 | 15.9 % | 1079 | 1.34 |
| 303A | 185 | 754 | 14.9 % | 953 | 1.26 |
| 303B | 178 | 725 | 14.3 % | 912 | 1.26 |
| 909 | 174 | 709 | 14.0 % | 1287 | **1.82** |
| *five sections* | 1133 | **4615** | **91.0 %** | | |
| delay + comp | 11 | 45 | 0.9 % | | |
| zero/master/meter/limit | 19 | 77 | 1.5 % | | |

## LEVI, and only LEVI, is out of line

**The four other engines form a tight cluster: 174, 178, 185, 198 µs/block — a
spread of 11 %.** LEVI is 398, which is **2.0× the largest of them and 2.3× the
smallest**. There is no second cluster and no outlier among the four; this is not
"five roughly equal costs with one slightly worse", it is four equal costs and
one that costs twice as much.

LEVI also owns the **worst tail**: `max/avg` 1.94 against 1.26–1.34 for 303A/303B/808.
909 is the other heavy tail at 1.82, and its single worst block (1287 µs/buffer)
is the second-worst thing in the render. Since an xrun is caused by the tail and
not the mean, both of those are the numbers that matter.

LEVI's worst single block, 3149 µs/buffer, is **59 % of the 5333 µs period in one
stage**.

## The same picture at `-O2` (`seco2`), and it is uniform

```
RIAPP closed: buffers=40927 xruns=0 render_max=3229 us render_total=88919 ms
period=5333 us wake_max=47 us wake_total=781 ms wake_n=40925
stg_total_avg=2368 us stg_dsp_avg=2288 us stg_evt_avg=32 us stg_playing=37318
RIAPP dstg 303a avg=76 us   RIAPP dstg 303b avg=74 us
RIAPP dstg 808  avg=82 us   RIAPP dstg 909  avg=78 us
RIAPP dstg levi avg=165 us  RIAPP dstg block avg=556 us
```

| section | `-O0` µs/block | `-O2` µs/block | speedup | share `-O0` | share `-O2` |
|---|---|---|---|---|---|
| LEVI | 398 | 165 | **2.41×** | 32.0 % | 28.4 % |
| 808 | 198 | 82 | 2.41× | 15.9 % | 14.1 % |
| 303A | 185 | 76 | 2.43× | 14.9 % | 13.1 % |
| 303B | 178 | 74 | 2.41× | 14.3 % | 12.7 % |
| 909 | 174 | 78 | 2.23× | 14.0 % | 13.4 % |
| block TOTAL | 1220 | 556 | 2.19× | | |

**Every section speeds up by the same 2.2–2.4×.** That is the useful negative
result: the optimiser is not selective here, so there is no engine that is
"already fine" and none that is pathologically unoptimised, and there is no
per-section `-O` decision to make. LEVI's share drops only 32.0 % → 28.4 %, and
only because it is the largest term while the block's fixed ~18 µs of non-section
work does not shrink.

`xruns` 3678 → **0**, `render_max` 6960 → 3229 µs, `wake_max` 5821 → **47 µs** —
the third `-O2` run to reach zero xruns.

## An honesty correction about the `-O0` series

Three identical `-O0` play-only runs have now produced **2320, 3116 and 3678**
xruns — a spread of 1.58×. Earlier records quoted 2320 and 3116 as a close pair,
and this one should not be read as 3678 being a regression against 3116: the
`-O0` xruns are **not tightly repeatable** and the spread was not recorded before
now. The `-O2` result is robust precisely because it is zero (0, 0, 0 across three
runs) rather than merely small.

That also means fine-grained `-O0` comparisons — this table against the previous
one — should be read as "the same picture, ±" and not as a measured 18 % increase
in cost from adding the section stages. The section *proportions* are stable
across all three runs to within 1 %.

## What this does and does not say

- **Actionable:** LEVI is the single biggest thing in the render, at twice the
  cost of any other engine, with the heaviest tail. It is where effort belongs.
- **Not yet actionable:** LEVI's own block is five calls —
  `levi_set_tempo`, `levi_arp_block`, `levi_seq_block`,
  `levi_voice_render_sum_stereo`, `engine_section_stereo` — timed together. Which
  of those carries the 398 µs is not established, and the obvious next split.
- **The tail is the target, not the mean.** 909's 1.82 max/avg is nearly LEVI's
  1.94 on a much smaller base, and a mean-only view of this table would have
  hidden it entirely.
- **Nothing here revises the FX-chain finding.** Delay plus comp is 45 µs/buffer
  at `-O0` and 37 µs at `-O2`.
- **Nothing here revises the optimisation finding**, and nothing here licenses
  shipping `-O2`: the [r12moves record](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md)
  leaves the v11 LVO convention unresolved.

## Method findings

- **A bucket that reports one number hides both the outlier and the shape.** Four
  engines at 174–198 µs and one at 398 µs averaged to a single "VOICES 1112 µs"
  that looked like a uniform cost. The actionable fact — *twice as expensive as
  anything else, with the worst tail* — was not visible and could not have been
  inferred from the bucket.
- **Gate each stage on the condition that decides whether it runs.** That is what
  makes `n=0` mean "never ran" instead of "cheap", and it is why the disabled-
  section mutants are killable at all.
- **A test fixture must vary the condition, not just assert it once.** Checking one
  section mask and assuming the rest is not enough: with only 303A enabled, a
  collision between the 808 and 303B buckets is invisible because both are zero
  either way. Exercising each section alone makes every pair distinguishable.
- **Report max/avg next to the mean.** The two heavy tails (LEVI 1.94, 909 1.82)
  against three tight ones (1.26–1.34) is a different story from the means alone,
  and the means alone would have said "909 is the cheapest section".
- **A repeated measurement's spread is part of the measurement.** The `-O0` series
  spread 1.58× across three identical runs, and until this run that was not
  written down anywhere — which means earlier quoted `-O0` figures carried an
  unstated uncertainty of that size.
- **The host laws found two real instrumentation bugs that the device could not
  have named**: `TOTAL` sharing a timestamp variable with the stages nested inside
  it, and a stage closed without being opened — the latter invisible at `-O0`
  because the stage preceding it had adjacent reads, and exposed only once
  sections were enabled one at a time.

## Standing gaps

- LEVI's internal split: `arp` vs `seq` vs `voice_render_sum_stereo` vs the
  stereo section mix. Same pattern, same gates, one more table.
- 909's tail: 174 µs average against a 316 µs worst block.
- Tab-switch xruns, the guest's 300–500 ms stalls, and the in-memory ring are all
  untouched by this.
- The v11 LVO convention, which gates the `-O2` decision.

## See Also

- [Render-stage breakdown: the voice render is 94 % of the cost](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md)
- [Optimisation level alone removes every xrun on the Dell](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md)
- [`r12moves` is an inlining counter, not an ABI hazard](2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md)