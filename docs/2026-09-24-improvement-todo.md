# Improvement-doc execution tracker

Source: `docs/2026-09-24-improvement-opportunities.md` (§12 order, Terry rule: one thing at a time).
Rules: TDD RED-first per change; `ri_audit.sh` 0/0 before each commit; feat:/docs: pairs on `main`; wiki record per slice; scratch in `/home/miller/Work/ri_build/`.

## Adoption gate (normative first)
- [x] Adopt §11 decisions D-a..D-j into spec `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (doc is non-normative until adopted)

## §12.1 Kernel totality + 808 deactivation + clip removal (§2.1, §2.3, §2.4)
- [x] `ri_exp` total over finite floats (0 for x ≤ −87, reduction |k|≤128, +Inf above 88.72283) + property test full-range (t3)
- [x] `ri_sin` domain safety (int64 reduction to |x|<5.7e19, bounded 0 beyond) + `ri_pow2` total (±127/128)
- [x] In-domain paths bit-identical (t2 green + 303 first-light re-render cmp IDENTICAL)
- [x] Long-silence regression test (t33; tau-scaled 3 s ≡ 30 s in t/tau coverage, stated in-test)
- [x] 808 voice deactivation at −100 dBFS (t35; quiet-at-death + exact-0-after)
- [x] 808 section-sum linear (t34 storm-vs-solo-sums; 909 clip deferred to §12.6)
- [x] Deliberate 808 golden re-baseline (bd/lt/mt/ht/oh/storm, each root-caused) + ledger notes
- HELD: per-sample multiplicative envelopes SKIPPED — deactivation bounds t < ~50 s always
  (max tau 4.0 → death at 46 s; float t exact there); absolute-time total exp is
  overflow-proof; cost goes to the §12.11 bench pass with numbers, not guesses.
- HELD: `ri_scale2` keeps the 32-iteration loop for |k|≤32 (wide bound totality-only) —
  inflating it cost 2.4× storm CPU (t1_808/t21/t23 budgets RED); caught by suite, fixed.

## §12.2 Control registry + 303 ID fix + 303B dispatch + Tune (§2.2)
- [x] Fix 0x0305 volume-vs-wave (panel row renamed waveform@0x0305 def 0; volume moves to 0x0306 def 100; guide corrected, audit strings kept)
- [x] Route 0x031x via shared dispatch (set_param normalizes block; twin-voice behavior test t36)
- [x] Add Tune (0x0307/0x0317, ±24 st, live ratio bend, pitch-exact) + Waveform to panel (both sections)
- [x] Panel/engine contract test-enforced (all 16 IDs reachable from panels; rows whitelisted to handled map; defaults pinned)
- [x] 303 goldens byte-identical (tune 0 = ×1.0 exact); render-path 303B voice scoped to §12.3 (comment in render.c, no mistarget)
- HELD: full single-source codegen (generate panels/guide/greps from one table) —
  engine IDs canonical in rb303.h, panel uses BASE+offset, contract pinned by t36;
  codegen when a third consumer appears (YAGNI).

## §12.3 Integrated stereo engine skeleton (§5.1, §5.3)
- [x] One `ri_engine_render` (stereo) + `ri_engine_render_mono` shared by CLI/export/AHI sinks
- [x] Event routing unified (3 per-path copies retired; device/block routing, unknown ignored)
- [x] render_song + render_rbngsong + au_render_frames rewired; first-light goldens byte-identical
- [x] Audit I1 tripwire retired → wired-core gate (engine call required in audio.c)
- [x] t6 file-vs-live IDENTICAL through the shared core (stronger than before)
- NOTE: audit Phase 0a greps `realloc` — the word "preallocated" in a comment
  failed the gate; reworded ("storage lives here"). Comment diction is load-bearing.

## §12.4 303 fidelity pass (§4.1)
- [x] Gate-length rule (D-h E0: half-step fall, ties hold) + t38 + ledger + re-baselines (m48)
- [x] MEG/VEG split + accent-sweep state + accent→min-MEG-decay (E1 lineage; t39; revoiced goldens) (m49)
- [x] Log-domain slide (τ untouched — OPEN-01; ri_log2 kernel; t40; glide re-baselines) (m50)
- HELD for measurement per review: band-limited osc, 4-pole ladder option, reso taper,
  cutoff/envmod anchors (no ears/captures available; §8 plan stands)
- [ ] Per-parameter smoothers at control rate

## §12.5 808 rebuild (§4.2)
- [x] 11 slots + 5 switches (last-wins) + MA + per-sound Level + accent-level law + LEVEL/ACCENT knob wiring (m51)
- [x] Metal fixed oscillators + choke rules + per-sound Tune/Decay/Snappy/Tone wiring (m52)
- [x] BD 808 regime (62 Hz / 4 ms sigh, paper-read) + SD structural verification + recursion held (m53)

## §12.6 909 completion (§4.3)
- [x] LT/MT/HT/RS/CP as sample-layer voices + per-voice Decay/Level (m54)
- [x] Flam bit/width decouple (compat kept) + shared CH/OH level + OH-wins rule + 909 clip removal (m55)

## §12.7 Scheduler + RBNG v2 (§5.2, §7.1) — DEFERRED after §12.8 (FX first:
  self-contained, no format surgery; rack iteration builds on the §12.3
  section mask meanwhile)
- [x] Pattern banks/lengths (per-instance model + Edit ops + emit) + RBNG chunk plan (v1.1 BANK) (m63)
- [x] 4 sections (engine 808/909 hosting: sets, event consumption, AC stamp, 909 bind) (m64)
- [ ] Shuffle flags (OPEN); [x] streaming emission (m67 §12.9c); [x] song track (m66)

## §12.8 FX routing + parity + PCF envelope (§3.2, §4.4)
- [x] Delay beats-honoring + caller-owned lines + tap slew + live-rate comp + pool retired (m56)
- [x] Delay Steps/triplet/fb-infinite/sustain-structure (m57)
- [x] PCF envelope + integer clock + Decay knob + HP drop, patterns open (m58)
- [x] Delay pan + stereo return + routing matrix/exclusivity (m62)
- [x] Appendix-D patterns (55 E1 rows + wrap + resolution wired) (m59)
- [x] Dist 2x oversample + comp ratio/GR meter (m61)

## §12.9 Song mode + transport + pattern edits (§3.1)
- [x] Transport state machine (states, bar mapping, loop model, record/display) (m65)
- [x] Song track (dense grid, downbeat capture, change emission, edits, STRK) (m66)
- [x] Automation lanes core (§12.9c — Tasks 2–4 + 5a FX + 5b drum per-voice keys + 5c strip keys/level 2026-09-26; 98 controls recordable); [x] record-path capture gating (m68); [x] streaming player/changeover (m67)

## §12.10 GUI parity + skins + MIDI (§9, §3.1)
- [ ] Panel inventory; focus bar; meters; skins via MCC classes; knob modifiers; transport/playhead from audio clock (§2.9 last row); MIDI maps from manual
- G8 CLOSED 2026-09-26 code-side (commits [§12.10 G8] c14486e/494b49c/9b537c8/0488f23/bb19ae7/2503557/a9528ad): skins, zoom, audit wiring, acceptance machine pass, Stop law, engine taps, camd patch preservation. REMAIN: G6b audio binding (no lane), ReIncarnation-101 human/audio pass (1/5 full + 4/5 partial), skins format review (CLOSED 2026-09-26: format 1 approved — header, named keys, role parts, load-time size check), per-module skin choice (owner req 2026-09-26, design in skins-design.md), upstream camd PR, debugdriver hang, runtime registry (rack design). Wiki ingest rides in the G8.4 commit.
- G8.1 skins DONE 2026-09-26 (commit [§12.10 G8]): design
  `docs/superpowers/specs/2026-09-26-skins-design.md`;
  **format (E0-6/E0-7) + dir layout AWAITING OWNER REVIEW — non-reversible**;
  core `gui/skin.c` + t75 (9 killing mutants), AROS loader `gui/skin_aros.c`,
  RSection bg/knob hooks, `RISECT mod=` + Ctrl+M cycling, `tools/mkskin.c` →
  `skins/808-RI/` (17 bg + 7 strips, PNG/RGBA), `skins/Template/` (blank,
  fallback-proven); lane proof `docs/evidence/gui/skins-g81.md` (Classic vs
  808-RI vs Template vs missing + live cycling, audit 0/0)
- G8.2 zoom DONE 2026-09-26 (commit [§12.10 G8]): `RISECT zoom=0..3`,
  `ri_skin_zoom_num()` + t76 (factor table vs panelgeo, sizes/pixels per
  zoom, P-18 drag-law zoom-independence), loader scales by the helper;
  lane matrix `docs/evidence/gui/zoom-g82.md` (303/808 x4, mix/fx/tr,
  808-RI x4 — crisp everywhere; compact legend crowding noted for owner;
  true drags need a human hand)
- C1 audit wiring DONE 2026-09-26 (commit [§12.10 G8]): Phase 12 runs
  t60-t73 + t75-t76 (t74 stays in its engine phase), RICtlDef/RI_SEC_NAMES
  single-source grep gates, AROS-only guard list + `ri_build_aros.sh
  sections` link gate; each gate mutant-proven (t76 factor in full audit,
  stray table file, broken TU)
- G8.3 acceptance DONE 2026-09-26 (commit [§12.10 G8]): `acceptance.md`
  rewritten for the section canvases (every tick → evidence, every gap →
  reason); ReIncarnation-101 machine-driven on riqemu1 (1 full + 4 partial:
  CC25/CC17 sweeps, 16-step 303 in Step mode, 909 low taps, pattern
  switch while playing; hearing/drags/pitch-mode/accent-flam/mute stay
  human; Solo is out of E1); dummy-panel row recorded (5 data
  touch-points, zero logic; runtime registry → rack design)
- C4 Stop law DONE 2026-09-26 (commit [§12.10 G8]): E1 p. 145 adopted —
  first stopped Stop → Loop Start (before the locator → song start);
  arm-only press retired (t58 repinned + edge mutant-proven, t69
  observables unchanged, geometry note closed)
- C2 live binding PARTIAL 2026-09-26 (commit [§12.10 G8]): engine
  per-section + per-FX peak taps + accessors (t78, mutant-proven, audio
  untouched); APP BINDING BLOCKED on an audio lane (Dell unavailable;
  riqemu1 has no sound device; QEMU AC97/HDA + AROS AHI driver surgery
  not attempted) — stand-in stays, G6b open
- C3 camd PARTIAL 2026-09-26 (commit [§12.10 G8]): series-format patch
  `0023-camd-mysprintf-varargs.diff` preserved + placement note (verify
  clean-apply; tree matches evidence); series commit BLOCKED on the
  t8-workgroup-size owner; upstream PR BLOCKED (no prs.py tool);
  debugdriver hang root cause open (needs lane debug + v1 rebuild)

## §12.11 Perf pass vs W1.1 budget (§6)
- [ ] `tools/bench` per-section evidence; per-section budget; continuous gating before beta exit
- [x] Drum-section tails (owner work 2026-10-07: polyphony, not a defect)
  - **Verdict:** the 909/808 "tails" are polyphony — Dell `(µs, active)`
    fits give r = 0.979/0.981 (909) and 0.999/0.995 (808) across Zombie
    Nation + The Knife; block cost tracks active voice-samples. The 808's
    max/avg (4.0×) exceeds the 909's (2.8×) at r = 0.999: same mechanism.
  - Crash/ride cost ~2.1× per voice-sample by design (audible Tune-shortened
    decay envelope). The bit-exact tau-hoist recovered its pow2 half on the
    host (−27% crash solos) but nothing attributable on target (909 avg
    31→31, slope 0.3018→0.3010), so it was reverted per §0 (no fix without a
    measured prize). Any further saving there changes the sound (owner §9.3).
  - Instruments kept: per-block drum counters (`t175`), `drum_bench`,
    Dell ring + dump. Evidence: `docs/evidence/drum-tail/2026-10-07-a0-measure.md`,
    `docs/evidence/drum-tail/2026-10-07-a1-tau-hoist.md`. Commits
    `bbb9f8f`/`a7bcfe3`/`6536353` + revert. No engine change shipped.
- [x] Tab-switch page cost (owner work 2026-10-07: MUI's and the blit's, stop)
  - **Verdict:** the 20–42 ms switch is MUI's page handling + the window
    blit, not our re-render. TAB probe (`fn/fb/fr/fl/mui`, `t176` law):
    `fb + fr` ≤ 19.9 % of `page_us` and ≤ 4.8 ms on every tab — both stop
    conditions hold, so no B1 replay skip (would save ≤ 4.8 ms for
    stale-pixel risk). `mui` never negative; xruns+0 on every switch.
  - Instruments kept: full-draw split in `draw_frame`, TAB split line.
    Evidence: `docs/evidence/gui/tab-switch/2026-10-07-b0-split.md`.
    Commit `2522c79`. No GUI change shipped beyond the probe.
  - **Review addendum (advisor 2026-10-07):** the remaining cost is mostly **screen write bandwidth**. On the Dell, writes to the screen run at ~220 MB/s against 4332 MB/s RAM to RAM (probe in the B0 evidence addendum), so a 1024x512 area costs ~9.5 ms however it is drawn. The likely cause is that the VESA framebuffer is not mapped write-combining (`rom/hidds/vesagfx`, `UpdateRect` → `CopyMem` to a `MapPCI` mapping); the Dell's MTRR/PAT state has not been read yet.
- [ ] Dell screen write bandwidth (from the tab-switch review 2026-10-07): (1) DONE 2026-10-07 — MEMTYPE on the Dell (two identical runs): FB `0xD0000000` is **UC** (MTRR default UC, PAT[0]=WB), so uncached is proven, not inferred; evidence `docs/evidence/gui/framebuffer/2026-10-07-f1-memtype.md`, commits `f31d2f3`/`97dbd04`/`4c806c9`/`486e314`; (2) DONE 2026-10-07 — sent upstream on owner instruction as AROS issue #1508 (proposal only, no code); F3 WC trial NOT DONE (stop rule: Dell runs 4 CPUs, no user-mode IPI); re-measure (probe + TAB split) only if a fix lands; (3) app side: avoid MUI backfill under canvases on page switch, so canvas pixels cross the slow path once (measure first).
- [x] Load-governor + repaint fix (owner Dell 2026-10-01, xruns while playing and switching tabs)
  - **Diagnosis** (both `RAM:RIPP9` logs, build `cdcf85c`, 19 min run, pulled after the owner quit the app): the pri-21 render task fix WORKS — all 7 tab switches logged `TAB … xruns+0` (six of them while playing, one while stopped); 13 min of playing with no tab switch at 0 xruns. xruns appear only in two windows, both "playing + tabs": buf 55227-59586 (+485, `overloads` 0→9) and buf 209690-216272 (+923, `overloads` 9→22). The last window is 923 xruns against 630 full repaints = 1.47 xruns per repaint, and repaints are only 5-12 % of a core, so this is not CPU exhaustion: while the governor holds the render task at pri -1 (below the UI), every repaint pre-empts it for a buffer or two. Two causes, both answered:
  - **Trip too eager.** Playing load hovers 585-1083 per mille around `RI_LIVEDRV_OVER_PM` 850, so the 1/8 EMA crossed the threshold constantly and re-tripped every ~2.3 s (13 entries in 19 min). The trip law now needs `RI_LIVEDRV_ARM_US` (2 s) of **continuous** over-budget load: the arm accumulates the buffer period while `load_pm >= RI_LIVEDRV_OVER_PM`, resets to 0 the moment a buffer is under it, is cleared at the probe, and is zeroed at the trip so each entry needs its own arm. Threshold 850, cap 1200, the 2 s hold and the pri -1 floor are unchanged, so a machine that genuinely cannot keep up still yields. (Owner deferred changing the floor itself: "Leave it, diagnose first.")
  - **Repaint storm.** `meter_round` (app/riapp.c) full-refreshed all 18 non-chase, non-MASTER canvases every 100 ms while playing. Only two live reads exist in all the art: `panel->playhead` through `ri_art_chase` (the 808/909 step lamps, already repainted by box) and `panel->focus` in the click path; `panel->playing` is read by no art at all. So `ri_panel_live` now returns **why** it changed (`RI_PANEL_CH_PLAYHEAD`/`PLAYING`/`FOLLOW`/`TAP`) and the pure `ri_panel_live_stale(mask, section)` maps that to nothing / step boxes / bar box / full. `meter_round` dispatches on it: drum rows repaint the old and new lamp boxes as before, the transport repaints the Song Position box when the song-mode display follows, and the blanket refresh survives only for a held delete-tap (which edits pattern data — the one change that can appear anywhere).
  - **Proof:** audit `AUDIT 0/0 PASS`; ASan/UBSan clean on t71/t72/t88 + t60/t61/t69; mutation 11/11 in `mut_fixA.txt` and 14/14 in `mut_fixB.txt` (t88/t71). Laws live in `tests/unit/t88_live_driver.c` (peaks never trip, a light buffer resets the arm, the re-trip needs a fresh 2 s) and `tests/unit/t71_panelui.c` (each reason bit alone, the policy over all 21 sections, the tap set pinned against the registry's step-key sections). **On-target proof still open** — the fix needs a redeploy under a new RAM name and an owner play-and-switch run compared against this baseline; the reported symptom is therefore not yet closed.

## G9 live app (G9 plan §12.11; opencode 2026-09-26, commits 48b552d…ee05365, audit 0/0)
- [x] G9.0 design note (`docs/superpowers/specs/2026-09-26-live-app-design.md`)
- [x] G9.1 control plane SPSC ring + t80 (FIFO/coalesce/wrap/refused/render-identical, mutants)
- [x] G9.2 live session core + t81 (chunk-agnostic bit-exact 64/128/256/137, automation, ctl@256, meters, mutants)
- [x] G9.3 AHI render-task structure + `audio_ahi_live` (ABIv1 compile; Dell 64/128 measurement open)
- [x] G9.4 RIAPP shell (ABIv1 link 172792 B; supersedes bare main.c, kept; panel wiring after Dell proof)
- [x] G9.5 record host half + t82 (touch/record/stop/publish/chase identical, FULL shown; mutant)
- [x] Dell first sound + knob-within-a-buffer + owner words + 5-min soak (`docs/evidence/audio/live-render-task.md`; G9b Step 1: 256 default owner-approved, P-21 measured)
- [x] Full panel integration into RIAPP (G9b Step 2, Dell-proofed: knobs sound, meters chase; `docs/evidence/gui/riapp-panel.md`)
- [ ] G10 carry-overs (skin chunk, zoom cap, minors)
- [ ] RIAPP full panels (owner req 2026-09-27): wire up every RISECT panel
  (303B, 909 incl. step editing, full mixers, FX, full transport) so all
  bound voices are reachable live; 909 demo content for the new voices
  rides with it (taste: owner ear).
  - Layout (owner 2026-09-27): tabbed groups via stock `Register.mui`
    (no custom tab widget); groups ~Synths/Drums/Mix/FX.
  - Device visibility (owner 2026-09-27, aligns with the extensible-rack
    requirement): GUI must let the user select which synths are
    visible/active (Korg, ASM Leviasynth later) — design the tab model
    for it now, don't hardcode four devices.
  - 303B (owner 2026-09-27): silent until programmed (no demo content).
  - Slice landed 2026-09-27 (t97/t98 + riapp.c, unproven on device):
    Register tabs, all voice canvases, 303B + 808/909 step sync, full
    value/FX paths, engine +S303B|S909. Remaining: device proof, the
    visibility-selection UI, 909 demo content (owner taste).
  - Devices tab landed 2026-09-27 (t98 +1 tab, unproven): per-device
    toggle buttons (display-only ShowMe; engine/mixer untouched),
    VIS ev-log lines. Device proof open.
  - Devices tab APPROVED by owner ear/eye 2026-09-27 (works as intended).
    Owner design challenge: 5th tab not the most elegant home (manages
    VISIBLE, requirement says ACTIVE). Decision 2026-09-27: rail first —
    toggles move to a slim always-visible rail above the Register (same
    visibility-only bit); activation (engine enable/disable) later.
  - Rail APPROVED 2026-09-27 (owner: works as intended). Proof CLOSED:
    ev-log VIS dev=1 show=0/1 (hide/show 303B, twice) with TR PLAY/STOP
    around the toggles; audio unaffected. Activation (engine
    enable/disable) remains the explicit next step.
  - Activation landed 2026-09-27 (t100, unproven): rail toggle now means
    ACTIVE (engine sections bit + row together, one atomic read per
    block); disabled = voice never triggers, state kept. Device proof
    open (Dell agent redial pending).
  - Activation proof CLOSED 2026-09-27 (owner, build 274c297): full
    mask walk 0f→0e→0c→04→00→02→03→07→0f→07 under PLAY (synths-only
    drums-only silence full round-trips, voices rejoin mid-phrase);
    TR STOP clean. Follow-up closed per owner: technique, not a bug
    (303 programming learning curve); no voice-path chase.
  - 303B audibility (owner 2026-09-27): two real bugs, fix unproven.
    (1) Note/Pause button unlabeled on 303A/B (geo LEGEND row missing;
    shared SYNTH1 table so one row fixes both) + 909 Flam button same
    shape; t61 legend invariant, t92/t93 pins re-pinned (same 12).
    (2) 303B knobs drove 303A params (shared table + raw last_hit);
    sync re-tags to the canvas section. Silence itself was REST-by-
    default (fresh 303 steps init Pause; keys set pitch only) — by
    design. Device proof open (labeled toggle + 01xx CTLs + sung 303B).
  - GUI feedback round 1 (owner 2026-09-27, unproven): Note/Pause legend
    between its LEDs, wide switch rect under the LED pair, BACK
    between its LEDs with the rect widened beneath (BACK reverted to
    its row); rail chips are labeled button + radio-dot LED (Pressed
    notify, Selected mirror). Divider-line attempt dropped (paints under
    the button face). t92/t93 re-pinned (8 synth pins, clean-build
    ground truth after a stale-object mis-pin).
  - GUI feedback round 2 (owner 2026-09-27, unproven): rail LEDs are red
    record-dots (spec swap, no layout shift); Note/Pause rect narrowed
    clear of its LEDs (w132); pins from clean-build ground truth.
  - Demo drive (owner 2026-09-27, ear verdict open): Zombie Nation
    drive — E-minor riff (accented quarters + rest), four-floor kept,
    tempo 140 (researched). Original, not transcribed.
  - 909 body v2 (owner 2026-09-27: kit thin/string-like): membrane modes
    + transient + decay in mk909 (same seeds); showcase hits accented;
    pack rebuilt with bit-exact carry-over (OLD-EXACT=18). APPROVED
    2026-09-27 (owner ear).
  - Spike hunt DEFERRED 2026-09-28 (owner): USB falsified by matched
    A/B (both 2 xruns, ~3.6 ms max). Rules: debug-only RIAPP
    (audit-gated), USB fresh-per-run ev-log. Resume on 3x spikes.
  - GUI feedback round 3 CLOSED 2026-09-27 (owner eyes + status log):
    rail LEDs are self-drawn green discs (C_MIX_GREEN, pixel-verified)
    after fixing NULL IntuitionBase, NULL-friend AllocBitMap, AreaEllipse
    area-state guru, and imageless-keeps-zero-size. Lesson recorded:
    canvas path does LEDs perfectly; rail duplication was the cost.
  - Rail LED boxes CLOSED 2026-09-27 (owner eyes): dots render clean.
    Saga article stands as the failure record.
  - GUI feedback round 3 (owner 2026-09-27, unproven): stock dots render
    black on this theme. Rail LEDs are self-drawn 18px bitmaps in exact
    canvas greens (on 0x38E040 = the Pattern-block LED, pixel-verified;
    off 0x1E4A22), Bitmap.mui + Transparent pen 0, built post-open from
    the screen colormap, swapped on toggle, freed at exit.
  - GUI feedback round 2 verdict (owner 2026-09-27): everything approved
    EXCEPT rail LED color (TapeRecord dot not bright red). Round 3: green
    on-state (TapePlay dot), dark off-state unchanged.


## Planned devices (extensible rack, spec D-k)
- [ ] Sampler/slicer — owner decision 2026-10-07: replaces the ESX-1 as the next rack device (sample parts, one slicing part, per-part filter + envelope); requirement, rationale and open questions: `docs/superpowers/specs/2026-10-07-device-sampler-slicer-requirement.md`. Not designed; no code.
- [ ] Korg Electribe ESX-1 — DEFERRED 2026-10-07 (owner: "we can always add the Korg later"); was owner plan 2026-09-26; requirement + rules for current work: `docs/superpowers/specs/2026-09-26-device-korg-esx1-requirement.md` (no 4-device assumptions; new lane-key block, strip, skin token, RBNG chunks — chunk IDs owner review; E1 = Korg ESX-1 manual; clean-room: no Korg samples/ROM/art).
- [ ] ASM Leviasynth — owner plan 2026-09-28; requirement + E1 + rules: `docs/superpowers/specs/2026-09-28-device-asm-leviasynth-requirement.md` (16v×8op algorithmic hybrid; manual v1.2.1 + SOS review + KVR specs as E1, PDFs kept out of repo; `0x0E` lane block; `levi` token; clean-room: own waves/topologies, no ASM content). v1: POLYPHONIC voice (owner decision) — slices landed through 3c-iii (3c-ii: sectlevi t107 chord editor, voice canvas geo/art, Levi panel slot 4, PAT_LEVI dispatch; 3c-iii: visdev/tabpages 5-wide, t100 Levi gate 0x1F+mutant, rail/voice/PAT canvases + LSTEP sync + 0x10 startup mask in RIAPP); demo part landed (owner 2026-09-28: Bm-G-D-A quarters, t95 + mutant); Dell staging 2026-09-28 ~11:20 UTC: 657a990 deployed to RAM:RIAPP (Search-verified), old instance broke clean (5.6M buffers/30 xruns), new session 0 xruns render_max ~75us, 909 pack 11 voices bound; note RIAPP_EVLOG=ram: persists in Dell ENVARC: (spike-hunt leftover) so ev-log lands in RAM: (locked while running); sibling RIAPPPWR proc untouched; owner verdicts 2026-09-28: NaN fix APPROVED (sustained play, Levi part audible), sound APPROVED for now, rail LED works as expected; Dell USB stick (Vk4aros:) error 42 then `no disk`, re-seated: BACK after reboot (session 6); rail BUG (5th chip dead caps) fixed in c69de53, RETESTED + strip proof APPROVED on device 2026-09-28 — ev-log ink (Vk4aros:, override unset, stick back): VIS dev=4 toggled twice (mask 1f<->0f, 1d<->0d), LSTEP chord edits + pitch (step0/lane1 note 67), MIX_LEVI CTL 1407/1400/1402 (insert/mute/level drag), PLAY..STOP 864k buffers with ZERO new xruns (15 pre-existing from stick event); ABIv1 lane proof 2026-09-28 (riaudio guest): f9d8ee9 played ~4 min, 43,598 buffers, 0 xruns, 115 us max — 8-op + 7-mode DSP healthy cross-platform (guest wav captured silence post-morning: capture stopped, lane-owner note, not a code signal); 40d430c device proof ink 2026-09-28 (Vk4aros ev-log, stick back): rail chip twice, chord edits + pitch, MIX_LEVI strip, PLAY..STOP 864k buffers; xrun forensics: all 12 xruns in the first ~4k buffers after play (71 us -> 35 ms spike, then 210k buffers clean INCLUDING fader-drag twiddling) — cold-start transient signature, input floods largely exonerated, hunt stays deferred; owner scope 2026-09-28: panel matches real hardware as close as possible with a different font (own art, clean-room); features ALL in order — algorithms+op modes, filters+envelopes, arp+sequencer, mod matrix, FX+performance; v2 slice (1) STARTED: algorithms E1; v2 slice (2a) LANDED 2026-09-28 (12469f8: multimode SVF taps + drive + 24dB cascade + Type/Drive controls; cascade darkens default voice — owner re-verdict needed); ABIv1 proof (riaudio guest): 28,522 buffers play, 0 xruns, 1032 us max, clean close; demo APPROVED on device 2026-09-28 (40d430c: DUO-identical output confirmed by ear); v2 slice (2b) LANDED 2026-09-28 (5967131: stage-2 tuning + voice ADSR/loop + Analog/Env bottom strip in hardware block order, taller canvas; UI-mapped defaults reproduce v1 constants); ABIv1 proof (riaudio guest): 5967131 23k buffers play, 0 xruns, 89 us max, clean close; 2026-09-29: look rework APPROVED on Dell (fba990e: knob row, KEYBOARD section, voice-only tab), v2 arp (3a/3b: stepper+rewrite+binding+player hook, t113-t115) + SEQ (3c: phrase looper+hook/binding, t117-t118) + matrix (4a slot core, 4b render hook, 4c-A route gates, t119-t120) + LFO core (4d, t122) + reverb core (5a, t123) LANDED with mutants + audit (t75 bg03 sibling-side open); 6fba227 deployed Dell 2026-09-29, regression APPROVED (look+sound identical); next: MIDI/focus-5.

## Upstream (AROS pull requests)
- [x] AHI honest rate (2026-10-07): six drivers write back the rate they actually run (HDAudio hardware-measured off-list split 50000->48000/60000->48000/100000->96000; NVHDMI/SB128/CMI8738/Envy24/Envy24HT compile-only). Branch `ahi-report-selected-rate` (6 commits + optional autodoc commit, UNPUSHED), PR draft `Vulkan4Aros/docs/dev/dispatch/drafts/ahi-selected-rate-pr.md`, carriage `src/abi-patches/v1/aros/0072-ahi-report-selected-rate.diff`. Evidence `docs/evidence/audio/ahi-rate/2026-10-07-h{1,2,3}-*.md`. (1) DONE 2026-10-08: Dell swap on a fresh boot, probe M = H within 0.1 % on all 8 rows (50000->48000 H=48049, 100000->96000 H=96098), RIAPP regression mix=48000 xruns 0, original restored (md5 verified). (2)+(3) DONE 2026-10-08: rebased onto upstream master, autodoc commit included, pushed as Ashahell:ahi-report-selected-rate (a5295e048a), PR aros-development-team/AROS#1518, approved and merged by Kalamatee 2026-10-08 (merge acd9a5fe2a), all 7 commits incl. the autodoc note.
- [x] AROS#1313 (`aros/0011`, carried in Vulkan4AROS) rework per Kalamatee's review 2026-09-30 — done: reworked and merged by Kalamatee 2026-10-01 (NOTIMPL restored, comment fixed, fields unguarded); only the optional `getrusage()` follow-up PR below remains open:
  - keep the fourteen new `struct rusage` fields; consider guarding everything beyond `ru_utime`/`ru_stime` with `#if defined(_BSD_SOURCE) || defined(_GNU_SOURCE)` (as `O_SHLOCK`/`O_EXLOCK` in `fcntl.h`); the guard makes `sizeof(struct rusage)` depend on feature macros, so a later `getrusage()` may write only the two POSIX fields — or argue for the unguarded layout in the PR;
  - remove the `int getrusage(int, struct rusage *);` declaration and restore the NOTIMPL line (nothing implements it: no `compiler/crt/posixc/getrusage.c`, still `.skip 2` in `posixc.conf`; header probes would report it available while links fail);
  - drop the "always fails with ENOSYS / see getrusage.c" comment; keep the existing note that the library does not implement it and the struct serves code with its own fallback;
  - correct our verification note: SPIRV-Tools sets `SPIRV_TIMER_ENABLED` only for Linux/Android, so an AROS cross build never includes `sys/resource.h`; an enabled timer would need `getrusage` at link time;
  - optional follow-up, separate PR: implement `getrusage()` — replace `.skip 2` in `posixc.conf` with the entry, add it to the mmakefile, fill `ru_utime` for `RUSAGE_SELF` from the per-task `iet_CpuTime` in `rom/exec/etask.h`.
  - [x] done: `getrusage()` for `RUSAGE_SELF` sent as aros-development-team/AROS#1490 (`484d7136b4`), approved and merged by Kalamatee 2026-10-07 (merge `a5f7f1739b`), no review comments.
- [x] AROS#1314 (`aros/0013`, carried in Vulkan4AROS) rework as an explicitly interim fix per Kalamatee's review 2026-09-30 — done: merged by Kalamatee 2026-10-01 with the FIXME and a once-only warning:
  - keep the forward of `strto*_l` to the plain `strto*` functions so libc++ `num_get` works today;
  - replace the "AROS locales are C-locale-only objects" comment with a FIXME: the locale argument is ignored because `struct __locale` has no numeric data yet (only ctype and case tables); these must be reworked when locale objects gain numeric conventions (`strtod()` then consults the current locale, `strto*_l()` the object passed in);
  - keep the `AROS_FUNCTION_NOT_IMPLEMENTED` visibility without firing on every call, e.g. a static once-only flag around a debug message saying the locale argument is ignored.

## Done
- (none yet)

## Levi performance: owner decisions (levi-perf P3, evidence docs/evidence/levi-perf/2026-10-06-p3-options.md; enabled docs/evidence/levi-perf/2026-10-06-q-enable.md)
- [x] O1 control rate: enabled at N = 8 (owner 2026-10-06)
- [x] O2 bank skip: enabled, static-morph gated (owner 2026-10-06)
- [x] O3 float kernels: not taken, code removed, evidence kept (`a284347`)
- [x] O4 voice cap 4/6/8 or a lite mode: not taken, code removed, evidence kept (`a284347`)
- [x] O5 denormals: no action (zero subnormal traffic)
- [x] Ship the P2 exact cuts to the stick (`Vk4aros:ReIncarnation/RIAPP`) (shipped as `e125db8`)
