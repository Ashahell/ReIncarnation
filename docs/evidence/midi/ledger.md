# MIDI interop ledger (R1+, owner decisions pending)

Rows here are E0 defaults picked for E1-silent points (owner preference
`owner-pragmatic-e0-defaults`): one recommended default each, named
constant in code, pinned by a host test, revisit note attached. Shipped
behavior is opt-in and off by default wherever a Classic/power-mode
decision is pending.

## M1: follower spec conformance (2026-10-08)

- **SPP while running: count and ignore** (`midi_follow.c`, counter
  `struct RIFollow.spp_ignored`, law pinned in t125/t126). The MIDI 1.0
  spec intends SPP to arrive only while stopped; Live in Song mode
  follows that. If an SPP arrives mid-take the follower emits no intent
  and bumps the counter instead of cueing a locate under a running
  transport. Revisit (owner decision D2/D4): if SMF mapping (R7) or
  recorded-take history ever needs mid-take SPP, define the cue point
  (next bar? next beat?) and the counter becomes the audit trail.
- **Start/Continue fire on the next F8** (armed state `play_arm`,
  t125/t126). One clock (~20 ms at 120 BPM) later than the status byte,
  per MIDI 1.0 "Syncing Sequence Playback". Not an E0 choice (spec
  mandates it); recorded so a future "start on FA" regression reads as
  a spec violation, not a judgment call.
- **Dropout ends a pending arm** (`midi_follow_poll` clears `play_arm`;
  a new take needs a new Start). E0: the spec is silent on arms across
  silence. Revisit if a master is found that sends FA once and clocks
  much later.

## M2: bridge + live-app MIDI in (2026-10-09)

- **P-19 flood cap: 256 channel messages + 64 intents** (`RI_MBR_CH_CAP`,
  `RI_MBR_IN_CAP`, t181). Oldest dropped and counted, per the spec's
  stated policy; no per-key coalescing (a stream, not a state).
  Revisit with P-19 if a flood profile ever needs shaping instead.
- **Input cluster default `riapp`** (t181-pinned string). A distinct
  cluster from `ri.remote` so a concurrent RISECT session keeps its
  lane. Revisit (owner decision 3): any other default name.
- **Remote channel default 1** (ReBirth manual's one channel; G7 keeps
  it). **Sync source default Internal** (no behavior change until the
  owner opts into MIDI clock in M3). **Latency offset default 0 ms**
  (M3 measures and corrects; the knob exists now so songs never carry
  it). **Leviasynth channel default 2** (M4 map; G7 stays on 1).
  **Clock/MMC out default off** (M5, pending owner decision 1).
  All six live in `ENVARC:` (`RIAPP_MIDI_IN/CH/SYNC/LEVI/CLKOUT/LATMS`),
  never in the song.

## M3: transport follows clock (2026-10-09)

- **Lock deadline 2 beats** (`RI_MTRANS_LOCK_BEATS`, t182). The
  follower needs just over one beat of steady clock; two is the
  announced bound with margin. Revisit if a master ramps tempo
  through the lock window and the deadline starts to bite.
- **Phase servo 0.01 BPM/tick, max 2.0 BPM** (`RI_MTRANS_TRIM_*`,
  t182). A converging estimate alone leaves a standing offset (the
  integral of its transient); the bounded trim drives it to zero
  instead. A one-bar local jump recovers in ~2 min; normal-op trims
  stay micro. Revisit with measured master drift (Ableton Link
  experience: masters wander; if the trim rails often, widen it).
- **Followed takes are live-only** (no tempo/transport history is
  recorded; the app logs `follow=…bpm live-only @…` on lock). E0
  pending owner decision D2; the counter-free design means there is
  nothing to migrate if recording is ever approved — only to add.

## M3b: lane-found corrections (2026-10-09)

- **SPP units are sixteenths, not ticks** (`midi_follow_spp` emits
  `beats * 4`; `midi_trans_apply` converts with the panel's PPQ).
  midi_io owns no PPQ, so the intent speaks the wire's own unit and
  the transport layer owns the engine one — one authority each.
  The first law (`beats * ppq/4`) was a quarter-distance bug; found on
  the lane, not by the host tests, because a synthetic test fed the
  same wrong unit in and compared the same wrong unit out.
- **Locate clamps to `song_bars`** (the panel's own bound, as its bar
  seeks do). Default 999 (E1 "playback continues to 999"): a locate
  past the arrangement stops at the end of the written song, which is
  the engine's law, not the applier's.
- **Servo reference = the engine's tick cursor.** The panel position is
  a display projection: per-frame, rebased at the playback edge, and
  (before M3b) at a fixed 120 BPM. Any of those make it the wrong
  number to close a phase loop against.
- **Trim rail is a bug signal, not a setting.** During the Dell
  loopback the ±2 BPM bound sat pinned for the whole take; that is
  what exposed (1) and (2). If it rails on a real master again, read
  the domains before touching the constants.

## M3c: the drift trace and what it found (2026-10-09)

- **Drift trace cadence: every 32 drain blocks while locked**, plus
  every lock edge (E0). One line carries bpm, engine tick, expectation,
  phase error in ticks, app-counted F8 / wire-side F8, the three drop
  counters and the wall clock. Read the ev-log's SECOND column knowing
  what it is: **audio buffer count, not a clock** (it misled a whole
  round of drift reasoning until the trace carried CurrentTime).
- **The expectation counts CLOCKS, not locked windows.** Under script
  jitter the follower relocks constantly; M3b advanced `expected` only
  while locked, so every relock swallowed the clocks already read off
  the queue — `f8=2196/2196` on the wire against half that in the
  expectation, and a phase error growing ~+40 s over 88 s. Fixed
  (t182 "relock clocks count").
- **A locate is render-owned** (`ri_live_locate` + `locate_gen`). The
  render recomputes `cursor_ticks` from `sample_cursor` every block, so
  a store from the app is discarded while playing; and pressing Play on
  an already-playing panel is a no-op, so the Start intent had nothing
  left to do. Before: a Start on the autoplaying demo left the engine
  at tick 12095 (≈2 min in). After: tick 0.
- **riqemu1 cannot prove drift** (no real-time audio pacing there: the
  engine advances in bursts while the clock runs on), so drift is a
  Dell-lane measurement only. riqemu1 still proves intents, locate,
  lock and display.

## M3d: a tempo change appends a map segment (2026-10-09)

- **`RI_LIVE_MAX_SEGS = 64`** (E0). A tempo change appends a segment at
  the current tick instead of rewriting the single segment. Sized in
  seconds, not events: MIDI follow rewrites the tempo several times a
  second, so 64 covers ~3.5 s of following. When it fills, the map
  collapses to the current rate and re-anchors `sample_cursor` to the
  same tick — that stalls nothing (it only rewrites history nothing
  reads back). Revisit if a take ever legitimately changes tempo more
  than 64 times between collapses; the collapse is the safety valve, not
  a cliff.
- **Why the old law was wrong**: the map was anchored at tick 0, so a
  tempo *drop* mapped the current tick to a sample position AHEAD of
  where the audio actually was; the render's forward-only tick walk then
  froze until the audio caught up. Not MIDI-specific — the tempo knob
  mid-song did it too. Lane cost before the fix: the engine frozen on 29
  of 143 samples (20%) of a 5-minute take; after: 0 of 36.

## M3e: the owner's ear proof failed, and why (2026-10-09)

**M3 does not pass.** The owner ran the 5-minute take and reported
(`m3-owner-fail.log`): a hanging tone, the Mixer/Master Comp button
doing nothing, and playback speed collapsing while they worked the
Levi.

- **Hanging tone + speed collapse: mine (M3d's collapse).** When the
  tempo map filled, the collapse re-anchored `sample_cursor` onto the
  NEW mapping. That skips or repeats music (a skipped note-off hangs a
  voice; a repeated/skipped region moves the song position fast). Their
  run is exactly the stress case: more tempo changes than segments.
  Host RED: `tempo change 63 moved the audio position (299008 ->
  294098)`. Law now: **a tempo change only appends; the audio position
  is physical** (t95). Their run also shows the tell — 6126 of 7500
  clocks arrived and the take ended 12500 ticks ahead.
- **NOT killed by the host test, said out loud:** anchoring the
  collapsed segment at tick 0 instead of at the cursor (mutant M2) also
  leaves the audio alone, and t95 does not distinguish it — it rewrites
  the map's history, which nothing reads back on a live take. The Dell
  lane is where that would show, not the host.
- **Still open, not mine:** the Mixer/Master Comp switch. It is an
  insert-assign switch (`RI_BIND_INSERT`, engine honours the route
  owner) and the COMP *tab* On/Off is an unwired lamp — so either the
  assignment never reaches the engine on the live path, or the owner
  pressed the tab. Needs its own phase; do not fold it into M3.

## M3f: the tempo map, again (2026-10-09)

- **The collapse now flattens the past, it does not pretend it was never
  there.** The map is a list of segments all anchored at tick 0, so
  dropping history silently re-maps the PAST: M3e kept the first segment
  (the song's own 140), the map then claimed the audio was ~4 s behind
  where it was, and the render's forward-only walk answered by throwing
  the tick cursor **346/497/579 ticks in a single block** (t95). That is
  the owner's "playback speed suffers", and a skipped note-off is a
  hanging voice. The collapse now measures where the audio really is
  with the full list, replaces the past with one synthetic segment that
  maps [0, cursor] exactly onto that sample position, and runs the real
  rate from the cursor on.
- **Law pinned in t95:** a tempo change may move the tick cursor neither
  further nor less than the audio it just played (4..16 ticks per 4096
  samples at 60 bpm). A jump is the walk catching up on a map that lost
  the past; a freeze is the walk stalled behind one that invented it.
- **MIDICLOCK (app/midiclock.c, proof tool).** MIDISEND's per-message cost
  caps its clock at ~60 BPM, so every proof so far ran the 140 BPM song at
  less than half its own tempo — the owner's "slow, low tempo", and a fair
  failure of the proof, not of the app. MIDICLOCK streams F8 itself from
  an EClock schedule and prints the tempo it achieved: **139.88 BPM when
  asked for 140** on the Dell (ABIv11 build via `ri_build_v11.sh … midiclock`).

## M3 closed by owner decision (2026-10-09): PARKED

- **Owner: M3 is parked, not passed.** What is proven: following at
  ~60 BPM on the Dell (7500/7500 clocks, no stalls, no xruns, phase error
  converging to -39 ticks, owner's ear "seems fine"), plus the lock law at
  the song's own 140 BPM on the host (t182, 17.857 ms intervals).
- **What is not proven: following at 140 BPM on real hardware.** This
  guest's CAMD delivers clocks in 10 ms system-tick batches, which is more
  jitter than the R1 lock law accepts; MIDICLOCK delivers a clean 140 BPM
  but the app cannot see it through that batching. Closing this needs a
  USB-MIDI interface and a real master (the actual use case), or a
  follower that tolerates batched arrivals (M1 estimator work).
- **Two owner ear-proof failures drove real fixes** and are recorded
  above (M3e/M3f: the tempo-map collapse). The Comp switch is the
  owner's own call: "this was fine", so the halved auto make-up from
  4167e32 stands and no sound default was touched.
- Nothing is pushed. Local: b1fa941, 1fa5675, f7f28c1, fd621e1, acc102f,
  7b3739f, cf60bf0.

## M4: the Leviasynth on its own channel (2026-10-09)

- **E0 device table** (midi_io/midi_chan.h): instance 0 = the ReBirth
  remote on channel 1, instance 1 = the Leviasynth on channel 2
  (`RIAPP_MIDI_LEVI_CH`, default 2). Rack rule: a channel belongs to one
  device and a device to one channel; rebinding a **taken** channel is
  REFUSED (RI_MCHAN_TAKEN), never stolen — the remote keeps working with
  an instrument plugged in. Revisit if a third device appears: RI_MCHAN_MAX
  is 4 and the table is role-indexed, so a new role is one #define plus a
  default row.
- **E0 unmapped-by-default CCs on the Leviasynth channel.** The two the
  plan's no-collision law makes non-negotiable: **CC 7 (master volume)**
  moves nothing, because the ReBirth master fader must not be reachable
  from the Leviasynth's channel; and the manual's **reserved** CCs (6, 38,
  98-101, 115-122, 124-127) are refused rather than guessed.
- **E1 citations.** The CC map cites the Leviasynth Keyboard Owner's
  Manual v1.2.1 "MIDI CC Charts" pp. 168-169 per row (manual text stays
  out of the repo; midi.guide's CC BY-SA table is not used). Rows are
  mapped only where ONE 7-bit value can address our parameter; the gaps
  carry their reason in `midi_levi_cc_why`, e.g. oscillator pitch needs
  mode + coarse + fine (one CC cannot address them), and LFO level is a
  matrix-slot amount in our model.
- **The live note/performance path rides the control plane's spare byte**
  (`ri_ctl_send2`, M4c/M4d): one 7-bit value cannot carry a note number
  or a 14-bit bend, so `RIControlMsg.flags` — which existed and was
  always zero — now travels beside the value and lands in `RIEvent.flags`
  bits 8-15. Layout: NOTE (val = velocity, hi = note | on<<7), BEND (val =
  low 7, hi = high 7, 14-bit with centre 8192), PRESS, PAT (hi = note),
  WHEEL. E0: a live note lands at the **block boundary** (256 frames =
  5.3 ms) — it has no scheduled sample, and a deterministic one-buffer
  quantum is honest where a fractional-sample claim would not be.
  Revisit only if a player complains about 5 ms of latency; the fix then
  is a per-event sample stamp, not a smaller quantum.
- **Bend scaling**: the panel's Bend Range (`RI_CTL_LEVI_VBENDRNG`,
  semitones 0..24) scales the 14-bit deflection, full scale at +-8191.
- **Not routed, on the record**: sustain pedal (CC 64) and all-notes-off
  (CC 123) reach nothing — the engine has no sustain gate and no panic.
  The plan says list rather than fake them, so they are listed.
- **Defect found after M4d was committed, and a coverage gap it
  exposes.** RIAPP's `midi_drain()` pushed PARAM and NOTE to the control
  plane but only *logged* the PERF branch — so a bend, a mod wheel or an
  aftertouch arriving on the Leviasynth channel was counted and traced,
  and never reached the engine. t184 did not catch it because t184 drives
  its own push helper; `midi_drain()` itself is AROS-app code with no
  host test. Two honest consequences, recorded rather than papered over:
  the branch is fixed (all three kinds now push, PERF through
  `ri_ctl_send2` like NOTE), and the lane proof below is what actually
  covers `midi_drain()` until a seam for it exists. Do not read M4's
  green host tests as "the app pushes every kind".

## Host reboot 2026-10-09: recovery, and two lane traps it exposed

- **The repo and the record survived; `/tmp` did not.** Every host build
  object, the `/tmp/opencode` binaries and the Dell's staged files were
  gone. The rebuilt objects are **bit-identical** to the pre-reboot
  hashes (`midi_levi.o d63daa5c`, `engine.o 4448eb09`,
  `ctlplane.o ba282f4d`) and the audit is `0/0 PASS` from a cold tree,
  which is a reproducibility check nobody had asked for.
- **The Dell agent reconnected by itself** (agent e6320, session 1) and
  `RAM:` in fact survived — the earlier "Dell RAM: was wiped" reading was
  wrong, and the files that looked wiped were the ones my own broken
  shell redirects had failed to write (below).
- **TRAP 1 — a build-script target that falls through builds the wrong
  thing under the name you asked for.** `ri_build_v11.sh` had a
  `midiclock` tool target and no `midisend` one, and an unrecognised
  third argument silently fell through to the RIAPP build:
  `ri_build_v11.sh . /tmp/opencode/MIDISEND.v11 midisend` produced a
  **1129544-byte RIAPP named MIDISEND.v11**. Staging that would have run
  the application twice in one proof. Fixed: a real `midisend` target,
  and an unknown tool now **refuses with exit 2** and writes nothing.
  The v11 SDK ships no `libstdcio`/`libposixc`, so the link is
  `-lamiga -ldos -lexec -lautoinit -lcamd`; `Open`/`Close`/`FGets` are
  dos.library. Verified on the Dell, not just at link time: SELFTEST
  through real camd.library reported `connected 1,1`, a readable cluster
  name (`6D 34 73 74`), and `got 2`.
- **TRAP 2 — the lane's `--exec` is not a shell, and a redirect inside it
  fails silently.** `version > RAM:ver.txt 2>&1` returns **rc=0** while
  writing nothing: the command is tokenised and the `>` becomes an
  argument. An earlier staging check in this same session read that rc=0
  as "the file was written" and it was not. Read `--exec` stdout (the
  CLI prints it) and never trust a redirect; for multi-step work use
  `--run-script`. **A PASS from an exec that redirected proves only that
  the command ran.**

## M5a: the clock-out schedule (2026-10-09)

- **E0, and the reason the module is shaped the way it is: the tick count
  at an absolute sample position is a pure function of that position, not
  an accumulator.** `ticks(n) = base + floor(((n - anchor) + lead) * ppq *
  bpm_milli / (sr * 60000))`. Nothing accumulates, so there is no residual
  to carry and no second rounding to disagree with. 48 kHz at 140 BPM is
  857.14 samples per tick — exactly the case an accumulator gets wrong —
  and t185 pins exactly **33600 ticks in ten minutes** with every interval
  857 or 858 and both values occurring (a schedule that only ever emitted
  857 would drift a tick per second; one that only ever emitted 858 would
  run fast).
- **E0 lead direction**: `lead` is the audio output latency in samples and
  the clock **leads** the audio by it. Lagging would deliver the tick after
  the sound it describes has left the device. The tick is due *at*
  `anchor + k*T - lead`, not one sample later.
- **E0 a tempo change re-anchors at the change position** and keeps the
  tick count already reached, so emitted ticks keep their sample positions.
  This is M3f's law applied to the output side **before** it bit: dividing
  by the new rate without re-anchoring re-times the past, which is what
  made the M3 owner's ear report that playback speed suffers. Mutant W is
  that exact bug.
- **Tempo is milli-BPM** so the render path does no float; `midi_io` speaks
  the wire's own 24 ppqn and never the app's 96 (M3b's "one authority
  each").
- **E0 fail-closed, including a zero PPQ.** A zero sample rate, tempo or
  PPQ owes nothing rather than dividing by zero, and `init` deliberately
  does **not** default a zero PPQ to 24: a silently-correct answer would
  hide the caller getting the wire constant wrong.
