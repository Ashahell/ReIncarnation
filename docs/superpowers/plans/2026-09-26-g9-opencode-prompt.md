# Prompt for opencode — §12.11 G9: the live app ("first sound from the panel")

> Written 2026-09-26 by the Claude session that shipped the G1–G7 GUI, the Dell owner pass, skin format 1 and automation Task 5b/5c. Everything below was checked against `main` at `ff83e68` (plus the Vulkan4AROS wiki at `39b115d0`). Where a fact is not verified, it says **verify**.

## 0. Who you are and what you are building

You are continuing ReIncarnation: a clean-room re-creation of Propellerhead ReBirth RB-338 2.0.1 for AROS x86-64. Fidelity comes from the ReBirth 2.0.1 Owner's Manual first (evidence class **E1**, cite printed page numbers) and the original hardware second. Where the manual is silent, you pick a pragmatic default (**E0**), record it as a ledger row or named constant with a revisit note, and pin it with a test. The owner accepts well-argued E0 defaults and tunes them from real use ("Actual usage will reveal if we need to adjust"). Ask the owner only when a choice is hard to reverse (file formats, directory layouts, song-chunk semantics).

**The gap you close.** Every part exists in isolation:
- the engine (303A/303B/808/909, FX inserts, pan/send/delay, strip level, meter taps);
- the sequencer (transport, pattern banks, song track, streaming player, automation lanes with render-safe publish);
- the formats (RBNG v1.2 with ATRK);
- the full GUI (all sections, keyboard, MIDI remote, skins, zoom).

**But nothing plays live.** `audio_io/audio_ahi_play.c` renders a whole song and then streams it with blocking `CMD_WRITE`. The proof app `RISECT` has no engine. The owner noticed on the Dell: *"the 303 doesn't produce sound, is that expected?"*

G9 delivers **one AROS application in which the panel plays sound in real time**, driven by the transport. It also makes knob moves audible within one device buffer, and makes the GUI meters show the engine's own peaks (closing G6b).

## 1. Read first (in this order)

1. `llm-wiki/index.md` → the sections **"GUI (§12.10)"**, **"Reviews"** and **"Lane infrastructure"**, and the records they link. At minimum read:
   - `2026-09-26-dell-owner-pass-and-skin-format-1.md`
   - `2026-09-26-automation-task-5b-5c-lane-keys.md`
   - `2026-09-26-automation-lanes-tasks-2-to-4.md`
   - `2026-09-26-streaming-player.md`
   - the WBS 2.7 AHI records (`2026-09-22-wbs27-negotiate.md`, `2026-09-23-wbs27-close-path-interruptible-play.md`)
   - `2026-09-22-dell-reference-box-iteration-abiv1-target.md`
2. `docs/superpowers/specs/2026-09-20-reincarnation-spec.md`:
   - §4 realtime contract **[LOCKED]**, especially §4.1 single render thread and §4.2 "hook only Signal()s, render lives in a Task, double buffer";
   - §5 one renderer for live and offline;
   - §8 event contract and sort key;
   - §13 GUI and control plane;
   - §17 degraded modes (#5 under-runs: surfaced, never a hang);
   - Appendix A P-21 (render-task priority, unmeasured).
3. `docs/superpowers/specs/2026-09-26-automation-design.md` §2.2 status blocks: the lane keys (`0x0Bsp` / `0x0Cpv` / `0x0Dpv`) are also the **control-plane addresses** — see G9.1.
4. `docs/superpowers/specs/2026-09-26-skins-design.md`: format 1 is APPROVED, and "Per-module skin choice" is designed but not built (item G10.1).
5. `docs/evidence/gui/dell-g8-zoom-skin.md`: the Dell lane recipe, and three AROS bugs we found there (read §"Skins on the Dell").
6. `docs/2026-09-24-improvement-todo.md` §12.9, §12.10: what is ticked and what remains.

## 2. What exists (do not rebuild it)

**Engine** — `engine/engine.{h,c}`, single renderer, 64-frame block, stereo f64 master:
- `ri_engine_init` / `defaults` / `load(ev, nev, total, sections)` / `apply_event` / `render(out_l, out_r, n, sr)` / `render_mono`.
- Routing and knob setters: `ri_engine_assign_insert`, `ri_engine_fx_set`, `ri_engine_set_pan`, `ri_engine_set_send`, **`ri_engine_set_level`** (new: P-17 `(v/127)^2`, 127 = unity and bit-identical, 64-sample zipless slew), `ri_engine_set_tempo`, `ri_engine_set_delay`.
- Meter taps: `ri_engine_section_peak`, `ri_engine_fx_peak`, `ri_engine_comp_gr` (C2, `t78`).
- `RI_EV_AUTOMATION` is routed **by lane key, for any device**, to the same setters the knobs use (`engine_automation()` in `engine.c`, `t79_auto_delivery`, `t52` block g).
- Drum voices: `s808` (16 sounds, slot map), `s909` (11 voices; the sample layers must be bound with `ri_engine_909_bind` or they render silence — **verify** where the 909 pack loader lives before the first live 909 run).

**Sequencer** — `engine/seq/`:
- `transport.{h,c}`: laws, record gate, Stop law per E1 p. 145.
- `pattern*.{h,c}`: model, edit, emit.
- `songtrack*`, `songsteps`.
- `player.{h,c}`: streaming; `ri_player_block(p, track, loop, map, ppq, tick_start, tick_end, out, cap)`; banks are live-read, non-owning.
- `riseq.{h,c}`: `RISeq`, the snapshot handshake `RiSeqRequestSnapshot` (GUI) / `RiSeqBeginBuffer` (render); loop cursor.
- `autolane*`:
  - lane model;
  - punch/sweep laws;
  - edits;
  - chase;
  - windowed emit with `RIAutoCarry`;
  - **render-safe publish `RIAutoPub` (front/back, `ri_auto_pub_request`/`apply`)**;
  - `ri_auto_allowed` (100 keys).
- `sched.h`: `RIEvent`, the §8 sort key.
- `clock.h`: `RITempoMap` (rate field — see the sample-rate note in G9.2).

**Audio** — `audio_io/`:
- `audio.{h,c}`: the W1 AudioObject API, `au_render_frames` (the **legacy mono 303 path — not the engine**), WAV/AIFF sinks, null backend.
- `audio_ahi.{h,c}` + `audio_ahi_play.c` (AROS only): `au_ahi_negotiate`, and `AuPlay`/`AuPlayEx`, which render the whole song first and then do a blocking `CMD_WRITE` stream.
- `probe_ahi.c`: the M1.1 probe of the low-level API with a PlayerFunc hook. On the Dell it measured **64 frames accepted**, best mode `0x003E0001` @ **44100 Hz / 16-bit** (**verify** the rate again at G9.3 start).

**Formats** — `project/rbng.{h,c}`: song + banks + track, `AUTO` legacy / `ATRK` v1.2 (caller buffer `atrk/natrk/atrk_cap`), `MODR` mod references.

**GUI (host-tested pure C + one Zune canvas):**
- `gui/ctlreg` (243 controls):
  - **`ri_ctlreg_auto_id(d)`** gives every control its lane key, 0 when it has no engine delivery;
  - `ri_ctlreg_help` (bubble names);
  - section/kind tokens.
- `gui/panelgeo` (Q units, zoom 0..3).
- `sect303/808/909/sectmix/sectfx/sectpat/secttr` + `sectui` dispatch.
- `keymap` (Appendix E), `panelui` (focus, keys, taps, live feed, mixer/FX pointers), `livestate` (position law, meter/GR scales — currently fed by a **stand-in clock**), `midimap`.
- `skin` (format 1) + `skin_aros` (loader).
- `gui/widgets/rsection.mcc.c` (the one canvas: off-screen double buffer, bubble help via `MUIM_CreateShortHelp`, skin hooks).

**Proof apps:**
- `app/sectproof.c` → `RISECT [303|808|909|mix|fx|tr|keys|live|remote] [demo] [mod=<name>] [zoom=0..3]`.
- `app/midisend.c`.
- `app/main.c` is still a bare Intuition window (Task 12 leftover).

**Tests and audit:** host tests through `t79`; the next free number is **t80**. `bash scripts/ri_audit.sh` runs everything (Phase 12 GUI, seq incl. t74/t77/t78/t79, AROS compile-only loops, gates).

## 3. Hard rules (non-negotiable)

1. **TDD with a behavioural RED.** Add stub bodies, run, and record the first failing `RI_ASSERT` line in the commit body; then GREEN. A missing header is not RED. Every invariant gets at least one **mutation proof** (mutate, see FAIL, revert, see PASS). The mutant must **compile**: with `-Werror`, an unused variable or label makes the build fail and silently re-runs the old binary. Check for build errors before trusting a "SURVIVED" or a "FAIL".
2. **`bash scripts/ri_audit.sh` = `AUDIT 0/0 PASS` before every commit.** If another session's WIP is in the tree:
   - verify in a scratch `git worktree` at HEAD plus your files;
   - it needs a sibling symlink `<scratch>/Vulkan4Aros → /home/miller/Work/projects/Vulkan4Aros`;
   - never edit `ri_audit.sh` while an audit runs;
   - `scripts/` must keep exactly **5** files.
3. **Commit only your own files** (never `git add -A`). Messages end with `[§12.11 G9]` (or the item tag) and `Co-Authored-By: OpenCode <noreply@opencode.ai>`. Run `git pull --rebase --autostash` before `git push`. Another Claude session works in this repo in parallel on GUI/skins/automation. **Check `git status` for foreign modified files before every commit, and never stage them.**
4. **Realtime contract (spec §4, LOCKED):**
   - the render path has no allocation, no DOS/Intuition calls, no locks/`Forbid`/`Disable`, no unbounded loops and no mutable statics;
   - AHI hooks only `Signal()`;
   - GUI → render goes through an SPSC FIFO and snapshot swaps at buffer boundaries.

   The audit greps render code literally, so avoid the string `free(` even in engine comments.
5. **One renderer (spec §5):** live and offline must produce the **same samples** for the same song and the same control history. Prove it with a host test that renders live-style (device-buffer chunks through the control plane) and compares bit-exact against the offline export.
6. **Layering:**
   - `engine/` never includes `gui/` or `project/`;
   - `autolane.h` sees `transport.h` only — `t77` has a banned-word guard that even catches comment words like "mixer";
   - pure modules are host-tested; AROS-only files carry `#ifndef __AROS__ #error`.
7. **AROS lessons** (each one cost a crash or a day):
   - `GetAttr`/`GetDTAttrs` store an **IPTR**; never pass a `ULONG`/`LONG` target;
   - `ObtainBestPen` per screen in `MUIM_Setup`, released in `MUIM_Cleanup`;
   - event handlers on `_win(obj)` in `MUIM_Setup`;
   - paint on every `MUIM_Draw`;
   - one raw-key owner per window;
   - fonts do not scale with zoom;
   - `ExAll`: request `ED_TYPE`, walk `ed_Next`, loop while it returns nonzero, `ExAllEnd` on an early stop (`ed_Size` is the file size);
   - picture size from `PDTA_BitMapHeader`;
   - an off-screen friend bitmap + one `BltBitMapRastPort` per draw avoids flicker;
   - a Software Failure only suspends the task, so to locate a crash, append one line per step to a `RAM:` log.
8. **Clean-room (spec §1):** no ReBirth or Roland pixels or samples, no `.rbm`/`.rbs` importer (OPEN-10).

## 4. Lanes and how to prove things

**Dell E6320** — real hardware, **ABIv11**, native **1366x768**, real HDA audio. This is the lane for G9 sound.
- Agent `e6320`, spool `/tmp/spike_spool_laptop`:
  `python3 /home/miller/Work/projects/Vulkan4Aros/scripts/spike_server.py submit --spool /tmp/spike_spool_laptop --wait 180 --put LOCAL:RAM:X --exec "Run >NIL: RAM:X …" --exec "wait 3" --ui-windows --ui-capture /ABS/PATH.png,2 --ui-close <title> --get RAM:LOG:/abs/local`
- **Always pass absolute local paths.** Relative `--get`/`--ui-capture` paths land in the spike server's cwd (Vulkan4Aros).
- **The Dell has no `tar`**, so `--put-tree` fails; put files one by one.
- Captures are **1/2 scale minimum** (scale 1 is refused at 1366x768). 1-pixel lines whose y positions share one parity vanish from them, so the owner judges thin lines on screen.
- **Only the owner can reboot the Dell.** If the agent drops (ConnectionReset), stop and ask them.
- **v11 build** (not yet a script target — making it `ri_build_aros.sh sections v11` is welcome, keeping the 5-file rule):
  - CC `/home/miller/Work/projects/Vulkan4Aros/src/abi/v11/toolchain-core-x86_64/x86_64-aros-gcc`;
  - SDK `…/src/abi/v11/sdk/Developer`;
  - CFLAGS `-std=gnu99 -O0 -mcmodel=large -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing -ffixed-r12 -fno-stack-protector -Wall -Wa,-W -I$SDK/include -I$SDK/include/aros/stdc -I.`;
  - link `-mcmodel=large -mno-red-zone -ffixed-r12 -nostartfiles -no-pie $OBJS $SDK/lib/startup.o -L$SDK/lib -lmui -lamiga -lcamd -lintuition -lgraphics -lutility -ldos -lexec -lautoinit` (AHI is opened at run time with `OpenDevice("ahi.device", …)` as `audio_ahi.c` does, so no extra link library is expected — **verify** when the first AHI object links);
  - gates: 0 unresolved (`x86_64-aros-readelf -s … | awk '$7=="UND" && $8!=""'`); r12 moves are **expected** on v11 (the r12==0 gate is v1-only);
  - build from a clean worktree at HEAD plus your files, so a sibling's WIP never ships.
- Installed on the Dell: `SYS:Classes/ReIncarnation/Mods/{808-RI,Template,Stale}`. `Stale` is a deliberate wrong-size test fixture: keep it, or delete it and say so. Two old Software Failure requesters from diagnostic builds may still be open (harmless).

**riqemu1** — private QEMU AROS lane, **ABIv1**, 1280x1024.
- Launcher `/home/miller/Work/vms/start_riqemu1.sh`; spool `/tmp/spike_spool_priv`; spike port 9295; HMP monitor `tcp 127.0.0.1:4477` (Python socket, no `nc`).
- **Owner rule: every reboot must come back at 1280x1024** (GRUB pinned; verify with a monitor `screendump`). Before a reset, move pending `jobs/*.json` aside and wait ~15 s.
- Keyboard injection works (`sendkey`); mouse does not.
- **No audio device is configured.** Use riqemu1 for the null backend, the render-task timing, and the ABIv1 build gate (`bash scripts/ri_build_aros.sh sections`, r12 moves == 0), not for sound.
- `camd.library` there is our fixed build; `DEVS:Midi/debugdriver` is parked.

**Never reboot or reset a lane another session is using.** Check the spool's `sessions/*.json` state first.

## 5. G9 scope — tasks, in order (each its own RED→GREEN commit)

### G9.0 Design note (short; brainstorm, then write)
Create `docs/superpowers/specs/2026-09-2x-live-app-design.md`. Spec §4 already decides the architecture: one render Task, the hook `Signal()`s, a double buffer, SPSC in, snapshots at boundaries. The note fixes:
- the live-session state machine;
- the control-plane message format;
- the sample-rate policy;
- the xrun/degraded behaviour;
- what the GUI shows.

Ask the owner only about irreversible points. Recommended E0s:
- **Sample rate:** run the whole session at the negotiated device rate. The engine takes `sr`; the tempo map carries a rate. Offline export stays 48 kHz. Record this as a ledger row, and prove live == offline **at the same rate**.
- **Control plane:** a fixed-capacity SPSC ring of `{ uint16_t key; uint8_t val; uint8_t flags; }`, drained at buffer start into `RI_EV_AUTOMATION` events at sample 0 of the buffer. Overflow keeps the newest value per key (coalesce), is counted, and is never silent.

### G9.1 Control plane (pure, host, `engine/seq/` or `engine/ctl/` — `t80`)
- SPSC ring: single writer (GUI) and single reader (render); power-of-two capacity; no alloc; coalescing on overflow; drop counter; drained into a caller-provided `RIEvent` array.
- The key space is exactly the lane keys: `ri_ctlreg_auto_id()` on the GUI side and `ri_auto_allowed()` as the gate, so a key the engine ignores is refused at enqueue and counted.
- The same path serves knob moves, MIDI CC (`midimap` → registry → key) and automation playback. **One path, no second setter route.**
- Tests: FIFO order; coalescing law; wrap-around; refused keys; a drained event is render-identical to calling the setter directly (reuse the `t79` pattern).

### G9.2 Live session core (pure, host — `t81`)
`struct RILiveSession`, render side, all caller-owned storage:
- engine;
- transport and `RISeq`;
- player plus banks and track;
- `RIAutoPub` front lane plus `RIAutoCarry`;
- a scratch event array.

`ri_live_render(s, out_l, out_r, frames, sr)` does, per call:
1. apply the staged snapshot (`RiSeqBeginBuffer`) and the automation publish (`ri_auto_pub_apply`);
2. drain the control ring;
3. compute the tick window from the transport;
4. build the events: player block + automation emit (lane order, carry) + control events, merged with the §8 sort;
5. call `ri_engine_load` and `ri_engine_render` in 64-frame blocks;
6. advance the transport.

Handle Stop/Play/loop wrap per the transport laws. The automation spec requires **punch-out-all, then chase** on every discontinuity, including a loop wrap.

**Key test:** render a fixture song through `ri_live_render` in device-buffer chunks (64, 128, 256 and an odd size) and compare against the offline export of the same song. They must be **bit-exact**, with and without an automation lane, and with a control move injected at a buffer boundary (the offline side gets the same move as an `RI_EV_AUTOMATION` at that sample).
- Chunk-agnostic output is a spec requirement; the engine already is.
- Keep `au_render_frames` (the legacy mono path) untouched; the live path uses the engine.

### G9.3 AROS render task + low-level AHI backend (`audio_io/`, AROS only)
- `AHI_AllocAudioA` with a `PlayerFunc` hook that only `Signal()`s the render Task.
- The render Task:
  - priority per P-21 — **measure** it; start above the GUI, below input.device;
  - `Wait()`s, then renders exactly the device buffer through `ri_live_render` into a preallocated double buffer;
  - converts f32 stereo to the negotiated format (16-bit on the Dell, with deterministic rounding like `auf_f32_to_s16`);
  - hands the buffer to AHI.
- **Under-runs:** counted (`AUQA_XRUN_COUNT`), shown in the GUI, never a hang (§17 #5).
- **Clean stop:** no leaked device, signal or task.
- Keep `ahi.device` `CMD_WRITE` as the documented fallback only if the low-level path fails on the Dell. **Measure** which one runs at 64/128 frames without xruns.
- Evidence file: `docs/evidence/audio/live-render-task.md`, with:
  - buffer size;
  - xruns over 5 minutes;
  - measured render time per buffer;
  - CPU headroom.

### G9.4 The application (`app/riapp.c` → `RIAPP`; replaces the bare `app/main.c` window or supersedes it — say which)
- **Layout:** one window with the full panel, reusing the RISECT `keys`/`live` layouts — transport + pattern sections + one synth by default, the whole rack where the screen allows. Respect the zoom-fit rule in G10.2.
- **Controls:**
  - transport keys (Play/Stop/Record per Appendix E and E1 p. 145);
  - every knob, fader and switch sends its lane key through the control plane;
  - pattern edits go through the existing pure edit functions on the GUI-side banks, then a snapshot request.
- **Meters:** `livestate` reads `ri_engine_section_peak`, `ri_engine_fx_peak` and `ri_engine_comp_gr` from a **render-published snapshot** (a small struct copied at buffer end; the GUI never reads engine internals). This closes **G6b**: the stand-in clock goes, and the position display follows the real transport.
- **Startup:**
  - load a built-in demo song (the same fixture as the G9.2 test);
  - AHI missing → null backend + the exact `RI_AUDIO_NULL_MSG`;
  - a missing 909 sample pack → the 909 renders silence and the GUI says so (§17, never silent).
- **Owner proof on the Dell:** the owner presses Play, hears the demo, turns a 303 cutoff knob, a 808 level and a mixer fader, and hears each within one buffer. Record their words in the evidence file. Then run a 5-minute soak with the xrun count.

### G9.5 Recording automation from the panel (after G9.4 sounds right)
- In RECORD state, knob moves also call `ri_auto_touch` on the GUI-side back lane:
  - sweep between updates per the pass-marker law;
  - `ri_auto_pass_end` on Stop or Record-off;
  - then `ri_auto_pub_request`.
- Playback moves the on-screen controls: the GUI chases displayed values from the published lane at the playhead, read-only.
- **"Lane full" is shown** (the sticky `RI_AUTO_FLAG_FULL`), never silent.
- Saving and loading go through `rbng` with the caller-provided ATRK buffer.
- Proof: record a filter sweep on the Dell, stop, play it back, save, reload, play again. All three passes must be audibly identical, and automated via the live-vs-offline test.

## 6. G10 — carry-overs (after G9, or when G9 is blocked on the owner or a lane)

1. **Per-module skin choice** (owner requirement): implement the design in `skins-design.md` §"Per-module skin choice":
   - per-section active table;
   - refcounted shared loads;
   - Ctrl+M cycles the focused section;
   - "whole panel" choice;
   - optional song chunk only when sections differ (**owner review of the chunk before it ships** — it is a file format).
2. **Zoom:**
   - `app/sectproof.c` hard-codes zoom 0 for the mix/fx/tr/keys groups; honour `zoom=`.
   - 2x does not fit the Dell's 1366-px screen for 303/808/909 (about 75–80 px is clipped on each side). The owner has not ruled; the recommended E0 is to cap the zoom at the largest factor whose width fits the screen, and to note it. Keep it switchable in one place.
3. **Deferred minors from the G8 review:**
   - dead `s_mod_pending` flag;
   - missing-mod readout vs name divergence;
   - `read_file` short-read contract;
   - the `t76` drag-law loop is vacuous over z;
   - a joined cosmetic line in `panelui`.
4. **909 knobs with no engine parameter** (BD Attack, SD Tone/Snappy, AC Level, Flam): engine DSP work, then bind in the registry, then the lane key appears automatically. `t77` then shrinks the unrecordable census from 28.
5. **camd:** the fix is preserved as `docs/evidence/gui/0023-camd-mysprintf-varargs.diff` (placement note beside it). The fixed tree exists only in an ignored build tree, so landing it is time-sensitive. The v1 series commit needs the owner of the Vulkan4Aros branch `t8-workgroup-size`, and the upstream PR needs `prs.py`. The debugdriver load hang is open (riqemu1, lane debug).

## 7. Owner decisions still open — do not decide; list them in your final report

- Dist exclusivity.
- The G5/G7 E0 items listed in `docs/evidence/gui/keyboard.md` and `midi.md`.
- 909 tap level.
- GR meter full scale.
- Meter floor.
- OPEN-10 (ReBirth file import).
- 2x zoom policy on small screens (recommendation above).
- The per-section skin song chunk (G10.1).

## 8. Definition of done for G9

- `RIAPP` on the Dell plays the demo song live. Knobs, faders and switches are audible within one device buffer. Meters and position come from the engine and transport. Xruns are counted and shown, and a 5-minute soak is recorded. The owner's listening notes are in `docs/evidence/audio/live-render-task.md`.
- Host tests prove:
  - live == offline bit-exact at the session rate, across buffer sizes, with automation and injected control moves;
  - the control-plane laws, with mutants;
  - no alloc, IO or statics in the render path (audit greps).
- The ABIv1 build gate passes (riqemu1, null backend run shown), and the v11 Dell build is clean.
- Records:
  - a raw article in `llm-wiki/raw/articles/` with an index entry under a new "Live app (§12.11)" section and a log entry at the top;
  - AROS-general lessons cross-posted to the Vulkan4AROS wiki (worktree of `origin/main`, `git push origin HEAD:main`, remove the worktree after);
  - tracker ticked.
- Final report to the owner:
  - results first;
  - commits;
  - what was measured on which lane;
  - rulings with their cost-if-wrong;
  - blocked items;
  - the open-decision list from §7.
