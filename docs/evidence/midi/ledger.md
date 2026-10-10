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

## riqemu1 restored after the host reboot (2026-10-09)

The private ABIv1 lane had **no unit at all** — it was started by hand
every time, which is precisely the failure the 2026-09-29 recovery record
warns about ("a second reboot would have silently dropped the proof
lane"). Both halves are now user units and **enabled**, so the next cold
reboot brings the lane back without a hand:

- `spike-riqemu1.service` — port 9295, bulk 9195, spool
  `/tmp/spike_spool_priv`, pairs copied from the durable master
  `~/.config/spike/pairs.anon-riqemu1.json` (the guest agent dials
  `10.0.2.2:9295` with no name, so it lands in the anonymous slot).
- `riqemu1-vm.service` — runs `~/Work/vms/start_riqemu1.sh` unchanged.
  Its hard-won args stay in the launcher and are not to be edited from the
  unit: `-vga vmware` (the VMWare monitor driver unlocks wide modes; std
  VGA has no matching driver), rtl8139 (e1000 wedges on bursts under
  user-net), AC97 only (sb128 and hdaudio fault in DriverInit), monitor
  4477.

**Verified, not assumed:**

- Agent back on its own: `anon`, session 1, ping 3 ms.
- **1280x1024** (`screendump` PPM header `P6 1280 1024`; agent
  `ui-windows` reports a 1280x1024 screen) — the required resolution.
- `Kickstart 51.51, Workbench 40.0`; the agent window is on a clean desktop
  (`docs/evidence/lane/2026-10-09-riqemu1-back-1280x1024.png`).
- **Bulk dial-back works here**: the 1129544 B RIAPP transferred in 15401 ms.
  That is the empirical answer to "is 9195 actually up?" — the bulk port is
  advertised to the agent and opens on demand, so its absence from
  `ss -ltn` at rest is expected, not the "configured flag, no listener"
  failure the Dell lane hit on 2026-10-05.

`/tmp/opencode/hmp.py` was rewritten rather than recovered: it now
**refuses** `quit`, `system_reset`, `system_powerdown`, `stop` and `cont`
by list, because "never send quit to the monitor" was a comment and
comments are what a reboot takes away.

## M5b: the outbound producer (2026-10-09)

- **E0 off by default, and "off" means silent on the wire.** `midi_out_init`
  leaves the producer disabled and a disabled producer queues **no byte at
  all** — not a transport edge, not a clock. A receiver is not obliged to
  ignore anything, so a feature that is merely "not acted on locally" is
  not off. `RI_MIDI_SET_CLK_OUT` / `RIAPP_MIDI_CLKOUT` carries it.
- **E0 SPP is legal while stopped only**, counted in `spp_ignored` while
  running — deliberately the SAME law the follower has inbound (M1). Two
  ends that disagree about when SPP is legal is a take waiting to go
  wrong. A locate above 16383 sixteenths is **refused, never wrapped**:
  a wrapped SPP seeks the master somewhere nobody asked for.
- **E0 the ring drops oldest and counts**, P-19 style, because a clock byte
  that arrives late is worse than one that never arrives. A 3-byte SPP is
  **all-or-nothing** — a half-sent SPP is a wire-level lie, not a dropped
  packet.
- **The ring's indices are monotonic counters, not masked indices.** Two
  earlier forms were wrong and t186 found both. With masked indices
  `head - tail` means two different things (empty and full are the same
  value), so a full ring **silently overwrote** instead of dropping and
  `dropped` stayed 0 while the oldest bytes were eaten; and subtracting
  masked indices without masking the result reads **4294967295** the
  moment the reader overtakes the writer. Monotonic counters cost one
  reserved slot (the ring holds `CAP-1`) and make occupancy exactly
  `head - tail`.
- **E0 the clock emit is CUMULATIVE, not the delta between consecutive
  render calls.** A skipped or coalesced call then emits everything it
  owed instead of quietly losing that many clock bytes. A clock that
  silently loses bytes is a take that silently drifts.
- **The stale-object trap, four times in one session.** `ri_build_host.sh
  test` does not rebuild. It bit as (1) a mutant failing `-Werror` on an
  unused variable and the test reporting nothing at all; (2) the same
  shape again for a second mutant; and (3) **a "clean" verification run
  that was actually still linked against a mutant's object**, which
  produced three confusing failures against correct code before the hash
  was checked. `midi_out.o` clean is `46dc2654…`. Always `all` before
  `test`, and hash-verify every mutant rebuild.

## M5c: MMC in (2026-10-09)

- **E0 MMC emits the FOLLOWER's intents** (`RI_FOLLOW_PLAY_START`,
  `RI_FOLLOW_STOP`, `RI_FOLLOW_SEEK`), not a parallel MMC set. A master
  sending MIDI clock and a master sending MMC therefore drive one path
  rather than two, and there is one Locate law instead of two.
- **E0 LOCATE's position is 16-bit big-endian MIDI beats = SIXTEENTHS**,
  the same unit SPP carries. Mutant AB is the little-endian reading, and
  it is M3b's SPP bug exactly: a seek to the wrong bar, with nothing on
  the wire to say so.
- **E0 the accumulator RESYNCS.** An `F0` seen inside an `F0` restarts the
  frame rather than nesting (mutant AA): nesting lets one truncated frame
  swallow every later command. An over-long frame is dropped and the
  parser is usable again immediately after its `F7`.
- **E0 `overflow` counts FRAMES, not excess bytes.** A byte count scales
  with the sender's noise rather than with the damage; "how many frames
  were thrown away" is the number worth watching.
- **Ableton Live neither sends nor receives MMC** (interop spec §0.9), so
  an MMC proof against Live proves nothing about the feature. That is why
  this is host-pinned and lane-logged rather than ear-proved against a DAW.
- **Mutant AC is an EQUIVALENT survivor, recorded as such and not claimed
  as a kill.** Removing the `enabled` guard from `midi_mmc_feed` leaves the
  feature off, because `finish()` guards independently — the observable
  law (no intent while disabled) is pinned, and the second guard is
  belt-and-braces that no test can see from outside. Saying "all mutants
  killed" here would be a claim the evidence does not support.
- **AROS upstream checked 2026-10-09 (twice): nothing new.** camd's newest
  commits are still `cb8c4c5f3` and `28ec43a51` (2026-10-05), which we
  already carry in both ABI carriages; nothing in `workbench/devs/USB`
  since 2025-12-28; and zero commits in the whole repository since the
  previous check. The carriage is current.

## M5d: framing the outbound stream (2026-10-09)

- **Framing is the sender's job and is E0:** a byte with the high bit set
  is a status byte and starts a new message; data bytes accumulate behind
  it; a message is emitted **whole**, when it reaches three bytes or when
  the next status byte closes it. A budget caps the bytes pulled per call
  so the sender task cannot starve the render.
- **E0 the framing state belongs to the PRODUCER, not to a function
  static.** The first version kept it in a `static` inside the pump, which
  meant two devices would interleave half-built messages and state would
  survive from one producer to the next with nothing to reset it. It now
  lives in `struct RIMidiOut`.
- **A bug worth keeping: `>= 0xF0` is NOT "realtime".** The first framing
  tested the high nibble, so **SPP (0xF2) was emitted as a one-byte
  message with no position at all** — the exact half-sent-SPP lie that
  t186 refuses to create on the producer side, arriving from the other
  direction. **Realtime is 0xF8..0xFF; 0xF0..0xF7 is system common and
  DOES take data bytes.** t188 pins both.
- **Mutant AD** (no framing at all) killed. **Mutant AE** (a new status
  byte discards the half-built message) **SURVIVED and is recorded as
  equivalent for this producer**: clock out emits only 1-byte realtime and
  3-byte SPP, both covered by the two emit paths above, so the path AE
  removes would only matter for a 1- or 2-byte system-common message —
  and nothing emits one yet. It is kept for F1/F3 when MMC out lands, and
  it is recorded as uncovered rather than claimed as killed.

## M5e1: the render feeds the producer (2026-10-09)

- **E0 the render's whole involvement is filling a ring.** `RILiveDriver`
  gains an optional, caller-owned `struct RIMidiOut *clk_out`; after
  `ri_live_render` it hands over `session->sample_cursor`, and transport
  commands forward Play as `midi_out_start` / Stop as `midi_out_stop`.
  **No send and no camd happens in the render path** — the confinement
  gate keeps AROS out of `live_driver.c` entirely, so that is enforced by
  the build rather than by discipline.
- **E0 NULL is the default state**, so every existing caller is unchanged:
  t189 renders 200 buffers with no producer attached and gets identical
  behaviour and 0 xruns.
- **E0 the clock is fed from the AUDIO clock, never a timer.** That is
  what makes "drift over ten minutes" a question with an answer: t189 pins
  that 51200 rendered samples owe exactly 51 ticks at 120 BPM, all `F8`,
  and 100 further blocks owe exactly 25 more. Mutant AF feeds half the
  audio position and gets 25 instead of 51.
- **A process failure worth recording.** This test passed on its FIRST
  run, because the driver was wired before the test was ever compiled —
  so no RED was observed. Rather than call it done, the hook was reverted
  and the test re-run to produce a real RED (4 laws failing: 0 ticks
  instead of 51, no FC, no reopen), then restored. **A test that was
  never seen red has not been shown to have teeth**, and the only way to
  know is to remove the thing it is testing.
- Mutants AF and AG killed, hash-verified (`live_driver.o` clean is
  `75011b0c…`).

## M5e2: the sender task and the proof receiver (2026-10-09)

- **E0 the sender is a task of its own, 1 ms tick.** The render fills the
  ring and stops; the bytes still have to reach camd from somewhere that
  is not the render, and "somewhere" matters — draining on the app's event
  loop bunches the clock into whatever rhythm the UI is running at, which
  is the jitter the follower's lock law on the other end rejects. The
  **schedule** is exact (t185), so the task carries bytes in small
  batches and nothing more; the lane proof measures what it produces.
- **E0 a sender that cannot start means clock out is OFF**, not a
  half-working clock: `ri_pal_midi_send_start` refuses, RIAPP disables the
  producer again and logs it.
- **A crashed proof tool taught the PAL a missing entry point.** camd's
  `CreateMidi`/`AddMidiLink` are **inline calls through `CamdBase`**, and
  `CamdBase` was only ever resolved inside `ri_pal_midi_open_in`. A tool
  that talks CAMD directly and never opens the backend therefore took an
  **illegal memory access on a NULL base** — MIDIRX died exactly there.
  `ri_pal_midi_init_lib()` now resolves the library without opening a port.
- **MIDIRX (`app/midirq.c`) is the missing half of the proof set.**
  MIDISEND plays a script *into* a cluster and MIDICLOCK plays a clock into
  one, but **nothing could listen**. It reports arrival INTERVALS of F8 —
  min, max, mean, spread, and per-slice counts — because a count only
  proves bytes arrived, and the question is what the wire does to an exact
  schedule. AROS-only; built by `ri_build_v11.sh … midirq`.
- **MIDIRX died TWICE on the Dell, both times the same shape.** First the
  NULL `CamdBase` (inline CAMD calls through a library nobody resolved).
  Then, after that was fixed, **a NULL `TimerBase`**: the tool declared
  `struct Device *TimerBase` — as `app/stepproof.c` does — but left it
  `NULL` and never assigned it from the opened timer device, so the very
  first `ReadEClock` was an illegal memory access. **Declaring the inline
  base is not the same as resolving it**, and the second time round I had
  literally copied the declaration without the assignment. That is the
  third time in this work a library/port base has been the crash, and the
  lesson generalises: *an inline call's base is a runtime dependency, and
  a declaration is not a resolution.*
- **The EClock arithmetic was wrong in three more places**, all of which
  would have made the tool lie rather than crash. AROS EClock is a 64-bit
  counter split hi:lo, so hi and lo are not separate clocks:
  `ev_hi + efreq * seconds` added microseconds to a 32-bit-of-64 counter
  and put the run's deadline about 51 billion years out (the tool could
  never have exited); `now.ev_hi * efreq` overflowed 32 bits; and
  `(now.ev_lo - prev.ev_lo)` underflowed the first time the low word
  wrapped. Everything now goes through one `ec_us()` helper in 64-bit
  microseconds, the same shape as `midi_camd.c`'s `eclock_us()`.
- **The Software Failure could not be dismissed, only rebooted past.**
  `ui-windows` on the Dell reports `Software Failure! 358,0 651x768
  [no-close]`: **no Cancel, no OK and no close gadget.** Escape and
  `--ui-click` do nothing to it, and while it is up the agent answers
  `--ping`, `--ui-capture` and `--ui-windows` normally but **every `--exec`
  returns rc=1 with no output** — the modal requester owns the screen and
  the shell behind it cannot run. So the lane's exec path is dead until a
  guest reboot, which is an owner action here (and `RAM:` is wiped by it,
  so everything is re-PUT afterwards). Capture kept at
  `docs/evidence/lane/2026-10-09-dell-midirq-software-failure.png`; its
  own text renders garbled, but the requester and its four buttons are
  legible and `ui-windows` names it exactly.
- **MIDIRX: what the lane actually established, after four crashes.**
  Every fault was captured from riqemu1, whose console is legible (the
  Dell's requester renders its own text as garbage, so nothing there is
  readable). In order:
  1. **NULL `CamdBase`** — inline CAMD calls through a library nothing
     resolved. Fixed with `ri_pal_midi_init_lib()`.
  2. **NULL `TimerBase`** — declared from `stepproof.c` without its
     assignment. *Declaring an inline base is not resolving it.*
  3. **A v11 binary on the ABIv1 lane.** riqemu1 is ABIv1 and
     `ri_build_v11.sh` builds ABIv11, so the tool died in the C runtime's
     startup — `Illegal address access ... Exec_49_FindTask` under
     `__startup_fromwb`, at a byte-identical PC across three attempts,
     before `main` ever ran. **The identical crash on two very different
     guests was the clue that made it an ABI mismatch rather than a
     fourth code defect.** Fixed by adding a `midirq` target to
     `ri_build_aros.sh`; with a v1 build the startup completes and the
     fault moves into `main`.
  4. **`Exec_77_SendIO` from `main`, with `RDX = 0x50`** — in a v1 exec
     library call RDX carries the library base, and `0x50` is not one.
     An unresolved inline/exec base again, this time for `OpenLibrary`.
     The pattern being copied from `app/midiclock.c` (`__TIMER_LIBBASE`,
     `__CAMD_LIBBASE`) is **v11-only**: midiclock has never been built for
     v1, so its workingness was never evidence about this lane.
- **WHAT MIDIRX HAS NOT DONE: completed a single run on either lane.** It
  has cost two owner reboots of the Dell and wedged both agents, and it has
  produced no measurement. The recommendation is to **retire MIDIRX** and
  take the intervals from **inside RIAPP instead**: the sender task
  already counts what it sent (`ri_pal_midi_sent()`), and the producer is
  portable and host-tested, so an interval histogram over the emitted
  ticks is ordinary TDD in code that has been green all along — and it
  needs no new AROS-only tool, hence no new unresolved library base.
- **LANE PROOF PENDING, and honestly so.** MIDIRX has still never completed
  a run on hardware, and the Dell agent stopped answering right after the
  crash — so the fixed binary is **staged but unrun**. The interval
  numbers do not exist. Nothing about the clock-out wire behaviour is
  claimed. And the proof belongs on the **Dell**, not riqemu1: riqemu1 has
  no real-time audio pacing, so F8 intervals there would be a lane
  artefact for exactly the reason M3c ruled it out for drift.

## M5f: MIDIRX retired, and the proof finally runs (2026-10-10)

- **The owner's diagnosis of MIDIRX was correct on every count, and mine
  was wrong.** Four faults, all in the poll loop: `CreateIORequest` builds
  a request with `io_Device = NULL` and `io_Command = 0` and it was never
  opened on `timer.device`, so `SendIO` dereferenced NULL; `GetMsg`
  followed by `WaitIO` removed the same message twice; the request was on
  `UNIT_ECLOCK` so its "2 ms" delay was 2000 EClock ticks (~55 us at
  36 MHz); and intervals were stamped when the poll drained the queue
  rather than when bytes arrived. **My "RDX = 0x50 is the library base"
  reading was also wrong: on this ABI the base is the last C argument, so
  for `SendIO(io)` it is in RSI.** The register was a symptom of the NULL
  device, not a missing library base. Four debugging rounds were spent on a
  self-inflicted NULL that was visible in the source.
- **THE DEFECT THAT MATTERS, and it is the design: MIDIRX timed its own
  polling.** At 140 BPM, ticks are ~17.9 ms apart and its poll was 2 ms
  (really 55 us), so the reported spread would have been mostly the
  instrument. **A measurement instrument that timestamps itself is not a
  measurement instrument.**
- **THE FIX: no new tool.** `MIDISEND <cluster> LISTEN` and
  `MIDISEND <cluster> CLOCKLOOP <secs> <logfile>` ride the M2 receiver task
  (`platform/aros/midi_camd.c`), which already waits on the CAMD signal,
  stamps each message with EClock when it wakes, counts drops and resolves
  its own library bases. `midi_io/midi_interval.{c,h}` is pure C and
  host-tested (t190, 9 mutants, 9 killed, each hash-verified). It **never
  reads a clock** -- arrival stamps are the caller's to supply. That
  separation is the entire design.
- **TWO DEFECTS OF MY OWN, both found by writing the test rather than the
  code.** Slicing counted against an absolute 32-bit base, so slice 0
  landed wherever the run started and would have wrapped on a long take;
  and the first test asserted an ACCIDENT of that bug ("the first slice is
  the sparsest") rather than a law. Also `LISTEN` required `argc < 6` and
  read `argv[5]` for what is a 5-argument command, so it exited 5 silently
  and looked like a tool producing no output rather than a tool that never
  started.
- **A PRE-EXISTING BUILD BUG, unrelated to M5:** `ri_build_v11.sh`'s
  `midisend` target had **never built** -- one `-c` carried two sources,
  which is a gcc error. The only v11 MIDISEND that ever existed was made by
  hand: "a build script outside the repo is how stale-binary mistakes
  happen", reproduced INSIDE the repo. Fixed, one `-c` per source.
- **I added an r12 gate to the v11 midisend target and it was WRONG.** That
  check belongs to the **v1** build, where the library base is in rdx and
  r12 must be preserved. On ABIv11 **r12 IS the base register** and the
  working MIDICLOCK has 28 such `mov %rax,%r12`; the gate failed a correct
  binary. Reverted with the reason recorded so it is not re-added. The
  ungated link the owner flagged is gone with MIDIRX itself.
- **THE PROOF RAN. First M5 run to complete on either lane** (riqemu1,
  `m5-clockout-cloop.run`, artifact `m5-cloop-riqemu1.log`):
  **sent=1000 clocks=1000 lost=0 badlen=0 backwards=0 intervals=999.**
  The camd path carries a clock without losing, reordering or misframing
  one byte.
- **AND IT DELIBERATELY DOES NOT CLAIM TIMING.** `verdict=2` JITTERY,
  `max_us=40143` ~ 2x `min_us=20025`, and 470 of 999 intervals in the last
  slice. Two independent reasons the intervals are not wire evidence:
  riqemu1 has **no real-time pacing**, so the sender's own cadence was
  unsteady; and the reader drains in batches, so arrival stamps are
  quantised by the drain. **Counts proven, timing not.** The no-drift claim
  stays pinned on the host by t185, and wire timing is still owed a
  real-time host. **Same lesson as MIDIRX, from the other side: an
  instrument that timestamps its own draining reports the draining.**
- **LANE RULES LEARNED THE HARD WAY.** (1) **Never `--exec` anything
  multi-step**: it is not a shell, and ONE blocked command there wedged the
  guest agent's entire command path until `system_reset` (an `echo
  ALIVE-CHECK` queued behind it never ran, and 9295 showed a backlog of 64
  with no accepts). Use `--run-script`. (2) **This shell honours ONLY `;`
  as a comment** -- `#`, `REM` and `/*` are each executed and each print
  `object not found`; measured across all five candidates, not guessed.
  (3) **`RAM:` does not survive `system_reset`**: a PUT that reported
  `sha_ok=True` was gone minutes later, and the next run read
  `RAM:MIDISEND: object not found`.
- **A LANE CAN BLOCK ITS OWN PROOF.** One script runs at a time, so a
  sender and a listener in two processes cannot overlap -- and `cmd &` does
  not background here, the script simply stops after the backgrounded line
  and the sender never runs. That is why `CLOCKLOOP` exists: both ends are
  the two TASKS of one process, with a real camd link between them.
- **Upstream, rechecked:** camd's newest are still `cb8c4c5f3` and
  `28ec43a51` (2026-10-05), already carried in both ABI carriages; nothing
  new in `workbench/devs/USB` since 2025-12-28; zero commits repo-wide
  between checks.

## M5g: the Dell counts too, and the sender was the jitter (2026-10-10)

- **AROS UPSTREAM, rechecked 2026-10-10** (this is the 6th check this
  slice): repo `aros-development-team/AROS`, `pushed_at 2026-10-09T23:30Z`.
  **camd is still unchanged** -- `workbench/devs/midi` has **zero** commits
  since 2026-09-01, and the newest remain `cb8c4c5f39` and `28ec43a517`
  (2026-10-05), both already carried in our v1 and v11 carriages. 100
  commits repo-wide since 2026-10-05, none MIDI-relevant. The USB-side news
  is real but **not applicable**: `fd3ad20c3a` "usb2otg: arm direct INT as
  INT on QEMU's DWC2 core" (2026-10-09) and `949d6834f8` "hub.class: no
  split transfers for devices on a root hub port" (2026-10-09) touch
  `arch/arm-native/soc/broadcom/2708/` -- the Raspberry Pi 2708 SoC tree,
  not the x86-64 PC target either lane runs. Nothing to carry.
- **THE DELL LOOPBACK: `sent=3000 clocks=3000 lost=0 badlen=0`, 60 s.**
  The counts replicate on the second lane and on the other ABI. camd
  carries a clock without losing, reordering or misframing a byte.
- **AND THE TIMING IS THE SENDER'S, PROVEN BY ITS OWN INVARIANCE.** The
  Dell run reported `min_us=20011 max_us=40080` -- and so did riqemu1
  (`min 20025 / max 40143`), on a lane with **no real-time pacing** against
  one that has it. **A spread that does not change when the host's pacing
  changes is not measuring the host.** CLOCKLOOP's sender was
  `send; Delay(1)`, and `Delay(1)` is a 20 ms shell tick plus the send's
  own cost, so it emitted ~20 ms and ~40 ms gaps on both machines for the
  same reason. **Same defect as MIDIRX, one level up: the instrument was
  timing its own loop.**
- **THE FIX: send the REAL schedule.** CLOCKLOOP now drives the same
  `midi_out` producer RIAPP drives (`midi_out_init/enable/start/render`),
  advanced by **EClock** instead of the audio sample clock, and pumps with
  `midi_out_pump`. The no-drift property lives in the producer -- the tick
  count at a position is a pure function of that position (t185) -- so the
  intervals reported are the schedule's own rather than a loop's.
- **THE TIMER IS OPENED PROPERLY, which is the whole MIDIRX lesson applied
  to the code that replaces it**: `OpenDevice("timer.device", UNIT_MICROHZ,
  trq)` as the handshake, `tr_node.io_Command = TR_ADDREQUEST`, then
  `SendIO / WaitPort / WaitIO` -- and **`GetMsg` is NOT also called**, since
  `WaitIO` removes the message and calling both corrupts the port's list.
  `ReadEClock` is an inline through the timer base, so `__TIMER_LIBBASE` is
  pointed at **our** base, taken from the request we actually opened, and
  `<proto/timer.h>` is included after the macro. Leaving it to the TU-global
  `TimerBase` is what gave MIDIRX a NULL base and an illegal access; four of
  the five failures in that whole round were unresolved inline bases, and
  the first build of this fix failed on exactly that (`U TimerBase`
  undefined) before the base was resolved.
- **I STAGED THE WRONG ABI ON THE DELL AGAIN.** I PUT the **v1** MIDISEND
  onto the **ABIv11** Dell, saw the size, and only caught it by comparing
  the two sha256s immediately after. This is the identical mistake that cost
  three MIDIRX rounds, and it is now the second time in this slice. The
  check that catches it costs one `sha256sum` on two files and should happen
  **before** every PUT, not after a suspicion.
- **THE DELL LANE IS DOWN AND NEEDS AN OWNER REBOOT.** The schedule-driven
  run wedged the agent's whole command path (ports 9292/9294 listening with
  a 64 backlog and no accepts), `system_reset` on monitor **4479** did not
  bring the agent back, and three screendumps show a clean desktop with no
  agent window and **no crash requester**. Note the Dell's monitor is
  **4479**, not 4447 -- 4447 is the `nvk` VM, and grabbing the wrong one
  wastes a diagnosis. riqemu1 is unaffected and answering.
  Also cleared the spool's 8 accumulated `jobs/*.json`: a pending job is
  re-executed by the next agent, so a backlog turns one bad run into a
  crash loop.
- **`RAM:` is wiped by `system_reset` on both lanes**, so everything staged
  before a recovery must be re-staged, and a `sha_ok=True` from before one
  is not evidence the file is there.

## M5h: MMC out and the clock-out lamp, and a stale-bounds bug wearing a passing test (2026-10-10)

- **MMC OUT (R3's "and add sending")**, `midi_io/midi_mmc.{c,h}`, t191.
  **Its own E0**, `RIAPP_MIDI_MMCOUT`, deliberately NOT the clock-out switch:
  driving another machine's transport is a bigger consequence than sending
  it a clock, and one switch for both would make the safe choice (clock
  only) inexpressible. Off means **not one byte is produced**, like
  everything else here.
- **THE LAW IS THE ROUND TRIP, and it is pinned by construction.** t191
  emits a locate and feeds the bytes straight back through the *in* path,
  requiring the sixteenths to survive. That is the only way to catch a
  unit mismatch between a writer and a reader of the same field -- which
  is **M3b's SPP bug arriving from the opposite direction**: `hh:mm` is
  16-bit big-endian MIDI beats = sixteenths, and a writer using any other
  unit would send a slave somewhere else with nothing on the wire to say
  so. Over-range **clamps** to 0xFFFF rather than wrapping, for the same
  reason. A short buffer emits **nothing**: a truncated SysEx is not a
  shorter message, it is a broken stream.
- **8 mutants, 8 killed**, each hash-verified. Two of the first round
  **survived, and both were gaps in the test, not the code**:
  - one probe at `cap=3` let a mutant that loosened the guard from `6` to
    `4` through -- and a 4- or 5-byte buffer would then have had SIX bytes
    written into it, an actual overflow. Now every short size is probed,
    and exactly-fits is probed too, or the guard would be over-tight by one.
  - the counters were never asserted, so a dead `sent++` was invisible.
    They are the ev-log's, so they now are.
- **The clock-out lamp (R4), t192: it follows BYTES, not the setting.**
  A lamp wired to "is it enabled" stays green through exactly the failure
  it exists to show: `RIAPP_MIDI_CLKOUT=1`, the sender task refuses to
  start, `midi_out_enable(o, 0)` fails it closed, no byte on the wire. It
  is driven from `ri_pal_midi_sent()` deltas, goes out the moment the
  bytes stop, and has **no timeout** -- a lamp that stays lit 300 ms after
  the last tick is showing the timeout, not the wire. It is also
  deliberately unreachable from `ri_str_indicator_set()`: a caller must
  not be able to switch on a lamp that is a report about bytes observed.
  6 mutants, 5 killed; **DB is EQUIVALENT, recorded as such** --
  `sending && (sent_total > 0u)` and `sending && sent_total` are the same
  expression for an unsigned, and its object hash came out bit-identical
  to the base, which is how an equivalent is told from a survivor. The
  other two were compile-kills from an unused parameter, which prove
  nothing and were redone.
- **A REAL BUG, FOUND BY A PIN MOVING: the section bounds were stale
  literals.** `ri_ctlreg_find` bounded its scan with two hand-written
  `RI_CTLREG_SEC_LO/HI` arrays. Adding the lamp row at the end of the
  transport run shifted every section after it by one index, and **six
  unrelated pattern sections started resolving the wrong rows**. It was
  caught only because a draw-hash pin moved. The lookup itself kept
  working, because a window shifted by one still CONTAINS its own rows and
  merely also contains its neighbours' -- which is why nothing failed
  loudly.
- **AND THE TEST THAT CLAIMED TO COVER IT DID NOT.** `t170`'s header and
  `ctlreg.c`'s comment both said the literals were "VERIFIED AGAINST THE
  TABLE by t170". t170 **derives** each section's range from the table and
  never compares it to the literals: a guard that checks a derived copy
  against itself is not a guard. This is the third time in this repo that
  a comment asserted a property the code did not have (a geometry-shape
  enum written from memory; a probe comparing a reg_id against an index).
  **Fixed by deleting the literals**: the bounds are now computed from the
  table on first use, so there is nothing to keep in sync. One 486-entry
  pass once, then the same bounded scan the literals were there to enable.
- **Then the lamp had nowhere to sit**: `t61` caught 16 registry controls
  against 15 geometry items, which is the correct catch and the reason
  that test exists. Placed at (253, 40), splitting the 130 px between Sync
  (188) and MIDI In (318) on the row the manual puts the other two
  indicator lamps (p. 144-145). **PROVISIONAL -- placed by arithmetic, not
  by eye.** The owner should confirm it reads as "the third lamp".
- **Only the transport section's draw and raster pins moved**, in both
  t92 and t93, and both were re-pinned from measured values with the
  reason recorded at the pin. Six sections moving would have been the bug;
  one section moving is the feature.
- **MMC out is driven from the ONE place the transport changes**
  (`sync_transport`), so no second path can move it and forget to
  announce it -- the same single-choke-point reasoning as the clock-out
  hook. And an intent that arrived **from the wire** sets `s_mmc_echo`,
  which suppresses the outbound: echoing a command back to the master that
  just sent it is a feedback loop, and a locate would bounce between the
  two machines.

## M5i: the schedule on a real lane, and four attempts at a 1 ms timer (2026-10-10)

- **THE ANSWER, at last, from a lane: `mean_us=17856`** against a
  theoretical **17857.14 us** for 140 BPM at 24 ppqn. That is **0.04 % off**,
  measured through the producer, through camd, and back out of a receiver.
  `sent=1708 clocks=1707 lost=1 badlen=0 other=1` -- the single loss is the
  last byte in flight when the receiver closes, which is recorded rather
  than rounded away.
- **THE SPREAD IS STILL NOT WIRE TIMING, and the log says so**:
  `min_us=5 max_us=40173 jitter_us=40168 verdict=2`. A 20 ms service tick
  and a reader that drains in batches put the arrivals into clumps; riqemu1
  additionally has no real-time pacing. **Counts proven, mean interval
  proven to 0.04 %, spread NOT proven.** The physical USB-MIDI wire remains
  the owner's proof.
- **FOUR ATTEMPTS AT A 1 ms SERVICE TIMER, ALL FAILING, AND NONE OF THEM
  NEEDED.** In order, and every one cost a wedged lane:
  1. reusing the request passed to `OpenDevice` -- hung;
  2. `SetSignal` + `Wait` on an allocated bit -- hung, because **this SDK's
     `struct IORequest` has no `io_Signal` field**, so nothing ever raises
     that bit and `Wait(sigbit)` is an unconditional hang;
  3. a fresh request per iteration -- `SendIO` reached `Exec_77_SendIO` with
     **`RSI = 0`**, a NULL `io_Request`, and faulted on its first
     dereference (`mov -0x28(%rsi),%rax`);
  4. **`Delay(1)` -- works.** It is what the original delay-loop CLOCKLOOP
     used, and that version completed on both lanes.
  The lesson is not "the fourth one was right". It is that **none of it was
  required**: `ReadEClock` needs the timer's library base and its frequency,
  both of which come from the `OpenDevice` handshake, not from a running
  timer stream. There is now **no `SendIO` in the loop at all**.
- **HOW IT WAS FOUND, since every symptom was a lie.** No crash requester on
  the Dell; the agent simply never finished. **Three of the first four
  attempts produced NO OUTPUT AT ALL** -- not even setup prints that had
  certainly executed -- because **the lane agent captures script output
  through a pipe, so a child's stdout is block-buffered and nothing flushes
  while the tool is hung.** A tool that only speaks on exit says nothing
  when it does not exit. Progress now goes to a file through dos `Write()`
  (no userspace buffer), which is why MIDIRX wrote `MIDIRX.LOG` and why this
  does too.
- **THE BISECT WAS THE ONLY THING THAT WORKED.** Replacing the timer wait
  with `Delay(1)` -- known-good -- made the loop complete, which placed the
  fault in the wait and cleared the producer, the EClock read and the pump
  in one run. Everything before that was guessing.
- **riqemu1 is the right lane to diagnose on and the wrong lane to measure
  on**, which is now the standing rule: its console is legible (the Dell's
  renders its own crash text as garbage), and it reproduces a Dell hang
  without costing an owner reboot.
- **`sent` WAS REPORTED AS 0 WHILE 1700 BYTES CLEARLY ARRIVED**, and `lost`
  is computed from it, so every run had been silently reporting `lost=0` --
  **a check that cannot fail is not a check.** Two independent sources now
  exist for that number (what `midi_out_pump` reports carrying, and what the
  sink counted handing over) and they are compared at run end.
- **A GATE I ALMOST SHIPPED PAST:** the progress file was at a hard-coded
  `RAM:` literal in `app/`, which T6 forbids outside `platform/aros/`. The
  audit caught it. The path is now derived from the caller's own log path,
  which also means the two can never land on different volumes.

## M5j: the Dell agrees, and `system_reset` was never a recovery (2026-10-10)

- **BOTH LANES, THE SCHEDULE MEASURED. riqemu1** `mean_us=17856`,
  sent 1708 / clocks 1707 / lost 1. **Dell** (ABIv11, 8 s)
  `mean_us=17853`, sent 3584 / clocks 3583 / lost 1, artifact
  `m5-cloop-dell.log`. Theory for 140 BPM at 24 ppqn is **17857.14 us**:
  **0.02 % and 0.04 % out**, on two machines, two ABIs, one with real-time
  pacing and one without. The single loss is the last byte in flight when
  the receiver closes, on both.
- **THE SPREAD IS THE INSTRUMENT, AND TWO LANES PROVE IT.** Both report
  `min_us` of 3-5 against `max_us` of ~40100, and both put ~89 % of their
  arrivals in the LAST slice. A spread that is the same shape on a paced
  host and an unpaced one is not measuring either: it is the **reader**.
  `min_us=3` says several ticks are being stamped microseconds apart, which
  is what a batch drain looks like -- the M2 receiver task stamps each
  message when it wakes and drains, so a batch shares a timestamp. **There
  is no way to measure per-message WIRE arrival with this architecture**,
  because the PAL exposes drain-time stamps, not arrival-time ones. That is
  a real structural limit, recorded rather than worked around: it would
  need per-message arrival timestamps from camd itself.
- **`system_reset` IS NOT A RECOVERY ON THE DELL. IT IS A VM KILL.**
  The Dell runs `-no-reboot`; riqemu1 does not. So `system_reset` there
  makes **QEMU exit** instead of rebooting the guest, and every "recovery"
  I performed on that lane **destroyed the VM**. I twice reported the lane
  as needing an owner reboot while my own command was the cause; the owner
  rebooted correctly and I misread their fix as evidence for my theory.
  **On this lane, recover by relaunching QEMU, not by resetting it** --
  and check the flags before assuming a lane behaves like another.
- **THE "IDENTITY HAS BEEN CLAIMED" MESSAGE WAS A CRASH-LOOP SYMPTOM.** The
  guest agent was restarting continuously; each new instance raced the
  server's still-held registration for `e6320` and was refused. Four
  `ATCPBIN` windows had accumulated. A fresh VM boot cleared them, and exec
  returned to rc=0 immediately. The server was never at fault: one session,
  one connection throughout.
- **AND I STAGED A STALE BINARY, AGAIN, AND THE SIZE CHECK CAUGHT IT.**
  The v11 MIDISEND on disk was 61768 B built at 11:14 -- still carrying the
  hanging timer code -- while the working v1 build was 66408 B. Caught by
  comparing sizes before the put, which is the check that has now earned its
  keep three times. The staged binary was rebuilt (62912 B) before the run.
- **`echo: file is not executable`** on the Dell: bare `echo` is not the
  shell builtin there. Every run script needs `echo "text"` with an
  argument, which is why the `;`-comment rule and this one belong together
  in the same note.

## R7a: the SMF writer, and three places I asserted the format wrongly (2026-10-10)

- **`project/smf_export.{c,h}`** — SMF type 1 export, t193, pure C,
  host-tested, no allocation and no IO. Built on the engine's own
  `ri_sched_emit_sorted()` output rather than a second derivation of "what
  notes does this pattern play", so the file and the instrument cannot
  disagree when one of them drifts.
- **THE MAPPING IS AN OWNER REVIEW ITEM** (spec R7) and is therefore
  isolated in `ri_smf_velocity()` rather than scattered through the writer.
  t193 pins it only as deterministic and in 1..127, never as a chosen
  convention. Accent 112 / plain 64 is the default and is **recorded, not
  defended**.
- **I GOT THE FORMAT WRONG THREE TIMES, AND THE TEST CAUGHT ALL THREE.**
  1. **Chunk lengths are FIXED 4-byte big-endian integers. VLQ is for DELTA
     TIMES inside a track and for nothing else.** I "fixed" the header
     length to a VLQ on the natural but wrong grounds that every length in
     the format is variable — which produced an **eleven-byte header** whose
     `MTrk` magic landed where the division field belongs. The file was not
     an SMF at all.
  2. **`ri_smf_vlq` returns a LENGTH, not an offset.** Writing
     `at = ri_smf_vlq(out + at, ...)` rewinds the write cursor to 1 and the
     track body overwrites the header. **Three sites had this**: the
     conductor chunk, the data chunks, and the per-event delta inside
     `build_track` — the last of which silently deleted the note-on and
     declared a length of 8 for 19 bytes of data.
  3. **Chunk lengths must be EXACT.** Four bytes too many shifts every
     later track; four too few loses the tail; neither is reported.
- **THREE MUTANTS SURVIVED THE FIRST PASS AND ALL THREE WERE TEST GAPS.**
  A test that asserts only on `ri_smf_delta_ticks()` and
  `ri_smf_velocity()` never once looked at **the bytes the writer emits**,
  so a constant delta, a note-off written as note-on-velocity-0, and a
  missing end-of-track all passed. Now the emitted bytes are decoded and
  checked: `00 90 24 70` then `30 80 24 40`, ending `FF 2F 00`.
- **AND ONE MUTANT WAS SCORED WRONG BY MY OWN HARNESS.** The chunk-length
  mutant does not fail an assertion — it **segfaults the test binary**,
  because a walk that trusts a declared length runs off the buffer on a
  malformed file. The harness scored on `grep FAIL` and called it
  **SURVIVED**. It now scores on the **return code**, and the test's chunk
  walk **bounds the declared length before stepping by it**, so a malformed
  file fails an assertion instead of killing the process. *A crash reports
  nothing, and a harness that reads crashes as passes will one day record a
  real kill as a survivor.*
- **RESULT: 10 mutants, 9 killed, 1 recorded as EQUIVALENT.** `SJ` removes
  the `ppq == 0` guard, which returns the same 0 the arithmetic already
  produces (`d * 0 / 48000 == 0`) — identical behaviour, verified rather
  than asserted. It is kept as a guard against a future divisor change, not
  claimed as a kill.
- **Verified output** (ppq 480, 120 BPM, one 303A track):
  `MThd 00 00 00 06 | 00 01 | 00 02 | 01 E0`, a conductor track with
  tempo 500000 us/qn, and a data track carrying
  `FF 03 04 "303A" | 00 90 24 70 | 30 80 24 40 | FF 2F 00` — note-on with
  accent velocity, delta 48 ticks (VLQ `0x30`), a real note-off, EOT.

## R7b: the SMF reader, and the exporter bug only a reader could find (2026-10-10)

- **`ri_smf_read()`**, t194. Quantises to 16ths into `struct RIStep` and
  **reports rather than guesses**: six counts (`stuck_notes`, `orphan_off`,
  `bad_note`, `past_end`, `tempo_changes`, `tracks`) because a flag set can
  only say *that* something went wrong, and the caller's job is to say which.
- **WRITING THE IMPORTER FOUND A REAL EXPORTER BUG.** **Every event in a
  track -- channel voice, meta AND sysex alike -- is preceded by a delta
  time**, and `put_text()`/`put_tempo()` wrote none. So **the exporter's own
  files were malformed**: a conforming reader consumes the `0xFF` as a delta
  time and the track falls apart. Nothing in the exporter's tests could have
  found it, because every one of them asserted on functions and never once
  read the file back. *This is the whole argument for having both halves.*
- **And the reader had its own version of the same bug**, one level in: the
  meta skip advanced `2 + length` and omitted the WIDTH of the length's own
  variable-length field, landing the cursor one byte early on **every track
  that carries a name** -- which is every track this exporter writes.
  Mutant TE pins it.
- **FOUR MORE OF MY OWN WRONG BELIEFS, all caught by the round trip:**
  - **Chunk lengths are fixed 4-byte big-endian; VLQ is for delta times
    only.** Same error as R7a, now pinned in both directions.
  - **`ri_smf_vlq` returns a length, not an offset.**
  - **`0x00,0x00,0x00` is THREE bytes.** I wrote `0,0,0x0` for a two-byte
    division field, making the header fifteen bytes and shifting every
    following offset by one. It presented as "the importer refuses to read
    its own file", which is the importer being correct.
  - **48 000 samples at 48 kHz with 480 ppq is FOUR 16ths, not two** -- what
    you get by forgetting the sample rate.
- **THE LAWS THAT MATTER ARE ALL INVISIBLE WHEN BROKEN:** a note-on with
  velocity 0 is a note-off (otherwise the note sounds forever); a note still
  held at end-of-track is closed **and counted** (a DAU saving mid-note is
  routine, and dropping it loses a hit); an orphan note-off is ignored and
  counted, never matched to whatever is nearest; a note above 127 is clamped
  **and counted per event**; events past the caller's array are dropped
  **and counted**, so a long pattern's ending never disappears silently.
- **THE IMPORTER BOUNDS A DECLARED LENGTH BEFORE STEPPING BY IT** (mutant
  TD). A file claiming a 2 GiB track is four bytes of header and nonsense,
  and a walk that trusts it leaves its own allocation. This is the same law
  that killed SA in R7a, arrived at from the reading side.
- **RESULT: 10 mutants, 10 killed**, scored on the return code so a crash
  cannot read as a pass. Two survived a first pass -- the note clamp and the
  past-the-end count -- because no test had built a file that reached them;
  both cases now exist.

## R6a: note and CC output per device, and the collision it exists to avoid (2026-10-10)

- **`midi_io/midi_devout.{c,h}`**, t195. Pure C, host-tested, no CAMD.
  Turns one already-sorted engine event into the three bytes that go on the
  wire, and it owns the two decisions that would otherwise be made at every
  call site.
- **THE CHANNEL MAP IS THE WHOLE RISK, AND AN UNCLAIMED CHANNEL IS REFUSED.**
  The G7 remote is a documented **one-channel** path (manual p. 134) that has
  to keep working while other things are plugged in (`midi_chan.h`). A device
  with no assigned channel therefore emits **nothing and is counted** --
  defaulting it to channel 1 would put the 303 on the remote, and **a note on
  the wrong instrument is worse than a note that does not play**. Channel 0 is
  additionally marked as the remote's so a caller can refuse it without
  hard-coding the number.
- **AND A CHANNEL PAST 15 CLAMPS — IT MUST NEVER WRAP.** Wrapping 16 to 0
  would put a voice on the G7 remote, which is the one collision this feature
  exists to prevent. The same is true of note and velocity: 200 wraps to 72,
  which is a **different instrument**. Every clamp is counted.
- **SLIDE IS LEGATO AND EMITS NOTHING.** A slide keeps the gate high and
  slews the pitch, so it must not re-attack; a fresh note-on is the audible
  defect. It is a separate flag, not a note-on variant, precisely so it
  cannot be forgotten — and it is counted, so a correctly-silent slide never
  looks like a silent failure.
- **NOTE-OFF IS ITS OWN FLAG, NOT "VELOCITY 0".** Overloading velocity would
  make a genuine velocity-0 note-on unrepresentable and put two meanings in
  one byte a reader has to guess between. Note-off is a real `0x8n`; a
  note-on with velocity 0 is still a note-on here.
- **CC NUMBERS COME FROM THE G7 REGISTRY, NOT A TABLE HERE.** Appendix C *is*
  `gui/ctlreg.c` — the same registry the G7 **input** path resolves through
  `ri_ctlreg_by_cc()`. **A second copy of that map in this file would be a
  second answer to the same question**, and the two would drift.
  `ri_devout_cc_named()` asks the registry, which is what makes "the same
  controller numbers the G7 map uses" a fact rather than a promise.
- **NO NEW TRANSPORT.** These three bytes go into `midi_out`'s ring like any
  other outbound message and the existing sender task carries them to camd.
  R6 needs no new AROS-only code, which is why it is a host-testable phase
  rather than a lane phase.
- **RESULT: 10 mutants, 10 killed.** One (`DI`) was a *compile* kill from an
  unused parameter, which proves nothing, and was redone so the remote-channel
  law is genuinely pinned.
- **MY OWN TEST WAS WRONG TWICE, IN A WAY THE MODULE WAS RIGHT.** It expected
  channel 15 to play **without being claimed** — which is the exact behaviour
  the module exists to refuse — and it assumed a device-indexed unassign when
  the caller owns the device map. Both fixed in the test, not the code.

## R6b: the drum note maps, keyed by sound id because lane disagrees (2026-10-10)

- **THE 303 NEEDS NO MAP AT ALL, and that is a finding rather than an
  omission.** `RI303Row.key` is a **0..12 scale degree**, not a MIDI note;
  the engine's `ri_p303_note()` already resolves it against
  `RI_303_BASE_NOTE` (E0, `docs/evidence/sequencer/303-base-note.md`) with
  the octave flag and clamps into 0..127. R6 therefore **contains no 303
  map**, because a second one would be a second answer to a question the
  engine has already answered, and the two would drift. Mutant EG pins the
  fact rather than leaving it in prose.
- **THE DRUM MAPS ARE KEYED BY SOUND ID, NOT BY PANEL LANE** — the one real
  design decision here. The two machines' lane enums **disagree**: lane 7 is
  `RI_L808_CB` on the 808 and `RI_L909_CH` on the 909, lane 8 is `CY` vs
  `OH`, lane 9 is `OH` vs `CC`, lane 10 is `CH` vs `RC`. **One table indexed
  by lane would play a completely different instrument on the two machines
  for four of eleven indices, and nothing would report it.** Mutant EB kills
  exactly that.
- **CORRECTION (owner asked what blocked R6c, and the honest answer is
  nothing).** I recorded above that the engine's lane translation was an
  unfinished identity table mapping rim shot onto low conga, and used it to
  call R6c blocked. **That was wrong.** `RI_LANE_TO_RB808_SLOT[i] == i` is
  **pinned by t53**, alongside `rb808_slot_of(RI_808_SLOT_DEFAULT[i]) == i`:
  the lane *is* the slot, and the sound stored in that slot is what the engine
  triggers. `RI_808_SLOT_DEFAULT = {0,1,2,3,4,8,10,14,13,12,11}`, so lane 5
  is slot 5 and slot 5 holds sound **8** — rim shot, exactly as it should. I
  had read the `RB808_*` slot **macros** as if they were sound ids; they are
  not.
  **The lane→sound step R6c needs was always there** — `e->s808.slot[lane]`
  for the 808 (live, user-remappable) and `RI_LANE_TO_RB909_VOICE[lane]` for
  the 909, both consumed in `engine/engine.c` before R6 sees anything. So R6
  keys its notes by sound id exactly as designed, and **R6c is not blocked**.
  *The lesson is not "look harder". It is that a blocker I asserted without
  reading the test that pins the thing is a blocker I invented, and three
  lines of `t53` would have refuted it.*
  The keying by sound id still stands on its own merits: the two machines'
  lane enums genuinely disagree at four of eleven indices, so a lane-indexed
  note table would still be wrong.
- **AN UNKNOWN SOUND ID IS REFUSED, NEVER GUESSED.** Defaulting it to the
  bass drum would put a **hi-hat on the kick** — audible, and invisible
  unless something counts it. Two separate tables (one per machine) for the
  same reason as above: a sound id means different things on the two.
- **THE NOTES ARE A CONVENTION, not a fact** — General MIDI percussion, the
  conventional answer and the one that makes a DAW's drum editor show the
  right name. **An OWNER REVIEW ITEM**, isolated in two functions so
  changing it is one edit and one test. t196 pins in-range, no two sounds
  colliding **within** a machine (sharing **across** machines is legitimate —
  the same GM note means the same drum on both), and refusal for an unknown
  id.
- **THE TEST CAUGHT A REAL BUG IN MY OWN IMPLEMENTATION.** `ri_devout_drum`
  **hardcoded channel 0** — the G7 remote — so every drum hit landed on the
  one channel this whole feature exists to keep clear. It was caught only
  because the test refuses an unclaimed channel and that one had quietly
  claimed it. The channel is now a required **argument**: the caller owns the
  device map. Mutant EE pins that drum hits cannot bypass the channel laws.
- **RESULT: 8 mutants, 7 killed, 1 recorded as EQUIVALENT.** `EH` removes the
  `enabled` guard in `ri_devout_drum`, but `ri_devout_note` below it checks
  the same flag and increments the same `refused` counter — identical
  behaviour, verified by reading both guards. Kept as defence in depth, not
  claimed as a kill. Two earlier mutants were *compile* fails from my own
  anchors (a duplicate `default:` and a mutant that still called the guarded
  function) and were rebuilt.

## R8a: the stem WAV writer, and what a mono test cannot see (2026-10-10)

- **`project/stem_wav.{c,h}`**, t197. Pure C, host-tested, **no IO** — the
  caller supplies a buffer. This is deliberately the FILE side only:
  rendering each mixer strip is the offline renderer's job, and keeping the
  two apart is what makes these laws testable without an engine. A WAV has
  no schema and no error reporting, so every mistake here produces a file
  that either will not open or will open and be subtly wrong.
- **THE SIZES ARE REAL BYTE COUNTS, and there is no way to ask for a
  placeholder.** Writing 0, or `0xFFFFFFFF` for "streamed", in the RIFF and
  data sizes is the commonest way to make a WAV no DAW opens. `FA` and `FB`
  are exactly that.
- **STEMS ARE SAMPLE-ALIGNED AND EQUAL-LENGTH, WHICH IS A PROPERTY OF A
  GROUP** and so cannot be enforced by a single-file writer — hence
  `RIStemSet`, which owns the common length as **the LONGEST** stem and
  **COUNTS** the ones it had to pad. Taking the first, the shortest or the
  average would each silently drop or invent audio on some channel.
- **PADDING MUST BE SILENCE**, and proving that needed the scratch buffer
  **dirtied first**. The pad buffer is `static`, so on a first write it is
  already zero and a writer that memsets the *wrong length* still looks
  perfect — which is why the mutant that truncates the memset survived the
  first pass. Writing a full-length stem first fills the tail with audio,
  and only then is the padding visible.
- **INTERLEAVED, WHICH A MONO TEST CANNOT SEE AT ALL.** Planar and
  interleaved produce **identical bytes for one channel**, so every mono
  case is blind to it. A planar file plays one channel then silence while
  its header looks perfect. The test now writes a stereo stem with full
  positive left and full negative right and reads the first frame back.
- **24-BIT IS THREE BYTES A SAMPLE** — the usual off-by-one, because it is
  not a byte count — and **IEEE float is FORMAT 3**, not format 1 with 32
  bits, which would open and play noise. An unsupported depth is **refused,
  not rounded**: a 20-bit request quietly becoming 24 is worse than no file.
- **MY TEST READ BLOCK-ALIGN AND BIT-DEPTH BACKWARDS** (32 and 34), which is
  the same class of mistake as writing 24-bit as a byte count. Fixed in the
  test.
- **RESULT: 10 mutants, 10 killed.** Three survived a first pass and all
  three were gaps of the kind this slice keeps finding: no stereo case (so
  interleave was untestable), the long stem always added first (so "first"
  and "longest" were indistinguishable), and a `static` pad buffer that was
  already zero (so a wrong-length memset looked right).

## R6c: the event-to-bytes translation (2026-10-10)

- **`ri_devout_translate()` / `ri_devout_emit()`**, t198. **This is not a
  mapping problem** — the engine has already done every part of it:
  `ri_p303_note()` puts a real MIDI note in `RIStep.note` before the
  scheduler runs, and `e->s808.slot[lane]` / `RI_LANE_TO_RB909_VOICE[lane]`
  resolve lanes to sounds. R6c is a **translation of four flags into three
  bytes**, and it is deliberately the only thing it adds.
- **THE LANE→SOUND STEP IS THE CALLER'S, AND R6 NEVER LOOKS.** The 808's
  mapping is live and user-remappable; the 909's is static. A helper that
  guessed would be wrong for one of them. `ri_devout_resolves_lane()`
  returns 0 and is pinned, so the fact is a test rather than a promise.
- **ACCENT IS VELOCITY, AND LIVE AND EXPORT MUST AGREE.** `GB` drifts the
  live accent velocity by 8 and t198 kills it against
  `ri_smf_velocity()`. Two conventions that disagree mean a loop exported
  from a live take comes back with different accents — nobody hears that as
  "wrong" and everybody notices it as *"this loop feels different"*.
- **NOTE-CONTINUE IS THE SLIDE.** `RI_EV_NOTE_CONTINUE` is rest+slide: the
  gate stays high and the pitch slews. Treating it as a fresh note-on is
  exactly the re-attack R6a refuses to emit, so it is its own case and emits
  nothing (`GC`).
- **INTERNAL EVENTS PRODUCE NO BYTES.** Flams, accents, pattern changes,
  automation, meters and transport translate to nothing (`GE`): a DAW
  recording those as notes is a DAW full of ghosts.
- **DRUMS CARRY THE ACCENT TOO.** Routing drums through `ri_devout_drum()`
  would have pinned every drum at one velocity, so an accented kick and a
  plain one would land in a DAW identically (`GF`). Every path now ends at
  `ri_devout_note()`, so the channel laws apply to drums as well.
- **A DESIGN FLAW THE MUTANTS FOUND IN ME.** I first keyed `emit` on
  `RI_DRUM_CLASS_*`, which has **no value meaning "melodic"**, so the 303
  path and the drums shared one parameter and the drums' 0 collided with
  it. **Four mutants survived because the melodic path had never run.** It
  is now keyed on the engine's own `RIEvent.device` — 0/1 the 303s, 2 the
  808, 3 the 909, exactly as `engine.c`'s dispatch reads it. One parameter,
  one meaning, taken from the field the event already carries.
- **RESULT: 10 mutants, 10 killed.** Four survived a first pass and all four
  were the same gap: `drum_class = 0` **is** `RI_DRUM_CLASS_808`, so every
  `emit` call in the test went down the drum path. A fifth, `GJ`, survived
  after the refactor because the event it reused had `note == 0`, so the
  melodic fallback refused it for an unrelated reason and the test could not
  tell the two paths apart.

## R6d: the engine's note tap, and the drain onto the wire (2026-10-10)

Two halves, because they are two different claims. t199 is the engine
recording notes without knowing a wire exists; t200 is those notes becoming
bytes.

### The tap (`engine/engine.{c,h}`, t199)

- **THE ENGINE RECORDS, `app/` SENDS.** The tap is a ring of records in
  `struct RIEngine`; `engine/` includes nothing from `midi_io`, names no
  channel and sends nothing. The confinement gate (no AROS outside
  `platform/aros`) keeps camd out of the audio path, and this is what keeps
  it out of the note path too.
- **THE TAP RECORDS THE RESOLVED SOUND, NEVER THE LANE.** The 808's
  lane→sound map is `e->s808.slot[lane]` and the user can remap it; the
  909's is the static `RI_LANE_TO_RB909_VOICE`. A tap recording the lane
  would hand `app/` a number it cannot interpret without re-implementing
  the engine's own resolution — and the app's copy would be wrong the moment
  a lane moved. `NA` (records the lane), `NB` (uses the static table and
  ignores the live one) and `NC` (the 909 equivalent) are all killed.
- **`ri_engine_load` MUST NOT CLEAR IT, and t199 pins that.** The live
  session reloads the engine every 256-frame buffer — the drum-ring A0 note
  in `engine.c` — so a reset in `load` would cap the ring at one block's
  notes. `NL` kills it. The reset is `ri_notetap_reset()`, explicit,
  once at startup.
- **OVERFLOW DROPS THE OLDEST, NOT THE NEWEST.** The note that just played is
  the one the caller is looking at. `NJ` (drops the newest) and `NK` (not
  counted) are killed.
- **THE LATE ACCENT IS A RECORD OF ITS OWN KIND.** A total accent
  (`RI_EV_ACCENT` to `RI_VOICE_ALL`) sorts after the same-sample hits and is
  applied retroactively to voices stamped at this cursor — so by the time it
  arrives the note-on has *already* been written to the tap, and **MIDI has
  no way to make a note it already sent louder.** Rather than pretend
  otherwise, the tap carries it as `RI_NOTEK_LATE_ACCENT` sharing the same
  `sample` stamp, which makes the loss **countable** rather than arguable.
  `NG` (not recorded), `NH` (mislabelled as a note) and `NI` (sample stamp
  dropped, so the pairing becomes unmeasurable) are killed.

### The drain (`app/core/live_driver.c`, `midi_io/`, t200)

- **R6 NEEDS NO TRANSPORT OF ITS OWN.** Notes go into `midi_out`'s ring and
  ride the *same* sender task and the *same* status-byte framer as the clock.
  That is why there is no new AROS code and no new socket in this phase.
- **AN UNASSIGNED CHANNEL IS REFUSED *BEFORE* `ri_devout_note()`.** That
  function *clamps* a channel above 15 rather than refusing it, which is
  right at its own boundary (channel 16 must not wrap onto the G7 remote)
  and catastrophic here: an unassigned "channel 255" would clamp to 15 and
  put every note on an instrument nobody chose. So `note_ch` is an `int`
  and `-1` means unassigned, checked in the drain. `LB` (passed through) and
  `LC` (defaulted to channel 1) are killed.
- **OFF MEANS THE TAP IS NOT EVEN WALKED.** `dev_out == NULL` — every caller
  today — returns immediately. `LA` kills it. And a *disabled ring* refuses
  a put outright (`MG`), so E0 means an **empty** ring, not a full one
  nobody reads.
- **THE LATE ACCENT EMITS NOTHING AND IS COUNTED** (`late_accents`), as is a
  refusal (`devout_refused`).

### Two real defects this phase found

1. **`put_run()`'s long-message guard only worked on a nearly-full ring.**
   The guard was `need >= CAP-1` where `need = n - room`, which catches a
   long message only when `room` is small. On an **empty** ring a 300-byte
   message took the `room < n` branch with `room = 255`, dropped 45 from the
   tail — which pushed `tail` past `head` — and made `head - tail` underflow
   to 4294967251. `room` then computed as 300, the size check passed, and
   **300 bytes were written into a 256-byte ring.** The check belongs where
   the length is known, not where the shortfall happens to be: `if (n >=
   RI_MIDIOUT_CAP) return 0;` at the top. `MA` kills the old behaviour. This
   was reachable from clock out too, not only from notes.
2. **`build/portable.mk` had NO HEADER DEPENDENCIES.** The rule depended on
   the `.c` alone, so editing a header rebuilt only the newer `.c` files and
   left the rest compiled against the **old struct layout**. It does not fail
   to link — it links a binary in which two translation units disagree about
   where `struct RIEngine` ends. Adding `struct RINoteTap` grew `RIEngine` by
   4 KiB; `engine.o` rebuilt and `live.o` (which embeds an `RIEngine` by
   value) did not, and `headless` **segfaulted inside `ri_live_render`** with
   gdb reporting `0x10200` as the faulting address. Now `-MMD -MP` with
   `-include $(CORE_OBJS:.o=.d)`. Verified: touching `engine/engine.h` now
   rebuilds 5 dependent objects where the old rule rebuilt 1.
   **Same class as the ABIv1/ABIv11 stale-binary trap — the build lies, and
   the evidence is a crash rather than an error.** `ri_build_host.sh` is
   immune (its `all` recompiles unconditionally), which is why t199/t200
   were valid while this was not.

### Mutation results

- **t199: 15 mutants, 14 killed, 1 equivalent.** `NO` (the ring index stops
  masking) returned the **identical object hash** — the compiler already
  emits the mask for `%` on a power-of-two, so it is equivalent by
  construction and not a test gap.
- **t200: 22 mutants, 19 killed, 3 equivalent**, and the survivors are worth
  naming because two of them are the *good* kind:
  - `LD` removes the drain's late-accent `continue`, and `ri_devout_record`
    independently refuses any kind that is not a plain note. **Two layers of
    refusal**, so the wire is byte-identical. `DQ` removes *both* and is
    killed — which is what proves the test had the power and that `LD` is
    genuinely equivalent rather than untested.
  - `LI`/`DI` (the same defect written twice) push bytes unconditionally;
    `put_run` independently refuses a zero-length push. Equivalent.
- Six survived a first pass and every one was a **test gap**, not a missing
  law. Two are worth recording because they are the kind that hide:
  - `MC` moves the drop policy from "discard the oldest" to "overwrite the
    newest" while leaving occupancy and the drop COUNT identical. My fill was
    **uniform** (identical clock bytes), which made both produce the same 255
    bytes. Only a **non-uniform** fill distinguishes them, and the difference
    is the whole point: dropping the oldest loses stale ticks, overwriting
    the newest deletes the note that just played.
  - `MB` made the inner guard "salvage" the shortfall instead of refusing. It
    still returned 0 — while silently discarding 254 bytes on the way out.
    **A return value alone is not enough to pin a drop policy**; the drop
    counter has to be asserted too.
- One test failure was the disabled-means-nothing law biting: a fill loop
  wrote to a ring that `fixture()` had left disabled, so every write was
  correctly refused and the loop never terminated. It hung the suite rather
  than failing it — which is exactly why the mutant harness scores on return
  code and never on `grep FAIL`.

### Build lists

`midi_io/midi_devout.c` added to `ri_build_aros.sh`, `ri_build_v11.sh` and
`build/portable.mk`. **AROS RIAPP only** — the standalone `MIDISEND` sender
does not need it, because the sender only pumps a ring.

## R6e: the five owner conventions, settled and pinned (2026-10-10)

Owner decision, 2026-10-10, on the R7/R6 GM note conventions. These are
**conventions, not placeholders** — every one is a place where a
plausible-looking change is silently audible somewhere else, so each is
pinned by t201 with a mutant that undoes it.

1. **THE 303 GOES OUT AS-IS, NEVER TRANSPOSED.** `RI_303_BASE_NOTE` is 36
   (key 0 = C2), a settled machine decision with its own evidence file. A GM
   plugin puts key 0 near middle C, so the same pattern arrives two octaves
   lower than a GM user expects. **The wire agreeing with the synth is worth
   more than the wire agreeing with a convention**; the cost is one
   session-level transpose in the DAW, done once. `CA` adds 24 and is killed.
2. **DRUMS GO TO CHANNEL 10 AND IT IS NOT A CHOICE.** GM defines percussion
   on channel 10 and nowhere else — a drum note on channel 3 selects a
   melodic instrument and plays a wrong pitched tone, or nothing.
   `ri_devout_emit` **overrides** the channel for the 808/909 and counts the
   override (`CB`, `CC`). Two consequences are pinned because either one on
   its own would be a bug:
   - **Channel 10 is claimed implicitly** (`CD`). Without that, the override
     moves the note to a channel nothing had claimed and every drum is
     refused for arriving where it was just sent.
   - **The melodic channel stays opt-in** (`CE`, and the live driver's own
     half, `CP`). Forcing drums to 10 must not quietly make an unassigned
     melodic channel legal — that is R6a's law and channel 0 is the G7
     remote. The **live driver** was changed to match: drums no longer
     require a configured melodic channel, so a user who only wants the 808
     out is not made to configure one first.
3. **VELOCITY IS 112 ACCENTED / 64 PLAIN, LIVE AND EXPORT ALIKE.** t198
   already pinned that `ri_devout_velocity()` and `ri_smf_velocity()` agree;
   `CF` and `CG` re-pin it now that it is a decision. **The 303 row is
   `{key, flags}` with no level field, so two levels is the WHOLE dynamic
   range of this wire** — not a stand-in for a bigger one. That is a
   property of the machine, not a shortcut here.
4. **A SLIDE IS SILENT.** Legato, no re-attack, no bytes (`CH`). The
   consequence is a real fidelity limit and is recorded rather than hidden:
   **a DAW recording a slide holds the STARTING pitch and never learns the
   destination, so a recorded slide plays back at the wrong pitch.** Owner
   took "accept it". Emitting pitch bend would fix it and make the wire
   stateful; it is the obvious future answer if this limit is felt.
5. **ONE PROGRAM CHANGE ON ENABLE, MELODIC CHANNEL ONLY** (`CI`–`CO`). Not
   per note — that is a stream of noise — and not while disabled, since E0
   means not one byte. Drums get none, because on channel 10 **the kit IS
   the program.** Refusals pinned: an unclaimed channel (`CK`, it would
   select an instrument behind the user's back), a channel above 15
   (**refused, never clamped** — `CJ`), and a one-byte buffer (`CO` —
   truncating `0xCn,pp` to `0xCn` selects whatever program the receiver
   last had, the opposite of self-describing). It counts as an emitted
   message (`CM`).

**THE PROGRAM NUMBER IS THE ONE THING I PICKED.** GM has no 303 program, so
any value is a convention. `RI_DEVOUT_PROGRAM_ELECTRIC_BASS_PICK` = 34
(GM 35 1-based), the closest analogue to a 303 line, and a **named constant
so that changing it is a one-line edit and not a search.**

### Mutation results

- **t201 + the live driver: 21 mutants, 19 killed, 2 equivalent.**
- **CE is equivalent**: by the time the implicit claim runs, `channel` has
  already been set to 10, so claiming `assigned[channel]` re-claims the slot
  it just claimed. The **identical object hash** confirms it is equivalent
  by construction, not untested.
- Three survived a first pass and all three were test gaps, one of them the
  same mistake as `GJ` in R6c and worth repeating: **`CJ` ran against a
  producer with nothing assigned**, so the clamp landed on an unclaimed
  channel and the claim check refused it for an unrelated reason. Claiming
  channel 8 first is what makes clamping and refusing distinguishable.
- Two mutations **failed to build and so proved nothing** (`CF`, `CH`): both
  left a parameter unused, and `-Werror` caught it. A mutation that does not
  compile is not a survivor and not a pass either, and a harness that scored
  them as "killed" would be reporting a compiler error as test evidence.

### Still open

**R6f — the app-level switch and the melodic channel.** `s_lv.drv.dev_out` /
`note_ch` from `riapp.c`, the one program change pushed into the ring on
enable, and the on-panel controls for the E0 setting and the channel map.
**Blocked on two owner answers: which E0 switch carries MIDI note output
(the existing clock-out switch, or its own), and which channel the melodic
303/808/909 notes play on.**

## R6f: the second E0 switch, and the note channel (2026-10-10)

### The switch is separate, and I was wrong to say otherwise

I recommended sharing the clock-out switch with note output and then found
the case that breaks it: **someone slaving their own drum machine to our
clock.** They want the clock and specifically *not* the notes — otherwise
turning on ReIncarnation's 808 fires the very machine they are driving, from
a pattern they did not ask to play. One switch makes that state unreachable.

**The codebase had already decided this once.** `RI_MIDI_SET_MMC_OUT` exists
separate from `RI_MIDI_SET_CLK_OUT`, and its own comment says why: *"clock
out and MMC out are different features with different consequences on a
slave, and one E0 switch for both would make the safe choice (clock only)
impossible to express."* This is the same argument one feature over.

**The ring and the sender are still shared.** That is why this costs two
settings and not a second transport: notes ride the clock's ring, its sender
task and its framer. What changed is that **the sender starts if EITHER is
on** — notes with no sender is a producer filling a ring nobody drains — and
that a failed start turns **both** off, fail-closed.

### The melodic channel, and the drum channel that is deliberately absent

- `RI_MIDI_SET_NOTE_CH` is 0..16 where **0 is UNASSIGNED and never
  defaulted** (`DC`, `DE`). There is no channel a 303 can go on that we are
  entitled to pick. `riapp` passes 0 through as `-1` and the drain refuses
  melodic notes on it.
- **Refused, never wrapped** (`DB`, `DF`). 17 wrapping to 1 would put the
  303 on a channel nobody chose; 0 wrapping to 16 would put it on the G7
  remote, which is a documented one-channel path (manual p. 134). A negative
  is refused too, and that one is not cosmetic — `-1` stored as a `uint8` is
  255, which is truthy, so the "0 means unassigned" translation would turn a
  refusal into 254, a channel nothing downstream checks for.
- **THERE IS NO DRUM-CHANNEL SETTING, AND THERE CANNOT BE.** GM defines
  percussion on channel 10 and it is not a choice, so a control for it could
  not do anything.
- `dev_out` is strictly 0/1 (`DA`), for MMC_OUT's reason: a truthy value
  would put notes on the wire from a control that reads as a slider.

### The attach, and the one program change

`ri_livedrv_devout_attach()` claims the channel and announces the instrument
**once** (`EA`): the program change names the instrument for the session, and
a drain that announced on every attach would fill a slave's channel with
them. It is idempotent, and it fails closed — a NULL producer, a NULL ring
or a disabled ring announces nothing and, just as importantly, **claims
nothing** (`ED`). `prog_sent` is set only when the bytes actually landed
(`EB`), so a disabled ring can still announce once it is enabled.

**NO CHANNEL MEANS NO PROGRAM CHANGE, BUT THE DRUMS STILL WORK.** There is no
melodic instrument to name, so there is nothing to announce — and channel 10
needs no configuration. That asymmetry is the entire reason the drum
override exists.

### The program number was wrong the first time

I picked **Electric Bass (pick)**. That was wrong on the articulation: that
is a plucked string, and `v303a`/`v303b` are one oscillator (saw or square)
into an envelope into a VCA into a resonant ladder filter with envelope
modulation and an accent — an acid bass. Now
`RI_DEVOUT_PROGRAM_SYNTH_BASS_1` = **38** (GM 38, 1-based; data byte 37).

**WHAT A PROGRAM CHANGE CANNOT FIX, LEDGERED: GM HAS NO MONOPHONIC
CATEGORY.** Both 303 instances are a *single* voice and every GM bass is
polyphonic, so a DAW holding our program change plays a held 303 line as a
**chord**. The program change names the timbre family; the real answer to
"which synth" is whatever monophonic bass patch the user loads, which a
program change can point at and never select. Same class as the late accent.

### Mutation results

**t202 + the settings: 11 mutants, 11 killed.** Three survived a first pass
and all three were test gaps, one of them worth the whole phase:

- **`EC` — attach answers "no channel" by quietly setting it to 1.** It
  announces nothing, because the producer's claim check refuses the
  unclaimed channel downstream — so a test that only counts bytes calls it
  equivalent. It is **not** equivalent: `note_ch` is left at 1, and the very
  next melodic note would be emitted on channel 1. That is precisely the law
  R6a exists to enforce, undone by the function whose whole job is to
  configure the producer. The check has to be on what comes *after* the
  attach, not on the attach itself.
- **`ED`** — with no ring the put fails anyway, so a byte count reads zero
  whether the attach did nothing or half its job. What matters is that it did
  not leave the melodic channel **claimed** on a producer nobody is draining.
  Attach is all-or-nothing.
- **`DF`** — a negative channel was simply untested; 17 was pinned and -1 was
  not, and -1 is the one that reaches 255.

### Still open

- ~~**The melodic channel is still unset.**~~ **RESOLVED 2026-10-10 —
  LEVIASYNTH FOR MELODIES.** `midi_settings_note_ch()`: an explicit
  `note_ch` wins, otherwise the melodic output **follows the Leviasynth
  channel**, which is 2 by default. One tested function owns the rule rather
  than `riapp` glue. **Neither set is still -1** — `levi_ch`'s default is 2
  but the layer does not assume it, because defaulting here would put the
  303 on channel 1 for a settings block built by hand. Drums are unaffected:
  GM channel 10, no channel needed. **The loop worth checking:** M4 receives
  Leviasynth notes IN on this channel and R6 sends melodic notes OUT on it,
  so they are opposite directions — but a DAW that echoes channel 2 back
  into ReIncarnation's input would retrigger the Leviasynth. Patch bay, not
  code; first thing to check if the Leviasynth ever plays itself.
- **No on-panel control yet.** `RI_MIDI_SET_DEV_OUT` and
  `RI_MIDI_SET_NOTE_CH` exist in the settings layer and are wired in
  `riapp.c`; the MIDI panel does not yet expose them, which needs a ctlreg
  row and a `panelgeo` slot.
- **The ear proof is unchanged and still outstanding:** a DAW (or Live)
  following RIAPP's clock and program change, confirming the 303 line and the
  808 hits arrive where they should — and specifically that the program
  change's *timbre* is one worth keeping.

## R6g: the settings are reachable — and the on-panel claim was wrong (2026-10-10)

### I had the scope of this backwards

I had written R6g as *"the on-panel controls, which need a ctlreg row and a
panelgeo slot."* Checking before building it: **no MIDI setting has a panel
control at all.** All six siblings — channel, sync, Levi channel, clock out,
latency, MMC out — are read from ENVARC at startup and nothing else. The
MIDI panel's registered controls are CC-triggered *input* controls (note
buttons, step keys), not outbound settings.

So exposing only the R6 pair would have invented a capability under a MIDI
phase and left six siblings behind. On-panel exposure is **one piece of work
for all seven**, not two.

What R6g actually was: make the two new settings reachable the way their six
siblings are — `RIAPP_MIDI_DEVOUT` and `RIAPP_MIDI_NOTECH`. Ten lines,
through the same `midi_getnum()` the other six use, and the startup `rlog`
line now carries `devout=` and `notech=` so a boot says what it read.

### What is proven and what is not

**Proven:** the settings layer beneath both variables — strict 0/1, 0..16
with 0 meaning unassigned, refused when negative, never wrapped, never
defaulted — is pinned by t202. Both AROS ABIs (v1 and v11) compile and link
with the new ENVARC reads and the new `s_devout` producer.

**NOT proven: the ENVARC path itself.** `midi_getnum()` → `midi_settings_set`
→ `riapp` wiring is thin glue that mirrors six calls that are known to work,
and the only genuinely new risk in it is a **mistyped variable name** — a
name nobody has ever booted with. Both lanes were pinged for this and **both
timed out** (riqemu1 `spike_spool_priv` and the Dell `spike_spool_laptop`,
45 s, jobs left queued), so no lane proof was available and none is claimed.

**This is the same shape of gap as the `portable.mk` one**, and it is worth
naming because the reasoning is the tempting one: "it compiles, it links,
and the six siblings use the same function, so a typo is unlikely." Low
probability is not absence of evidence.

### The proof, when a lane is back

Thirty seconds of work, and it needs no ear and no audio:

```
setenv RAM:RIAPP_MIDI_DEVOUT=1
setenv RAM:RIAPP_MIDI_NOTECH=5
setenv RAM:RIAPP_LOG=RAM:
```

Boot RIAPP, then read `RAM:RIAPP.LOG`. The `RIAPP midi in=... devout=1
notech=5` line settles both the names and the wiring. `RIAPP_LOG=<vol>` is
what pins the log to a readable volume.

### Still open

- **On-panel exposure for all seven MIDI settings** — the real R6g follow-up,
  now correctly scoped as a single piece of work rather than two controls.
  It needs a ctlreg row, a `panelgeo` slot, and a save path; settings are
  "never in the song", so there is no song-save integration to extend
  either.
- **No runtime path.** None of the seven can be changed after startup — they
  are ENVARC-only. Turning clock out on or off means editing the ENVARC and
  restarting. That is a pre-existing property of all six, and R6 inherits
  it rather than introducing it, but it is the thing that most limits R6's
  usefulness in practice.
- **The melodic channel is still unset**, which is correct and means nothing
  melodic comes out until one is picked. Drums work with no channel at all.
- **The ear proof is unchanged:** a DAW following RIAPP's clock and program
  change, confirming the 303 line and the 808 hits arrive where they should,
  and that Synth Bass 1 is a timbre worth keeping.

## "Leviasynth for melodies" — the melodic channel, and the combined proof (2026-10-10)

Owner instruction, 2026-10-10: *"do the M4 ear proof. combine with lamp
position if possible. Leviasynth for melodies. same rules."*

### The melodic channel is settled

`midi_settings_note_ch()` — an explicit `RIAPP_MIDI_NOTECH` wins, otherwise
the melodic output **follows the Leviasynth channel** (2 by default). One
tested function owns the rule rather than `riapp` glue, because the failure
mode of putting it in glue is a second place to be wrong about it.
**Neither set is still -1**: `levi_ch`'s default is 2, but the layer does
not *assume* it, because defaulting there would put the 303 on channel 1 for
a settings block built by hand rather than by `midi_settings_defaults()`.

**THE LOOP WORTH CHECKING FIRST IF THE LEVIASYNTH EVER PLAYS ITSELF:** M4
receives Leviasynth notes IN on this channel and R6 sends melodic notes OUT
on it. Opposite directions, so they do not collide — but a DAW that echoes
channel 2 back into ReIncarnation's input would retrigger. That is the
patch bay, not this code.

**AND THE PART WORTH SAYING PLAINLY:** the R6 melodic source is the **303**,
not the Leviasynth. `RIEvent.device` 0/1 is 303A/303B. What lands on
channel 2 is *303 content played by whatever patch the receiver has there*.
"Leviasynth for melodies" names the **channel**, not the voice.

### The lamp: I nearly claimed it did not exist

Looking for the clock-out lamp's draw site, I searched for `RI_STR_*` and
`clk_out` across `gui/` and `app/` and found only `secttr.c` — the state —
plus `sectproof.c`. By that evidence the conclusion was "**there is no draw
site; the lamp is never rendered**", which would have gone in a commit.

It is wrong. The lamp is fully wired: a `R(TRANSPORT, 15, LED, "", "Clock
Out", ...)` row in `gui/ctlreg.c` and
`{ TR(15), RI_GEO_RECT, 0, 253, 40, 12, 12 }` in `gui/panelgeo.c`. My
search missed both because neither file mentions `RI_STR_CLKOUT` — the
ctlreg row is a literal `15` and the geometry is a `TR(15)` macro.

**Worth recording as a method note:** the state layer and the render layer
share no names, so "I cannot find where it is drawn" is not evidence that it
is not drawn. The finding that would have been reported was produced by a
search, not by a check.

### The lamp's position, and the question it needs

Placed by arithmetic and never seen: between Sync (188, 40) and MIDI In
(318, 40) on the row the manual puts the other two indicator lamps (p. 144–145),
x=253 splitting that span, y=40 otherwise empty between them.

The question is not "is it in a sensible place" — it is **"does it read as
'the third lamp', or as clutter?"** Either answer moves it.

And the test has a **negative half** that matters more than the positive:
`ri_str_clkout_set(s, sending, sent_total)` is `(sending && sent_total > 0)`
and is deliberately unreachable from `ri_str_indicator_set()`. So the lamp
must be **dark before the sender starts and lit while bytes flow**. A lamp
that glows when nothing is being sent is worse than no lamp, because it
reports a wire that is not there.

### The combined run is prepared and NOT staged

`docs/evidence/midi/m4-levi-r6.run` and `m4-levi-r6-proof.md`. One session,
three questions, sharing a boot, two processes and a channel:

- **A — M4 inbound**, the 2026-09 script unchanged.
- **B — R6 outbound**, the first run that puts R6d's drain on a wire.
- **C — the lamp**, which **B is what makes light**.

**BLOCKED ON THE LANE, NOT ON THE WORK.** Both lanes were pinged twice and
**both timed out** every time (`spike_spool_priv`, `spike_spool_laptop`, 30 s
and 45 s). Nothing is deployed; the binaries are built and their hashes are
recorded so a PUT can be checked against them. Per the standing rule the
owner runs the ear part and I read the counters.

### Still open

- **The combined run**, on the lane. The Dell is the only lane with real
  audio; riqemu1 cannot answer C.
- **R6h** — on-panel exposure for all seven MIDI settings.
- **No runtime path** for any MIDI setting (ENVARC-only, restart to change).
- **R8b**, **R9**, **R5**.

## R8b: the per-strip tap (2026-10-10)

`ri_mix_render` took five bus inputs and returned ONE output, so a stem
could not be produced without rendering the whole song once per strip — and,
worse, without knowing what a strip *is*. This adds the tap, and the tap's
definition is the whole design.

- **A STRIP IS EXACTLY WHAT THE MIX ADDS.** The tap writes the same
  `bus_in[b][i] * applied[b]` product the accumulator is summing, not a
  recomputation of it. **The five stems, summed at the master gain,
  reconstruct the mix BIT-IDENTICALLY** — checkable with `==`, because
  float addition is not associative and only the mix's own bus order
  reproduces its rounding. `SB` (post-master), `SH` (solo not reaching the
  tap), `SC` (muted strip still writing) and `SA` (recomputing the gain)
  are all killed by that one law.
- **PRE-MASTER.** The master fader is a mix decision, not a strip property;
  baking it into five stems means the user cannot re-mix without applying it
  five times. t203 changes the master and asserts **no stem moves**.
- **INCLUDES THE SLEW, DELIBERATELY.** `applied[]` ramps over
  `RI_MIX_RAMP_SMP` (64) samples so a fader move does not click. Recomputing
  the strip from the fader's *target* would leave the stems not summing to
  the mix for the first 64 samples of every fader move — which is exactly
  where a stem's head is. t203 pins the invariant **through the ramp**, not
  only after it.
- **A BUS WITH NO INPUT MUST WRITE SILENCE, NOT LEAVE ITS STEM ALONE.**
  Skipping the write leaves the previous render's samples: a stem file of
  stale audio that nobody edited and everybody believes. One explicit
  `0.0f` in the mixer (`SD`).

### Mutation results — and a false kill that was not caught for two runs

**9 mutants, 7 killed, 2 equivalent.**

- **A HARNESS THAT INVENTED ITS OWN TESTS.** I listed `t205_mixer_render`
  and `t206_mixer_render` as regression tests. **They do not exist.**
  `subprocess` returns non-zero on a missing binary and the harness read that
  as KILLED — **a false kill, the exact inverse of the segfault that was once
  mis-scored as SURVIVED.** It went unnoticed for two full runs because
  those runs looked perfect. The harness now refuses to start unless every
  named test exists on disk. A missing test is not a failing test; it is a
  broken harness, and it is indistinguishable from success unless you check.
- **`SI` is equivalent by construction** — it returns the **identical object
  hash**: the compiler already CSEs `bus_in[b][i] * cur` into `v`.
- **`SG` is equivalent and it corrected a comment.** A mutant that
  reimplemented `ri_mix_render` outright, same math and same accumulate
  order, **survives** — the two are byte-identical for a NULL tap. So
  "one implementation" is a **maintenance argument, not a testable law**,
  and my test comment claimed otherwise. Corrected in place, because a
  comment asserting more than its evidence outlives the evidence.

### Four test gaps, three of them my own mistakes

- **`SD` passed against the mutant it was written for.** The stale buffer
  happened to hold *silence* — the previous section had soloed bus 2 out —
  and a stale buffer only proves anything when it holds something.
- **A NULL TAP ENTRY AND A NULL BUS INPUT ARE DIFFERENT THINGS, and I got
  this wrong twice.** `strips[b] = 0` means "do not RECORD bus b", which
  correctly leaves its stem alone — there is no claim about what an
  unrecorded stem contains. The case needing an explicit zero is a bus the
  mix is not *receiving*, which is an entry in **`bus_in`**, not in
  `strip_out`. Both wrong versions were still passing.
- **The slew section measured nothing.** It called `settle()` — four full
  blocks, 2048 samples — which walks straight past the 64-sample ramp the
  whole section exists to examine. Then its second version rendered `N`
  samples again, which settles the ramp for the same reason. Only a **16**-
  sample block observes it.
- **And the per-sample sum law was wrong by construction.** `out[i]` uses
  the master value *at sample i*, which a caller cannot see, so checking a
  per-sample sum against the block's final master is invalid — it failed on
  the pristine build. Settling the master alone and re-zeroing `applied[]`
  (a plain struct field, so a legal setup) makes the gain a known constant
  and the law checkable at every sample, head included.

Each of these is the same shape: **a test that passes without proving the
thing it names.** Three of the four were only visible because a mutant
survived that should not have.

### Still open

- **R8b proper — running the engine per stem.** This is the tap and the set
  integration; the loop that renders a song and hands five strip buffers to
  `stem_set_add` is not written.
- **R9** — loop-exact renders with an optional tail, which is where the
  per-strip render has to prove it does not change a note's timing.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane (both lanes
  timed out on ping for the third time this session).

## R8c attempt: the app does not use the mixer, and out-of-order events overrun (2026-10-10)

Two findings. **One changes what R8 is; the other is a memory-corrupting
defect.** The stem tap itself is **NOT landed** — see the end for why.

### FINDING 1: `ri_mix_render` IS NOT THE MIX THE APP USES

R8b put a per-strip tap on `ri_mix_render`. Searching for its callers
outside tests turns up exactly two: `tools/bench.c` and `tools/render.c`.
**`engine/mixer/mixer.c` is linked into both AROS ABIs and into the portable
build and has no application caller at all.**

The app mixes in `engine_section()`, which applies the strip fader **in
place** to `scratch`/`scratchR` and accumulates `scratch[i] * gl` into a
**double** master. The Levi has its own copy, `engine_section_stereo`.

So R8b's tap is correct for the portable/tools mixer and is **not the stem
path**. That is worth having found before R8c built a renderer on top of it.

**METHOD NOTE.** I nearly asserted the opposite — that stems could not be
produced because the tap was on the wrong path — from a grep that searched
`gui/` and `app/` for `RI_STR_*`. A search that finds nothing is evidence
about the *search*, and the render layer and the state layer share no names.
The positive evidence was simple: grep every `.c` for `ri_mix_render` and
read the list.

### FINDING 2: out-of-order events overrun the output buffer

Found by my own test, while building R8c's fixture. I wrote events with
samples **0, 512, 256** — unsorted — and the process **segfaulted inside
`ri_engine_render`**. From the source:

    next = e->total;
    if (e->evpos < e->nev && e->ev[e->evpos].sample < next)
        next = e->ev[e->evpos].sample;
    ...
    run = next - e->cursor;          /* unsigned */

At cursor 512 the next event's sample is 256, so **`run = 256 - 512`
underflows to about 2^64**, the slice loop runs effectively forever, and it
writes `out_l[done + c + i]` far past the caller's buffer. Not a wrong
answer — a memory-corrupting overrun, and on the render task a crash.

**REACHABLE?** Not from the live path: the scheduler emits sample-sorted and
that is `RIEvent`'s contract. **But R8 and R9 build event arrays BY HAND** —
that is exactly what an offline stem or loop render does — so the first work
to assemble events itself is the first work that can get this wrong. And the
law everywhere else here is *refused, never guessed*.

The guard compares and **stops**, counting `ev_unsorted` and returning what
was really rendered. It does **not** sort: reordering the caller's array would
render music the caller did not describe. An event exactly **at** the cursor
is not behind it — that is the ordinary zero-length run most note-ons arrive
as, and refusing those would refuse most music.

### Mutation results

**6 mutants, 5 killed, 1 equivalent.**

- **`UC` is equivalent by an invariant**: returning `n` instead of `done` at
  the guard is identical, because `done + n == total` holds at that point.
  Worth stating rather than treating as a gap.
- `UD` (an event at the cursor treated as behind it), `UA` (no guard), `UB`
  (not counted), `UE` (`ri_engine_init` never resets the complaint) and `UF`
  (counts it and renders on anyway — silent corruption instead of a stop) are
  all killed.

### Why the stem tap did NOT land

It works — I verified `SL[0][0] == L[0]` exactly — but three things came out
of it that are not yet resolved, and landing a half-verified tap on the
audio path is the wrong trade:

1. **I had a real offset bug in my own tap.** `ml`/`mr` are block-local and
   indexed from 0; the offset is applied once at `out_l[done + c + i]`. My
   tap wrote `tl[i]` for every slice, i.e. every slice to the head of the
   buffer. Fixed by threading `pos` through `engine_section` **and**
   `engine_section_stereo` — the Levi is section 4 with its own accumulator,
   so a tap added only to `engine_section` would have left the fifth stem as
   a file of the caller's fill.
2. **`e->scratchR` is never written for a mono section.** `rb303_render`
   fills `scratch` only, so the R stem for the 303s carries whatever was in
   `scratchR` last. The master sums it too, so the tap is *faithful* — which
   is precisely the problem: a faithful tap of an uninitialised buffer
   exports stale audio. **This is a pre-existing engine question and it is
   not mine to settle here.**
3. **The sum law has to be weaker than R8b's.** The master accumulates in
   `double` and stem buffers are `float`, so five narrowed stems cannot
   reproduce a double sum. R8b's mixer tap *is* bit-exact because that mixer
   accumulates in `float`. A tolerance is the law this code can keep; an
   equality is not, and pretending otherwise would be a law that breaks on
   the first platform with different float behaviour.

### Still open

- **R8c proper** — the stem tap, once (2) and (3) have answers. Not a
  rewrite: the shape works and the offset bug is understood.
- **The `scratchR` question**, which is its own piece of work and predates
  R8: does a mono section's right output belong to be stale?
- **R9** — loop-exact renders with an optional tail.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane (both lanes
  timed out on ping for the fourth time this session).

## RETRACTION: finding 2 of the R8c slice was wrong (2026-10-10)

**I reported a defect that does not exist.** This replaces, and withdraws,
item 2 of the R8c attempt above.

> *~~`e->scratchR` is never written for a mono section. `rb303_render` fills
> `scratch` only, so the R stem for the 303s carries whatever was in
> `scratchR` last. The master sums it too, so the tap is faithful — which is
> precisely the problem: a faithful tap of an uninitialised buffer exports
> stale audio.~~*

**There is nothing to fix.** The mono path does not merely fail to *write*
`scratchR` — it never *reads* it, so its contents cannot reach the output, the
send or the section meter however stale they are.

### Why I got it wrong

`engine.c` holds two near-identical static functions 60 lines apart:
`engine_section` (mono, sections 0–3) and `engine_section_stereo` (the Levi,
section 4). **They take the same parameter list**, and their accumulates
differ in exactly one token:

    engine_section:          double s = e->scratch[i];
                             ml[i] += s * gl;   mr[i] += s * gr;

    engine_section_stereo:   mr[i] += (double)e->scratchR[i] * gr;

I read the stereo one and attributed it to the mono one. The mono function
meters `scratch` directly, sends `scratch`, and **mirrors the mono sample to
both outputs deliberately** — so a 303 on its own is centred, not left-only.

### What the probe had already said, and I read past three times

Planting `1.0e30f` into every `scratchR` slot and re-rendering changed
**nothing** — bit-identical `out_l` and `out_r`. That result was on screen and
I explained it away as "the linked object is stale", compiled a fresh
`engine.o`, got the same answer, and moved on. The sentinel showing no effect
*was* the finding. A probe result that contradicts a claim should end the
investigation, not generate a better explanation for the claim.

It survived a segfault hunt, four probes, a `nm` sweep for duplicate symbols,
and a grep. What would have settled it in one step is the boring check I did
last: print the **values** the probe observed next to the **expression** the
source has. `scratchR[0] = 0` versus `mr[i] += scratchR[i] * gr` is a
contradiction you can see without any theory.

### t206: the law that was true all along, now pinned

- **A MONO SECTION MIRRORS TO BOTH OUTPUTS** — `mr += s * gr`, not
  `mr += scratchR * gr`. A 303-only render is centred; `memcmp(L, R) == 0`.
- **THE MONO PATH IS IMMUNE TO `scratchR`.** Plant `1e30` there and the output
  is bit-identical — for the 303A, the 303B, the 808, the 909, and with a
  stereo section present in the same block. **This is the property that made
  the R8c stem tap safe, and the property my claim denied.**
- **THE MONO SECTION METER READS `scratch`, NOT THE AVERAGE.** The stereo path
  meters `0.5 * (scratch + scratchR)`; mixing the two up is exactly what made
  the original claim look reasonable.
- **THE LEVI FILLS `scratchR` BEFORE READING IT** — even with no note at all,
  so the one path that does read it is not exposed either.

### Mutation results

**5 mutants, 4 killed, 1 survived.**

- **`SA` is the defect I claimed, and it is killed** — making the mono path
  read `scratchR` into the right output fails t206 immediately. That is the
  evidence the claim described a real change that t206 can see.
- `SB` (mono stops being centred), `SC` (mono meter switched to the stereo
  average) and `SE` (`scratchR` leaking into the mono send) are killed.
- **`SD` survives and correctly so**: silencing the mono send is out of scope
  for a test about `scratchR`. That is a gap in a *different* test's remit,
  not a hole in these laws, and it is recorded rather than absorbed.

### What this changes about R8c — and what is NOT a blocker

Of the three blockers I listed, **one is now resolved and one was never a
blocker**:

1. **My offset bug** — real and understood. **AND NOT CURRENTLY IN THE
   TREE.** I wrote the fix, verified it, and then reverted the whole tap
   along with it; an earlier version of this ledger called it "fixed" and
   that was false. Re-applying it is part of the work, not a completed step:
   `pos` has to be threaded through **both** `engine_section` **and**
   `engine_section_stereo`, because the Levi is section 4 with its own
   accumulator and a tap on only one of them leaves the fifth stem as a file
   of the caller's fill.
2. ~~`scratchR` staleness~~ — **withdrawn; there is no staleness.**
3. **The sum law must be a tolerance, not an equality** — stands. The master
   accumulates in `double` and stems are `float`.

So R8c is blocked on **one** thing, not three, and it is a statement about
arithmetic rather than a defect in the engine.

### AND THE "BLOCKER" IS NOT A BLOCKER

I listed the sum law as the one thing R8c is blocked on. It is not. It is a
decision about what the test asserts and what the ledger claims, and it
settles in a sentence: **the stems reconstruct the mix to float accumulation
precision, and that is far below the noise floor of every format the stems
are written in.**

Five terms summed as `float` against a `double` master differ by about
6e-8 relative — roughly -144 dBFS. A 16-bit PCM stem quantises at about
-96 dBFS and a 24-bit one at about -144 dBFS. So for the only thing anyone
does with stems — re-mixing them in a DAW — the error is **at or below the
quantisation of the file itself** and is not a limitation anybody can hear.

Which means R8c is not blocked on anything. What is left is work I have not
done: re-apply the tap with the `pos` fix, pin the tolerance, and gate it.
The correct next move is to do that, not to ask a question.
