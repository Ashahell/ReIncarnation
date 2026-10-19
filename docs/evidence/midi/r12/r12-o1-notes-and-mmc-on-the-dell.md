# R12 O1 — RIAPP's own notes, device programme change and MMC on the Dell wire

Written 2026-10-19. Phase O1 of
`docs/superpowers/plans/2026-10-19-riapp-midi-out-opencode-prompt.md`.

**What this proves.** With `RIAPP_MIDI_OUT=applemidi.out.0` set (a different
cluster from `RIAPP_MIDI_IN`), RIAPP's *own* output — the 303, the 808/909
drums, the one-shot programme change, transport Start/Stop and the MMC
Play/Stop SysEx — all left the Dell on the **out** port and reached the host
`aseqdump` on `128:3` (`arosbox`). Before O0 the sender ignored its port and
MMC was refused by the AROS PAL for `len > 3`; both are now wired and both are
on the wire. `NOTEIN seen=` never moved, so nothing came back to RIAPP.

**What this does not prove, and two things it found.** CC out is still not
built (O3). And the shared "every note-on has its note-off" gate is **not
clean**: drums are one-shot by design, and the melodic channel shows a
handful of *duplicated accent note-ons* that the accounting places
**downstream of RIAPP** (the network/RTP path), not in the guest. Both are
written up as OPEN below. There is also a **drum-channel off-by-one**: the
drums left as status nibble `0x0A`, i.e. MIDI channel 11, not the GM
percussion channel 10.

---

## 1. What was deployed (hash-verified, pulled back)

| role | file | size | sha256 |
|---|---|---|---|
| shimmed `netmidid` (clean, from N0) | `SYS:System/Network/Services/netmidid` | 122448 B | `c71fe8ff5a63396004951af74bb336b1da62687ba28d9216f3478c02f0cb1172` |
| RIAPP run A (O1 init-order fix; no devout line) | `SYS:Classes/ReIncarnation/RIAPP` | 1207192 B | `f39c62825c7ba8ce003773611e6d241dd8ab03cc73281701fa9cda7595622d47` |
| RIAPP run B (run A + `RIAPP devout:` counters) | `SYS:Classes/ReIncarnation/RIAPP` | 1207624 B | `41afc88825ef2d7fd91f1784e9d9ef427d61eb46c28e182eae6bc5c16adc81d6` |

Both RIAPP binaries report `RIAPP LOG build=95facca diag=0`, i.e. **the O0
tree** (`95facca`) plus the O1 init-order fix and the devout counters; the
short hash is unchanged because the fix is uncommitted at the time of the
runs. The clean `netmidid` closes the "the gate ran on a debug build" gap
from N1. The deployed bytes were pulled back and hashed on the host; copies
are in the log directory below.

`netmidid` N1 check 1 was re-checked on the clean binary before the runs: the
rtpmidid peer log shows the session `CONNECTED` and ALSA lists `128:3`.

## 2. Method

Two runs, identical settings, to remove the ambiguity that fogged run 4 (where
three RIAPP sessions shared one pulled `RIAPP.LOG`). Before each run the Dell
`RIAPP.LOG`/`RIAPP-EV.LOG` were deleted and the host capture used a unique
filename.

Guest settings (from the guest log, `RIAPP midi in=… out=…`):

```
RIAPP_MIDI_IN=applemidi.in.0   RIAPP_MIDI_OUT=applemidi.out.0
RIAPP_MIDI_CLKOUT=1 RIAPP_MIDI_DEVOUT=1 RIAPP_MIDI_NOTECH=1 RIAPP_MIDI_MMCOUT=1
RIAPP_LOG=Vk4aros:  RIAPP_EVLOG=Vk4aros:
```

Song: `songs/` **`o1-song.rbng`**, 36 bars at 140 BPM — a short original
pattern (303 + 808 + 909 + Leviasynth), **not** a `songs/local/` cover. It
auto-plays on load and auto-stops at the end. Host capture:
`aseqdump -p 128:3` via `aseqcap.py` (stdbuf + per-line timestamps).

- **Run A** — repeat, binary `f39c6282` (no devout line).
- **Run B** — primary, binary `41afc888` (adds `RIAPP devout:` counters).

`rtpmidid` ran host-side throughout with `--rtpmidi-discover false`.

## 3. Guest log, run B (primary)

```
RIAPP LOG build=95facca diag=0
RIAPP midi in=applemidi.in.0 out=applemidi.out.0 ch=1 sync=0 levi=2 clkout=1 lat=0 devout=1 notech=1
RIAPP song RAM:o1-song.rbng: 36 bars at 140 BPM
RIAPP play
… (heartbeats) …
RIAPP clk: on=1 run=1 pend=0 dropped=0 sent=5304 err=0      <- final, after Stop
RIAPP devout: emitted=1750 refused=768 legato=0 late=143    <- final, after Stop
RIAPP stop
RIAPP closed: buffers=16375 xruns=0 render_max=2934 us … stg_playing=11895 stg_stopped=4480
```

`err=0` and `dropped=0` on every `RIAPP clk:` line; `xruns=0` on every
heartbeat. The evlog for run B:

```
ev 5 2 NOTEIN dev 0 ch 1 REFUSED (channel taken)
ev 6 2 NOTEIN dev 1 ch 1 REFUSED (channel taken)
ev 7 2 NOTEIN dev 2 ch 10 bound
ev 8 2 NOTEIN bound 2 drums_implicit=1
ev 9 2 NOTEIN played=0 refused=0 unclaimed=0 dropped=0 seen=0     <- baseline
ev 12 2 DEV out=1 ch=1 prog=38
ev 13 2 MMC out=1
ev 20 21 SONG load RAM:o1-song.rbng bars=36 bpm=140
ev 21 21 MMC tx=2 bytes=6        <- Play
ev 22 21 TR PLAY
ev 79 11917 MMC tx=1 bytes=6     <- Stop
ev 80 11917 TR STOP
```

Note the **single** `NOTEIN … seen=0` line: the NOTEIN counters are logged on
change, so a baseline that never moves is exactly one line. It never moved.

## 4. Wire, run B (primary) — `128:3`

```
11.635801 128:3   System exclusive           F0 7F 7F 06 02 F7     <- MMC Play
11.639872 128:3   Program change           0, program 38
11.639876 128:3   Start
11.639878 128:3   Note on                  0, note 36, velocity 112   <- 303 accent
11.639880 128:3   Note on                 10, note 36, velocity 64    <- 808 bass drum
…
75.074495 128:3   System exclusive           F0 7F 7F 06 01 F7     <- MMC Stop
75.078738 128:3   Stop
```

Counts over the run window (first event to Stop):

| class | count |
|---|---|
| Clock | 3548 (first 11.65563 → last 75.063828; span 63.408 s; **55.94 clocks/s**) |
| Program change | **1** — channel `0` (= MIDI 1), program 38, at 11.639872 |
| Start / Stop | 1 / 1 |
| Note on ch `0` (MIDI 1, the 303) | 541 |
| Note off ch `0` | 540 |
| Note on ch `10` (nibble `0x0A`, the 808/909) | 665 |
| Note off ch `10` | **0** |
| System exclusive | 2 (`F0 7F 7F 06 02 F7`, `F0 7F 7F 06 01 F7`) |
| Control change | 0 |

## 5. Gate O1, item by item

| gate item | result | lines |
|---|---|---|
| Programme change 38 on channel 1, **exactly once, before the first melodic note** | **PASS** | `Program change 0, program 38` @11.639872; `Start` @11.639876; first melodic note-on @11.639878. One PC only. |
| **303 on channel 1** | **PASS** | 541 note-ons on ch `0` (aseqdump is 0-based ⇒ MIDI ch 1), notes 36/39/42… the 303 line. |
| **808/909 on channel 10** | **OPEN — see Finding 2** | Drums left as status nibble `0x0A`; aseqdump prints `10`, but by the project's own rule (nibble+1) that is MIDI channel **11**, not the GM percussion channel 10 (nibble `0x09`). |
| **Every note-on has its note-off** | **OPEN — Findings 1 & 3** | ch0: 541 on / 540 off (net +1). ch10: 665 on / **0 off** (one-shot by design). |
| **MMC Play and Stop as SysEx `F0 7F 7F 06 02 F7` / `F0 7F 7F 06 01 F7`** | **PASS** | Both frames on the wire (above). The brief's *unverified* item is resolved: **rtpmidid does forward SysEx to ALSA.** |
| **Clock still flows on the same port** | **PASS** | 3548 clocks at 55.94/s over 63.408 s on `128:3`; guest `err=0`, `dropped=0`. |
| **Accounting: guest `sent` + devout counters vs the wire** | **PASS (explained)** | §6. |
| **No echo: `NOTEIN seen=` must not move** | **PASS** | One `NOTEIN … seen=0` baseline line, never re-emitted (counters log on change). Output port ≠ input port, so no feedback path. |

`on=1 run=1 pend=0 dropped=0 sent=… err=0` on every heartbeat; `sent` grows
from 787 → 5304 and never resets.

## 6. Accounting — guest `sent` and `emitted` against the wire

Run B, final:

```
RIAPP clk:    sent=5304 err=0 dropped=0
RIAPP devout: emitted=1750 refused=768 legato=0 late=143
```

`sent` is the **whole sender ring** (`platform/aros/midi_camd.c` `send_sink()`
→ `s_sent++`): clock, notes, the one-shot programme change, and Start/Stop.
It does **not** include MMC, which the main task sends directly via
`ri_pal_midi_send()` and never routes through the ring.

Guest ring contents, decomposed:

```
3552 clocks + 1749 note messages + 1 PC + 1 Start + 1 Stop = 5304   (= sent)
```

The 1749 note messages come from `RIAPP devout: emitted=1750` (= 1749 note
messages + 1 PC). The guest's own clock count is fixed by the audio clock:
`sent − 1749 − 3 = 3552 clocks`, which matches 63.45 s of playing at 140 BPM
(56 clocks/s).

Wire, same run:

```
3548 clocks + 1746 note messages + 1 PC + 1 Start + 1 Stop = 5297
plus 2 MMC SysEx (not in `sent`)                          = 5299 total
```

**Difference = 5304 − 5297 = 7 ring messages** (4 clocks + 3 note messages),
i.e. **0.13 %**, lost on the host path — not the guest. The 2 extra wire
messages are the MMC SysEx, which `sent` deliberately does not count.

`refused=768` are note records the devout module rejected under its own
laws. The dominant class is the **Leviasynth** (engine device 4): no note map
exists for it, and `ri_devout_emit()` **refuses rather than guesses**
(device > `RI_DEVOUT_909`). Nothing on the wire corresponds to them, which is
correct — the R6 melodic source is the 303, not the Leviasynth.
`legato=0` (this song has no slides), `late=143` counts 808/909 total-accent
records the wire cannot carry (there is no "make the note I already sent
louder").

## 7. Run A — the repeat

Same settings, binary `f39c6282` (no devout counters). Guest final
`sent=5306 err=0 dropped=0`.

```
13.943226 128:3   System exclusive           F0 7F 7F 06 02 F7   <- MMC Play
13.949463 128:3   Program change           0, program 38
13.949484 128:3   Start
13.949486 128:3   Note on                  0, note 36, velocity 112
…
77.419451 128:3   System exclusive           F0 7F 7F 06 01 F7   <- MMC Stop
77.420600 128:3   Stop
```

Wire: 3552 clocks (63.446 s, 55.97/s), 1 PC, Start/Stop 1/1, 2 MMC SysEx,
ch0 548 on / 540 off, ch10 669 on / 0 off. Same gate results as run B.

**Capture artifact (excluded):** the run-A capture stayed open after the run
and caught one stray `F0 7F 7F 06 02 F7` at `383.422494` (~6 min later) from a
later RIAPP load. The guest log for run A shows a single session; the stray is
outside the run window and is excluded from every count above. It is recorded
rather than hidden because it is exactly the kind of cross-talk that made
run 4 ambiguous.

## 8. Findings (OPEN)

### Finding 1 — the duplicated accent note-ons are downstream of RIAPP, not the guest

**Symptom.** In both runs the melodic channel carries a few more note-ons than
note-offs, and the surplus is exactly the count of *duplicated accent
note-ons*: two identical `Note on ch 0, note 36, velocity 112` a hair apart,
with a single note-off.

| run | ch0 on/off | net | duplicate pairs (pitch 36) | spacing | orphan offs |
|---|---|---|---|---|---|
| B (primary) | 541 / 540 | **+1** | 2 — @17.6561, @23.6567 | 15.8 ms (≈3 audio blocks) | 1 — pitch 42 @52.4038 |
| A (repeat) | 548 / 540 | **+8** | 8 — @19.966 + k·6.000 s | 15.8 ms | 0 |

The pairs land every 6.000 s (30 times the 200 ms tick), *not* on a musical
grid, and the count differs between runs (8 vs 2) — a race, not a pattern
feature.

**The accounting places them downstream of RIAPP.** For run A:

- `RIAPP clk: sent=5306` = clocks + note messages + 1 PC + 1 Start + 1 Stop,
  so `clocks_g + note_msgs_g = 5303`.
- The wire shows `clocks_w = 3552` and no duplicated clocks (no zero-length
  clock interval; one 7.6 ms jitter). The host cannot *invent* clocks, so
  `clocks_g ≥ 3552`, hence **`note_msgs_g ≤ 1751`**.
- The wire carries **`note_msgs_w = 1757`** (1217 on + 540 off).
- `note_msgs_w > note_msgs_g`: the network path delivered **more** note
  messages than RIAPP sent. Since it can only lose or duplicate, the surplus
  (≥ 6, consistent with the 8 observed pairs) was **duplicated downstream of
  the guest**.

The run-B orphan note-off (an off with no prior on, pitch 42) is the mirror
image — a note-on dropped on the same path — and is the same class.

**Candidates, now weighted.** (i) *Guest-side note-tap race* — **not
supported**: the counts cannot be produced by RIAPP alone (run A). (ii)
*Host/RTP duplication* — **supported**. rtpmidid is not the obvious duplicator
(its log says the inbound RTP-MIDI journal headers are `WIP` — it parses them
and sends feedback but does not replay from the journal), so the duplication
is more likely in the Dell's `netmidid` or the ALSA delivery under the
`128:3`/`128:2` port pair. **A host-side repro was attempted and blocked**:
`/tmp/ri/build/render --rbngsong … --dump-events` fails with
`RBNG invalid: … ATRK @5642: ATRK without buffer`.

**Why it matters.** A DAW recording this pass would gain a handful of doubled
accent hits and one dangling note-off per ~60 s. It is small (2–8 events), but
it is *not* the guest, so chasing it in `engine.c` is the wrong tree.

### Finding 2 — the drums left on MIDI channel 11, not GM channel 10

`RI_DEVOUT_GM_PERCUSSION` is `10u` (`midi_io/midi_devout.h:65`) and is used
**as a raw status nibble** in `ri_devout_note()` (`buf[0] = 0x90 | ch`), with
no `−1` conversion — unlike the melodic path, which converts the 1-based
setting to a nibble (`midi_settings_note_ch()` returns `ch − 1`;
`RIAPP_MIDI_NOTECH=1` ⇒ nibble 0 ⇒ MIDI channel 1, exactly as the wire shows
for the PC and the 303).

Consequences:

- The drums left as status **`0x9A`** — aseqdump prints `Note on 10`, but
  aseqdump is 0-based (proved on the host: a file sending nibble `0x99`
  prints `9`; nibble `9` = MIDI channel 10). `0x9A` is nibble 10 = **MIDI
  channel 11**. The project's own ledger already states the rule:
  *"`0x9A` is nibble 10, i.e. channel 11; `0x99` is channel 10"*
  (`docs/evidence/midi/ledger.md:4267`).
- The project's **inbound** drum path uses **9**: `midi_settings_notein_ch()`
  returns `9` for the 808, and the ledger says *"the answer is 9 whatever the
  settings say"*. So RIAPP would **not recognise its own drum output** if it
  were looped back (the 808 is bound at nibble 9; the output is nibble 10).

This is **pre-existing** code (not touched by O0/O1) and no test catches it
because `t201` asserts against `RI_DEVOUT_GM_PERCUSSION` itself. It is a
one-constant change (`10 → 9`) if the owner agrees; **not changed here**
pending an owner decision, because it is a behaviour change outside the O0/O1
brief.

Gate note: the brief's O1 gate reads "808/909 notes on channel 10". If read
literally as GM channel 10, this item **fails**; if read as "aseqdump prints
10", it passes. The two readings differ, and the code/ledger/inbound path all
say the intended channel is 10 (nibble 9).

### Finding 3 — drums carry no note-off (one-shot, by design)

The engine emits **note-on only** for the 808/909 (`engine/engine.c:718` —
*"one-shots: no gate to release"*); `ri_devout_emit()` therefore emits a
drum note-on with no release. The wire shows 665 on / 0 off. This is intended,
but the shared gate "every note-on has its note-off" cannot hold for drums as
written. A DAW sees 665 dangling drum note-ons per minute-long pass.

### Finding 4 — Leviasynth notes are refused, never guessed (by design)

`refused=768` is dominated by the Leviasynth (engine device 4): it has no note
map on the output side and is refused rather than mapped to a plausible
channel/note. By design (`R6e`), and consistent with "the R6 melodic source is
the 303". Recorded so the counter is not mistaken for a fault.

## 9. What remains unproven

- **CC out** — not built (`ri_devout_cc()` has no caller). O3 is design only.
- **A real DAW** on the far end; `aseqdump` is a listener, not a recorder.
- **Clock-in at 140 BPM** into RIAPP while it plays (O2's other half).
- The **downstream duplicator's exact identity** (netmidid vs ALSA delivery);
  the repro was blocked.

## 10. Raw logs (kept under `/home`, not committed)

All under `/home/miller/.worktrees/r12-logs/r12-o1/`:

| file | what |
|---|---|
| `o1-devout-aseq.log` | run B wire, `aseqdump -p 128:3` |
| `o1-devout-RIAPP.LOG` | run B guest `RIAPP.LOG` |
| `o1-devout-RIAPP-EV.LOG` | run B guest evlog |
| `o1-clean-aseq.log` | run A wire (includes the excluded stray at 383.4 s) |
| `o1-clean-RIAPP.LOG` | run A guest `RIAPP.LOG` |
| `o1-clean-RIAPP-EV.LOG` | run A guest evlog |
| `RIAPP.deployed-o1fix.bin` | run A binary pulled back (`f39c6282…`) |
| `RIAPP.deployed-o1devout.bin` | run B binary pulled back (`41afc888…`) |
| `netmidid.deployed-clean.bin` | clean shimmed netmidid (`c71fe8ff…`) |

Host peer log: `/home/miller/.worktrees/r12-logs/rtpmidid-dell.log`.
