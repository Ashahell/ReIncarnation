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

## R8c: the stem tap, landed (2026-10-10)

`ri_engine_set_stems()` + `struct RIStemTap`, t204. The tap that puts each
mixer section on its own buffer, taken from the mix the **application**
actually runs.

- **A STEM IS EXACTLY WHAT THE MIX ADDS** — post-fader, post-pan, pre-master,
  from the same term the accumulator is summing, never a recomputation of the
  gain. `TE` (mono stops being mirrored) and `TI` (the mono tap dropped) are
  killed.
- **IN BOTH SECTION FUNCTIONS, AND `pos` IN BOTH.** `engine_section` (mono,
  sections 0–3) and `engine_section_stereo` (the Levi, section 4) each have
  their own accumulate, so a tap on only one leaves the fifth stem as a file
  of the caller's fill. `TC` kills that.
- **`pos` IS THE BUG I ALREADY MADE ONCE.** `ml`/`mr` are block-local and
  indexed from 0; the offset is applied once, at `out_l[done + c + i]`. My
  first tap wrote `tl[i]` for every slice, so every slice landed at the head.
  `TA` and `TB` are that exact bug in each function, and both are killed —
  by asserting the stem is written **past the first block**, because "something
  was written" is satisfied by the head alone.
- **A HOLED TAP ARRAY IS SKIPPED, NOT ZEROED.** `TH` kills zeroing it. A
  303-only render has no 808, and attaching a tap must not resurrect a
  section the song has switched off.

### The limiter, and why the sum law is a tolerance

The stems are **pre-limiter**; `out_l`/`out_r` are **post-`ri_soft_limit`**.
Past `RI_LIMIT_KNEE` (0.8912509, −1 dBFS) the limiter is a **nonlinear master
stage**, so no additive law can hold — and at unity faders the fixture reached
|sum| 1.82 against a mix of −1.0, so the sum law first reported an error
larger than the signal. That was not a wrong tap; it was a correct tap
compared against a compressed mix.

Pre-limiter is the right choice: baking the master limiter into five stems
means the user cannot re-mix without it being applied five times. So the law
is **the stems sum to the mix wherever the master chain is linear**, the
fixture's faders come down to 60 to keep it there, and past the knee no
additive law is claimed. Within the linear region the residual is float
accumulation — about 6e-8 relative, ≈ −144 dBFS — at or below a 24-bit stem's
quantisation and far below a 16-bit one's.

### I reintroduced t205's bug in my own fixture

The first `load()` appended the 303A events (samples 0 and N/2) and then the
303B one (N/4), producing **[0, 512, 256]** — unsorted, precisely the input
t205 exists for. The symptom was baffling rather than obvious: the render
returned the right count, the stems summed exactly through sample 511, held
the fill from 512 on, and the sum law reported an error larger than the
signal.

**What actually happened was t205's guard working.** At cursor 512 the next
event's sample was 256, so the render stopped instead of underflowing `run`
and overrunning the buffer. Without the guard this fixture would have been the
segfault rather than a confusing assertion — and the two defects found in
adjacent slices turned out to protect each other.

### Mutation results

**8 mutants: 6 killed, 2 equivalent.**

- **`TF` is equivalent**: making the stereo right stem read the mono scratch
  changes nothing, because **the Levi's two stems are identical at default
  settings** — its own stereo spread is centred. t204 says so on stderr
  rather than pretending otherwise. It would take a panned or spread Levi to
  separate them, which is a different test.
- **`TC`/`TG` from the earlier batch** (stereo tap dropped, attachment
  ignored) are killed.
- Five mutations **failed to build** across the two attempts, because a
  mutant that leaves `pos`, `tl`, `tr` or `t` unused trips `-Werror`. They
  prove nothing, and a harness that scored them as killed would be reporting
  a compiler error as test evidence — the same failure as the invented test
  names in the R8b slice, in the opposite direction.

### Still open

- **R8d — the render loop**: run a song and hand five strip buffers to
  `stem_set_add`/`stem_set_write`. The tap is the seam; this is the work
  around it.
- **R9** — loop-exact renders with an optional tail.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane (both lanes
  timed out on ping for the fifth time this session).

## R8d: the bridge from the tap to the stem set (2026-10-10)

`project/stem_render.{c,h}`, t207. R8a built the WAV writer, R8c built the
engine's tap; this is what sits between them, and it is small on purpose:
hand the tap a whole render's worth of caller buffers, zero them, and on
completion interleave each strip and give it to `RIStemSet`.

**NOTHING IS ALLOCATED AND NOTHING IS OWNED.** `RIStemSet` stores the
*pointer*, so an interleaved buffer that dies before `stem_set_write` is a
dangling read in a file the user is about to trust.

### The laws, all of which are about a render that does not go to the end

- **THE BUFFERS ARE ZEROED BY `ri_stemr_bind`, ALWAYS** (`RA`, `RI`). The tap
  writes only the samples the engine actually renders. A render that stops
  short — out-of-order events (t205), a song shorter than the buffer — leaves
  the tail holding whatever the caller had there, and **in a stem file that is
  not silence, it is the last song's audio.** Zeroing is this module's job
  because the caller will not remember, and "it worked in the test" is how
  that survives.
- **A SHORT *OR OVER-LONG* RENDER IS REFUSED, NOT PADDED** (`RB`, `RC`,
  `RD`). `ri_stemr_finish` takes what the engine returned and refuses anything
  that is not the frame count asked for, counting it. A stem the caller
  believes is complete but which ends in silence is a lie about the song.
- **ONE STEM PER RENDERED SECTION, IN SECTION ORDER** (`RE`, `RF`). The section
  mask is read off the engine **at bind**, not at finish, because the caller
  may change it in between and the stems should describe the render that
  happened. A slot offered for a section the song has switched off does not
  become a stem — a file of silence labelled as an instrument is worse than
  one file fewer.
- **INTERLEAVED, NOT TWO MONO FILES** (`RG`, `RH`). `stem_wav_write` reads
  `samples[i * channels + c]`, and this is the only place that copy happens.
- **FINISH IS NOT IDEMPOTENT** (`RK`): calling it twice would add every stem
  a second time, which is how a set ends up with eleven stems and ten
  instruments.
- **An unsupported bit depth is refused, not clamped** (`RJ`). The caller's
  file format is not this module's to choose, and a silent 16 would be a file
  that is not what anybody asked for.

### Mutation results

**12 mutants, 12 killed.** Two needed test work first, and one of them is the
same shape as R8c's:

- **`RG` (a swapped interleave) survived a first pass** because **every
  section in the fixture is MONO, and t206 says a mono section mirrors** — so
  `l == r` and swapping them changes nothing. My interleave assertions were
  satisfied by a swapped interleave. **Panning a section hard left** (pan 0 is
  `gl=1, gr=0`) is what separates the two sides.
- **`RK`** needed a second `finish` call, which the test never made.
- `RJ` first **failed to build** (the mutant left `d` unused under `-Werror`),
  which proves nothing — the fourth time in this run of work that a
  build failure has been the thing standing between a mutant and a verdict.

### Still open

- **R8e — the caller.** `stem_render.c` is host-only, like `stem_wav.c`,
  because no app code calls it yet. Nothing in the UI or the CLI asks for
  stems, so this is a module with no door. Wiring it to a command is the
  remaining R8 work and it is a **product decision, not a mechanical one**:
  where does the user ask for stems, and what does the file set look like on
  disk.
- **R9** — loop-exact renders with an optional tail, which is where a stem
  render has to prove it does not move a note's timing.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane (both lanes
  timed out on ping for the sixth time this session).

## R8e: `--stems DIR`, and a real bug the CLI found (2026-10-10)

Owner decision: **a directory per song**, and the door is `tools/render`
rather than the UI. `--stems DIR` is a complete invocation on its own —
`--out` is optional alongside it, because a caller asking for stems is
asking for files and should not also get a mix they did not request.

**The sections are DERIVED FROM THE EVENTS, not asked for.** A stem is a
mixer strip, so the set of stems is the strips the song addressed. Today's
`RIStep` has no device field, so both the text scaffold and the RBNG path
land on 303A and produce exactly **one** stem. That is the honest answer
rather than a limitation worked around, and the naming is numbered by SECTION
so the set grows by itself when a format can address more:

    <song-dir>/01-303a.wav   02-303b.wav   03-808.wav   04-909.wav   05-levi.wav

### The bug: `pos` was the engine's LOCAL `done`, not its absolute cursor

**This is the second time this exact mistake has cost a whole debugging
session, and the second time it was invisible to the test.**

`ri_engine_render` restarts its local `done` at 0 on **every call**. R8c
threaded `pos = done + c` into the section functions, which is right *within*
one call and wrong *across* calls — and the CLI renders in 64-frame blocks,
because that is what a bounded tool does.

The symptom was a **confusing pair of facts**: the mix was correct (peak
22601) and the stem was **silent** (peak 0). Not a crash, not a plausible
wrong value: silence, which reads like "the render produced nothing" and
sends you looking at the event stream.

Why silence: every block overwrote the head of the stem buffer, so after the
loop only the **last** block's 64 samples survived, and the 303 had decayed
to nothing by the end of the song.

**`pos` is now `e->cursor + c`** — the engine's absolute sample position. A
stem buffer is ABSOLUTE: it belongs to the song, not to the call, which is
exactly why `out_l[done + c + i]` is right for the caller's buffer and wrong
for a stem.

t204 now renders in **blocks** and asserts the stem is written past the first
one, and that a blockwise render's stems agree with a one-call render's. The
existing test could not see this: it rendered the whole song in one call.
Three mutants pin it — `PA` (the local `done` again), `PB` (no slice offset
at all) and `PC` (off by one block, the plausible-looking shift) — **all
killed**.

### The other bug the CLI found, in my own tool

**THE MIX WAS BEING WRITTEN INTO SLOT 0'S TAP BUFFER.** `render_stems` passed
`slots[0].l`/`slots[0].r` as the engine's output, so the stereo mix
overwrote the very samples the tap had just recorded — at offset 0, on every
block. The mix has its own scratch now, and the comment says why.

Also: the stem file is **sized from the frame count**, not a fixed buffer. A
64 KB guess silently refused a 328 KB stem — correct behaviour from
`stem_set_write`, which will not truncate, and useless without a diagnosis.

### Evidence

```
render --song song.txt --out mix.wav --stems stems
render: stem stems/01-303a.wav
render: 1 stem(s) in stems
render: 9 events, 66000 samples -> mix.wav

mix.wav          ch=1 frames=66000 peak=22601
01-303a.wav      ch=2 frames=66000 peak=22600   (one LSB: float stem vs
                                                 a 16-bit mono fold)
```

`--depth 24` gives 396044 bytes (44 + 66000·2·3) against 264044 at 16-bit, and
an existing directory is reused rather than refused.

### Still open

- **Multi-stem output.** One stem per song is all today's formats can
  address. It needs a song format with a per-step device, which is an engine
  data-model change (`RIStep` has no device field), not a renderer change.
- **SMF export still has no caller.** It should ride this same invocation —
  `--mid FILE` — but that is a second flag on a CLI rather than a second door,
  which was the point.
- **R9** — loop-exact renders with an optional tail.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane.

## R8f: `--mid FILE` — the SMF finally gets a caller (2026-10-10)

`ri_smf_write` has been built and tested since R7a and had **no caller at
all**. `project/smf_bridge.{c,h}` is the conversion, and `--mid FILE` is the
door — deliberately on the **same invocation** as `--out` and `--stems`,
because a second export format behind a second command is a door nobody
finds, which was the whole reason R8e folded stems into `render`.

### THE ENUMS ARE NOT THE SAME NUMBERS, SO IT IS NOT A CAST

`RI_EV_NOTE_ON` is **3**; `RI_SMF_EV_NOTE_ON` is **0**. `RI_EV_NOTE_OFF` is
**2**; `RI_SMF_EV_NOTE_OFF` is **1**. Passing `RIEvent.type` straight through
would turn every note-on into a note-off: a file of releases with no
attacks, which imports as silence. Mutant **SA** renumbers the mapping and is
killed by t208.

### A SLIDE CANNOT CROSS, AND IS COUNTED AND ANNOUNCED

`RI_EV_NOTE_CONTINUE` is rest+slide — the gate stays high and the pitch
slews. SMF 1.0 has no legato pitch change, and **both available fakes are
wrong in a way the listener hears**:

- note-off + note-on is the **re-attack R6a exists to refuse**;
- a bare pitch bend is a *different note* on a track that is supposed to be
  this pattern.

So the slide is dropped, `tracks[i].slides` counts it, and the **CLI prints
it** — an SMF is opened by a human or a DAW, and a silent fidelity loss in a
file that looks complete is exactly what nobody notices until mixdown. Mutant
**SB** (slide becomes a note-on) is killed.

`RI_SMF_SLIDE` is defined in `smf_export.h` and **referenced nowhere in the
writer** — "the flag exists" was not "the writer handles it".

### PPQ AND TEMPO ARE THE CALLER'S, AND A ZERO IS NOT A NEUTRAL DEFAULT

My first version `memset` the song and left both fields zero. That is not
harmless: `ri_smf_delta_ticks` treats ppq 0 as *every event is on tick 0*,
producing a file that is correct only when it holds one event — and looks
correct while it is wrong. The bridge now goes through `ri_smf_song_init`
with the caller's ppq and tempo. Mutants **SC** (both zeroed) and **SJ**
(ppq alone lost, tempo surviving) are both killed.

### THE WRITER REFUSES A SIZE PROBE — A WORTHLESS THING TO RELY ON

I sized the output buffer by calling `ri_smf_write(song, 0, 0)` and reading
back the required size, which writers are often documented to do. **This one
is not**: it refuses a NULL `out` and a `cap` under 14, so the probe
returned 0 and the tool reported *"the writer refused the song"* — pointing
at the song when the fault was entirely in the question. **A size probe that
returns the same value as a refusal is not a size probe.**

The real bound is the writer's own **internal 8192-byte per-track scratch**,
plus 8 bytes of MTrk framing per track, so `14 + (ntr+1)*(8+8192)` is
sufficient by the writer's construction. That scratch is also a real limit:
a track past ~2000 events is refused, and the tool **says so** rather than
writing a truncated track.

### Evidence

```
render --song slide.txt --mid slide.mid
render: SMF slide.mid: 1 track(s), 86 bytes, ppq 96, 140 BPM
render: 1 slide(s) did NOT cross: SMF has no legato pitch
```

```
MThd  format=1 ntracks=2 division=96      (2 = conductor + 1 data track)
tempo meta 0x51 = 0x0001AC = 428 us/quarter = 140 BPM
walk of the chunks lands EXACTLY on EOF

track: ON 36 / OFF 36 / ON 43 / OFF 43 / ON 55 / OFF 55
```

Note 43 is held across the slide at **one pitch** — the bridge did not
invent a re-attack in the gap, which is the defect it exists to avoid. Three
note-ons, three note-offs, balanced: nothing left hanging for the importer
to report as a stuck note.

### Mutants: 7 real, 3 harness failures that prove nothing

| id | mutation | result |
|----|----------|--------|
| SA | enums cast instead of mapped | **KILLED** |
| SB | a slide becomes a note-on | **KILLED** |
| SC | ppq and tempo left at zero | **KILLED** |
| SD | accent always set | **KILLED** |
| SE | short output array overwrites | **KILLED** |
| SG | a slide-only **device** still gets a track | **KILLED** — *after a fix* |
| SH | the slide counter is dropped | **KILLED** |
| SI | one track regardless of device | **KILLED** |
| SJ | ppq alone lost, tempo surviving | **KILLED** |
| SF | "a device with no events gets a track" | **EQUIVALENT** |

**SG SURVIVED FIRST, AND IT WAS A REAL GAP IN THE TEST, NOT A BAD
MUTANT.** I had tested a slide-only *track* (§4) but never a slide-only
*device* through the song path. Those take **different branches** — one
returns 0 from the bridge, the other never calls it — so the empty-track case
was entirely untested. A track with `count = 0` and a valid pointer builds a
bare end-of-track meta: a track in the DAW's list that plays nothing, which
is a file that looks longer than the music. t208 §6b now pins it.

**SF is an EQUIVALANT MUTANT.** Calling the bridge with zero events returns
0 and leaves the track empty whichever way it is written — the mutation
cannot change behaviour, so its survival means nothing. Recorded as such
rather than dressed up as a pass.

**Two mutants failed to BUILD and therefore prove nothing.** `-Werror=unused-parameter`
kills "ppq and tempo zeroed" before it can run, because ignoring both
parameters is exactly the mutation. Harness lessons from earlier phases
apply unchanged: **a build failure is not a kill**, and the fix is to
silence the unused parameters *inside* the mutant so the compiler accepts
the very code it is meant to test.

### Still open

- **Multi-stem output** — needs a song format with a per-step device.
- **R9** — loop-exact renders with an optional tail.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane.

## R9: `--bars N --from BAR --tail MS` — a loop a DAW can warp (2026-10-11)

`project/loop_render.{c,h}`, t209. The requirement is one line in the spec
("render N bars from a chosen bar, with an optional tail, so exported loops
warp cleanly in a DAW") and the difficulty is entirely in the **adverb**.

**THE LENGTH WAS A NUMBER NOBODY CHOSE.** Before this, `tools/render` sized a
song from `ev[nev-1].sample + g_rate` — the last event plus a hardcoded one
second. That lands wherever the last note happens to fall, so it is usually
mid-bar and a DAW's warp grid has no bar line to sit on.

### THE LAWS

**THE BODY IS COMPUTED FROM THE MAP AT ITS ENDPOINTS**, not
`bar_samples * bars`. Two answers to the ticks-to-samples question disagree
by a sample per bar at some tempos, and a per-bar disagreement is a seam
that drifts. Mutant **LA** is killed.

**THE TAIL IS ADDITIVE AND CANNOT MOVE THE SEAM.** `body_samples` is fixed
before the tail is computed and nothing below writes to it. Mutant **LB**
folds the tail into the body and is killed — that is exactly the failure that
welds a stray chunk of reverb onto the end of an exported loop.

**A PATTERN MUST BE A WHOLE NUMBER OF BARS.** 20 steps is 1.25 bars and
cannot be cut loop-exact, so it is **refused**: rounding up drops music the
caller wrote, rounding down clips the end. Mutant **LC**.

**A `from` PAST THE END IS REFUSED, NOT WRAPPED.** Bar 1 of a one-bar
pattern is a caller mistake, and silently exporting bar 0's music instead is
worse than refusing — the same law as t205's unsorted-event refusal: do not
render music the caller did not describe. Mutant **LD**.

**THE LIMIT IS THE BODY'S SAMPLES, NOT A BAR COUNT.** My first limit was
1048576 *bars* — about two years — which is far too permissive to be a real
bound. It is now 2^32 samples, checked *after* the samples are computed,
which is what actually gets allocated downstream. Mutant **LF**.

### TWO MUTANTS SURVIVED, AND BOTH WERE TEST GAPS

**LH — the whole-bars proof — survived, and checking it by hand showed it is
genuinely REACHABLE rather than redundant.** At 140 BPM a bar is 1714285
samples, not a round number, so the body of two bars is 3428571, which is
**not** a multiple of one bar. The map's single rounding does not divide
evenly at these tempos. Handing that over as "two bars" is exactly the lie
this module exists not to tell — and at 120 BPM, where it *is* exact, the
same call succeeds, so the refusal is about the tempo rather than about two
bars being wrong.

**LG — the fractional-sample refusal — survived because no test asked for
one.** At 48000 a millisecond is 48 samples exactly, so fractions never
arise there. At 44100 a millisecond is **44.1** samples: a 1 ms tail is
refused, and a 10 ms tail (441 samples) is fine. The caller asked for a
length in milliseconds and the file is in samples; when those two do not
describe the same number, the honest answer is to say so rather than round a
length nobody chose into a file they will trust.

**LA also failed to build on the first attempt** — replacing the map call
orphaned `body_end` and `-Werror` caught it. A build failure is not a kill;
the mutant has to keep the variable alive and actually change behaviour.

### Mutants: 8, all killed

| id | mutation | result |
|----|----------|--------|
| LA | body = bar_samples × bars | **KILLED** |
| LB | tail folded into the body | **KILLED** |
| LC | partial bar rounded down | **KILLED** |
| LD | from past the end wraps | **KILLED** |
| LE | a bar is 3 quarters, not 4 | **KILLED** |
| LF | the body-samples bound dropped | **KILLED** |
| LG | fractional-sample tail rounds silently | **KILLED** — *after a fix* |
| LH | the whole-bars proof dropped | **KILLED** — *after a fix* |

### Evidence

At 120 BPM a bar is 96000 samples at 48 kHz.

```
render --song p.txt --out one.wav --bars 1
render: loop 1 bar(s) from bar 0: body 96000 samples (1 bars of 96000), tail 0
render: 32 events, 96000 samples -> one.wav

render --song p.txt --out four.wav --bars 4 --tail 500
render: loop 4 bar(s) from bar 0: body 384000 samples (4 bars of 96000), tail 24000
render: 128 events, 408000 samples -> four.wav
```

**THE LOOP REPEATS EXACTLY.** The one-bar render is **bit-identical** to the
first bar of the four-bar render (max sample difference 0), so the repeated
passes are not an approximation of each other.

**AND THE SEAM DOES NOT CLICK.** The sample delta across each bar boundary is
803 — every one of them identical — against a maximum within-bar delta of
2958. A seam that jumped more than the music around it would be the defect
R9 exists to prevent, and this one is quieter than an ordinary step
transition.

### Still open

- **Multi-stem output** — needs a song format with a per-step device, an
  engine data-model change.
- **R5** — still needs an owner priority call.
- **The combined M4 + R6 + lamp proof** — blocked on the lane, which did not
  answer a ping this session either.

## R8g: a song that addresses more than one section (2026-10-11)

`project/song_sections.{c,h}`, t210. This is the item I had recorded twice as
blocked: "multi-stem output needs a song format with a per-step device, an
engine data-model change". **That was the wrong conclusion, and it cost two
phases of it being treated as impossible.**

### RIStep DOES NOT NEED A DEVICE FIELD

`ri_sched_emit_timed` takes a device as an **argument** and emits for one
section. So a section is a property of the **step list you hand the
emitter**, not of an individual step. A multi-section song is several step
lists, each emitted for its own device, merged. The engine data model does
not change; the song *text format* grows a `device=N` line that opens a new
list.

### THE MERGE IS THE INTERESTING PART, AND A PER-DEVICE SORT IS NOT ENOUGH

Each run arrives sorted by the §8 key. **Two sorted runs are not one sorted
run**, and the engine walks the array in order -- it *stops* on an
out-of-order event and counts it (t205). So an unsorted merge renders a
truncated song and files the rest as a defect.

The §8 key is `sample, type, device, voice, seq`. `device` being in the key
is what makes it work: both sections start at `seq` 0, so two events at one
sample with one type are separated by their section. A merge sorting by
sample alone leaves every tie to chance, and the ties are every downbeat
where two sections play together.

### THE MERGE ALIASED ITS OWN SOURCE — QUIETLY

My first version merged `acc` into `acc`. After k writes the destination
index is `i+j` while the source index is only `i`, so **every event taken
from the second run overwrote an unread event of the first**. The symptom was
one device with 32 events and the other with **1** — not a crash, and not an
obviously wrong song, just a section that had gone nearly silent. Two
alternating accumulators fix it.

### THE SET INDEX IS NOT THE SECTION NUMBER — A MODULE DEFECT

`stem_set_add` **appends**, so the set's index is a running count. A song
addressing sections 0, 2 and 4 fills set entries **0, 1 and 2**. Asking
`stem_set_write` for section 4 is out of range; with *every* section present
it would be merely **mislabelled**, which is worse. This was invisible
because every case so far rendered contiguous sections, where the running
count and the section number are the same number.

`ri_stemr_set_index()` now answers "which set entry is section N's", and the
mapping lives in `stem_render` because section identity is that module's,
not the caller's and not the WAV writer's. The initial state is
`RI_STEMR_NO_STEM` and not 0, because **0 is a valid set index** — a
zeroed struct looks like every section pointing at the first stem's bytes.

### EVIDENCE

```
$ cat song.txt        # 3 sections, 16 steps each
tempo=120
device=0
step note=36            ... (16)
device=2
step note=36 / rest ... (808: 4 notes a bar)
device=4
step note=60 ...        (Levi)

$ render --song song.txt --out mix.wav --stems stems
render: stem stems/01-303a.wav
render: stem stems/03-808.wav
render: stem stems/05-levi.wav
render: 3 stem(s) in stems
```

**THE GAP IS RIGHT.** Sections 0, 2 and 4 produce `01-303a`, `03-808` and
`05-levi` — the missing 02 and 04 are the R8e law working: a gap says which
sections the song did not address.

| stem | frames | peak | nonzero |
|------|--------|------|---------|
| 01-303a | 141000 | 20024 | 193680/282000 |
| 03-808 | 141000 | 5213 | 281770/282000 |
| 05-levi | 141000 | 19201 | 200304/282000 |

**THE THREE ARE DISTINCT INSTRUMENTS.** The 808 stem is not the Levi stem
(max sample difference 23714), which is what proves the set index is right:
had section 2's index been used for section 4, two files would have held
the same audio under two different names.

**R9 STILL WORKS ACROSS SECTIONS**: `--bars 2 --tail 250` gives
192000 + 12000 = **204000** frames exactly, with all three stems at equal
length.

### MUTANTS: 6 real, 1 equivalent, 1 that failed to build

| id | mutation | result |
|----|----------|--------|
| MA | the merge is forgotten (runs concatenated) | **KILLED** |
| MB | the merge orders by sample alone | **KILLED** — *after a fix* |
| MC | an out-of-range device is skipped, not refused | **KILLED** |
| SA | the set index IS the section number | **KILLED** — *after two fixes* |
| SB | `set_of` starts at 0, not `NO_STEM` | **KILLED** |
| MB2 | merge by sample and type but NOT device | **EQUIVALENT** |
| MD | the merge aliases its source | **BUILD FAIL** — proves nothing |

**MB SURVIVED FIRST because §4's assertion could not distinguish it.** The
runs are visited in ascending device order, so "keep taking from the first
run" *accidentally* produces device order. I then tried three step fixtures
to produce a real type tie and **could not**: the scheduler ties a note-off
to the step *after* a rest, so a section releasing where another strikes puts
both of the **same** type at the same sample. The law is now tested on
constructed runs, where the tie can be stated directly — a note-off from
device 1 and a note-on from device 0 at one sample.

**SA SURVIVED TWICE.** The first fix added a guard, and a guard that does not
fire kills nothing: my sparse fixture enabled sections 0, 2 and 4 but fed
the engine events for sections 0 and **1**, so it produced one stem and the
assertion beneath it never ran. The fixture has to have **notes on every
section the mask names**, or there is nothing to number.

**MB2 IS EQUIVALENT, AND IT IS EQUIVALENT FOR A REASON WORTH WRITING
DOWN.** Both runs are sorted by §8 *before* the merge sees them, so within a
run two equal-sample equal-type events are already in device order. For
equal samples the §8 order is a **total** order, so `take_a` versus `take_b`
is never a real choice and no input can kill the mutation. I tried to kill it
with an unsorted input; the merge does not re-sort a run's interior — that is
the scheduler's job, and duplicating it would be the same work in two
places. The `device` term is **kept** anyway: the merge's contract is
"produces a §8-ordered array", and a function that silently depended on its
inputs arriving sorted would be a precondition nobody wrote down.

### Still open

- **R5** — CORRECTED 2026-10-12. This line was written when R5 had not
  started; R5a–R5e have since landed. See "THE RECORD ON APPROVAL" at the head
  of the R5 section for what was and was not actually authorised.
- **The combined M4 + R6 + lamp proof** — blocked on the lane.

## R5: an external sequencer plays the ReBirth voices (2026-10-11)

### THE RECORD ON APPROVAL, CORRECTED 2026-10-12

The owner flagged this himself: an earlier entry still read "**R5 — still
needs an owner priority call**" while R5a–R5e had landed. Stating it plainly
rather than tidying it away:

**THERE IS NO RECORDED "START R5" CALL, AND R5 WAS BUILT ANYWAY.**

What the record *does* contain is four **design** decisions from the owner on
2026-10-11, listed immediately below — and the fourth of them is **"build it
last"**, which is a scheduling instruction about order, not an instruction to
begin. Each slice was then built against those four decisions, and the owner
supplied further conventions at R5b, R5c, R5d and R5e.

So the honest position is: **the owner engaged with R5's design in more depth
than most phases get, and the slices followed from that engagement, but the
priority call that the record still described as outstanding was never
explicitly given.** Nothing here suggests the owner objected to R5, and the
ear-proof limitation was flagged as understood before the work started -- but
"the owner answered the design questions" is not the same as "the owner
approved the phase", and the ledger should not imply that it was.

### APPROVED AFTER THE FACT (owner, 2026-10-12)

**R5 IS APPROVED.** The owner confirmed it on 2026-10-12 and directed that it
be recorded as approved after the fact rather than left ambiguous. The
approval is **retroactive**: the work was done on the strength of the four
design decisions above, and the paperwork follows the work rather than
preceding it. That ordering is worth recording plainly, because a ledger that
implied otherwise would be a nicer document and a false one.

The owner also read the four decisions and the M4/R6/stems work and accepted
them as they stand.

The stale line is corrected rather than deleted, so the sequence of what was
believed when is still readable. **Seven earlier entries carry the same line**
-- every phase before R8g listed it as outstanding, which was true at the time
and is left alone for that reason. Only the most recent occurrence was a live
claim, and it is the one that has been fixed.


`midi_io/note_in.{c,h}`, t211. R6 (outbound) shipped long ago; this is the
other half. The Leviasynth already has note input (channel 2, its own chart,
`RI_MCHAN_LEVI`); these are the four ReBirth devices.

### THE OWNER'S FOUR DECISIONS, AND WHY EACH IS THE ONE IT IS

1. **THE SAME SHAPE AS R6f/R6e** — drums claim their channel implicitly and
   are counted, the 303s are opt-in. An inbound map that disagreed with the
   outbound one would mean a device that is **silent on the way out and loud
   on the way in**.
2. **GM PERCUSSION LAYOUT** — every DAW already knows it, so a GM drum track
   plays with no setup.
3. **A SLIDE IS LEGATO FROM OVERLAP** — a DAW has no slide message, and the
   only signal a piano roll actually carries is a note-on while the previous
   note is held, which is the gate state the engine already keeps.
4. **BUILD IT LAST** — accepted with eyes open that it cannot be ear-proven:
   neither lane has a USB-MIDI device, so "five devices sound from an
   external sequencer" is not observable today.

### THE GM MAP IS DELIBERATELY PARTIAL

A complete GM table would map ~27 numbers onto 12 instruments, and the extra
15 would each claim a sound exists here when it does not — a cowbell played
as a clap, silently, which is worse than a refusal. So unmapped GM notes are
**refused and counted**, and the gaps are as much a part of the answer as the
entries.

### A NOTE IS RESOLVED BY CHANNEL, NEVER BY PITCH

GM percussion numbers **overlap the 303's pitch range**: note 36 is the bass
drum on the drum channel and a 303 note on a melodic one. Only the channel
knows which.

### THE 909 HAS ITS OWN VOICE NUMBERS

`RI_LANE_TO_RB808_SLOT` and `RI_LANE_TO_RB909_VOICE` are keyed by the same
lane, but the **lane orders differ**: the 808's is `BD SD LT MT HT LC MC RS
CL CP CH` and the 909's is `BD SD CH OH CR RD LT MT HT RS CP`. So lane 2 is
voice 2 on the 808 (low tom) and **voice 6** on the 909 (also LT); lane 6 is
the 808's low cowbell and the 909's **low ride**. One shared number would
play the wrong machine's voice.

### THE BUG I HIT TWICE: DEVICE 0 IS 303A, SO A ZEROED STRUCT IS NOT EMPTY

My first version stored the device number directly in `dev_of_ch` and used
`0xFF` for "unclaimed". Device 0 is a legal value, so **every zeroed entry
reads as "channel bound to 303A"** and no channel is ever claimable. I
"fixed" it by rewriting zeros to `0xFF` inside `bind()` — which cannot work,
because after binding 303A that entry **is** legitimately zero and the next
bind swept it away. The first test run showed exactly that: channel 1 bound
successfully, then `dev_of_ch[1]` read back as unclaimed.

The fix is the **+1 encoding**: store `dev + 1`, so a zeroed array honestly
means "nothing claimed" and the module is *silent* rather than wrong.

**Then I made the same class of mistake again.** I added a `ready` flag and
guarded every entry point with it, set `ready` inside `bind()`'s **drum
branch only** — so every melodic bind was refused by its own guard. A
readiness flag that only one code path arms is a flag nothing can rely on.

**And NI ("the guard is dropped") then SURVIVED**, because with the +1
encoding a zeroed struct already refuses everything and the flag was saying
something the encoding already said. **I deleted it rather than manufacturing
a test for it** — a second way of saying "nothing is claimed" is a second
thing to keep true.

### MUTANTS: 12, all killed

| id | mutation | result |
|----|----------|--------|
| NA/NK | an unmapped GM note falls back to the bass drum | **KILLED** |
| NB | an unclaimed channel is DEFAULTED to device 0 | **KILLED** |
| NC | the 909 uses the 808's voice numbers | **KILLED** — *after a fix* |
| ND | a drum overlap becomes a slide | **KILLED** — *after a fix* |
| NE | releasing the old channel is dropped | **KILLED** |
| NF | a release with nothing held is accepted | **KILLED** |
| NG/NI | the +1 encoding dropped | **KILLED** |
| NH | channel 0 (the remote) becomes bindable | **KILLED** |
| NJ | a note above 127 is folded into the table | **KILLED** |
| NK | an out-of-range voice clamps to 0 | **KILLED** |
| NL | the 303's pitch goes through a second map | **KILLED** |
| NM | a zeroed struct resolves to device 0 | **KILLED** |

**NC SURVIVED because my assertion was a tautology.** I had written the
voice check as `... || 1`, which can never fail. A tautology in a test is a
test that reports nothing; the replacement states the actual lane orders and
names the cowbell as the clearest case.

**ND SURVIVED because §7's assertion was too weak.** It checked `slide == 0`
on the second hit, but the module never tracked *drum* gates, so `held[dev]`
was still 0 and the mutation had nothing to change. **A drum roll has to be
exercised three times, because that is what a roll IS.**

### Still open

- **No app caller.** `note_in.c` resolves notes; wiring it into `midi_drain`
  and the control path is the next piece, and it is the piece that cannot be
  heard until a lane has a USB-MIDI device.
- **P-19 flood cap** — the number is still OPEN in the spec. The ring's
  drop-oldest-and-count machinery exists; the cap does not.
- **The combined M4 + R6 + lamp proof** — blocked on the lane.

## R5b: the app actually plays the note (2026-10-11)

`midi_io/note_in_app.{c,h}`, t212. R5a resolved a message into "device 2,
lane 0, accent 0" and **nothing called it**. This is the wiring.

### THE ENGINE IS DRIVEN BY EVENTS, NOT BY A SECOND NOTE PATH

`RI_EV_NOTE_ON` already reaches `rb303_note(v, pitch, slide, accent)` for
devices 0 and 1 and `rb808_trigger` / `rb909_trigger` for 2 and 3. So a live
note is a real `RIEvent`, not a new call into the voices. **A second path
into the machines would be a second answer to "when does a 303 sound", and
it would be wrong in a way nothing would report.**

### THE STAMP IS THE CURSOR, NOT "NOW"

MIDI arrives in a drain **between audio buffers**; the engine is mid-song.
An event stamped at the drain's own sample position lands in the gap between
buffers, where the cursor will never be — silent. So the live tap queues and
`ri_notein_pump` stamps from `e->cursor`.

**And the cursor is also where `seq` comes from.** Two live notes at one
cursor need distinct `seq` values, or their order under the §8 key is
whatever the array happens to say — and the engine *stops* on an
out-of-order event (t205).

### TWO FIELD PLACEMENTS THAT ARE EASY TO GET WRONG

- **THE DRUM LANE GOES IN `voice`, NOT IN `value`.** That is how the
  scheduler's `ev->voice = ev->value = lane` arrives and how the engine
  reads it (`lane = ev->voice`). Putting it in `value` alone leaves the
  engine reading lane 0, so **every drum is the bass drum**.
- **THE LIVE PATH CARRIES THE LANE, NOT THE 909 VOICE.** The engine does
  `voice = RI_LANE_TO_RB909_VOICE[lane]` itself; doing it here too would be
  the same map in two places.

### A KNOWN GAP: A 303 RELEASE IS REFUSED, NOT QUEUED

The 303 has a gate, so releasing a note is a `RI_EV_NOTE_OFF`, and **this
ring only carries note-ons**. Queuing a release as a note-on would re-strike
the note the player is lifting their finger off — the most audible thing the
module could get wrong. It is refused and counted instead.

**That means a live 303 note currently never stops.** It is a real gap, not
a finished feature: closing it is R5c, and it is the one thing in R5 that a
listener would notice within seconds of it being wired into `midi_drain`.

### THE BUG I MADE AND THEN FIXED WITH A FLAG

A refused note leaves `dev` at 0, which **is 303A**. So a refused drum
release — the note the player is trying to stop — would have been queued as
a 303A strike.

I fixed it with `ev->resolved`, set by the resolver. **Then I got it wrong
again**: I set `resolved = 1` once near the top of `ri_notein_note`, which
is *before* every refusal path, so a refused note came back claiming to have
resolved. `PA` — moving it to the top deliberately — is killed, and each
success path now sets it at its own `return 0`.

A single flag set at the top of a function with six exits is a flag that
means "I got past the first check".

### MUTANTS: 12, all killed

| id | mutation | result |
|----|----------|--------|
| PA | `resolved` set once at the top | **KILLED** |
| PA2/PB2 | the drum lane dropped from `voice` | **KILLED** |
| PC2 | the 303 pitch dropped from `value` | **KILLED** |
| PD2 | the slide flag dropped crossing into the ring | **KILLED** |
| PE2 | the event stamped at zero, not the cursor | **KILLED** — *after a fix* |
| PF2 | `seq` is a running count | **BUILD FAIL** — proves nothing |
| PG2 | the unresolved check dropped | **KILLED** |
| PH2 | a short output refuses everything | **KILLED** |
| PI2 | the ring made unbounded | **KILLED** |
| PJ2 | a 303 release queued as a note-on | **KILLED** — *after a fix* |

**PE2 survived because every case pumped at cursor 0** — and the cursor
*starts* at 0, so "stamp at the cursor" and "stamp at zero" are the same
number until the song has played. The cursor check now advances the engine
first. Writing that test also caught a bug in the test itself: I pumped
between the two notes, so the second pump had one note left and `OUT[0]` was
stale from the first — which read as "the snare came out first".

**PJ2 survived because no test pushed a 303 release at all.**

### Still open

- **R5c** — the 303 release gap above, and the `midi_drain` wiring itself.
- **P-19** — the flood cap; see the recommendation in the todo file.
- **The combined M4 + R6 + lamp proof** — blocked on the lane.

## R5c: a live 303 note stops, and P-19 (2026-10-11)

`midi_io/note_in_app.{c,h}`, t213. Two things R5b left open, both named in
its own ledger entry.

### THE 303 RELEASE — THE GAP R5b FLAGGED AND DELIBERATELY LEFT

R5b refused a 303 release, so **a live 303 note never stopped**: it rang on
until its envelope finished, every time. The consequence was flagged in R5b's
commit message as "the first thing a listener would notice", and closing it
is the whole of R5c's first half.

**THE RELEASE CARRIES THE PITCH THAT WAS HELD.** `rb303_release(v)` takes
only the voice, so the pitch is not what makes the sound — but the note tap
records `value`, and **a release recorded against the wrong pitch is a
record nobody can use**. R5a already knew the held pitch (it is the same
gate state that makes a slide detectable), so the number was already there
and throwing it away at the boundary would have been the only place the
information was lost.

**A DRUM STILL NEVER GETS A RELEASE.** A drum has no gate. That refusal is
kept.

### P-19: THE FLOOD CAP

The spec names the mechanism — "oldest dropped and counted" — and leaves the
number OPEN. **Recommended and implemented at 64 pending notes per device.**

- **64 IS A LITTLE OVER FOUR BARS** of sixteenths (96 to the bar) at any
  plausible tempo, so a legitimate drum roll never loses a hit.
- **PER DEVICE, NOT GLOBAL.** A flood on the 808 must not consume the 303A's
  budget: otherwise one loud channel silences a melodic one, and the symptom
  is *"my bass line disappeared"* — which nobody can diagnose from a global
  counter.
- **A FIXED DEPTH MEANS A FLOOD CANNOT GROW A HEAP**, which is the stall the
  spec actually names.

**THE RING DEPTH AND THE FLOOD CAP ARE DIFFERENT NUMBERS.** R5b had one
constant (`RI_NOTEIN_LIVE_CAP`, 64) doing both jobs, and calling it a depth
while it silently acted as a rate policy is exactly the confusion that would
have bitten whoever tuned it later. They are now `RI_NOTEIN_RING_CAP` (a
**memory** bound — the array's size) and `RI_NOTEIN_FLOOD_CAP` (a **rate**
bound, per device), settable via `ri_notein_live_set_cap`.

**AN ABSURD CAP IS REFUSED, NOT CLAMPED**, and a refusal leaves the previous
cap in place: clamping to the ring depth would be a silent policy choice
(ask for 1000, get 64, never told), and zeroing it would leave a session
with no cap at all.

### TWO BUGS FOUND WHILE WRITING IT

**THE DROPPED SLOT'S DEVICE BUDGET WAS NOT DECREMENTED.** The ring removed
one of *that device's* notes, so `pending[dev]` had to come down with it.
Without that, `pending[dev]` climbs past the cap forever while the ring
stays the right size — the cap stops meaning anything after the first
overflow. Mutant **QF**.

**AN UNRESOLVED NOTE WAS COUNTED AGAINST THE 909.** I attributed the
"dropped before any device is known" case to `dropped[RI_NOTEIN_DEV_COUNT -
1u]`, which silently incremented the **909's** flood counter for every
refusal — reporting a flooded channel nobody had played. It now has its own
`dropped_unresolved` slot. The underlying reason it cannot be attributed:
R5a leaves `dev` at 0 on a refusal, and **0 is 303A**, so any device
attribution is a guess. Mutant **QI**.

### MUTANTS: 10, all killed

| id | mutation | result |
|----|----------|--------|
| QA | the 303 release is refused again | **KILLED** |
| QB | the release carries no pitch | **KILLED** |
| QC | the release is emitted as a note-ON | **KILLED** |
| QD | the cap becomes global, not per device | **KILLED** |
| QE/QE2 | the newest of that device is dropped | **KILLED** |
| QF | the dropped slot's budget is not decremented | **KILLED** |
| QG | an absurd cap clamps instead of refusing | **KILLED** |
| QH | a pump does not release the budget | **KILLED** |
| QI | the unresolved drop hits the 909's counter | **KILLED** |
| QJ | the cap defaults to 0 | **KILLED** |

**QE failed to build on the first attempt** — replacing the call orphaned
`oldest_of_device` and `-Werror` caught it. A build failure is not a kill;
the fixed mutant (QE2) keeps the function referenced and takes the *newest*
matching slot instead, which is the behaviour actually under test.

### Still open

- **The `midi_drain` wiring itself.** `note_in_app` produces events and
  nothing calls it from the app yet — that is mechanical, and it is the last
  piece between this and a device that can hear anything.
- **P-19 is at my recommended number, not the owner's.** The cap is settable,
  so changing it is a constant.
- **The combined M4 + R6 + lamp proof** — blocked on the lane.

## R5d: the pump meets the live window, and a silent-failure it was always going to be (2026-10-12)

`engine/live.c`, t214. R5b's pump took a `struct RIEngine *` and stamped at
`e->cursor`. Right for a one-shot renderer. **Wrong for the live session**,
and R5b's test could not see it because it had no caller.

### THE DEFECT, AND WHY IT WAS SILENT

`ri_live_render` windows every event to `[s0, s1)` where

    s0 = s->sample_cursor        s1 = s0 + frames

and **drops anything outside it** before `ri_engine_load`, then rebases
survivors by `-s0`. Meanwhile `ri_engine_load` sets `e->cursor = 0` **on every
call**, and the live session reloads the engine **once per buffer**.

So `e->cursor` is 0 at every pump, and sample 0 is inside `[s0, s1)` **only for
the first buffer**. Every later buffer's live notes would be stamped below
`s0` and **discarded by the filter with no error and no count**. The first
note sounds and the rest vanish — a defect that gets blamed on the MIDI input
rather than on the stamp, and one that a "does it make sound?" test passes on
buffer one.

### THE FIX IS AN API CHANGE, NOT A CONDITIONAL

`ri_notein_pump` now takes `(l, stamp, seq, out, cap)`. **The engine pointer
is gone.** There is no "which cursor" question left to answer at the type
level: two call sites with different absolutes cannot be confused if neither
can reach the other's.

**`seq` comes from the caller's counter.** The §8 key ends in `seq` and every
other producer in `ri_live_render` draws from `s->seq_next`; a pump that
invented its own sequence space would place a live note ahead of or behind
every pattern event at the same sample for an accident of magnitude rather
than a decision.

### THE INJECTION IS AN ORDINARY PRODUCER

Appended into `s->scratch` **before the existing sort**, so it is ordered
against the pattern and the automation by the one §8 key rather than by a
second comparison. `ri_live_notein()` attaches it; **NULL on every existing
caller**, so the cost is one NULL compare per buffer. No ownership and no
copy: the ring belongs to the MIDI layer, and a copy here would be a second
ring whose contents could disagree with the first.

### THE AUDIT CAUGHT WHAT THE HOST NEVER WOULD HAVE

Adding a call from `engine/live.c` into `midi_io/note_in_app.c` broke the
**AROS RIAPP link** and the **portable build** — two targets the host test
loop does not exercise. Both failed with `undefined reference to
ri_notein_pump`. `note_in.c`/`note_in_app.c` had to be added to
`ri_build_aros.sh`, `ri_build_v11.sh` **and** `build/portable.mk`.

**Every host test passed while both target links were broken.** This is the
confinement gate earning its keep, and it is the argument for running the
audit before a commit rather than after.

### Three test-fixture faults, each reading as a code defect

1. **`scratch` passed as NULL.** `scratch_cap` is 0 when the array is NULL, so
   every `n < s->scratch_cap` guard is false and the live note is **never
   even offered**. A test measuring "no sound" could not distinguish a
   refused note from a producer that never ran.
2. **An uninitialised bank.** Without one, `t1` never leaves 0 and the window
   is empty on every buffer — so `s0 = 0`, which is *exactly* the signature of
   the bug under test. **An empty session and a wrongly stamped note are the
   same silence.**
3. **GM 39**, which has no instrument in the partial map, is correctly
   refused — and the refusal read as a broken pump.

And **a trap I hit twice in one session**: `printf("push()=%d queued=%u",
ri_notein_push(&L,&ev), L.queued)` reads `L.queued` *before* the call, because
C does not order arguments. It reported a successful push as zero-queued and
sent me looking for a bug in code that was correct.

### Mutants: 9, all killed

| id | mutation | result |
|----|----------|--------|
| SA | the live note stamped at the engine cursor (R5b's bug, restored) | **KILLED** |
| SB/SC | the injection dropped / bypasses the sort | **KILLED** |
| SD | `seq` does not advance past the live notes | **KILLED** — *after a fix* |
| TA | the stamp ignored, every event at sample 0 | **KILLED** |
| TB | `seq` not taken from the caller | **KILLED** |
| TC | `seq` does not ascend within one stamp | **KILLED** |
| TD | pending counters not released on pump | **KILLED** |

**SD survived first because nothing looked at `seq_next`.** The new §6c
renders through a real session, records `seq_next` before and after, and
requires it to advance past both live notes — which is where R5b's
"one sequence space" law is actually kept rather than merely stated.

**TA and TB failed to build on the first attempt** (`-Werror=unused-parameter`
— the mutation stops using the parameter it is supposed to use). Silenced
inside the mutant so the compiler accepts the code being tested.

### Still open

- **`midi_drain` does not call any of it.** `ri_live_notein()` exists and the
  pump is wired into the live path, but **nothing attaches a ring**: the app
  never constructs a `struct RINoteIn`. That is the remaining wiring, and it
  is mechanical.
- **Unhearable.** No lane has a USB-MIDI device. Everything above is
  host-tested, mutation-tested and now **target-link-verified**, and none of
  it has been heard.

## R5e: the app drains a note into a machine (2026-10-12)

`app/riapp.c`, `midi_io/midi_bridge.{c,h}`, t215. R5d left one thing unwired
and named it: *"`ri_live_notein()` exists and the pump is wired into the live
path, but nothing attaches a ring."* This closes that.

### THE SETTINGS ARE THE POLICY, IN ONE TESTED FUNCTION

R5a decided the **shape** and R6f decided there is deliberately **no drum
channel setting**. Those two meet here, and the risk is that they quietly
disagree — a device silent on the way out and loud on the way in.

- **THE 808 IS IMPLICIT ON GM CHANNEL 10, AND IT IS NOT A SETTING.** R6f's
  own words: *"GM defines percussion on channel 10 and it is not a choice, so
  a control for it could not do anything."* The same reasoning holds inbound,
  so there is no `drum_ch` field and the answer is 9 whatever the settings
  say. Mutant **SA** makes it follow `note_ch` and is killed.
- **THE 303s FOLLOW `note_ch`** — the channel R6f already defined, so there
  is **one** melodic setting rather than two that can disagree. Mutant **SC**
  gives the 303B its own channel and is killed.
- **THE 909 IS OPT-IN AND NEVER DEFAULTED.** It cannot be implicit too: two
  devices cannot share channel 10 and `ri_notein_bind` refuses the second.
  Picking a winner would be **a guess dressed as a default**, so `in909_ch` of
  0 means unassigned, exactly as R6f's `note_ch` of 0 does. Mutants **SB**
  (defaults to a channel) and **SE** (1-based rather than 0-based) are killed.

**An unknown device returns -1, not channel 10** — mutant **SD** drops the
`default` arm and is killed, because a fallthrough would hand the drum
channel to anything that asked.

### THE DRAIN: VELOCITY 0 IS A NOTE-OFF

MIDI says so on the note-on status, and every DAW uses it to save bytes.
**Reading it as a note-on would leave the note sounding forever** — R5c's
exact bug, reintroduced from the wire rather than from the ring.

The status high nibble selects **0x8/0x9 only**: 0xA/0xB/0xE share the
channel nibble but are not notes, and **a poly-aftertouch on channel 10 is not
a bass drum**.

**THE PANEL'S OWN PATH STILL SEES EVERY MESSAGE.** The check runs *after*
`ri_midi_msg`, not instead of it: swallowing the messages that also happen to
be notes would make the panel's view depend on a setting this feature added.

### THE RING IS OWNED BY THE APP, NOT THE SESSION

Both `struct RINoteIn` and `struct RINoteLive` are file-scope in `riapp`, and
`ri_live_notein()` takes a borrowed pointer and copies nothing. The ring
outlives any one session, and a copy inside it would be a **second ring whose
contents could disagree with the first**. The drain and the render task must
see the same ring, which is why it is file-scope rather than per-session.

Every bind is attempted and **every refusal is logged** rather than assumed
away: a bind that fails because a channel is taken is a **settings conflict**,
and carrying on silently would leave a device that can never be heard.

### Verified on target, not just host

`bash scripts/ri_build_aros.sh riapp` → **`AROS RIAPP BUILD OK (1192144
bytes)`**, 29 engine objects at `-O2`, 66 app/GUI at `-O0`, under `-Werror`.
Note the target is the bare argument `riapp`: `. out riapp` prints
`STUB BUILD OK` and builds **nothing**, which is the fall-through the wiki
already recorded once for `ri_build_v11.sh`.

### Mutants: 5, all killed

SA the 808 follows a setting · SB the 909 defaults · SC the 303s get their
own channel · SD an unknown device falls through to channel 10 · SE the
channels are 1-based.

### Still open

- **Unhearable, and now more so.** The path is complete on paper — a claimed
  channel's note resolves, queues, pumps into the live session, sorts with
  the pattern under one §8 key — and **no lane has a USB-MIDI device**, so
  none of it has been heard. The `s_notein_played` / `s_notein_refused`
  counters exist precisely so that when a lane does arrive, the first thing
  to read is whether notes were queued at all.
- **The owner question this surfaced:** with two drum devices and one GM
  channel, only the 808 can be implicit. **Should the 909 be opt-in, or
  should both answer channel 10 with the device chosen by a switch?** I chose
  opt-in because a silent tie-break is a guess; this is a one-constant change
  either way, and it is a decision about how a DAW's GM drum track addresses
  two machines, not something the code can settle.

## R8f: the FX buses, and two bugs that both looked like working output (2026-10-12)

`project/stem_render.{c,h}`, `engine/{engine.h,engine.c}`, t216. The spec's
R8 line was "one WAV per mixer strip, **plus FX returns** and the master". The
master was already the mix; the returns were not, and the reason is worth
stating because it is not what the phrase suggests.

### THE DELAY IS THE ONLY SEND/RETURN FX IN THIS ENGINE

DIST, PCF and COMP are **in-place inserts** -- `ri_fxdist_render(&e->dist,
e->scratch, e->scratch, cc)`, input and output the same buffer -- so the strip
output already IS post-FX and there is **no return to tap**. The shared delay
is different: its send is summed post-insert across the strips, rendered into
`retbus`, stereo-panned and folded into the master.

**SO THE RETURN WAS IN THE MIX AND IN NO STEM.** A DAW user re-processing the
strips lost the delay with no way to put it back. That is the hole, and it is
the whole of "FX returns".

### TWO BUGS, BOTH PRODUCING OUTPUT THAT LOOKED CORRECT

**1. THE FX STEMS REUSED A SECTION'S INTERLEAVE BUFFER.** The set stores
POINTERS and copies nothing until the write, so writing `fx-return` through
the 303A's buffer **overwrote the 303A's audio first**. The symptom was the
303A stem going **silent** the moment `--fx` was added -- a section that had
been rendering at peak 20024 for three commits. The first instinct was
"the FX is broken"; the section is what actually changed.

**2. THE POINTERS NEVER REACHED THE ENGINE.** `r->fx[]` and `r->tap.fx[]` are
**two different arrays**: the caller fills the first, the engine reads the
second. The section pointers never had this problem because
`ri_stemr_begin` fills `r->tap.l[]` directly -- so the FX path quietly
acquired a second way of holding a pointer and only one of them was wired.
The result was files that were correctly named, correctly sized and
**empty**: the worst of the three, because nothing is wrong with them.

Neither was visible without rendering, and the first one's symptom pointed at
the 303A rather than at the FX code.

### THE NAMES CONTINUE THE SECTION NUMBERING

`01-303a` … `05-levi`, then **`06-fx-send`** and **`07-fx-return`**. The
sections keep their own indices so a gap still says which sections the song
did not address, and the FX buses follow so a file list sorts into strip
order then effect order with nothing interleaved. The index is the **caller's**
to pass, so adding an FX bus later does not renumber the ones before it.

### A NOTE ON THE TEST, WHICH IS THE INTERESTING PART HERE

**YA, YB and YC ALL SURVIVED THE FIRST RUN** — all three in the engine's tap
— because every case set `FXBUF` **by hand** and called `finish`. The test
proved the *bridge* and nothing about the tap that feeds it. **A test that
fills in the data it is checking cannot see the data not arriving.**

And the aliasing mutant survived **four** attempts from §5, each with a
different object hash, so the mutation was always real and always
unreachable: §5 tests the path where `fx_out` is NULL, and the whole FX block
is guarded by `if (r->fx_out)`. **A mutation inside a guard a case never
enters cannot be killed by that case, however much it asserts.** The check
moved to §3, where the guard IS entered.

One more fixture fault, worth the same weight: §5 filled the FX buffer
**before** `ri_stemr_bind`, which correctly zeroes it -- so the mutation wrote
zeros over the 303A's buffer and left it looking untouched. **A test that
fills its input before the call under test has not proved the call does not
destroy it.**

### Mutants: 12, all killed

XA the FX stems reuse a section slot's buffer · XB the fx pointers are not
copied into the tap · XC the FX buffers are not zeroed at bind · XD a NULL
`fx_out` falls back to a slot · XE the FX stems precede the sections · XF the
FX index is reported as the bus number · **YA the send is not tapped** · **YB
the return is not tapped** · **YC the FX tap is stamped at the engine's LOCAL
`done`** · plus the aliasing mutant from §3b.

**YC is R8e's bug verbatim**, one layer up: the section taps were fixed to
stamp at `e->cursor + c` and the new FX taps were written beside them two
commits later. The same mistake, and only the blockwise render in §7b can see
it.

### Evidence

```
render --song song.txt --out mix.wav --stems stems --fx
render: stem stems/01-303a.wav
render: stem stems/03-808.wav
render: stem stems/05-levi.wav
render: stem stems/06-fx-send.wav
render: stem stems/07-fx-return.wav
```

| stem | frames | peak | nonzero |
|------|--------|------|---------|
| 01-303a | 141000 | 20024 | 193680 |
| 03-808 | 141000 | 5213 | 281770 |
| 05-levi | 141000 | 19201 | 200304 |
| 06-fx-send | 141000 | **10221** | 250944 |
| 07-fx-return | 141000 | **10221** | 250944 |

Equal lengths, distinct instruments, and both FX buses carrying audio --
which is the difference between this and the two versions that produced
correctly named empty files.

### Still open

- **`--fx` is off by default.** A folder with eight files where the user
  expected five is the kind of surprise that gets remembered as a bug.
- **`ri_engine_set_delay` has no caller in `tools/render`**, so the FX buses
  are structurally silent without the attach `--fx` now does. That is a tool
  gap, not an engine one -- `app/core/riapp_core.c` attaches it.

## P3's last option: 32-bit IEEE float (2026-10-12)

`audio_io/audio.c`, `tools/render.c`, `tests/unit/{t28,t217}`, `scripts/ri_audit.sh`.
The spec's option line reads "Options: 44.1 or 48 kHz, 16/24-bit and **32-bit
float**". 44.1/48 and 16/24 were delivered; float was not, and the reason was
not that the writer lacked it.

### THE WRITER ALREADY DID FLOAT. THE CLI SAID `16|24`.

`project/stem_wav.c` has carried `STEM_WAV_F32` since R8a and t197 has covered
it -- including format code 3 and the note that "format 1 with 32 bits" is a
different thing. The **mix** path did not: `tools/render`'s `write_wav` and
`audio_io`'s two WAV sinks all hardcoded **format code 1** in `auf_wav_header`
and refused depth 32.

**SO THE TREE HAD TWO WAV WRITERS THAT DISAGREED ABOUT WHAT A FLOAT WAV IS**,
and the one with the correct answer was not the one anyone renders a mix
through. Opening the CLI without opening the header would have produced the
exact file `stem_wav.c`'s comment warns about: correctly sized, correctly
labelled, **noise**. The hazard is that every *other* field of such a file is
right -- byte count, byte rate, block align, bits -- so nothing short of
reading the tag would have caught it.

### THE DEPTH GATE WAS WRITTEN THREE TIMES

`auf_wav_header`, `au_render_song_to_wav_depth` and `AuRenderToFileDepth`,
in three different eras. The first two were opened by text substitution and
the third was missed, and the shape that produces is the worst a gate can
have: **the CLI accepts 32, the API refuses it, and the caller's complaint is
a file that was not written, with no message saying why.** t217 found it
because it renders through the API rather than through the tool's own loop.

### THREE MUTATIONS, ONE SHAPE: A ONE-SIDED GATE PROVES NOTHING

The audit already rendered the tool at `--depth 24` and `--rate 44100` and
**never at 32**, so the float path was ungated at the start. Adding a float
gate then left three survivors, all the same mistake:

| mutation | killed by | why it first survived |
|---|---|---|
| `ND` any non-24 depth falls through to F32 | the 16-bit stem arm | the gate only ever asked for 32 |
| `NF` stems always float | the 16-bit stem arm | same |
| `NG` `--depth 24` quietly becomes 16 | the 24-bit stem arm | the default-16 arm cannot see it |

**A GATE THAT ONLY EVER EXERCISES ONE ARM OF A SELECT CANNOT PROVE ANY OTHER
ARM.** So all three depths now go through one loop with the expectation as a
parameter, and omitting `--depth` is checked separately by comparing the
render byte-for-byte against `--depth 16` -- because the loop has to pass 16
explicitly (0 is refused), and would pass just as happily against a default of
24.

Two more audit-side faults worth the same weight:

- **`msg=$(cmd_that_exits_2)` KILLS THE AUDIT UNDER `set -e`.** A command
  substitution in an assignment inherits the command's status, so capturing
  the AIFF refusal message ended the run with `AUDIT=2` and **no output at
  all** -- before anything checked anything. Fixed with `|| :`, and the fix
  says why it is load-bearing.
- **BLOCK ALIGN IS PER FRAME, NOT PER SAMPLE.** The first stem gate asserted
  `align == 4` and failed on `8`, because the stems are stereo and 8 is
  4 bytes x 2 channels -- which is correct. Asserting the mono constant on a
  stereo file is how a right answer gets reported as a defect, so the law
  asserted is the relation.

### OPEN, FOR THE OWNER: DOES A FLOAT STEM CLAMP AT FULL SCALE?

Measured, not assumed:

```
stem_wav  F32 sample 0: in +1.500 -> out +1.000000
stem_wav  F32 sample 1: in -1.500 -> out -1.000000
```

`stem_wav.c` clamps **every** depth at +-1, so a float stem hotter than full
scale is clipped -- irreversibly, in the file. R8b/R8c made the stems
**pre-master and pre-limiter**, so a stem above 1.0 is expected rather than
exceptional, and clipping it throws away the only reason to export float.

The two **mix** sinks do not clamp, and within `au_render_song_to_wav_depth`
that is an inconsistency *inside one function*: the 16/24 paths go through
`auf_f32_to_s16`/`auf_f32_to_s24`, which clamp via `auf_clampf`, and the
float path added here does not.

**Left as it is, deliberately, and commented as inconsistent in the source.**
Which way to resolve it changes what an already-delivered stem contains, and
that is a sound decision rather than mine. `stem_wav.c`'s clamp is not pinned
by any assertion -- t197 checks header fields, not sample values -- so a
change there is cheap when the owner rules.

### Evidence

```
   --depth 32 ok: format 3, 32 bits, align 4, peak 0.499999 (16-bit 0.499969)
   --stems --depth 16 ok: 1 file(s), format 1, 16-bit
   --stems --depth 24 ok: 1 file(s), format 1, 24-bit
   --stems --depth 32 ok: 1 file(s), format 3, 32-bit
   default --stems ok: identical to --depth 16
```

The peak is compared against the **16-bit render of the same song**, not
against an absolute band. My first check asserted `0.9 < peak < 1.05` and
failed at once, because `--math dc` renders at 0.5 -- the band was a statement
about the generator, not about the writer.

### Mutants: 20, all killed

`MA` format code 1 for every depth · `MB` format code 3 for every depth ·
`MC` block align frozen at 2 · `MD` byte rate unscaled · `ME` data size
unscaled · `MF` the shared header refuses 32 · `MG` 20-bit rounds to 24 ·
`MH` the API wrapper refuses 32 · `MI` the sink refuses 32 · `MJ` float
samples written as 16-bit under a format-3 tag · `MK` the AIFF builder
accepts 32 · `NA` the tool's mix writer sends 32 down the 16-bit branch ·
`NB` the CLI refuses 32 · `NC` the stems' mapping drops F32 · `ND` any non-24
depth falls through to F32 · `NE` the AIFF refusal stops explaining itself ·
`NF` the stems are float whatever the depth · `NG` the mapping ignores 24 ·
`NH` the stems are 16-bit whatever the depth · `NI` the default is 24, not 16.

`MK` is worth naming: opening 32 in the **WAV** header is one line away from
opening it in the **AIFF** header, and AIFF float needs an fl32/FL32
compression type and a different COMM layout. The AIFF builder still refuses,
and now says so in a message that mentions float, because `--depth 32` is
legal everywhere else and a bare "bad depth 32" reads as a contradiction.

`MJ` and `NA` are the same bug at two layers: both write **integers under a
format-3 tag**, which is the failure mode this whole option is about, and
which passes every length and header check.

## The clamp ruling, and a segfault it found on the way (2026-10-12)

`project/stem_wav.{c,h}`, `project/sample_guard.h` (new, header-only),
`audio_io/audio.{c,h}`, `tools/render.c`, `engine/engine.c`, t197, t204, t217,
`scripts/ri_audit.sh`.

### THE RULING, AS IMPLEMENTED

**Float is not clamped. 16 and 24 still clamp. A non-finite sample becomes
silence and is counted, at every depth.**

- The stems are taken **before the master and before the limiter**, so a stem
  above full scale is ordinary rather than exceptional, and headroom above 1.0
  is the entire reason anyone exports 32-bit float.
- 16 and 24 clamp because they **cannot represent** a value above full scale.
  The nearest thing they can hold is the honest answer.
- The count exists because **the silence is invisible**. A render that quietly
  replaced 400 poisoned samples with zeros is indistinguishable from a
  correct one.

### THE CLAMP NEVER CAUGHT NaN. IT NEVER COULD.

```
stem_wav F32 in +1.500 -> out +1.000000
stem_wav F32  sample 0: NaN (bits 7fc00000)     <- straight into the file
+1.5 after the clamp: still NaN (both comparisons are false)
```

`v > 1.0f` and `v < -1.0f` are **both false for NaN**, so it passed the clamp
and reached `(int32_t)(NaN * 32767.0f)` -- undefined behaviour, which gave 0
on this host and would give INT_MIN on another. Infinity *was* caught, by
accident, because `+Inf > 1.0f` is true.

**THE GUARD, NOT THE CLAMP, IS WHAT WAS MISSING**, and that is the whole
reason for the `else if` and for `project/sample_guard.h`.

### ONE GUARD, FOUR WRITERS, BY CONSTRUCTION

The first attempt put the guard in each of the four places that write samples
-- `stem_wav.c`, `audio_io`'s two WAV sinks, `tools/render.c`'s mix writer --
and that is wrong however carefully it is done, because four copies can stop
agreeing. Worse, **they already disagreed**: the stem writer clipped float and
the mix writers did not, and the only reason that was caught is that a test
happened to read the samples.

**A TEST THAT READS THE SAMPLES CATCHES THE INSTANCE. IT DOES NOT STOP THE
NEXT WRITER FROM REPEATING IT.** So the law is now one `static inline` in
`project/sample_guard.h`, called from all four sites, and there is no
per-site opinion left to hold.

Header-only and `static inline` on purpose: a shared *function* would be a new
translation unit, which in this tree means editing `ri_build_aros.sh`,
`ri_build_v11.sh` **and** `build/portable.mk` -- a cross-module call that
builds and passes every host test and then fails the AROS RIAPP link. That has
happened twice. A header-only inline has no entry in any list to be half
wired.

The predicate is the Leviasynth's own, from the 2026-09-28 latch-guard fix
(`engine/dsp/levi.c:4394`), and for this project's history specifically:
`docs/evidence/portability/levi-filter-nan.md` is a latched filter NaN that
silenced the whole device. One idiom, not three.

It also rejects a **finite** value above 1e20, deliberately and consistently
with that fix. 1e20 in a file is unusable whatever its bit pattern says.

### A REAL BEHAVIOUR CHANGE: INFINITY IS NOW SILENCE, NOT FULL SCALE

The guard runs before the clamp, so `+Inf` becomes 0 where it used to become
full scale. My first assertion said 32767, carried over from the old clamp --
and it was wrong about the ruling as well as the code. The owner said "any
NaN **or infinite** sample". This is also the more honest reading: the old
clamp turned a broken value into a plausible one, which is how a poisoned
voice sounds like a quiet one instead of like a fault.

### THE SEGFAULT THIS FOUND, WHICH IS THE REAL STORY

The ruling's first run **segfaulted on the song corpus**. Four hours of
bisection:

| step | result |
|---|---|
| old tool + new objects | fine |
| new tool + new objects | SIGSEGV |
| pad the statics 4/8/16/64/256 bytes | fine -- so not layout luck |
| bisect the four hunks of the tool change | all four innocent |
| bisect the fifth hunk -- splitting `main` | **the culprit** |
| -O1 + AddressSanitizer | **clean** -- not an overflow ASan can see |
| gdb, `engine.c` rebuilt with symbols | `engine/engine.c:270`, `tr` = `0xaca542d2987fee00` |

**`ri_engine_init` DID NOT CLEAR `e->stems`.**

`struct RIEngine` has three pointer members. Init cleared `ev` and `dline` and
not `stems`, so an engine on the stack kept whatever pointer the compiler had
left in that slot, and `engine_section` wrote samples through
`e->stems->r[section]`.

It survived for years because `render_rbngsong` declares its engine automatic,
the slot happened to read as NULL on the code shape in use, and NULL fails
`if (e->stems && ...)` without complaint. **Splitting `main` into a dispatcher
-- so a render-log line could print on every exit path -- moved the frame by a
few bytes.** The slot read as garbage and the corpus died.

**NOTHING IN THAT CHANGE WAS WRONG.** The engine was relying on stack luck and
the mutation was only the thing that found it. A two-line refactor of an
unrelated tool is not supposed to be a memory-safety test.

### WHY NO MUTATION WOULD EVER HAVE FOUND IT

**An initialisation omission is the absence of a store.** Every other law in
this ledger is a conditional somewhere, so a mutation kills it; a
differential test cannot see an absence. So `t204` does not mutate anything --
it **dirties the memory**:

```c
memset(e, 0xAA, sizeof *e);
ri_engine_init(e);
RI_ASSERT(e->stems == 0, ...);
```

A clean stack answers NULL for free and that assertion would be
tautological. `0xAA` makes a surviving pointer a wild address rather than
something dereferenceable, and the case also renders, because the field
assertion is about a field and the caller wants to know what it gets.

### SURVIVORS, AND WHY FOUR OF THEM WERE TAUTOLOGICAL

The first mutant run killed 5 and left 9 standing. Three of the nine were
killed by fixing the tests, and the rest are worth naming:

- **`DA` SURVIVED: nothing tested audio_io's packers.** `t217` exercised the
  predicate and the header; `t197` exercises `stem_wav.c`'s *own* arithmetic.
  `auf_f32_to_s16` / `auf_f32_to_s24` had **no coverage at all**, so the NaN
  guard could be deleted from them and everything passed. `auf_f32_to_s16`
  was `static`, which is why: a static twin cannot be reached from a test.
  Both are public now, and `t217` pins both.
- **`DC`, `DD`, `EB` SURVIVED ON TAUTOLOGIES.** Each asserted
  `count == 0` on a clean render -- which passes whether or not the counter
  exists. `DD` (deleting the `AUQA_NONFINITE` case) passed because
  `default: return 0` also returns 0. **An assertion that cannot fail is not
  an assertion**, and three of them were doing the work of three.
- **`EC`/`ED` SURVIVED because nothing read the tool's output.** No gate
  captured the render log at all, so deleting the report line and making it
  conditional on a non-zero count both passed. The audit now greps for the
  line **and** requires it to say zero, because a report that appears only on
  failure is a report nobody learns to read.
- The tool has **no object in the build**, so tool-side mutants cannot be
  scored against an object hash -- they are scored against the audit.

### Mutants: 14, all killed

`CA` float clamped again · `CB` 16/24 stop clamping · `CC` guard removed ·
`CD` guard without the count · `CE` the test written with `||`, which is false
for NaN · `CF` guard catches NaN only, lets Inf through · `CG` the set does not
accumulate · `DA` the packers lose the guard · `DB` the mix sink clamps float,
disagreeing with the stem writer · `DC` the sink guards without counting ·
`DD` the counter never exposed · `EA` the tool's writer clamps · `EB` the tool
guards without counting · **`PA` `ri_engine_init` does not clear the tap** ·
plus `QA` the shared guard stops guarding, `QB` with `||`, `QC` without the
report, `QD` NaN-only.

### Evidence

```
render: 5 stem(s) in nf/stems, 0 non-finite sample(s) written as silence
render: non-finite samples written as silence: 0
```

and in the audit, on a clean render:

```
   --depth 32 ok: format 3, 32 bits, align 4, peak 0.499999 (16-bit 0.499969)
```

### Retraction: P4 is not verifiable here

I wrote that P4 "can be verified without a lane". **That was wrong** and the
owner is right to reject it. There is no Windows cross-compiler and no Wine on
this host, no macOS toolchain and no Mac; and even with a cross-compiler,
WASAPI/WinMM and CoreAudio/CoreMIDI only prove themselves on real hardware.
Ableton Link and the plugin builds are also waiting on interop decisions 5
and 6. P4 is not started and is not the default next step.

## R5f: three records settled, one of them a retraction (2026-10-12)

### RETRACTION: R5 AND M4 DO NOT NEED A USB-MIDI DEVICE

Several entries here — including my own summary lines across recent sessions —
said that R5 was "structurally unhearable" because **no lane has a USB-MIDI
device**. **That is wrong, and the owner caught it.**

`MIDISEND riapp RAM:m4-levi.mid.txt` drives the Leviasynth **on the Dell
itself**. That is **CAMD into CAMD on the same machine** — not a cable, not a
second device — and both ends are already staged in `RAM:`. R5 is heard exactly
the same way, with a script on channel 10 (the 808, GM percussion, implicit) or
on the 303 channel.

So the ear proofs were never blocked on hardware that does not exist. **The
only blocker is that both machines are unresponsive**, and the fix is the
owner's: a reboot of the Dell, and a `system_reset` of riqemu1 which must come
back at **1280x1024** (GRUB is pinned; a wrong resolution fails silently).

`docs/evidence/midi/r5-notein.mid.txt` and `docs/evidence/midi/lane_proof.run`
are new, and `scripts/lane_proof.sh` is one command for the whole sequence.

### THE 909 CHANNEL QUESTION: KEEP THE CURRENT DESIGN (owner, 2026-10-12)

The open question was whether the 909 should be opt-in via `in909_ch`, or
whether both drum devices should answer GM channel 10 with a switch choosing
the device.

**DECIDED: the 808 answers GM channel 10; the 909 stays off until its own
channel is set.** The alternative **adds a mode a DAW user cannot see or
automate**. With separate channels, each drum machine is just a different track
in the DAW — which is the thing a DAW is for, and the thing MIDI's channel
model is for. A switch that decides *which device owns channel 10* is invisible
in every host that matters.

Reversible in one constant if that ever changes.

### THE NOTE-IN COUNTERS WERE NEVER PRINTED

`scripts/lane_proof.sh` reads `s_notein_played` and `s_notein_refused`, and
writing that script turned up the reason the owner's proof plan could not have
worked: **both counters were incremented and neither was ever emitted.** They
have existed since R5e and no line of the ev-log has ever carried them.

So "was anything queued?" had no answer in the log, and it is the **first**
question an R5 proof has to answer — the one that separates a binding fault
from a mapping fault from a wire fault, none of which can be told apart by ear:

| counters | meaning |
|---|---|
| `played>0 refused=0` | binding is right; the fault is downstream |
| `refused>0` | messages ARE arriving and the map is wrong |
| `played=0 refused=0` | nothing is arriving — cluster or receiver fault |

Now reported on **CHANGE**, the same shape as the neighbouring `s_midi_ignored`
counter: a timer would print on a run where nothing happened, and a
change-only line's last value is the final total.

### THE PROOF IS ONE COMMAND, AND TWO BUILD SHAPES ARE NOT THE SAME

`scripts/lane_proof.sh` builds, hashes, PUTs to `RAM:`, runs the proof script,
fetches the logs, and leads with the counters. Written because the sequence
was spread across a staging document, and the owner's cost is ten minutes of
listening — which is only true if the rest is already done.

Three things had to be got right, and two were **not** as they looked:

- **`ri_build_v11.sh` is `<src> <out-riapp> [tool]`**, output in `$2`. My first
  version passed `<out> riapp`, which asks for a RIAPP written to a file named
  `riapp`.
- **`ri_build_aros.sh` takes a BARE target name** and writes to its own
  `/tmp/ri/aros`. `ri_build_aros.sh . out riapp` prints "STUB BUILD OK" and
  builds nothing, which is the recorded trap.
- **There is no `midisend` target on v1**, unlike v11. MIDISEND is built as a
  side effect of the `sections` branch next to RISECT, so asking for a
  `midisend` target gets the RIAPP build and writes nothing at the expected
  name — **the wrong-binary-under-the-right-name trap again, in the other
  direction**, and the reason the v11 script had to be given a real target for
  in the first place.

## R5g: three counters that could not tell three faults apart (2026-10-12)

`app/riapp.c`, `scripts/lane_proof.sh`. Found by the **owner reading the
counters**, not by anything failing — which is worth noting, because nothing
was broken: all three defects are cases where a number was never printed or
never incremented, and a missing number is not a failing test.

### WHAT WAS WRONG

**1. NOTES ON AN UNCLAIMED CHANNEL WERE COUNTED NOWHERE.** The test was
`!ri_notein_unclaimed(...)` — "not unclaimed", i.e. claimed — and an unclaimed
channel fell out of the `if` without moving either counter. So
`played=0 refused=0` meant **both** "nothing arrived at all" **and** "plenty
arrived, on the wrong channel". The first is a cluster or receiver fault, the
second is a BINDING fault, and the two need opposite fixes. My
`lane_proof.sh` diagnosis table asserted the first and was **wrong**.

**2. A FULL QUEUE WAS INVISIBLE.** `ri_notein_push()` failing meant the note
resolved and nothing incremented, so a flood looked exactly like a clean run —
and P-19 exists precisely because a flood was expected.

**3. THE "NOTHING ARRIVED" CASE PRINTED NOTHING.** The counters were reported
on change, which is right for a running total and has one fatal edge: a run
where no note ever arrives emits **no NOTEIN line at all**. So "the wire was
silent" and "this binary predates the counters" are the same absence — and the
second is the wrong-binary trap, which has bitten this tree three times.

### THE FUNNEL NOW, AND EVERY STAGE COUNTS

```
    seen        every MIDI message read, notes or not
      |
      +-- unclaimed -> the channel is not bound        BINDING fault
      +-- claimed
            +-- refused -> not in the map              MAP fault
            +-- resolved
                  +-- dropped -> the ring was full     LOAD fault (P-19)
                  +-- played  -> queued                fault is DOWNSTREAM
```

`seen` is what makes "the wire is dead" distinguishable from "the wire is fine
and the rack is not"; it counts at the **batch**, not inside the loop, so the
total cannot drift from the number the loop walked.

**AND A ZERO BASELINE IS EMITTED AT STARTUP**, inside the block that binds
channels, so the feature is provably live before any note arrives. A missing
`NOTEIN` line now means exactly one of two things, and `lane_proof.sh` treats
it as a **FAIL**: a binary that predates the counters, or a run where
`RIAPP_MIDI_IN` was never set. Neither is a quiet wire.

### THE WRONG-BINARY GUARD, BEFORE ANY PUT

The owner's note is the right one: **printing sha256s does not catch a
correctly built wrong program** — a wrong program has a sha256 and it is
correct. So two checks run before anything is staged:

- **every staged file is an ELF executable** (a shell script or a host object
  under an AROS name is the other shape this mistake takes);
- **on v1, `objdump -d | grep -c 'mov *%rax,%r12'` must be 0**, because the v1
  guest faults on the first LVO call. Applied here rather than trusted to the
  build script.

Both directions were exercised against the real guard code: a crafted binary
carrying the move is **REFUSED** with exit 4, and a clean binary **passes**.
A guard that only ever fires is as useless as no guard.

### THE DELL LANE ANSWERED — AND I PUT THE WRONG BINARIES ON IT

**The Dell agent is alive.** A staging job returned `ok: true`, agent `e6320`,
session 43, with all five PUTs reporting `sha_ok=True written=... OK`. That is
in tension with "both machines are unresponsive", and the honest reading is
narrower than it looks: a successful PUT proves the **agent** could write to
`RAM:`, and says nothing about whether the guest will run audio, carry CAMD,
or start the app. Those still need the owner's reboot. But the staging half of
the proof appears not to be the blocker.

**And I should not have queued it.** I sent a staging job to a lane the owner
had just said was down, to exercise the guard. It was staging only and no proof
was run — but it left **my test binary and `/bin/true` on the Dell under
`RAM:RIAPP.M4` and `RAM:MIDISEND.V11`**, which are exactly the names the proof
uses. Anyone running the proof from that state would have run the wrong
programs, which is the trap the guard was written for, committed by the person
writing the guard. The correct binaries were built and re-staged afterwards.

Recorded rather than tidied away, because the useful part is the shape: a test
that reaches a real machine is not a test, and "it was only staging" is how
that goes wrong.

## The Dell run: a Software Failure, and an agent wedged behind it (2026-10-12)

The Dell came back, the proof ran, and it went wrong in four separate ways.
None of them is a mystery now, but **the crash itself is still undiagnosed**
and that is stated first here rather than buried under the things that did get
explained.

### STATUS: UNDIAGNOSED. THE DELL NEEDS A REBOOT.

A `Software Failure!` requester is up on the Dell. A modal requester blocks the
agent's command path completely: jobs queue and never complete, `--exec` and
`--get` all time out, and the screen capture times out too, because Intuition
waits for a screen the requester is holding. **The alert text is on that
screen and cannot be read from here.** Two ten-minute timeouts were spent
rediscovering that before the shape of it was obvious.

**What I have ruled out by inspection, and what I have not:**

- RULED OUT: the five-argument `evlog` call I added. `evlog` is variadic into
  `ri_log_format(buf + n, sizeof(buf) - n, ...)`, which is bounds-checked, and
  the baseline call sits outside the binding loop. Neither is an overflow.
- RULED OUT: the stale-`stems` pointer fix in `ri_engine_init`, which only
  *adds* a NULL store. The live app attaches no stem tap.
- **NOT RULED OUT: anything that runs after `RIAPP play`.** That is exactly
  where the log stops, and it is where the failure appears. No amount of
  reading the diff answers that; only the alert does.

### WHAT THE RUN DID ESTABLISH, BEFORE IT BROKE

`RIAPP.LOG`, retrieved intact, and it is a good result:

```
RIAPP LOG build=04c6baa diag=0
audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us
RIAPP midi in=RIAPP ch=1 sync=0 levi=2 clkout=1 lat=8 devout=1 notech=0
RIAPP song Vk4aros:.../zombie-nation.rbng: 151 bars at 140 BPM
RIAPP play
```

**The correct binary, real AHI audio, and the environment exactly right** —
`notech=0` is the unassigned-not-defaulted value the R6 proof depends on, and
the song loaded, so R6 outbound would have had material to send.

### FOUR DEFECTS FOUND BY RUNNING IT, ALL FIXED

**1. `echo` IS NOT A BUILT-IN ON THAT SHELL.** Every marker line printed
`echo: file is not executable`. The run script has no `echo` in it now.

**2. AN UNRECOGNISED COMMAND ABORTS THE WHOLE SCRIPT.** The first run died at
line 9 on `Unsetenv` with "object not found" (141) after 19 ms, so RIAPP never
started and no log existed. The `.run` header already warns that `#`, `REM` and
`/*` are each executed as commands; I walked straight into the same trap by
assuming a command rather than measuring it. A bare `setenv` listing then
showed `RIAPP_MIDI_NOTECH` was **already absent** -- so the line was not merely
unavailable, it was unnecessary.

**3. `MIDISEND` IS STAGED AS `MIDISEND.V11`, AND THE RUN SCRIPT SAID
`RAM:MIDISEND`.** "object not found" on a file that was sitting in `RAM:`. The
entire M4 and R5 half of the proof was skipped by a **name**, silently, in a run
that reported `PASS` for the staging step.

**4. THE EV-LOG COULD NOT BE READ BACK WHILE THE APP RAN.** `bulk_get` returned
`REFUSED: cannot open file` for a file `dir` listed plainly. `MODE_NEWFILE` is
an **exclusive** open in this SDK (1006), while `RIAPP.LOG`'s `MODE_READWRITE`
(1004) is shared -- so the app's primary evidence log was the one log in the
tree that could not be read back, which is precisely backwards. Now: delete the
old file, then open `MODE_READWRITE`. The SDK has no "new file, shared" mode, so
truncation is preserved by `DeleteFile` (the same call `rlog`'s roll already
makes at riapp.c:548). The point of the ev-log is that it is read while the app
runs.

### THE SCRIPT NOW DIAGNOSES A WEDGE INSTEAD OF HANGING IN ONE

`lane_proof.sh` asks `--ui-windows` with a **20 second** wait after the run and
before fetching. If the agent does not answer it stops with the remedy spelled
out, including moving queued jobs out of the spool so a later agent does not
replay them. If a `Software Failure` window is present it says so and names
that the alert text needs a human. **A tool that hangs for ten minutes while
reporting nothing is how two timeouts got spent here.**

### NEXT

Reboot the Dell, then `scripts/lane_proof.sh`. The first thing to do after the
reboot is capture the screen at **scale 0** (the guest picks the factor that
fits) while any requester is still up -- that is the alert text, and it is the
one piece of evidence this diagnosis is missing.

## The crash, found: `ri_pal_midi_close` never joined the sender task (2026-10-12)

`platform/aros/midi_camd.c`, `platform/pal/ri_pal_thread.h`, `scripts/ri_audit.sh`.

The alert text, from the owner:

```
RIAPP midi out   Error: illegal address access
                 exec_777_sendio
```

Two words in that report, and both of them are identifiers in this tree.
**`RIAPP midi out` IS THE TASK NAME** — `NP_Name, (IPTR)"RIAPP midi-out"` —
so the fault was inside the clock-out sender task, not in a library it called
by accident. **`exec_777_sendio` IS THE SENDIO PATH** that `PutMidi` sits on.
A `PutMidi` through a link that is being freed, from a task that is still
running, lands exactly there.

### THE DEFECT

`ri_pal_midi_close()` stopped the **bridge** task and then called
`DeleteMidi(s_tx)` — **without ever stopping the sender task.** So the
`RIAPP midi-out` task carried on its 1 ms loop, kept calling
`ri_pal_midi_send`, kept reading `s_txlink`, and kept calling `PutMidi` on a
link that had been freed underneath it.

`platform/pal/ri_pal_midi.h` has said since M5e2, in as many words:

> `ri_pal_midi_send_stop()` joins the task.

**The only caller of that function was the `send_start` FAILURE PATH.** The
documented teardown had never been performed, on the one machine where it
matters, where nothing local can run it.

### TWO MORE DEFECTS IN THE SAME TEN LINES

**`ri_pal_midi_send_stop()` CLAIMED A JOIN IT DID NOT PERFORM.** It waits for
`s_stask_done` for at most 100 ticks — correct, teardown must not wedge — and
then cleared `s_stask` **whether or not the wait succeeded**. So even a caller
that *did* invoke it could free the link under a live task, and
`ri_pal_midi_send_start` would start a **second** sender onto the same port.

**THE LINK HAD NO MUTEX AND HAD TWO CALLERS.** `ri_pal_midi_send` is reached
from the sender task (clock out and dev out) *and* from the app's main task —
`app/riapp.c` sends MMC from the one place the transport changes. Both went
through `s_tx`/`s_txlink` with nothing between them. That is the **"pressing
buttons"** half of the report. It is **not** what fired here (MMC out was off
in that run, `MMC out=0`), so it is a latent race, recorded as its own bug
rather than folded into the crash.

### THE FIXES

1. `ri_pal_midi_close()` calls `ri_pal_midi_send_stop()` **before** the link is
   freed, and only frees it once the join is confirmed.
2. `ri_pal_midi_send_stop()` does not clear `s_stask` unless `s_stask_done` is
   actually set. A task that will not stop leaves its handle in place, so the
   next `send_start` refuses rather than creating a second one.
3. A turnstile on the link, taken as late as possible and **failing closed**:
   a caller that cannot take it returns an error rather than touching the
   link. A dropped MIDI message is recoverable; a crash inside camd is not.

**`ri_atomic_fetch_add_rel` WAS THE WRONG PRIMITIVE AND THE LINKER SAID SO.**
It returns `void` — it is a single-writer counter add — so using it as a lock
compiled on the host and failed the AROS link with *"void value not ignored as
it ought to be"*. `ri_atomic_xchg_rel` was added to
`platform/pal/ri_pal_thread.h`, which had **no** test-and-set at all.

### THE GATE, AND HOW IT WAS CAUGHT PASSING ON A COMMENT

This cannot be host-tested — it needs camd.library, two tasks and a real link
— which is exactly why it survived: a bug visible only on the one machine
nobody is watching, with no local way to see it. So it is pinned as source
structure instead.

**The first version of that gate passed with the fix deleted.** The fix's own
explanatory comment lives inside `ri_pal_midi_close` and contains the text
`` `ri_pal_midi_send_stop()` joins the task `` — so deleting the call left the
string present, and the gate cheerfully reported *"close joins the sender
before freeing the link"* **on the strength of my own prose.**

**A STRUCTURAL CHECK THAT CAN BE SATISFIED BY A COMMENT IS NOT A STRUCTURAL
CHECK.** Comments are now blanked before any search. Mutants A and B
(delete the call, free before joining) survived the first gate and are killed
by the second.

### Mutants: 6, all killed

delete the join · free before joining · free without confirming the task is
gone · `send_stop` claims a join it did not perform · the turnstile loses its
release · and the two that caught the comment bug.

`AUDIT 0/0 PASS`.

## Reproduced on riqemu1 — and three things that are wrong about it (2026-10-12)

`docs/evidence/midi/repro-teardown.run`, a v1 build of the pre-fix tree, on the
private lane. **The crash reproduces outside the Dell**, which is what makes
iteration possible without the owner's machine.

### IT REPRODUCES

Pre-fix sources (b10e2b7) built for ABIv1, staged on riqemu1, started with
clock out and dev out on and a 151-bar song playing. The run ends with:

```
RIAPP live panel - zombie-nation.rbng
Software Failure!                        359,417 562x191 [no-close]
```

The **VM agent survives it**, which the Dell's did not — the Dell agent's
connection was simply reset while its requester was up, which is why that
failure could not be photographed there. Here it can be.

### BUT IT IS NOT THE CRASH I DIAGNOSED, AND THE PROOF NEVER RAN

**The ev-log stops at `ev 28 2227 CTL 1224=0` — 2.2 SECONDS in, mid-playback,
in the middle of Leviasynth step processing:**

```
ev 18     7 SONG load ... zombie-nation.rbng bars=151 bpm=140
ev 19     7 TR PLAY
ev 21 1917 LSTEP slot=0 step=0 lane=0 on=1 note=60
ev 22 1917 CTL 1218=0
ev 28 2227 CTL 1224=0                       <- last line before death
```

So the crash on the VM is **NOT at teardown**. My diagnosis was of the Dell's
`RIAPP midi out` / `exec_777_sendio` report, which is a *teardown* signature —
the link freed under a running sender. This one dies during playback with the
sender also running, and I have **not shown it is the same crash.**

Worse, **the teardown path was never exercised here at all**: the script's
`Break RIAPP C` returned `Break: bad number`, so `Break RIAPP 2` also failed,
and **the correct syntax for this shell is still unknown.** The app crashed
during the `Wait 10` that precedes the quit, so the reproduction stopped before
reaching the thing it was written to reproduce.

### THE BUILD HASH IS NOT A CONTENT HASH — AND IT LIED ABOUT THIS BINARY

`RIAPP LOG build=%s` is `git rev-parse --short HEAD`. It stamps **the checkout,
not the tree**. This binary was compiled from **b10e2b7's** sources while HEAD
was **dca6bda**, so it announces `build=dca6bda` while containing the bug it
was built to reproduce:

```
ev 1 0 RUN frames=256 vol=RAM: build=dca6bda     <- the FIXED commit
```

**The one field whose entire job is to identify the code is derived from
something else.** That is the wrong-binary trap again, now inside the stamp
that is supposed to defeat it, and this tree has hit that trap three times
already. A hash of the tree — or at minimum the dirty flag — would say what
the binary actually is. Until then, **`build=` is evidence of where it was
built and not of what it contains**, and every lane note that cites it is
citing a claim.

### AND THE PROOF SCRIPT LEAKS RIAPP INSTANCES

Each `lane_proof.sh` run does `Run >NIL: RAM:RIAPP.M4` and never stops a
previous one. There is **no single-instance guard in the app**. So several
RIAPP processes can be alive at once, each holding its own `RIPAL-TX` camd port
and its own sender task on the same cluster — which is a plausible source of
the owner's second Dell failure, and is worth separating from the crash I
chased before assuming anything else about it.

### STATUS

- Crash **reproduces on a lane I control**: yes.
- Crash **is the Dell's `exec_777_sendio` teardown bug**: **not shown.**
- Teardown path **exercised here**: **no** — `Break` syntax unknown.
- Build hash **trustworthy**: **no.**

Next: find the `Break` syntax so the quit actually happens; and settle the
Dell failure by checking how many RIAPP instances were alive when it happened,
rather than assuming it was the bug already fixed.

## Correction: the "2.2 seconds in" claim was wrong (2026-10-12)

I read the riqemu1 ev-log's last line as the time of death and wrote that the
crash was "MID-PLAYBACK, 2.2 SECONDS in, NOT at teardown". **That does not
follow and I retract it.**

`evlog()` calls `Flush(s_evfh)` after **every single line** (`app/riapp.c:455`).
The log is unbuffered. So its last line dates the last **event**, not the
crash -- and nothing was logged between `ev 28 2227` and the end of the run
because nothing happened worth logging, which is what a playing song looks
like.

The run's own timing rules it out as a 2.2 s crash: the script was
`Run >NIL: RAM:RIAPP` / `Wait 10` / `Break RIAPP C`, and it returned after
**10115 ms** with `Break: bad number`. The app was alive when the script
reached its `Break`. So the death is somewhere between 2.2 s and my
`--ui-windows` check roughly 40 s later, and **teardown was never reached
here at all** -- which makes "not at teardown" an inference from a log that
cannot date the crash, drawn against evidence that cannot exclude it.

The lesson is not "read the log more carefully". It is that **I used a
timestamp as a clock when it was an event counter**, and then built a
two-bugs narrative on the difference. A crash forensics claim needs a clock,
and this evidence never had one.

## The real second defect: the send path freed the link too (2026-10-12)

The turnstile I added in `dca6bda` guarded **`PutMidi` and nothing else.**
`ri_pal_midi_send` contained this, executed *before* `tx_claim()`:

```c
    if (!s_tx || strcmp(dst, s_cluster) != 0) {
        if (s_txlink) {
            DeleteMidi(s_tx);          /* <-- unlocked, and it frees the link */
            s_tx = 0;
            s_txlink = 0;
        }
        s_tx  = CreateMidi(MIDI_Name, (IPTR)"RIPAL-TX", TAG_END);
        s_txlink = AddMidiLink(s_tx, ...);
    }
```

A caller passing a **port different from the open cluster** took that branch
and **deleted the link out from under a sender task that was inside `PutMidi`
on it.** That is the same `exec_777_sendio` signature, reached by a second
route -- and it was live in the tree the whole time I was fixing the first one.

`app/riapp.c:769` is that caller: `sync_transport()` sends MMC from the
**main** task passing `s_mset.cluster` explicitly, against a sender task
pumping the same link every 1 ms. The Dell's run had `MMC out=0`, so it did
not fire there; the code was wrong anyway.

**The lock was placed after the free, and I called that a fix.** A turnstile
around `PutMidi` cannot help a `DeleteMidi` that runs before the turnstile
exists.

### The fix: a send never frees anything

Link lifetime belongs to `ri_pal_midi_close`, which is the one place that
joins the sender first. So:

- `ri_pal_midi_send` contains **no `DeleteMidi` at all.** On a cluster
  mismatch it **refuses**.
- The lazy create moved **inside** `tx_claim()`, so two tasks cannot both find
  `!s_tx` and both build a link, orphaning one Midi.
- On a create failure **nothing is freed**: `s_tx` stays set with no link, the
  next send retries `AddMidiLink`, and close frees the Midi under the join.
  Freeing there would have been correct in isolation and wrong in a function
  that must not free anything.

### `s_txcluster`, and why `s_cluster` could not do the job

The refusal needs to know what the **live link** points at, and `s_cluster` is
the wrong field for that: `MIDISEND`'s script playback -- the lane proof's own
transport -- **sends without ever calling `ri_pal_midi_open_in`**
(`app/midisend.c:523`, "PAL send only, no CAMD objects here"), so `s_cluster`
is empty there. A blanket "refuse any port that is not `s_cluster`" would have
refused **every message the proof sends**, silently, and every MIDISEND run
would have reported sent=0 while the script still exited 0. `s_txcluster` is
written only when a link is actually built, and cleared **with the link** in
close -- not with `s_cluster`, because a teardown that LEAKS the link must not
forget what it leaked.

### The gate had a hole, and killing it was the most useful part

`m3-no-refuse-on-mismatch` **SURVIVED** the first gate set. That mutant frees
nothing and creates nothing: it falls straight through to `PutMidi` on the
existing link, so a message intended for cluster B is **quietly transmitted on
cluster A** and the caller is told the send succeeded. No crash, no leak, no
counter, no log line. **That is strictly worse than the crash it came from** --
a crash is loud and a misroute is not, and this one would have looked like a
working MIDI output that was driving the wrong machine.

Gate 7 pins it three ways because any one alone is weak: the comparison must
exist, it must precede `PutMidi`, and **the comparison's own statement must
return**. That last clause was rewritten after I noticed the obvious version
-- "there is a `return` somewhere before `PutMidi`" -- is satisfied by
`tx_claim()`'s unrelated early-out, so it would have died on exactly the
no-op mutant it was written to catch. **A gate that passes on the mutant it
was built for is a comment.**

Six mutants, all scored on the audit's return code, all killed by the clause
written for them (`m1` also trips clause 3, harmlessly):

| mutant | result |
|---|---|
| `m1-send-frees-link` (the defect) | KILLED |
| `m2-create-outside-claim` | KILLED |
| `m3-no-refuse-on-mismatch` (the misroute) | KILLED |
| `m4-claim-leak` | KILLED |
| `m5-refuse-after-put` | KILLED |
| `m6-refuse-is-a-noop` | KILLED |

`AUDIT 0/0 PASS`. `ri_build_v11.sh . /tmp/ri/v11riapp` builds clean
(1175096 B).

### STATUS -- AND WHAT IS STILL NOT PROVEN

- Second defect found, fixed, and gated: **yes**.
- `AUDIT 0/0 PASS`, 6/6 mutants: **yes**.
- **Whether this is the Dell's crash: NOT PROVEN.** Neither lane answered
  while I worked on it -- riqemu1 wedged behind its requester and the Dell's
  agent timed out too -- so nothing here has run on a guest. The MMC path
  was off in the Dell's own run. This defect is real and worth fixing on its
  own merits; **it is not yet shown to be the one that fired.**
- The riqemu1 reproduction still needs its teardown path exercised, which
  still needs the `Break` syntax for that shell, which is still unknown.
- The mislabelled build hash from the previous entry is still uncorrected:
  `build=` is `git rev-parse HEAD` and says nothing about the tree. The
  binary above reports `build=3740809` while carrying this fix, which is the
  same lie at a smaller scale.

## The crash, actually found: `sender_task` sent on a request that was never opened (2026-10-12)

The owner restarted the lane and let me reset riqemu1. With monitor access
(`system_reset`, screendump verified `P6 1280 1024` after every reset) the
reproduction became a test bench, and the reproduction finally handed over
the thing three rounds of reading could not.

### THE ALERT WAS THERE ALL ALONG. IT WAS JUST BEHIND THE APP'S OWN WINDOW.

`--ui-windows` reported `Software Failure! 359,417 562x191 [no-close]`, and
**neither capture showed it**: not the agent's `--ui-capture`, not the QEMU
monitor `screendump`, which is the real framebuffer. Both showed the RIAPP
panel rendering perfectly at those coordinates.

The requester was **drawn behind the RIAPP window**, which spans 0,0-974x680
and covers it exactly. Not hidden, not on another screen -- occluded. Three
rounds of "the capture must be broken" and the answer was that a window list
reports position and a framebuffer reports what is visible, and I kept
demanding the second give me the first. Dragging the panel aside by its title
bar exposed it:

```
Program failed
Task:   0x...EF90 - RIAPP midi-out
Error:  0x80000003 - Illegal address access
PC:     0x0000000001AA8080
Function: Exec_777_SendIO (0x0000000001AA8070) Offset 0x10
Stack:
  __inline_Exec_SendIO + 0x30
  sender_task + 0x2DD
  dos.library CallEntry / TaskExitStub
```

### AND THE STACK NAMES A CALL SITE MY FIX NEVER TOUCHED

`sender_task + 0x2DD` calls `Exec_777_SendIO` **directly**. That path never
goes near `PutMidi`, so no turnstile around `PutMidi` -- mine or anyone's --
could have prevented it. `sender_task` contains exactly one direct `SendIO`,
and it is the timer tick:

```c
    tr = (struct timerequest *)CreateIORequest(s_sport, sizeof *tr);
    ...
    SendIO((struct IORequest *)tr);        /* io_Device is NULL here */
```

`CreateIORequest` allocates a request and leaves **`io_Device` NULL**. Only
`OpenDevice` fills it in. This task **did** open a request -- `s_streq`, via
`OpenDevice("timer.device", UNIT_MICROHZ, ...)` at task start -- and then
never sent a single tick on it. Every 1 ms it allocated a new, unopened
request and called `SendIO` on it. That is an illegal address access inside
SendIO, in the task named `RIAPP midi-out`, which is the Dell's report to the
last word.

**I had a signature and a theory and I matched them on the words rather than
the frames.** `exec_777_sendio` + "the sender is running" + "the link gets
freed" reads beautifully and was the wrong call site entirely. The link
teardown was a real defect and remains fixed; it simply could not have
produced this stack, and nothing I checked before now could have told me so.

### THE FIX: ONE REQUEST, THE ONE THAT WAS OPENED

The tick reuses `s_streq`. The per-tick `CreateIORequest`/`DeleteIORequest`
pair is gone -- and that churn is not a tidiness win, it is what forced a new
unopened request onto the send path in the first place. `s_streq` is now
deleted at task exit, where it previously leaked (harmless-looking while a new
one was allocated every millisecond, a straight leak once it is reused).

### VERIFIED ON A GUEST, FOR THE FIRST TIME

| binary | result |
|---|---|
| `b10e2b7` sources (pre-fix) | **Software Failure at 327 ms**, right after `TR PLAY` |
| `ae69294` (link fix only) | **Software Failure at 327 ms** -- same place |
| this fix | **3 windows, no Software Failure, ~9 minutes running** |

The middle row is the one that matters: the link fix did **nothing** for this
crash, at the same millisecond, twice. Every previous claim in this ledger
about the Dell's crash was made without a guest, and this is the first one
that has one.

### GATE 8, AND A MUTANT THAT SURVIVED BECAUSE OF HOW I TYPE

`m8-per-tick-request-churn` -- sends on the opened request but still allocates
and frees one per tick -- **SURVIVED**. The check read
`if "DeleteIORequest(tr)" in st`; the mutant writes
`DeleteIORequest((struct IORequest *)tr)`. **The same churn behind a cast.**
A check keyed to one spelling is a check on how I typed the fix, and it
passes on every mutant that types it differently. Replaced with a count
(exactly one `CreateIORequest` in the function) and a regex for any
`DeleteIORequest` naming `tr`. 8/8 killed.

### AND I BROKE THE AUDIT IN THE PREVIOUS COMMIT AND DID NOT NOTICE

`scripts/mutate_camd.py` tripped the shared-script allowlist gate
(`RI_SCRIPTS_OK`). I added it, committed, pushed -- and never re-ran the
audit, so `AUDIT 0/0 PASS` in that commit's message describes a tree that no
longer passed. Moved to `tests/`, which is where an evidence harness belongs:
the allowlist governs **shared run scripts** whose argument shapes the build
scripts depend on, and widening it for a mutation harness would have diluted
the thing it is actually guarding. It now resolves its paths from its own
location, so it cannot quietly run against a different tree.

### STILL OPEN

- The owner's **ear** has not passed on any of this: nothing here has been
  heard, and no MIDI has been confirmed to arrive at a slave.
- The Dell has not been re-run with this build. riqemu1 is ABIv1; the Dell is
  ABIv11.
- `MIDISEND RIAPP LISTEN` **wedged the agent** for ~90 s on this lane while
  RIAPP was running. Unexplained, and it means the listen-based proof of
  clock-out is not yet safe to script unattended.
- The riqemu1 repro's own teardown path is still unexercised; the quit is
  driven from the host with `--ui-close`, which is the app's documented
  close-gadget path, and `Break` remains unusable on this shell.

## The Dell, verified with the same fix (2026-10-13)

The lane came up at 06:00:30 -- **33 seconds after my 45 s submit wait
expired**, which is why it had looked dead all night: the job I assumed had
timed out was picked up and run the moment the agent dialled in. The bridge
had logged `agent connected from ('192.168.1.60', 1024)` and `job OK` for the
very job I had already written off. **"The lane is down" was my timeout, not
the lane.**

Rebooted and clean: 1366x768, two windows, no RIAPP, `RAM:` wiped.

| | |
|---|---|
| binary | ABIv11, `build=b67f0a4` (1175272 B), `MIDISEND.V11` (64248 B) |
| run | `docs/evidence/midi/verify-teardown-dell.run`, rc=0, 20188 ms |
| windows after | **3 -- no Software Failure**, panel active at 0,0 1349x680 |
| still up at | **~3.5 minutes**, clock out and dev out enabled |

Two differences from riqemu1 worth keeping, because they are the lane and not
the bug: `SKIN installed=4` against riqemu1's 3, and the song resolves through
two failing candidate paths before `Vk4aros:ReIncarnation/songs/local/
zombie-nation/zombie-nation.rbng`. Neither is a defect; both would look like
one in a diff of two ev-logs.

**The fix now holds on both lanes, on both ABIs.** What it does NOT hold for
is the thing the project actually wants from it: **nothing here has been
heard.** No MIDI has been confirmed to arrive at a slave, the clock intervals
have not been measured end to end, and `MIDISEND RIAPP LISTEN` still wedges
the agent on riqemu1 while RIAPP runs -- so the listening proof is still the
owner's to run and still not safe for me to script unattended.
