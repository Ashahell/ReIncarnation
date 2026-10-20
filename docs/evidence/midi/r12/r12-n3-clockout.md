# R12 N3 — clock out on the wire: two 5-minute runs, mean 17857.1 µs (2026-10-19)

## Labels

**N3** (clock-out measurement) on the Dell e6320, over the real LAN, through
`rtpmidid` to an ALSA subscriber. Two independent runs, each one continuous
play of **303.4 s** (5.06 min). The N1j gate is a separate run on the same
build and is recorded separately. Nothing here is inferred from the peerless
run on riqemu1.

This supersedes the 30 s probe recorded earlier in this file; that probe
(1681 clocks in 30.000 s, mean 17857.3 µs) is consistent with the two runs
below but is not a run of record.

## Setup

- Dell e6320, ABIv11, repo build **`5194334`**
  (`SYS:Classes/ReIncarnation/RIAPP`, 1205072 B, `RIAPP LOG build=5194334`).
- `RIAPP_MIDI_CLKOUT=1`, `RIAPP_MIDI_IN=applemidi.out.0`, `devout=0`.
  The input cluster is deliberately the **output** cluster: in the `5194334`
  build the AROS sender still sent to the cluster the input open selected
  (`ri_pal_midi_send_start` did not use its `port` argument yet), so this was
  the honest configuration for a clock-only test. **Fixed later in R12 O0
  (`95facca`):** the sender now uses the `port` it is given, so this
  workaround is no longer needed. The self-loop is visible in the evlog (the
  app hears its own F8s) and changes nothing about the outgoing timing.
- Host `192.168.1.81`: `rtpmidid 24.12.2` with
  `--rtpmidi-discover false`; session `CONNECTED` (the peer exists as
  `alsapeer arosbox` on ALSA port **128:2** for the whole run; the sniffer's
  subscription to it succeeded).
- The Dell's `netmidid` is the same **local debug build** as N1j (128496 B,
  sha256 `dad3644b…`; markers and bind logging plus the build-only entropy
  shim, no protocol change). The markers go to `NIL:` unless `netmidid.log`
  names a file; they do not alter forwarding.
- `clocksniff 335 128:2` (host): subscribes to 128:2, prints one
  `CLOCK_MONOTONIC` nanosecond stamp per `F8` (`tools/clocksniff.c`).

### The test song: 176 bars at 140 BPM

The only 140 BPM song in the tree is `zombie-nation.rbng`, 151 bars =
258.9 s — shorter than the **5 minutes** N3 asks for — and the app's
transport loop is not wired (`ri_loop_clamp` has no caller). Rather than
stitch two plays together, the run uses a derived test song:

- The derived test song is **not in the repo**: it is cover-derived, so it
  lives in `songs/local/` (git-ignored) as `r12-n3-long140.rbng`, 10090 B,
  sha256 `cba87db91cb594c129ba7ef240d95367c393275b1a126b3e10c07e2d9f9debf9`.
  It is `songs/local/zombie-nation/zombie-nation.rbng` with **one** change:
  the STRK slot vector of the last sounding bar (row 150, `[0,0,0,7,0]`) is
  copied to row 175. Rows 151–174 stay slot 0 (silent). That makes the last
  sounding bar 176, so `ri_core_load_song()` reports
  **`RIAPP song RAM:long140.rbng: 176 bars at 140 BPM`**.
- The transport plays one bar of tail past the last sounding bar
  (`song_poll_end()` stops at `song_bars + 1`), so a run is **177 bars =
  303.43 s**. The measured first→last stamp spans below are 303.464 s and
  303.410 s.
- The clock producer is per audio block and independent of note content, so
  the silent bars still carry clock.

### Run procedure (and the discarded first attempt)

The first attempt started the sniffer while the previous RIAPP instance was
still playing. Its log contains 132 stamps (2.336 s) from that instance, an
**11.52 s startup gap**, then the new instance's play: 16995 stamps over
303.458 s. That run was discarded as a run of record (the post-gap segment
alone agrees with runA, which is a useful cross-check).

The two runs of record therefore do, in this order:

1. `Break NAME=SYS:Classes/ReIncarnation/RIAPP C` and wait 4 s — no F8s on
   the wire;
2. start `clocksniff 335 128:2` on the host;
3. launch RIAPP (`Run ... RIAPP SONG=RAM:long140.rbng`) with the config above;
4. let the song end, then pull `Vk4aros:RIAPP.LOG`.

Each run is one app process, one song load, one continuous play. The stamp
streams have **no gap > 1 s** (checked by the stats script), and the app's
final heartbeat says `run=0` — the song ended, not the measurement window.

## Result

| | runA | runB |
|---|---|---|
| F8 stamps in window | 16995 | 16992 |
| span, first→last stamp | 303.463844 s | 303.410377 s |
| intervals | 16994 | 16991 |
| rate | 56.0001 intervals/s | 56.0001 intervals/s |
| **mean interval** | **17857.117 µs** | **17857.123 µs** |
| mean − nominal (17857.1429) | −0.026 µs | −0.020 µs |
| median | 16030.1 µs | 16037.6 µs |
| standard deviation | 4523.2 µs | 4346.1 µs |
| min | 0.1 µs | 0.1 µs |
| **max (worst gap)** | **56354.8 µs** | **54949.3 µs** |
| p1 | 3465.8 µs | 4280.7 µs |
| p5 | 11040.3 µs | 11086.0 µs |
| p95 | 23589.4 µs | 23114.3 µs |
| p99 | 31619.3 µs | 29718.4 µs |

Both runs cover **5.06 minutes of play**, continuously, on the same build.
The distribution of the 16994 / 16991 intervals:

runA:

```
     0-  100 us:   135 ( 0.79%)
   100- 8000 us:   174 ( 1.02%)
  8000-12000 us:   654 ( 3.85%)
 12000-16000 us:  7135 (41.99%)
 16000-19500 us:  2708 (15.94%)
 19500-22000 us:  5190 (30.54%)
 22000-30000 us:   805 ( 4.74%)
 30000-70000 us:   193 ( 1.14%)
```

runB:

```
     0-  100 us:   107 ( 0.63%)
   100- 8000 us:   188 ( 1.11%)
  8000-12000 us:   653 ( 3.84%)
 12000-16000 us:  7156 (42.12%)
 16000-19500 us:  2661 (15.66%)
 19500-22000 us:  5240 (30.84%)
 22000-30000 us:   820 ( 4.83%)
 30000-70000 us:   166 ( 0.98%)
```

### Phase residual against an ideal grid

The interval table says how far apart consecutive clocks are; the phase
residual says how far each clock is from where an ideal 17857.1429 µs grid
would put it. With `r_i = (t_i - t_0) - i × 17857.1429 µs`, then removing
the constant offset (the mean of `r`, which is only where the first stamp
happens to sit on the grid):

| residual, mean offset removed | runA | runB |
|---|---|---|
| standard deviation | 3476.7 µs | 3277.3 µs |
| min (early) | −5.41 ms | −5.33 ms |
| p99 | +13.90 ms | +12.68 ms |
| **max (late)** | **+38.51 ms** | **+39.04 ms** |
| raw residual at the last stamp (drift) | −0.44 ms | −0.34 ms |

The residual is bounded, not a random walk: it returns toward the grid
because the producer is cumulative. The +38.5 / +39.0 ms maxima are the same
>30 ms tail as the interval table (1.1% / 1.0%), and the standard deviation
is dominated by the 16/21 ms block alternation. The sub-millisecond
end-of-run residual is the rate being exact to 0.03 µs. The raw mean offset
was −1.16 ms (runA) and −0.34 ms (runB); it is a constant phase, not drift.

- **Rate and mean are exact over five minutes**: 56.0001 intervals/s and
  17857.117 / 17857.123 µs against 17857.1429 µs (0.026 / 0.020 µs low).
- The two runs agree with each other to 0.006 µs on the mean and to 1.4 ms
  on the span.
- Guest counters during both runs: `pend=0 dropped=0 err=0` on **every**
  clock heartbeat (20 per run), and `xruns=0` on **every** heartbeat line
  (20 per run). Final running heartbeats:

  ```
  runA  buffers=55385 xruns=0 render_max=3235 us wake_max=81 us load=261/1000 overloads=0
  runB  buffers=55506 xruns=0 render_max=3282 us wake_max=46 us load=260/1000 overloads=0
  ```

  Clock out did not cost audio: no xrun and no overload in 5 minutes.

## Instrumentation accounting, and the sender-side mean

RIAPP does **not** log an interval mean (`MIDISEND` logs `mean_us`; RIAPP
logs counters). What the log carries is, once per ~15 s heartbeat, `t=`,
`buffers=` and `sent=`. The sender-side mean below is derived from the first
and last heartbeat with `run=1`, using those counters and the period the log
itself reports (`audio: ... period=5333 us`):

| | runA | runB |
|---|---|---|
| first `run=1` hb (`t`, `buffers`, `sent`) | 9304, 2895, 858 | 9667, 2876, 853 |
| last `run=1` hb | 9581, 55385, 16535 | 9944, 55506, 16572 |
| Δbuffers | 52490 | 52630 |
| Δsent | 15677 | 15719 |
| **sender mean = Δbuffers × 5333.333 µs / Δsent** | **17857.16 µs** | **17856.95 µs** |

The host measured 17857.117 / 17857.123 µs over the whole run; the
sender-side window (the run=1 heartbeats, 280 s of the 303 s) gives
17857.16 / 17856.95 µs. The two sides agree to **0.05 µs**.

`sent` counts **messages** through the PAL sink, including the start/stop
bytes, and this closes the accounting exactly:

| | guest `sent` (final) | host F8 | host non-F8 (`others`) |
|---|---|---|---|
| runA | 16997 | 16995 | 2 |
| runB | 16994 | 16992 | 2 |

16997 = 16995 F8 + FA + FC, and 16994 = 16992 + FA + FC. Nothing was lost
between the app's sink and the ALSA subscriber.

**The `t=` line is not a usable timebase at this precision.** It is a
DateStamp with 1 s resolution, and over these runs it advances ~1.1% slower
than both the audio-buffer clock and the host's `CLOCK_MONOTONIC` (runA:
277 s of `t=` against 279.95 s of buffers for the same two heartbeats). That
is why the sender-side mean above uses `buffers=`, not `t=`. The cause of
the DateStamp skew was not investigated; it does not affect the clock
schedule, which is derived from the audio sample clock.

## Why the spread is what it is

**The producer fills the ring once per audio block, not per clock.** The
session runs 256-frame blocks at 48 kHz (5.33 ms). `midi_out_render()` emits
every clock that became due since the last call, so an F8 leaves at the
**first block boundary after its sample-accurate due time**. At 140 BPM the
due times fall 3.35 blocks apart, so the instantaneous interval between
clocks alternates between 3 and 4 blocks — 16.0 ms and 21.3 ms — with the
occasional 2-block 10.7 ms catch-up. That is the 12–22 ms mass above
(88% of the intervals).

The rate is still exact because the producer is **cumulative**
(`o->last_tick < owed`): a skipped render call emits what it owes instead of
silently dropping clock bytes. Over 303 s that law holds to 0.03 µs.

The sender's 1 ms pump is not the cause of either the block spacing or the
tail: `app/timerres.c` measured the Dell's `UNIT_MICROHZ` 1 ms wait at
**min 1019 / mean 1977 / max 7928 µs** over 200 waits — slower than asked,
but well below the 16/21 ms block spacing — and `pend=0` in every heartbeat
says the ring never waited on it.

The tail (1.1% / 1.0% over 30 ms, worst 56.4 / 54.9 ms) is the same
quantisation plus host-side scheduling; it is not a rate error.

## Fix design for the owner (recorded, not built)

The 16/21 ms alternation is a release-time artifact, not a rate error. The
fix is latency compensation: carry the due time with each message and let
the sender hold it until due.

1. `midi_out_render()` stamps every ring entry with its **due time** — the
   same audio-clock/sample position that decides `owed` today — not only the
   byte count.
2. `sender_task` reads the entry, compares due time with now, and waits until
   due before handing it to the PAL sink.
3. Run the schedule behind a **fixed delay of at least one audio block**
   (5.33 ms at 256 frames / 48 kHz), so the sender always sees an entry
   before its due time. The delay is the cost; the mean and rate do not
   change.
4. The hold uses the existing timer tick. `app/timerres.c` measured that tick
   on the Dell at **mean 1977 µs, min 1019 µs, max 7928 µs** over 200 waits,
   so expect a **few milliseconds** of residual jitter around the due time,
   not ~1 ms. A finer hold needs a better timer source, which is a separate
   question.
5. Keep the cumulative `last_tick < owed` law. This changes *when* a byte is
   released, not *which* bytes are owed.

This is R2's latency-compensation item. It is **not built** in this pass; the
runs above are the un-compensated baseline.

## Measurement caveat

`clocksniff` timestamps events when it **drains** the ALSA sequencer, not
when the packet arrived. Events delivered in one poll wake-up therefore
collapse to ~0 µs (the 0.6–0.8% first bucket) even though the wire spacing
was not zero. The measured spread is an **upper bound** on the guest's wire
jitter; the rate and mean are unaffected.

## The two defects this measurement found (fixed before these runs)

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

Both fixes are in `5194334`; the measurements above are from that build.

## Artifacts

In this directory:

- `r12-n3-runA-clocks.log`, `r12-n3-runB-clocks.log` — one
  `CLOCK_MONOTONIC` nanosecond stamp per F8 (16995 / 16992 lines).
- `r12-n3-runA-heartbeat.txt`, `r12-n3-runB-heartbeat.txt` — the guest session
  from `RIAPP LOG build=` to the last heartbeat: song line, 20 × `t=`,
  `buffers=` and `clk:` lines.
- `r12-n3-stats.txt` — `tools`-independent interval statistics for both logs.

The derived song is deliberately **not** in this directory or in git: it is a
cover derivative, kept outside the repo in `songs/local/` (git-ignored),
10090 B, sha256 `cba87db9…`; its derivation is in the Setup section.

The scripts that produced the numbers (stats, run driver) and the full
unfiltered guest logs are in `/home/miller/.worktrees/r12-logs/r12-n3b/`.

## Not proven

- A DAW or hardware clock follower; the far peer is `rtpmidid 24.12.2`
  forwarding to ALSA, and `clocksniff` is the observer.
- Clock **in** (the Dell following an external master) — a different lane.
- Jitter after the fix design above: not measured, because it is not built.
  The un-compensated baseline is quantised at the 5.33 ms block; holding each
  entry to its due time should reduce that to the sender's timer resolution
  (~2 ms mean, 7.9 ms max measured), not to ~1 ms. The **rate** is already
  exact; the **instantaneous spacing** is what the fix changes.
- Whether the >30 ms tail (1.1%) is guest-side or host-side: `clocksniff`
  timestamps at drain time, so it cannot separate them.
- The cause of the guest DateStamp (`t=`) skew against the audio clock.
