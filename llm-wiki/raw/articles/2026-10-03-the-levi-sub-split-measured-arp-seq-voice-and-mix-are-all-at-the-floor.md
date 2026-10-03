# The LEVI sub-split is measured, and all four of its internals sit at the timer floor (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, riqemu1, ABIv1)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [LEVI internal split on riqemu1, verbatim](../evidence/2026-10-03-levi-subsplit-riqemu1.md)
- Related: [five sections, one culprit: LEVI is 32 percent](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md), [AC97 resamples 48 kHz to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md), [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md)

This closes a standing gap that had been open since 2026-10-02 and explicitly
named: *"LEVI's internal split: `arp` vs `seq` vs `voice_render_sum_stereo` vs the
stereo section mix. Same pattern, same gates, one more table."*

The instrumentation was already built and mutation-proven. What was missing was a
lane, and riqemu1 became usable once AHI worked.

**The answer is not the one the gap expected.** All four sub-stages are
indistinguishable from a no-op, and more than half of LEVI's cost is not inside
any of them.

## The table

`RIAPP dstg n=24042`, Zombie Nation, 151 bars at 140 BPM, `xruns=0` over 26474
buffers:

| stage | avg µs | max µs | % of block |
|---|---|---|---|
| zero / delay / comp / master / meter / limit | 6 each | 49–98 | 1.7 % each |
| 303a | 70 | 980 | 19.9 % |
| 303b | 68 | 153 | 19.4 % |
| 808 | 37 | 135 | 10.5 % |
| 909 | 7 | 55 | 2.0 % |
| **levi** | **57** | **786** | **16.2 %** |
| lev-arp | 6 | 38 | 1.7 % |
| lev-seq | 6 | 54 | 1.7 % |
| lev-voice | 8 | 69 | 2.3 % |
| lev-mix | 6 | 76 | 1.7 % |
| block | 351 | 3048 | 100 % |

```
five sections sum : 239 us = 68% of block
outside sections  : 112 us = 32% of block
LEVI internals sum:  26 us of levi's 57 us
LEVI unattributed :  31 us = 54% of levi
```

## The finding: 6 µs is the floor, and everything below it is invisible

`zero` is a no-op — it clears `ml/mr/sendbus` — and it reads **`avg=6 us`**. Six of
the sixteen stages read exactly 6:

```
stages sitting at the floor: zero, delay, comp, master, meter, limit,
                             lev-arp, lev-seq, lev-mix
```

**So 6 µs per block is the instrumentation's resolution, not a cost.** Measured as
excess over that floor, LEVI's four instrumented internals are:

```
arp +0    seq +0    voice +2    mix +0        total +2 us
```

**All four together account for about 2 µs of LEVI's 57.** The gap asked where
LEVI's time goes; the honest answer is that the four regions it named contain
almost none of it. Either the bulk of `levi_block` sits outside those regions, or
they are the wrong regions — and this table cannot distinguish those two, only
that it is not inside them.

That is a real result *and* a limit on the instrument, and both belong in the
record: **a sub-stage table cannot resolve anything under the floor, so "cheap"
and "never ran" look identical here** — exactly the distinction the 2026-09-02
record made a point of for *section* stages reporting `n=0`. There is no `n` on
these rows to fall back on, which is the gap that remains.

## LEVI is not the outlier in this configuration

| | 2026-09-02 (Dell) | 2026-10-03 (riqemu1) |
|---|---|---|
| LEVI share of block | ~30 % | **16.2 %** |
| rank among sections | 1st, and the outlier | **3rd**, behind 303a (19.9 %) and 303b (19.4 %) |

**This does not refute the earlier finding, and the reason matters.** riqemu1's
clock is resampled by `44100/48000 = 0.91875`, so the lane can shift absolute
figures by a factor of about **1.09** — nowhere near enough to explain a
**2×** difference. So the two runs genuinely differ in configuration: different
song, different section enablement, or different optimisation level. Which one
needs a like-for-like repeat. **The resample factor is small enough to be
checked before attributing a large discrepancy to it** — and that check is what
keeps this from being either a false refutation or a false confirmation.

## Also worth naming

```
RIAPP dstg 303a   avg=70 us max=980 us
RIAPP dstg levi   avg=57 us max=786 us
RIAPP dstg block  avg=351 us max=3048 us
RIAPP hb: ... render_max=5034 us ... period=5333 us ...
```

`render_max` 5034 µs against a 5333 µs period: a single block consumed almost
the whole period. Against `avg` 351 that is one block, not a sustained condition
— the same "report max next to the mean" rule the 2026-09-02 method findings ask
for, and the reason this reads as an outlier rather than as a stall.

## Playback was proven without capturing a sample

`parec` returned nothing. That proved nothing — this host is PipeWire, so
`@DEFAULT_MONITOR@` no longer means what it did under PulseAudio. The evidence
that actually settled it was host-side state:

```
Sink Input #741   Corked: no   application.name = "riqemu1"
57  alsa_output...analog-stereo  PipeWire  s32le 2ch 48000Hz  RUNNING
```

An uncorked stream plus the sink moving SUSPENDED → RUNNING. **The recorded
"peak 0 proves nothing" lesson arriving from the opposite direction**: a capture
that yields *nothing at all* is equally uninformative, and the fix is to find an
instrument that reports state rather than samples.

Transport was driven with `sendkey spc` over the QEMU monitor, **not** a click —
which is why it works on a lane where injected clicks arrive nowhere. And the
sequencing rule held exactly as recorded: launch, measure, `--ui-close`, then
read. `exec` returned `rc=1` with no output for the entire time RIAPP ran, and
`rc=0` the moment it exited.

## Standing gaps

- **What the other 54 % of LEVI is.** The four instrumented regions do not contain
  it. Either `levi_block` spends it outside them, or they are the wrong regions.
- **Sub-6 µs resolution.** No `n` on the sub-stage rows, so "cheap" and "never
  ran" are indistinguishable — the trap the 2026-09-02 record named for sections.
- **Like-for-like against the Dell**, to say whether the 16.2 % / ~30 % gap is
  song, section enablement, or optimisation level.

## Method

- **Use a floor-reading stage as the instrument's resolution.** `zero` costs
  nothing and reads 6 µs, which converts every other 6 into "unmeasurable" and
  every 8 into "about 2". Without that anchor the table looks like sixteen
  costs.
- **Check the resample factor before calling a discrepancy a refutation.** 1.09
  cannot explain 2×; saying so is what keeps this honest in both directions.
- **State a limit as a finding.** "54 % unattributed" is more useful than four
  small numbers that suggest the question was answered.

## See Also

- [five sections, one culprit: LEVI is 32 percent](2026-10-02-five-sections-one-culprit-levi-is-32-percent.md) — the standing gap this closes, and the earlier headline
- [AC97 resamples 48 kHz to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md) — the resample factor used above as a bound
- [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md) — why `sendkey` rather than a click, and the exec/ui split