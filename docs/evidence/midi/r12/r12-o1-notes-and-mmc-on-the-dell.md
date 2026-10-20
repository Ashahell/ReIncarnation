# R12 O1/O1b — RIAPP's own notes, device programme change and MMC on the Dell wire

Written 2026-10-19; O1b added 2026-10-20. Phase O1 (and its correction) of
`docs/superpowers/plans/2026-10-19-riapp-midi-out-opencode-prompt.md`.

**What this proves.** With `RIAPP_MIDI_OUT=applemidi.out.0` set (a different
cluster from `RIAPP_MIDI_IN`), RIAPP's *own* output — the 303, the 808/909
drums, the one-shot programme change, transport Start/Stop and the MMC
Play/Stop SysEx — all left the Dell on the **out** port and reached the host
`aseqdump` on `128:3` (`arosbox`). Before O0 the sender ignored its port and
MMC was refused by the AROS PAL for `len > 3`; both are now wired and both are
on the wire. `NOTEIN seen=` never moved, so nothing came back to RIAPP.

**What O1b corrects.** O1 found two things and got one of them wrong:

- The **drum-channel off-by-one is real and is now fixed**: the drums left as
  status `0x9A` (MIDI channel 11), not the GM percussion channel 10. The fix
  keeps the owner's 1-based constant `10` and converts to the wire nibble `9`
  at the status byte; O1b's wire shows every drum on column `9` and none on
  `10`.
- The **duplicated accent note-ons are not established as downstream** and are
  **not reproduced** in O1b. The O1 accounting that placed them downstream
  rested on a clock count *derived* from `sent`, not measured. O1b measures
  the guest's clocks and note-ons directly (new counters), and the run carried
  **0 duplicate pairs** in 1750 note events. The engine and scheduler are
  ruled out by host tests (one accented 303 step is exactly one note). The O1
  duplicates remain **OPEN**: a transient that did not reproduce, not a
  demonstrated lane fault.

CC out is still not built (O3).

---

## 1. What was deployed (hash-verified, pulled back)

| role | file | size | sha256 |
|---|---|---|---|
| shimmed `netmidid` (clean, from N0) | `SYS:System/Network/Services/netmidid` | 122448 B | `c71fe8ff5a63396004951af74bb336b1da62687ba28d9216f3478c02f0cb1172` |
| RIAPP run A (O1 init-order fix; no devout line) | `SYS:Classes/ReIncarnation/RIAPP` | 1207192 B | `f39c62825c7ba8ce003773611e6d241dd8ab03cc73281701fa9cda7595622d47` |
| RIAPP run B (run A + `RIAPP devout:` counters) | `SYS:Classes/ReIncarnation/RIAPP` | 1207624 B | `41afc88825ef2d7fd91f1784e9d9ef427d61eb46c28e182eae6bc5c16adc81d6` |
| RIAPP O1b (drum fix + `clocks=`/`nen=`) | `SYS:Classes/ReIncarnation/RIAPP` | 1208392 B | `5f2b163441b70557bae985a19a9f0ad4b2e77dbfbef5426a22da911db5c9b6d2` |

Runs A and B report `RIAPP LOG build=95facca diag=0` (the O0 tree plus the
uncommitted O1 fix; the short hash is unchanged because the fix was not
committed at run time). O1b reports `build=bcbe62b`, the O1b commit that
contains the drum fix and the counters. The clean `netmidid` closes the "the
gate ran on a debug build" gap from N1. The deployed bytes were pulled back
and hashed on the host; copies are in the log directory below.

**Build lane.** O1b was built with `scripts/ri_build_v11.sh` (ABIv11), not
`scripts/ri_build_aros.sh`: the latter targets the **v1** SDK and produces a
binary that takes a `Software Failure!` requester at startup on the Dell
before the first log line. That is documented in `ri_build_v11.sh` and was
reproduced here while trying the wrong lane; the two stuck requesters needed a
reboot to clear. The v11 binary reports `r12moves=193`, the same convention as
the O1 binaries.

`netmidid` N1 check 1 was re-checked on the clean binary: the rtpmidid peer
log shows the session `CONNECTED` and ALSA lists `128:3`. After the O1b reboot
the clean netmidid was verified again (`c71fe8ff…`) and restarted by hand.

## 2. Method

O1: two runs, identical settings, to remove the ambiguity that fogged run 4
(where three RIAPP sessions shared one pulled `RIAPP.LOG`). Before each run the
Dell `RIAPP.LOG`/`RIAPP-EV.LOG` were deleted and the host capture used a unique
filename.

O1b: one run after the reboot, same settings, one capture, logs deleted before
the launch. It was halted after ~59 s of the 36-bar song (before the MMC Stop
at the end), because the drum gate and the duplicate check do not need the
tail. That is why O1b has **no Stop frame**.

Guest settings (from the guest log, `RIAPP midi in=… out=…`):

```
RIAPP_MIDI_IN=applemidi.in.0   RIAPP_MIDI_OUT=applemidi.out.0
RIAPP_MIDI_CLKOUT=1 RIAPP_MIDI_DEVOUT=1 RIAPP_MIDI_NOTECH=1 RIAPP_MIDI_MMCOUT=1
RIAPP_LOG=Vk4aros:  RIAPP_EVLOG=Vk4aros:
```

**Correction (2026-10-20): `NOTECH=1` collides with the G7 remote, so the
303s' note INPUT was never exercised in O1 or O1b.** `RIAPP_MIDI_NOTECH=1`
puts the 303s on MIDI channel 1, which is the G7 remote's channel; the guest
log says so on both runs (`NOTEIN dev 0 ch 1 REFUSED (channel taken)`,
`dev 1 ch 1 REFUSED`). Note *output* on channel 1 was fine — `DEV out=1
ch=1` and 541 note-ons on the wire — but no inbound note could reach a 303.
Everything this note proves about note input is the **808's** channel-10
path. A run that needs both directions must put the 303s on a free channel;
O2e used `NOTECH=3`.

Song: `songs/` **`o1-song.rbng`**, 36 bars at 140 BPM — a short original
pattern (303 + 808 + 909 + Leviasynth), **not** a `songs/local/` cover. It
auto-plays on load and auto-stops at the end. Host capture:
`aseqdump -p 128:3` via `aseqcap.py` (stdbuf + per-line timestamps).

- **Run A** — repeat, binary `f39c6282` (no devout line).
- **Run B** — primary for O1, binary `41afc888` (adds `RIAPP devout:` counters).
- **O1b** — the correction, binary `5f2b1634` (`build=bcbe62b`).

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

The two `REFUSED (channel taken)` lines are the `NOTECH=1` collision from §2,
not evidence about the wire: the 303s asked for the G7's channel and were
refused at startup. The 808 still bound channel 10, which is the only note-in
path this run exercised.

## 4. Wire, run B (primary) — `128:3`

```
11.635801 128:3   System exclusive           F0 7F 7F 06 02 F7     <- MMC Play
11.639872 128:3   Program change           0, program 38
11.639876 128:3   Start
11.639878 128:3   Note on                  0, note 36, velocity 112   <- 303 accent
11.639880 128:3   Note on                 10, note 36, velocity 64    <- 808 bass drum (OLD: nibble 10)
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

The drum channel in this table is the **defect**, not the intended wire; O1b
below is the corrected run.

## 5. Gate O1/O1b, item by item

| gate item | result | lines |
|---|---|---|
| Programme change 38 on channel 1, **exactly once, before the first melodic note** | **PASS** | `Program change 0, program 38` @11.639872; `Start` @11.639876; first melodic note-on @11.639878. One PC only. |
| **303 on channel 1** | **PASS** | 541 note-ons on ch `0` (aseqdump is 0-based ⇒ MIDI ch 1), notes 36/39/42… the 303 line. |
| **808/909 on channel 10** | **PASS on O1b, was OPEN on O1** | O1: drums left as status nibble `0x0A` (channel 11). O1b: 669 note-ons on column `9` (= nibble 9 = `0x99` = MIDI channel 10) and **0 on column 10**; first drum note `10.793438 Note on 9, note 36, velocity 64`. |
| **Every note-on has its note-off** | **OPEN — by design for drums, plus the O1 duplicate pairs (not reproduced on O1b)** | ch0: 541 on / 540 off (net +1) on O1 run B; O1b: 540 on / 541 off (the single extra is a note-off at transport start). ch10: 665 on / **0 off** (one-shot by design). |
| **MMC Play and Stop as SysEx `F0 7F 7F 06 02 F7` / `F0 7F 7F 06 01 F7`** | **PASS** | Both frames on the wire in O1. The brief's *unverified* item is resolved: **rtpmidid does forward SysEx to ALSA.** O1b carried Play only (halted before the end). |
| **Clock still flows on the same port** | **PASS** | O1: 3548 clocks at 55.94/s over 63.408 s on `128:3`; guest `err=0`, `dropped=0`. O1b: 3482 clocks, guest `err=0`, `dropped=0`. |
| **Accounting: guest `sent` + devout counters vs the wire** | **PASS (explained, and measured on O1b)** | §6. |
| **No echo: `NOTEIN seen=` must not move** | **PASS** | One `NOTEIN … seen=0` baseline line, never re-emitted (counters log on change). Output port ≠ input port, so no feedback path. This is a no-echo result only: with `NOTECH=1` no 303 could accept input, so it says nothing about melodic note input. |
| **Drums are notes our own input accepts** | **PASS (host test + wire)** | t201 round trip: the 808 binds at nibble 9, the emitted status nibble is 9, and `ri_notein_note()` resolves it to the 808 bass drum. On the wire every drum is on column 9. |

`on=1 run=1 pend=0 dropped=0 sent=… err=0` on every heartbeat; `sent` grows
from 787 → 5304 and never resets.

## 6. Accounting — guest counters against the wire

### 6a. O1 run B (no clock counter yet)

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
messages + 1 PC).

**The 3552 is derived, not measured.** O1 had no clock counter: it subtracted
the note messages and the three transport/PC messages from `sent`. That is
fine for run B, where `emitted` is known, but it is not a guest count of
clocks, and it cannot be carried over to run A (which had no `emitted`
counter). **The "note_msgs_g ≤ 1751" bound in the earlier draft depended on
it and is retracted.** O1b adds a real `clocks=` counter (0xF8 messages that
`send_sink` handed to CAMD) and a real `nen=` counter (note-on messages).

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

### 6b. O1b (measured clocks and note-ons)

Final heartbeat (`build=bcbe62b`):

```
RIAPP clk: on=1 run=1 pend=0 dropped=0 sent=5189 err=0 clocks=3443 nen=1205
RIAPP devout: emitted=1745 refused=768 legato=0 late=143
RIAPP closed: buffers=11691 xruns=0 … stg_playing=11670 stg_stopped=21
```

The last heartbeat is at `buffers=11550`; the close is at `11691`, so
**141 buffers = 0.752 s** of output happened after the counters were frozen.
The wire over the same capture:

```
3482 clocks + 1750 note events (1209 on + 541 off) + 1 PC + 1 Start = 5234
```

and the guest at the last heartbeat sent `5189`. The wire window is 0.752 s
longer, and in that tail it carried 39 clocks and 45 ring messages — the
same order as 0.752 s × 56 clocks/s = 42 and 0.752 s of notes. The two ends
therefore agree within the unmeasured tail; this run does **not** establish a
loss figure, and does not need to. What it establishes is that `clocks=` and
`nen=` are now real guest counters, not inferences.

## 7. Run A — the O1 repeat

Same settings as run B, binary `f39c6282` (no devout counters). Guest final
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

## 8. O1b — the drum gate and the duplicate check

### 8a. Drum gate: PASS

Wire (`128:3`), first notes:

```
10.788641 128:3   System exclusive           F0 7F 7F 06 02 F7   <- MMC Play
10.788646 128:3   Note off                0, note 39, velocity 64  <- transport-start release
10.793424 128:3   Program change           0, program 38
10.793434 128:3   Start
10.793437 128:3   Note on                  0, note 36, velocity 112  <- 303 accent
10.793438 128:3   Note on                  9, note 36, velocity 64   <- 808 bass drum, nibble 9
…
72.294096 128:3   Note on                  9, note 42, velocity 64
72.400311 128:3   Note on                  9, note 46, velocity 64
```

Counts over the capture:

| class | count |
|---|---|
| Clock | 3482 |
| Program change | 1 (ch `0`, program 38) |
| Start / Stop | 1 / 0 (halted before the song end) |
| System exclusive | 1 (`F0 7F 7F 06 02 F7`, Play) |
| Note on ch `0` (303) | 540 |
| Note off ch `0` | 541 (the one extra is the transport-start release) |
| **Note on ch `9` (808/909)** | **669** |
| **Note on ch `10`** | **0** |
| Note off ch `9` | 0 (one-shot by design) |

`aseqdump` prints the status nibble, proved independently on the host: a file
sending `0x99` prints `9` (`/tmp/opencode/chantest.mid`). So column `9` is
status `0x99` = MIDI channel 10. The t201 round trip pins the same byte at the
source, and the O1b guest evlog still binds the input drum channel as
`NOTEIN dev 2 ch 10 bound` — output and input now agree.

### 8b. The redundant note-ons: 0 pairs on O1b

Duplicate detector: any two note-ons with the same channel, note and velocity
within 30 ms (not necessarily adjacent — a drum hit often sits between them).

| run | note events | duplicate pairs | spacing | pitch / velocity |
|---|---|---|---|---|
| O1 B | 1746 | 2 | 15.791, 15.752 ms | 36 / 112, 112 |
| O1 A | 1757 | 8 | 15.757–15.860 ms | 36 / 112, 112 |
| **O1b** | **1750** | **0** | — | — |

The O1 pairs were *identical*: same note (36), same velocity (112), always the
accented 303 note, 15.8 ms (≈3 audio blocks) apart, with the 808 bass drum
between them. Run A's eight pairs are at
`19.949857, 25.950443, 31.950445, 37.950488, 43.950466, 49.950511, 55.950500,
61.950522` (paired with the `+15.8 ms` line). O1b carried none.

## 9. Findings

### Finding 1 — the O1 duplicate accent note-ons: not downstream, not reproduced (OPEN)

The earlier draft called these downstream. **That claim is withdrawn.**

- **The clock figure it rested on was derived.** O1 had no clock counter, so
  `clocks_g = sent − note_msgs − 3` was an inference, and the run-A bound
  `note_msgs_g ≤ 1751` came from applying run B's `emitted` to run A. O1b now
  measures `clocks=` and `nen=` directly (§6b).
- **O1b reproduced nothing.** 0 duplicate pairs in 1750 note events, with the
  same song, settings, port and detector. A single clean run does not prove
  the lane is innocent, but it removes the basis for saying the lane is guilty.
- **The engine cannot produce a second note-on for an accented 303 step.**
  `tools/render --song` on a one-bar song with one accented step dumps exactly
  `1 NOTE_ON (flags=ACCENT) + 1 RI_EV_ACCENT + 1 NOTE_OFF`. The total accent is
  a 303 parameter change, not a hit. t199 pins that the engine's note tap
  records **one** NOTE for that step and that the 303 `RI_EV_ACCENT` adds
  **no** record; t200 feeds the same event stream and requires **one** note-on
  and one release on the wire (`ri_devout_emitted == 2` for the bar).
- **What is not ruled out.** A 15.8 ms (≈3-block) re-delivery somewhere on the
  Dell's `netmidid` / CAMD / RTP-MIDI path, or a transient in the host's ALSA
  delivery. The exact hop was not identified because the symptom did not
  reproduce. Finding the hop needs a run that still shows pairs; the guest
  side can now measure its own note-on count (`nen=`), which is the first
  split that was missing.

**Why it still matters.** If it recurs, a DAW gains a few doubled accent hits
per minute. It is small, but it must not be chased in `engine.c` on the
strength of the withdrawn claim.

### Finding 2 — the drums left on MIDI channel 11; fixed in O1b

`RI_DEVOUT_GM_PERCUSSION` is `10u` and was used **as a raw status nibble** in
`ri_devout_note()` (`buf[0] = 0x90 | ch`), with no `−1` conversion — unlike the
melodic path, which converts the 1-based setting to a nibble. So the drums
left as status **`0x9A`** = nibble 10 = **MIDI channel 11**, while
`midi_settings_notein_ch()` binds the 808 at nibble **9**. RIAPP would not
have recognised its own drums on a loopback.

**The tests agreed with the bug:** t200, t201 and t202 built the expected byte
as `0x90 | RI_DEVOUT_GM_PERCUSSION`, the same pattern as the MMC bytes
(t191 pinned `0x7E` because it was written from the wrong constant).

**Fixed (commit `bcbe62b`).** `RI_DEVOUT_GM_PERCUSSION` stays the human,
1-based `10`; `RI_DEVOUT_GM_PERCUSSION_NIBBLE` is the wire nibble `9` and
indexes `assigned[]` with the same meaning the melodic path uses. The tests
now assert literal `0x99` (on) and `0x89` (off), and t201 carries a round trip:
an emitted 808 bass drum is fed through `ri_notein_note()` and must resolve to
the 808 at nibble 9. The raw-nibble version dies as a mutant (the round trip
and the literal-byte assertions both fail).

**Gate, measured:** O1b shows 669 drum note-ons on column `9`, **0** on column
`10`, and the status byte is `0x99` (column 9 is the nibble; the host's
`aseqdump` was proved 0-based on `chantest.mid`). Recorded in the ledger as a
defect against the 2026-10-10 decision.

### Finding 3 — drums carry no note-off (one-shot, by design)

The engine emits **note-on only** for the 808/909 (`engine/engine.c:718` —
*"one-shots: no gate to release"*); `ri_devout_emit()` therefore emits a
drum note-on with no release. The wire shows 665 on / 0 off (O1) and 669 on /
0 off (O1b). This is intended, but the shared gate "every note-on has its
note-off" cannot hold for drums as written. A DAW sees dangling drum note-ons
per pass.

### Finding 4 — Leviasynth notes are refused, never guessed (by design)

`refused=768` is dominated by the Leviasynth (engine device 4): it has no note
map on the output side and is refused rather than mapped to a plausible
channel/note. By design (`R6e`), and consistent with "the R6 melodic source is
the 303". Recorded so the counter is not mistaken for a fault.

### Finding 5 — why the O1 init-order fix was needed for device out

The O1 fix (now in `riapp_main`/`midi_setup`) was **not** a tidy-up: it is
what made device out work at all. The render task was started before
`midi_setup()` read `ENVARC`, so `s_mset.dev_out` was still `0` when the
driver was wired, `drv.dev_out` stayed `NULL`, and every note and the
programme change were dropped before reaching the PAL. **Clock out worked
throughout because `clk_out` is wired elsewhere** (the clock producer is
attached independently of `dev_out`), which is exactly why the defect looked
like "notes are broken" rather than "MIDI out is broken". The wiring now
happens inside `midi_setup()` after the settings and the ring are up. Without
this, O1 would have measured a silent device path and called it a lane fault.

## 10. What remains unproven

- **CC out** — not built (`ri_devout_cc()` has no caller). O3 is design only.
- **A real DAW** on the far end; `aseqdump` is a listener, not a recorder.
- **Clock-in at 140 BPM** into RIAPP while it plays (O2's other half).
- **303 note input while RIAPP plays out.** O1/O1b set `NOTECH=1`, which the
  G7 remote already owns, so both 303s were refused; only the 808's channel-10
  input was exercised. O2e (2026-10-20) exercises a 303 on channel 3 and is
  recorded in the O2 note.
- **The O1 duplicate pairs' exact hop** — did not reproduce on O1b; the engine
  and scheduler are ruled out by host tests, the lane is not. O2 later located
  it in the host's RTP-MIDI journal replay (`r12-o2-…md` §2).
- **A note-on/off pairing gate for drums** — drums are one-shot by design, so
  the shared gate must either exclude them or count them explicitly.
- **A final-counter line at shutdown** — the last heartbeat precedes the close
  by up to a block count, so exact per-run loss accounting needs either a
  longer run that ends on a heartbeat or a final `RIAPP clk:` line.

## 11. Raw logs (kept under `/home`, not committed)

O1, under `/home/miller/.worktrees/r12-logs/r12-o1/`:

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

O1b, under `/home/miller/.worktrees/r12-logs/r12-o1b/`:

| file | what |
|---|---|
| `o1b-aseq.log` | O1b wire, `aseqdump -p 128:3` |
| `o1b-RIAPP.LOG` | O1b guest `RIAPP.LOG` |
| `o1b-RIAPP-EV.LOG` | O1b guest evlog |
| `RIAPP.deployed-o1b.bin` | the wrong-lane (v1) binary, kept as the counter-example (`7e9b8f9f…`) |
| `RIAPP.deployed-v11.bin` | the working ABIv11 binary (`5f2b1634…`) |
| `netmidid.afterboot.bin` | clean netmidid re-verified after the reboot (`c71fe8ff…`) |

Host peer log: `/home/miller/.worktrees/r12-logs/rtpmidid-dell.log`.
