# R12 O2 — both directions at once, and the duplicate note-on's hop (2026-10-20)

**Build:** `build=8d7cb30` for the counter run, ABIv11, sha256
`428554bebeb1a75f763a7a11b2c306d0e70f760cb33e093a84339650a525a056`,
deployed to `SYS:Classes/ReIncarnation/RIAPP` and pulled back hash-verified.
The gate run itself is `build=bcbe62b` (the committed O1b binary); it did not
need the new counters. The §1.1 correction run O2e reused the committed
`8d7cb30` binary (same sha256, not redeployed).

**Question.** O2 asks for R5 and R6 together on separate clusters: RIAPP plays
the song out while a short gate file arrives from the host. The same run also
reproduced the duplicated accent note-on that O1 left **OPEN**, so the second
half of this note names its hop instead of leaving it as "downstream".

## 1. Gate O2 — both directions, one run

Setup: one RIAPP, `RIAPP_MIDI_IN=applemidi.in.0`,
`RIAPP_MIDI_OUT=applemidi.out.0`, `RIAPP_MIDI_CLKOUT/DEVOUT/NOTECH/MMCOUT=1`,
song `RAM:o1-song.rbng`. Host capture on ALSA `128:3` (`arosbox`), then
`aplaymidi -p 128:3 gate-notes.mid` (the N1j file: one channel-2 note and one
channel-10 note) about 20 s into playback.

The brief set `RIAPP_MIDI_NOTECH=1`, and **the G7 remote already owns channel
1**: both 303s were refused for input at startup (`dev 0/1 ch 1 REFUSED
(channel taken)`), so 303 note input was not exercised in O2 any more than in
O1. The note-in that did move is the 808's. O2e (§1.1) uses `NOTECH=3` for a
run that needs both directions.

| Gate line | Measured |
|---|---|
| `NOTEIN played=` moves | `played=0 -> 1`; `seen=0 -> 3 -> 4`; `refused=1`, `unclaimed=2` |
| Output keeps flowing | 1635 clocks and 570 note-ons (ch0 260, ch9 310) in 39.96 s; **largest clock gap 26.6 ms**, against a 16-22 ms normal spread |
| `RIAPP clk: err=` | `0` on every line, including the last |

The channel-10 note-on is the one the 808 plays; its note-off is refused by the
implicit-drum rule. The channel-2 pair is **unclaimed by the R5 note-in rack**
because the brief set `RIAPP_MIDI_NOTECH=1`, which the G7 remote already owns
(§1.1), so no 303 could bind and the rack had no owner for channel 2.

The gate is **met**. R5 note-in and R6 note/clock-out ran in one session on
separate clusters. The R5 half is the **808's** channel-10 path; the 303s'
note input was not exercised because of the `NOTECH=1` collision, and §1.1
shows the Leviasynth still answered channel 2 independently of the rack.

### 1.1 Correction (2026-10-20): the Leviasynth did answer channel 2

An earlier reading of this run said the channel-2 note produced "no `LEVI`
lines at all, only `unclaimed=2`", and asked whether the Leviasynth had
stopped answering its own channel between N1j and O2. **The gate log
contradicts that reading: both `LEVI` lines are in it.** From
`o2-RIAPP-EV.LOG` (the gate run, `build=bcbe62b`):

```
ev 10  2 LEVI chan remote=1 levi=2 devices=2
ev 23 2618 LEVI note 36 on vel 100
ev 26 2618 NOTEIN played=0 refused=0 unclaimed=1 dropped=0 seen=1
ev 27 2719 LEVI note 36 off vel 0
ev 29 2719 NOTEIN played=1 refused=0 unclaimed=2 dropped=0 seen=3
```

That is the same event as N1j (`riapp-ev-after.txt`: `ev 24 3469 LEVI note 36
on vel 100`, `ev 28 3570 … off`), with one difference: N1j left `note_ch`
unset, so the 303A also claimed channel 2 and `played` moved (`unclaimed=0`);
O2 set `note_ch=1`, so the 303A was refused and the **rack** reported
`unclaimed=2`. The Leviasynth map is not the rack:

- `midi_drain()` calls `midi_levi_message(&s_chan, …)` **before** the NOTEIN
  funnel (`app/riapp.c:1011` vs the `ri_notein_unclaimed()` gate at `:1057`);
- `midi_chan_bind(&s_chan, RI_MCHAN_LEVI, s_mset.levi_ch)` runs at `app/riapp.c:3198`,
  **before** `ri_pal_midi_open_in()` at `:3279`, so the init-order change in
  `272cf20` did not move it after the receiver;
- `NOTEIN` counts the R5 device binds (`note_ch`/808/909), not the
  Leviasynth's own table.

**The other O2 logs do not contain the gate file.** `o2b`, `o2c` and `o2d`
were the duplicate-hop counter runs; they played the song and no
`aplaymidi`, and their capture starts at the app's first output. Their
`RIAPP-EV.LOG` files therefore have neither `LEVI note` nor a `NOTEIN` change
line, and they are not a before/after comparison for input. The one run that
played `gate-notes.mid` is `o2`.

**Host test.** `tests/unit/t234_levi_independent.c` builds both settings
shapes exactly as `midi_setup()` does (`note_ch` unset and `note_ch=1`),
feeds channel-2 note-on/off, and requires the Leviasynth action in both,
while the rack returns "resolved" in the first and "unclaimed" in the second.
It also pins `NOTECH=3`: a channel-3 note is the 303A's and is **not** a
Leviasynth note, while channel 2 remains the Leviasynth's. `ri_audit.sh`
runs it.

**Dell confirmation (O2e).** One short run, same test build `8d7cb30`, one
setting changed to `RIAPP_MIDI_NOTECH=3` so the 303s have a free channel, and
a gate file (`gate3.mid`) carrying a channel-2 note, a channel-3 note and a
channel-10 drum. Guest log:

```
RIAPP midi in=applemidi.in.0 out=applemidi.out.0 ch=1 sync=0 levi=2 … notech=3
ev 5 2 NOTEIN dev 0 ch 3 bound
ev 6 2 NOTEIN dev 1 ch 3 REFUSED (channel taken)
ev 7 2 NOTEIN dev 2 ch 10 bound
ev 29 3305 LEVI note 36 on vel 100        <- channel 2
ev 32 3305 NOTEIN played=1 … unclaimed=1  <- channel 3 played; channel 2 unclaimed by the rack
ev 33 3403 LEVI note 36 off vel 0
ev 35 3403 NOTEIN played=3 … unclaimed=2
ev 37 3464 NOTEIN played=3 refused=1 …    <- the drum release, by design
RIAPP clk: … sent=4109 err=0 clocks=2726 … (final)
```

The wire on `128:4` carried **2726 clocks** — exactly the final `clocks=` —
with `err=0` on every heartbeat. Both directions were live in one session:
the Leviasynth's channel-2 note reached its map, the 303A's channel-3 note
was played by the rack, and the 808's channel-10 note played. There was no
regression to fix, so no code changed and no mutant was added; the pin is
t234 and the measurement is this section.

## 2. The duplicate note-on: the hop is the host's rtpmidid journal replay

O1 recorded duplicated accent note-ons and deliberately did **not** call them
downstream. O1b's run had zero pairs, so the finding stayed open. O2, O2b, O2c
and O2d all reproduced them, which allowed the hop to be measured instead of
inferred.

### 2.1 The signature, and how often it appears

Every pair is the same message: **channel 0, note 36, velocity 112**, the 303
accent. The first copy is the musical note-on; the second is one packet later
(15.7-16.0 ms), with a drum hit between them, so the pair is not adjacent.

| Run | Build | Pairs |
|---|---|---|
| O1 A | `272cf20` | 8 |
| O1 B | `272cf20` | 2 |
| O1b | `bcbe62b` | **0** |
| O2 | `bcbe62b` | 4 |
| O2b | `d5c7581` + O2 working tree | 5 |
| O2c | `d5c7581` + O2 working tree | 5 |
| O2d | `8d7cb30` | 5 |

The run-to-run variation is a **phase** effect, not flakiness in RIAPP: the
Dell's journal checkpoint repeats every **337 packets** (measured exactly in
the host log: seqnum 33461, 33798, 34135, 34472, 34809), which at the 56 Hz
clock rate is 6.0 s. The song's accent pattern is also clocked from the same
56 Hz, so when a checkpoint lands in the packet right after an accent note-on
the pair appears; when the session starts at another phase, none do. O1b is
the zero-phase run.

### 2.2 The split: app -> netmidid -> wire

O2d, one run, three independent counts:

| Where counted | Value |
|---|---|
| RIAPP `RIAPP clk:` final (after the sender stopped) | `sent=2712 clocks=1802 nen=626 n36a=38 pend=5 err=0` |
| netmidid `NETMIDISTATUS`, delta over the run | `msg_out=2711 pkt_out=1807`, `journals_omitted=0`, `pkt_lost=0` |
| Host wire (`aseqdump` on 128:4) | 2716 messages: 1800 clocks, 631 note-ons, 282 offs, 1 PC, 1 MMC SysEx, 1 Start; **n36a=43** |

`n36a` is a diagnostic counter at RIAPP's CAMD boundary: it counts exactly
`90 24 70` (channel 0, note 36, velocity 112) calls to `PutMidi`. It says the
app sent that message **38** times; the wire carries it **43** times. The five
extra copies are exactly the five duplicate pairs.

netmidid's own `msg_out` delta is **2711**, which is RIAPP's 2712 sends plus
the one direct MMC SysEx minus two clocks that were lost between `PutMidi`
and the service's encoder (the wire is two clocks short of the guest's
`clocks=1802` as well), and it is **five short of the wire**. The journal is a separate RTP-MIDI section, so it
is not part of `msg_out`: netmidid encoded 2711 normal messages and the host
turned them into 2716.

The wire minus netmidid is +5; the app's own n36a count is 5 lower than the
wire. **The second copy never left RIAPP, and never left netmidid's message
encoder.**

### 2.3 The host log shows the exact bytes

The rtpmidid log records the packet before and the journal packet after. Each
of the five duplicates has this shape (run O2d, seqnum 33460 -> 33461):

```
I got data from seqnum 33460. 0 channels.
  ... 47 90 24 70 00 99 24 40 40 ...      <- 90 24 70: the accent note-on
I got data from seqnum 33461. 1 channels.
  Chapters: 00001000
  Parse chapter N, channel 0
  1 note on count, -13 noteoff count
  ... 81 F1 24 F0 48 0A 0C 81 F1 24 C0 00 24 ...
```

Chapter N decodes as: one note-on, note `0x24` = 36, velocity byte `0xF0` =
`0x80 | 0x70` — the high bit is the Y bit and velocity is 112. The note
number byte is `0x24`, not `0x80 | 0x24`, which is the sender saying this
note-on was in the **previous packet** (one packet = 15.8 ms, the measured
gap).

**What RFC 6295 says the Y bit is for.** In Appendix A.6.2, the Y bit is a
sender's hint about the execution time of a NoteOn **recovered from the note
log**: Y=1 recommends playing it, Y=0 recommends skipping it. It is not a
live note-on, and Chapter N carries no execution time of its own. Section 4
("The Recovery Journal System") gives the journal its purpose and its
trigger: the journal codes history back to a checkpoint so a receiver can
repair the stream, and the receiver role is stated as "**If** the sequence
number break is due to a packet loss event … the receiver MUST repair all
indefinite artifacts … caused by the loss." The only case that resembles a
live apply is the first packet of a session, which the RFC explicitly says to
process "as if it were a packet that ends a loss event". There is no
sequence break at seqnum 33461, so the Y=1 note is not a packet-loss
recovery; replaying it as live MIDI is the receiver acting outside that
section.

The receiver's code is `lib/rtppeer.cpp` in rtpmidid v24.12
(`parse_journal` -> `parse_journal_chapter` -> `parse_journal_chapter_N`).
For every journaled note-on with Y=1 it calls `midi_event()` unconditionally —
it does not first ask whether a packet was actually lost, and it does not use
the journal only for recovery. The source itself calls this path WIP (the
warning `This RTP MIDI header has journal. WIP.` is logged on every packet).
So the journal state is being **played as live MIDI**, one packet later.

That is the hop. The duplicate is a **host-side RTP-MIDI journal replay**,
below netmidid's message count; it is not RIAPP's engine, devout, ring, CAMD
send, or netmidid's encoder.

### 2.4 The negative probes, and why they are negative

`MIDISEND` probes were run at the same time, to try to reproduce it without
the app:

- one held note with no following data packet;
- the same with a 20 ms clock stream (F8);
- two notes 40 ms apart.

All three arrived once, with no journal chapter N in the host log. That is
consistent with the mechanism, not a contradiction: the chapter is emitted on
the sender's 337-packet checkpoint and carries the note-on only while its Y
bit is set (`elapsed <= 1000`, i.e. 100 ms). A 2-second script never crosses a
checkpoint, so it cannot show the replay. The song does, because the checkpoint
and the accent live on the same 56 Hz grid. The probes are recorded as a
**negative control** for the checkpoint rule, not as evidence against the hop.

## 3. What is left for the owner

The repair is a lane decision, not an app change:

- **rtpmidid:** apply the journal only on a sequence gap (RFC 6295's recovery
  rule) instead of replaying every Y=1 note in every packet. This is the
  host's bug as observed, and it is WIP upstream.
- **netmidid:** the sender sets Y from `elapsed <= 1000` ("recent"), not from
  "the receiver may not have it". If the intended meaning is the RFC one, that
  bit is also wrong. Either change alone removes the duplicate; the pair is
  worth fixing together.

Neither was changed here. The O2 brief is a Dell/app phase, and the owner has
not chosen a lane fix.

## 4. What this proves, and what it does not

**Proven:** the O2 gate (both directions, no output stall, `err=0`); the app's
send count for the duplicated message (38) versus the wire (43); netmidid's
encoded count (2711) versus the wire (2716); the journal bytes and the
receiver code path that turns them into the extra note-on.

**Not proven / not done:** a real DAW on the other end; clock-in at 140 BPM
still rests on the O1/N3 runs; CC out is not built and its design is only now
decided (O3); the 303s' note input was refused in O2 by the `NOTECH=1`
collision and is exercised only in the short O2e run (§1.1); and the exact
netmidid rule that picks the 337-packet checkpoint is measured from the wire,
not instrumented in `sender.c`.

## 5. Raw logs

Under `/home/miller/.worktrees/r12-logs/r12-o2/`:

- `o2-aseq.log`, `o2-RIAPP.LOG`, `o2-RIAPP-EV.LOG` — the gate run (O2).
- `o2b-aseq.log`, `o2b-RIAPP.LOG`, `o2b-RIAPP-EV.LOG` — first counter build.
- `o2c-aseq.log`, `o2c-RIAPP.LOG`, `o2c-RIAPP-EV.LOG` — netmidid read after a
  restart, counters at zero.
- `o2d-aseq.log`, `o2d-RIAPP.LOG`, `o2d-RIAPP-EV.LOG` — the committed build
  `8d7cb30`; the numbers in 2.2 are from this run.
- `jprobe-aseq.log`, `jprobe2-aseq.log`, `jprobe3-aseq.log` — the negative
  `MIDISEND` probes.
- `netmidi.out` — the clean service's stdout, empty (no packet log in it).

Under `/home/miller/.worktrees/r12-logs/r12-o2e/` (the §1.1 correction):

- `o2e-aseq.log` — wire, `aseqdump -p 128:4`; 2726 clocks.
- `o2e-RIAPP.LOG`, `o2e-RIAPP-EV.LOG` — guest log and evlog (`build=8d7cb30`,
  `notech=3`).
- `RIAPP.OUT` was not retrievable (`cannot open file`; the redirect lives on
  `RAM:` and was not copied back). It is not needed: `RIAPP.LOG` and the wire
  are the two ends.

Gate file: `/tmp/ri/o2e/gate3.mid` (channel 2, channel 3, channel 10).

Host rtpmidid log (live, all sessions):
`/home/miller/.worktrees/r12-logs/rtpmidid-dell.log`; the O2d chapter-N
blocks are at lines 521778, 523614, 525453, 527289, 529127.

Deployed binary: `/tmp/opencode/RIAPP.deployed-8d7cb30.bin` (sha256 above).
The netmidid service was the clean local build `c71fe8ff...` (122448 B) used
in O1b; it was restarted once before O2c so `NETMIDISTATUS` starts at zero,
and `rtpmidid` stayed up with `--rtpmidi-discover false` throughout.
