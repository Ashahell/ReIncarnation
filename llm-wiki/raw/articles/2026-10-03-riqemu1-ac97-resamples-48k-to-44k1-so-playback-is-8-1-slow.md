# The AC97 resamples 48 kHz to 44.1 kHz, so riqemu1 playback is 8.1 % slow (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, riqemu1)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [riqemu1 lane measurements](../evidence/2026-10-03-riqemu1-lane-measurements.md)
- Related: [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md)

## The finding

The owner's ear reported playback as *"very, very slow"*. It is, and it is a
**rate conversion in QEMU, not a performance problem in the engine.**

```
guest:  audio: AHI low-level mode=0x00390004 mix=48000 Hz buffer=256 frames period=5333 us
host:   format.rate = "44100"   (Sink Input #2337, application.name = "riqemu1")
        ──────────────────────────────────────────────────────────────────────
ratio:  44100 / 48000 = 0.91875          → 8.1 % slow
```

QEMU's AC97 model is fixed at 44.1 kHz and resamples the guest's 48 kHz stream
down to it. **Pitch and tempo go together**, which is why it reads as a
performance problem rather than as a resampler: a synth played 8 % slow sounds
like a synth being too slow, not like a clock being wrong.

`start_riqemu1.sh` passes `-device AC97,audiodev=pa0`, and there is no AC97 rate
knob on the QEMU side to change.

## The guest is not slow, and its own counters say so

This is the part worth keeping, because "slow playback" invites a CPU-bound
explanation and every number here refuses it:

```
RIAPP hb: buffers=35876 xruns=0 render_max=3113 us load=3/1000
RIAPP dstg block    avg=462 us  max=976 us
RIAPP dstg levi     avg=172 us  max=647 us
RIAPP dstg lev-voice avg=124 us  max=573 us
```

- **`xruns=0`** — the deadline was never missed, not once, across 35876 buffers.
- **`render_max` 3113 µs against a 5333 µs period** is 58 % of the budget.
- **`load=3/1000`** — the render task is using 0.3 % of a period.
- **Pacing ratio 1.00.** 3808 buffers × 5333 µs = 20.3 s of wall clock, carrying
  3808 × 256 / 48000 = 20.3 s of audio. The guest is producing exactly as much
  audio as real time requires.

A CPU-bound renderer would show `xruns` climbing and `render_max` at or past the
period. Neither happens. **The engine is keeping up and still everything sounds
slow, because the samples are being stretched on the way out.**

The Levi sub-split, incidentally, came out of the same run and is clean —
`dstg n=3453`, `lev-voice avg=124 us`, `block avg=462 us` — the only
device-measured sub-split numbers that exist.

## The fix

**Run AHI at 44100 so AC97 never resamples.** That removes the conversion rather
than compensating for it, and it costs nothing on this lane: the engine is
running at 3/1000 load, so 44100 is as cheap as 48000.

Not implemented. It is a one-line change in `au_live_open`'s rate request and it
should be an owner decision because it changes the rate the Dell lane negotiates
too, and the Dell's AC97 is a *different* device whose native rate should be
probed rather than assumed.

The alternative — leave the engine at 48000 and accept 0.91875 — is only
defensible if a future guest can be given a 44.1 kHz sound card, which QEMU's
AC97 cannot.

## A capture that proves nothing, and why that matters

Recorded because "silent" and "not playing" look identical from the host:

```
samples 2719744  peak 0  rms 0.0  non-zero 0.00%
```

That was captured while the transport was **stopped** — `stg_playing=0`, no
`RIAPP play` line. It is not evidence about audio at all, and treating a zero-peak
capture as "audio is broken" would have been a false diagnosis of the same family
as the stale `fallback: RAM:` line.

A **later** capture, after the owner pressed Play by hand and audible output was
confirmed, is the one that would carry weight; it is not in the raw file yet.

## Why the Dell numbers are not comparable

Every xruns figure in this lane came from the Dell, whose sound card is native
48 kHz and needs no resample. **A number measured on riqemu1 is on a resampled
clock and must not be compared against a Dell figure for absolute timing.** Stage
*proportions* still transfer — the engine does the same work per buffer either
way — so `lev-voice / block` is comparable and `xruns` is not.

That distinction was not written down anywhere before this, and it matters for
the Levi sub-split measurement that is still blocked on lane availability.

## See Also

- [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md) — what made AHI usable there at all
- [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md) — why the transport had to be started by hand before any of this could be measured