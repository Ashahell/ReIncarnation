# The LFO path never runs: the cost is the voice render itself (2026-10-04)

- Source: ReIncarnation session, 2026-10-04 (opencode lane, Dell E6320, ABIv11, `-O0`)
- Collected: 2026-10-04
- Published: 2026-10-04
- Raw: [verbatim](../evidence/2026-10-04-lfo-path-never-runs.md)
- Related: [the guest crashed in utility.library](2026-10-04-the-guest-crashed-in-utility-library.md), [the sub-split charges LEVI 6 us per stage it contains](2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md), [the Dell lane's two silent traps](2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md)

The last open question inside the hot spot had one candidate: the LFO block runs
`RI_LEVI_NLFO`(5) x `RI_LEVI_NVOICES`(8) = **40 inner iterations per sample**,
against only 8 voice calls. That is a prima facie case for the cost.

**It never runs.**

## The answer, in one line

```
  lfo_samples              : 0
  lfo_iters                : 0   <-- the LFO path never executes
  fx_samples               : 0   (levi_fx_mod never runs)
  LFO iterations : voice calls = 0.000 : 1
```

Summed over every logged block, and zero in the busiest one too. **Not one LFO is
trigged anywhere in this song's Levi part**, so the 40-iterations-per-sample
suspicion is not merely smaller than expected — it is absent. `levi_fx_mod` is
likewise never reached.

The hypothesis was good and it was wrong, which is why it needed a counter rather
than more staring at the source.

## So the cost is the voices

```
  lev-arp         4 us    1 % of levi,  0 % above its own floor
  lev-seq         4 us    1 % of levi,  0 % above its own floor
  lev-voice     543 us   91 % of levi, 94 % above its own floor
  lev-mix         6 us    1 % of levi,  0 % above its own floor
  lev-tempo       4 us    1 % of levi,  0 % above its own floor
```

Every stage except `lev-voice` sits on the lane's own 4 µs floor. **91 % of LEVI
is `levi_voice_render_sum_stereo`, and 94 % of that is real work above the
floor.** The four stages that are not the voice render are, in effect,
unmeasurable on this lane.

**Peak polyphony is 4 voices of 8 slots** (256 active of 512 calls in the busiest
block), and the idle four early-out exactly as `levi_voice_render_stereo`
intends — so the unused slots are close to free, and the cost tracks *sounding*
voices rather than the polyphony setting. That is the useful shape of the answer:
**the target is per-voice DSP, and it scales with how many voices are actually
sounding.**

## Both lanes agree once the tax is removed

| | riqemu1 (Zombie Nation) | Dell (The Knife) |
|---|---|---|
| block | 538 µs | 1208 µs |
| LEVI reported | 263 µs | 597 µs |
| LEVI corrected | 227 µs | **573 µs** |
| **LEVI share, corrected** | **42.2 %** | **47.4 %** |
| `lev-voice` share of LEVI | 81 % | **91 %** |
| control / pair cost | 6 µs | 4 µs |
| residual inside LEVI | 8 µs | 32 µs |

Two different songs, two different clock domains, one resampled and one not — and
**42.2 % against 47.4 %**. With the child-count tax subtracted on both sides, the
lanes are describing the same machine. `levi_voice_render_sum_stereo` is the cost
on both, at 81 % and 91 % of LEVI.

## Three things the run also settled

**An explicit `SONG=` appeared not to win. It did — see the correction below,
which matters more than the point it was making.**

> **Correction 2026-10-04: `RIAPP.LOG` APPENDS ACROSS RUNS.** The The Knife line
> read here was from an earlier session; this run's own line is further down the
> file and reads `RIAPP song RAM:zombie-nation.rbng: 151 bars at 140 BPM`. The
> override worked, and the code's precedence comment is correct. The mistake was
> reading the FIRST matching line of an appending log instead of the last run's.
> Record: [songs could not open](2026-10-04-songs-could-not-open.md).
>
> The original claim is left in place below because the reasoning error, not the
> finding, is what is worth remembering.

**An explicit `SONG=` did not win.** The run was launched
`Run RAM:RIPP-VCOUNT SONG=RAM:zombie-nation.rbng`, and what loaded was:

```
RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars
```

The code says *an explicit `SONG=`/`PLAYLIST=` always wins, because an argument is
a decision and this is a default*. It did not. The likely cause is the guest
Shell mangling an argument containing both `=` and `:`, which is **not
established** — but it is a finding about argument passing, and it means the
"always wins" comment should not be trusted until someone reproduces it.

**The Dell has no song library where riqemu1 does**, and the guest Shell has no
`mkdir`:

```
[put ] ... -> SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng
[submit] error: ProtocolErr: put_begin failed: cannot open temp file
```

`put` cannot create the directory, and the Shell accepts only `Run`, `SetEnv`,
`echo`, `dir`, `status`, `wait`, `Break`, `Getenv`. The song went to `RAM:`
instead. On this machine the demo path resolves through `Vk4aros:`, which is why
The Knife was found at all.

**`RI_V11_OPT=-O0` is not optional.** A rebuild that omitted it came out
`-O2` — 880608 B, `r12moves=286` — where the baseline is 1100264 B and 41. The
lane record's reason holds exactly: *an `-O2` deploy changes CPU cost by ~24 % by
size alone and would confound the metrics being measured*. This run was measuring
CPU cost.

## Two lane corrections

**`exec` does not die while RIAPP runs on the Dell.** With `RAM:RIPP-VCOUNT` live
as Process 8:

```
[exec] 'status' -> rc=0 (48 ms)
```

The "RIAPP running kills the guest exec channel" defect is **riqemu1-specific**,
not a general lane law. It had been recorded generally; it is narrower than that.

**A `Software Failure!` requester clears on a VM restart, with nobody at the
keyboard.** riqemu1 was powerdown'd and relaunched and came back clean:

```
  display: P6 1280 1024 255
[exec] 'status' -> rc=0 (10 ms)
```

This refines the previous record's "needs a human click, or a guest reboot": a
**VM** restart is enough. The other half stands unchanged — `sendkey tab` then
`ret` still does not dismiss one.

## Open

- **Why the song did not load on riqemu1** with a present file and neither
  demo-song line logged, and **why `SONG=` lost to the demo path here.** Two
  lanes, two silent song-selection anomalies, one shared code path.
- **The residual 32 µs inside LEVI** (5.4 % here, 8 µs on riqemu1). Small and
  stable, still unattributed.
- **`levi_voice_render_sum_stereo` itself is unsplit.** 91 % of LEVI is one
  function, and its per-region split is blocked by the same wall as before — a
  clock read costs 4 µs and the function averages ~8 µs per sample here. Work
  counters, again, or a much cheaper clock.
- **Songs with LFOs actually triggered.** `lfo_iters = 0` means this song cannot
  say anything about the LFO path's cost, only that it is idle here.

## Method

- **A counter answers "does this run at all" where a timer cannot answer
  "how much".** The LFO question was not "how expensive" but "is it reached",
  and that needed no clock at all.
- **Check the floor before reading a small number as small.** Four of five LEVI
  stages read exactly the lane's control value, which is the difference between
  "cheap" and "unmeasurable".
- **Re-derive the optimisation level from the build output.** `-O2` looked like a
  successful build and would have confounded the entire measurement.

## See Also

- [the guest crashed in utility.library](2026-10-04-the-guest-crashed-in-utility-library.md) — why the counters were counters, and the AHI contention that made a run lie
- [the sub-split charges LEVI 6 µs per stage it contains](2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md) — the child-count tax subtracted on both sides of the table above
