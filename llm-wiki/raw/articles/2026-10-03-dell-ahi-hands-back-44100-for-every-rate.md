# The Dell hands back 44100 for every rate it is asked for, and `AHI_BestAudioID` cannot select 44100 at all (2026-10-03)

- Source: ReIncarnation session, 2026-10-03 (opencode lane, Dell E6320, ABIv11)
- Collected: 2026-10-03
- Published: 2026-10-03
- Raw: [Dell AHI rate probe, verbatim](../evidence/2026-10-03-dell-ahi-rate-probe.md)
- Related: [AC97 resamples 48 kHz to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md), [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md)

The question was narrow — *what rate does the Dell's card actually run at?* —
and the answer disprovES something I had written down hours earlier, in the
wrong direction: **the Dell is not native 48 kHz either.**

## The measurement

```
RI_RATE req=48000 mode=0x003E0001 got=44100 bits=16 stereo=1 hifi=1 maxch=128 range=44100-192000 nfreq=5
RI_RATE list[0]=44100
RI_RATE list[1]=48000
RI_RATE list[2]=88200
RI_RATE list[3]=96000
RI_RATE list[4]=192000
```

Two facts, and they pull in opposite directions.

**The card offers 44100** — it is `list[0]` and the floor of
`range=44100-192000`. So the fix for riqemu1's 8.1 % slow playback is not
ruled out on this machine.

**Nothing ever comes back as anything but 44100.** Every candidate, retried
against a known-good mode id:

| requested | AHI read back | |
|---|---|---|
| 48000 | **44100** | `CONVERTED_TO=44100` |
| 44100 | **44100** | exact |
| 32000 | **44100** | `CONVERTED_TO=44100` |
| 22050 | **44100** | `CONVERTED_TO=44100` |
| 11025 | **44100** | `CONVERTED_TO=44100` |
| 8000 | **44100** | `CONVERTED_TO=44100` |

Six requests, one answer. `AHIA_MixFreq` is a suggestion and this driver treats
it as decoration.

## The trap, and it is the reason this probe exists

```
RI_RATE req=44100 mode=INVALID (BestAudioID)
RI_RATE req=44100 retry_with_known_mode=0x003E0001
RI_RATE req=44100 mode=0x003E0001 got=44100 ...
```

**`AHI_BestAudioID` refuses 44100 on a card whose own frequency list is
`list[0] = 44100`.** It is the one rate on this machine that is both native and
unselectable. Every other candidate — 32000, 22050, 11025, 8000 — was also
refused, so the selector matches *only* 48000, which is the one rate that
arrives as a conversion.

So the obvious implementation of the 44100 fix — change the requested rate and
let the existing mode selection find it — **fails to open audio on the Dell.**
Not slowly, not degraded: `AHI_BestAudioID` returns `AHI_INVALID_ID` and there
is no audio at all. Reaching 44100 requires supplying a mode id directly
(`mode=0x003E0001`, obtained from the 48000 request) and letting `MixFreq` do
what it will.

This is a genuine AROS driver defect, not a usage error. `AHI_BestAudioID` is
supposed to select the best mode *satisfying* the request; here it neither
satisfies the request nor offers a native alternative that does.

## The correction: the log line is a request, not a fact

Earlier today I wrote, in the AC97 article:

> Every xruns figure in this lane came from the Dell, whose sound card is native
> 48 kHz and needs no resample.

**That is false, and the probe is what shows it.** The Dell's AHI accepts a 48000
request and hands back 44100. I inferred "native 48 kHz" from the log line

```
audio: AHI low-level mode=0x00390004 mix=48000 Hz buffer=256 frames period=5333 us
```

which reads as a measurement of what is happening and is actually a record of
what was *asked for*. Everything derived from that line is on the wrong clock.
That is a more useful lesson than the false claim it replaces: **a line in your
own log that repeats an argument you passed is not evidence about the device.**

The two lanes do genuinely differ, though, and now for a measured reason:

| lane | requested | AHI read back | what the host plays |
|---|---|---|---|
| Dell | 48000 | **44100** | 44100 — no conversion |
| riqemu1 | 48000 | **48000** | 44100 — QEMU resamples, 8.1 % slow |

So the corrected comparability rule is not "the two lanes run on different
clocks". It is: **both lanes end up at 44100, and the riqemu1 lane additionally
converts to get there** — which is exactly why riqemu1 is 8.1 % slow and the
Dell is not, on the same engine and the same requested rate.

## Why the 44100 fix is now safe to propose

Before this measurement I recorded the fix as an owner decision requiring a
native-rate probe first, because the Dell's card was a *different device* whose
rate should be probed rather than assumed. The probe has now been run:

- **44100 is native on the Dell** — `list[0]`, and the range floor.
- **44100 is reachable on the Dell** — allocating with the known mode id returns
  it exactly, no conversion.
- **44100 is what both lanes converge on anyway**, so asking for it removes the
  riqemu1 conversion without changing what the Dell does.

The one precondition the probe exposed is the `AHI_BestAudioID` defect: the
change has to supply the mode id, not just alter the requested frequency.

Not implemented. That is still the owner's call, and it now has a measured basis
rather than an assumption.

## The probe had a bug that produced a plausible wrong number

The first run reported:

```
RI_RATE req=48000 mode=0x003E0001 got=44100 ...
RI_RATE list[4]=192000
RI_RATE req=48000 CONVERTED_TO=192000
```

`got=` said 44100; `CONVERTED_TO=`, a few lines later, said 192000. The
frequency-list query writes through `AHIDB_Frequency` as an *output*, exactly as
the mode query does — so walking the list overwrote the mode's own frequency
with the final list entry, and the resampling tell then reported the driver's
highest supported rate as though the mode had returned it.

The fix was a second variable for the list query plus a snapshot of the readback
taken immediately. **A diagnostic that reports a wrong number is worse than one
that reports nothing**, because it survives review: `192000` is a real number
from this machine, and it would have been filed as a finding. It is recorded
here so the raw file's first block is not mistaken for the measurement.

## Two lane facts worth the trip

**`Run` is the wrong way to run a foreground probe.** AROS `Run` spawns and
returns, so the child's console output arrives after the agent has already read
the channel:

```
[exec] 'Run RAM:probe_rate' -> rc=0 (103 ms)     no output
[exec] 'wait'                -> rc=0 (1122 ms)    still no output
[exec] 'RAM:probe_rate'      -> rc=0 (74 ms)      output appears
```

`rc=0` from `Run` means "spawned", not "ran". A measurement probe that reports
through stdout has to be invoked in the foreground.

**The `exec`/ui split reproduced on demand.** On riqemu1, with RIAPP live:

```
[exec] 'status' -> rc=1 (4 ms)      (no output)
[ping] -> ok=True (3 ms)
[ui  ] windows 1280x1024 screen, 3 window(s) (3 ms)
       RIAPP live panel                         0,0 974x680 [active,close@5,0]
```

Exactly as recorded, and it is why no cross-lane probe run followed: a second AHI
client beside a live RIAPP risks a contention AHI cannot arbitrate. The
riqemu1 side of the comparison above comes from the earlier verified readback
(`alloc_audio: OK freq=48000`) rather than from a fresh run.

## Method

- **Ask the device, do not infer it from your own log.** The claim this record
  overturns came from a log line echoing a request.
- **Read the mode back, always, whatever was asked.** That one habit is the
  entire difference between "the card does 48 kHz" and `got=44100`.
- **A refusal is not a "no".** `BestAudioID` returning INVALID means the
  *selector* declined. Re-running the request against a known-good mode id is
  what separated "the selector cannot match this rate" from "the driver cannot
  play this rate" — a distinction that decided whether the fix was reachable.
- **Give each query its own storage.** Two AHI queries writing through the same
  output slot is a silent, plausible-looking corruption.

## See Also

- [AC97 resamples 48 kHz to 44.1 kHz](2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md) — the finding this extends, and the one carrying the claim this record corrects
- [AHI on riqemu1: the loader ignores `sh_addralign`](2026-10-03-ahi-on-riqemu1-the-loader-ignores-sh-addralign.md) — why AHI is usable on either lane at all