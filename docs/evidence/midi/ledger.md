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
