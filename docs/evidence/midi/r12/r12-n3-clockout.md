# R12 N3 — clock out on the wire: 56.000 clocks/s, mean 17857.3 µs (2026-10-19)

## Labels

**N3** (clock-out measurement) on the Dell e6320, over the real LAN, through
`rtpmidid` to an ALSA subscriber. The N1j gate is a separate run on the same
build and is recorded separately. Nothing here is inferred from the peerless
run on riqemu1.

## Setup

- Dell e6320, ABIv11, repo build **`5194334`**
  (`SYS:Classes/ReIncarnation/RIAPP`, 1205072 B, `RIAPP LOG build=5194334`).
- `RIAPP_MIDI_CLKOUT=1`, `RIAPP_MIDI_IN=applemidi.out.0`, `devout=0`.
  The input cluster is deliberately the **output** cluster: the AROS sender
  sends to the cluster the input open selected (`ri_pal_midi_send_start`
  ignores its `port` argument), so this is the honest configuration for a
  clock-only test. The self-loop is visible in the evlog (the app hears its
  own F8s) and changes nothing about the outgoing timing.
- Song `zombie-nation.rbng`, 151 bars at **140 BPM**, autoplay at load.
  24 ppqn gives a nominal **56.000 clocks/s** and a nominal interval of
  `60e6 / 140 / 24` = **17857.14 µs**.
- Host `192.168.1.81`: `rtpmidid 24.12.2` with
  `--rtpmidi-discover false`; session `CONNECTED`; the Dell appears as
  `alsapeer arosbox` on ALSA port **128:2**.
- `clocksniff 30 128:2` (host): subscribes to 128:2, prints one
  `CLOCK_MONOTONIC` nanosecond stamp per `F8`, 30 s window.
  `/home/miller/.worktrees/r12-tools/clocksniff.c`.

## Result

```
clocks=1681  span=30.000 s  rate=56.000/s
mean=17857.3 us  median=16064.5  sd=3834.9
min=0.2  max=53274.5  p1=7017.3  p5=13465.1  p95=21856.2  p99=26667.4
nominal=17857.1   mean-nominal=+0.1 us   worst-nominal=+35417.3 us
```

- **Rate is exact**: 1681 clocks in 30.000 s = 55.9997/s against 56.0000/s.
- **Mean interval is exact**: 17857.3 µs against 17857.1 µs (+0.1 µs,
  0.0006%), and against the assessment's 17853 µs estimate.
- Guest-side counters agree: `RIAPP clk: on=1 run=1 pend=0 dropped=0
  sent=853 err=0` then `sent=1710` on the next heartbeat (~15 s later,
  ≈56/s). The ring never backs up and nothing is dropped.

Distribution of the 1680 intervals:

```
     0-  100 us:    6 ( 0.4%)
   100- 8000 us:   15 ( 0.9%)
  8000-12000 us:   51 ( 3.0%)
 12000-16000 us:  700 (41.7%)
 16000-19500 us:  304 (18.1%)
 19500-22000 us:  536 (31.9%)
 22000-30000 us:   58 ( 3.5%)
 30000-70000 us:   10 ( 0.6%)
```

## Why the spread is what it is

**The producer fills the ring once per audio block, not per clock.** The
session runs 256-frame blocks at 48 kHz (5.33 ms). `midi_out_render()` emits
every clock that became due since the last call, so an F8 leaves at the
**first block boundary after its sample-accurate due time**. At 140 BPM the
due times fall 3.35 blocks apart, so the instantaneous interval between
clocks alternates between 3 and 4 blocks — 16.0 ms and 21.3 ms — with the
occasional 2-block 10.7 ms catch-up. That is the 12–22 ms mass above.

The rate is still exact because the producer is **cumulative**
(`o->last_tick < owed`), which is deliberate: a skipped render call emits
what it owes instead of silently dropping clock bytes. Over 30 s that law
holds to 0.1 µs.

The long tail (0.6% over 30 ms, worst 53.3 ms) is the same quantisation plus
host-side scheduling; it is not a rate error — the mean over the window is
0.1 µs from nominal.

## Measurement caveat

`clocksniff` timestamps events when it **drains** the ALSA sequencer, not
when the packet arrived. Events delivered in one poll wake-up therefore
collapse to ~0 µs (the 0.4% bucket) even though the wire spacing was not
zero. The measured spread is an **upper bound** on the guest's wire jitter;
the rate and mean are unaffected.

## The two defects this measurement found (fixed before the numbers above)

The first clock attempt produced **zero** F8 for 20 s while the app reported
`clkout=1` and "playing". Two independent defects, each masking the other:

### 1. The app killed its own sender at startup

`midi_setup()` started the clock-out sender at the top, and a few lines
later called `ri_pal_midi_open_in()`, whose first act is
`ri_pal_midi_close()` → `ri_pal_midi_send_stop()`. The sender was joined and
stopped immediately; `ri_pal_midi_send_start()` had returned 0, so there was
no "clock out refused" line. On the unmodified HEAD binary (before either
fix), TaskList shows the receiver and the render task but **no
`RIAPP midi-out`**:

```
process ... RIAPP render
process ... RIAPP midi
(no RIAPP midi-out)
```

Fix: start the sender **after** the input open. The same order is required
for correctness anyway — the AROS sender sends to the cluster the input
open selected.

### 2. The sender pumped once and blocked forever

With the sender now present, the heartbeat told the rest of the story:

```
RIAPP clk: on=1 run=1 pend=255 dropped=613 sent=0 err=0
```

`pend=255` is the ring full (cap 256), `dropped` climbing, `sent=0`,
`err=0`. `sender_task` did:

```c
SendIO(timer);        /* 1 ms */
WaitPort(port);       /* waits out the tick */
sigs = Wait(1UL << skill_sig);   /* waits for STOP -- forever */
```

The second wait blocks until teardown signals stop, so the loop body ran
**once** (the pump at the top, with an empty ring at startup) and then hung.
The task was alive and "waiting"; nothing was sent and nothing errored.

Fix: one `Wait()` on the timer port bit **and** the stop bit, reap the reply
only when the port fired, and let `CheckIO/AbortIO/WaitIO` cancel an
outstanding tick when stop wins the race. The sender now pumps every ~1 ms.

Both fixes are in `5194334`; the measurement above is from that build.

## Artifacts

`/home/miller/.worktrees/r12-logs/r12-n3/`:

- `clocksniff-30s.log` — 1681 timestamp lines + header
- `stats.txt` — the exact summary and histogram above
- `riapp-log-excerpt.txt` — `build=5194334`, `midi in=...`, the two clk
  heartbeat lines
- `pre-fix-ring-full-sent0.txt` — the `pend=255 ... sent=0` signature
- `tasklist-unmodified-head.txt` — receiver present, sender absent
- `tasklist-after-fix.txt` — both `RIAPP midi` and `RIAPP midi-out`
- `timerres.txt` — AROS `UNIT_MICROHZ` 1 ms wait actually takes
  min 1019 / mean 1977 / max 7928 µs over 200 waits (the pump is not the
  30 ms tail's cause)

Tools in the repo: `tools/clocksniff.c` (host, `-lasound`) and
`tools/timerres.c` (ABIv11 probe, build line in its header).

## Not proven

- A DAW or hardware clock follower; the far peer is `rtpmidid 24.12.2`
  forwarding to ALSA, and `clocksniff` is the observer.
- Clock **in** (the Dell following an external master) — a different lane.
- Jitter below the 5.33 ms block quantisation: the producer's ring is filled
  per audio block by design, so a sample-accurate wire clock would need a
  producer called at finer granularity. The **rate** is already exact; the
  **instantaneous spacing** is quantised.
- Whether the 0.6% >30 ms tail is guest-side or host-side: `clocksniff`
  timestamps at drain time, so it cannot separate them.
