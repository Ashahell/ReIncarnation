# Render-stage breakdown: the voice render is 94 % of the cost, and the FX chain is not the problem (2026-10-02)

- Source: ReIncarnation session, 2026-10-02 (opencode lane)
- Collected: 2026-10-02
- Published: 2026-10-02
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P9 interlude)
- Prior: [2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md), [2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md)
- Commit: `5d233a1`. Raw logs: `~/Work/vms/ri-p9/logs/stg-2026-10-02/` (`stg1`, `stg2`, `stgo2`, `stgo2b`, `stgbase`). The A,B,B,A figures used in the 53 % section come from a **different** directory, `~/Work/vms/ri-p9/logs/ab-2026-10-02/` (`B1.stick.log`, `B2.stick.log`)
- Grounding: checked mechanically on 2026-10-02. Every integer and decimal literal in this record was grepped against the cited raw logs. What is verbatim in the logs: all buffer/xrun/`render_max`/`render_total`/`wake_*` figures and all per-stage `avg`/`max`/`n` values. What is **derived** and therefore shown with its components: µs/buffer (= µs/block × blocks-per-buffer, both printed), shares (%), blocks-per-buffer (= dstg n ÷ playing, both printed), speedups and spreads (ratios of two printed figures), and the idle fractions (printed buffer counts and `period`).

## Why this was measured rather than argued

The standing open question was "where does the render's 2806 µs/buffer go". Every
answer so far had been an aggregate, and one of those aggregates had already been
caught being an average over an idle session. So the first act was to build the
measurement, and the second was to distrust it.

Two tables, both injected-clock, both off unless a clock is handed over:

| table | granularity | stages |
|---|---|---|
| `struct RILiveStages` (`engine/live.h`) | per **buffer** | STOPPED, EVENTS, SORT, FILTER, LOAD, DSP, METERS, TOTAL |
| `struct RIEngineStages` (`engine/engine.h`) | per **block** | ZERO, VOICES, DELAY, COMP, MASTER, METER, LIMIT, TOTAL |

The engine has no OS call, so both clocks are function pointers, `NULL` by
default, armed in exactly one place — `au_live_run`, the AHI backend. The host
path pays one predictable branch per boundary and nothing else.

## The result (`stg2`, `-O0`, play-only, 180 guest waits)

```
RIAPP closed: buffers=39102 xruns=3116 render_max=6726 us render_total=175310 ms
period=5333 us wake_max=5824 us wake_total=52746 ms wake_n=39100
stg_total_avg=4903 us stg_dsp_avg=4824 us stg_evt_avg=31 us
stg_playing=35611 stg_stopped=3491
RIAPP dstg n=144928
RIAPP dstg zero   avg=5 us max=36 us
RIAPP dstg voices avg=1112 us max=1606 us
RIAPP dstg delay  avg=7 us max=29 us
RIAPP dstg comp   avg=4 us max=37 us
RIAPP dstg master avg=4 us max=26 us
RIAPP dstg meter  avg=5 us max=27 us
RIAPP dstg limit  avg=5 us max=28 us
RIAPP dstg block  avg=1179 us max=1674 us
```

Blocks per buffer: `144928 / 35591 = 4.072`.

| | avg µs | share |
|---|---|---|
| **DSP (`ri_engine_render`)** | **4824** | **98.4 % of TOTAL** |
| EVENTS (chase, player, automation, control drain) | 31 | 0.6 % |
| SORT + FILTER + LOAD + METERS | 16 | 0.3 % |
| unattributed (TOTAL − parts) | 32 | 0.65 % |
| TOTAL | 4903 | |

Per block, scaled by 4.072 blocks/buffer:

| | µs/buffer | share of TOTAL 4903 |
|---|---|---|
| **VOICES (303A/303B/808/909/LEVI)** | **4528** | **92.4 %** |
| DELAY + COMP (the FX chain) | 45 | 0.9 % |
| ZERO, MASTER, METER, LIMIT | 77 | 1.6 % |

**The standing suspicion — that the FX chain over the voices was the cost — is
refuted at both optimisation levels.** Delay plus comp is 45 µs/buffer at `-O0`
and 37 µs/buffer at `-O2`. It cannot produce 3116 xruns, and it does not become
relatively more expensive under optimisation: it shrinks with everything else and
its share moves 0.9 % → 1.6 %.

The sequencer side is not hiding anywhere either. Gathering, sorting, filtering
and loading events for the whole 256-frame buffer costs 51 µs.

## The instrumentation was measuring itself, and I had to subtract it

The `-O2` runs are clean (0 xruns), but every absolute figure above includes the
cost of my own clock reads: 14 per buffer in `live.c` plus 4.072 × 16 per block in
`engine.c` is **79 `ReadEClock` calls per buffer**, sitting inside the render they
are timing.

Measured directly rather than guessed — same script, `-O0`, current tree with the
instrumentation stashed (`stgbase`):

```
RIAPP closed: buffers=40705 xruns=1977 render_max=6343 us render_total=170241 ms
period=5333 us wake_max=5808 us wake_total=53301 ms wake_n=40703
```

| | µs/buffer | render_max |
|---|---|---|
| uninstrumented | 4182 | 6343 |
| instrumented | 4483 | 6726 |
| **difference** | **301** | **383** |

301 µs over 79 reads is **3.8 µs per `ReadEClock`** — not the sub-microsecond
hardware read I had assumed. `render_max` rose by 383 µs, i.e. the instrumentation
also inflates the number the governor acts on.

Corrected, the `-O0` playing render is **4590 µs/buffer = 86 % of the 5333 µs
period**, not the 4903 the instrumented table reports. The correction is 6.4 %, so
the *proportions* — the actual finding — are unaffected; the absolute numbers needed
a control to be quotable at all.

## The 53 % figure in the A,B,B,A record is also a session average

`render_max` agrees across every run ever taken on this guest: 6310–6359 µs in the
A,B,B,A arms, 6343 µs in `stgbase`. Same workload. So the difference between the
A,B,B,A's 2806 µs/buffer and this run's 4182 µs/buffer is *not* the workload — it is
how much of each session was playing.

Solving `render_total` against a 9 µs idle render for each run's own duration:

```
stgbase: 40705 buffers = 217.1 s,  170241 ms  ->  197.7 s playing, 19.4 s idle (8.9 %)
B1/B2:   8515 buffers =  45.4 s,   23895 ms  ->   27.7 s playing, 17.7 s idle (39 %)
```

The A,B,B,A script clicks Play, waits, walks five tabs with four waits between
each, stops — about 28 s of playback inside a 45 s session. The idle fraction is
39 %, and `0.61 × 4590 + 0.39 × 9 = 2803` against the measured
`23895000 / 8515 = 2806.2`. The 2.6 µs is the rounding in a 39 % fraction, not slack
in the argument — and the raw pair `8515` / `23895` is verifiable in `B1.stick.log`.

So the record that withdrew *"the render task uses 32.8 µs of every 5333 µs, about
0.6 % of a core"* replaced one session average with another, less idle one. The
honest playing figure is **86 % of the period, not 53 %**. Same category error, one
step harder to see because the new number is closer to the truth.

This is not an argument that the A,B,B,A was worthless — it compared two binaries
under one protocol and got the right answer. It is an argument that its throughput
figure must not be quoted as a per-playing-buffer cost.

## Method findings

- **An aggregate is not a breakdown, and a breakdown is not trustworthy until you
  have subtracted yourself from it.** 301 µs of the 4903 was my instrumentation.
  Without the stashed control, every number would have been 6.4 % too high and
  nobody would have known.
- **A synthetic clock cannot see a stage that measures nothing.** With a clock that
  advances a fixed tick per *read*, a stage opened and closed back to back costs
  exactly the same as a stage that spans real work — both are one tick. The zero-width
  `EVENTS` bug (see below) was invisible to the unit test and obvious on the target,
  where 61 µs sat between `TOTAL` and the parts with no stage claiming it.
- **"No unattributed gap" is a law of the device, not of the model.** On the guest
  the gap must be ~0; with a per-read clock the wrapper legitimately exceeds the sum
  of its leaves by exactly the extra reads. Asserting the model version would have
  been asserting something false.
- **Two real bugs, both caught by the tests, neither by inspection:**
  `TOTAL` and the stages nested inside it shared one timestamp variable, so `TOTAL`
  measured from the last inner stage instead of from its own start; and `EVENTS` was
  opened *and closed* immediately after the event gather. The first made `TOTAL`
  bracket nothing; the second hid 61 µs/buffer.
- **A wrapper measurement and a leaf measurement cannot share a variable.** Same
  trap, two levels up and two levels down; the unit law for it is that the wrapper
  spans exactly `2 × COUNT − 1` clock reads.
- **`ri_build_host.sh test NAME` links the object files already in `/tmp/ri/build`
  and does not rebuild them.** A stale `live.o` made a genuine fix look like a
  failing test, and cost a full diagnostic cycle. Always `ri_build_host.sh engine`
  (or `all`) first.
- **Two clock hooks, deliberately not cascaded.** Arming the session clock into the
  engine too is tidier and is wrong: the engine's nested reads inflate any clock
  that advances per read, and `t88` — calibrated for exactly two reads per buffer —
  fails all nine of its governor laws when it happens. The test now asserts the
  independence and the read count of both setters, so it cannot be "tidied" back.
- **A pre-existing ASan failure, not mine, confirmed by stash:** `t88` fails 9 laws
  under ASan+UBSan and passes in the normal build, identically with and without this
  work. Left alone and recorded; it is a timing-sensitive governor test.
- **Guarding a measured quantity against a `0xFFFFFFFF` clamp is worth a law.** A
  stage sample longer than ~71 minutes of microseconds would wrap a `uint32_t`; the
  clamp and a saturation law together make that a tested path rather than an assumed
  one.

## Standing gaps

- **VOICES is one bucket over five engines.** 303A, 303B, 808, 909 and LEVI are
  timed together, so the actionable target is "the voice render" and not yet "which
  voice render". The obvious next split.
- The corrected `-O0` render (4590 µs) exceeds the 5333 µs period, so an xrun is
  structurally guaranteed on this guest for this patch — see
  [the optimisation-level result](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md).
- The instrumented numbers must never be quoted without the −301 µs control. The
  stage tables are a diagnostic; they are not free, and they are not the shipping
  measurement.

## See Also

- [The optimisation level alone removes every xrun on the Dell](2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md)
- [Scripted A,B,B,A: the governor arm is a real win, the repaint policy is a real regression](2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md)
- [Bounding the damage-box build](2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md)