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
