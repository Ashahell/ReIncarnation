# LEVI internal split measured on riqemu1, 2026-10-03 — verbatim

**Ingested:** 2026-10-03 into ReIncarnation `llm-wiki`
**Source:** `T:RIAPP.LOG` pulled from riqemu1 over the agent channel after a
playback run, built from the repo's `riapp` target (`scripts/ri_build_aros.sh
riapp`, ABIv1).
**Provenance:** verbatim log lines. Derived values show their components.
**Recorded in:** [the LEVI sub-split is measured, and all four internals sit at the timer floor](../articles/2026-10-03-the-levi-sub-split-measured-arp-seq-voice-and-mix-are-all-at-the-floor.md)

## 1. Lane and binary

```
$ ./scripts/ri_build_aros.sh riapp
AROS RIAPP BUILD OK (1105560 bytes)

  r12 base moves: 0  (v1 => 0)
  unresolved:     0
  OS/ABI:                            AROS
  Machine:                           Advanced Micro Devices X86-64

[bulk] /tmp/ri/aros/RIAPP -> RAM:RIAPP  1105560 B, 15345 ms  sha_ok=True written=1105560 OK
```

`r12moves == 0` is the v1-lane gate; riqemu1 is the v1 lane, and a v11 binary
would fault before its first log line.

## 2. Host-side proof that playback actually ran

`parec` returned nothing, which proved nothing (this host is PipeWire, so
`@DEFAULT_MONITOR@` does not mean what it did under PulseAudio):

```
  no samples captured
```

The authoritative host indicators instead:

```
Sink Input #741
	Format: pcm, format.sample_format = "\"s16le\""  format.rate = "44100"  format.channels = "2"
	Corked: no
		application.name = "riqemu1"

57	alsa_output.pci-0000_11_00.6.analog-stereo	PipeWire	s32le 2ch 48000Hz	RUNNING
```

`Corked: no` plus the sink moving SUSPENDED -> RUNNING. **Playback confirmed
without a single sample captured**, which is the recorded peak-0 lesson arriving
from the opposite direction.

## 3. What was run

```
RIAPP song SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng: 151 bars at 140 BPM
RIAPP play
```

Transport started with `sendkey spc` over the QEMU monitor on 127.0.0.1:4477 —
not a mouse click, which is why it works on this lane at all.

## 4. The measurement — settled table, verbatim

```
RIAPP dstg n=24042RIAPP dstg zero   avg=6 us max=49 usRIAPP dstg delay  avg=6 us max=56 usRIAPP dstg comp   avg=6 us max=53 usRIAPP dstg master avg=6 us max=98 usRIAPP dstg meter  avg=6 us max=94 usRIAPP dstg limit  avg=6 us max=52 usRIAPP dstg 303a   avg=70 us max=980 usRIAPP dstg 303b   avg=68 us max=153 usRIAPP dstg 808    avg=37 us max=135 usRIAPP dstg 909    avg=7 us max=55 usRIAPP dstg levi   avg=57 us max=786 usRIAPP dstg lev-arp avg=6 us max=38 usRIAPP dstg lev-seq avg=6 us max=54 usRIAPP dstg lev-voice avg=8 us max=69 usRIAPP dstg lev-mix avg=6 us max=76 usRIAPP dstg block  avg=351 us max=3048 us
```

Matching heartbeat:

```
RIAPP hb: buffers=25323 xruns=0 render_max=5034 us wake_max=89 us wake_n=25321 prio=21 arm_us=0 load=3/1000 overloads=0 snd=1/1/1/3 pend=1/1/1/3
```

Session close:

```
RIAPP closed: buffers=26474 xruns=0 render_max=5034 us render_total=10801 ms period=5333 us wake_max=89 us wake_total=584 ms wake_n=26472 stg_total_avg=1543 us stg_dsp_avg=1464 us stg_evt_avg=12 us stg_playing=668
```

`xruns=0` over 26474 buffers.

## 5. The table, with shares derived

`% block` is `stage avg / block avg` = e.g. `70/351`.

| stage | avg us | max us | % of block |
|---|---|---|---|
| zero | 6 | 49 | 1.7 % |
| delay | 6 | 56 | 1.7 % |
| comp | 6 | 53 | 1.7 % |
| master | 6 | 98 | 1.7 % |
| meter | 6 | 94 | 1.7 % |
| limit | 6 | 52 | 1.7 % |
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

Derived:

```
five sections sum : 239 us  = 68% of block      (70+68+37+7+57=239; 239/351)
outside sections  : 112 us  = 32% of block      (351-239=112)
always stages sum :  36 us                      (6*6=36)
LEVI internals sum:  26 us  of levi's 57 us     (6+6+8+6=26)
LEVI unattributed :  31 us  = 54% of levi       (57-26=31)
```

## 6. The timer floor, and why it is the whole story

`zero` is a no-op stage — it clears `ml/mr/sendbus` — and it reads
**`avg=6 us`**. Six of the sixteen stages read exactly 6:

```
  stages sitting at the floor: zero, delay, comp, master, meter, limit, lev-arp, lev-seq, lev-mix
```

So **6 µs per block is this instrumentation's resolution floor**, and the four
LEVI sub-stages are all *at* it. Measured as excess over the floor:

```
  LEVI sub-stages, excess over floor: arp +0  seq +0  voice +2  mix +0  (total +2 us)
```

**All four of LEVI's instrumented internals together account for about 2 µs of
LEVI's 57 µs.** 54 % of LEVI is unattributed by the four regions the standing gap
asked about.

## 7. Prior finding, and why it is not refuted by this

The 2026-09-02 record's headline is "LEVI, and only LEVI, is out of line", with
`levi avg=398 us` at `-O0` and `levi avg=165 us` against `block avg=556 us` at
`-O2` — about 30 % of the block.

Here LEVI is **16.2 %**, third, behind both 303 sections (19.9 %, 19.4 %).

**The resample cannot explain that.** riqemu1's clock is
`44100/48000 = 0.91875`, so a factor of about 1.09 at most; the difference here
is roughly 2x. So the two runs differ in configuration — different song, section
enablement, or optimisation level — and identifying which needs a like-for-like
repeat, not an inference from these two tables.

## 8. Outliers worth naming

```
RIAPP dstg 303a   avg=70 us max=980 us
RIAPP dstg levi   avg=57 us max=786 us
RIAPP dstg block  avg=351 us max=3048 us
RIAPP hb: ... render_max=5034 us ...
```

`render_max` 5034 µs against a 5333 µs period, and `block max=3048`. A single
block at 3048 µs is the whole period nearly consumed — the 5 ms outlier is one
block, not a sustained condition, since `avg` is 351.

## 9. How the run was driven, for reproduction

```
sendkey spc   -> PLAY, over the QEMU monitor on 127.0.0.1:4477
[ui  ] windows ... RIAPP live panel  0,0 974x680 [active,close@5,0]
[exec] ... -> rc=1 (no output)      <- exec dies while RIAPP runs
[ui  ] close '#0' [closerequest] ok
[exec] 'status' -> rc=0             <- exec restored the moment RIAPP exits
[bulkget] T:RIAPP.LOG -> ... 23329/23329 B, 5 ms  OK (sha verified)
```

Launch, measure, **close, then read** — the sequencing rule, and it held again.