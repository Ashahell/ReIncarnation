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

**Pitch and tempo go together**, which is why it reads as a performance problem
rather than as a resampler: a synth played 8 % slow sounds like a synth being too
slow, not like a clock being wrong.

> **Status: the mechanism below is WRONG on its central claim, and the recommended
> fix is not achievable on this lane.** Both corrected 2026-10-03, the second
> after a host cold-reboot gave the chance to measure this lane directly.
>
> **1. QEMU's AC97 is NOT fixed at 44.1 kHz and there IS a rate knob.** From
> `hw/audio/ac97.c` (QEMU 11.1.1), `AC97_PCM_Front_DAC_Rate` is **guest-writable**
> whenever `EACS_VRA` is set, and the reset path stores `0xbb80` — which is
> **48000** — then calls `open_voice(s, PO_INDEX, 48000)`. The rate is used raw
> (`as.freq = freq`). The model defaults to 48 kHz and takes its rate from the
> guest.
>
> **2. The guest never asks for 44100 either.** AROS's ac97 AHI driver hardcodes
> 48000 in two places (`ac97-main.c:39`, `ac97-main.c:147`), and the probe
> confirms the mode: **`range=48000-48000 nfreq=1`, `list[0]=48000`**. Every rate
> requested comes back as 48000, 44100 included.
>
> **So "run AHI at 44100" cannot work on riqemu1** — there is no 44100 mode to
> select, and a 44100 request is silently converted. It remains a no-op on the
> Dell, which already returns 44100 for everything.
>
> **Where the 44100 does come from is narrowed but not settled.** QEMU defaults an
> unspecified `-audiodev` frequency to 44100 (`audio.c:253`), the launch passes
> none, and all host sinks are 48000 — so neither the guest, QEMU's device model
> nor the host explains it. Whether the device's `as.freq` overrides that template
> default could not be read from this tree. **The decisive experiment:** add
> `frequency=48000` to the `-audiodev`, play, read the host stream rate.
>
> The **measurement** in this article stands — the guest really does produce
> 48000 at exactly real-time pace and the host really does play 44100. What was
> wrong was saying QEMU was the thing converting. Full record:
> [riqemu1 rate and QEMU's AC97 source](../evidence/2026-10-03-riqemu1-rate-and-qemu-ac97-source.md).

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

## The fix — as originally written, and why it does not survive

**Run AHI at 44100 so AC97 never resamples.** That was the recommendation, on the
reasoning that 44100 is QEMU's fixed rate and matching it removes the conversion.

**It is not achievable on this lane.** `probe_rate` on riqemu1 returns
`nfreq=1`, `list[0]=48000`, `range=48000-48000`: there is exactly one mode, and a
44100 request comes back as 48000. No amount of changing what ReIncarnation asks
for will produce 44100 here.

It is equally pointless on the Dell, which already returns 44100 for every rate
— see [the Dell hands back 44100 for every rate](2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md).

> **TESTED 2026-10-03 AND REJECTED. There is no launch-line knob.**
>
> ```
> qemu-system-x86_64: -audiodev pa,id=pa0,...,frequency=48000:
>     Parameter 'frequency' is unexpected
> ```
>
> And it is not a `pa` peculiarity — **every** backend refuses it: `pa`, `alsa`,
> `pw`, `none`, `sdl`, `coreaudio`, `jack` all reject `frequency`. So
> `pdo->frequency`, which `audio/audio.c:253` defaults to 44100 when unspecified,
> **cannot be overridden from the command line at all.**
>
> **The guest is exonerated.** AROS's `ac97` AHI driver mentions a rate exactly
> twice, both 48000, and **never writes a codec rate register at all** — no
> `CODEC_FMT`, no `EXTENDED_AUDIO`, no `VRA`. Combined with the readback
> (`got=48000`, `nfreq=1`), the guest asks for 48000, reports 48000, and never
> contradicts itself. **Nothing in ReIncarnation chooses 44100.**
>
> **What remains unexplained, and the bound that is established:** the effective
> 44100 comes from neither the AC97 reset default (48000, `0xbb80`), nor the AROS
> driver (48000), nor any CLI option (none exists) — so it is in **QEMU's audio
> back end**, upstream of this project. The supporting trace is incomplete: the
> QEMU source tree on this host is intermittently unreadable (`ls -la` on
> `paaudio.c` succeeded and an immediate `sed` on the same path returned *No such
> file or directory*), so whether `ss.rate = as->freq` reads the device's settings
> or the audiodev template's is **not established**. Recording the bound rather
> than a mechanism. Full record:
> [the audiodev frequency fix is rejected by experiment](../evidence/2026-10-03-audiodev-frequency-rejected.md).

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

> **Status: the "why the Dell numbers are not comparable" section below was
> WRONG on its premise and has been corrected in place.** It claimed the Dell's
> card "is native 48 kHz and needs no resample". Measured on the Dell
> (2026-10-03): a 48000 request comes back from AHI as **44100**, and *every*
> rate comes back as 44100. The inference came from this article's own log line,
> `mix=48000`, which records the request rather than what the device did.
> Both lanes in fact converge on 44100 — riqemu1 by resampling, the Dell by
> AHI declining the request. Full record, and the corrected comparability rule:
> [the Dell hands back 44100 for every rate](2026-10-03-dell-ahi-hands-back-44100-for-every-rate.md).

## Why the Dell numbers are not comparable

**Corrected premise.** The Dell is *not* native 48 kHz — its AHI hands back
44100 for every rate requested, so the engine mixes at 44100 there too. What
actually differs between the lanes is one step in the chain, not the clock:

| lane | requested | AHI read back | what the host plays |
|---|---|---|---|
| Dell | 48000 | 44100 | 44100 — no conversion |
| riqemu1 | 48000 | 48000 | 44100 — QEMU resamples, 8.1 % slow |

The engine is therefore running at 44100 on **both** lanes, and the same
`period=5333 us` in the log above is not the period on either. Stage
*proportions* still transfer — the engine does the same work per buffer either
way — so `lev-voice / block` is comparable across lanes and `xruns` is not,
but not for the reason first written here.

The sharper rule, and the one worth keeping: **a log line that echoes an argument
you passed is not evidence about the device.** `mix=48000` is what the engine
asked for. Every figure derived from it is on the wrong clock, on both lanes,
and the only thing that settles it is reading `AHIDB_Frequency` back off the
allocated handle.

> **An unattributed 48 kHz stream was seen alongside the guest's 44.1 kHz one
> (2026-10-04) — recorded because it bears on the bound above, and explicitly not
> counted as evidence against it.** Two uncorked sink inputs at different rates:
>
> ```
> Sink Input #403  s16le      44100   application.name = "riqemu1"
> Sink Input #440  float32le  48000   (application.name NOT captured)
> ```
>
> This is the one host-side datum suggesting 48000 is reachable at all on this
> machine. **It is not the guest** — its format is `float32le` where every QEMU
> `pa` stream here has been `s16le`, other lanes were live at the time, and the
> stream did not survive to be re-inspected (a later listing showed **0** sink
> inputs). **It says nothing about what the guest's AC97 path can be made to do.**
> Full record, and what would settle it:
> [a 48 kHz stream coexisted with the guest's 44.1 kHz one](../evidence/2026-10-04-a-48khz-stream-coexisted-with-the-guests-44k1.md).

## See Also

- [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md) — what made AHI usable there at all
- [the riqemu1 lane cannot be driven by injection](2026-10-03-riqemu1-cannot-be-driven-by-injection.md) — why the transport had to be started by hand before any of this could be measured