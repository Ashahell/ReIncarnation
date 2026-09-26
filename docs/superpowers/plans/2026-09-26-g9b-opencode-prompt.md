# Prompt for opencode — §12.11 G9b: from "first sound" to a playable ReIncarnation (multi-step)

> Written 2026-09-26 by the Claude session that made the Dell play: commits `dd93ae7` (real low-level AHI stream + RIAPP first sound) and `e884439` (render timing, small-buffer path, W capture). Both are **local on `main`, not yet pushed** — Step 0 pushes them. This prompt is split into **steps that are each independently finishable**: finish one, commit, report, then continue. If you run short of context, stop cleanly at a step boundary and write the handoff line described in §6.

## 0. Context in one paragraph

ReIncarnation re-creates Propellerhead ReBirth RB-338 2.0.1 for AROS x86-64. The fidelity order is:
1. **E1:** the Owner's Manual, citing printed page numbers.
2. **The hardware.**
3. **E0:** a pragmatic default, ledgered and pinned by a test.

The owner accepts well-argued E0 defaults ("Actual usage will reveal if we need to adjust"); ask only about irreversible choices (file formats, directory layouts, song-chunk semantics).

**As of today, `RIAPP` plays the demo song live on the Dell E6320:**
- a 303 line over an accented 808 beat;
- the 303 cutoff, strip level and pan change it audibly;
- 0 xruns over minutes at 1024-frame buffers;
- it can record its own output to a WAV.

The owner has confirmed by ear: *"we hear both"*, *"balance is good now, pan works"*, *"sounded good again"*.

## 1. Read first

1. `docs/evidence/audio/live-render-task.md` — **everything measured about the live path**; the sections "Dell first sound" and "Buffer ladder, timing, capture" are required reading.
2. `docs/superpowers/plans/2026-09-26-g9-opencode-prompt.md` — the previous prompt. Its §3 hard rules and §4 lane recipes **still apply verbatim**; this file only adds to them.
3. `llm-wiki/index.md` → "Live app (§12.11)", "GUI (§12.10)", "Lane infrastructure".
4. `docs/superpowers/specs/2026-09-26-live-app-design.md` (the G9.0 design); spec `2026-09-20-reincarnation-spec.md` §4 (realtime, LOCKED), §5, §8, §17.
5. The code, as it stands:
   - `audio_io/audio_ahi_live.{h,c}`;
   - `app/riapp.c`;
   - `engine/live.{h,c}`;
   - `engine/seq/ctlplane.{h,c}`;
   - `gui/ctlreg.{h,c}` (`ri_ctlreg_auto_id`).

## 2. How the live path works now (do not rebuild it)

- **One render Process** ("RIAPP render", priority 10) owns every AHI object.
  - It opens `ahi.device` on `AHI_NO_UNIT`; the library base is `io_Device` (there is no `LIBS:ahi.library`).
  - It picks `AHI_BestAudioID` for stereo HiFi at 48 kHz; the Dell gives mode `0x003E0001` and **accepts a 48000 Hz mix rate**, and the session runs at the queried `AHIC_MixFreq_Query` rate.
  - It allocates one channel with two `AHIST_DYNAMICSAMPLE` `AHIST_S16S` sounds as the double buffer.
- **The `SoundFunc` hook only counts and `Signal()`s.** On each start of a half, the task renders the other half through `ri_live_render` and queues it with `AHI_SetSound(…, AHISF_NONE)`. A late task makes AHI loop the last half; that is counted as an xrun, never a hang.
- **GUI → task:** a transport request word (`au_live_request`) and the SPSC control plane (`ri_ctl_send` with lane keys) only. The task is the only caller of `ri_live_render`, `ri_live_play` and `ri_live_stop`.
- **Small buffers:** below 1024 frames the task adds `AHIA_PlayerFunc` + `AHIA_PlayerFreq` = rate/frames (Fixed 16.16). Without it, AHI mixes about 50 passes/s (~960 frames) and shorter halves loop within one pass. The no-input runs gave 256 → 0 xruns, 128 → 0 and 64 → 1, **but nobody has listened at those sizes yet**. At 1024 the owner heard the PlayerFreq build as "worse", so it is off from 1024 up.
- **Render cost:** EClock timing in the task; at 1024 frames, max 2.86 ms and mean ~1.8 ms per 21.33 ms period (~8 %).
- **W capture:** the task copies each half into a GUI-allocated buffer while `cap_on`; the GUI writes `RAM:RIAPP.wav` (no file IO in the render task). Q while recording also saves.
- **Logging:** `RAM:RIAPP.LOG`, opened, appended and closed per line (a `Run >file` redirect stays empty). `RawDoFmt`/`VFPrintf` args are **packed 32-bit LONGs**, never IPTRs.
- **Demo mix:** the 303 strip starts at 72 (−9.9 dB) and the 808 downbeats are accented, because the owner wanted the 808 louder. The voice calibration is unchanged; the 808 section RMS sits ~10 dB under the 303 at equal settings.
- **Dell build:** the ABIv11 recipe from the G9 prompt §4, with `-O2 -fno-builtin -DPCF_TABLE_VERIFIED=1` and the object list of the `riapp` target in `scripts/ri_build_aros.sh`. It links with `-lamiga -lintuition -lgraphics -lutility -ldos -lexec -lautoinit`, 0 unresolved. The ABIv1 gate: `bash scripts/ri_build_aros.sh riapp`.

## 3. Lane lessons from this session (new — read before touching the Dell)

- **Stuck keys:** agent `--ui-rawkey CODE,up` does **not** release the key; it stays down and auto-repeats. Always release with **`CODE|0x80`** (e.g. `--ui-rawkey 0x10 --ui-rawkey 0x90` for Q). A stuck Q made RIAPP quit on every window activation and cost two false "click bugs". If a window "disappears on click", log IDCMP classes first (`class=0x00200000 code=0x71` was the tell), then inject releases for 0x10/0x40/0x37/0x60.
- `--ui-close` does not match RIAPP's long title; quit it with a click at (100,60) plus the Q press/release.
- **Order of operations:** `--ui-click l,100,60` activates the window; keys go to the active window. Leave 1 s between a click and a key.
- **Evidence:**
  - the owner listens (you cannot);
  - automate what you can (buffers, xruns, render time from the log);
  - ask the owner for the audible verdict with **one precise question per run**.
- **Only the owner reboots the Dell.** A Software Failure requester only suspends the task; use RAM-log probes to find crashes.

## 4. Steps

### Step 0 — Land what exists (small)
1. `git log origin/main..main` must show exactly `dd93ae7` and `e884439` (plus anything of yours).
2. Run the full audit (`AUDIT 0/0 PASS`) in a clean worktree, then `git pull --rebase --autostash` and `git push`.
3. Add an llm-wiki raw article `2026-09-2x-live-ahi-first-sound.md`: the facts in §2/§3, the evidence file's numbers, the owner's words. Add the index entry under "Live app (§12.11)" and a log entry at the top.
4. Cross-post the AROS-general lessons to the Vulkan4AROS wiki:
   - `AHIST_DYNAMICSAMPLE` double-buffer streaming;
   - PlayerFreq vs mixing-pass length;
   - the `ahi.device` base via `AHI_NO_UNIT`;
   - `RawDoFmt` packed LONG args;
   - agent rawkey release = `CODE|0x80`.

   Use a worktree of `origin/main`, `git push origin HEAD:main`, remove the worktree afterwards.

**DoD:** pushed, audit green, both wikis updated.

### Step 1 — Buffer size, by ear and by numbers (Dell)
Goal: pick the shipped default buffer (spec P-21 / OPEN-09) from **owner listening + WAV + logs**.
- Record the same scripted take with W at 1024 (no PlayerFreq), then 512/256/128/64 (PlayerFreq). Ask the owner to press Space, W, three C presses, Q, and nothing else.
- Fetch each `RAM:RIAPP.wav` and compare them on the host:
  - peak, RMS and a spectral-difference sum against an **offline render of the same control history**, using `ri_live_render` on the host at 48 kHz with the same moves at the same buffer indices — live == offline is bit-exact by t81;
  - any sample-level difference is a device-path defect (dropped or looped halves), so count repeated half-blocks.
- Explain "the PlayerFreq build sounds worse at 1024". Hypotheses to test:
  - pass length equal to the half length makes the queue race the pass start;
  - AHI resampling or clipping at the pass boundary.

  Keep whichever path gives bit-exact WAVs.
- Then a **5-minute soak** at the chosen size: xruns, render max/mean.
- Update the evidence file, the ledger row for P-21 (render priority) and the default in `RIAPP_DEV_FRAMES`.

**DoD:** default chosen with evidence; the owner confirms by ear; soak logged.

### Step 2 — The real panel in RIAPP
Goal: RIAPP shows the ReBirth panel (the RISECT canvases), not a text window.
- **Layout:** reuse `app/sectproof.c`'s `live`/`keys` layouts (RSection canvases, panelui focus/keys, skins, zoom). Start with transport + pattern sections + 303A + the 808 mixer strip; the whole rack where the screen allows (1366x768 on the Dell; 2x zoom does not fit — the owner decision is still open, so default to 1x).
- **One path to the engine:**
  - every value change on a canvas calls `ri_ctl_send(ctl, ri_ctlreg_auto_id(def), value)` when the key is nonzero;
  - pattern edits go through the pure edit functions on the GUI-side banks, then a snapshot request;
  - transport keys use `au_live_request`.
- **Meters and position:**
  - the render task publishes a small `RILiveMeters` copy at buffer end (double-buffered or a sequence counter);
  - `livestate` reads only that copy — **replace RIAPP's current racy direct read**;
  - this closes G6b on the device.
- **Tests:** a host test (next free t-number) proving a canvas value change produces exactly one control-plane message with the right key and value, for a sample of controls from every section. AROS: an owner check on the Dell, turning three knobs and hearing them.

**DoD:** the panel drives sound on the Dell; meters move with the music; audit green.

### Step 3 — 909 sounds
- RIAPP says "909 pack: unbound - 909 renders silence". Find the 909 sample-pack loader (RBNM, `project/rbnm.c`, `ri_engine_909_bind`) and ship a **clean-room** default pack or a documented install path. No Roland/ReBirth samples (spec §1). If no legal pack exists, say so and stop.
- Add a 909 part to the demo song, and have the owner listen for balance.

**DoD:** 909 audible with owner confirmation, or a documented blocker.

### Step 4 — Automation from the panel (G9.5 AROS half)
- In RECORD state, panel moves also go to `ri_live_record_touch` (host half exists, t82), then publish, then playback moves the on-screen controls (read-only chase from the published lane at the playhead).
- "Lane full" is shown.
- Save and load via `rbng` ATRK: a `S`/`O` key or menu writing `RAM:RIAPP.rbng`.
- Owner proof: record a cutoff sweep, stop, play it back, save, reload, play again. W-record each pass; the three WAVs must be **bit-identical** in the automated region.

**DoD:** as stated, audit green, evidence file updated.

### Step 5 — Recording UX (small)
- **W:** "start" writes a numbered file (`RAM:RIAPP-001.wav`, …) so takes never overwrite each other.
- Show "REC" in the window title.
- Optional: 24-bit via `auf_f32_to_s24` (render at f32, convert on the GUI side).

### Step 6 — Carry-overs (from the G9 prompt §6, still open)
- Per-module skin choice (the song chunk needs owner review).
- Proof-app zoom for the grouped views.
- The G8 deferred minors.
- The 909 knobs with no engine param (28 unrecordable).
- camd patch landing (`docs/evidence/gui/0023-camd-mysprintf-varargs.diff`) + the debugdriver hang.
- Engine 808/303 calibration: the 808 section sits ~10 dB RMS under the 303 at equal settings. Compare against the manual and hardware references before touching the voices; goldens must be regenerated deliberately, never silently.

## 5. Owner decisions still open (never decide; list them in each report)

- Dist exclusivity.
- G5/G7 E0 items.
- 909 tap level.
- GR full scale.
- Meter floor.
- OPEN-10 (ReBirth file import).
- 2x zoom policy on small screens.
- The per-section skin song chunk.
- **New:** the shipped buffer default (Step 1 recommends; the owner confirms).

## 6. Reporting and handoff (every step)

- **Report:**
  - results first: what the owner heard and the numbers;
  - commits;
  - which lane and which measurements;
  - rulings with cost-if-wrong;
  - blocked items;
  - the §5 list.
- **Honesty rules:**
  - never write "structure", "wired" or "done" for code that is not reached at run time — the previous G9.3 "backend structure" rendered one buffer and never opened AHI;
  - say "placeholder" when it is one.
- **Context:** if context runs low, finish or revert the current step and commit. Then append a line to `docs/2026-09-24-improvement-todo.md` §12.11: `NEXT: Step N — <state in one sentence>, last commit <hash>`.
