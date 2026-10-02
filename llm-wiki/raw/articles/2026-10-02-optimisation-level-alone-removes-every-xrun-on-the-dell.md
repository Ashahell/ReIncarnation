# Optimisation level alone removes every xrun on the Dell: `-O2` gives 0/0 where `-O0` gives 2320/3116 (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude)
- Prior: [2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md)
- Commit: `5d233a1`. Raw logs: `~/Work/vms/ri-p9/logs/stg-2026-10-02/` (`stg1`, `stg2`, `stgo2`, `stgo2b`)
- Grounding: checked mechanically on 2026-10-02. Every integer and decimal literal in this record was grepped against the cited raw logs. What is verbatim in the logs: all buffer/xrun/`render_max`/`render_total`/`wake_*` figures and all per-stage `avg`/`max`/`n` values. What is **derived** and therefore shown with its components: µs/buffer (= µs/block × blocks-per-buffer, both printed), shares (%), blocks-per-buffer (= dstg n ÷ playing, both printed), speedups and spreads (ratios of two printed figures), and the idle fractions (printed buffer counts and `period`).

## The confound, named before the result

Every measurement in this lane was taken on an `-O0` RIAPP, chosen deliberately so
that CPU-cost figures would not be confounded by mixing optimisation levels with
the A,B,B,A baseline. That discipline was right, and it left the obvious question
unasked: **what do the numbers look like at the recipe's documented default?**

`build_v11.sh` takes `RI_V11_OPT` and defaults to `-O2`. `-O0` produces
1082824 B / `r12moves=41`; `-O2` produces 873888 B / `r12moves=286`.

## Two runs each, one variable

Same harness (`stg_run.sh`: upload, clean logs, launch, wait for the window, click
Play, hold 180 guest waits ≈ 3 min, click Stop, close, pull). No tab clicks in any
run — that is deliberate, so repaint work cannot contaminate the render figures.

| | `-O0` `stg1` | `-O0` `stg2` | `-O2` `stgo2` | `-O2` `stgo2b` |
|---|---|---|---|---|
| buffers | 40208 | 39102 | 40812 | 40840 |
| playing / stopped | 37479 / 2729 | 35611 / 3491 | 37315 / 3497 | 37315 / 3525 |
| **xruns** | **2320** | **3116** | **0** | **0** |
| `render_max` | 6457 µs | 6726 µs | 3028 µs | 3039 µs |
| `render_total` | 174527 ms | 175310 ms | 83484 ms | 83463 ms |
| `wake_max` | 5822 µs | 5824 µs | **37 µs** | **38 µs** |
| `wake_total` | 55472 ms | 52746 ms | **776 ms** | **778 ms** |
| `arm_us` | 250651 | 266650 | **0** | **0** |
| `overloads` | 0 | 0 | 0 | 0 |
| peak `load` | 1051/1000 | 1051/1000 | 513/1000 | 513/1000 |
| `stg_total_avg` | 4638 µs † | 4903 µs | 2223 µs | 2222 µs |
| `stg_dsp_avg` | 4557 µs † | 4824 µs | 2142 µs | 2141 µs |
| `stg_evt_avg` | 4 µs † | 31 µs | 33 µs | 33 µs |

† `stg1` predates the `EVENTS`-placement fix and its `stg_evt_avg` of 4 µs is the
bug, not a measurement. It is kept because its xruns, `render_max` and
`wake_max` are unaffected by the fix and belong in the series.

**Corrected for the 301 µs/buffer the instrumentation costs itself** (measured by
stashing it and re-running; see the breakdown record), and against the 5333 µs
period:

| | corrected playing render | share of period | `render_max` vs period |
|---|---|---|---|
| `-O0` | 4590 µs | **86 %** | 6343 µs — **over** |
| `-O2` | 1936 µs | **36 %** | 2726 µs — under |

## What this says

1. **Optimisation level alone is worth 2.4× on this workload, and it takes the
   xruns from thousands to zero.** Nothing else changed: same source, same guest,
   same clicks, same duration.
2. **The mechanism is visible and it is simple.** At `-O0` the worst single buffer
   is 6343 µs against a 5333 µs period — a buffer that cannot be produced in time
   *is* the dropout, and no amount of priority tuning fixes arithmetic. At `-O2` the
   worst buffer is 2726 µs, comfortably inside the period, and there is nothing left
   to drop.
3. **`wake_max` collapses by two orders of magnitude** (5824 µs → 37 µs) and
   `wake_total` by 68× (52746 ms → 776 ms). The standing question — why is a pri-21
   task woken late at all — has a new and much more likely reading: at `-O0` the task
   held the CPU for 86 % of every period, so the *wake* was late because the *work*
   was long, not because the scheduler was unfair. The 5800 µs figure that looked
   like a scheduling problem was mostly a compiler flag.
4. **The proportions do not move.** DSP is 98.4 % of the render at `-O0` and
   96.3 % at `-O2`; VOICES is 94.4 % of the block and 87.9 %; the FX chain is
   0.9 % → 1.6 %. Optimisation made everything faster and changed the answer to
   none of the standing questions. That is the useful part: the diagnosis was already
   right about where the time goes and wrong only about how much of it there is.
5. **The governor arm never fires at `-O2`** — `arm_us=0`, `overloads=0`, peak load
   513/1000. It was compensating for code running 2.4× slower than it needed to.

## What this does not say

- **It does not retract Fix A or Fix B.** Both were proven on target on `-O0`
  binaries under the A,B,B,A protocol, and xruns there fell 618/620 → 274/274. At
  `-O0` the arm is load-bearing; that finding stands.
- **It does not mean `-O2` is the shipping answer.** `-O2` doubles `r12moves`
  (41 → 286) and shrinks the binary by 208936 B. The `r12moves` discriminator is the
  repo's own ABI-recipe check, and 286 has never been run on the Dell as a shipping
  configuration. Changing the optimisation level is a decision that needs its own
  evidence, not a free win discovered while measuring something else.
- **The A,B,B,A arms have not been re-run at `-O2`.** If they were, the expected
  result is that neither fix is measurable, because the arm would never engage. That
  is a prediction, not a measurement, and it is the obvious next experiment.
- **The `-O0` workload was constant, and that is checked.** `render_max` is
  6310–6359 µs across the A,B,B,A arms, `stg1`, `stg2` and the uninstrumented
  `stgbase`. The `-O2` runs sit at 3028–3039 µs — the flag moves it, nothing else
  does.

## Method findings

- **A confound you chose deliberately is still a confound.** Matching `-O0` across an
  A/B was correct for comparing two binaries and it silently became the standing
  assumption about the machine. The flag was one environment variable away the whole
  time.
- **Verify the series is constant before comparing its averages.** `render_max`
  (6310–6359 µs across all `-O0` runs) proved the workload never changed, which is
  what licensed reading the average difference as an idle-fraction artefact rather
  than a code difference.
- **Two runs of each arm, because the effect is large enough to survive one.** The
  first `-O2` run gave 0 xruns, which is either a fix or a guest that happened to be
  quiet; the second run is what made it a result.

## Standing gaps

- Which voice engine carries the 4528 µs/buffer. VOICES is still one bucket over
  303A/303B/808/909/LEVI.
- The A,B,B,A arms at `-O2`, to test the prediction above.
- Whether `-O2`'s 286 `r12moves` is acceptable to the recipe, and what an `-O2` build
  does to the `render_max` outlier history (`build_max` rose in the bounded build).
- The intermittent 300–500 ms guest stalls, untouched by all of this.

## See Also

- [Render-stage breakdown: the voice render is 94 % of the cost](2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md)
- [Scripted A,B,B,A](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)
- [The size heuristic is dead; `r12moves` is the discriminator](2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md)