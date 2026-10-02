# MIDI interop R1: the clock-in follower core, the realtime wire parser, and the sync-source state

- Source: ReIncarnation repo commits `47debed`, `50dc90a`, `622907d` (OpenCode lane, 2026-09-29, interop requirement R1)
- Collected: 2026-10-02
- Published: 2026-10-02
- Spec: `docs/superpowers/specs/2026-09-29-interop-requirement.md` (P1 sync over MIDI; R1–R12; the realtime constraints; the testing plan; seven open owner decisions)
- Prior: [2026-09-29-gui-round3-s1-s3-and-interop-requirement.md](2026-09-29-gui-round3-s1-s3-and-interop-requirement.md) (the requirement, and the "MIDI clock only drives the Sync LED" baseline this work sits behind), [2026-09-26-gui-remote-midi-g7.md](2026-09-26-gui-remote-midi-g7.md) (the one-channel G7 receiver that still owns channel voice bytes)
- Code: `midi_io/midi_follow.{c,h}`; tests `tests/unit/t125_midi_follow.c`, `t126_midi_rt.c`, `t127_midi_sync.c`; audit lines 226-228 of `scripts/ri_audit.sh`

## Why this needed its own core instead of using `midi.h`

The spec's "where we are today" already listed what existed and what was unwired:

> `midi_io/midi.h` has a simulated 24-ppqn clock counter with drift measurement (`midi_clock_*`) and an MMC transport parser. Neither is wired to the transport.

`midi_clock_init` / `midi_clock_advance` / `midi_clock_drift` and `midi_mmc_cmd` are still there (verified in `midi_io/midi.h`), and they are still unwired. They were built for the **Sync LED**: a simulated clock that advances on its own and a drift number to compare against. That is a display concern, and a display concern cannot express what a follower has to do — reject a wild tick instead of averaging it, hold a locked estimate through jitter, latch one STOP when the stream dies rather than emitting a STOP per poll, and tell the caller *what* to do instead of doing it.

So R1 adds `midi_io/midi_follow.{c,h}` beside the old clock rather than replacing it: a pure follower core with its own laws, and **no transport coupling at all**.

## The four decisions that shaped it

1. **Pure, integer, caller-owned.** No allocation, no IO, no DOS, no MIDI calls — the realtime constraint (spec §4 LOCKED) means this can only ever be fed by a bridge task. Interval math is integer ("deterministic twins"); float appears only at the BPM readout.
2. **Intents, not actions.** The core returns a `struct RIFollowIntent` (`kind` plus `seek_tick`) and the caller applies it via `ri_tr_*`. Nothing in the follower knows what a transport is. This is what kept the slice to one file and one test per behaviour, and it is why the wiring could be deferred without leaving a mess.
3. **Own laws, not "average everything".** The estimate is a rolling mean over `RI_FOLLOW_LOCK_N = 24` accepted intervals; an interval deviating by more than a quarter of the running mean is rejected outright and leaves the estimate standing.
4. **Dropout is a state, not an event.** `RI_FOLLOW_MISSED_TICKS = 96` missed intervals (or `RI_FOLLOW_COLD_US = 2000000` µs before the first tick ever arrives) produce exactly **one** latched `RI_FOLLOW_STOP`. Any further traffic clears the latch. Without the latch, a dead cable emits a STOP on every poll.

## What is in the box (three commits, three tests)

| Commit | Test | Surface |
|---|---|---|
| `47debed` | `t125_midi_follow` (30 asserts) | `midi_follow_init/tick/start/continue/stop/spp/poll`, `midi_follow_bpm`, `midi_follow_locked` |
| `50dc90a` | `t126_midi_rt` (19 asserts) | `midi_follow_rt` — the byte-level wire parser |
| `622907d` | `t127_midi_sync` (26 asserts) | `midi_sync_*` — Internal/MIDI source, latch, dropout |

**Intent kinds:** `RI_FOLLOW_NONE` 0, `PLAY_START` 1, `CONTINUE` 2, `STOP` 3, `SEEK` 4. Every entry point fails closed with `2` on a NULL pointer or a bad source, and `0` otherwise — "0 ok (intent may be NONE)" is the contract, so a caller must switch on `kind`, not on the return.

**SPP is a scale decision, not a detail.** `midi_follow_spp` multiplies by 24: "SPP beats are MIDI beats (16ths)" and the engine PPQ is 96, so SPP beat *b* becomes `b * 24` ticks. `t126` pins `0x00, 0x01` (128 beats) → 3072 ticks.

**The wire parser's rules** (`midi_follow_rt`), each one a thing a naive single-byte switch gets wrong:

- `0xF8` clock tick, `0xFA` start, `0xFB` continue, `0xFC` stop;
- `0xFE` active sense and `0xF6` tune request are consumed and produce nothing;
- `0xF2` *arms* SPP and emits nothing; the two following data bytes complete it, so a pair may be split across calls;
- **realtime lands inside a pending SPP pair** (MIDI law) — a tick between the two data bytes does not break the pair, which `t126` pins with an interleaved-tick case;
- **any other status byte aborts a pending SPP** (an unterminated pair must not swallow the next message's data byte);
- channel voice bytes are ignored and return NONE — notes and CC belong to the G7 path, so there is one owner per byte class in the whole program.

**Sync source** (`midi_sync_*`), the settings-side half: `RI_SYNC_INTERNAL` / `RI_SYNC_MIDI`; a locked MIDI clock latches the measured tempo and reports "following"; `midi_sync_knob_locked` is the read-only law that tells the UI the tempo knob must display the measured value instead of taking edits; a dropout ends following but **freezes** the held tempo so the display does not blank; reselecting MIDI starts idle rather than resuming a stale lock.

## Two discipline notes worth more than the code

**The `-Wunused-parameter` stale-object trap, twice.** Mutating the tick case to `return 0` without using `now_us` breaks the build under `-Werror`; the host build then leaves the *previous* `midi_follow.o` in place and the test links against it and passes. A mutant that "survives" that way is a false negative in the strongest sense — the proof of a kill is worthless. The discipline that catches it: hash `/tmp/ri/build/midi_follow.o` before and after every mutant rebuild and require the hash to change. Both `50dc90a` and `622907d` record hitting this.

**A vacuous assert, caught by a mutant.** In `t127` the unlocked-path check asserted `midi_sync_tempo(&s) == 0.0f` — and the default state is *also* 0.0, so the assert could not fail. The "latch while unlocked" mutant therefore passed. Weakening the value to a non-default `999.0f` made the assert able to fail and the mutant die. **An assert on a value that equals its default proves nothing**; the fix is to set the value under test to something distinguishable first.

The companion scar from the same session: a stray `git checkout HEAD -- midi_io/midi_follow.c` reverted the implementation mid-mutant-cycle, and the `.good` backup taken before the mutants was what rescued it. Backups before mutants are now explicit procedure.

## What is deliberately not here

- **No CAMD wiring and no transport application.** The intents exist; nothing consumes them yet. `ri_tr_play/stop/seek` are the intended application point.
- **No clock out, no MMC out, no note/CC out.** Whether those are Classic extensions or Power Mode is still open owner decision 1 in the spec.
- **No focus-5 / Levi keyboard-focus slot.** That sits in the sibling's keyboard/panel territory (`gui/keymap.c`), so it is a spec-and-hand-over, not a trespass.
- **No latency offset and no phase lock.** The spec's P1 asks for both; a follower core can produce the intent, but the *offset* is a property of the sender's clock and the audio device, so it belongs with the wiring.
- **The old `midi_clock_*` and `midi_mmc_cmd` are untouched** and still unwired. Two clock models now coexist, one for the LED and one for following; only the second can drive tempo.

## Status relative to the requirement

Against the spec's own inventory, R1 closes the *"tempo does not follow MIDI clock"* gap at the **core** level and leaves every **wiring** gap open. The application-layer claim in the wiki's requirement record — MIDI clock only drives the Sync LED — is still true of the shipped app and is left standing.

The spec's testing plan puts the proof on riqemu1 with real `camd.library` loopback (a `MIDISEND` sender on a cluster → the RIAPP receiver, as the G7 proof did), and that is the next slice: wire the bridge, apply the intents, and prove a real `0xF8` stream moves the transport. The Dell/USB-MIDI and post-port Link tests stay with the owner.

## See Also

- [GUI round 3: S1–S3 landed by OpenCode; interoperability requirement (Ableton)](2026-09-29-gui-round3-s1-s3-and-interop-requirement.md)
- [G7 Remote MIDI Control, Standard Mapping over CAMD](2026-09-26-gui-remote-midi-g7.md)
- [Songs, playlists and the Zombie Nation demo (owner 2026-09-30)](2026-09-30-songs-playlists-zombie-nation.md) (the same "pure core, caller applies" seam at the song level)