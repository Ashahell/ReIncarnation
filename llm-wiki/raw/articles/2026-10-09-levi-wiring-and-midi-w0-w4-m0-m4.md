# Leviasynth wiring W0–W4 and MIDI interop M0–M4: the panel adopts its own instrument, and the transport follows a clock (2026-10-08 – 2026-10-09)

- Source: ReIncarnation session (OpenCode lane), 2026-10-08 – 2026-10-09. Plan `docs/superpowers/plans/2026-10-08-levi-wiring-and-midi-opencode-prompt.md`; 25 commits tagged `[levi-midi]`, `5c71a40`…`7b178c4`.
- Evidence: `docs/evidence/levi-wiring/2026-10-08-advisor-audit/` (the audit harness and its output), `docs/evidence/midi/ledger.md` (every E0 default), `docs/evidence/midi/m0-camd.md`, `m3-riapp-proof.md`, `m4-riapp-proof.md`, and the per-take logs beside them.
- Collected: 2026-10-09
- Published: 2026-10-09
- Spec: `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R1–R12, P1–P4)
- Prior: [2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md) (the follower core this work wires), [2026-09-26-gui-remote-midi-g7.md](2026-09-26-gui-remote-midi-g7.md) (the one-channel receiver and the CAMD bug it found), [2026-09-29-gui-round3-s1-s3-and-interop-requirement.md](2026-09-29-gui-round3-s1-s3-and-interop-requirement.md) (the requirement, and the "MIDI clock only drives the Sync LED" baseline this work overturns)

## What this slice was

Two pieces of one plan, run in order.

**W (wiring)** closed four defects an audit harness found in the Leviasynth's own panel-to-engine path. **M (MIDI)** took the ReBirth interop requirement from "CAMD prerequisites and a follower core that nothing consumes" to "a Dell-lane take that follows a scripted clock for five minutes".

The two are related by one fact: **the host test harness had been the only witness for both, and the lane is where the real defects were.** M3's five most expensive fixes were all found on hardware, not in tests. That is the article's spine.

## W0–W4: the audit harness became gated laws

The starting evidence is the audit output, which is worth keeping in the record because its shape changed the work:

```
rows: 180 keyed, 19 default-mismatch, 2 state-dead, 79 silent (all contexts)
encoders: 893 live slots, 201 default-mismatch, 96 nomove, 10 nokey, 10 state-dead, 559 silent (all contexts)
```

- **W0 (`5c71a40`)** — baseline re-run. Default and `ctx` runs of the harness match, **except one LFO-STEP slot that flips SILENT↔STATE between two identical runs**. That is an uninitialised read, not a Leviasynth defect, and it is the reason W4 exists.
- **W1 (`8a868c8`)** — the dead-slot law moved. `DIGITAL FILTER` p1 slot 6 (VEL>ENV, `RI_SLEVI_DVEL`) has been live since P9b, so the law that flagged it was itself stale. Dead-slot law now derives from `RI_SLEVI_NMOD-1` (`RI_SLEVI_M_PERF`). 3 mutants killed.
- **W2 (`de54a90`)** — bug 1, the real one. `ri_panel_levi_adopt` (`gui/panelctl.c`) now pushes **every keyed registry row plus every live encoder slot on every module page** — 180 rows and 1093 slots — through the same bridge path a RIAPP knob turn uses, chunked at `RI_CTL_CAP/2` with caller drains so the 256-entry control plane cannot overflow silently. t178 pins it, including **9 documented exemptions** (legacy fan-out rows, FX preset bundles). RED was 234 misses.
- **W3 (`0905aba`)** — bug 2. Morph Pos is kept as a 7-bit per-voice knob (`mpos_knob`) and the internal `mpos` is re-derived from it in `levi_set_slot` and `levi_set_amode`, so mode changes and slot changes land back on the knob instead of losing it. t179 pins four orders against set-last twins. **No RBNG change**: automation already carried 7 bits, the knob is runtime state.
- **W4 (`48a7a93`)** — `ri_slevi_init` left `lfsc/lfsv/lfsr` uninitialised, which is the W0 flip. Init to 0/0/0 (t145). t180 gates the residue: init determinism, a delivery sweep over all 180 keyed rows and every live slot (zero STATE), the NOMOVE and NOKEY classifications with destination proofs, and **10 render contexts** (velocity, vibrato, glide-legato, bend, keytrack→cutoff route, delay type, idle-op silence vs live-op audibility, analog-cutoff-127 identical, width identical, reverb tone).

**The lesson recorded here rather than in a commit message: the audit's SILENT list is not a bug list.** 559 encoder slots read SILENT across all contexts and most of them are correct — a control can be stored, delivered and inaudible by design. "Fixing" one into audibility by widening its range is the failure mode, and it needs an E1 reason, not an audit row.

## M0: the Dell's CAMD was broken upstream, twice

Before any of our code ran, the lane could not deliver a MIDI byte. Two upstream AROS bugs, both by Jaime Dias on 2026-10-05, both ancestors of `origin/master`:

- `28ec43a517` "camd: format cluster names through a va_list" — `mysprintf()` read varargs as `void *start = &fmt + 1`, which is m68k-only. On x86-64 every cluster name was garbage, so **no two CAMD links could ever connect**. This is the same bug the G7 lane found and patched locally in September; the local diff is now retired as superseded.
- `cb8c4c5f39` "camd: skip arena hunks when scanning a driver for MidiDeviceData" — the ELF arena loader stores hunk size 0, so unscanned `LoadDriver()` wrapped to ~4 GB off the end of memory. **That is why the first camd open blocked on `DEVS:Midi/debugdriver`**, the "exposed, still open" item in the G7 article.

Both diffs are carried verbatim with credit headers into both ABI carriages (`src/abi-patches/v1/aros/0073-…`, `0074-…`; `patches/*.v11.patch`), verified to apply cleanly, with the trees left pristine after the check.

The proof is a pair of SELFTEST runs, and they are worth reading side by side:

| | Dell, before | Dell, after owner reboot |
|---|---|---|
| senders connect | `connected: 0, 0` | `connected: 1, 1` |
| cluster name | `00 61 6D 64` (garbage) | `6D 30 70 6F` ("m0po…") |
| echo | `got 0`, rc=20 | `got 2`, rc=0 |

"**MIDI doesn't work on the Dell**" before the M0 reboot is the expected reading of a broken camd, not a finding about our code. M0 is green on both ABIs.

## M1–M2: from a core nothing consumes to a bridge that does

- **M1 (`ce64f6e`)** — the MIDI 1.0 downbeat law: `FA`/`FB` **arm**, and the **next `F8`** emits `PLAY_START`/`CONTINUE` exactly once. A follower that starts on `FA` is early by one clock. `FA`-then-`STOP` disarms; dropout drops the arm; SPP seeks while stopped and is **counted and ignored** while running (`spp_ignored`), which is an E0 reading of a spec silence and is ledgered with its revisit note.
- **M2 (`5ccd1f6`)** — the portable `midi_bridge` router: realtime and system-common to the owned follower, everything else to the GUI queue, P-19 flood caps of 256 channel messages and 64 intents with oldest-dropped-and-counted and **no per-key coalescing** (it is a stream, not a state). The AROS backend grows a dedicated bridge task with EClock stamps; the host backend feeds the same router. RIAPP drains into midimap, pushes value diffs, and counts intents.

M2 also killed a **batch-final collapse**: transient transport edges must sync **per message**, because a Play followed by a Stop in the same drained batch is two commands, not one state. The lane RED missed it (Play + final Stop), the fixed binary logs both, and a host probe now replicates the sequence.

## M3: the transport follows the clock — and the owner's ears found what tests could not

This is the expensive part, and the sequence matters more than any single number.

**M3 (`b1fa941`)** wired the applier: intents onto the panel transport, a tempo law with a bounded phase servo, `tempo_lock` making the tempo knob read-only and TAP dead while following. Lock deadline 2 beats; servo 0.01 BPM/tick bounded to ±2.0 BPM, both ledgered.

**M3b (`1fa5675`) — five defects the host tests could not find.** All five were in the same shape: *the host fed the wrong unit in and compared the wrong unit out*, so the test agreed with the bug.

1. `ri_core_meters` projected 16ths at a **fixed 120 BPM** — the Song Position ran at the wrong speed at every tempo but 120.
2. The servo measured against the **panel's projection** instead of the engine. The panel position is per-frame, rebased at the playback edge, and was at a fixed rate — a wrong number, in a different instant, from the one the clock is measured against. Its error railed the ±2 BPM trim for a whole take.
3. A **local Stop did not freeze the expectation** — the trim railed against a parked engine.
4. **SPP landed a quarter of the distance it named.** The intent speaks sixteenths; the follower emitted `beats * 24`, so Continue after a seek played 4× early. Found because the first seek script wrote `F2 00 28`: SPP is `F2 <lsb> <msb>`, so that was position **5120**, which located bar 320 of a 151-bar song and immediately ran the take off the end.
5. The **Song Position never rebased at the playback edge** — a take that started late appeared minutes into the song (BAR 22 for a take located at bar 10).

**M3c (`f7f28c1`) — the drift trace, and the read that cost a round.** The ev-log's second column is the **audio buffer count, not a clock**; that misread cost a round of reasoning before the trace carried `CurrentTime`. With a real trace, two more defects: **relocks swallowed clocks** (the expectation advanced only while locked, so every jitter relock dropped clocks already off the queue — `f8=2196/2196` on the wire against a third short in the expectation, phase error growing to +4726 ticks ≈ 40 s over 88 s), and **a locate has to be render-owned** (the render recomputes the tick cursor every block, so the app's store was discarded while playing; a Start intent on the autoplaying demo left the engine at tick **12095**).

After both, the same 88 s Dell script:

```
ev 17 follow=63bpm eng=308  exp=316  err=+8    f8=79/79    drop=0,0,0
ev 66 follow=62bpm eng=8809 exp=8800 err=-9    f8=2200/2200 drop=0,0,0
```

Phase error over the whole take **−119 … +75 ticks** with no trend, every clock accounted for on both sides, no drops.

**M3d (`fd621e1`) — a tempo change must append, not rewrite.** The tempo map was anchored at tick 0, so a tempo *drop* mapped the current tick to a sample position **ahead** of where the audio actually was, and the render's forward-only tick walk froze until the audio caught up. Not MIDI-specific: the tempo knob mid-song did the same. Lane cost before the fix: **engine frozen on 29 of 143 samples (20%)** of a 5-minute take. A tempo change now appends a segment at the current tick (`RI_LIVE_MAX_SEGS = 64`, E0, the collapse is the safety valve).

**M3e (`acc102f`) — the owner's ear proof failed, and the failure was mine.** The owner ran the 5-minute take and reported a **hanging tone**, the Mixer/Master Comp button doing nothing, and **playback speed collapsing**. First two symptoms of the third: when the segment list filled, the collapse re-anchored `sample_cursor` onto the new mapping, so music was skipped or repeated — a skipped note-off hangs a voice, a repeated region moves the song position fast. Host RED: `tempo change 63 moved the audio position (299008 -> 294098)`. The law is now **a tempo change only appends; the audio position is physical.**

One thing was explicitly *not* claimed: anchoring the collapsed segment at tick 0 rather than at the cursor also leaves the audio alone, and t95 does not distinguish them. It rewrites map history that nothing reads back on a live take, so the Dell lane is where it would show. **A green host test does not close a question the host cannot see.**

**M3f (`7b3739f`) — a collapse flattens the past; it does not pretend it was never there.** The map is a list of segments all anchored at tick 0, so dropping history silently **re-maps the past**: keeping the first segment (the song's own 140) made the map claim the audio was ~4 s behind, and the render answered by throwing the tick cursor **346/497/579 ticks in a single block**. That is the owner's "playback speed suffers". The collapse now measures where the audio really is with the full list, replaces the past with **one synthetic segment that maps [0, cursor] exactly onto that sample position**, and runs the real rate from the cursor on. Law pinned in t95: a tempo change may move the tick cursor neither further nor less than the audio it just played (4..16 ticks per 4096 samples at 60 bpm).

**M3g (`cf60bf0`) — the proof tool the test was missing.** MIDISEND's per-message cost caps its clock at ~60 BPM, so **every proof so far ran the 140 BPM song at less than half its own tempo** — a fair failure of the proof, not of the app. `MIDICLOCK` (`app/midiclock.c`, AROS-only) streams `F8` itself from an EClock schedule and prints what it achieved: **139.88 BPM when asked for 140** on the Dell. (It spin-waits the last 1.5 ms to beat the 10 ms system tick.)

### The owner's take, and the disposition

`RAM:RIAPP.M3`, build `fd621e1`, 7500 clocks at ~40 ms (a ~62 BPM scripted master):

- **7500/7500 clocks** both sides; bridge/intent/CAMD drops **0,0,0**; **0 xruns**;
- **engine frozen: 0 of 155** during-take samples (M3d took 29 of 143);
- session tempo **61.0–62.7 bpm**, 486 samples/tick — the audio rate and the tempo agree;
- **phase error −125 … −32 ticks, converging** (mean −82.9 first third, −44.7 middle, **−39.3** last); the take ended at engine tick 30043 against the master's 30000 — **43 ticks = 0.45 s after five minutes, and shrinking**;
- Start located to tick 0, the master's final Stop ended it.

**The owner parked M3. It is not passed.** What is proven: following at ~60 BPM on the Dell, plus the lock law at the song own 140 BPM on the host (t182, 17.857 ms intervals). What is **not** proven is following at 140 BPM on real hardware, because **this guest's CAMD delivers clocks in 10 ms system-tick batches** — more jitter than the R1 lock law accepts. MIDICLOCK delivers a clean 140 BPM and the app cannot see it through the batching. Closing that needs a USB-MIDI interface and a real master, which is the actual use case, **or a follower that tolerates batched arrivals** (M1 estimator work).

riqemu1 proves intents, locate, lock and display but **cannot prove drift**: it has no real-time audio pacing, so its audio clock runs in bursts while the MIDI clock runs on. Drift is a Dell-lane measurement.

## M4: the Leviasynth gets its own channel, its own map, and live notes

Until M4 the whole MIDI channel space belonged to the G7 remote, whose CC lookup `ri_ctlreg_by_cc` is **global**. M4 gives each device its own map and pins the no-collision law: **a CC on the Leviasynth channel must never move a ReBirth control, and a ReBirth CC must never move a Leviasynth parameter.**

- **M4a (`ff736d2`)** — `midi_io/midi_chan.{c,h}`: devices keyed by **instance**, not by message kind. The rack rule: a channel belongs to one device and a device to one channel, and **rebinding a taken channel is refused (`RI_MCHAN_TAKEN`), never stolen** — so the remote keeps working with an instrument plugged in. E0: remote on channel 1, Leviasynth on channel 2 (`RIAPP_MIDI_LEVI_CH`). `midi_io/midi_levi.{c,h}` carries the parser and the CC map, **cited per row to the Leviasynth Keyboard Owner's Manual v1.2.1 "MIDI CC Charts" pp. 168–169**; the manual text stays out of the repo and midi.guide's CC BY-SA table is not used. A row is mapped only where **one** 7-bit value can address our parameter; every gap carries its reason. **CC 7 (master volume) is refused outright** — it would move a ReBirth control — as are the manual's reserved CCs.
- **M4c/M4d (`e0aa191`)** — the live note and performance path. One 7-bit control value cannot carry a note number or a 14-bit bend, so `RIControlMsg.flags` — which existed and was **always zero** — now travels beside the value into `RIEvent.flags` bits 8–15 (`ri_ctl_send2`). Keys NOTE (val = velocity, hi = note | on<<7), BEND (14-bit, centre 8192), PRESS, PAT, WHEEL. The panel's Bend Range scales the bend. E0: a live note lands at the **block boundary** (256 frames = 5.3 ms) — it has no scheduled sample, and a deterministic one-buffer quantum is honest where a fractional-sample claim would not be. **Not routed, on the record**: sustain pedal (CC 64) and all-notes-off (CC 123) reach nothing, because the engine has no sustain gate and no panic.

### Two findings worth more than the code

**The bit that turned every note-on into a note-off.** The high byte was read with `& 0x7F`, which stripped the note's on/off bit at **position 15**. Every note arrived as a note-off, so the whole path was silent and every counter still looked healthy. t184 now pins the **audio itself**: the note sounds, its note off stops it, velocity 0 is a note off, and bend/AT/poly-AT/wheel land in the engine's own state.

**A green host test that could not see the defect it was written for.** After M4d was committed, RIAPP's `midi_drain()` was found to push PARAM and NOTE to the control plane but only **log** the PERF branch — a bend, mod wheel or aftertouch arriving on the Leviasynth channel was counted, traced, and dropped on the floor. t184 did not catch it because t184 drives its own push helper and `midi_drain()` is AROS-app code with **no host test**. Fixed in `2a17e90`, and the gap is written down rather than implied: **M4's green host tests do not mean "the app pushes every kind"**, and the lane proof is what covers `midi_drain()` until there is a seam for it.

W4's amendment is the same shape and worth noting as a rule change: t77's law *"every allowed key has a control"* now **exempts exactly five keys** (`RI_CTL_LEVI_NOTE`..`RI_CTL_LEVI_WHEEL`) because the allow-list now carries live inputs as well as panel-reachable parameters. Ten Levi tests that probed `0x0ECC` as "the first refused key" probe `0x0ED1` instead.

## What is not done

- **M5** (clock out, MMC in/out, clock-out LED) — unstarted, behind `RIAPP_MIDI_CLKOUT`, off by default, pending owner decision 1 on whether these are a Classic extension or Power Mode.
- **M6** — this article; the interop spec's "Where we are today", the todo and the ledger are the remaining items.
- **R5** (303/808/909 note input), **R6**, **R7** (SMF), **P3**, **P4** — untouched.
- **The 140 BPM hardware gap** above, and the unexplained panel `TR STOP` mid-take (both Dell runs, no MIDI intent, no dropout log, not reproduced since).
- **The Comp switch**: traced to `4167e32` halving the auto make-up (+18 → +9 dB). The owner's call was "this was fine", so no sound default was touched.

## The lane discipline this slice had to learn

- **`ri_build_host.sh test` does not rebuild.** A mutant that fails to compile under `-Werror` links the *previous* `.o` and false-passes. Every mutant rebuild here is **hash-verified** (`sha256sum /tmp/ri/build/*.o` before and after). A build that succeeds is not proof that anything was mutated.
- **An assert on a value equal to its default proves nothing.** Weakening the value under test to something distinguishable is what lets the mutant die.
- **Never send `quit` to the riqemu1 monitor**; read `RAM:RIAPP-EV.LOG` only after the app exits.
- **Never `avail flush`** on the Dell: library swaps need an owner reboot. A reboot wipes `RAM:`, so every staged binary is re-PUT after it.
- **The ev-log's second column is a buffer count, not a clock.** `t=` is wall clock; drops are `ch_dropped,in_dropped,camd_dropped`; `xr=` is session xruns.
- **An AROS-only proof tool must not live in `tools/`** — `midiclock.c` moved to `app/` for exactly this reason.

## See Also

- [MIDI interop R1: the follower core, the wire parser, the sync source](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md) (the pure core M1–M3 wire up; its "nothing consumes the intents" is now historical)
- [G7 Remote MIDI Control, Standard Mapping over CAMD](2026-09-26-gui-remote-midi-g7.md) (the receiver that still owns channel voice bytes, and the CAMD bug that turned out to be upstream)
- [AHI honest rate proven on the Dell; `avail flush` crashes AROS in a ROM expunge](2026-10-08-ahi-rate-dell-proof-avail-flush-crash-and-ahi-v7-verdict.md) (the same lane week: `avail flush` crash, upstream-then-carriage discipline)
- [Leviasynth v1 implementation](2026-09-28-leviasynth-v1-implementation.md) (the panel W0–W4 audits)
- [The item cull that threw away 98 percent](2026-10-05-the-item-cull-that-threw-away-98-percent.md) (the same "equivalent in production is not tested" family of lesson)
