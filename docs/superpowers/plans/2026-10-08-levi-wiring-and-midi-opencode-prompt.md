# Prompt for OpenCode: Leviasynth wiring fixes, t107, and the MIDI work

> Written 2026-10-08 by the Claude advisor session, on the owner's request:
> *"fix the bugs, update t107 and do the midi work"*.
>
> **There are two tracks:**
> - **W (wiring):** W0–W4 fix the Leviasynth panel/engine bugs found by the
>   advisor audit, repair t107, and turn the audit into gated laws.
> - **M (MIDI):** M0–M6 bring CAMD up to date on both ABIs, make RIAPP listen
>   to MIDI at all, make the transport and tempo follow MIDI clock, give the
>   Leviasynth its own MIDI channel, and add opt-in clock/MMC out.
>
> Do W before M. Every phase ends with a commit and a short report. If you run
> short of context, stop at a phase boundary and write the handoff block (§8).
>
> **You are done when the gates in §7 for the phases you reached read PASS,
> with evidence where the gate names it.**

---

## 0. What is known (verified by the advisor on 2026-10-08)

### 0.1 The Leviasynth audit

The harness and its full output are committed beside this prompt:
- `docs/evidence/levi-wiring/2026-10-08-advisor-audit/levi_audit.c` (how to
  build and run it is in its header comment; `ctx` adds a context sweep and
  takes ~7 min);
- `docs/evidence/levi-wiring/2026-10-08-advisor-audit/levi_audit.out.txt`.

It drives every keyed registry row of `RI_SEC_LEVI` (180) and every live
encoder slot on every MODULE page (893), through `ri_engine_apply_event`, and
compares engine state and a two-note render.

**Summary line from the run:**
```
rows: 180 keyed, 19 default-mismatch, 2 state-dead, 79 silent (all contexts)
encoders: 893 live slots, 201 default-mismatch, 96 nomove, 10 nokey, 10 state-dead, 559 silent (all contexts)
```

What that means:
- **Delivery is fine.** Every key the panel sends reaches the engine, apart
  from the items in 0.3. t116 (`bridge == direct setter`) passes, and so do
  t106, t110 and t83.
- **"silent" is not "broken".** Those controls change engine state, but a
  two-note render in the nine contexts the harness tries is bit-identical.
  Most need context the harness does not build: the sequencer needs the
  transport running, matrix routes and macros need a source and destination,
  velocity/polyat amounts need velocity or pressure input, glide needs legato
  mono notes, some per-op params only matter on ops the default algorithm
  does not sound. W4 triages them; it does not have to make each one audible.

### 0.2 Bug 1: the panel shows values the engine is not running at startup

- RIAPP's startup burst sends every sounding panel default through the bridge
  so that "the engine adopts the panel" (`app/riapp.c` ~lines 2618–2666). It
  covers 303A/303B, 808, 909, the FX canvases, the mixer strips including the
  Levi strip, and MASTER. **It does not send the Leviasynth section.**
- So 19 registry rows and 201 encoder targets show one value while the engine
  plays another, until the user touches the control. Each is a `DEFAULT` line
  in the audit output.
- **Worst example:** Cutoff and Analog Cutoff show 96, which maps to
  ~3437 Hz through `levi_set_param_ui`, but `levi_init_set` starts both at
  `RI_LEVI_DEF_CUTOFF` = 12000 Hz (`engine/dsp/levi.h:138`). The first touch
  makes the sound jump.
- Others: Reso, Ratio, Analog Reso, the voice ADSR (46–49), Width, Bend Range,
  Vibrato Rate, Glide Time, Delay Wet/FB Tone, Reverb Tone/Hi Damp, the Pre/Post
  FX presets, every per-oscillator envelope (`0x0F` block), LFO rate/speed and
  the LFO step editor values.
- **Capacity trap:** `RI_CTL_CAP` is 256 (`engine/seq/ctlplane.h:13`). The
  Levi section alone is ~180 row keys plus ~730 per-op/env/LFO keys. A single
  burst into the plane overflows it. Read what `ri_ctl_send` does when full
  (refused? coalesced? counted in `refused`?) before choosing a design.
- **Related question to answer, not assume:** when a song or RBNG file with
  Levi data loads, does the Levi panel follow the engine, or only the engine?
  If the panel stays at its defaults while the song sets the engine, that is
  the same bug class in the other direction. Report it; fix it in W2 if the
  fix is the same mechanism.

### 0.3 Bug 2: Morph Pos only works if it is set after the slots

- `levi_set_param_ui` case `RI_CTL_LEVI_MPOS` (`engine/dsp/levi.c` ~line 2909)
  converts the 7-bit knob to an internal position using
  `levi_morph_slots()` **at the moment the knob moves**, and stores only the
  converted `v->mpos` (`levi_set_mpos`, ~line 4724).
- With fewer than two active slots the result is 0. Nothing recomputes it when
  slots are added, removed or the algorithm mode changes (`levi_set_slot`
  ~line 4709, `levi_set_amode`).
- **Consequences:** set Morph Pos first, then fill slots → the sound stays at
  slot 1 while the knob shows 127. Change the slot count after setting it →
  the position is wrong for the new list. Automation/song replay order decides
  the sound.
- The audit shows it as `STATE row 93 Morph Pos key 0e34=127` and the same on
  the ALGORITHM page encoder: on a default engine the key changes nothing.

### 0.4 t107 is stale (3 failures)

`scripts/ri_build_host.sh test t107_levi_sect` fails 3 asserts:
- `tests/unit/t107_levi_sect.c:158` "dead slot inert" and `:160` "dead slot
  blank": the test still says DIGITAL FILTER page 1 slot 6 is dead "until P9".
  P9b (`e0e4fbd`) made it live (`VEL>ENV`, `RI_SLEVI_DVEL`,
  `gui/sectlevi.c:385`).
- `:214` "module clamp (RIBBON, P8d)": expects a clamp to 36 (`M_RIBBON`). P9c
  (`51d0e56`) added `RI_SLEVI_M_PERF` = 37 and `RI_SLEVI_NMOD` = 38.
- The test's last commit is `d5f2f96` (P8d); P9b–P9d never updated it.

### 0.5 Other audit residue W4 must classify (do not assume they are bugs)

- `STATE row 206 / SEQUENCER slot 4 CLEAR key 0eb7=1`: clearing an empty
  sequence may be a correct no-op.
- `STATE enc LFO n 3/3 slot 1 STEP` (value 0) and `slot 3 RAMP` (value 1):
  probably editor cursor/command slots where the harness picked the current
  value or a command that needs a selected step. Confirm against
  `levi_set_lfo_stepctl` and the P8e article.
- `NOMOVE`: the PARAM slots on MATRIX 1|2 … 31|32 (slots 3 and 7) and MACRO n
  Rk|k+1 (slots 2 and 6) do not move with no destination chosen. Probably by
  design (the param list depends on the destination). Confirm.
- `NOKEY`: LFO n pages 2/3 and 3/3, slot 8 STEP EDIT: no lane key. Probably a
  UI-only mode switch. Confirm.

### 0.6 What MIDI does today

- **G7 (done, `d576987`):** `gui/midimap.{h,c}` maps the 99 ReBirth Appendix C
  CCs and the note tables on one channel; Sync LED from clock. It is proven
  through real `camd.library` on riqemu1, **but only in `RISECT remote`**.
- **RIAPP opens no MIDI at all.** `grep -n -i "midi\|camd" app/riapp.c` finds
  only a comment ("the MIDI slice owns focus 5", ~line 2565). The live app
  has no remote control, no clock, no notes.
- **PAL:** `platform/aros/midi_camd.c` is the CAMD backend for
  `ri_pal_midi_*`: one receiver node (`MIDI_RecvSignal`, `MIDI_MsgQueue 512`),
  a cached sender, and `ri_pal_midi_poll()` meant to be called from the app
  loop. `time_us` is always 0 ("pending T8 clock").
- **R1 (`47debed`, `50dc90a`, `622907d`):** `midi_io/midi_follow.{c,h}` is a
  pure follower core (t125/t126/t127): BPM estimate and lock, a wire parser,
  dropout latch, `midi_sync_*` Internal/MIDI source. **Nothing consumes its
  intents.** The old `midi_clock_*`/`midi_mmc_cmd` in `midi_io/midi.h` are
  also unwired.
- **No Leviasynth MIDI:** every one of the 228 `R(LEVI, …)` rows has
  `RI_MIDI_CC_NONE`, and nothing in `app/` calls `levi_note_on`/`levi_note_vel`.
  The Leviasynth plays only from chord steps and songs.
- **No output** of any kind: no clock, no notes, no CC, no MMC.

### 0.7 Spec conformance gap in R1 (fix in M1)

- MIDI 1.0: a receiver in sync mode does **not** start on `0xFA` Start or
  `0xFB` Continue; it starts on the **next `0xF8`** clock (MIDI 1.0 spec,
  "Syncing Sequence Playback"; see §1.6).
- `midi_follow_start`/`midi_follow_continue` (`midi_io/midi_follow.c`
  ~lines 72–89) emit `PLAY_START`/`CONTINUE` immediately on the status byte.
  Applied to the transport as is, ReIncarnation would start one clock
  (~20 ms at 120 BPM) early.
- SPP: `midi_follow_spp` scales 16ths × 24 to the engine's PPQ 96 (pinned by
  t126). Keep that. Decide (E0, ledgered) what an SPP **while running** does;
  the spec intends SPP to be sent while stopped. Recommended: apply SPP only
  while stopped, count and ignore it while running.

### 0.8 AROS: CAMD status, verified against upstream on 2026-10-08

- **Our G7 fix is now upstream, and so is the debugdriver hang.** On
  `aros-development-team/AROS` master (fetched 2026-10-08, tip `7b9b93dcb1`):
  - `28ec43a517` (2026-10-05, Jaime Dias) "camd: format cluster names through a
    va_list": the same `mysprintf` → `VNewRawDoFmt` fix G7 made locally.
  - `cb8c4c5f39` (2026-10-05, Jaime Dias) "camd: skip arena hunks when scanning
    a driver for MidiDeviceData": `LoadDriver()` read a hunk size of 0 from the
    ELF arena loader and scanned ~4 GB off the end of memory. That matches the
    G7 article's open item "loading `DEVS:Midi/debugdriver` blocks the first
    camd open".
- **Neither fix is in our local trees:**
  - `Vulkan4Aros/src/abi/v1/AROS` HEAD `c5e043a2ff` (2026-10-01) lacks
    `28ec43a517` (`git merge-base --is-ancestor` says no).
  - `Vulkan4Aros/src/abi/v11/AROS/workbench/libs/camd/strings.c:47` still has
    `void *start=&fmt+1;`. **So the Dell's camd.library (ABIv11) cannot connect
    two links by name**, unless a fixed build was already deployed there by
    hand. Check the Dell before assuming either way.
  - No camd patch exists in the carriage (`src/abi-patches/v1/aros/` has none;
    the G7 diff lives only in `ReIncarnation/docs/evidence/gui/`).
- **CAMD facts that shape the bridge** (read in upstream source):
  - Default link masks pass everything: `ml_EventTypeMask = ~0`,
    `ml_ChannelMask = ~0` (`addmidilinka.c` ~line 74). But `CMD_All` in
    `midi/camd.h` **excludes `CMF_RealTime`**; if you ever set
    `MLINK_EventMask, CMD_All`, clock stops arriving. Use `CMD_All|CMF_RealTime`
    or leave the default.
  - **CAMD timestamps are not arrival times.** `mm_Time` is copied from the
    *sender node's* `mi_TimeStamp` pointer (`goodputmidi.c` ~line 82,
    `midifromdriver.c` ~line 104), which defaults to a dummy
    (`createmidia.c` ~line 70). A follower needs its own arrival stamp.
  - **Do not use `MIDI_RecvHook`.** `mididistr.c` (~lines 75–130) calls
    `h_Entry` directly with a BarsnPipes-style register prototype, not
    `CallHookPkt`; on x86-64 that is a five-argument C call. Use signal + queue.
  - A full queue sets `CMEF_BufferFull` and **drops** further messages
    (`mididistr.c` ~line 24). Count drops via `GetMidiErr`.
  - Small upstream bug spotted in passing: the SysEx 3-byte filter compares
    `sxf_ID1` three times (`mididistr.c` ~lines 52–56). Not ours to fix now;
    list it in the report as a possible upstream follow-up.
- **USB-MIDI on the Dell** goes through Poseidon `camdusbmidi.class`
  (`rom/usb/classes/camdmidi/`); hosted builds have
  `arch/all-unix/devs/midi/hostmidi.c`.

### 0.9 What Ableton does on the wire (for the Dell proof)

- **As master,** Live in **Song** mode sends SPP + Continue every time the play
  position changes, and Start from the beginning. In **Pattern** mode it sends
  only Start, at the next bar, for devices that do not understand SPP. It has
  a "MIDI Clock Sync Delay" (ms) setting.
- **Live has no native MMC** (it is SysEx, which Live does not handle; Max for
  Live or third-party tools needed). So MMC out is for other gear, and MMC in
  will not come from Live.

---

## 1. Read first

1. **This repo:**
   - `docs/superpowers/plans/2026-10-06-levi-perf-opencode-prompt.md` §3
     (the repo rules; §2 below repeats the ones that matter);
   - `docs/superpowers/specs/2026-09-29-interop-requirement.md` (R1–R12, the
     realtime constraints, the test plan, the seven open owner decisions);
   - `docs/superpowers/specs/2026-09-28-device-asm-leviasynth-requirement.md`
     (E1 rules: the manual stays out of the repo; cite pages by number);
   - `gui/midimap.{h,c}`, `tests/unit/t73_midimap.c`, `app/sectproof.c`
     (`RISECT remote`), `app/midisend.c`;
   - `platform/pal/ri_pal_midi.h`, `platform/aros/midi_camd.c`;
   - `midi_io/midi_follow.{c,h}`, `midi_io/midi.{c,h}`, t125/t126/t127;
   - `engine/seq/ctlplane.{c,h}`, `gui/panelctl.c`, `engine/live.c`,
     `engine/engine.c` (`engine_automation`, `ri_engine_apply_event`),
     `engine/seq/transport.c` (`ri_tr_*`), `gui/secttr.c`;
   - `gui/sectlevi.{c,h}`, `gui/ctlreg.c` (the `R(LEVI, …)` rows),
     `engine/dsp/levi.{c,h}`;
   - `app/riapp.c` (startup burst ~2618, `sync_values` ~823, the main loop
     ~3075) and `app/core/riapp_core.c`.
2. **The wiki (`llm-wiki/`), read before you write anything:**
   - `raw/articles/2026-09-26-gui-remote-midi-g7.md` (G7, the camd fix, the
     parked debugdriver);
   - `raw/articles/2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md`
     (R1 laws; the `-Wunused-parameter` stale-object trap; the vacuous-assert
     lesson);
   - `raw/articles/2026-09-26-lane-proof-round-t2-g7-t4.md` (PAL MIDI on
     riqemu1);
   - `raw/articles/2026-09-30-leviasynth-fidelity-p1-p5.md`,
     `2026-10-01-leviasynth-fidelity-p8e-lfostp.md`,
     `2026-10-01-leviasynth-fidelity-p9a-perfsig.md`,
     `2026-10-01-leviasynth-fidelity-p9b-perfamt.md`,
     `2026-10-01-leviasynth-fidelity-p9d-glidechord.md`;
   - `raw/articles/2026-09-28-levi-filter-nan-crash.md` (why the 12 kHz
     default exists; do not reintroduce the instability);
   - `raw/articles/2026-10-02-leviasynth-fidelity-p9e-tap-tempo.md` (the
     transport tempo path).
3. **The AROS sources** (upstream master, not our snapshots):
   `Vulkan4Aros/src/abi/v1/AROS` with `git fetch origin master`, then
   `git show origin/master:workbench/libs/camd/<file>` for `mididistr.c`,
   `createmidia.c`, `addmidilinka.c`, `setmidiattrsa.c`, `getmidierr.c`,
   `putmidi.c`, and `compiler/include/midi/camd.h`.
4. **Skills:** `aros-development` (ABIv11 for the Dell, ABIv1 for QEMU; mmake
   `make workbench-libs-camd`; never `avail flush`; library swaps need a
   reboot; the opencode preflight), `karpathy-guidelines`, `karpathy-llm-wiki`
   (for M6).
5. **E1 for the Leviasynth MIDI map (M4):** the Leviasynth Keyboard Owner's
   Manual v1.2.1, MIDI CC chart section (download to `/tmp`, never commit;
   cite page numbers only).
   - Cross-check only, do not copy: midi.guide's Leviasynth page lists 110
     fixed CC assignments (CC 1 mod wheel, CC 7 master volume, CC 5 glide time,
     CC 55 digital filter cutoff, CC 71 analog resonance, among others). That
     data is CC BY-SA 4.0, so nothing from it goes into the repo; the manual
     is the source.
6. **Web sources the advisor used** (re-check, do not trust the summary):
   - MIDI 1.0 sync rules: <http://midi.teragonaudio.com/tech/midispec/seq.htm>,
     <http://midi.teragonaudio.com/tech/midispec/clock.htm>,
     <http://www.somascape.org/midi/tech/spec.html>;
   - Ableton: <https://www.ableton.com/en/manual/routing-and-i-o/>
     (sync section), <https://help.ableton.com/hc/en-us/articles/209071149-Synchronizing-Live-via-MIDI>;
   - MMC in Live: <https://forum.ableton.com/viewtopic.php?t=163735>.

---

## 2. Hard rules

1. **Clean-room:** no ASM content (samples, patches, firmware, art, manual
   text). Manuals and the midi.guide table stay out of the repo. Cite manual
   pages by number.
2. **Realtime contract (spec §4, LOCKED):** no CAMD, DOS, timer.device or
   allocation in the render path. MIDI enters through a bridge task and crosses
   to the render only through the control plane or snapshot (SPSC). Caches live
   in structs, never in mutable `static`s in render code. Do not write the
   string `free(` in engine code or comments.
3. **One renderer (spec §5):** live and offline stay sample-identical; the
   existing one-renderer tests stay green.
4. **TDD with a behavioural RED** and a **mutation proof** for every invariant
   (mutate, FAIL, revert, PASS). Mutants must compile under `-Werror`.
   `ri_build_host.sh test NAME` links stale objects: run
   `ri_build_host.sh all` first, every time. Back up a file before mutating it.
   **An assert on a value equal to its default proves nothing.**
5. **`bash scripts/ri_audit.sh` ends in `AUDIT 0/0 PASS` before every commit.**
   If foreign WIP is in the tree, verify in a scratch worktree (with the
   `Vulkan4Aros` sibling symlink). `scripts/` keeps exactly the files it has.
6. **Commits:** only your own files (never `git add -A`); tag `[levi-midi]`;
   trailer `Co-Authored-By: OpenCode <noreply@opencode.ai>`; **do not push**.
   In `Vulkan4Aros`, commit on a branch, never push, never open a PR.
7. **E1-silent points:** pick one recommended default, make it a named
   constant, add a ledger row with a revisit note, pin it with a test (owner
   preference, see `owner-pragmatic-e0-defaults`). Ask instead only when the
   choice is hard to reverse: file formats, RBNG chunk IDs, directory layouts.
8. **File formats:** no RBNG change without the owner. If the Morph Pos fix
   needs a new stored field, stop and ask (§9).
9. **Extensible rack:** per-device MIDI channels and note maps are tables keyed
   by device instance. No code that assumes four (or five) devices.
10. **Lanes:**
    - **Dell (ABIv11):** only the owner reboots it. **Never `avail flush`**
      (it crashed `tlsf_freevec` on 2026-10-08 and wedged the agent). A
      replaced `camd.library` takes effect only after an owner reboot. Never
      quit an RIAPP you did not start (`--ui-windows` first). Deploy test
      binaries to `RAM:`. One `--ui-capture` per job. Read logs with `--get`.
    - **riqemu1 (ABIv1):** spooler port 9295; if anything restarts it, it
      must come back at 1280x1024 (verify with a monitor `screendump`).
    - Do not touch lanes other sessions use.
11. **Owner decisions are the owner's** (§9). Where this prompt gives an E0
    default for one, implement it as **opt-in, off by default**, ledger it as
    "pending owner decision N", and keep it reversible.

---

## 3. Phases

### W0: baseline

1. `scripts/ri_build_host.sh all`; run the full unit suite and record which
   tests fail on HEAD (expected: only t107, 3 asserts).
2. Build and run the audit harness (default and `ctx`); confirm the summary
   lines in §0.1. If they differ, stop and report why before changing code.

### W1: t107

1. Update the three asserts to the current design, and make them **derive
   from the constants** so they do not rot again:
   - the DIGITAL FILTER page-1 velocity slot is live, sends `RI_SLEVI_DVEL`,
     and its text is not blank;
   - pick a slot that is genuinely dead on some page (if any remain) for the
     "dead slot inert/blank" law; if none remain, assert that for every page
     `enc_live == 0` implies `set_value == 0` and empty text;
   - the module clamp is `RI_SLEVI_NMOD - 1` (`RI_SLEVI_M_PERF` today).
2. Mutation proof for each (e.g. make slot 6 dead again; clamp to NMOD-2).
3. Commit.

### W2: the Leviasynth adopts its panel at startup (bug 1)

1. **RED first:** a host test (next free `tNNN`; grep for the number) that
   builds the RIAPP core the way `app/riapp.c` does at startup, runs the
   startup adoption, drains it, and then asserts that **for every Levi key**
   (registry rows and every per-op/env/LFO/step key the panel holds) sending
   the panel's current value changes nothing (the audit's `DEFAULT` check
   becomes 0). It must fail on HEAD with the 19 + 201 mismatches.
2. **Design** (pick one, justify it in the commit):
   - (a) extend the startup burst to the Levi section, chunked so it never
     exceeds `RI_CTL_CAP`, draining between chunks the way the existing burst
     does when `!s_live`; or
   - (b) a non-realtime "adopt panel" step that applies the Levi panel state
     directly to the engine before the live session starts, using the same
     key mapping as the bridge (one function, shared, not a second mapping).
   - Whichever you choose, the mapping from panel state to keys must be the one
     the bridge uses (`ri_panel_ctl_send` / `ri_slevi_ctl_key`), so a later
     control cannot be added to one and forgotten in the other. A test pins
     that every Levi value control is covered.
   - Do **not** "fix" it by changing `levi_init_set` defaults to match the
     panel unless you can show the sound is identical for every key; the
     panel is the truth (`app/riapp.c` comment, "the panel is the truth, the
     session follows").
3. **The song-load direction (§0.2):** check what happens to the Levi panel
   when a song with Levi data loads. Report it. If it is the same bug,
   fix it with the same mechanism in reverse and pin it.
4. Mutation proof (drop one block from the adoption; the test must name the
   missing keys).
5. **Dell check (no reboot needed):** build ABIv11 RIAPP, deploy to `RAM:`,
   start it, capture the Levi tab once, and log (ev-log) the first Cutoff
   touch. Report that there is no audible jump on first touch only if the
   owner listens; otherwise say "not listened".
6. Commit.

### W3: Morph Pos survives slot and mode changes (bug 2)

1. **RED:** host tests for (i) MPOS set before the slots are filled, (ii) MPOS
   set, then a slot added/removed, (iii) MPOS set, then `AMODE` changed to
   MORPH. Each asserts the voice's effective position equals what the same
   knob value gives when set last.
2. **Fix:** keep the 7-bit knob value per voice (or set-wide) and derive
   `mpos` from it whenever the slot list or mode changes (in `levi_set_slot`,
   `levi_set_amode`, and wherever slots are restored). Keep `levi_set_mpos`
   for internal positions.
3. **RBNG:** check `t105_rbng_levi` and the Levi chunk. If the knob value is
   already what is stored, no format change. If storing it needs a new field,
   **stop and ask** (§9, decision L1).
4. The bit-exact oracle (t172) and the control-rate/bank-skip tests must stay
   green; if morph-hold or bank-skip caches key on `mpos`, invalidate them on
   the recompute.
5. Mutation proof; commit.

### W4: audit residue → gated laws

1. Classify every `STATE`, `NOMOVE` and `NOKEY` line (§0.5): by design or bug.
   Fix the bugs (each with a RED test).
2. Turn the audit's two load-bearing checks into **gated** tests (fast, no
   7-minute sweep):
   - every keyed Levi control and every live encoder slot moves engine state
     for at least one value in its range (except a documented allow-list with
     a reason per entry);
   - the W2 adoption law.
3. Pick ~10 of the `SILENT` controls across groups (seq, matrix, macro,
   velocity amounts, glide, vibrato, OscPan 4/6, Analog Cutoff 127, Width,
   Delay Type) and either build the context that makes each audible in a test,
   or explain why it is inaudible by design. Analog Cutoff at 127 (~14.5 kHz)
   versus the 12 kHz start is inaudible on two sine-ish notes for a reason;
   confirm it, do not assume it.
4. Report the remaining silent list as "needs an ear test on the Dell", with
   the context each needs.
5. Commit.

### M0: AROS CAMD prerequisites (both ABIs)

1. **Verify** the facts in §0.8 yourself on upstream master (both commit
   hashes, the file contents, our two trees).
2. **Carry both upstream commits** (`28ec43a517`, `cb8c4c5f39`) into the
   Vulkan4AROS patch carriage for v1 and v11, following how
   `src/abi-patches/v1/aros/` is numbered and listed (find the series list the
   build actually reads; do not invent a new mechanism). Credit the upstream
   commits and author in each patch header. Retire
   `ReIncarnation/docs/evidence/gui/2026-09-26-camd-mysprintf-x86_64.diff` as
   superseded by upstream (leave it, add a note).
3. **Build** `camd.library` for ABIv1 (riqemu1) and ABIv11 (the Dell) with
   `make workbench-libs-camd` in the right trees. Verify the ABI per the
   `aros-development` skill (r12 vs rdx count) and that the binary changed.
4. **riqemu1:** deploy, reboot (1280x1024, screendump), then reproduce the G7
   proof (`MIDISEND` into a cluster `RISECT remote` listens on) **and** try
   un-parking `DEVS:Midi/debugdriver`: with `cb8c4c5f39` it should no longer
   block the first camd open. Record the result either way.
5. **Dell:** first find out which `camd.library` it runs (version string,
   size, hash) and whether cluster names come out garbled (`MIDISEND
   SELFTEST` or an equivalent probe). Then stage the fixed ABIv11 build in
   `RAM:`, and **ask the owner** to install it and reboot. Do not install it
   into `LIBS:` yourself and do not flush.
6. Commit (both repos, branches, no push).

### M1: follower spec conformance

1. **RED tests** (extend t125/t126):
   - `0xFA` then `0xF8`: `PLAY_START` is emitted on the **F8**, not on the FA;
     the same for `0xFB` → `CONTINUE`;
   - FA followed by STOP before any F8: no start;
   - SPP while stopped → `SEEK`; SPP while running → no intent, and a counter
     records the ignored SPP (E0 default, ledgered; §0.7);
   - Song-mode Live sequence: STOP, SPP n, CONTINUE, F8 → seek to n×24 ticks
     then continue on that F8.
2. Implement as an armed state in the follower, keeping it pure and integer.
3. Mutation proofs; commit.

### M2: RIAPP listens to MIDI (bridge task + G7 in the live app)

1. **Bridge task** (AROS, `platform/aros/`): a dedicated process that owns the
   CAMD receiver node, waits on its signal, and **stamps every message on
   arrival** with the project's microsecond clock (`ri_time_us` if T8 landed,
   otherwise timer.device/EClock in the bridge only). It routes by byte class:
   - realtime (`F8 FA FB FC F2…`) → the follower (owned by the bridge) →
     intents into an SPSC to the app/transport side;
   - channel voice + SysEx → an SPSC the GUI drains into `midimap` (G7
     behaviour unchanged, one owner per byte class, as R1 laid out).
   - Count CAMD drops (`GetMidiErr`, `CMEF_BufferFull`) and SPSC overflow
     (oldest dropped and counted; the spec's P-19 flood cap, E0 value ledgered).
   - No `MIDI_RecvHook` (§0.8). Event mask: default or
     `CMD_All|CMF_RealTime`, never `CMD_All` alone.
   - Hot-unplug (TC-2.8.4): if the cluster disappears or CAMD errors, the
     bridge reports it and keeps running; the app never hangs.
   - `ri_pal_midi_*` gains what this needs (an arrival timestamp; a way to run
     the receive side on its own task). Keep the host backend and t-tests
     working; the host backend gets the same SPSC shape so the routing is
     host-tested.
2. **G7 in RIAPP:** drain the channel SPSC into `midimap` exactly as
   `RISECT remote` does, so the 99 CCs, the note tables, the MIDI LED and the
   Sync LED work in the live app. Focus 5 (the Levi tab) must not swallow
   ReBirth remote notes meant for focus 1–4.
3. **Settings (E0, ledgered, owner decision 3 pending):** input cluster name,
   remote channel, sync source, latency offset, Levi channel, clock-out on/off,
   all from `ENVARC:` variables with documented names and defaults (per
   machine). Not in the song.
4. **Proof on riqemu1:** `MIDISEND` script into RIAPP's cluster: the G7 subset
   from the G7 article (BD steps by note, CC 38, CC 17, a channel-2 Play
   ignored, a channel-1 Play starts) now works **in RIAPP**. Screendump.
5. Commit.

### M3: the transport and tempo follow MIDI clock (R1 applied, R4 in part)

1. Apply follower intents on the app side through `ri_tr_*` and the session
   tempo, never from the bridge into the render directly.
   - Start → play from the song start on the next F8; Continue → from the
     current position; Stop → stop; SPP (stopped) → locate.
   - Tempo: while locked, the session tempo follows `midi_follow_bpm`, with
     phase kept to the clock so long-term drift is zero (design note in the
     commit: how phase error is measured and corrected, and the bound).
   - `midi_sync_knob_locked`: the tempo control shows the measured tempo and
     is read-only while following; TAP is disabled while following.
   - Dropout: hold the tempo, then the latched single STOP (R1 laws).
   - Latency offset (ms, E0 default 0, ledgered).
2. **History (owner decision 2):** E0 default: a followed take is
   **live-only**, and the app says so (log line + status). Do not record the
   tempo history now.
3. **Host tests:** a jittered 24-ppqn stream (±1 ms and ±3 ms) drives the
   whole app-side chain; assert lock within N beats (E0, ledgered), bounded
   tempo error, zero long-term drift over 10 minutes simulated, and that
   one-renderer tests stay green with sync source Internal.
4. **riqemu1:** a `MIDISEND` realtime script (F8 at a steady interval, FA, SPP,
   FB, FC) moves RIAPP's transport; log the measured tempo and the start
   tick. riqemu1 has no sound; this is a transport proof.
5. **Dell (after the owner reboots onto the M0 camd):** USB-MIDI interface
   via `camdusbmidi.class` to a PC running Live as master, Song mode. The
   owner listens for 5 minutes and confirms the two stay in time; you capture
   the log. If the owner is not available, stop at "ready for owner proof".
6. Commit.

### M4: the Leviasynth on its own MIDI channel (R5 for the Levi)

1. **Channel table** keyed by device instance (rack rule). E0 defaults,
   ledgered: remote (G7) channel 1 as today, Leviasynth channel 2; 303/808/909
   note input is not in this phase.
2. **Notes:** note on/off with velocity → `levi_note_vel` / `levi_note_rel_vel`
   (and chord mode as P9d defines it), through the control plane or a note
   SPSC into the render, honouring the locked same-sample event ordering
   (spec §6). There is no live Levi note path today; design it once, generic
   enough for other devices later.
3. **Performance controllers:** pitch bend (Bend Range), mod wheel (CC 1),
   channel and poly aftertouch to the P9a performance signals
   (`levi_press` and friends), sustain (CC 64) if the engine supports it;
   otherwise list it.
4. **CC map:** from the Leviasynth manual's CC chart (E1, page numbers in
   comments). For each manual CC, map it to our key when we have that
   parameter; list every manual CC we cannot map and why. Put the CC numbers
   in the Levi `R(LEVI, …)` rows' cc field only if that does not collide with
   the G7 one-channel Appendix C lookup (`ri_ctlreg_by_cc` is global today);
   otherwise a per-device CC table. **A CC on the Levi channel must never move
   a ReBirth control, and vice versa** — pin that with a test.
5. Tests: t73 stays green; new tests for the channel split, notes, bend, the
   CC map (spot rows against the manual pages) and the no-collision law.
6. **Dell:** with the owner, play the Leviasynth from a keyboard or Live on
   channel 2 and turn a few mapped CCs. Ev-log ink; the owner's ear.
7. Commit.

### M5: clock out and MMC (opt-in, off by default; owner decision 1 pending)

1. **Clock out (R2):** 24 ppqn derived from the render task's sample clock,
   handed to a sender task (no CAMD in the render path). Start / Continue /
   Stop, and SPP on locate while stopped (Song-mode behaviour, which Live
   understands). Compensate the audio output latency. A host test pins the
   tick schedule against the sample clock (no drift over 10 minutes); a
   riqemu1 CAMD receiver probe logs the `F8` intervals.
2. **MMC in (R3):** wire `midi_mmc_cmd` (Play, Stop, Locate) through the same
   intent path as M3. MMC out: Play/Stop/Locate when clock out is on. Note in
   the ledger that Live itself does not send or receive MMC (§0.9).
3. Both are **off by default**, behind the `ENVARC:` setting, classified
   "Classic extension, pending owner decision 1". They must not change the
   Classic DSP path (one-renderer tests prove it).
4. Clock-out LED (R4).
5. Commit.

### M6: the record

1. **Wiki** (karpathy-llm-wiki rules: raw first, grounding invariant, Status
   blocks, index, log):
   - a new article for this work (W and M);
   - mark the G7 article's "still has to enter the Vulkan4AROS v1 patch series
     and upstream" and the debugdriver item **Outdated (2026-10-08)**: both
     fixed upstream on 2026-10-05 (`28ec43a517`, `cb8c4c5f39`), and now in our
     carriage (your commits);
   - update the R1 article's "nothing consumes the intents" with a Status
     block pointing to M2/M3.
2. **Interop spec:** update "Where we are today" and the status line (what is
   now done, what remains: R5 for 303/808/909, R6, R7, P3, P4).
3. **Todo** (`docs/2026-09-24-improvement-todo.md`): tick what is done, add
   what is open, including the SILENT ear-test list from W4.
4. **Ledger:** every E0 default this work introduced, one row each, with a
   revisit note.
5. Commit.

---

## 4. Traps

- **Stale objects.** `ri_build_host.sh test` does not rebuild; a mutant that
  fails to compile under `-Werror` silently tests the old object.
- **Ring overflow.** A startup adoption pushed through the 256-entry control
  plane in one go loses keys silently unless you check `refused`.
- **`CMD_All` has no realtime bit.** Setting it as an event mask silently
  kills clock.
- **CAMD `mm_Time` is not arrival time** (§0.8). A follower fed it locks to
  zeros.
- **Start is on the next F8** (§0.7). A follower that starts on FA is early by
  one clock.
- **The Dell camd is probably the broken one.** A "MIDI doesn't work on the
  Dell" result before the M0 reboot is expected, not a finding about your code.
- **`avail flush` on the Dell** crashes it. Library swaps need an owner reboot.
- **Live does not speak MMC.** An MMC test against Live proves nothing.
- **Two clock models exist** (`midi_clock_*` for the LED, `midi_follow_*` for
  following). Do not wire the LED one to the transport.
- **The audit's SILENT list is not a bug list.** Do not "fix" a control into
  audibility by changing its range or law without an E1 reason.
- **Morph caches.** Bank-skip and morph-hold state may assume `mpos` only
  changes through the knob; a recompute on slot change must invalidate them.

---

## 5. Out of scope

- SMF import/export (R7): needs owner decision 4 and is a file format.
- Note input for 303/808/909 (rest of R5) and note/CC output (R6).
- Stems and loop renders (P3), Ableton Link, plugins, RTP-MIDI (P4).
- Recording the followed tempo/transport history (owner decision 2; E0 is
  live-only).
- The camd SysEx filter bug (report only).
- Any RBNG format change (ask first).

---

## 6. Evidence layout

- `docs/evidence/levi-wiring/` — W phases (the advisor audit is already in
  `2026-10-08-advisor-audit/`; add your re-runs beside it, dated).
- `docs/evidence/midi/` — M phases: riqemu1 scripts, logs, screendumps; Dell
  logs and captures; the camd version/hash before and after.
- Ledger rows in the existing ledger file the G7 work used
  (`docs/evidence/gui/midi.md`) or a new `docs/evidence/midi/ledger.md` if
  that file is G7-only; say which in the report.

---

## 7. Success gates

| Phase | Gate |
|---|---|
| W0 | Baseline recorded; audit summary matches §0.1 or the difference is explained |
| W1 | t107 PASS; 3 mutants killed |
| W2 | New adoption test RED on HEAD, PASS after; audit `DEFAULT` count 0; mutant names the missing keys; song-load direction reported |
| W3 | Three ordering tests PASS; t172 and t105 green; no RBNG change (or owner asked) |
| W4 | Every STATE/NOMOVE/NOKEY line classified; two gated laws added; ~10 SILENT controls explained |
| M0 | Both upstream fixes in v1 and v11 carriage; ABIv1 + ABIv11 camd built and ABI-verified; riqemu1 G7 proof re-run; debugdriver result recorded; Dell build staged and owner asked |
| M1 | Start/Continue on next F8; SPP-while-running law; mutants killed |
| M2 | RIAPP G7 subset proven on riqemu1 through real CAMD; drop counters exist and are tested |
| M3 | Host jitter tests PASS (lock, error bound, zero drift); riqemu1 transport proof; Dell 5-minute owner proof or "ready for owner" |
| M4 | Levi channel notes/bend/aftertouch/CC map tested; no-collision law; Dell owner proof or "ready for owner" |
| M5 | Clock-out schedule test; riqemu1 F8 interval log; off by default; one-renderer green |
| M6 | Wiki article + Status blocks; interop spec, todo, ledger updated |
| every commit | `AUDIT 0/0 PASS` |

---

## 8. Reporting and handoff

After each phase, report in five lines or fewer: what changed (commit hash),
the gate result, the evidence path, any E0 default added, anything surprising.

If you stop early, write a handoff block at the end of your last report:
- the last phase completed and its commit;
- the phase in progress and exactly where it stands (files touched, tests
  RED/GREEN);
- open questions for the owner;
- the lanes' state (what is deployed where; whether the Dell is waiting on an
  owner reboot).

---

## 9. Owner decisions (list them; do not decide)

- **L1.** If the Morph Pos fix needs a new RBNG field: approve the format change?
- **D1** (interop spec decision 1). Clock out, note out and MMC out: Classic
  extension or Power Mode? (M5 ships them off by default as a Classic
  extension, pending this.)
- **D2.** Followed performances: record the tempo/transport history, or
  live-only? (E0: live-only.)
- **D3.** Sync and MIDI settings in `ENVARC:` per machine, or in the song?
  (E0: `ENVARC:`.)
- **D4.** SMF mapping for accent/slide/flam and the drum note map (blocks R7).
- **D7.** Priority of R5 (303/808/909 note input), R6, R7 and P3 after this.
- **New.** Default MIDI channel for the Leviasynth (E0: 2) and for future rack
  devices.
- **New.** Install the fixed `camd.library` on the Dell and reboot (M0).
