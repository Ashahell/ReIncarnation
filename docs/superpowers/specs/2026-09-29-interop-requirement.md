# Interoperability with other music programs (owner requirement, 2026-09-29)

**Status:** requirement recorded; **R1–R4 implemented** (2026-10-08 – 2026-10-09,
commits `7c9fe32`…`7b178c4`, all tagged `[levi-midi]`). P1 sync-in is done and
**parked, not passed**: following is proven at ~60 BPM on the Dell and the lock
law at the song's 140 BPM on the host, but **not at 140 BPM on real hardware**,
because this guest's CAMD delivers clocks in 10 ms system-tick batches.
**R2/R3 implemented 2026-10-09/10** (M5a–M5e1, commits `e3ec539`…`0b04879`):
clock out is a drift-free 24 ppqn schedule driven by the render's audio
sample clock — the tick count at a position is a pure function of that
position, so it cannot drift — and MMC in lands on the *follower's* intents,
so a master sending clock and a master sending MMC drive one path.
**Remaining:** the F8-interval measurement from a real receiver (the proof
tool `MIDIRX` has never completed a run; the ledger records a recommendation
to take the intervals from inside RIAPP instead), MMC out, the clock-out
LED, R5 (303/808/909 note input), R6, R7 (SMF), P3 and P4 — everything
still off by default, pending owner decision 1.
Full record: `llm-wiki/raw/articles/2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md`;
E0 defaults in `docs/evidence/midi/ledger.md`.

**Owner question (2026-09-29):** "can ReIncarnation work with Ableton?"

**Owner instruction:** "document that we need MIDI clock support and anything else required to communicate with other music programs."

**Related documents:**
- spec `2026-09-20-reincarnation-spec.md`:
  - §13 "MIDI: CAMD backend, note→step, CC→ctl_id learn, clock/MMC; hot-unplug survival";
  - §17 failures 4 and 8;
  - W3 "network/MIDI expansion" (deferred);
- `docs/evidence/gui/midi.md` (G7 remote control);
- `midi_io/midi.{h,c}`;
- portability plan `docs/superpowers/plans/2026-09-26-portability-plan.md` (Windows backends later).

## Where we are today

**Remote control in (G7, done):**
- A CAMD receiver on one channel maps CC n to the panel control of that controller number (all 99 pinned in t73).
- Notes 12–96 drive the switches, pattern select and Program Synth (manual ch. 13, pp. 127–134 and Appendix C).
- Proven through real `camd.library` on riqemu1.

**MIDI clock in (M0–M3, done 2026-10-08/09; parked, not passed):**
- **Following is live.** A scripted master starts, stops, seeks and sets the tempo: `FA`/`FB` arm and the **next `F8`** fires Start/Continue (MIDI 1.0), 3 s of silence latches exactly one Stop, `SPP` locates while stopped (and is counted-and-ignored while running), and the session tempo follows the measured clock through a **bounded phase servo** (0.01 BPM/tick, max ±2.0 BPM; lock deadline 2 beats — all E0, ledgered). The tempo knob goes read-only and TAP goes dead while following; the display shows the measured value.
- **Proven on the Dell:** a 5-minute take at ~62 BPM — **7500/7500 clocks on both sides, drops 0,0,0, 0 xruns**, engine frozen 0 of 155 samples, phase error −125…−32 ticks **converging** (mean −82.9 → −39.3), ending 43 ticks (0.45 s) from the master after five minutes.
- **Not proven:** 140 BPM on real hardware. The lock law accepts steady 17.857 ms intervals (proven on the host) but this guest's CAMD batches clocks at the 10 ms system tick. `MIDICLOCK` sends a clean 139.88 BPM and the app cannot see it through the batching. Closing this needs a **USB-MIDI interface and a real master** (the actual use case) or a batch-tolerant estimator.
- Followed takes are **live-only** — no tempo/transport history is recorded (E0, pending owner decision D2).
- The Sync LED still works as before, driven off the same stream.
- `midi_io/midi.h`'s `midi_clock_*` display model and `midi_mmc_cmd` are still **unwired and untouched**. Two clock models still coexist; only `midi_follow_*` can drive tempo, and it is the one wired.
- **Second device (M4).** G7 still owns channel 1 alone; the Leviasynth owns channel 2 with its own CC map (manual pp. 168–169). A CC on one channel can never move the other device's controls, pinned by t183. **No note input for the 303s, 808 or 909 yet** — that is R5.

**Output:**
- **Clock out and MMC out are built but off by default** (`RIAPP_MIDI_CLKOUT`, default 0). The render fills a byte ring and an AROS sender task carries it to CAMD, so no CAMD call ever happens in the render path. **Nothing else is sent**: the G7 receiver still "never transmits" (manual p. 127), and there is no MIDI out of notes or CC. **Unmeasured**: the F8 intervals as they actually reach a wire — the host tests prove the schedule, not the wire.

**Audio:**
- the offline renderer writes WAV;
- RIAPP's **W** records the live output to `RAM:RIAPP.wav`;
- there are no stems (one stereo mix only).

**Files:** RBNG/RBNM songs only. No Standard MIDI File import or export.

**Other:**
- no Ableton Link;
- no plugin build;
- ReIncarnation runs only on AROS, where no mainstream DAW runs.

## What "working with Ableton (or any DAW)" needs

In priority order. Each item is a requirement to design later; none is designed here.

### P1: sync over MIDI (the minimum; works across two machines with a MIDI cable or USB-MIDI)

**R1. MIDI clock in (follow).** Clock `0xF8` at 24 ppqn, Start `0xFA`, Continue `0xFB`, Stop `0xFC`, Song Position Pointer `0xF2`.
- The transport follows: Start plays from the song start, Continue from the current position, Stop stops, and SPP moves the position (in 16ths).
- **Tempo follows the incoming clock.** Smooth it (a jitter filter; USB-MIDI and CAMD time stamps are coarse) and phase-lock beats to the clock, so the long-term drift is zero. `midi_clock_drift` is the starting point.
- **Sync source setting:** Internal or MIDI. While following, the tempo control shows the measured tempo and is read-only.
- **Clock dropout:** hold the last tempo, then stop after a timeout (E0 value, ledgered). Never hang (spec §17 failure 8).
- **Latency offset setting (ms)**, so ReIncarnation's audio lines up with the master's.
- **Realtime contract (spec §4, LOCKED):**
  - clock events arrive through the CAMD bridge task and cross to the render task only through the control plane or snapshot (SPSC);
  - the render path does no MIDI calls.
- **One renderer (spec §5):** a followed performance must be reproducible offline from the recorded tempo/transport history. Decide whether the history is recorded, or a followed take is live-only; this is an owner decision.

**R2. MIDI clock out (lead).** The same messages sent while ReIncarnation is the master: 24 ppqn from the render timeline, Start/Continue/Stop, and SPP on locate.
- Timing comes from the render task's sample clock, handed to a sender task (never CAMD calls in the render path).
- Compensate for the audio output latency (buffer size), so the slave's audio lines up with ours.
- **Beyond E1:** ReBirth's receiver never transmits (manual p. 127). Clock out is an extension, so classify it: Classic extension vs Power Mode (W3). This is an owner decision. It must not change the Classic DSP path.

**R3. MMC in and out.** Play, Stop, Locate (sysex). The parser exists (`midi.h`); wire it to the transport, and add sending.

**R4. Settings and indicators:**
- MIDI in and out port selection (CAMD clusters or links);
- sync source (Internal / MIDI clock / Link later);
- clock out on/off;
- latency offset.
- Keep the MIDI and Sync LEDs (p. 144–145), and add a clock-out LED.
- Stored in `ENVARC:` or the song (owner decision).

### P2: notes and data both ways

**R5. Note input per device.** An external sequencer (a DAW's MIDI track) plays the 303/808/909 voices directly: one channel per device, with notes mapped to the 303 pitch/accent/slide and to the 808/909 instruments. This is separate from, and must not collide with, the G7 remote-control channel (the manual's one-channel remote stays as is).
- Events enter the scheduler as `NOTE_ON`/`NOTE_OFF` with the locked same-sample ordering (spec §6).
- The spec's "MIDI flood cap" (P-19, OPEN) applies.

**R6. Note and CC output per device.** Patterns play external synths or DAW instruments: 303 steps become notes with velocity for accent, and slides are legato/overlapping; 808/909 instruments become a note map. Knob moves and automation go out as CC, using the same controller numbers as the G7 map (Appendix C), so a DAW can record ReIncarnation's automation.

**R7. Standard MIDI File export and import (SMF type 1).**
- Export: patterns and song (one track per device), with automation as CC. Accent, slide and flam are mapped to documented note/velocity/CC conventions.
- Import: drum/303 patterns from SMF, quantized to 16ths, with a report of anything that can't be represented.
- This is a file-format-adjacent choice: the mapping is an owner review item.

### P3: audio exchange

**R8. Stem export.** The offline renderer writes one WAV per mixer strip, plus FX returns and the master, sample-aligned and of equal length, with the tempo and a loop-exact length in the filename or metadata. This lets you drop a ReIncarnation song into Live as stems. Multichannel output is W3 in the spec; stems are the offline form of it.
- Options: 44.1 or 48 kHz, 16/24-bit and 32-bit float.

**R9. Loop-exact renders:** render N bars from a chosen bar, with an optional tail, so exported loops warp cleanly in a DAW.

### P4: after the Windows/macOS port (portability plan backends)

**R10. Ableton Link.**
- **Features:** network tempo, beat phase and start/stop sync with Live and every other Link app, with no cables.
- **Needs:**
  - UDP multicast networking. On AROS that is AROSTCP, which is fragile in our lanes (Vulkan4AROS wiki: AROSTCP networking records). So do Windows/macOS first, and AROS only if AROSTCP multicast proves solid.
  - **Licence:** the Link SDK is GPLv2+ or a proprietary licence from Ableton. A clean-room implementation of the published protocol is the alternative. This is an owner decision.

**R11. Plugin build: VST3 (Windows/macOS), AU (macOS), optionally CLAP.** ReIncarnation runs inside Live. This is the deepest integration.
- The host drives transport and tempo.
- Parameters = the stable control IDs (spec §13), so host automation maps 1:1.
- State = RBNG song data in the plugin chunk.
- Audio via host buffers of any size, adapted to the 64-frame engine block without breaking "one renderer".
- Optional multi-out: one stereo pair per mixer strip.
- Needs the portable core (`app/core/`, PAL) and a host-side GUI backend, which the display-list GUI already supports (`gui/draw/` + host rasterizer).
- **Licences:** VST3 SDK (dual GPLv3 / proprietary), AU (Apple), CLAP (MIT). This is an owner decision.

**R12. MIDI over the network (optional):** RTP-MIDI / AppleMIDI, so an AROS machine and a DAW PC can exchange clock and notes over Ethernet without MIDI hardware. It depends on the AROSTCP quality, like R10.

### Not possible

**ReWire** (the original ReBirth's way to run inside a DAW) is **discontinued**: Reason Studios ended it, and Ableton removed it in Live 11. Do not plan for it.

## Constraints (apply to every item)

- **Realtime (spec §4, LOCKED):** no MIDI, network or DOS calls in the render path. All I/O goes through bridge or sender tasks, with SPSC queues and snapshots.
- **Classic compatibility:** W2 Classic behaviour (E1) is unchanged by default. Extensions (clock out, note out, Link) are opt-in, and their classification (Classic extension vs Power Mode W3) is an owner decision.
- **Robustness:** survive a MIDI flood (P-19 cap, oldest dropped and counted), USB-MIDI hot-unplug (TC-2.8.4) and a clock dropout, with no stall and no hang.
- **Determinism:** a followed or externally driven take must either be reproducible offline or be explicitly marked live-only.
- **Clean-room (spec §1):** no third-party SDK code without the owner's licence decision.
- **Extensible rack:** per-device channels and note maps are keyed by device instance, never by a fixed four; new devices (ESX-1, Leviasynth) join the same tables.

## How to test (when designed)

- **Host, pure:**
  - a clock-follower model fed jittered 24-ppqn streams (the tempo error and long-term drift are bounded, and it locks within N beats);
  - SPP/Start/Continue/Stop state machines;
  - an SMF writer/reader round-trip;
  - a note-map table per device;
  - stem sums equal the master mix (bit-exact before master processing, or documented).
- **riqemu1:** real `camd.library` loopback (a `MIDISEND` sender on a cluster → the RIAPP receiver, as the G7 proof did), and RIAPP clock out → a CAMD receiver probe that logs `0xF8` intervals.
- **Dell:** USB-MIDI interface (Poseidon `camdusbmidi.class`) to a PC running Live. The owner confirms by ear that the two stay in time for 5 minutes, both as master and as slave.
- **Post-port:** Link sessions with Live on the same LAN; plugin validation (VST3 validator, auval).

## Owner decisions (open)

1. Clock out, note out and MMC out: Classic extension, or Power Mode (W3)?
2. A followed performance: record the tempo/transport history for offline reproduction, or keep it live-only?
3. Where sync settings live: `ENVARC:` (per machine) or the song.
4. The SMF mapping for accent/slide/flam and the drum note map (a GM drum map, or ReBirth's Appendix C notes).
5. Ableton Link: licence the SDK (GPLv2+ or proprietary), or clean-room the protocol?
6. Plugin formats (VST3 / AU / CLAP) and their licences.
7. Priority of P2–P4 relative to the GUI round 3 and the Levi/ESX-1 devices.
