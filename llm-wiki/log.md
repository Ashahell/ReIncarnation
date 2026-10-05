# ReIncarnation llm-wiki — log

## [2026-09-28] ingest | GUI hardware-look pass
- Disposition: New (slice record)
- Raw: llm-wiki/raw/articles/2026-09-28-gui-hardware-look-pass.md
- Updated: llm-wiki/index.md (Live app (§12.11) entry)
Commit `adc51ee`; AROS colour finding cross-posted to Vulkan4AROS.

## [2026-09-28] update | AROS#1281 record: r12 mechanism disputed by the end-to-end fix
- Disposition: Disputed (Status block on the follow-up; cascade from `2026-09-28-audio-in-qemu.md`)
- Raw: llm-wiki/raw/articles/2026-09-27-aros-1281-exchange.md

## [2026-09-28] record | AHI lifecycle guru (owner evidence)
- Disposition: New (no article covered it; evidence commit `3db1c92` grounded verbatim)
- Raw: llm-wiki/raw/articles/2026-09-28-ahi-lifecycle-guru.md
- Evidence: `docs/evidence/portability/ahi-lifecycle-guru.md`
- Updated: llm-wiki/index.md (Live app (§12.11) entry)

## [2026-09-28] record | Audio in QEMU (scratch HDA lane, music captured)
- Disposition: New (lane + rebuilds + ladder + proof; nothing covered it)
- Raw: llm-wiki/raw/articles/2026-09-28-audio-in-qemu.md
- Updated: llm-wiki/index.md (Live app (§12.11) entry)

## [2026-09-27] update | sb128 fault: byte-exact instruction found
- Disposition: Update (scratch HDA lane repro: stock probe faults identically; `movaps (%rdx)` at `DriverInit+0x135` over RDX≡12; 2026-09-21 movups patch never landed; durable fix scoped to fork lane)
- Raw: llm-wiki/raw/articles/2026-09-26-riqemu1-sb128-open-hang.md

## [2026-09-27] record | Trademark hygiene slice
- Disposition: New (assessment + rename + sample rules + Korg read; no existing article covered it)
- Raw: n/a (session source; facts grounded in `489bd69`, `project/arexx.c:26`, `reference/packs/classic-01`, `scripts/ri_audit.sh`)
- Updated: llm-wiki/index.md (Live app (§12.11) entry)

## [2026-09-27] update | Rail LED bitmap saga: root cause + fix
- Disposition: Update (the open saga is resolved in place: Resolution section)
- Raw: llm-wiki/raw/articles/2026-09-27-rail-led-bitmap-saga.md
Zune Bitmap.mui builds its transparency mask only in the remap pass (needs MappingTable/SourceColors); `Transparent` is a colour index. Rail LEDs are now a self-drawing Area subclass (RLed); green discs without a box on the Dell; toggle awaits the owner.

## [2026-09-27] record | Upstream AROS#1281 exchange (stealth)
- Disposition: New (exchange record: their theory, our falsification, posted data, narrower-probe offer)
- Raw: llm-wiki/raw/articles/2026-09-27-aros-1281-exchange.md
- Updated: llm-wiki/index.md (Live app (§12.11) entry)
- Rule banked: never name this project on public trackers

## [2026-09-27] design | Devices tab elegance challenge (owner)
- Disposition: Open question (owner verdict: works, but is it elegant?)
- Requirement: llm-wiki/raw/articles/2026-09-24-extensible-device-rack-requirement.md
  + docs/2026-09-24-improvement-opportunities.md §5.6
The requirement says ACTIVE (engine enable/disable, zero CPU, state
kept, block-boundary swap, mixer per instance); the tab does VISIBLE
only (ShowMe, engine keeps rendering). Verdict: scaffolding, not the
destination. Recommendation: make the bit mean ACTIVE first (sections
bitmask already read per block in ri_engine_load — the plumbing
half-exists), then decide its final home (rail vs row-power). Behavior
before chrome.

## [2026-09-27] panels | Devices tab: visibility toggles (unproven)
- Disposition: New (needs Dell proof)
- Updated: docs/2026-09-24-improvement-todo.md (RIAPP full panels)
Fifth Register tab with per-device toggle buttons (MUIO_Button +
ReturnID 1001..1004); ShowMe rows display-only, engine/mixer untouched;
VIS ev-log lines. t98 +Devices title/frame (RED FAIL -> PASS).

## [2026-09-27] crash | 909 pack descriptor dangle guru'd the render task
- Disposition: CLOSED by device proof (owner session, build cb0e702)
- Evidence: docs/evidence/portability/909-pack-dangle.md
Stack `lay[]` bound by pointer; `set_layers` now copies into
voice-owned `store[]` (t99 RED FAIL -> PASS, mutant killed). Piano key
was coincidence. Proof: 1132-event session (~24 min, 272k buffers),
909 beat + sweeps + pitch-walk repeat, clean TR STOP, no guru,
0 xruns (render_max 53% of period).

## [2026-09-27] panels | Tabbed RIAPP slice: t97/t98 + full canvases + step sync
- Disposition: New (unproven on device; needs Dell proof round)
- Updated: docs/2026-09-24-improvement-todo.md (RIAPP full panels)
Register.mui Synths/Drums/Mix/FX driven by the t98 page model over the
t97 visible set; 303B + 808/909 step sync (DSTEP/DSTEPAC ev-log);
engine +S303B|S909; audit 0/0. Open: device proof (4-tab screenshots,
909 programming -> audio, demo regression), visibility-selection UI,
909 demo content.

## [2026-09-27] decisions | RIAPP tabs + device visibility + 303B silent
- Disposition: Update (owner answers: layout + 303B content)
- Updated: docs/2026-09-24-improvement-todo.md (RIAPP full panels)
Tabs via stock Register.mui; device visibility model up front (aligns
with the 2026-09-24 extensible-rack requirement); 303B silent until
programmed (no demo content change).
- Disposition: Update (soak rerun + freeze retrospective)
- Evidence: docs/evidence/portability/dell-soak-rerun.log
68,657 buffers, 0 xruns, graceful close (unattended). Earlier freeze
unreproduced in 4+ later interactive runs; leading theory: two RIAPP
instances contending one AHI device (pre close-verify discipline).
- Disposition: Update (owner listening verdicts close two findings)
- Raw: llm-wiki/raw/articles/2026-09-26-lane-proof-round-t2-g7-t4.md
- Updated: docs/evidence/portability/delay-line.md
- Evidence: ev-log PAT selections vs heartbeat snd/pend tracking
Pattern selection flips audible (empty-slot dropouts as designed); delay
send blooms on-grid after the tempo-clock fix; comp fine downstream.
0 xruns throughout. Open: 5-min soak rerun, new-voice timbre, full panels.
- Disposition: New (device-proof record)
- Raw: llm-wiki/raw/articles/2026-09-26-lane-proof-round-t2-g7-t4.md
- Updated: llm-wiki/index.md (Lane infrastructure entry)
- Evidence: docs/evidence/portability/t2-lane-proofs.md + captures/traces
riqemu1: G7 final state equals committed trace; T2 byte-identical
(new-vs-old2, first-old-run outlier documented). Dell: RIAPP 2885 bufs
0 xruns on hardware. Open: owner listening, skinned/zoom proof, soak.
- Disposition: Update (owner answered all six questions)
- Raw: llm-wiki/raw/articles/2026-09-26-portability-t1-t8-implementation.md
- Updated: docs/superpowers/plans/2026-09-26-portability-plan.md (§9 resolutions)
- Evidence: docs/evidence/portability/t9-float.md (FTZ section), red-t94.txt
Resolutions: system-APIs-only, Makefile kept, FTZ ON (wired), WASAPI
shared→exclusive, %APPDATA%. Next: device proof round (owner lanes).
- Disposition: Update (implementation record grows to T1–T10)
- Raw: llm-wiki/raw/articles/2026-09-26-portability-t1-t8-implementation.md
- Updated: llm-wiki/index.md (Portability (§12.12) entry)
- Evidence: docs/evidence/portability/t2-canvas.md, docs/evidence/gui/host-raster/
Commits `6345908`, `b1c0956`; audit 0/0. Open: T8 riapp_core, device proofs, §8 decisions.

## [2026-09-26] record | Portability T1–T8 implementation (PAL + headless)
- Disposition: New (implementation record for the plan's T1–T8 slice)
- Raw: llm-wiki/raw/articles/2026-09-26-portability-t1-t8-implementation.md
- Updated: llm-wiki/index.md (Portability (§12.12) entry)
- Evidence: docs/evidence/portability/
Commits `0c14362`…`20d7c5b`; audit 0/0 each. Open: T2 canvas, T8/T9 remainders, T10 gates, device proofs, §8 decisions.

## [2026-09-26] ingest | Planned device: Korg Electribe ESX-1
- Disposition: New (owner requirement)
- Raw: llm-wiki/raw/articles/2026-09-26-planned-device-korg-esx1.md
- Updated: llm-wiki/index.md (Reviews, beside the device-rack requirement)

## [2026-09-26] record | Step 1 buffer default + riqemu1 sb128 open hang
- Disposition: New (two slice records closing Step-1 and lane-diagnostic gaps)
- Raw: llm-wiki/raw/articles/2026-09-26-buffer-default-256-dell.md, llm-wiki/raw/articles/2026-09-26-riqemu1-sb128-open-hang.md
- Updated: llm-wiki/index.md (Live app (§12.11) entries)

## [2026-09-26] record | G9b Step 2 panel in RIAPP, Dell-proofed
- Disposition: New (slice record: bridge + seqlock + panel + bounded-open fix + owner proof + decisions)
- Raw: llm-wiki/raw/articles/2026-09-26-step2-panel-dell-proof.md
- Updated: llm-wiki/index.md (Live app (§12.11) entry)
- Evidence: docs/evidence/gui/riapp-panel.md

## [2026-09-26] ingest | Portability plan (platform layer)
- Disposition: New (plan record)
- Raw: llm-wiki/raw/articles/2026-09-26-portability-plan.md
- Updated: llm-wiki/index.md (new section "Portability (§12.12)")
Plan `c05d49e`.

## [2026-09-26] ingest | Live AHI first sound on the Dell + buffer ladder + W capture
- Disposition: New (slice record); Update (corrects opencode's G9.3 "backend structure" claim: it was a placeholder)
- Raw: llm-wiki/raw/articles/2026-09-26-live-ahi-first-sound-dell.md
- Updated: llm-wiki/index.md (Live app (§12.11) entry)
Commits `dd93ae7`, `e884439`, `8953cf6`.

## [2026-09-26] slice | §12.11 G9 live app (opencode implementation)
- Disposition: New (completion record)
- Raw: llm-wiki/raw/articles/2026-09-26-g9-live-app-opencode.md
- Updated: llm-wiki/index.md (new Live app (§12.11) section)
Commits `48b552d`…`ee05365`; audit 0/0. Dell sound/soak still owner-lane.

## [2026-09-26] ingest | §12.11 G9 live-app handoff to opencode
- Disposition: New (handoff record)
- Raw: llm-wiki/raw/articles/2026-09-26-g9-live-app-handoff-opencode.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
Prompt `b8c00c2`.

## [2026-09-26] ingest | Automation Task 5b/5c lane keys + strip level
- Disposition: New (slice record); Update (supersedes R-5B/R-5C BLOCKED in the Tasks 2-4 record)
- Raw: llm-wiki/raw/articles/2026-09-26-automation-task-5b-5c-lane-keys.md
- Updated: llm-wiki/index.md (Reviews entry)
Commit `91d07b4`; audit 0/0.

## [2026-09-26] ingest | Dell owner pass, skin format 1, Dell skin-loading fixes
- Disposition: New (slice record); Disputed→corrected (earlier "808-RI renders on the Dell" claim, evidence doc Status: Outdated)
- Raw: llm-wiki/raw/articles/2026-09-26-dell-owner-pass-and-skin-format-1.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
Commits `3cc129c`, `1208e96`, `79e5424`; AROS lessons cross-posted to Vulkan4AROS.

## [2026-09-26] Tasks 2–4 | §12.9c automation lanes (sweep/edits/chase/publish/ATRK/audit + 10 mutants)
- Disposition: New (slice record)
- Raw: llm-wiki/raw/articles/2026-09-26-automation-lanes-tasks-2-to-4.md
- Updated: llm-wiki/index.md
Commits `7753178`/`ceca27e`/`bd56eab`/`6a76c47`/`4d34176`/`fe96f72`/`2820334` (supersedes `db13c14` per R-REVIEWED-PLAN). Rulings R-WRITE-BOTH/R-ATRK-ORDER/R-READER-ATOMICITY; mutants 10/10 (6 + 8 needed new pins: tick-exact paste-tail, staged-request). Task 5: 5a FX shipped (`c6dd53f`, mapped 14→28); 5b BLOCKED (voice key, R-5B-BLOCKED); 5c BLOCKED (IDs, R-5C-BLOCKED); spec Status blocks (`ab54103`).

## [2026-09-26] Revision | songtrack plan/spec deferrals closed against r2 owners
- Disposition: Update (no new raw — 9-line alignment edit)
- Updated: songtrack plan R10 + Step 4/5 notes, songtrack spec §5 table (+1 row)
- Commit: `29f9e35` [§12.9b]
Closed: capture gating (m68), knob halves (automation r2 §2.5), changeover (streaming m67). Still open: TRANSPORT emission, run view (GUI editor), master ppq/24 + rbng.h comment (automation Task 3c). Web corroboration of E1 automation claims: 3 queries, zero indexed sources — basis stays the in-repo manual greps.

## [2026-09-26] Task 3 | §12.9c streaming emission (merge/cap/split/loop/end + R4 carry)
- Disposition: New (slice record; supersedes the m67 mutant tally)
- Raw: llm-wiki/raw/articles/2026-09-26-streaming-emission-task3.md
- Updated: llm-wiki/index.md (mutant tally 9/9)
Tests-only commit `07cc95b` (production complete in Task 2); mutant (e) killed via dedicated START-carry scope (R-CARRY-SCOPE); full audit 0/0.

## [2026-09-26] ingest | §12.10 G8 complete code-side (skins, zoom, acceptance, C1–C4)
- Disposition: New (completion record)
- Raw: llm-wiki/raw/articles/2026-09-26-g81-g84-skins-zoom-acceptance-carryovers.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
G8.1 skins + G8.2 zoom + C1 audit wiring + G8.3 acceptance machine pass (ReBirth-101 1/5 + 4/5 partial) + C4 Stop law + C2 engine taps + C3 camd patch preservation; rulings R-G8.1-1..5; lane lessons (detached runs, Break, locked-file fetch, trace semantics, comma anomaly, full-res verdicts).
- Disposition: New (review record)
- Raw: llm-wiki/raw/articles/2026-09-26-automation-spec-and-plan-review.md
- Updated: llm-wiki/index.md (Reviews entry)
Spec r2 approved by owner; plan R1–R10 applied and aligned with opencode's Task 2 stubs (`6ee2892`).

## [2026-09-26] ingest | Dell E6320 native 1366x768
- Disposition: New (lane record; AROS facts cross-posted to Vulkan4AROS)
- Raw: llm-wiki/raw/articles/2026-09-26-dell-native-1366x768.md
- Updated: llm-wiki/index.md (Lane infrastructure entry)
GRUB default now vesa=1366x768x32 (vesagfx single mode; IntelGMA lacks Sandy Bridge); verified after reboot.

## [2026-09-26] ingest | §12.9c record-path capture gating (m68)
- Disposition: New (slice record)
- Raw: llm-wiki/raw/articles/2026-09-26-record-capture-gating.md
- Updated: llm-wiki/index.md (§12.9 entry), docs/2026-09-24-improvement-todo.md (gating ticked, m68)
Record gate shipped (feat b7e4896): RECORD-state + cursor-quantize capture, t59 PASS, 2 mutants killed, audit 0/0. Open: automation lanes (§12.9c remainder).

## [2026-09-26] ingest | Bare `Assign NAME:` removes the assign (lane rule from Vulkan4Aros)
- Disposition: New (lane rule; absent from this wiki — full-text search for EXISTS/insert-volume came back empty)
- Raw: llm-wiki/raw/articles/2026-09-24-assign-probe-removes-assign-lane-rule.md
- Updated: llm-wiki/raw/articles/2026-09-26-streaming-player.md (Task-1 `18d4835` provenance was unnamed); llm-wiki/index.md (Lane infrastructure entry)
Triaged the 2026-09-24/25 Vulkan4Aros lane findings against this wiki: e1000 Tx-pool fix + Dell ATCPBIN deploy, 32 KB send cap, session-13 wedge correction, DH0/Run QUIET/routes, aros_v1j rtl8139 switch — all already recorded here (No material). Lavapipe/CTS/juggler items are a different domain (No material).

## [2026-09-26] ingest | §12.10 G8 handoff to opencode
- Disposition: New (handoff record)
- Raw: llm-wiki/raw/articles/2026-09-26-g8-handoff-opencode.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
G8 scope + carry-overs C1–C4 + open owner decisions, as handed to opencode.

## [2026-09-26] ingest | §12.9c streaming player (m67)
- Disposition: New (slice record)
- Raw: llm-wiki/raw/articles/2026-09-26-streaming-player.md
- Updated: llm-wiki/index.md (§12.9 entry), docs/2026-09-24-improvement-todo.md (streaming ticked, m67)
Streaming player shipped (feat 2a9d111 + 49e362f): per-section phase, pattern-end changeover, per-block emission; t74 PASS; 8/9 mutants killed (END-carry survivor documented R-MUTSURV); audit 0/0. Open: songtrack implementation (owner direction), automation lanes + record gating.

## [2026-09-26] ingest | §12.10 G7 Remote MIDI (Standard Mapping) + AROS camd fix
- Disposition: New (slice record; camd finding cross-posted to Vulkan4AROS)
- Raw: llm-wiki/raw/articles/2026-09-26-gui-remote-midi-g7.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
Appendix C mapping proven over real CAMD after fixing camd's x86-64 varargs bug; debugdriver hang open; riqemu1 reboots pinned to 1280x1024.

## [2026-09-25] ingest | §12.10 G6a live state
- Disposition: New (slice record; lane notes cross-posted to Vulkan4AROS)
- Raw: llm-wiki/raw/articles/2026-09-25-gui-live-state-g6a.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
Sample-count playheads, running light, taps at the playhead with held delete, meter scales; proven with real keys on a labelled stand-in clock; G6b (render task + engine meter taps) open.

## [2026-09-25] lint | 0 issues (post-m66 sweep)
- m66 (songtrack) verified present in raw + log + index.
- No unindexed raws; all index/log links resolve.
- Sibling G5 commits present in history; their records are theirs.

## [2026-09-25] m66 | §12.9b song track (dense grid + emission + STRK)
- Disposition: New (reviewed plan, native execution, TDD t59)
- Raw: llm-wiki/raw/articles/2026-09-25-songtrack.md
- Updated: engine/seq/songtrack.{h,c} + songtrack_emit.h (new),
  project/rbng.* (STRK + track field), tests/unit/t59 (new),
  scripts/ri_audit.sh (Phase 7b loop + static-state grep + AROS line),
  docs/autodoc/seq.doc, docs/2026-09-24-improvement-todo.md
- Full `ri_audit.sh` 0/0. 8 mutants proven (incl. 1 hard fault).

## [2026-09-25] ingest | G5 key precedence decided
- Disposition: Update (decision record; supersedes the E0 precedence note in the G5 record)
- Raw: llm-wiki/raw/articles/2026-09-25-g5-key-precedence-decision.md
- Updated: llm-wiki/index.md (G5 entry: E0 → decided)
Owner-delegated: programming keys of the focused section win overlapping keys (TB-303/TR-808/909 write-vs-play mode analog); options stay independent (ReBirth menu); mutual exclusion rejected.

## [2026-09-25] ingest | §12.10 G5 keyboard + focus bar
- Disposition: New (slice record; cross-posted lane finding to Vulkan4AROS)
- Raw: llm-wiki/raw/articles/2026-09-25-gui-keyboard-focus-g5.md
- Updated: llm-wiki/index.md (GUI (§12.10) entry)
Appendix E map + front-panel focus; real-key proof via QEMU sendkey; two self-found bugs fixed; audit 0/0 verified in a scratch worktree because the live tree held the song-track WIP.

## [2026-09-25] ingest | Song track plan review (§12.9b)
- Disposition: New (review record; plan + spec committed `39d8de4`)
- Raw: llm-wiki/raw/articles/2026-09-25-songtrack-plan-review.md
- Updated: llm-wiki/index.md (Reviews entry)
Eleven findings (3 HIGH: lost capped changes, unclamped loop, dropped replay property), all applied; plan code assembled in a scratch worktree: t59 PASS, eight mutants FAIL, one self-inflicted guard trip caught and fixed.

## [2026-09-25] ingest | riqemu1 DH0 boot with visible agent; aros_v1j on rtl8139
- Disposition: New (lane record; cross-posted to Vulkan4AROS)
- Raw: llm-wiki/raw/articles/2026-09-25-riqemu1-dh0-boot-v1j-rtl8139.md
- Updated: llm-wiki/index.md (Lane infrastructure entry)
InstallAROS DH0 needs no extra assigns; User-Startup runs the agent in a CON: window (`Run QUIET` keeps the boot shell hidden; duplicate `route add` removed); fixed ATCPBIN.v1 installed; Live CD detached. aros_v1j: rtl8139 live, verified.

## [2026-09-25] ingest | §12.10 GUI parity G1–G4 (all sections)
- Disposition: New (rollup of 10 commits `f130142`…`31ed17b`)
- Raw: llm-wiki/raw/articles/2026-09-25-gui-parity-sections-g1-g4.md
- Updated: llm-wiki/index.md (new GUI (§12.10) section)
Registry, measured geometry, behaviour t60–t69, one RSection canvas, riqemu1 demo proofs. Registry fix: PCF Freq/Q/Amt/Decay are faders (p. 159). Open for owners: Dist exclusivity (p. 59 vs p. 157), engine Stop law vs p. 145.

## [2026-09-25] lint | 0 issues (post-m65 sweep)
- m65 (transport) verified present in raw + log + index.
- No unindexed raws; all index/log links resolve.
- No new commits or uncommitted work since the m65 record: nothing
  to compile. Tree clean at b169425.

## [2026-09-25] m65 | §12.9a transport state machine (first song slice)
- Disposition: New (reviewed plan, native execution, TDD t58)
- Raw: llm-wiki/raw/articles/2026-09-25-transport.md
- Updated: engine/seq/transport.{h,c} (new), riseq.{h,c} (state),
  tests/unit/t58 (new), scripts/ri_audit.sh (seq block),
  docs/2026-09-24-improvement-todo.md
- Laws: stop-click, last-valid-bar seeks, loop canonicals, ppq norm.
- Full `ri_audit.sh` 0/0. 3 mutants proven.

## [2026-09-25] m64 | §12.7 engine 808/909 hosting (4 sections live)
- Disposition: New (bounded design, owner-approved; TDD t57)
- Raw: llm-wiki/raw/articles/2026-09-25-engine-808-909-hosting.md
- Updated: engine/engine.{h,c} (sets, routing, sections, bind),
  tests/unit/t57 (new), scripts/ri_audit.sh (Phase 7b loop),
  docs/2026-09-24-improvement-todo.md
- Review's "808: no choke" outdated (t42 pins it); AC retroactive exact.
- Full `ri_audit.sh` 0/0. Zero golden fallout.

## [2026-09-25] lint | 0 issues (post-m63 sweep)
- m63 (pattern-model-rbng-v11) verified present in raw + log + index.
- No unindexed raws; all index/log links resolve.
- Sibling lanes untouched by this sweep: §12.10 GUI work
  (gui-parity plan + ctlreg.{h,c} + t60 + MOD_gui line) is uncommitted
  under its own coexistence rule; its G1.6 audit wiring is unblocked
  now that §12.7a has merged — flagging, not claiming.
- check_evidence.py not run: it targets the skill's wiki/<topic>/
  layout with Raw fields; this wiki's raw/articles ARE the source
  records, so index+link lint is the applicable check.

## [2026-09-25] m63 | §12.7a pattern model + RBNG v1.1 (first slice)
- Disposition: New (architectural path; TDD t53–t56)
- Raw: llm-wiki/raw/articles/2026-09-25-pattern-model-rbng-v11.md
- Updated: engine/seq/pattern.{h,c} + pattern_emit.c (new), sched
  (octave/carry/gate), project/rbng.* (BANK + conversion), 7 ledgers,
  bank-v11 golden + fuzz seeds, audit Phase 7b, todo §12.7, spec status
- Commits f6b958f/7f20572/4afcfc7/1fd0d38/f25f2ec/e925f6e/0961d60.
- Full `ri_audit.sh` 0/0 (baseline 0/0 before).

## [2026-09-25] ingest | Pattern model: fidelity review, implementation plan, slide-direction finding
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-25-pattern-model-fidelity-review-and-plan.md
- Updated: llm-wiki/index.md (Reviews)
- Also: aros_v1j rtl8139 switch still pending — idle watcher running (lane busy with another session's w097 run)

## [2026-09-25] update | aros_v1j left on old e1000 (busy); VM lanes should use rtl8139
- Disposition: Update (follow-up section in the 2026-09-25 root-cause record)
- aros_v1j mid-run for another session (w097 poll every ~30 s) → not rebooted; recommend `-device rtl8139` at its next restart, keep one private e1000 lane for Dell-driver regression coverage

## [2026-09-25] m62 | §12.8 routing matrix + pan + stereo return (engine home)
- Disposition: New (bounded design, owner-approved; TDD t51/t52)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-8-routing.md
- Updated: engine/fx/route.{h,c} (new), engine/engine.{h,c} (inserts,
  pan/send, delay send + stereo return, master linked comp),
  engine/fx/fx.{h,c} (resync/pcf-raw/set-rate/linked helpers),
  scripts/ri_build_host.sh (route TU), tests/unit/t51+t52 (new),
  docs/2026-09-24-improvement-todo.md
- Neutral bit-identical (zero golden fallout). Full `ri_audit.sh` 0/0.

## [2026-09-25] m61 | §12.8: dist 2x oversample + comp ratio/GR meter
- Disposition: New (TDD t50 RED-first)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-8-dist-comp.md
- Updated: engine/fx/fx.{h,c} (OS state, ratio, GR meter, makeup helper,
  dist reset, pattern-54 clamp), tests/unit/t50 (new), tests/unit/t1_fx
  (restart protocol), tests/golden/pcf/fx-chain.wav (+sidecar, −38 dB
  root-caused re-baseline), docs/2026-09-24-improvement-todo.md
- Alias 13 kHz fold 2.5x down; GR −9.5 dB hot. Full `ri_audit.sh` 0/0.

## [2026-09-25] m60 | RI-STEPS exit path: canonical union loop (close + clicks live)
- Fix: `app/stepproof.c` event loop is now NewInput-first + `Wait(app|timer|break)`
  (replaces pure manual-`Wait`; diag13 reason superseded — the seeded-bit failure
  only applied to pure-NewInput sleep, the union serves both).
- Device proof (QEMU lane): `ui-close RI-STEPS` exits the process cleanly
  (Status confirms gone); step-toggle click moves PAT 0000→0001 (clicks served).
- Gates: V11 `-Werror` clean, V1 link clean, full `ri_audit.sh` 0/0.
- Closes the owner-feedback entry below (close gadget was dead, now served).

## [2026-09-25] owner-feedback | RI-STEPS close gadget dead (m43 tradeoff confirmed)
- Owner tried to close the RI-STEPS window via gadget on the QEMU lane: nothing
  happens (manual-`Wait` loop never serves NewInput — documented m43 tradeoff).
  Killed from here via `Break <pid> C` (clean, lane tidy). Vehicle needs a real
  exit path (input hook/subtask) before Dell hands/feel.

## [2026-09-25] lint | 0 issues (ingest sweep m58–m59 + e1000 lane)
- m58/m59 raw+log+index verified present; no unindexed raws; all links resolve.
- Sibling e1000 lane work logged by its author (ingest + deploy entries);
  their untracked e1000 article left for their commit.
- No contradictions; D-k/D-l adoption entry stands.

## [2026-09-25] m59 | §12.8c2: Appendix-D patterns extracted + wired
- Disposition: New (E1 mechanical extraction + TDD wiring)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-8c2-pcf-patterns.md
- Updated: engine/fx/pcf.{h,c} (table, install, wrap, resolutions),
  tools/render.c (fixtures install pattern 0), tests/unit/t49 (new),
  tests/unit/t25_pcfopen (55 + refusal intact),
  docs/evidence/pcf/patterns.md (new E1 ledger) + engine.md (OPEN-04 closed),
  reference/pcf-patterns.{bin,json} (new ledger), scripts/ri_audit.sh
  (patterns presence gate), docs/2026-09-24-improvement-todo.md
- 55 patterns (review said 54 — off by one). Zero golden fallout.
- Full `ri_audit.sh` 0/0.

## [2026-09-25] m58 | §12.8c1: PCF envelope + integer clock + HP drop
- Disposition: New (TDD, t48 link-RED → PASS with revert-checks)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-8c1-pcf-envelope.md
- Updated: engine/fx/pcf.{h,c} (env, integer clock, restart, step_index,
  decay knob, HP drop), engine/fx/fx.{h,c} (PCF_DECAY echo/apply),
  tests/unit/t48 (new), tests/unit/t1_fx (clock assert),
  docs/2026-09-24-improvement-todo.md
- Zero golden fallout (Amt≈0 fixtures). Full `ri_audit.sh` 0/0.

## [2026-09-25] deploy | New ATCPBIN (drops desynced sessions) live on the Dell
- Disposition: Update (deployment status in the 2026-09-25 root-cause record)
- `SYS:ATCPBIN` sha `a576a88f…` (backup `.pre-sendbroken`); reboot → agent back 10:13:37 (session 23); 5/5 captures PASS

## [2026-09-25] ingest | Private e1000 A/B lane: recipe and gotchas
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-25-private-e1000-ab-lane-gotchas.md
- Updated: llm-wiki/index.md (Lane infrastructure)
- Also: `Version <ELF>` did not kill the agent this time (earlier landmine not reproduced); agent `g_send_broken` change built (v1 + v11) but NOT yet deployed to any lane

## [2026-09-25] deploy | Patched e1000.device live on the Dell
- Disposition: Update (deployment status in the 2026-09-25 root-cause record)
- Backup `e1000.device.pre-txpool`; installed sha `b98be161…`; reboot 09:11 → agent back 09:13:26 (session 22); 10/10 scale-2 captures PASS

## [2026-09-25] m57 | §12.8b1: delay Steps/triplet/fb-infinite
- Disposition: New (TDD, t47 RED 6 → PASS)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-8b1-delay-parity.md
- Updated: engine/fx/fx.{h,c} (steps model, STEPS/TRIPLET IDs, fb 1.0),
  tests/unit/t47 (new), fx-delay + fx-chain goldens + sha (root-caused),
  docs/2026-09-24-improvement-todo.md
- First diffs at second echo (mechanism proof). Full `ri_audit.sh` 0/0.

## [2026-09-25] ingest | Lane wedge root cause: e1000.device FreeMem() in IRQ context
- Disposition: New; Disputed
- Raw: llm-wiki/raw/articles/2026-09-25-e1000-wedge-root-cause-freemem-in-irq.md
- Updated: llm-wiki/index.md (new "Lane infrastructure" section)
- Proven by reproduction: original driver + memstress wedged at round 14 with `tlsf_freevec` ← `Exec_35_FreeMem` ← `e1000func_clean_tx_irq` ← `e1000func_IntHandler`; fixed driver 45/45, memstress corrupt=0
- Supersedes: 2026-09-24-spike-lane-wedge-fix ("single ~786 KB Send()") and m42 "transfer size, not driver"; disputes Vulkan4Aros arostcp-outbound-wedge.md "QEMU-emulation-side"
- Dell patched driver built (sha b98be161…), not deployed (approval pending); aros_v1j untouched (busy)

## [2026-09-25] m56 | §12.8a: delay beats + caller-owned lines + live comp
- Disposition: New (TDD, t46 link-RED → PASS with revert-checks)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-8a-fx-delay.md
- Updated: engine/fx/fx.{h,c} (beats store, retarget/slew, CreateDelay,
  Destroy, per-render comp derive, 20–500 clamps), tests/unit/t46 (new),
  tests/unit/t1_fx §7 (destroy/reuse contract),
  docs/2026-09-24-improvement-todo.md
- Zero golden fallout (fixtures on direct API). Full `ri_audit.sh` 0/0.

## [2026-09-25] m55 | §12.6b: 909 flam decouple + OH-wins + linear mix
- Disposition: New (TDD, t45 link-RED → PASS with revert-checks)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-6b-909-decouple.md
- Updated: engine/dsp/rb909.{h,c} (flam bit/width, hat rules, linear mix),
  tests/unit/t45 (new), tests/unit/t1_909 §6 (OH-wins re-contract),
  docs/evidence/909/ch.md + oh.md (steal rows),
  docs/2026-09-24-improvement-todo.md
- Zero golden fallout (fixtures under clip threshold). Full `ri_audit.sh` 0/0.
- Held: velocity levels (§12.7), panel hat knob (§12.10).

## [2026-09-25] m54 | §12.6a: 909 eleven voices + Level/Decay knobs
- Disposition: New (TDD, t44 RED → PASS with revert-checks)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-6a-909-voices.md
- Updated: engine/dsp/rb909.{h,c} (IDs, fields, knob application),
  engine/dsp/params.c (LEVEL/DECAY wired), tools/render.c (bake sweep/click,
  maps, wants), tests/unit/t44 (new), tests/unit/t1_909 (bad-voice 11),
  bd + 5 new goldens + sha, 5 ledgers, audit Phase 9 (11-voice loops),
  docs/2026-09-24-improvement-todo.md
- 5 old goldens byte-identical (bypass transparency). Full `ri_audit.sh` 0/0.

## [2026-09-25] m53 | §12.5c: BD 808 regime (paper-read, E1 numbers)
- Disposition: New (TDD, t43 RED 7 → PASS)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-5c-808-bd.md
- Updated: engine/dsp/rb808.h (62 Hz / 4 ms sigh), tests/unit/t43 (new),
  t1_808 §1 + t23_808bd (re-based trajectories), t42 BD-tone meter (click band),
  bd + storm goldens + sha (root-caused), docs/evidence/808/bd.md,
  docs/2026-09-24-improvement-todo.md
- SD verified structural (no code); recursion held. Full `ri_audit.sh` 0/0.

## [2026-09-25] m52 | §12.5b: metal fixed freqs + choke + per-sound params
- Disposition: New (TDD, t42 RED → PASS with honest meters)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-5b-808-metal.md
- Updated: engine/dsp/rb808.{h,c} (METAL_HZ, blep, choke, tune/snappy/tone),
  engine/dsp/params.c (SNAPPY/TONE wired), tests/unit/t42 (new),
  tests/unit/t23_808hat + t1_808 §2 (E1 re-contract),
  ch/oh/cy/cb/storm goldens + sha (root-caused), 4 voice ledgers,
  docs/2026-09-24-improvement-todo.md
- 11 other goldens byte-identical (exact defaults). Full `ri_audit.sh` 0/0.

## [2026-09-25] m51 | §12.5a: 808 slots/MA/level (structural, 16 sounds)
- Disposition: New (TDD, t41 + per-family revert-checks)
- Raw: llm-wiki/raw/articles/2026-09-25-imp-12-5a-808-slots.md
- Updated: engine/dsp/rb808.{h,c} (slots, MA, level, accent law),
  engine/dsp/params.c (LEVEL/ACCENT wired), tools/render.c + bench.c (NSOUNDS),
  tests/unit/t41 (new), t34 (slot rewrite), mask pins (0xFFFF),
  storm + ma goldens + sha, audit Phase 8 (ma loops), docs/evidence/808/ma.md,
  docs/2026-09-24-improvement-todo.md
- 15 single-voice goldens byte-identical; storm structural (11 slots).
- Full `ri_audit.sh` 0/0.

## [2026-09-24] m50 | §12.4c: log-domain glide (RC on pitch CV, τ kept)
- Disposition: New (TDD: ri_log2 link-RED → Taylor-RED → atanh-GREEN; t40 RED 3 → PASS)
- Raw: llm-wiki/raw/articles/2026-09-24-imp-12-4c-logslide.md
- Updated: engine/dsp/kernels.{c,h} (ri_log2), tests/property/t3 (log2 pins),
  engine/dsp/rb303.{h,c} (logfreq/log_target + exactness guard), params.c
  (tune maintains logs), tests/unit/t40_logslide.c (new),
  first-light set + sched-check.wav + sha (root-caused re-baselines),
  docs/2026-09-24-improvement-todo.md
- t22_303slide passes unchanged. Full `ri_audit.sh` 0/0. Owner A/B flagged.

## [2026-09-24] lint | 1 issue found, 1 fixed (ingest sweep m43–m49 + rack)
- Coverage: m43/m44/m45/m46/m47/m48/m49 all raw+log+index OK; no unindexed raws.
- Gap found: D-k/D-l adoption (f959fdf) unlogged; sibling rack entry still said
  "proposal only" — closed by the adopt entry above (append-only, no rewrite).
- Links: all index/log targets resolve. Evidence: RED counts (731/253/1246/395),
  revert-check (19201), gate lengths (2572), onset (sample 4), 86.55 ms mean
  (rounded conversion of verbatim 86554.2 µs) all traced to raw.
- No contradictions (12.1's identical-goldens claim predates the deliberate
  12.4a/b re-baselines; each article is dated).

## [2026-09-24] adopt | D-k/D-l moved proposal → normative (spec §20)
- The device-rack ingest entry below recorded "spec not edited (proposal only)".
  Superseded same day: `f959fdf` adopts D-k (rack model, Classic bit-identical
  preset, other racks = Power Mode) and D-l (compiled-in registry until OPEN-07)
  into `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` §20.
- Engine skeleton (§12.3 section mask + reserved bits) noted compatible; rack
  iteration deferred to §12.7/RBNG v2. No raw article (two-row table change);
  source of truth is the spec diff itself.

## [2026-09-24] m49 | §12.4b: dual envelopes (MEG/VEG + sweep, E1 values)
- Disposition: New (TDD, t39 compile-RED → PASS; deliberate revoicing)
- Raw: llm-wiki/raw/articles/2026-09-24-imp-12-4b-meg-veg.md
- Updated: engine/dsp/rb303.{h,c} (meg/veg/sweep/accented, lineage laws),
  engine/dsp/params.c (unchanged dispatch — TUNE/normalize carry over),
  tests/unit/t39_meg_veg.c (new), tests/unit/t22_303slide.c (dual-env pins),
  first-light set + sched-check.wav + sha (root-caused re-baselines),
  docs/evidence/303/filter-candidate.md (D7 superseded, D8 added),
  docs/2026-09-24-improvement-todo.md
- Accent pins green unmodified. Full `ri_audit.sh` 0/0. Owner A/B flagged.

## [2026-09-24] m48 | §12.4a: gate-length rule (half-step fall, D-h E0)
- Disposition: New (TDD, t38 RED 2 → PASS; contract change)
- Raw: llm-wiki/raw/articles/2026-09-24-imp-12-4a-gate-length.md
- Updated: engine/seq/sched.c (fractional OFFs, tie suppression),
  tests/unit/t38_gate_fraction.c (new), t1_303walk/t21_songfile/t21_schedfeed/
  t21_snapbuild/t21_looppcm (deliberate D-h-cited pin updates),
  3 first-light goldens + events + sha + sox table (root-caused re-baseline),
  docs/evidence/sequencer/gate-length.md (new E0 ledger),
  docs/2026-09-24-improvement-todo.md
- t1_sched passes unchanged (OFF count preserved). Full `ri_audit.sh` 0/0.

## [2026-09-24] ingest | Owner requirement: extensible device rack
- Disposition: New; Update
- Raw: llm-wiki/raw/articles/2026-09-24-extensible-device-rack-requirement.md
- Updated: docs/2026-09-24-improvement-opportunities.md (new §5.6, decisions D-k/D-l, work-order steps 2–3); llm-wiki/index.md (Reviews section)
- Classic stays the default 4-device rack (bit-identical); spec not edited (proposal only)

## [2026-09-24] m47 | §12.3: one renderer wired (engine core, 303s skeleton)
- Disposition: New (TDD, t37 link-RED → PASS with revert-check)
- Raw: llm-wiki/raw/articles/2026-09-24-imp-12-3-one-renderer.md
- Updated: engine/engine.{h,c} (new; walker + 303A/303B + f64 master + mono fold),
  tools/render.c (both song loops rewired, apply_event retired),
  audio_io/audio.c (thin mono sink, engine-loaded rewind, I1 marker retired),
  scripts/ri_audit.sh (tripwire → wired-core gate), scripts/ri_build_host.sh
  (MOD_engine), tests/unit/t37_engine_single.c, docs/2026-09-24-improvement-todo.md
- First-light golden + events byte-identical; t6 file-vs-live IDENTICAL.
- Full `ri_audit.sh` 0/0 (one incident: 0a rejects "preallocated" in comments).

## [2026-09-24] m46 | §12.2: 303 control-ID contract (wave/volume, 303B, Tune)
- Disposition: New (TDD, t36 RED 19 → PASS)
- Raw: llm-wiki/raw/articles/2026-09-24-imp-12-2-303-control-contract.md
- Updated: engine/dsp/rb303.h (303B IDs + TUNE + `tune_st`), params.c (shared
  dispatch + ratio bend), rb303.c (tune in note/slide), gui/panels.c (8 rows ×
  2 sections), docs/ReIncarnation.guide (map corrected), tools/render.c
  (303B non-forwarding documented for §12.3), docs/2026-09-24-improvement-todo.md
- 303 goldens byte-identical. Full `ri_audit.sh` 0/0. Codegen HELD (YAGNI, t36 pins it).

## [2026-09-24] m45 | Input-device ReplyPort hygiene (shared-lane commit)
- 2-line fix in `gui/widgets/rknb.mcc.c` (`rknb_input_open`): set
  `io_Message.mn_ReplyPort` after `CreateIORequest` (AROS convention — the
  call does not set it; same pattern as stepproof/diag13 timer setup).
  Behavior-neutral on today's DoIO-only warp path; forward-correct for SendIO.
- Verified: AROS compile-only with exact audit Phase-12 flags (v1 SDK) clean.
  Stale ~6.5 h in the tree, sibling lane undisturbed (RAM: binaries unaffected).
- No new raw (convention already practiced in-tree); recorded here for coverage.

## [2026-09-24] ingest | Codebase review: ReBirth 2.0 + hardware fidelity, verified defects
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-24-codebase-review-rebirth-fidelity.md (companion doc: docs/2026-09-24-improvement-opportunities.md)
- Updated: llm-wiki/index.md (new "Reviews" section)
- Verified by host harness: `ri_exp` diverges below ≈ −25 (exp(-100)=-60.1); 808 RS/CL/CH/OH reach 1.400 full-scale 2–3 s after one trigger; ri_sin(500)=-7.8e15
- Code defects: 303 ID 0x0305 volume/wave mismatch + 303B unhandled; delay beats overridden (fx.c:332); FX pool leak; PCF beat_pos stall; 808 soft-clip step 1.0→0.647; one-renderer 303-only; mono
- New E1 source: ReBirth RB-338 2.0.1 Owner's Manual — PCF Appendix D diagrams (patterns 0–53) = OPEN-04 closure candidate; tempo 20–500 contradicts code/RBNG 30–300; 808 11 slots/16 sounds (maracas missing) re-opens spec §2.3 item 1
- Cascade: spec not edited (proposals only, doc §11); working-tree rknb.mcc.c reply-port line noted as redundant, not reviewed further

## [2026-09-24] ingest | Owner approvals close m33/m34 pendings + ui-capture lane note
- Disposition: Update (index cascade; approval + lane findings already written in the articles during device testing)
- Raw: llm-wiki/raw/articles/2026-09-24-knob-name-labels-mui.md, llm-wiki/raw/articles/2026-09-24-commit-observable-undo-counter.md
- Updated: llm-wiki/index.md (eyeball/count pendings → owner-approved 2026-09-24)
- m33 knob labels approved (spelling + alignment); m34 commit observable approved (UNDO click→1, drags accumulate; acceptance checkbox checked); lane note: ui-capture wedged the agent with a healthy app — agent-side flake, detached-only rule stands
- Still open per acceptance.md: fader travel; step-toggle click check (m35)

## [2026-09-21] upstream | Filed aros-development-team/AROS#1281, corrected twice, now OCR-verified
First filing quoted screenshot-derived values that failed corroboration against the ISO driver binary. Second pass used tesseract OCR on our OWN QEMU HMP screenshots: verified call chain probe main → OpenDevice → lddemon → ahi _DevOpen → _LoadModeFile → hdaudio _LibInit → InitResident → fault, plus config-parse signpost just before and crash-alert buttons present. Issue rewritten to verified facts + marked hypotheses: https://github.com/aros-development-team/AROS/issues/1281. Lesson reinforced: screenshot values are observations until cross-checked; OCR+HMP makes them checkable even when images can't be viewed.

## [2026-09-21] upstream | HDAudio driver: no newer upstream fix exists
Checked both upstreams via local clones + GitHub commits API for `workbench/devs/AHI/Drivers/HDAudio`: aros-development-team tip `1fc148378e` (2026-08-16 BAR mapping) and deadwood2 tip `29dfac42dd` (2026-08-15 init-failure handling) both match local trees — nothing newer. AHI `Device/` dir shows only catalog/copyright churn since July. Blocker stands; #1281 filed (corrected to verified-facts-only) or fix in-tree as separate AROS work.

## [2026-09-21] fix | Guest probe page-fault: stale r12 SDK, fixed to v1 build-pc tree
**Tell:** any cross-built guest binary faults on its first LVO call (user-mode near-NULL read, CR2=0x2a9) while stock binaries (C:Date) run — check `objdump -d bin | grep -c 'mov %rax,%r12'` (must be 0).
Root cause: `ri_build_aros.sh` used the env-default v11 SDK whose proto headers emit the stale r12 base convention; the v1 guest reads base from rdx. Proven by TU-level disasm diff (v11 trees: r12=2; v1 tree: r12=0/rdx=2) + hello_ri FAIL→PASS on-guest after rebuild. Fixes in tree: v1 SDK locked by absolute path + CRT shim (absolute symlinks — relative ones dangle) in `ri_build_aros.sh`; same SDK switch + r12==0 artifact gate in `ri_audit.sh` (full audit green). Companion finding (AROS-side, out of scope): audio init on codec-less hardware kills the calling task (agent wedge); exact fault site UNCONFIRMED — driver-init attribution hypothesis only. M1.1 numbers stay UNMEASURED. Diag method that worked: hello-probe bisect, QEMU HMP screenshots + sendkey dismissal, stale spool-job clearing before reboot. Skill `aros-development` gained the addendum; backup serial preserved at /tmp/v1_serial.log.pre-reboot-crash-evidence.

## [2026-09-20] exec | Implementation plan executed (14/14 tasks, branch work/ri-planexec)
Subagent-driven execution with per-task review + fix loops (1–5 rounds) + final whole-branch review. All tasks complete: Tasks 1–4, 6–14 green (G5 stays OPEN — no guest audio hardware; M1.1 numbers UNMEASURED, no fabrication). Notable catches: brief kernels recipe disproven by measurement (superseded), 3 silent 909 goldens (phase-reduced + audibility gate), wrapper-PCF state reset (mutant-proven), one-renderer divergence (tripwire landed, extraction is follow-up). Final review: mergeable. Rulings + deferred minors in .superpowers/sdd ledger (workspace removed per skill; git history is the record).

## [2026-09-20] plan | Implementation Plan v2 (review #2 applied)
Rewrote plan to 14 tasks with numbered gates G0–G14: target-driven build + AROS skeleton + lab rules; bounded table kernels with 10k-determinism T2; multi-segment clock with locked rounding + int128/portable runs; ledger-first first light (math + event goldens, compare tool, minimal walker); split W1 (spike G5, backend G6); measured scheduler; hardened Tasks 8–13 (exact headers, concrete assertions, ledger rows, audit phases); plan-level DoD. Verification: 14 task headers, 15 gate lines, placeholder scan CLEAN (meta mention only). Also resolves the currency-review drift flags (OPEN-09/P-21, f32/f64, §0.1 traceability now in-plan).

## [2026-09-20] ingest | Currency review: plan + index vs spec v5
- Disposition: Update (index); No material (plan file itself, prior-art, decisions — already recorded).
- Fixed stale index: "v4 current" header → v5; OPEN table 8 rows → 9; ledger P-01–P-20 → P-01–P-21.
- Flagged plan-vs-v5 drift (fold into plan before Task 5 starts, no file changes today): Task 5 should cite OPEN-09/P-21 explicitly (device-buffer floor + render-task priority as M1.1 outputs); Task 10 mixer should state f32 buses + f64 master + single f32 rounding; Task 4 first-light now traces to §0.1 normative rule, not just convention. Spec and plan otherwise consistent (first-light, engine/device split, hook-woken render task all present in plan Tasks 4–5).

## [2026-09-20] plan | Implementation plan written (13 tasks)
Wrote `docs/superpowers/plans/2026-09-20-reincarnation-implementation.md` (512 lines) via writing-plans skill: phased lab→kernels→clock→first-light→W1→scheduler→808→909→PCF/FX→mixer→GUI→formats→REL, full TDD steps with real C/commands, Vulkan4AROS toolchain + audit reuse, W3 deferred appendix. Self-review: spec coverage mapped per section, placeholder scan CLEAN (one meta mention), type consistency grepped. Tree dirs created (engine/audio_io/midi_io/gui/project/scripts/tests/tools).

## [2026-09-20] review | Spec v4 → v5 (review #3)
- Added: §0.1 first light (one offline command, one 303, one golden before anything else); §4.2 AHI hook context (hooks may only Signal; render lives in a Task; low-level API first, ahi.device fallback); engine block (64, locked) vs device buffer (negotiated, OPEN-09) split; §8 per-type payload mapping + insertion-index tiebreak (total order for D0); §15 f32 buses / f64 master accumulator + project-owned kernels; §7 128-bit mul_div intermediate with T1 overflow proof; P-12 accent placeholder reconciled; P-21/OPEN-09 rows.


## [2026-09-20] spec | Engineering Specification v4 — review #2 applied
Applied all 8 ordered actions + structural items (403 lines): front normative-summary + machine-readable OPEN table (8 rows); Appendix A ledger P-01–P-20; Appendix B 303 candidate equations; Appendix C 808/909 skeletons; Appendix D demoted sketches + ABI freeze gate; locked clock rules + 303 state machine; §17 degraded-mode table (8 failures); §18 strict build gates + W1.1 report gate; GUI owned-UX policy; PCF black-box plan; docs/evidence/ dirs created. Excellent parts untouched. Verification: placeholder scan CLEAN, appendices + OPEN rows + section refs grepped.

## [2026-09-20] prior-art | Internet prior-art sweep documented and applied
Four parallel research streams (303 DSP, 808/909 drums, AROS platform, engine methods) returned 32 findings, written to `reference/prior-art.md` with URLs, evidence classes, and per-entry spec impacts. Useful items applied to the spec (272 lines): dual-envelope accent correction, 3-on/3-off gate + 44 µs latch timing, filter-topology constraints, excitation-scale accent, kick attack bump, metal-generator freqs, snare tolerance, clap tail param, 808 three-state verify flag, 909 hybrid + Tune=clock + shared-ROM rule + no-ROM-dump rule, AHI ≥20 ms floor, CAMD/realtime + Poseidon scope, MCC widget reuse, IFF/iffparse + datatypes, rational mul_div clock, split-at-boundary + flush/process params, D1 enforcement list, golden fail-only diagnostics. Verification: placeholder scan CLEAN, 11 register-entry refs, no CJK residue.

## [2026-09-20] spec | Engineering Specification v3 — M2.1 gates closed
Closed all five OPEN gates in `docs/superpowers/specs/2026-09-20-reincarnation-spec.md` (271 lines): frozen RIEvent + ordering; §4.1 single-thread ownership + deferred SMP DAG/miss policy; M2.1 interaction numbers (E0 knob/fader/step + E1 303/909 programming from SoS/Open303/Zune research); asset pipeline (masters, 2x art, manifest, tools); bench procedure + provisional budgets; E1 Open303 constants with recorded 40-vs-60 ms slide tension for M2.2. Verification: only [OPEN] hit is the marker legend; all gate greps pass.

## [2026-09-20] decisions | Four open decisions closed into the spec
D-001 locked: second skin "808-RI" (user-proposed). Slide method locked: slide TC as runtime parameter, blind 40-vs-60 A/B rig at M2.2, lock on verdict. Reference box: stays unnamed until the M1.1 benchmark report names it. W3: strict deferral confirmed, no design text beyond the locked list until W2 gates pass.

## [2026-09-20] ingest | Seeded llm-wiki with AROS audio material from Vulkan4AROS
Copied verbatim `raw/articles/2026-07-16-hosted-aros-wsl2-audio-ahi-alsa-pulse-bridge.md` (WSL2 host fix: AHI needs ALSA→Pulse bridge, else SDL SIGILL-cascade; env-only).
Added excerpts: roadmap audio status ("AHI is ported ... AHI Prefs + MP3 player") and HIDD sound-system draft (`HIDDA_Capabilities`).
Cross-refs recorded in `index.md` for `log.md:3273-3295` and `index.md:366` summaries in Vulkan4AROS.
No Rebirth 2.0 / audio-upgrade design entries were found in Vulkan4AROS — this wiki is the seed they will be written into.

## [2026-09-20] ingest | Comprehensive Plan: AROS Audio Modernization + "ReBirth 2.0 for AROS"
Stored user-provided plan verbatim at `raw/articles/2026-09-20-aros-audio-modernization-rebirth-2.0-plan.md` and linked from `index.md` Plans section.
Covers W1 audio foundation, W2 RB-338 v2.0.1 parity, W3 Power Mode, risks, timeline. Legal note accepted as constraint: original artwork/samples, distinct external name.

## [2026-09-20] ingest | ReIncarnation — Deep Technical Dives
Stored user-provided deep dives verbatim at `raw/articles/2026-09-20-reincarnation-deep-technical-dives.md` and linked from `index.md` Plans section.
Covers TB-303 DSP model, audio.library API sketch, FORM RBNM skin/mod format. Companion to the Comprehensive Plan.

## [2026-09-20] ingest | ReIncarnation — Deep Technical Dives, Part 2
Stored user-provided part 2 verbatim at `raw/articles/2026-09-20-reincarnation-deep-technical-dives-part2.md` and linked from `index.md` Plans section.
Covers PCF filter + 54-pattern table, TR-909 multi-layer sampler voice, FORM RBNG song format. Classic audio paths now fully specified.

## [2026-09-20] ingest | ReIncarnation — Deep Technical Dives, Part 3
Stored user-provided part 3 verbatim at `raw/articles/2026-09-20-reincarnation-deep-technical-dives-part3.md` and linked from `index.md` Plans section.
Covers TR-808 voice recipes, transport/sequencer scheduler, determinism, spec-complete table + build order. Spec now end-to-end.

## [2026-09-20] ingest | ReIncarnation — Work Breakdown Structure (M2.2 → M2.6)
Stored user-provided WBS verbatim at `raw/articles/2026-09-20-reincarnation-wbs-m2-2-to-m2-6.md` and linked from `index.md` Plans section.
Covers RIDevice framework pre-decision, modules 2.1–2.16 with TC gates, milestone acceptance table, open items. Execution contract for M2.2–M2.6.

## [2026-09-21] ingest | Codec-less guest: HUNK +8 payload offset, first green probe (rc=0)
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-21-codec-less-hunk-plus8-green-probe.md
Four crash causes closed with forward-motion proof (ReadConfig, sb128 movaps, Mix vectorization, probe r2-copy fix in `audio_io/probe_ahi.c` uncommitted); HUNK payload mod16=8 derived from Guru load address; terminal codec-less state (VOID accepts/never completes, CloseDevice hangs); open items listed. Linked from `index.md` Session findings section.

## [2026-09-21] ingest | Third-party consultant analysis (HUNK/SendIO/lddemon/P4)
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-21-consultant-hunk-sendio-lddemon-analysis.md
Stored consultant Q&A verbatim (SendIO gate decode, HUNK +8 mechanism, UND-stub doctrine, UAF doctrine, lddemon cache hypothesis + ZZTEST, P4 ladder, ranked fault tree). Two of its high-priority hypotheses (H1 request fields, H2 malformed r2) were subsequently confirmed by session evidence. Linked from `index.md` Session findings section.

## [2026-09-21] update | Codec-less record: CloseDevice hang closed (HookEntry stub)
- Disposition: Update (addendum to same-day record, old "Open" items 1-2 superseded in place with Status note)
- Raw: llm-wiki/raw/articles/2026-09-21-codec-less-hunk-plus8-green-probe.md
CloseDevice/VOID-completion hangs shared one root cause: hand HookEntry stub jumped to h_Entry (+0x10, itself — MinNode is 16 bytes) instead of h_SubEntry (+0x18); tight jmp loop, slave never answers kill. Fixed, verified live (M50c/M46/M43, TRUE-WaitIO err=0, CloseDevice done, rc=0 SUMMARY in 7714 ms). Remaining: linked-r2 natural completion semantics.

## [2026-09-21] ingest | P4.1/P4.2 green on VOID, dev_min_frames=64, audit clean
- Disposition: New (closes the linked-r2 open item from the two prior records; no contradiction — prior files list it as open/proposed)
- Raw: llm-wiki/raw/articles/2026-09-21-p41-p42-green-devmin-64.md
P4.1 independent pair err=0/err=0 TRUE-WaitIO; ladder 7/7 REPLIED err1=err2=0, dev_min=64, rc=0, zero IRQs; r2-pending was cold-start timing. aros_audit.sh 0 errors / 2 warnings. Cascade: none — prior records' open-item notes now read as resolved by this file (index entry states the closure).

## [2026-09-21] ingest | x86-64 HUNK alignment rule + shadow-build recipes
- Disposition: New (formalizes the mechanism from the HUNK record + consultant analysis into a rule; reproducible recipes new)
- Raw: llm-wiki/raw/articles/2026-09-21-hunk-rule-and-shadow-builds.md
Rule (no movaps/movdqa on HUNK data, flags, gate checklist), full clang/ld.lld recipes for device + 3 driver shadows with stub provenance, probe builds, deployment vectors, standing measurements, options A/B/C. Cascade: none.

## [2026-09-21] ingest | ZZTEST lddemon verdict + stock-bypass evidence audit
- Disposition: New (executes the proposed ZZTEST; revises the certainty of the older stock-bypass claim, no contradiction with measured facts)
- Raw: llm-wiki/raw/articles/2026-09-21-zztest-lddemon-verdict.md
Fresh-name load via DEVS: works (NOT-FOUND→FOUND 0x4a44b7a0, M20 on later opens); kickstart-resident refuted by package string scans; cache-on-fresh-boot impossible; original bypass observations flagged as contaminated (multi-boot serial, lost intermediate build). Cascade: none (prior files list ZZTEST as proposed; index entry states execution).

## [2026-09-21] lint | 0 issues found, 0 auto-fixed
Index↔raw consistency (all raws indexed, all raw/doc links resolve), metadata headers 3/3 on all 6 new files, no contradictions beyond the already-annotated ZZTEST revision. No fixes needed.

## [2026-09-21] ingest | VOID Stop verdict, HookEntry fix, TRUE-WaitIO green run
- Disposition: New (companion to the HUNK record; narrative overlaps its addendum, new material is the rebuild recipe + verdict values + green numbers)
- Raw: llm-wiki/raw/articles/2026-09-21-void-stop-hookentry-green-run.md
Covers M50 kill-sent-no-death verdict, MinNode-16 root cause, VOID rebuild recipe (version.h, gatestubs reuse, setcall_stub, UND gates), green run rc=0/7714 ms with TRUE-WaitIO err=0 and CloseDevice done. Cascade: none required — the session record's addendum already carries the Status note superseding its items 1-2; body lines preserved as mid-session narrative.

## [2026-09-21] ingest | ahi.library absent from v1 tree (P1–P3 no-go)
- Disposition: New (tree-wide absence proof + no-go rationale; absence itself was noted before)
- Raw: llm-wiki/raw/articles/2026-09-21-ahi-library-absent-no-go.md
No binary/source/conf/libinit anywhere; AHI subtree builds prefs programs only; port cost vs fake-timing value argue no-go. M1.1 stays worded from P4. Cascade: none.

## [2026-09-21] ingest | Device-as-library P1–P3 run (no-go premise superseded)
- Disposition: New + Update (no-go record annotated Status: Outdated in place, history preserved)
- Raw: llm-wiki/raw/articles/2026-09-21-device-as-library-p1-p3.md
- Updated: llm-wiki/raw/articles/2026-09-21-ahi-library-absent-no-go.md
Platform contract (AHI_NO_UNIT open → base from io_Device); probe fix; BestAudioID 0x1f0002, AllocAudio OK, ladder all rc=0, verify 0 ticks (honest zero); zero faults. Cascade: Status block on the no-go file; index entries for both updated.

## [2026-09-21] ingest | P3 playback started, 50,503,035 ticks in 5 s
- Disposition: New + Update (resolves the open item in the device-as-library record; annotated there)
- Raw: llm-wiki/raw/articles/2026-09-21-p3-playback-ticks-50m.md
- Updated: llm-wiki/raw/articles/2026-09-21-device-as-library-p1-p3.md
AHIC_Play TRUE → AHIsub_Start; 50,503,035 vs 3750 expected (≈13,467×, VOID unclocked); M1.1 low-level complete; zero faults. Cascade: Status note on the prior record; index entries updated.

## [2026-09-22] sound | First audible sound on the reference box (operator-confirmed)
- Disposition: New (milestone record)
- Raw: llm-wiki/raw/articles/2026-09-22-first-sound-dell-tone.md
- Updated: llm-wiki/index.md (entry)
Scratch probe_tone (440 Hz sine, v11 27344 B): 2× rc=0 nominal; operator heard ~10 s ring-like tone (ladder bursts, expected). Resolved in the follow-up entry above.

## [2026-09-22] ingest | Storm test mechanics recorded; coverage re-verified
- Disposition: Update (article gap fill) + ingest (verification)
- Updated: llm-wiki/raw/articles/2026-09-22-wbs21-storm.md (repeat loop, assert shape, slide-flag structure); llm-wiki/log.md (this entry)
- Verified: all 4 2.2 articles + 4 test files each indexed/logged exactly once; uncommitted set fully covered (rb303.h/c→click article; audit→phase entries).
- Live lanes: v1 9091 + laptop 9292 serves up; 4 serve procs total (other session's v1b/v1dh0 untouched).

## [2026-09-22] sound | First music on the reference box (operator: clean)
- Disposition: New (milestone record)
- Raw: llm-wiki/raw/articles/2026-09-22-first-music-dell-clean.md
- Updated: llm-wiki/index.md (entry)
Scratch dell_player + first-light 3 loops; mono/stereo bug (found by ear, fixed 1 line, replay clean); file exonerated by host analysis. Uncommitted.

## [2026-09-22] sound | Volume resolved: host + AHI prefs stages, no driver work
- Disposition: Update (closes the volume question on the first-sound record)
- Updated: llm-wiki/raw/articles/2026-09-22-first-sound-dell-tone.md (Resolution)
`SYS:Prefs/AHI` confirmed (has output volume, maxed; OCR can't read Topaz, operator reads it); laptop Fn volume maxed; replay definitely audible, rc=0 nominal. Two stages behaved — codec amp exonerated. AHI prefs window closed (operator).

## [2026-09-22] ingest | Docs-accuracy pass committed (9af4966) + live lane state + known wart
- Disposition: Update (two stalenesses fixed) + ingest (commit record) + ops note
- Updated: docs/evidence/formats/m1-1-report.md (backend cell INPUT→DECIDED low-level, Task 6 code + record cited); llm-wiki/raw/articles/2026-09-22-dell-reference-box-iteration-abiv1-target.md (confirm-on-ABIv1 refined: driverless QEMU cannot confirm driver timing; verdicts stand on Dell characterization where QEMU is blind, ABIv1-confirm where observable)
- Committed 9af4966 (with the M2.1 snapshot record) [gate:G14], pushed.
- Live lane state (volatile, for next session): v1 spike serve up on 9091 (home-fs spool, anon slot); QEMU aros_v1 idle at Workbench with the green shadow set deployed in RAM: (ahi.device.fixed + patched sb128 + void shadow + probes — reboot wipes); Dell agent idle (last session 750); /tmp under quota pressure from a concurrent session (spool symlink fix holds; keep --get destinations off /tmp).
- Known wart (reported, unordered): ca755fb message trailer says [gate:G6] but the content is WBS 2.1 (TCs, no G-gate). Fixing means amending pushed main — needs explicit order.

## [2026-09-22] green | ABIv1 full green via 1-byte HookEntry patch; Appendix A reproduced end-to-end
- Disposition: New (closes the Stop-blocker; supersedes the scheduled void rebuild)
- Raw: llm-wiki/raw/articles/2026-09-22-abiv1-full-green-one-byte-hookentry-patch.md
- Updated: llm-wiki/raw/articles/2026-09-22-abiv1-acceptance-lane-p1p2-green-stop-blocked.md (Disposition superseded-note); llm-wiki/index.md (entry)
Stop kill-no-death was the ahi.device shadow's hand HookEntry stub (`jmp *0x10(%rdi)`, 4 install sites), never void (no such jump in either build; trampoline correctly targets Slave). One byte patched (0x5ee5: 0x10→0x18, no relocs), disassembly = documented fixed form. Session-9 PASS 12088 ms: Appendix A line-for-line (50.6M ticks, err1=0/err2=-2, dev_min=0). ABIv1 acceptance evidence COMPLETE; G5 closure (spec row + commit) separate, untaken. Scratch binary only (`/home/miller/Work/ri_build/ahi.device.fixed`); source-faithful clib_stubs rebuild remains the durable path.

## [2026-09-22] lane | ABIv1 acceptance lane proven P1/P2 (Appendix-A match); full green Stop-blocked, rebuild scheduled
- Disposition: New (work unit close-out with scheduled follow-up)
- Raw: llm-wiki/raw/articles/2026-09-22-abiv11-acceptance-lane-p1p2-green-stop-blocked.md
- Updated: llm-wiki/index.md (entry)
v1 recipe byte-identical to Sep21 green (29840 B, symtab shape, task.resource=1, r12=0, UND=1); session-8 PASS P1/P2 = Appendix A line-for-line; repo probe untouched (variants in /home/miller/Work/ri_build/). Chain: setcall-noop startup death → -llibinit real; sb128-stock returns post-reboot → redeploy; ReadConfig-stock wins without PROGDIR open → progdir_probe; Stop kill-no-death in BOTH voids → post-fix void rebuild scheduled. v1 serve 9091/home-spool; v1c untouched. Cascade: none (Vulkan4Aros cross-post needs no change — no lane-infra delta beyond recorded spool move).

## [2026-09-22] m2 | TC-2.2.6 two-instance independence (TDD property pin)
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs22-303indep.md
- Updated: tests/unit/t22_303indep.c, scripts/ri_audit.sh (303 phase), llm-wiki/index.md
Source audit (no shared state) + A1==A2 across B's activity (crosstalk -inf); energy guards (B-slide silence found); clobber-mutant caught; 2 transient quota-race audit FAILs before green. Uncommitted.

## [2026-09-22] m2 | TC-2.2.3 slide legato (TDD property pin)
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs22-303slide.md
- Updated: tests/unit/t22_303slide.c, scripts/ri_audit.sh (303 phase), llm-wiki/index.md
Env continuity + slew + 5τ reach, both entry forms; scratch-mutant caught exactly (frozen file untouched); audit 0/0. Uncommitted.

## [2026-09-22] m2 | TC-2.2.5 waveform-click flag (feature, TDD)
- Disposition: New (DSP change + test + audit wiring)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs22-303click.md
- Updated: engine/dsp/rb303.h, engine/dsp/rb303.c, tests/unit/t21_303click.c, scripts/ri_audit.sh (303 phase), llm-wiki/index.md
RED (missing field) → classic/smooth bounds → mutant-caught → GREEN + audit 0/0 (goldens byte-identical). Env flakes: 2 quota AROS-phase FAILs + 1 CWD-misfire, all verified transient. Uncommitted.

## [2026-09-22] m2 | TC-2.2.2 accent envelope (TDD property pin)
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs22-303accent.md
- Updated: tests/unit/t22_303accent.c, scripts/ri_audit.sh (303 phase), llm-wiki/index.md
Peak 1.0 + monotone decay + 1/e window; scratch-mutant caught; audit 0/0. Uncommitted.

## [2026-09-22] ear | Slide feel: 40 ms provisional-good, A/B stays open
- Disposition: Update (operator judgment recorded; no code change)
- Updated: llm-wiki/raw/articles/2026-09-22-wbs22-303slide.md (Standing)
Glide feel hard to judge by ear alone → keep P-03 40 ms default, marked good for now. Blind 40-vs-60 A/B (spec OPEN-01) remains OPEN. Also answered "what should slide sound like" from the dive + spec gate table (glide/no-dip/no-gap; defect checklist: zipper, dip, click, wrong tau).

## [2026-09-22] m2 | TC-2.2.1 Dell run GREEN (reference hardware)
- Disposition: Update (hardware proof for the filter pin)
- Updated: llm-wiki/raw/articles/2026-09-22-wbs22-303filter.md (Standing)
Cross-build ABIv11 (32640 B); session 750 PASS rc=0 in 2263 ms, agent alive. All 20 freqs ±1 dB on hardware; AROS libm proven correct by the comparison itself. Scratch: /home/miller/Work/ri_build/aros_storm/.

## [2026-09-22] m2 | TC-2.2.1 filter response (TDD property pin)
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs22-303filter.md
- Updated: tests/unit/t22_303filter.c, scripts/ri_audit.sh (303 phase), llm-wiki/index.md
Independent float64 reference (Appendix B form); 20/20 within ±1 dB (34x margin); mutant-caught; audit 0/0. Uncommitted.

## [2026-09-22] measure | Pitch question nailed: 440 Hz on the tuner, rate handling correct
- Disposition: Update (closes the 48k-into-44100 pitch question with measurement)
- Updated: llm-wiki/log.md (this entry)
Continuous 440 Hz tone (10 s, 0.9 FS, scratch `tone_cont`) → phone chromatic tuner reads **440 Hz**. The 8.2%-flat hypothesis (48k data at 44.1k clock → 404 Hz) is REFUTED: the driver delivers correct pitch (resamples or runs 48k clock — mechanism unprobed, outcome measured). Earlier guitar-mode "E string" reading explained (440 > top string E4; wrong tuner mode, not evidence). No resampling work needed. Scratch: `/home/miller/Work/ri_build/tone_cont.c`, guest `RAM:tone_cont`.

## [2026-09-22] sound | Post-reboot replay audible, quality acceptable
- Disposition: Update (re-replays after laptop reboot)
- Updated: llm-wiki/log.md (this entry)
Fresh boot (agent session 752): re-put tone + player + WAV (RAM: wiped as expected); tone SUMMARY nominal, 3 music loops rc=0, agent alive. Operator: tone audible, quality "not amazing, but acceptable" — consistent with known benign factors (48k content into 44100 mode, laptop speakers, ladder-burst shape). No new defect indicated.

## [2026-09-22] ingest | Coverage re-verified (filter/Dell/ABIv1); lanes alive
- Disposition: Update (verification-only ingest; no new findings)
- Updated: llm-wiki/log.md (this entry)
- Verified: filter article + Dell Standing + ABIv1 section all indexed/logged; every uncommitted file (rb303 filter test, audit wiring, storm updates, index/log) traced to a record; all index links resolve.
- Live lanes: 4 serves up (v1 9091 + laptop 9292 + 2 other-session); QEMU aros_v1 + Dell agent last seen alive (sessions 9 / 750).

## [2026-09-22] m2 | TC-2.1.4 ABIv1 storm run + ftrapv root cause (all lanes reconcile)
- Disposition: Update (acceptance-lane number + 141-vs-304 curiosity resolved)
- Updated: llm-wiki/raw/articles/2026-09-22-wbs21-storm.md (ABIv1 section, RESOLVED note)
Canonical v1 recipe (+ trailing -lstdcio re-scan) → 66064 B, r12=0; QEMU aros_v1 (identity verified) ratio 0.2100 PASS. Full table: host-trapv 0.46, host-plain/QEMU 0.21, Dell 0.90 — ftrapv 2.2x tax × ~4.3x Zen5→SandyBridge gap explains all; no pathology. Scratch: ri_build v1_* objects + storm_v1.

## [2026-09-22] m2 | TC-2.1.3 PCM half: swap transparency (TDD)
- Disposition: New (test extension; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-swappendix.md
- Updated: tests/unit/t21_swaprender.c, llm-wiki/index.md
Identity + delivery + bounded/finite + determinism + same-song transparency; step-metric and retrigger-identity hypotheses refuted with evidence (saw wraps, filter tails); mutant-caught; audit 0/0. Uncommitted.

## [2026-09-22] m2 | WBS 2.7 negotiate slice: engine AHI open on hardware
- Disposition: New (unit + audit wiring + Dell run)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs27-negotiate.md
- Updated: audio_io/audio_ahi.h, audio_io/audio_ahi.c, scripts/ri_audit.sh (backend AROS-compile line), llm-wiki/index.md
Negotiate-only, hookless (proven on Dell: M1.1 numbers, rc=0); v1-header friction fixed; playback + close next. Uncommitted.

## [2026-09-23] lint | 2.3 ingest verified, 0 issues found, 0 auto-fixed
Index↔raw consistency (all local raws indexed, all local links resolve), metadata headers 3/3 on the new article, no contradictions (the sole "400-base" hit is the new log entry's own pre-lock narrative). One dead-link candidate investigated and cleared: `2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md` is an intentional cross-repo reference (index marks it "not copied here") and the file verifies present in the Vulkan4AROS wiki. No cascade updates (WBS + deep-dives are verbatim user sources, immutable; spec v5 untouched by a slice ingest). SDD progress BASE advanced to `f5e6914` (local, gitignored).

## [2026-09-23] gate | sox external-validation gate live over all 7 export goldens
- Disposition: Update (closes half of the export.md external-validation OPEN)
- Updated: scripts/ri_audit.sh (Phase 1 sox loop: ch/rate/bits/samples on math-dc/dc-24/dc-441/first-light/first-light-441/dc-aiff/first-light-aiff, warn-and-skip without sox), docs/evidence/formats/export.md (validation section)
- sox-ng 14.8.0.1 installed on the host lane (`pacman -S sox`; no apt on Omarchy). All 7 goldens read exact. Audition import remains device-side.
- The new gate proved load-bearing on its first run: my loop appended `.wav` twice (`math-dc.wav.wav`) and the gate failed honestly before the fix. Full `ri_audit.sh` 0/0 after (0 FAIL lines).

## [2026-09-23] lint | m5–m14 ingest verified, 0 issues found, 0 auto-fixed
Index↔raw consistency (all local raws indexed, all local links resolve), m5–m14 log entries present, metadata headers 3/3 on the 4 newest raws. One dead-link candidate investigated and cleared: `2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md` is an intentional cross-repo reference (index marks it "not copied here") and the file verifies present in the Vulkan4AROS wiki. No new material to compile (HEAD `448b7c6` fully ingested, tree clean).

## [2026-09-23] update | USB stick errorcode 42 (HFERR_Phase) → clean boot after re-seat + backup posture
- Disposition: Update (Status block on the Dell GUI first-light record; no new raw)
- Updated: llm-wiki/raw/articles/2026-09-23-dell-gui-first-light-classic-window.md, llm-wiki/log.md (this entry)
- SFS errorcode 42 = `HFERR_Phase` (`devices/scsidisk.h:42`) on USB mass-storage read at offset 49152, during boot self-access after a power-button reboot. Recovery: reboot + physical re-seat → all volumes mounted, 0 errors. Points at connection/controller state, not media.
- Backup: full image + mbr hash-verified intact; ENVARC: delta (7 files) pulled fresh to `/home/miller/Work/stickwork/envbak-2026-09-23/`. Rules: never hot-unplug the boot stick; re-image is last resort.

## [2026-09-24] m41 | Live chase: timer beat clock + playhead + self-measured lag
- Disposition: New (first 2.10-full device slice)
- Raw: llm-wiki/raw/articles/2026-09-24-live-chase-timer-lag.md
- Updated: gui/widgets/rstp.{h,mcc.c} (chase attr + highlight), app/stepproof.c (beat clock + lag), llm-wiki/index.md
- No new pure logic (chase math pinned); timer Base trick; signal-seed loop.
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Pending: owner eyeball (advancing playhead) + lag number on a fresh instance.

## [2026-09-24] m40 | First-click jump: warp arms past 3px + owner Q&A
- Disposition: New (one-variable fix; two questions answered from code)
- Raw: llm-wiki/raw/articles/2026-09-24-first-click-jump-qa.md
- Updated: gui/widgets/rknb.mcc.c (arming), llm-wiki/index.md
- Jump = press-tremor yank; armed threshold keeps press 1:1. Buttons-gone = by design (bevel boxes retired). FLAMRES = our flam slot, placeholder today (documented in params.c), label stays honestly.
- Full `ri_audit.sh` 0/0. Binary on RAM:. Pending: owner first-click check on a fresh instance.

## [2026-09-24] m43 | Chase beat runs: manual-Wait + MICROHZ measured, VBLANK rejected

## [2026-09-24] m44 | §12.1: kernel totality + 808 voice rest + linear sum
- Disposition: New (first improvement-review slice; TDD, 4 new tests RED-first)
- Raw: llm-wiki/raw/articles/2026-09-24-imp-12-1-kernel-totality-808-rest.md
- Updated: engine/dsp/kernels.{c,h} (total: exp 0/+Inf, sin int64, pow2 ±127/128),
  engine/dsp/rb808.{c,h} (rest at −100 dBFS, linear section sum),
  tests/property/t3_kernels_total.c, tests/unit/t33_808_silence.c,
  t34_808_storm_linear.c, t35_808_deactivate.c (RED 731/253/1246/395 → PASS),
  6× 808 goldens re-baselined with per-diff root causes + ledger notes,
  spec §20/OPEN-10/parity rows (D-a–D-j adopted), docs/2026-09-24-improvement-todo.md
- Suite caught a real 2.4× storm-CPU regression from the widened scale loop; fixed
  (32-iteration common path kept). 303 first-light re-renders byte-identical.
- Full `ri_audit.sh` 0/0. Held with justification: multiplicative envelopes,
  integer voice time (both subsumed by the rest bound); 909 clip → §12.6.
- Disposition: New (m41 pending closed with numbers; vehicle loop-shape change)
- Raw: llm-wiki/raw/articles/2026-09-24-chase-beat-manual-wait-microhz.md
- Updated: app/stepproof.c (manual Wait + Break exit, early MICROHZ setup, grid off-by-one fix, diag stripped), llm-wiki/index.md
- diag13 matrix (bare/+libs/+window/+MUI = rc 1/11/21/31): device delivers everywhere — NewInput never surfaces seeded bits on Zune (blind instrument, struck all NewInput-observed "stall" verdicts incl. the session-13 microhz hypothesis).
- Measured QEMU file-log, 255 fires: mean 86554 µs vs 86207 requested (+0.4%), jitter ±0.1 ms, drift +347 µs/fire. VBLANK same loop fires at 100.18 ms (blank quantization) — rejected for tempo.
- Full `ri_audit.sh` 0/0. Final binary QSTEPF opens + Break-exits clean. Glow-motion pixel proof deferred (capture slots unstable; same-SetAttrs FIRES repaints live).
- Pending: owner Dell eyeball (advancing glow) + LAG/FIRES numbers on a fresh instance.

## [2026-09-24] m41b + label-proof | Belated verdict pointers (no new code)
- m41b: commit `3175071` (feat: `io_Flags` hygiene on timer rearm + warp DoIO, plus the
  m41-article forensics section) shipped without its own log line — recorded here
  belatedly. Verdict arrived in m43: hygiene was innocent-but-insufficient; the loop
  was blind (NewInput), not dirty. See the Resolution section in
  `2026-09-24-live-chase-timer-lag.md` and the m43 record.
- Label proof: commit `8d53322` flipped the m39 proof section (agent census
  1924/2040 px light bucket, zero grey seam hits) with no log line — recorded here.
  Owner spelling-crispness eyeball still open.

## [2026-09-24] m42 | Lane incident: wedged jug v1dh0 with a capture, restored it- Myui-capture on their e1000 lane wedged it identically (ping dead, 0 bytes) — 5th incident, 3rd lane/NIC. Reconstructed their exact QEMU launch from recorded `ps`, relaunched, agent back (session 12), desktop verified.
- Lessons: big captures wedge ANY lane (Dell Intel, QEMU e1000, QEMU rtl8139 eventually) — transfer size, not driver; never test-capture on a shared lane again (private riqemu1 only); their disk untouched (RAM:-only artifacts, died with the process).
- Apology owed and given; no data loss (reboot-equivalent state).

## [2026-09-24] m39c | Wedge-fix correction: chunking unproven (session-13 wedge)
- Session 13 wedged on the chunked agent, identical partial-then-reset signature (299384/786521). "Verified" struck in both lane articles; chunking kept as harmless mitigation.
- Standing pattern: intermittent ~5–10% per 786 KB frame; small/chunked traffic never wedges. Next wedge: redial-first (untested) before reboot.

## [2026-09-24] m39b | Spike lane wedge fixed: chunked sends, hammer 12/12
- Disposition: New (cross-repo fix; proof by hammer)
- Raw: llm-wiki/raw/articles/2026-09-24-spike-lane-wedge-fix.md
- Root cause: single ~786KB Send() stalls AROSTCP (3 identical server-log signatures; first-frame-post-reboot kills cumulative theory; chunked puts never wedge).
- Fix in Vulkan4Aros `arostcp_shared.h` (guest-only, committed `67eeca3c`, pushed): 32KB `Send()` cap, byte-identical stream.
- Agent rebuilt v11, backup-first deploy (BAK kept), reboot, hammer 12/12 PASS session-stable.
- Aside: `Version <ELF>` kills agent 3/3 (BAK identical — not the new build); avoid, self-restores.

## [2026-09-24] m39 | Custom label class: MUIC_Text inverts, own pixels instead
- Disposition: New (forensics + minimal custom class; device proof pending)
- Raw: llm-wiki/raw/articles/2026-09-24-custom-label-class.md
- Updated: gui/widgets/rlbl.{h,mcc.c} (new), app/panel909.c, scripts/ri_audit.sh (gate rlbl), llm-wiki/index.md
- m31-clean vs names-black isolates the row; undo row light exonerates contents. Owned fill + pen-1 text.
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Pending: owner eyeball (dark-on-light, no bevels) on a fresh instance.

## [2026-09-24] m38 | Proof close-out: separator resolved, custom steps approved
- Disposition: New (verdict capsule; no code)
- Raw: llm-wiki/raw/articles/2026-09-24-proof-closeout-seam-steps.md
- Updated: m36 + m37 articles (proof sections flipped), llm-wiki/index.md
- Separator absent fresh (census clean) → transient artifact, refresh moot. Steps: visual + doubling clicks approved.
- 2nd capture wedge = agent flake (no guru, healthy apps); reboot recovered (session 5).

## [2026-09-24] m37 | Custom RStp class: stock can't size, build our own
- Disposition: New (m36 FixWidth refuted on device → custom class)
- Raw: llm-wiki/raw/articles/2026-09-24-custom-rstp-class.md
- Updated: gui/widgets/rstp.mcc.c (rewrite), gui/widgets/rstp.h (new), app/stepproof.c, scripts/ri_audit.sh (gate rstp.h), m36 article (struck correction), llm-wiki/index.md
- 32px AskMinMax, own on/off art, armed click-toggle; m21/m22 lessons in from the start.
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Pending: owner click sequence (0→1→9→8) on a fresh instance. Next: beat clock + chase highlight.

## [2026-09-24] m36 | Separator seam + step hit-rate: repaint-all + 32px targets
- Disposition: New (forensics + two minimal fixes; device proof pending)
- Raw: llm-wiki/raw/articles/2026-09-24-seam-repaint-step-targets.md
- Updated: app/panel909.c (repaint-all on text change), app/stepproof.c (32px targets), llm-wiki/index.md
- **Bar:** Intuition proofs clean → MUI-side paint; layout uniform → stale-pixel seam at the 80px abutment. Refresh-test discriminates (owner).
- **Steps:** 14px targets → misses; 32px via shell.
- Full `ri_audit.sh` 0/0. Both binaries on RAM:. Pending: owner refresh-test + click sequence on fresh instances.

## [2026-09-24] m35 | 2.10 step-toggle proof: 16 buttons + pattern readout
- Disposition: New (first 2.10 device slice; chase timing stays host-pinned)
- Raw: llm-wiki/raw/articles/2026-09-24-step-toggle-proof-16buttons.md
- Updated: app/stepproof.c (new), scripts/ri_audit.sh (gate it), llm-wiki/index.md
- No new pure logic (aggregation inline, toggle pinned) → no new host test.
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Pending: owner click-toggle check (0→1→9→8) on a fresh instance. Next: beat clock for live chase.

## [2026-09-24] m34 | Commit-on-release observable: per-knob undo counter
- Disposition: New (TDD helper; MCC attr + app wiring; device proof by count)
- Raw: llm-wiki/raw/articles/2026-09-24-commit-observable-undo-counter.md
- Updated: gui/panels.[ch] (`ri_ctl_format_count`), tests/unit/t29_paneldefault.c (count pins), gui/widgets/rknb.h (new) + rknb.mcc.c (commits attr), app/panel909.c (UNDO row + notifies), scripts/ri_audit.sh (gate rknb.h), llm-wiki/index.md
- Two file damages self-caught by readback (signature eat + dropped NULL guard), repaired pre-build.
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Pending: owner count check (click→1, drags accumulate) on a fresh instance.

## [2026-09-24] m33 | Knob name labels in the MUI app (hardware silkscreen parity)
- Disposition: New (no new logic; device proof by eyeball)
- Raw: llm-wiki/raw/articles/2026-09-24-knob-name-labels-mui.md
- Updated: app/panel909.c (name row), llm-wiki/index.md
- Names/knobs/values rows = hardware row order. AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Pending: owner eyeball (spelling + alignment) on a fresh instance.

## [2026-09-24] m32 | Owner numeric approval: full range + readouts verified
- Disposition: New (verdict capsule; right-click + fine boxes checked)
- Raw: llm-wiki/raw/articles/2026-09-24-owner-numeric-approval.md
- Updated: docs/evidence/gui/acceptance.md, llm-wiki/index.md
- **Approved by number:** 0–127 all knobs, live tracking, LEVEL→100, fine steps, overall fine.
- **Still open:** commit-on-release, fader travel.

## [2026-09-24] m31 | Notify wiring + value readouts: knob values go somewhere
- Disposition: New (RED→GREEN pins; first app listeners; device proof by number)
- Raw: llm-wiki/raw/articles/2026-09-24-notify-readouts-first-wiring.md
- Updated: gui/panels.[ch] (`ri_ctl_format_value`), tests/unit/t29_paneldefault.c (format pins), app/panel909.c (text row + notify + loop refresh), llm-wiki/index.md
- AROS -Werror clean. Full `ri_audit.sh` 0/0. Binary on RAM:.
- Stale-link rule (3rd strike): rebuild ALL lane objects on new symbols, both lanes.
- Pending: owner numeric confirmation (drag tracks, LEVEL→100) on a fresh instance.

## [2026-09-24] m30 | Owner device approval: drag arc m25–m29 closed
- Disposition: New (verdict capsule; flips pendings to approved where earned)
- Raw: llm-wiki/raw/articles/2026-09-24-owner-approval-drag-arc.md
- Updated: docs/evidence/gui/acceptance.md (either-axis + reversal boxes checked), llm-wiki/index.md
- **Approved:** horizontal, vertical-to-max, sensitivity, in-press reversal, click safety.
- **Still open (do not claim):** shift-fine/right-click/commit/fader on device; divider identity (frame-follows unanswered); grey a/b/c unanswered.

## [2026-09-24] lint | index↔raw consistency re-checked, 0 issues (1 candidate cleared: cross-repo AHI reference, intentional)
- Dead-link scan: sole hit `2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md` is the known intentional cross-repo pointer (cleared 2× before, file lives in Vulkan4AROS wiki). No action.

## [2026-09-24] m29 | Accumulator clamp: reversal bites at once (TDD), device test pending
- Disposition: New (host-green + deployed; device proof PENDING owner re-test)
- Raw: llm-wiki/raw/articles/2026-09-24-accumulator-clamp-reversal.md
- Updated: gui/knob_logic.[ch] (`ri_knob_clamp_acc`), gui/widgets/rknb.mcc.c (double acc + per-move clamp), tests/unit/t1_knob.c (2b pins), docs/autodoc/gui.doc + audit check_sig, llm-wiki/index.md
- **Root cause:** pinned pointer → unbounded overshoot past clamp → reversal unwinds dead zone; fresh click re-anchors (why it always worked).
- RED → GREEN (PASS knob). Stale-object trap both lanes (host `all`-first; AROS relink deps).
- Full `ri_audit.sh` 0/0. Binary on RAM:. Pending: owner in-press reversal test on a fresh instance.

## [2026-09-24] m28 | Warp-target basis fix: outer- vs content-relative coords, device test pending
- Disposition: New (single-variable fix; device proof PENDING owner re-test)
- Raw: llm-wiki/raw/articles/2026-09-24-warp-target-basis-fix.md
- Updated: gui/widgets/rknb.mcc.c (drop Border terms + trap comment), llm-wiki/index.md
- **Root cause:** warp target double-counted borders (mouse = outer-relative, content = border-adjusted); per-event (10,25) bias → runaway values + dead reversal. Both v2 symptoms, one cause.
- Full `ri_audit.sh` 0/0. Fixed binary on RAM:. Pending: owner sensitivity + back-and-forth test on a fresh instance.

## [2026-09-24] m27 | Grab revision: edge-trigger yanked, warp-every-move, device test pending
- Disposition: New (revises unproven m26 design; m26 article stands as trail)
- Raw: llm-wiki/raw/articles/2026-09-24-grab-revision-warp-every-move.md
- Updated: gui/widgets/rknb.mcc.c (warp policy only), llm-wiki/index.md
- **Evidence:** diag5 0xEF + diag6 raw (borders 10/25 sane) → edge warp fired only on huge drags = giant teleports; horizontal harmed without need, vertical helped at a fight-price.
- Warp-after-every-move: pointer hovers, motion accrues 1:1, edges unreachable, no teleports. Fail-soft kept.
- Full `ri_audit.sh` 0/0. v2 binary on RAM:. Pending: owner feel test on a fresh instance.

## [2026-09-24] m26 | Pointer grab: edge-clamp diagnosed, warp-back implemented, device test pending
- Disposition: New (host-green + deployed; device proof PENDING owner re-test)
- Raw: llm-wiki/raw/articles/2026-09-24-pointer-grab-warp.md
- Updated: gui/widgets/rknb.mcc.c (accumulator + input.device warp), llm-wiki/index.md
- **Root cause:** absolute-position math + screen edge at 66 px vs 75 px needed up (geometry, no run needed); spec M2.1 already mandated grab — was never implemented.
- Warp = `IECLASS_NEWPOINTERPOS`/`IND_ADDEVENT` (same call the spike agent uses); 8 px margin; fail-soft NULL; no warp loop.
- Full `ri_audit.sh` 0/0. Binary on RAM:. Pending: owner up-drag-to-max test on a fresh instance (close both stacked windows first).

## [2026-09-24] m25 | Both-axes knob drag: owner amendment, TDD, device test pending
- Disposition: New (host-green + deployed; device proof PENDING owner re-test)
- Raw: llm-wiki/raw/articles/2026-09-24-both-axes-drag-amendment.md
- Updated: gui/knob_logic.[ch] (dx+dy eff), gui/widgets/rknb.mcc.c (start-x), tests/unit/t1_knob.c (8 migrated + 5 new pins), spec M2.1 (amended), acceptance (reworded, unchecked), llm-wiki/index.md
- **Decision:** vertical (ReBirth manual + spec lock) vs horizontal (owner feel) → BOTH, owner chose over horizontal-only/keep-vertical.
- RED (new pins vs old fn) → GREEN (PASS knob) after `all` (stale-object trap: lone `test` links cached objects — 9 phantom FAILs).
- Full `ri_audit.sh` 0/0. Binary on RAM:. Pending: owner horizontal + vertical regression test.

## [2026-09-24] m24 | Wiki doc-sync: compact lock struck, acceptance earns MCC boxes (Update, no code)
- Disposition: Update (cascade corrections after m19/m22 moved the code)
- Raw: llm-wiki/raw/articles/2026-09-24-doc-sync-compact-strike-acceptance.md
- Updated: docs/evidence/gui/panel-909-geometry.md (compact narrative struck visibly → v2), docs/evidence/gui/acceptance.md (centers box → v2 + m22; new checked boxes: art render, click safety; drag/fine/commit/right-click stay unchecked), llm-wiki/index.md
- Left alone deliberately: t29_knobart header (historically accurate), log history (immutable), ref909 images (measured in m19/m22).
- Audit's acceptance-content gates (P-18, TC-2.9/2.10/2.11, ReBirth-101, Tester, boxes) all preserved.

## [2026-09-24] m23 | Third-party open alternatives: verification + license boundaries (reference, no code)
- Disposition: Reference (consultant brief checked against the web)
- Raw: llm-wiki/raw/articles/2026-09-24-third-party-open-alternatives-assessment.md
- Updated: llm-wiki/index.md
- **Verified:** JC-303 (`midilab/jc303`, mixed GPL shell / MIT DSP core — touch separately), RP-8 (`luchak.itch.io/rp8`, workflow-only reference).
- **Not found as described:** RE:BORN 338 (no repo/demo surfaced), consultant's "jsynth" (.rbs player claim matches nothing; only a generic web synth exists).
- **Assessment:** Open303 analysis = legitimate study/ear reference, GPL stays out of tree; RP-8 song automation supports M2.6 scope; .rbs import NOT in scope; 2017 Roland takedown reinforces clean-room rule (unchanged).
- No repo files touched.

## [2026-09-24] m22 | MCC knobs live: _win crash fixed, ungated Draw proven, four knobs + click test on device
- Disposition: Resolved (m21's crash + wedge closed)
- Raw: llm-wiki/raw/articles/2026-09-24-mcc-knob-crash-bisect-lane-wedge.md (Resolution section)
- Updated: gui/widgets/rknb.mcc.c (_win target, unconditional Draw), llm-wiki/index.md
- **Root cause:** `MUIM_Window_Add/RemEventHandler` sent to `_window` (Intuition Window*) instead of `_win` (MUI window object) — decoded from the on-site guru disassembly (`obj+0x38` renderinfo, `+0x20` mri_Window, inline dispatch through garbage). One-letter macro, full type confusion.
- **Second defect:** initial show-time Draw carries no MADF_DRAWOBJECT (gated red blank, ungated green painted) — production Draw paints unconditionally (idempotent full frame).
- **Measured:** diag2 open/live/close clean, no guru; panel909 four olive knobs, orange pointers up, tick rings, pitch-80, x 54/134/214/294; click down/up no crash, pointers steady.
- **Honest remainder:** real drag needs hold-and-drag primitive or human; no app notify listeners yet.
- Full `ri_audit.sh` 0/0 (0 FAIL lines). Detached-only throughout.

## [2026-09-24] m21 | MCC knob class: custom Numeric subclass, device crash, bisection, lane wedge (UNPROVEN)
- Disposition: New (code-complete, device proof PENDING — window-open crash; Dell lane wedged, needs on-site recovery)
- Raw: llm-wiki/raw/articles/2026-09-24-mcc-knob-crash-bisect-lane-wedge.md
- Updated: gui/widgets/rknb.mcc.c (custom class), app/panel909.c (class wiring), gui/knob_blit.h (stdint), llm-wiki/index.md
- **Measured:** AROS -Werror compile clean; on-device class+knob create/dispose rc=0 (creation INNOCENT); crash needs window open (Show/Draw path); guru names WHd_panel909.
- **Doctrine:** NEVER foreground-run any Dell binary — always detached; modal requesters unreachable remotely (click/RETURN/ESC all zero effect); USR1-drop rejected.
- **Blind hardening:** Draw reads cached `cur`, no dispatcher reentry.
- Full `ri_audit.sh` 0/0 (0 FAIL lines). Recovery runbook in article (detached A/B/C matrix).
- Scratch kept: `/home/miller/Work/ri_build/diagrknb*.c`, dell2 binaries/captures/winlists.

## [2026-09-24] m20 | Panel composition on device: bg, rules, labels, knobs (eyeballed + measured)
- Disposition: New (composition helper + proof vehicle + constants + pins)
- Raw: llm-wiki/raw/articles/2026-09-24-panel-composition-device-proof.md
- Updated: gui/knob_blit.h (new decls), gui/knob_blit.c (rect helper), gui/panels.h (W/H/BG constants), app/knobproof.c (composition), tests/unit/t29_layout.c (constant pins), llm-wiki/index.md
- **Measured:** bg exact `#dcdcd6`; knob x 0px error; labels legible; rules code-proven, sub-pixel at capture scale (stated).
- **Self-caught:** header edit deleted rect API + corrupted return type — repaired pre-build, verified by diff.
- Full `ri_audit.sh` 0/0 frozen tree (0 FAIL lines). Remaining: MCC integration, typography, 2.10 step GUI.

## [2026-09-23] m19 | Knob v2 art lock: 70px pitch from hardware ratio, pointer-tip geometry, exact orange on device
- Disposition: New (geometry re-lock + device proof; art internals already pinned in m17)
- Raw: llm-wiki/raw/articles/2026-09-23-knob-v2-pitch70-device-lock.md
- Updated: tests/unit/t29_layout.c (v2 asserts), gui/panels.c (v2 table), app/knobproof.c (v2 geometry + window), docs/evidence/gui/panel-909-geometry.md (v2 lock, history struck visibly), llm-wiki/index.md
- **Measured:** hardware pitch ratio 1.35 → pitch 70; x exact, y via pointer tips (tick-gap bias diagnosed); angles correct incl. 12°=77.6° convention check; `#e37c3b` byte-exact on device; tick rings visible on all four.
- Full `ri_audit.sh` 0/0 frozen tree (0 FAIL lines). Remaining: panel composition, MCC integration.

## [2026-09-23] m18 | Knob blit on device: BGRA root cause via calibration, orange pointers proven
- Disposition: New (blit helper + proof vehicle + gates; closes the "go" deliverable)
- Raw: llm-wiki/raw/articles/2026-09-23-knob-blit-device-bgra-proof.md
- Updated: gui/knob_blit.c (new), app/knobproof.c (new), scripts/ri_audit.sh (guard + leak + compile lines), llm-wiki/index.md
- **Debugging textbook:** lavender ghosts → SDK .conf + ICD pattern study → 6-swatch calibration hypothesis test → BGRA words proven 6/6 (incl. exact 50%-blend 149) → one-line fix → eyeball + byte-exact `#e07b2e` (32px) + charcoal bodies at doc centers ±1px.
- Full `ri_audit.sh` 0/0 frozen tree (0 FAIL lines). Remaining art: panel composition around knobs, then MCC integration retiring MUIC_Knob.

## [2026-09-23] m17 | Knob art recipe: pointer-angle contract + procedural frames + C port (TDD, eyeballed)
- Disposition: New (art recipe + pins; AROS blit + device proof next)
- Raw: llm-wiki/raw/articles/2026-09-23-knob-art-recipe-frames.md
- Updated: gui/knob_logic.c/.h (mdeg fn), gui/knob_art.h/.c (new: table + renderer), scripts/ri_build_host.sh (MOD_gui), scripts/ri_audit.sh (AROS + Phase 12 lines), tests/unit/t29_knobart.c (new), llm-wiki/index.md
- **TDD:** RED watched; GREEN all GUI tests. Full `ri_audit.sh` 0/0 frozen tree (0 FAIL lines).
- **Eyeball:** TR-909 photo + ReBirth screenshot fetched/viewed (design language confirmed; McGill fetch failed transport, unneeded). PIL v1→v2 iterated visually; C port 87% identical (divergences documented).
- **Honesty:** pointer-low pin corrected pre-GREEN (hub-ring draw order); cleanup done (stale binaries/.o/wavs removed, evidence kept).

## [2026-09-23] correction | Vision works in this environment (old "cannot view PNGs" lore retired)
- The `read` tool presents images directly — verified live: viewed the TR-909 reference (1200×1200) and the Dell `e2shot2.png` capture in-session. The Vulkan4Aros 2026-09-20 "cannot view PNGs" record described a different harness, not this one.
- Consequence: screendump eyeballing is available from here on (complements, never replaces, pixel-census measurement). Prior "blind analysis" wording in the preceding entry now reads as over-cautious, preserved as written.
- Seen in the reference: light warm-gray panel, dark charcoal compact knobs with bright orange radial pointers, orange section labels + Roland mark, dark-red displays, red/orange/yellow buttons, black typography. (The linked image reads as a software-recreation rendering — LCD presets, PANEL/OPTION/HELP/ABOUT — but the shared design language is what matters, not the source.)
- Seen in ours (`e2shot2.png`): small monochrome gray knob blobs, no pointers/color/labels/structure at this scale. Shape + color mismatch confirmed visually, matching the pixel evidence.
- Next: custom knob-frame art can now iterate against direct visual reference (render → view → compare), still under never-pixel-copy.

## [2026-09-23] analysis | TR-909 reference photo measured (knob fidelity groundwork, no art yet)
- Disposition: New (measurement-only; zero pixels reused, nothing rendered)
- Raw: reference photo `https://r2.gear4music.com/media/68/683873/1200/preview.jpg` fetched to `/home/miller/Work/ri_build/ref909/tr909.jpg` (scratch, NOT repo — never-pixel-copy policy)
- Measured: 1200×1200 straight-on (vertical edges constant x24/1176 across all rows); unit palette grays (#30–#e0) + warm orange/amber/rust family (#a06020–#e06020) + reds (#c04040, #804040); saturated elements in bands (knob-row candidate y288-336 tan, button field y480-768 with reds, dark lower section y768-816). User verdict stands: MUIC_Knob gray circles match neither shape nor color.
- Open: blind analysis cannot attribute blobs to knobs-vs-buttons-vs-trim reliably; need user-confirmed knob specifics (body color, pointer style, voice-knob row) before procedural art.
- Next: custom knob-frame renderer (original 2x pixels, spec frame counts, measurement-driven colors) replacing MUIC_Knob visuals; knob_logic untouched.

## [2026-09-23] m16 | Dell 909 panel: explicit-open root cause + compact geometry measured twice, doc locked (TC-2.9.1 device evidence)
- Disposition: New (H2a proof + E0→E2 sizing trail + E0→measured doc lock)
- Raw: llm-wiki/raw/articles/2026-09-23-dell-909-panel-measured-lock-tc291.md
- Updated: app/panel909.c (measured E2 config), gui/panels.c + panels.h (compact accessor), tests/unit/t29_layout.c (compact asserts), docs/evidence/gui/panel-909-geometry.md (lock + struck E0), docs/evidence/gui/acceptance.md (silhouette box checked), llm-wiki/index.md
- **H2a:** creation-open ignored on lane; explicit post-creation open → `RI-909 288x88` listed. Single variable, decisive.
- **Measured twice pixel-identical:** 160×88 at (0,0); knobs x 30/63/96/129 (exact 33px pitch), y≈52, ~32px visuals, all four identical. Knob.mui intrinsic wins over FixWidth everywhere measured.
- **Integrity:** doc-lock-compact chosen over circular lock (design rule + regression gate, falsifiable); custom-draw alternative stays user-optional. Full `ri_audit.sh` 0/0 over frozen tree (0 FAIL lines).
- **Self-caught:** s3–s5 marker gap (false contradiction), SetAttrs include, #define-eating replaceAll, head-truncated build verdict.
- Next: knob drag measurement (TC-2.9.2 device half) + LED/chase (needs 2.10 step GUI).

## [2026-09-23] m15 | Dell GUI first light: Classic window opens, runs, closes clean (TDD layout accessor + v11 lane)
- Disposition: New (first GUI binary on Dell hardware; delivery pipeline proven with the empty window)
- Raw: llm-wiki/raw/articles/2026-09-23-dell-gui-first-light-classic-window.md
- Updated: gui/panels.c (909 knob-rect accessor), gui/panels.h (rect + decl), tests/unit/t29_layout.c (new), scripts/ri_audit.sh (Phase 12 line), llm-wiki/index.md
- **TDD:** RED watched (missing type+function); GREEN all four GUI tests; audit 0/0.
- **Dell (agent e6320, all PASS):** v11 build warning-free first try → ri_classic 28,048 B (task.resource 0, r12 live); put 901 ms sha_ok; run rc=0; winlist shows the 6-panel title at 0,0 1024×768; ui-close → gone, no alert.
- **Fidelity:** layout now (doc-driven, device-measured from here), ReBirth-style art later under never-pixel-copy; MUIC_Knob stock look claimed as-is, not as art.
- Next: four knob widgets at accessor rects → screendump → centers vs doc ±2px.

## [2026-09-23] m14 | M2.5 AIFF export: plain-AIFF writer + pins + goldens (TDD feature)
- Disposition: New (both writers + pin + 2 goldens; AIFF-C out, live rate still open)
- Raw: llm-wiki/raw/articles/2026-09-23-m25-aiff-export-tc213.md
- Updated: audio_io/audio.c/audio.h (BE writers + header + sink + wrapper), tools/render.c (--format + dispatcher + AIFF writer), scripts/ri_audit.sh (Phase 1 AIFF goldens + t32), tests/golden/303/dc-aiff.wav + first-light-aiff.wav + .sha256 (new), docs/evidence/formats/export.md (AIFF section), llm-wiki/index.md
- **TDD:** RED watched in two stages (test compile bug, then link-phase 2 symbols). GREEN all checks; WAV default byte-identical.
- **External:** ffmpeg parses both goldens, dc-aiff decodes byte-identical to WAV PCM (closest available to sox clause). Web: Wikipedia container shape; 80-bit constants verified vs ffmpeg refs (McGill fetch failed transport).
- Full `ri_audit.sh` 0/0 twice (0 FAIL lines). Spirv-val vacuous.

## [2026-09-23] m13 | M2.5 export rate: TC-2.13.4 44.1 kHz native render (TDD feature)
- Disposition: New (--rate threading + 2 goldens; AuRender/live rate + AIFF stay OPEN)
- Raw: llm-wiki/raw/articles/2026-09-23-m25-export-rate-44100-tc2134.md
- Updated: tools/render.c (g_rate threading + tail scaling), scripts/ri_audit.sh (Phase 1 44100 goldens + t31), tests/golden/303/dc-441.wav + first-light-441.wav + .sha256 (new), docs/evidence/formats/export.md (status + section), llm-wiki/index.md
- **Design finding:** no resampler needed — engine DSP is sr-generic; the lock was render-tool RI_SR + fixed tail. Live rate untouched (touches Dell-verified code — own slice).
- **TDD:** RED watched (missing goldens); GREEN all checks; default-rate outputs byte-identical (math-dc + first-light). Full `ri_audit.sh` 0/0 twice (baseline completed untouched this time).
- **Honesty:** replaceAll hit the TAIL #define (caught on readback); wrong golden dir caught by failing render; no websites needed; spirv-val vacuous.

## [2026-09-23] m12 | M2.5 mod pack: TC-2.12.1/TC-2.12.5 pins (no production change)
- Disposition: New (one pin + ledger lock; no `engine/` file touched)
- Raw: llm-wiki/raw/articles/2026-09-23-m25-modpack-tc2121-tc2125.md
- Updated: scripts/ri_audit.sh (formats phase t30 line), docs/evidence/formats/rbnm-full.md (first Status line: LOCKED at 2.12.1/2.12.5), llm-wiki/index.md
- **TDD:** green first run (property pin; layer IDs confirmed by passing load). Measured: 14/14 sane+finite, round-trip identical, 4095/4096 differ + identical remix.
- Full `ri_audit.sh` 0/0 final (0 FAIL lines). Baseline self-invalidated AGAIN (rc=1 line-417 artifact) — lesson strengthened: no edits until the baseline completes.
- **Honesty:** 2.12.2 already 500/500, 2.12.3/2.12.4 device-side; no websites needed; spirv-val vacuous.

## [2026-09-23] m11 | M2.5 export depth: TC-2.13.4 24-bit WAV (TDD feature)
- Disposition: New (export depth for both writers + pin + golden; 44.1 kHz + AIFF stay OPEN)
- Raw: llm-wiki/raw/articles/2026-09-23-m25-export-depth-24bit-tc2134.md
- Updated: audio_io/audio.c/audio.h (header/pack/depth/wrapper), tools/render.c (--depth + float buffers), scripts/ri_audit.sh (Phase 1 dc-24 + t28), tests/golden/303/dc-24.wav + .sha256 (new), docs/evidence/formats/export.md (new, PARTIAL), llm-wiki/index.md
- **TDD:** RED watched clean (link-phase, 3 missing symbols, test compiled diagnostic-free). GREEN all checks; 16-bit math-dc byte-identical thru the rewire.
- **Real finding pre-GREEN:** s24 -1.0 edge (-8388607, truncation, twin-consistent with s16 -32767) — assertion fixed, not code.
- Full `ri_audit.sh` 0/0 twice (0 FAIL lines). No websites needed; spirv-val vacuous.

## [2026-09-23] m10 | M2.4 GUI-tail host cores: TC-2.10.3 copypaste + TC-2.11.1 FX latency (no production change)
- Disposition: New (two pins + two ledger rows; no `engine/` file touched)
- Raw: llm-wiki/raw/articles/2026-09-23-m24-guitail-copypaste-fxlatency.md
- Updated: scripts/ri_audit.sh (songsteps + Phase 10 lines), docs/evidence/formats/rbng.md (TC-2.10.3 section), docs/evidence/pcf/engine.md (latency row), llm-wiki/index.md
- **TDD:** both green first run (property pins). Measured: 16 steps note+flags identical thru file+convert; RMS move 0.348 (63/64 differ) in buffer N+1. Full `ri_audit.sh` 0/0 final (0 FAIL lines).
- **Process lesson:** pre-change baseline audit self-invalidated (rc=2 syntax error — bash parses as it executes; never edit ri_audit.sh under a live run). Failure proves nothing; final green stands alone.
- **Honesty:** TC-2.10.2/2.9.4/2.9.5/2.10.1/2.11.2 out of host scope with per-item reasons; no websites needed; spirv-val vacuous.

## [2026-09-23] m9 | WBS 2.6 mixer acceptance: TC-2.6.1–2.6.3 pins (no production change)
- Disposition: New (three per-TC pins + ledger lock; no `engine/` file touched)
- Raw: llm-wiki/raw/articles/2026-09-23-wbs26-mixer-acceptance-tc261-tc263.md
- Updated: scripts/ri_audit.sh (mixer phase three t26 lines), docs/evidence/mixer/engine.md (LOCKED at 2.6.1–2.6.3, ballistics stays GUI-subjective), llm-wiki/index.md
- **TDD:** all three green first run (property pins); no production cycle in this slice — stated plainly.
- **Measured:** 9 anchors ±0.5 dB + send==fader ratios; rendered 16×16 matrix exact + solo transients ≤9/64; meter 20.00 dB/s (ledger match 0.0999454) + fallback + adoption. Full `ri_audit.sh` 0/0 twice (0 FAIL lines); goldens identical.
- **Honesty:** absolute 0.02 solo bound corrected pre-run (unachievable by construction → derived 9/64 multi-bus bound); ballistics feel out of host scope; no websites needed; spirv-val vacuous.

## [2026-09-23] m8 | WBS 2.5 FX acceptance: TC-2.5.1–2.5.5 pins (M2.4 opens, no production change)
- Disposition: New (five per-TC pins + ledger lock; no `engine/` file touched)
- Raw: llm-wiki/raw/articles/2026-09-23-wbs25-fx-acceptance-tc251-tc255.md
- Updated: scripts/ri_audit.sh (Phase 10 five t25 lines), docs/evidence/pcf/engine.md (LOCKED at 2.5.2–2.5.5, 2.5.1 stays OPEN-04), llm-wiki/index.md
- **TDD:** four pins green first run (property pins); the fifth caught a REAL bug pre-GREEN — mine (memcmp over loader-unwritten struct tails = my stack garbage; memset, green). Production untouched.
- **Measured:** cutoff row8 0 cents, v0 ratio exactly 0.0625, v64 unity exact; sync 0/0.0028/0.0017% (ledger match); 5-min 14.4M samples bit-exact (0.119 s); DC-norm 3×3 ≤1e-6; monotonic sweep max 0.90507; swap energy 0.8101; refusal 54×16 neutral. Full `ri_audit.sh` 0/0 twice (0 FAIL lines); goldens re-render identical.
- **Honesty:** refusal pin ≠ pattern coverage (tag TC-2.5.1-OPEN); no websites needed (contracts local); spirv-val vacuous.

## [2026-09-23] m7 | WBS 2.9 first panel: TC-2.9.1–2.9.3 host pins + right-click default (TDD)
- Disposition: New (two pins + one accessor the pins demanded; M2.3's last leg)
- Raw: llm-wiki/raw/articles/2026-09-23-wbs29-first-panel-tc291-tc293.md
- Updated: gui/panels.c (`ri_panel_default_ctl` impl), gui/panels.h (decl), scripts/ri_audit.sh (Phase 12 two t29 lines), docs/evidence/gui/panel-909-geometry.md (new, E0), docs/evidence/gui/acceptance.md (right-click row, unchecked), llm-wiki/index.md
- **Gap analysis:** TC-2.9.2 right-click had no accessor; TC-2.9.1 named a panel-geometry doc that didn't exist; TC-2.9.3 fully covered by t1 (composition pin only).
- **TDD:** RED watched — `t29_paneldefault` build-RED (implicit declaration); `t29_chase` green pin first run. GREEN: both exit 0, `t1_knob` still PASS. Full `ri_audit.sh` 0/0 twice (pre-change baseline + final, 0 FAIL lines).
- **Honesty:** mouse-capture is MCC-shell platform behavior (out of pin scope, stated); one self-caught slip (stray decl in panels.c + comment-only header) fixed pre-GREEN; no websites needed (contracts local); spirv-val vacuous.

## [2026-09-23] m6 | WBS 2.4 909 acceptance: TC-2.4.1–2.4.5 pins + panel-Tune wiring (TDD)
- Disposition: New (five per-TC pins + one production surface the pins demanded)
- Raw: llm-wiki/raw/articles/2026-09-23-wbs24-909-acceptance-tc241-tc245.md
- Updated: engine/dsp/rb909.h (`RI_CTL_909_*` 0x0900–0x0903, `rb909_set_param` decl), engine/dsp/params.c (909 section: TUNE writes the engine byte, LEVEL/DECAY/FLAMRES documented placeholders), scripts/ri_audit.sh (Phase 9 five t24 lines), docs/evidence/909/{bd,sd,ch,oh,cr,rd}.md (LOCKED), llm-wiki/index.md
- **TDD:** RED watched — `t24_909xfade` build-RED (`rb909_set_param`/`RI_CTL_909_TUNE` undeclared); accent/quirk/retrig/swap green pins on frozen code.
- **GREEN:** all five exit 0 — xfade worst 0.01278 dB / boundary 0.008906 dB (band 1.0); acc1 1.1503; flam onset 1682, ratio 0.7605; quirk/retrig maxdiff exactly 0.0; swap BUSY×2 + edge ≤1e-4. Full `ri_audit.sh` 0/0 twice (post-change + final incl. ledgers, 0 FAIL lines); all six 909 goldens re-render byte-identical (no regen — param section touches no render path).
- **Honesty:** retrigger/swap pinned on BD, siblings cite the voice-generic path; quirk pinned per-voice direct (no proxy); no websites needed (contracts local: WBS + spec §11 + prior-art); spirv-val vacuous (no SPIR-V in repo).

## [2026-09-23] m5 | WBS 2.3 808 acceptance: TC-2.3.1–2.3.5 pins + metal-ratio lock (TDD)
- Disposition: New (five per-TC pins + one production constant the pins demanded)
- Raw: llm-wiki/raw/articles/2026-09-23-wbs23-808-acceptance-tc231-tc235.md
- Updated: engine/dsp/rb808.c (ratio array → WBS contract {0.83,1.48,2.26,2.92,3.94,5.31}), engine/dsp/rb808.h (`RI_808_METAL_BASE` 400→1000 → partials 830..5310 Hz; ctl-id block `RI_CTL_808_BASE+0..5`; `rb808_set_param` decl), engine/dsp/params.c (808 section: `RI_808_DECAY_TBL` 9 anchors 0.18..2.8 s + `rb808_set_param`), tests/unit/t1_808.c (§2 base literal → `(double)RI_808_METAL_BASE`), scripts/ri_audit.sh (Phase 8 five t23 lines), tests/golden/808/{ch,oh,cy,storm}.wav + `.sha256` (regenerated; other 11 byte-identical), docs/evidence/808/{ch,oh,cy,bd}.md (LOCKED), llm-wiki/index.md
- **TDD:** RED watched ×2 — `t23_808hat` 8/12 FAIL pre-lock (the 4 passing partials were coincidental harmonics of the 400-base cluster); `bd` build-RED (`rb808_set_param`/`RI_CTL_808_DECAY` undeclared); clap/accent/storm green pins on frozen code (clap peaks 0.21/9.5/18.2/27.3 ms, gaps 9.3/8.7/9.1 ms).
- **GREEN:** after lock + param section — hat m0 5–45× above ±0.5% skirts at all 6 contract freqs, CH+OH; BD white-box tau 0.18/2.8 ±10% at knob 0/127 + rendered 1/e crossing (64 ms RMS window — 5 ms window was shorter than the 48 Hz settled period); all five exit 0. Full `ri_audit.sh` 0/0 (Phase 8 re-render determinism over all 16 goldens).
- **Stale-render trap:** `ri_build_host.sh all` compiles MOD objects only; `$OUT/render` is linked by ri_audit.sh (line 36). Hand-run golden renders used the pre-lock `render` binary → byte-identical to committed goldens → suspicious; relinked `render` → expected 4/12 golden split. Golden re-renders must use the relinked tool.
- **Honesty:** "14-voice" in TC-2.3.5, engine has 15 — the storm pin uses mask 0x7FFF (superset, noted in test + article); spirv-val vacuous (no SPIR-V in repo), noted per standing.

## [2026-09-23] ingest | Wiki audit: close-path/ABI corrections + record annotations
- Disposition: Update (three records annotated; one body correction in the fresh close-path record)
- Updated: llm-wiki/raw/articles/2026-09-22-wbs27-negotiate.md (Status appended), llm-wiki/raw/articles/2026-09-22-wbs21-swappendix.md (Status appended), llm-wiki/raw/articles/2026-09-23-wbs27-close-path-interruptible-play.md (ABI phrase corrected), llm-wiki/index.md (three rows annotated), llm-wiki/log.md (this entry)
- **ABI correction:** the Dell E6320 is the ABIv11 iteration reference (m1-1: v1-SDK startup.o opens task.resource, which ABIv11 rejects); ABIv1 is the ULTIMATE acceptance target. The close-path record's "ABIv1 target" phrase is corrected in-file (fresh record); the negotiate record carries a Status note instead (older record, body left as written).
- m3's log said the negotiate article was "Updated" — the file was never touched; it now carries its missing Status (m3 `f7b435b` + m4 `f41efa8` continuation + the ABI clarification).
- t21_swaprender: the 09-22 swappendix article claimed `ri_audit.sh` "(already wired)" — m3 found it wasn't and wired it; the article now carries a Status so the claim can't contradict the m3 record.
- Coverage audit: every recent commit topic maps to an indexed article (WBS 2.1 builder/snapshot/swap/storm chain, 2.2 303 family, 2.7 negotiate + close path) — no missing records found.
- Lane: e6320 healthy, pending 0; no spool restart needed (the user's head-up went unused).

## [2026-09-23] m4 | WBS 2.7 close path: interruptible playback (AuPlayEx, Dell green)
- Disposition: New (API + audit decl-wiring + Dell run)
- Raw: llm-wiki/raw/articles/2026-09-23-wbs27-close-path-interruptible-play.md
- Updated: audio_io/audio_ahi.h, audio_io/audio_ahi_play.c, scripts/ri_audit.sh (AuPlayEx decl gate), llm-wiki/index.md
- `AuPlayEx(ao, stop)` polls the caller-owned flag between CMD_WRITE chunks (granularity ≤ 1 chunk ≈ 171-186 ms) and on stop releases EVERYTHING (Close + CloseDevice + DeleteIORequest + DeleteMsgPort + DeleteFile); `AuPlay` is now a wrapper → `AuPlayEx(ao, NULL)` (full-song path unchanged: `played 250286 bytes rc=0`).
- **STOP PROVEN ON DELL (one boot, job 20260923-143658-211894-1):** ahi_neg rc=0 → `auplay_stop` `stopped 81920 bytes rc=1` (5×16384 B chunks ≈ 0.93 s of ~2.84 s song; dump on a chunk boundary) exit 0 → ahi_neg rc=0 (NO wedge) → `auplay` `played 250286 bytes rc=0` → ahi_neg rc=0. Stop driven by a spawned task (NewCreateTask + TASKTAG_ARG1, Delay(50) then flag flip).
- **TDD:** RED = scratch `auplay_stop_main.c` implicit-declaration of AuPlayEx against the pre-API header; GREEN after. Audit gained the decl-grep for both AuPlay and AuPlayEx; fresh `ri_audit.sh` 0/0.
- **Grounded in driver autodoc** (v11 `Docs/ahidev.texinfo`): "All I/O requests must be completed before CloseDevice()" — between-chunks polling keeps the close-time request table empty, so release needs no AbortIO. spirv-val vacuous (no SPIR-V in repo). ABI gates re-checked: task.resource 0, real UND 0.

## [2026-09-23] m3 | WBS 2.7 playback slice: engine music on hardware (leak wedge fixed)
- Disposition: Update (same slice completed) + postmortem (leak wedge)
- Updated: llm-wiki/raw/articles/2026-09-22-wbs27-negotiate.md, audio_io/audio_ahi_play.c (new TU), scripts/ri_audit.sh (both TUs + CWD pin + t21_swaprender wired), llm-wiki/index.md
- **PLAYBACK PROVEN ON DELL:** `auplay` → `played 250286 bytes rc=0` (≈2.84 s mono 16-bit @ 44100) via full engine path (AuStart → AuRenderToFile → AuPlay → open unit 0 → CMD_WRITE stream → close). `AHIST_M16S` (mono by design; 09-22 mono-as-stereo bug on record).
- **LEAK WEDGE POSTMORTEMED:** one leaked `AllocAudioA` wedges the driver-global HW reservation (blocks unit-0 AND fresh NO_UNIT opens) until reboot — the 09-22 `open unit 0 FAILED ioerr=0` + fresh negotiator rc=10. Fix: `au_ahi_negotiate` releases EVERYTHING (FreeAudio + CloseDevice + port/req delete). Proven by sequence on ONE boot: negotiate → play → negotiate all rc=0.
- **AUDIT ROBUSTNESS FIX:** `ri_audit.sh` `cd "$ROOT"` at startup — S909 (`--909pack`) died with a silent `|| exit 1` from any non-repo CWD (render's pack inventory is CWD-relative, tools/render.c:714); only `bash -x` surfaced it. Audit 0/0 from any directory now.
- **Straggler wired:** t21_swaprender (TC-2.1.3 PCM half, 09-22 article claimed "already wired" — it wasn't; added to the t21 list; passes).

## [2026-09-22] ingest | Commits f6df244 + 8487809 recorded; live lane refresh
- Disposition: Update (commit records missing from log) + ingest (ops state)
- Updated: llm-wiki/log.md (this entry)
- `f6df244` feat: WBS 2.1 builder chain songsteps->feed->file->looppcm (TC-2.1.2) — pushed to origin/main.
- `8487809` feat: WBS 2.1 snapshot playback path, snapbuild + windowing + swap soak + storm (TC-2.1.3/2.1.4) — pushed to origin/main.
- Live lane refresh: v1 + laptop spike serves up (9091 home-spool anon, 9292 pairs e6320); QEMU aros_v1 idle with green set in RAM:; Dell agent last session 750. ENV CHURN: a new `aros_v1dh0_nocd` guest appeared and `v1c` is gone (other session churning VMs) — always re-verify guest identity (Info volumes) before consequential runs, never assume monitor-port ownership.

## [2026-09-22] ingest | Commits d4f14fb recorded; copytruncate + live lane refresh
- Disposition: Update (commit record) + ingest (ops, with authorization noted)
- Updated: llm-wiki/log.md (this entry)
- `d4f14fb` feat: storm slide coverage + first sound on reference box [TC-2.1.4] — pushed to origin/main.
- Copytruncate (USER-AUTHORIZED): frozen v1b debug VM's serial log (1.6 GB, still growing) copied to `/home/miller/Work/v1b_gdbdis_serial.log.relocated-20260922`, original truncated to unblock linking. QEMU appends (O_APPEND); seconds of trace at the seam potentially lost, all prior content preserved. NOTE: `du` later showed only 856K real blocks (sparse) — the true hogs are /tmp/claude-1000 (5.4G) + /tmp/opencode (653M), untouched per rule.
- Live lane refresh: spike serves v1b 9191 + laptop 9292 + v1 9091 + v1dh0 9195 (last is the other session's new guest lane); QEMU aros_v1 idle with green set in RAM:; Dell agent session 750 alive; /tmp quota still binding (link needs a window; TMPDIR ignored by x86_64-aros-ld).

## [2026-09-22] m2 | TC-2.1.4 storm: held-note slide covered (TDD follow-up)
- Disposition: Update (slide path in storm context; silence semantics pinned both ways)
- Updated: tests/unit/t21_storm.c; llm-wiki/raw/articles/2026-09-22-wbs21-storm.md (Standing)
303B note + mid-buffer slide_to: renders sound AND differs from plain (RED "slide inaudible" watched, GREEN ratio 0.474); slide-from-silence = silence by voice semantics (gate needs held note), coherent. Audit 0/0 (after one transient quota flake on the AROS phase). Uncommitted.

## [2026-09-22] m2 | TC-2.1.4 Dell run GREEN (reference hardware, thin margin)
- Disposition: Update (first Dell run FAIL was a test bug; fixed; PASS)
- Updated: tests/unit/t21_storm.c (assert `<= 1.0`); llm-wiki/raw/articles/2026-09-22-wbs21-storm.md (Dell Standing)
Cross-build ABIv11 clean (no libm needed by DSP; uname/clock/printf work); Dell session 750: ratio 0.9000 PASS (0.45× ≤ 0.5×). Margin thin (10%) — watch item. Assert double-half corrected; -march ruled out first (0.93→0.90). Scratch: /home/miller/Work/ri_build/aros_storm/ + storm_split.c.

## [2026-09-22] m2 | TC-2.1.4 DSP-side storm budget (host proxy, TDD)
- Disposition: New (test + audit wiring; core-path storm waits on device wiring)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-storm.md
- Updated: tests/unit/t21_storm.c, scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
4 sections, ratio 0.459/0.5 (thin — AROS-box run open); per-section energy (303B slide-silence found); determinism; /tmp/ri/build+run symlinked to home fs (quota); audit 0/0. Uncommitted.

## [2026-09-22] m2 | WBS 2.1 staged swap arbiter + 10k soak (TC-2.1.3 event half)
- Disposition: New (arbiter + soak test; PCM click half waits on voices)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-seqswap.md
- Updated: engine/seq/riseq.h, engine/seq/riseq.c, tests/unit/t21_seqswap.c, llm-wiki/index.md
RED (missing fns) → vacuous-proof removed → preset-cancel discriminator → mutant-caught exactly there → GREEN + audit 0/0. Uncommitted. (Phase 6b already runs t21_seqswap from the prior turn.)

## [2026-09-22] m2 | WBS 2.1 snapshot-build + event windowing (TDD)
- Disposition: New (2 micro-cycles: builder completion + window query)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-snapbuild-evwin.md
- Updated: engine/seq/snapbuild.h, engine/seq/snapbuild.c, engine/seq/riseq.h, engine/seq/riseq.c, tests/unit/t21_snapbuild.c, tests/unit/t21_evwin.c, scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
RED watched both; mutants caught both; GREEN + audit 0/0. Swap-soak + storm next. Uncommitted.

## [2026-09-22] m2 | TC-2.1.2 PCM half green: loop double-render equality
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-looppcm.md
- Updated: tests/unit/t21_looppcm.c, scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
Doubled pluck-loop iteration-identical (settle-gap design; sustain case needs reset semantics — open); size-guard arithmetic corrected from writer source; pitch-mutant non-vacuity; audit 0/0. Uncommitted.

## [2026-09-22] m2 | WBS 2.1 file-level song path pin (RBNG→builder→walker)
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-songfile.md
- Updated: tests/unit/t21_songfile.c, scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
Round-trip + convert + 6 exact events; PASS first run; test-side mutant (6 cascading fails); audit 0/0. Uncommitted.

## [2026-09-22] m2 | WBS 2.1 converter→walker feed pin (integration)
- Disposition: New (test + audit wiring; no production change)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-schedfeed.md
- Updated: tests/unit/t21_schedfeed.c, scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
Hand-derived 10-event expectations (NULL-opts FLAM, §8 sort via seq 6,8,7); PASS first run; mutant-caught (16 fails); audit 0/0. Uncommitted.

## [2026-09-22] m2 | WBS 2.1 song→steps converter (builder step 1, TDD)
- Disposition: New (converter + test + build/audit wiring)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-songsteps.md
- Updated: engine/seq/songsteps.h, engine/seq/songsteps.c, tests/unit/t21_songsteps.c, scripts/ri_build_host.sh (MOD_sched), scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
RED (missing header) → mutant-caught → GREEN (PASS, audit 0/0). Walker feed + file golden next. Uncommitted.

## [2026-09-22] m2 | WBS 2.1 snapshot contract + loop cursor (TC-2.1.2 math half, TDD)
- Disposition: New (contract + cursor + test + audit wiring)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-snapshot-looppos.md
- Updated: engine/seq/riseq.h, engine/seq/riseq.c, tests/unit/t21_seqloop.c, scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
RED (missing type) → mutant-caught → GREEN (both seq tests PASS, audit 0/0). PCM half waits on song→events builder. Uncommitted.

## [2026-09-22] m2 | WBS 2.1 first step: riseq + TC-2.1.1 (TDD, audit 0/0)
- Disposition: New (module + test + build + audit phase)
- Raw: llm-wiki/raw/articles/2026-09-22-wbs21-riseq-tc211.md
- Updated: engine/seq/riseq.h, engine/seq/riseq.c, tests/unit/t21_seq.c, scripts/ri_build_host.sh (MOD_sched), scripts/ri_audit.sh (Phase 6b), llm-wiki/index.md
RED (missing header) → test-logic correction (quantization) → mutant-caught → GREEN (PASS t21_seq, audit 0/0). Create→Init adaptation for heap ban; 44.1 kHz sweep for rate-independence. Uncommitted.

## [2026-09-22] task6 | Latency floor reconciled to measured 64 + harness exit fix (TDD, audit 0/0)
- Disposition: New (code + test + gate; backend choice recorded)
- Raw: llm-wiki/raw/articles/2026-09-22-task6-latency-floor-64-measured.md
- Updated: audio_io/audio.h (define 64u), audio_io/audio.c (comments), tests/unit/t6_w1backend.c (assert 64), scripts/ri_audit.sh (gate pins 64u), scripts/ri_build_host.sh (exit propagation), llm-wiki/index.md (entry)
RED watched ("latency 256, want 64"); GREEN (one-renderer SHA intact, fallback pinned); full audit 0/0. Backend: LOW-LEVEL confirmed by measurement. Uncommitted.

## [2026-09-22] gate | G5 CLOSED: OPEN-09 measured + OPEN-06 named (spec rows + commit)
- Disposition: Update (gate commit; report Status flipped)
- Updated: docs/superpowers/specs/2026-09-20-reincarnation-spec.md (OPEN-09 MEASURED with values + report pointer; OPEN-06 NAMED Dell/ABIv1); docs/evidence/formats/m1-1-report.md (Status PARTIAL→MEASURED)
- Commit: `86e975c` `chore: M1.1 AHI measurement closes OPEN-09 [gate:G5]` (exact prescribed message), pushed to origin/main; ri_audit 0/0 on the result. Prose references to OPEN-09 as open (§2.3 etc.) deliberately left — rows are the gate mechanism (OPEN-08 precedent).

## [2026-09-22] docs | m1-1-report.md gains Appendix B + filled hardware fields (G5 still OPEN)
- Disposition: Update (report doc; no commit, no spec-row change — gate commit rides separately)
- Updated: docs/evidence/formats/m1-1-report.md (Status PARTIAL; 9/9 fields filled from hardware; hypothesis verdicts incl. accepted-but-not-honored third outcome; Dell-lane procedure-as-executed; Appendix B verbatim runs 1-3); llm-wiki/raw/articles/2026-09-22-m1-1-real-hardware-abiv11-e6320.md (report open-item closed)
Backend input for Task 6: low-level to 64 frames w/ MixFreq+hook; device path no advantage. Chosen-backend cell is INPUT, not a decision.

## [2026-09-22] measure | 11 Hz verdict CLOSED: Player rate fixed, PlayerFreq accepted-but-not-honored
- Disposition: Update (verdict on the measurement record; no new raw — ladder-line-only variants are scratch)
- Updated: llm-wiki/raw/articles/2026-09-22-m1-1-real-hardware-abiv11-e6320.md (verdict + open-item close)
Variants `probe_ahi_{128,256}` (repo probe untouched; `-O2` recipe proven within 8 B of the recorded `.o`; 24.0 KB, task.resource=0, UND=1): frames=128 → 55/1875, frames=256 → 55/935, repro 64 → 55/3750 (all rc=0, session 750). Observed exactly 55 across all three 5 s windows while requests span 750/375/187 Hz → rate does NOT track; driver-owned fixed ~11 Hz, deterministic. `AHIA_PlayerFreq` accepted (rc=0) but callback delivery stays 11 Hz; AllocAudio still 44100/16-bit. Spec consequence: latency/PlayerFunc-rate gate against ~11 Hz reality or engine independent of Player rate. Scratch sources under /home/miller/Work/ri_build/ (kept, outside repo).

## [2026-09-22] pin | Reference-box identity pinned (BIOS/AROS/stick/HDA) + M1.1 reproduced post-reboot
- Disposition: Update (pin values into the decision record; reproduction note on the measurement record)
- Raw: (none new — values landed in existing records)
- Updated: llm-wiki/raw/articles/2026-09-22-dell-reference-box-iteration-abiv1-target.md (TO-PIN → pinned); llm-wiki/raw/articles/2026-09-22-m1-1-real-hardware-abiv11-e6320.md (session-750 reproduction); llm-wiki/index.md (entry)
Pinned, all box-measured: BIOS A19 (operator read; no software path on stick); Kickstart 51.51 / AROS 41.3 / Exec 51.8 64-bit / GRUB 2.12 (`vesa=1024x768 ATA=32bit`); stick sha256 `dbbc8fad…e522f` (8,589,837,312 B), mbr md5 `02962980…4ce5b7`; HDA 8086:1C20 rev 04, Dell sub 0x0492, BAR0 0xe2e60000/0x4000, IRQ 22 (PCITool full save 11,606 B GET + ENVARC:hdaudio.config 2,182 B GET, both sha-verified; mode file binds hdaudio↔0x003E0001). Codec: IDT 92HD90 HYPOTHESIS (Dell P12S manual + driver-page compat + driver IDT branch; verb-level OPEN — DumpDebugBuffer absent, Sashimi opens no window via Run). Reproduction: post-reboot probe rerun identical (base 0x0101376c80, varies, >4GB). Lane notes: /tmp usrquota blocked submits twice (cleared own /tmp/ri/run fuzz+audits ~80MB; spool moved to /home/miller/Work/spike_spool_laptop via symlink); RequestChoice + foreground sashimi wedge the agent (operator Enter/close-gadget; redial displaces); errno 60 = ETIMEDOUT (host side verified clean: IP .81, nft+ufw, listener). Cascade: index entry updated.

## [2026-09-22] decision | Dell E6320 named iteration reference; ABIv1 stays the ultimate target
- Disposition: New (decision record; binds future work)
- Raw: llm-wiki/raw/articles/2026-09-22-dell-reference-box-iteration-abiv1-target.md
- Updated: llm-wiki/raw/articles/2026-09-22-m1-1-real-hardware-abiv11-e6320.md (Status note); Vulkan4Aros llm-wiki/raw/articles/2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md (Status note)
Iteration settles on Dell (real codec/timing); acceptance passes on ABIv1. Consequences: dual-build every probe (v1 SDK + v11 SDK), source-portable not binary-portable, lane-scoped thresholds, characterize-on-Dell/confirm-on-ABIv1, QEMU/VOID retained. Pinned: E6320/stick/MAC/IP/ahi.device v6; TO-PIN: BIOS rev, AROS build/rev, stick hash, codec ID. Cascade: index section entry.

## [2026-09-22] ingest | M1.1 measured on real hardware (E6320 ABIv11, v11-toolchain build fix)
- Disposition: New (first hardware measurement; toolchain grounding)
- Raw: llm-wiki/raw/articles/2026-09-22-m1-1-real-hardware-abiv11-e6320.md
v1-SDK startup.o opens task.resource → ABIv11 privilege violation (Exec_83_OpenResource); v11 build (gcc-16.1.0 + v11 SDK startup.o, AHI header from v11 source) runs the probe fully. Run session 285 PASS: base 0x0100eed720, best_mode 0x003E0001, ladder 7/7 → low_min=64, verify 55/3750 (shortfall 3695, honest real timing), unit 0 PRESENT err1=0/err2=-2, SUMMARY complete. Cascade: cross-posted to Vulkan4AROS wiki (ABIv11 laptop build rule).

## [2026-09-21] integration | work/ri-planexec merged to main and pushed
- Commits (verified: ri_audit 0/0 on pre-merge tree and on merged result):
  `a973ea1` probe fix [gate:G5], `39e13b5` campaign docs [gate:G14],
  `9419def` M1.1 codec-less appendix [gate:G14].
- Merge: fast-forward `0c23eb8..9419def` into `main`; audit green after;
  pushed `main`; deleted local branch; pruned `origin/work/ri-planexec`
  (verified fully merged, tip identical). Tree clean.
- Held out (local-only, uncommitted): `/tmp/ari-hda-fix` worktree sources
  (M-markers, ReadConfig barriers, SB128 volatile, HookEntry-adjacent
  device work); `/tmp` shadows/probes (scratch by convention).

## [2026-09-21] ingest | ahi.library upstream/deadwood survey + linked-r2 analysis
- Disposition: New + New (two sources, one turn)
- Raw: llm-wiki/raw/articles/2026-09-21-ahi-library-upstream-deadwood-survey.md; llm-wiki/raw/articles/2026-09-21-linked-r2-delay-analysis.md
Neither fork ships/builds ahi.library (deadwood differs only in catalog sources); r2-chaining traced to WaitingList promotion suspect with bisect recipe; not gate-blocking. Cascade: none.

## [2026-09-21] ingest | Repo probe hardened and green end-to-end
- Disposition: New (TDD close-out; complements the bisect/analysis records with the deliverable fix + final run)
- Raw: llm-wiki/raw/articles/2026-09-21-repo-probe-green.md
P4 abort bounds (dev_min counts natural err==0 only); final green (actuals 5513 Hz/32-bit/128ch, 50.3M vs 3750, err1=0/err2=-2, SUMMARY, alive, zero IRQ); /tmp quota hygiene. Cascade: none.

## [2026-09-21] update | Linked-r2 bisect executed, in-flight chaining confirmed
- Disposition: Update (Status note on the analysis record; no new raw — result is one measured line)
- Updated: llm-wiki/raw/articles/2026-09-21-linked-r2-delay-analysis.md
P4.2b WaitIO r2 done err=0 (linked-to-completed, TRUE WaitIO); undo-delay branch works, in-flight chaining is the broken case; promotion-on-completion remains the precise suspect. Index entry updated.

## [2026-09-27] design | Devices tab elegance: rail-first decision (owner)
- Disposition: Decision (challenge -> direction)
The tab managed VISIBLE; the rack requirement says ACTIVE. Owner
picked rail-first: toggles to a slim always-visible rail above the
Register (same visibility-only bit); activation later. t98 reverts to
4 tabs (nothing consumes the 5th).

## [2026-09-27] ingest | Tabbed RIAPP + device rail + activation
- Disposition: New (no existing article covers RIAPP panels)
- Raw: llm-wiki/raw/articles/2026-09-27-tabbed-riapp-rail-activation.md

## [2026-09-27] ingest | 909 pack descriptor dangle crash
- Disposition: New (nothing covers the pack bind path)
- Raw: llm-wiki/raw/articles/2026-09-27-909-pack-dangle-crash.md

## [2026-09-27] ingest | Dell lane bridge recovery
- Disposition: New (lane-ops; complements the Vulkan4AROS laptop article, not copied)
- Raw: llm-wiki/raw/articles/2026-09-27-dell-lane-bridge-recovery.md

## [2026-09-27] ingest | Rail LED dots bitmap saga
- Disposition: New (owner-ordered handoff: everything tried, failures, readings)
- Raw: llm-wiki/raw/articles/2026-09-27-rail-led-bitmap-saga.md

## [2026-09-28] ingest | Render-spike hunt deferred (USB falsified)
- Disposition: New (hunt record; hunt itself deferred by owner decision)
- Raw: llm-wiki/raw/articles/2026-09-28-render-spike-hunt-deferred.md

## [2026-09-28] ingest | Planned device ASM Leviasynth
- Disposition: New (second named rack device; E1 researched same-day)
- Raw: llm-wiki/raw/articles/2026-09-28-planned-device-asm-leviasynth.md

## [2026-09-28] ingest | Mix/FX tabs as a rack bay
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-28-mix-fx-rack-bay.md

## [2026-09-28] ingest | Leviasynth v1 implementation (slices 3b-3c-iv)
- Disposition: New (implements the planned-device article; decisions recorded)
- Raw: llm-wiki/raw/articles/2026-09-28-leviasynth-v1-implementation.md

## [2026-09-28] ingest | Levi filter NaN crash
- Disposition: New (crash record; fix bc9237c, owner re-proof approved)
- Raw: llm-wiki/raw/articles/2026-09-28-levi-filter-nan-crash.md

## [2026-09-28] ingest | Dell Levi proof runs
- Disposition: New (lane-ops; complements the bridge-recovery article)
- Raw: llm-wiki/raw/articles/2026-09-28-dell-levi-proof-runs.md

## [2026-09-28] lint | 0 issues found, 0 auto-fixed

## [2026-09-28] ingest | Rack pass 2 and GUI round-3 plan
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-28-rack-pass-2-and-gui-round3-plan.md

## [2026-09-29] ingest | GUI round 3 S1–S3 and interoperability requirement
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-29-gui-round3-s1-s3-and-interop-requirement.md

## [2026-09-29] ingest | Leviasynth v2 arp/seq/matrix/LFO/reverb
- Disposition: New (device lane; complements the v1 implementation article)
- Raw: llm-wiki/raw/articles/2026-09-29-leviasynth-v2-arp-seq-matrix-lfo-reverb.md

## [2026-09-29] lint | v2 ingest self-check: 0 issues (links resolve, hashes verified in git, index row placed)

## [2026-09-29] ingest | GUI round 3 S4–S5 master + zoomfit
- Disposition: New (companion to the S1–S3 article; S4/S5 + visual/wiring/lane findings)
- Raw: llm-wiki/raw/articles/2026-09-29-gui-round3-s4-s5-master-zoomfit.md

## [2026-09-29] lint | 1 issue found, 0 auto-fixed

## [2026-09-29] ingest | GUI round 3 S4b + S5 evidence closure
- Disposition: New (companion to the S4–S5 article; S4b, menu fix, captures, z2 finding)
- Raw: llm-wiki/raw/articles/2026-09-29-gui-round3-s4b-menu-captures.md

## [2026-09-29] lint | 1 issue found, 0 auto-fixed
- Note: the S4–S5 article's pending-captures line is superseded by this ingest (repo convention: no retro-edits, same as the S1–S3 S4-in-progress line).

## [2026-09-29] ingest | Zoom-guard proof + spooler fix
- Disposition: New (companion; guard proof, persist mystery, spooler lesson)
- Raw: llm-wiki/raw/articles/2026-09-29-zoom-guard-proof-spooler-fix.md

## [2026-09-29] lint | 0 issues found, 0 auto-fixed

## [2026-09-29] ingest | GUI round 3 approval closes the round
- Disposition: New (owner verdict record; closes the round)
- Raw: llm-wiki/raw/articles/2026-09-29-gui-round3-approval-close.md

## [2026-09-29] lint | 0 issues found, 0 auto-fixed

## [2026-09-29] ingest | Owner font decision (in-house face kept)
- Disposition: New (closes round-3 §9 item 1)
- Raw: llm-wiki/raw/articles/2026-09-29-font-decision-in-house.md

## [2026-09-29] lint | 0 issues found, 0 auto-fixed

## [2026-09-29] ingest | S7a–c + S6 click research
- Disposition: New (feature record + ranked suspects + server-side prep)
- Raw: llm-wiki/raw/articles/2026-09-29-s7abc-s6-research.md

## [2026-09-29] lint | 0 issues found, 0 auto-fixed

## [2026-09-29] ingest | S6 closed + S7 proven on the Dell
- Disposition: New (click root causes + acceptance + rendering proof)
- Raw: llm-wiki/raw/articles/2026-09-29-s6-closed-s7-proven.md

## [2026-09-29] lint | 0 issues found, 0 auto-fixed

## [2026-09-29] ingest | ExAll fix + acceptance numbers + key forensics
- Disposition: New (companion; close-out record)
- Raw: llm-wiki/raw/articles/2026-09-29-exall-acceptance-forensics.md

## [2026-09-29] lint | 0 issues found, 0 auto-fixed

## [2026-09-29] ingest | GUI round 3 close-out verified; audit fixes
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-09-29-round3-verification-audit-fixes.md

## [2026-09-30] ingest | Leviasynth fidelity plan P1–P5
- Disposition: New (phase record; continues the v2 record)
- Raw: llm-wiki/raw/articles/2026-09-30-leviasynth-fidelity-p1-p5.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-09-30] ingest | Songs & playlists + Zombie Nation demo
- Disposition: New (feature record; two owner decisions: local-only cover, RBNG Levi bank v1.5)
- Raw: llm-wiki/raw/articles/2026-09-30-songs-playlists-zombie-nation.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-09-30] ingest | Method findings from the songs session
- Disposition: New (companion to the songs & playlists record)
- Raw: llm-wiki/raw/articles/2026-09-30-method-findings-songs-session.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-09-30] ingest | Leviasynth fidelity P6a + P6b (voice)
- Disposition: New (continues the P1–P5 phase record)
- Raw: llm-wiki/raw/articles/2026-09-30-leviasynth-fidelity-p6a-p6b.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-09-30] ingest | riqemu1 wide modes + parked audio
- Disposition: New (lane record; extends the sb128-open-hang record)
- Raw: llm-wiki/raw/articles/2026-09-30-riqemu1-wide-modes-parked-audio.md
- Updated: llm-wiki/index.md (Lane infrastructure entry list)

## [2026-09-30] ingest | riqemu1 quarantine revert + riaudio sound proof
- Disposition: New (corrects the quarantine in the wide-modes/parked-audio record; extends the 09-28 audio-in-qemu record)
- Raw: llm-wiki/raw/articles/2026-09-30-riqemu1-quarantine-revert-riaudio-sound.md
- Updated: llm-wiki/index.md (Lane infrastructure entry list)

## [2026-10-01] ingest | Menu hang + tab-switch artifacts: load governor and damage-box fix
- Disposition: New (follows the songs & playlists record)
- Raw: llm-wiki/raw/articles/2026-10-01-menu-hang-tab-artifacts-load-governor.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P6c + P6d (voice complete)
- Disposition: New (continues the P6a + P6b phase record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p6c-p6d.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P7a (device delay)
- Disposition: New (starts the P7 FX record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p7a-delay.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Dell deploy: ABIv11 trap + RIAPP and songs on the USB stick
- Disposition: New (deploy record; follows the menu-hang/load-governor record)
- Raw: llm-wiki/raw/articles/2026-10-01-dell-deploy-abiv11-usb-stick-layout.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P7b (device reverb)
- Disposition: New (continues the P7 FX record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p7b-reverb.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P7c (pre/post mod engines)
- Disposition: New (continues the P7 FX record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p7c-modfx.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P8a (device tempo, P7d closed)
- Disposition: New (starts the P8 record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p8a-tempo.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P8b (device arp)
- Disposition: New (continues the P8 record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p8b-arp.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P8c (device sequencer)
- Disposition: New (continues the P8 record)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p8c-seq.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Dell Zombie Nation telemetry + song-load diagnosis
- Disposition: New; Update (narrows the starvation hypothesis in the menu-hang record)
- Raw: llm-wiki/raw/articles/2026-10-01-dell-zn-telemetry-song-load-diagnosis.md
- Updated: llm-wiki/index.md (Live app entry list)
## [2026-10-01] ingest | Leviasynth fidelity P8d (ribbon)
- Disposition: New (continues the P8 record); Update (pins the P8d commit hash)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p8d-ribbon.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P8e (step-LFO editor)
- Disposition: New (closes the P8 record); Update (amends the P8e plan key-block note; pins the P8d commit hash)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p8e-lfostp.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P9a (performance signals)
- Disposition: New (opens the P9 record); Update (amends the P9a bend plan bullet: per-sample semitone offset, not a retune pass; pins the P8e commit hash)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p9a-perfsig.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P9b (performance amounts)
- Disposition: New (continues the P9 record); Update (pins the P9b commit hash; records the P9a mono-refresh gap closed here)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p9b-perfamt.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Clipping (Comp make-up) and tab-switch dropouts (render priority)
- Disposition: New; Update (resolves the deferred 2026-09-28 render-spike hunt; supersedes the pri-10 render choice in the menu-hang record)
- Raw: llm-wiki/raw/articles/2026-10-01-clipping-comp-limiter-tab-stall-priority.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P9c (keyboard zones)
- Disposition: New (continues the P9 record); Update (pins the P9c commit hash; records the panel reachability probe the zone dim law forced)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p9c-zones.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Leviasynth fidelity P9d (glide hold + chord mode)
- Disposition: New (continues the P9 record); Update (corrects the P9d code-time decision on what releasing the glide button does to a slide in progress — it stops it, it does not finish it; pins the P9d commit hash)
- Raw: llm-wiki/raw/articles/2026-10-01-leviasynth-fidelity-p9d-glidechord.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-01] ingest | Dell xruns while playing and switching tabs (governor trip law + repaint policy)
- Disposition: New; Update (confirms the pri-21 render-task fix held on the device — all 7 tab switches `xruns+0` — and records the two remaining causes, the pri -1 fallback tripping on peaks and the 100 ms tick's blanket repaint; leaves the owner's symptom open pending a redeploy)
- Raw: llm-wiki/raw/articles/2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-02] ingest | Leviasynth fidelity P9e (transport tap tempo)
- Disposition: New (continues the P9 record); Update (pins the P9e and skin-fix commit hashes; closes the P9e lane-proof box as deliberately deferred; records the mutation-run lessons and the t92-list-vs-t93-pixel pin distinction)
- Raw: llm-wiki/raw/articles/2026-10-02-leviasynth-fidelity-p9e-tap-tempo.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-02] ingest | Dell lane: the ABIv1 link gate is not the Dell, and `--get` lies about open files
- Disposition: New; Update (corrects the standing assumption that a green `scripts/ri_audit.sh` AROS link gate is evidence the change runs on the Dell; corrects the standing assumption that a `--get` on `RAM:RIAPP.LOG` shows current contents)
- Raw: llm-wiki/raw/articles/2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md
- Updated: llm-wiki/index.md (Lane infrastructure entry list)

## [2026-10-02] ingest | The xrun fix worked and the symptom did not close: repaint count fell 98.6 %, repaint cost did not move
- Disposition: New; Update (Disputed ×2 + Outdated ×1 on `2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md` — the on-target proof that record left open is answered, and the priority-inversion conclusion it drew is overturned in favour of a per-repaint cost); Update (correction: `2026-10-01-menu-hang-tab-artifacts-load-governor.md` pinned `AU_LIVE_PRI` 10, which contradicted the tab-switch record and the tree; restated as 10-as-recorded / 21-now); Update (both 2026-10-02 rows' neighbours re-statused)
- Raw: llm-wiki/raw/articles/2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md
- Updated: llm-wiki/index.md (Live app entry list)
- Triggered by: the owner's report against `RAM:RIPP9F` ("the GUI is way less responsive and audio playback is choppy — this wasn't the case with claude's fix"), which is a *different* symptom from the one `3dadda7` targeted. Evidence required quitting the app first, because a process holds its own log: `--ui-close` on the RIAPP window, then a bulk get of both logs.
- Note for the next session: the `draw:` heartbeat line has **two** fields named `n=` (full repaints, then partial). A keyed parse keeps the last one and silently inverts every repaint comparison. This produced a confidently wrong first reading of the owner's session and is the reason the previous record's 1.47 ratio is now 1.12.

## [2026-10-02] ingest | Advisor review of the xrun record: the mechanism is withdrawn, the numbers are not
- Disposition: Update (self-review, same day). Three claims withdrawn in `2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md`; two Status blocks added there and one in `2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md`.
- Updated: llm-wiki/index.md (Live app row for the 2026-10-02 xrun record)
- Why this is logged separately from the ingest above: the record was wrong, not just incomplete. "The diagnosis does not hold at priority 21" was correct — a pri-21 task cannot be pre-empted by the GUI, the claimed blocking point was never named, the 303 ms tail may be a symptom of the stall it was cited as the cause of, and 407 dropouts in 7.7 s (~28 % of buffers) never reconciled with 0.6 % of a core. The earlier record also mis-read "excludes saturation" as "excludes per-event cost". The two claims that held (repaint cost did not move; the pri -1 fallback was suppressing it) are kept in their weaker form.
- Two of the three advisor's factual claims were themselves wrong and were refuted from the repo, not from memory: `4167e32` is already an ancestor of HEAD and on `origin/main`, and its stall cap and the arm are different layers rather than duplicates; and the ev log's RUN line reads `build=3dadda7`, so the priority-10 worry does not apply.
- Artefacts lost: `/tmp/opencode/fix_f*.log` went with the 07:48 host restart and `RAM:` went with the owner's reboot. The figures are in the record and in `58b00c4`; the raw bytes are gone. Log destinations need to be sticky (`Vk4aros:` on the USB stick, not `RAM:`) before another on-target run.
- Followed by `5940cc3`: the metrics the diagnosis lacked (wake latency, repaint reason).

## [2026-10-02] ingest | Sticky log destination (`Vk4aros:`), and the size heuristic retired
- Disposition: New; Update (`2026-10-02-dell-lane-abiv11-build-and-bulkget-staleness.md` marked Outdated on its size table — the `-O2` label on its own row is wrong and size cannot separate the lanes)
- Raw: llm-wiki/raw/articles/2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md
- Updated: llm-wiki/index.md (Lane infrastructure entry list)
- Canonical location, now documented: `RIAPP.LOG` and `RIAPP-EV.LOG` go to the first mounted USB/stick volume — `Vk4aros:` on `USBSCSI0P1:` (30.0G VFAT) on the Dell — with `RAM:` as the fallback only, and `RIAPP_LOG=<vol>` / `RIAPP_EVLOG=<vol>` to pin either. One shared header (`platform/pal/ri_pal_sticky.h`) holds the candidate list so the two logs cannot disagree about it.
- Proven on-target as `RAM:RIPLOG` (`RUN frames=256 vol=Vk4aros: build=0c2ba7f`): the log is on the stick AND pullable while the app is running, so it is durable and live rather than locked and then transient. Idle close `xruns=0`, with the new wake-latency fields populated.
- Lane-heuristic correction, measured on one tree rather than re-read: v1 = 1088384 B / `r12moves` 0 (faults), v11 `-O0` = 1081512 B / 41 (runs), v11 `-O2` = 867320 B / 285 (runs). The previously deployed `RAM:RIPP9F` was an **`-O0`** build, so **the A/B must run both arms at the same optimisation level** and the baseline stays `-O0`. Size bands are retired as a discriminator: the v1/v11 gap is 6872 bytes.
- `T:` is NOT a durable location on this guest and is excluded from the probe list: `dir T:` and `dir RAM:T` both return the same single `Tmp...` file, i.e. `T:` is a RAM-backed scratch assign. Recording it because it looks like the answer next to a wiped `RAM:` and would have preserved the status quo while appearing to fix it.
- Second instance in two days of a test that could not include the thing it tested: `t153` first mirrored the volume table because `fs_aros.c` is AROS-only, and all eight mutants of the real file survived. Moving the table to a shared header is what turned the set into 5/5 behavioural kills.

## [2026-10-02] ingest | Scripted A,B,B,A: the arm wins, the repaint policy regresses, and the xrun cause is the governor
- Disposition: New; Update (`2026-10-02-dell-xrun-proof-result-and-the-cost-per-repaint.md` — both Status blocks upgraded **Disputed -> Refuted by measurement**; `2026-10-01-dell-xruns-governor-arm-and-repaint-policy.md` — Refuted status added)
- Raw: llm-wiki/raw/articles/2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md
- Updated: llm-wiki/index.md (Live app entry list)
- Evidence: `~/Work/vms/ri-p9/logs/ab-2026-10-02/` — 4 ev-logs + 4 main logs, sha-verified. Arms `4167e32` vs `b68443c`, both `-O0`, same built-in song, five tabs each, clicks confirmed per click against the ev-log.
- Result: xruns **618/620 → 274/274**, `overloads` **8 → 0**, `render_max` **85-136 ms → 6.4 ms**, five-tab total **120 ms → 279-294 ms**. Within-arm spread ~0.3 %; B's two runs were identical.
- Consequence for the two fixes shipped as a pair: **Fix A (arm) is a large win, Fix B (repaint policy) is a GUI regression of 2.3x on tab switches.** Either alone would have been reported as a partial success; together they read as "partially fixed".
- **The prior record's causal claim is now inverted, not merely doubted:** B does 2.3x more repaint work and has 56 % fewer xruns, so repaint cost is not the cause — the governor is. And **no tab switch caused a dropout in any of the 20 switches** in either arm, so the 2026-10-01 tab-switch xruns remain unexplained.
- **The worst error of the episode, named precisely:** "0.6 % of a core" was an *idle* average (a 7 h session that was ~99 % idle) applied to a playing session. Over playback it is **53-54 % of a 5333 us buffer period**, mean wake latency **881-894 us**, worst wake **5802-5810 us** — a full period late on its own. An average over a mostly-idle window describes the idle case; quoting it about a busy one is a category error no unit or formula reveals.
- `arm_us=261317` alongside `overloads=0` (B2) is the reason the field was added: the load was continuously over budget for 261 ms and the arm reset it, indistinguishable from "never over budget" without the counter.
- Follows: bound the damage-box `build_dl` (Fix B's cost), and decide the stall cap's fate knowing it is **not** sufficient alone. Protocol gaps recorded: ~0.7 s tab gap rather than the advisor's 4 s, and the built-in song rather than ZN.

## [2026-10-02] ingest | Bounded damage-box build: 1.9x less GUI work, xruns unmoved (which is the result)
- Disposition: New; Update (`2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md` — its Fix B finding is now acted on and measured)
- Raw: llm-wiki/raw/articles/2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md
- Updated: llm-wiki/index.md (Live app entry list)
- Evidence: `~/Work/vms/ri-p9/logs/ab-2026-10-02/` — C1 unbounded vs D2/D4 bounded, same harness and script.
- Result: box repaint average 5149 -> 2730 us (1.89x), step-lamp work per window 1.35 s -> 0.70 s (1.92x), **xruns 272 -> 266 and wake_max unchanged at ~5.8 ms**. The GUI half of the owner's complaint is measurably better and the audio half is exactly where it was, which is what the A,B,B.A predicted.
- **A metric that reads all zeros is more often broken than absent.** The repaint-reason split reported `0/0/0` in all three buckets for four runs: `draw_frame` cleared `d->dmg_why` before the timing block read it, and NONE was not a printed bucket. Two cancelling failures and it read as "nothing happened".
- Equivalence of the clip is exact, not approximate: `replay_dl_dmg` already skips every command for which `ri_dcmd_hits_box` is 0, so culling on push yields the same surviving stream; no clip set means the push path is byte-identical, and t92/t93 did not move.
- Exclusions recorded: **D1 was a transient guest stall** (512 ms tab switch with `xruns+7`, clean on re-run as D2) — one run is not evidence even when it looks dramatic. **D3** had only one heartbeat window. n=1 unbounded vs n=2 bounded.
- Third instance of this lane's structural trap: an AROS-only caller cannot be reached by any host test, so a mirrored test survives every mutant of it. `mut_fixM4`/`mut_fixM5` are restricted to host-compiled code and the AROS wiring is proved on target.
- Open: the xruns are still undiagnosed; `wake_max` ~5.8 ms against a 5333 us period with the render task at 53 % of a period; tab-switch latency unchanged (a tab switch is a full repaint); `build_max` got worse (121552 -> 310630 us) and is unexplained; the guest shows intermittent 300-500 ms stalls that confound every measurement.

## [2026-10-02] ingest | Lane A/B procedure and the testability boundary (triage of the unrecorded remainder)
- Disposition: New (two records); Update (`2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md` and `2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md` — both now cite the consolidated standing rule instead of each rediscovering the trap)
- Raw: llm-wiki/raw/articles/2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md; llm-wiki/raw/articles/2026-10-02-testability-boundary-aros-only-code-and-mirrored-tests.md
- Updated: llm-wiki/index.md (Lane infrastructure entry list)
- Triage basis: the four most recent records were already ingested and indexed, so this pass looked for knowledge that existed only in scattered mentions. Two things did. `ab_run.sh` had **zero** hits anywhere in the wiki, and the verified click coordinates were scattered across five files with no single place to look them up. The AROS-only testability trap appeared in five articles as a passing remark and had bitten three separate times without ever being written down as a rule.
- **Discipline point worth recording:** the operational knowledge was the most expensive kind to acquire — three failed probe attempts, a run that silently started nothing, a truncated ev-log from a duplicate instance — and it was the most fragile, because it lived in one session's transcript. That asymmetry is why it needed a record rather than another code comment.
- The second record is the one most likely to save time elsewhere: "a green suite in this tree means the host-compiled parts are covered" is not currently written down anywhere, and it is the claim a future lane would most naturally over-rely on.

## [2026-10-02] ingest | Render-stage breakdown, and the optimisation level that removes every xrun
- Disposition: New (two records); Update (`2026-10-02-dell-scripted-ab-abba-governor-arm-wins-repaint-policy-regresses.md` and `2026-10-02-damage-box-bounded-build-1p9x-less-gui-work-no-xrun-change.md`)
- Raw: llm-wiki/raw/articles/2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md; llm-wiki/raw/articles/2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md
- Updated: llm-wiki/index.md (two new entries; the A,B,B,A and damage-box entries now carry the 86 % and 37 us corrections)
- Triage basis: the standing question was "where do the render's 2806 us/buffer go", and every answer so far had been an aggregate — one of which had already been caught being an idle-session average. So the measurement was built first and then distrusted, which turned out to be the right order twice.
- **Result:** the voice render is 92.4 % of the cost and the FX chain is 0.9 %, refuting the standing suspicion about the FX chain at both optimisation levels. DSP itself is 98.4 % of the render, so the sequencer is not hiding anywhere.
- **Discipline point that cost the most and was worth it most:** the instrumentation was measuring itself. 79 `ReadEClock` calls per buffer, 3.8 us each — not the sub-microsecond read assumed — found only by stashing the instrumentation and re-running the identical script. Without that control every absolute figure would have been 6.4 % too high and nobody would have known.
- **The correction that reaches furthest:** the A,B,B,A's "53 % of a period" was itself a session average over 39 %-idle runs. `render_max` agreeing at 6310-6359 us across every `-O0` run is what proved the workload was constant and the average difference was protocol idle time. The record that withdrew "0.6 % of a core" replaced one session average with a less idle one; the honest playing figure is 86 %.
- **Refuted rather than confirmed:** the owner's standing suspicion about the FX chain. Also worth recording that a zero-width stage is invisible to a synthetic per-read clock — both real bugs this session (the shared wrapper timestamp, and `EVENTS` opened and closed together) were caught on the device and by exact tick arithmetic, not by any law about stage width.

## [2026-10-02] ingest | The Knife (Genesis) as deep house: a faithful-progression re-cut
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-10-02-the-knife-deep-house-song-cut.md
- Updated: llm-wiki/index.md (one new entry)
- Owner request: "Take 'The Knife' by Genesis and create a modern Deep House version... progression faithful, arrangement changed, no vocals."
- **Rights:** the 2026-09-30 decision covers this case unchanged — the repo is public and this is someone else's music, so the arrangement stays in the git-ignored `songs/local/` and goes only to the Dell. Nothing about the song is committed.
- **The research mattered more than expected.** Songparts gave the key (G# minor = Ab, 135 BPM, 4/4, the *Abacab* studio version) and nothing else did: the two public chord transcriptions contradict each other (live = C minor 146, "official audio" = a different seven-chord set). The MIDI transcription (Simon Goodwin, midi-karaoke.info `20e6a739.mid`) carries the actual harmony in its organ and bass tracks and overruled both sheets.
- **What the song is, once folded into blocks:** almost everything sits on an Ab pedal. One four-stab figure (Eb-Ab-B / E-Ab-Db / Eb-Gb-Bb / Eb-Ab-B) is the hook; the blocks are the Ab pedal, then Bbm+bVI on Bb, then Cm/Fm and the bright Cmaj7/Am7 on C, then the bridge's C-Bb-G-Gb-F-Eb walk and its Ab-Gb-E-Bbm descent, then the Bbm/Ebm chorus with an Ab-F bass turnaround, then the "we have won" line. So "faithful progression" for a house re-cut means keeping the pedal blocks and moving the tempo, not re-harmonising.
- **The tool gap:** `tools/mid2rbs.py` maps a contiguous MIDI bar range onto a contiguous song bar range, which can transcribe but cannot re-cut. `arrange.py` wraps it per section and rebuilds the song track, so the committed tool stays untouched.
- **The bug worth remembering:** the wrapper's first version emitted mid2rbs' own slot numbers instead of its global ones. Because the slot is baked into each pattern line, dedupe and numbering both still "worked", and the render played happily — with no bass at all in bars 32-92. It was found by pitch-tracking a drums-muted render, not by any compile, render or error. Fix: re-point the slot inside every emitted line, and normalise the slot out of the dedupe key.
- **Verification order that was worth it:** headless render -> per-bar level/brightness (caught an outro louder than the drop, twice) -> drums-muted harmonic-sum pitch class per bar (caught the bass bug and now proves the progression) -> pattern-data diff against the source MIDI (the hook slots decode note-for-note to the flute bars).
- **Two findings for the tool backlog:** `tools/inspect --rbng` cannot read *any* song with automation (the reader needs a caller-provided ATRK buffer that `inspect` never allocates — it fails on the approved Zombie Nation song too), and the 303 note range folds out-of-range notes by whole octaves silently, so the arrangement map needs per-section transposes.

## [2026-10-02] ingest | Dell lane: the bare-path launch wedges the agent, and --ui-capture can hang
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md
- Updated: llm-wiki/index.md (one new entry)
- Trigger: the owner reported a "cannot load a song" requester on the Dell and asked for a screenshot; the screenshot was unobtainable because the lane itself had wedged.
- **Cause 1 (mine):** `--exec 'Vk4aros:ReIncarnation/RIAPP PLAYLIST=...'` starts the GUI app through the agent's shell and blocks on the child's output pipes. RIAPP never exits, so the identity is stuck inside one action and every later `--exec`/`--ui-capture` queues. The lane's known `Run RIPA` lesson is the mirror image — `Run RIPA` without a volume starts *nothing*, while a bare path starts something that never lets go. Use `Run <volume>:<command>` and nothing else for long-lived programs.
- **Cause 2:** the queued `--ui-capture` then hung for 40+ minutes with a fresh heartbeat and an ESTABLISHED socket, so the session *looked* healthy. `state = busy:<job-id>` naming the same action repeatedly is the only tell. There is no server-side cancel and the queue is serial, so a screenshot is a way to lose the lane.
- **Recovery and its cost:** `kill -USR1 <serve pid>` is the documented operator sweep (`kick_all`) — it closed the socket, marked both stuck jobs done and released the queue. But the guest agent did not reconnect in 5 minutes of polling, and the lane needs a guest-side restart. The agent has reconnected on its own earlier in the day (session 1 → 4), which is exactly why the no-reconnect case must be written down rather than assumed away.
- **The song is exonerated, with the evidence, so it is not re-litigated:** rbsc round-trips it through the same `rbng_read_song` the app calls; songplay renders 203.3 s at peak 0.850 with 0 clipped / 0 xruns and never printed `automation refused`, so all 223 automation events were accepted; `RI_RBNG_MAX_FILE` 65536 vs 15538 bytes, `RI_CORE_AUTO_CAP` 8192 vs 223 events, `RI_PLAYLIST_PATH` 256 vs a 58-char resolved path — every limit has headroom; `ri_playlist_dirname`/`ri_playlist_parse` resolve the Dell-style playlist path to the same three absolute paths the previous day's working playlist produced; and `song_load_path`/`RI_CORE_AUTO_CAP` have not changed since the songs commit, so the older RIAPP on the stick cannot be running different song code.
- **Still owed:** the requester's own text, which `song_fail` writes to the guest log as `RIAPP <what> <path>: <why>`. That is the one piece of evidence that needs the lane back.

## [2026-10-02] correction | The Dell lane is shared — SIGUSR1 is not a private reset
- Disposition: Update
- Raw: llm-wiki/raw/articles/2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md
- Updated: llm-wiki/raw/articles/2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md
- Found while waiting for the guest to redial: **both** opencode sessions on this machine share `/tmp/spike_spool_laptop`. When the reset was spent, five jobs were queued and four were the other lane's — including a `put RIAPP.v11 -> RAM:RIAPP_SEC` and a 190-action run whose first action is a `ui_click`, against a guest that still had a modal requester up.
- Two corrections to the procedure: `kill -USR1` closes the one shared session, so it may only be spent after reading `state = busy:<job-id>` and confirming the stuck job is yours; and a `ui_click` against a guest with a modal requester is the same hang as `ui_capture`, so the other lane's run is the next thing at risk. On a shared lane, diagnose from the log and never from a UI action.

## [2026-10-02] ingest | Lane procedure gains a third silent-failure mode; the section split is built but unmeasured
- Disposition: Update
- Raw: llm-wiki/raw/articles/2026-10-02-dell-lane-scripted-ab-procedure-and-click-map.md
- Updated: llm-wiki/index.md (lane entry)
- Not an ingest of a new source — a cascade from the lane going down mid-task. The record's own title said "the two ways a run silently produces nothing"; it is now three.
- **The new mode:** a disconnected guest agent. `submit` cannot distinguish it from a malformed job — both simply never come back — so a full scripted run printed nothing and looked complete. Only `status` reports it, per identity, as `disconnected pending=N`.
- **The durable lesson, which is about harnesses rather than this lane:** piping every submit through `grep -E "..." | tail -2` to keep the log readable also discards the only evidence that the command failed. A measurement harness that cannot tell "no result" from "no result because the other end is dead" will quietly emit a series of empty runs that read like measurements. Every step now asserts its evidence arrived — the window is listed, clicks were injected, the pulled log is non-empty, the close line exists — and the script exits non-zero instead of continuing.
- **Recorded as blocked, not skipped:** the five-way section split of the voice render is committed, tested and mutation-proven at 34/34, but has no device number. Stating a measurement that was never taken would be the exact error this wiki keeps catching elsewhere, so the gap is written into the record's standing gaps instead.


## [2026-10-02] ingest | MIDI interop R1: follower core, realtime wire parser, sync source
- Disposition: New; Update (`2026-09-29-gui-round3-s1-s3-and-interop-requirement.md`)
- Raw: llm-wiki/raw/articles/2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md
- Updated: llm-wiki/index.md (one new entry)
- **Why an article at all:** the wiki held the interop *requirement* (P1-P4, R1-R12, the realtime constraints, the testing plan, seven open owner decisions) but nothing at all about the three commits that started answering it. `midi_follow` appeared nowhere in the other 198 articles.
- **The finding that shaped the design:** the spec's own inventory says `midi.h`'s `midi_clock_*` and MMC parser are "neither is wired to the transport" — and that is still literally true today. They were built for the Sync LED (a self-advancing simulated clock plus a drift number), which structurally cannot reject a wild tick, hold an estimate through jitter, or latch a single dropout stop. So R1 adds a second model beside the first rather than replacing it, and the two now coexist: only the follower one can drive tempo. Recorded so a future session does not "discover" `midi_clock_*` and assume it is the follower.
- **The seam that kept the slice small:** the core returns intents (`NONE/PLAY_START/CONTINUE/STOP/SEEK`) and never touches a transport. That is why CAMD wiring could be deferred without leaving a mess, and why SPP is a pure scale decision (`b * 24`, 16ths at engine PPQ=96 — pinned at 128 beats -> 3072 ticks).
- **Three laws worth carrying:** rolling mean over 24 intervals with quarter-mean outlier rejection; dropout is a latched *state*, not an event (96 missed intervals, or 2 s cold, -> exactly one STOP, cleared by any traffic); realtime bytes land inside a pending SPP pair while any other status byte aborts it.
- **Two discipline records, both from real incidents in this lane:** (1) the `-Wunused-parameter` stale-object trap — a mutant that breaks an `-Werror` build leaves the previous `.o` in place, the test links it and passes, so the mutant "survives" and the kill proof is worthless; hash-verify `/tmp/ri/build/*.o` across every mutant rebuild (hit twice, in `50dc90a` and `622907d`). (2) a vacuous assert — asserting `tempo == 0.0f` when the default is also 0.0 cannot fail, so an "unlocked-latch" mutant passed; setting the value under test to `999.0f` first made the assert able to fail and the mutant die. Rule: never assert a value that equals its default.
- **Cascade:** the requirement article's "MIDI clock only drives the Sync LED" claim is left standing — it is still true of the shipped app — with a dated pointer that the *core* exists and only the wiring is missing.
- **Still open and unchanged:** CAMD wiring + transport application (next slice, riqemu1 loopback proof per the spec), focus-5 (sibling's keyboard/panel territory, so spec-and-hand-over), clock out / MMC out classification (owner decision 1), latency offset and phase lock.

## [2026-10-02] ingest | r12moves is an inlining counter, not an ABI hazard
- Disposition: New (one record); Update (`llm-wiki/index.md` — the optimisation-level entry's r12moves framing)
- Raw: llm-wiki/raw/articles/2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md
- Triage basis: not an ingest of a source. A cascade from the optimisation record's own caveat — "-O2 doubles r12moves (41 → 286), so not a free win" — which had been inherited as settled without ever being checked. The lane was down, so it was checked host-side.
- **The correction:** the audit gate asserts "guest binaries MUST emit rdx-base calls; any `mov %rax,%r12` means the stale r12 SDK leaked in". The v11 SDK's `aros/x86_64/libcall.h` is autogenerated with **r12 as the library base register by design**, saving and restoring it around every call, and no rdx variant exists anywhere under `src/abi/`. So the invariant is unachievable as written, `-ffixed-r12` cannot suppress it (the register is named in the asm template and in a `register ... __asm__("r12")` variable), and the count rises with optimisation purely because more got inlined: **0 in our objects at `-O0`, 245 at `-O2`, confined to the eight AROS-only platform files that call AHI/Intuition/dos.** No engine or DSP object is affected at any level.
- **Consequence for the standing record:** the size-heuristic record promoted `r12moves` to "the discriminator, not size". It remains a fine build fingerprint — that is genuinely why it separated v1 from v11 — but it is not evidence of ABI conformance and must stop being cited as such.
- **Consequence for the open decision:** `-O2` is blocked on understanding the v11 LVO convention, not on more performance data. That is a more tractable question than the one it replaces, and the four measurements it rests on are unaffected.
- **Boundary held:** the gate's pass/fail logic is deliberately unchanged. Relaxing a gate written after a real fault is the owner's call, and this finding does not identify that fault. Only the incorrect comment was corrected, in place, with the evidence — and `AUDIT 0/0 PASS` still holds because the gated artifacts make no library calls.
- **Method worth keeping:** check the generator, not the generated file. One grep of `gencall.c` settled what three rounds of disassembly had only narrowed, and per-`.o` counts beat per-symbol counts in a linked binary (symbol attribution put instances in a file that compiles to zero).

## [2026-10-02] ingest | Five sections, one culprit: LEVI is 32 % of the render
- Disposition: New (one record); Update (`llm-wiki/index.md` — the breakdown entry's standing gap is now closed)
- Raw: llm-wiki/raw/articles/2026-10-02-five-sections-one-culprit-levi-is-32-percent.md
- Triage basis: closes the open gap the breakdown record left — "VOICES is still one bucket over 303A/303B/808/909/LEVI" — which was the last measurement step the render-cost thread needed before the next decision could be made.
- **Result:** the four drum and 303 engines form a tight cluster (174, 178, 185, 198 us/block, 11 % spread) and LEVI is 398 — twice the largest of them, 32.0 % of the whole render, with the heaviest tail (max/avg 1.94, worst single block 59 % of a buffer period). At `-O2` every section speeds up by the same 2.2-2.4x, so the optimiser is not selective and there is no per-section optimisation decision to make.
- **The negative result is the useful one:** uniform speedup across all five engines rules out both "already fine" and "pathologically unoptimised", so the earlier framing of the optimisation question as needing per-section evidence was wrong.
- **Recorded as a correction, not a footnote:** three identical `-O0` play-only runs gave 2320 / 3116 / 3678 xruns, a 1.58x spread never written down before. Earlier quoted `-O0` figures carried an unstated uncertainty of that size. The `-O2` result is robust because it is zero (0/0/0) rather than merely small — which is a reason to prefer a zero-result experiment over a small one when both are available.
- **Two host-side bugs the tests caught that the device could not have named:** the wrapper sharing a timestamp variable with the stages nested inside it, and a stage closed without ever being opened — the latter invisible while the preceding stage had adjacent reads, and exposed only once sections were enabled one at a time.


## [2026-10-02] lint | 24 issues found, 20 auto-fixed — the Grounding Invariant had never been checked
- Disposition: lint (no new source). Scope: the whole wiki, plus a mechanical grounding pass over the four records written earlier today.
- **Mechanical, all clean:** 201 articles, 0 missing from the index, 0 dead index entries, 137 internal links with 0 broken, 24 Status blocks and none undated.
- **The finding that mattered: nothing had ever verified the Grounding Invariant** for the records written earlier in the day. The skill's `check_evidence.py` expects a `wiki/` + `raw/` layout and this project uses `llm-wiki/` with the records themselves in `raw/articles/`, so the script does not run here and the equivalent check had simply never been done. Written and run: every integer and decimal literal in the four new records grepped against the cited raws.
- **Result: one real citation gap, and one arithmetic error.**
  - The breakdown record quoted the A,B,B,A's B1/B2 buffer and `render_total` figures while citing only the `stg-2026-10-02/` log directory. Those numbers live in `ab-2026-10-02/`. Fixed — the citation now names both directories, and `8515` / `23895` verify verbatim in `B1.stick.log`.
  - Its idle-fraction arithmetic was stated as exact: `2806 = 0.61 × 4590 + 0.39 × 9`. The right-hand side is **2803.4**, against a measured `23895000 / 8515 = 2806.2`. Corrected to show the 2.6 µs and attribute it to the rounding in a 39 % fraction rather than leaving an equality that does not hold.
  - Everything else "unfound" was **derived** — shares, ratios, blocks-per-buffer, idle fractions — and the skill permits those provided the components are findable. Each was checked to have its components printed, and each record now carries an explicit `- Grounding:` line saying which figures are verbatim and which are derived and how. Three records have one.
- **Auto-fixed:** three records still said `Commit: unpushed at collection` after being committed; all now name their commits. The r12moves record cited no source at all and now names the two SDK files and the counting method.
- **Judgment report — two real contradictions found and marked, both mine to fix and neither previously flagged:**
  - **The size-heuristic record contradicted itself and the audit.** It claimed `r12moves > 0` is the v11 lane's "reliable discriminator" and that this is "the same check the audit's ABIv1 gate applies" — but the gate requires **zero** and fails the build otherwise, so it treats the value the record calls the lane's signature as a failure. Marked **Disputed**, with the r12moves finding that the count is an inlining counter rather than ABI evidence.
  - **That same record's "optimisation level changes CPU cost by ~24 %" is the *size* delta read as a cost.** `(1082824 − 873888) / 873888 = 23.9 %` is how much smaller the `-O2` binary is. The measured CPU difference is **2.4×** (4590 → 1936 µs corrected, xruns → 0). A reader taking "24 %" at face value would badly under-weight a confounder that the sentence exists to warn about. Marked **Disputed**.
  - **The xrun-proof record's "0.6 % of a core, so it is *late*, not *crowded*"** had no Status block at all — its two existing blocks cover the suppressor reading and the advisor's objections, not this. It is the same idle-average error the A,B,B,A record corrected, one level further from the measurement, and the conclusion inverts: 86 % of the period, **crowded, and too long for its own period**. Third block added, Refuted by measurement.
- **Cascaded into the lane record** from the *other* session's findings, which apply directly to my harness: the spool is **shared**, so `kill -USR1` is not a private reset and a queued `ui_click` can wedge the lane for someone else; and **`--get` can serve stale bytes**, so a stale pull is a well-formed log from the *previous* run and every structural check passes on it. The stage harness now clears both local destinations before pulling and asserts the log is non-empty and carries a close line. **Verified rather than assumed:** all seven stage runs carry distinct close lines (buffers 40208 / 39102 / 40812 / 40840 / 40705 / 38038 / 40927), so no number in those records came from a stale read. The record's heading moves from "two ways" to "four".
- **Not fixed, reported:** 153 articles have no inbound link from another article. That is a systemic property of the older research records rather than 153 separate problems, and it is not actionable as written.

## [2026-10-02] ingest | AROS does not resolve `..` — the playlist entry failure
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-10-02-aros-does-not-resolve-dotdot-playlist-entry-failure.md
- Updated: llm-wiki/index.md (one new entry)
- Owner report: "there's a requester up on the Dell, stating that it cannot load a song." Resolved to one line of app log: `RIAPP song Vk4aros:ReIncarnation/songs/local/../demo/riapp-demo.rbng: open failed`.
- **Cause:** `ri_playlist_dirname` resolves entries by plain concatenation and `read_file` (`project/rbng.c:117`) `fopen`s the result; AROS does not canonicalise a `..` component. Proven on the guest with CLI only — no GUI action: `type .../songs/demo/riapp-demo.rbng` -> rc=0 FORM, `type .../songs/local/../demo/riapp-demo.rbng` -> rc=10 object not found, `list .../songs/local/../demo` -> rc=20 object not found.
- **Why it survived since 2026-09-30:** it is not a startup failure. Entries 0 and 1 load and play, so "the playlist loads" is true and the demo looks healthy; the failure only appears on auto-advance past the last song, ~7m45s in. The 2026-09-30 record was accurate and still hid it. **Lesson: a playlist that only fails on its last entry is indistinguishable from a working one in any test that does not run past the end.**
- **How it was caught:** polling the window count once a minute — 3 windows steady, then 4 at 17:12:05, matching 7m45s after a 17:04 launch. Bisected with a knife-only playlist, which loaded (104 bars, 124 BPM) and so killed "the song is broken"; the three-entry playlist loaded entry 0 and then failed 7m45s in, killing "the playlist is malformed"; the remaining suspect was the entry path, and two `type` commands settled it. **The requester's text was never needed.**
- **Fixed and proven:** the playlist no longer uses `..`; `riapp-demo.rbng` sits next to it, all entries are plain names, and the reason is in the file's comment so the next tidy-up does not reintroduce it. Dell proof is a full cycle plus a wrap — `ev 9` knife (104 bars), `ev 17` zombie at t=38139 (151 bars), `ev 25` demo at t=87010 (16 bars), `ev 33` knife again at t=92485 — no `open failed`, no requester, `xruns=0` across 162,746 and 110,640 buffers.
- **Still owed, and it is a fork not a bug report:** `project/playlist.c` still accepts `..`, so the trap stays armed for the next playlist. Either reject at parse time with a message naming the entry, or canonicalise `.`/`..` textually (needs its own tests, and new path logic in a shared file). Owner call; not taken unilaterally.
- **Generalisable:** the host PAL resolves `..`, the guest does not, so a host-only test of the same playlist passes while the Dell fails. Any path-handling change needs a guest check, not just a host one.
- **Diagnosis depended on two things worth keeping:** `song_fail` logs the reason *before* it requesters (`app/riapp.c:814`), so the evidence exists while the app is blocked on the box; and the binary deployed on the stick predates `0c2ba7f`, which is why `RAM:RIAPP.LOG` — and the reason in it — was erased by the cold reboot (`delete RAM:RIAPP.LOG` -> rc=5 "No file to delete").

## [2026-10-02] ingest | riqemu1 up, audio verified, and a record I had pushed turns out to be wrong
- Disposition: New (one record); **Disputed/corrected** (`2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md`, plus its index entry and the comment beside the gate in `scripts/ri_audit.sh`)
- Raw: llm-wiki/raw/articles/2026-10-02-riqemu1-up-audio-verified-no-riapp-and-a-record-corrected.md
- **riqemu1 is up.** Spooler on 9295 listening *before* the guest boots (its agent dials out), then `start_riqemu1.sh`. `-enable-kvm -cpu host`, agent connected in ~10 s as `anon`, guest Kickstart 51.51 / WB 40.0. `vm_restart.sh` is unusable here for three checkable reasons: two hardcoded `/root` paths that do not exist, a `pkill -f qemu-system-x86_64` that would kill the other session's VM, and no `-enable-kvm` (pure TCG, so any CPU number would be meaningless).
- **Audio works, verified host-side rather than assumed:** PulseAudio shows `Sink Input #250` with `application.name = "riqemu1"`, `process.id` equal to the qemu pid, `media.name = pa0`, s16le 2ch 44100Hz — AC97's native rate, streaming into the host stack. On the suggested `-model es1370`: this guest has **no ES1370 mode** (`ac97 NVHDMI CMI8738 HDAUDIO VIA-AC97 SB128`), so it would present a card with no driver. Generic-correct, wrong for this image.
- **No RIAPP runs on it.** The v11 binary uploads and returns rc=0, then the guest raises `Software Failure!` / **`Type: Illegal instruction (?!)`**. Building for v1 compiles and fails at link on `-lstdlib`/`-lcrt`, which is a **C-runtime naming difference between lanes** (v1 `libstdc.a`/`libstdcio.a` vs v11 `libstdlib.a`/`libcrt.a`), plus different SDK roots. `ri_build_aros.sh` only ever built a stub and `probe_ahi` for v1.
- **The correction, and it is the most important line in this entry.** Needing a *different lane* is what exposed the error. `ri_build_aros.sh` uses the **v1** SDK and the audit gates its output at zero — and v1 genuinely counts zero, because only v11 ships `aros/x86_64/libcall.h` (r12=45) and v1 has no arch-specific x86_64 libcall header at all. My r12moves record claimed the gate's invariant was "not achievable in this tree" and that `r12moves` "must not be cited as ABI conformance". Both false: it is a **v1-lane gate**, and `r12moves` is a reliable **ABI fingerprint** (zero = v1, non-zero = v11), which is stronger than what I replaced it with and is how the size-heuristic record used it all along. Only the narrower within-v11 point survives. My error, shipped to `origin/main`, found only by asking a question I had not thought to ask before.
- **A harness hazard, found by its symptom.** `t156` failed on a clock-independence law and the code was innocent: `mut.sh` writes a mutant, runs the test, restores ~20 lines later, so an interrupt in between parks the **mutant** in the tree — which is what happened. Restore now runs in a `finally`, on SIGINT/SIGTERM/SIGHUP and via `atexit`; verified by deliberately interrupting a run. Same shape as the Dell capture wedge: a lane that cannot be interrupted safely will eventually hand you a result you did not ask for.
- **Lane differences recorded before anyone automates here:** guest `wait` is ~8–29 ms versus ~1.06 s on the Dell, which alone invalidates the settle-by-N-waits idiom; screen 1024×768 so the Dell click map does not apply; `--ui-capture` caps at scale 2 and refuses scale 1, which is why the ABI conclusion rests on toolchain evidence rather than the screenshot.
- **Blocked, not skipped:** the Levi sub-split is built, tested and mutation-proven (25/25, with `mut_fixM6` 14/14, audit 0/0, ASan clean) and has no device number, because the Dell belongs to another session and riqemu1 cannot run RIAPP.

## [2026-10-02] ingest | HEAD misses the audio deadline on a real song
- Disposition: New
- Raw: llm-wiki/raw/articles/2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md
- Updated: llm-wiki/index.md (one new entry)
- Found while deploying the owner's two decisions (canonicalise `..`, log fallback `T:`): the repo HEAD binary had **19,703 xruns over 94,489 buffers** on the Dell playing a real five-device song, with `stg_dsp_avg=5626 us` against a 5,333 us buffer period.
- **Controlled, not asserted:** the 00:15 binary (`RIAPP.prev2`, kept as the control) was run on the identical playlist minutes later and gave `xruns=0 render_max=3679 us` — while failing the `..` entry with `open failed`. So the fix works and the xruns are a property of the binary, in the same pair of runs. Not the song, not `project/playlist.c` (reached once, before any audio), not `RI_PAL_STICKY_FALLBACK` (never consulted here — `Vk4aros:` mounts), and 0-vs-19,703 is far outside the guest's own documented stalls.
- **The alternative explanation is stated, not buried:** the old `render_max` is *uninstrumented*, so DSP-got-more-expensive vs instrumentation-cost-in-an--O0-debug-build is unresolved from this lane. What is not unresolved is that HEAD does not fit the deadline as it stands.
- **Why it matters beyond the number:** this is the first on-device figure for HEAD. Every earlier xrun number in the wiki came from the 00:15 binary, so the whole earlier campaign measured something that is no longer on the stick.
- **Handover, not a fix:** the record names the render-stage and governor lane as the owner of the next step, gives the two-run reproduction, and notes that HEAD stays deployed with `RIAPP.prev2` one `copy` away for a clean listening session.
- **Left on the stick:** `songs/local/dotdot.rbpl` — entry 3 is `../demo/riapp-demo.rbng` on purpose with no same-directory copy, so it is simultaneously the `..`-fold regression fixture and a repeatable five-device song for the render work.

## [2026-10-02] ingest | One requester, two instances, and a silent null backend
- Disposition: New; Update (`2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md`, whose revert advice caused this incident)
- Raw: llm-wiki/raw/articles/2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md
- Updated: llm-wiki/index.md (one new entry; the superseded entry's summary now flags itself)
- Owner report: the "Cannot load song" requester was up again, 40 minutes after the `..` fix was deployed and **proven on hardware**. It was not a regression.
- **Cause, from the guest's own log:** `Process 8 Loaded as command: Vk4aros:ReIncarnation/RIAPP.prev2` — the pre-fix binary — on `dotdot.rbpl`, whose entry 3 is `../demo/riapp-demo.rbng` on purpose. Hence `open failed`. The trap was mine: I had left the old binary beside `RIAPP` as "the clean-audio one" AND left a fails-by-design fixture on the device next to the playlists you actually listen to.
- **The finding worth keeping: AHI contention fails silently.** Launching a second instance gave `audio: AHI unavailable - null backend active (offline render only) [err 4]` and — verified in `app/riapp.c:1839` — **that path only logs**. The window opens, panels draw, the playlist loads, the transport plays, and there is no sound and no complaint. Strictly worse than a requester, and the exact failure mode this project has a rule against ("a silent failure reads as nothing happens" — the reason `song_fail` exists).
- **Two corrections to my own diagnosis, recorded because both pointed the wrong way:** the requester belonged to process 8, not to mine (there was exactly one), and the AHI failure produced no requester at all — it produced silence.
- **Evidence hazard:** `RIAPP.LOG` is `MODE_NEWFILE` at startup, so the second instance truncates the first's log. Under contention the log cannot answer the question, because the run you want is the one erased. Close every instance, then pull.
- **Two close-path traps:** with a requester up, window index 0 is the *requester* (`[no-close]`, 391x123) and index 1 is the panel — so the documented `--ui-close "#0"` pre-flight closes nothing and then launches a second instance. And `--ui-close` on the panel returned `ok (727 ms)` while both windows persisted; the process is wedged on its own modal box. Both the success and the refusal of `--ui-close` are ambiguous here; only `--ui-windows` + `status` settle it.
- **A third defect of mine, found while checking:** `songs/local/demos.rbpl` named `riapp-demo.rbng` in its own directory, and that file existed only on the Dell between one upload and the deletion I did for the `..` fixture — never in the host directory. The playlist was broken as written. Fixed by making the directory self-contained, which buys more than the fix: with no `..`, `demos.rbpl` loads on **every** binary, including the old one.
- **Fixture and naming hygiene:** `dotdot.rbpl` is removed from the device (it has served its purpose; it lives on the host), and `RIAPP.prev2` is renamed `RIAPP-old-no-dotdot-fix` so it cannot be launched by accident. A binary's name should say what it lacks, not how old it is.
- **Cascade:** the earlier article's one-command revert is marked **Status: Outdated** with the date and the reason, because leaving that advice standing is what produced this incident. The index entry that carried it now flags itself as superseded.
- **Still open:** `Process 8` holds a modal requester that repeated `--ui-close` did not dismiss and remains in `status`. Clearing it needs a guest-side `kill` (not reachable through the lane's CLI) or a reboot.

## [2026-10-02] ingest | The riqemu1 Software Failure: two faults, one fixed, one an upstream miscompile
- Disposition: New (one record); **Disputed/corrected** (`2026-10-02-riqemu1-up-audio-verified-no-riapp-and-a-record-corrected.md`)
- Raw: llm-wiki/raw/articles/2026-10-02-riqemu1-software-failure-is-two-faults-wrong-abi-fixed-and-misaligned-movaps.md
- **One title, two independent causes.** Fault 1 **fixed**: the v11 binary was the wrong ABI for this v1-lineage guest, and the repo already ships the right recipe — `scripts/ri_build_aros.sh riapp` gives 1099600 B at **`r12moves = 0`** (v11 was 41) and the panel then opens and renders. Fault 2 **not fixable here**: AHI cannot open on this image.
- **Two corrections to a record written hours earlier, both mine.** The crash is a **privilege violation**, not an illegal instruction — the guest's capture ceiling (scale 2, 512×384; scale 1 refused) makes the two indistinguishable in a screenshot, and the full report as *text* settles it. And the **r12 explanation is wrong**: AROS's `arch/x86_64-all/ABI_SPECIFICATION` specifies **R12 for base** and documents `-ffixed-r12` as the sanctioned mechanism, so v11's r12 sequence is the documented ABI. The "stale convention" wording in `ri_build_aros.sh` does not match upstream — flagged, **not changed**, because it sits beside a gate that is demonstrably right and rewording it is an owner call.
- **The mechanism, established by experiment rather than inference.** All modes present → the report names **`sb128.audio DriverInit`**. Quarantine the five faulting drivers → the fault **moves into `ahi.device ReadConfig`**. Both are `movaps` on a misaligned address (both constants end in `c`) → **#GP**. So it is not driver selection: it is **miscompiled binaries on this image**, reproducible with `probe_ahi`, which contains no ReIncarnation code. Image-specific, not AROS-wide — the Dell's AHI works and every xruns measurement depends on it. Quarantine only moves the fault, so **all eleven renames were reverted and the guest restored** to 7 drivers / 6 modes, verified.
- **Upstream checked, as asked: there is no ES1370 AHI driver.** Full driver set enumerated from the AROS tree; `-model es1370` would present a card with no mode. `ac97` is correct for QEMU's `-device AC97`, and AROS forum guidance agrees.
- **An unproven change was written and reverted.** `NOAUDIO` (skip the AHI attempt at `app/riapp.c:1832`) is two lines beside the existing `CAPTURE=` parsing, but it did not clear the requester and the log was unreadable behind it, so it could not be verified as honoured. Reverted rather than shipped.
- **Method worth keeping:** a requester's title is not its diagnosis — get the report, not a picture of it. A binary's own build script is the recipe (I reinvented it and got the C runtime wrong twice). Compare the crash module *before and after* an intervention; that single comparison is what proved the problem was the binaries rather than driver selection, which four earlier experiments had not reached. And a **9.9 s stall in a bulk `rename`** (`cmi8738.audio`, `HDAUDIO`, against 7–22 ms for every other file) was pointing at the culprit the whole time.
- **Standing:** riqemu1 is a GUI/render-cost lane, not an audio lane, until `ahi.device` is rebuilt AROS-side. That blocks the Levi sub-split measurement on any lane while the Dell belongs to another session.

## [2026-10-02] update | The wedged process cleared itself, and the xruns are the song's DSP cost
- Disposition: Update (`2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md`); no new article -- the findings close the two items that article left open rather than starting a new thread
- Owner instruction: "commit and push, use all relevant skills." Nothing was unstaged (the previous turn had already committed and pushed `0e02b22`, tree clean, `origin/main..HEAD` empty), so rather than manufacture a commit this entry records the verification that was genuinely outstanding.
- **The wedged `Process 8` resolved itself.** Gone from `status`, no RIAPP window, AHI free on the next check -- consistent with a guest reboot. No guest-side `kill` was needed, which corrects the pessimism in the earlier record: *the lane has no `kill`* is not automatically a dead end, because a process stuck on its own modal box may not stay stuck. Worth knowing before concluding a wedged guest needs a reboot.
- **The owed verification, run properly this time.** Confirmed no RIAPP window *before* launch, then confirmed **one** instance *after* it (`Process 8 ... Vk4aros:ReIncarnation/RIAPP`) rather than assuming -- the mistake that produced the original incident was launching on assumption. Verified no requester at the ~5 min midpoint, then closed and only *then* pulled.
- **Result: HEAD + `demos.rbpl` works as a listening session.** 3 songs plus the wrap (`the-knife` 104 bars @124, `zombie-nation` 151 @140, `riapp-demo` 16 @140, `the-knife` again), `AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us` -- the **real** backend, not the null one -- and no `open failed`, no requester, no error line anywhere in 37 KB of log. This proves the self-contained `demos.rbpl` fix on hardware: `riapp-demo.rbng` resolves out of the directory the playlist actually names.
- **A read-the-per-action-`rc` confirmation.** `delete Vk4aros:RIAPP.LOG` returned `rc=5` ("No file to delete") and the **job verdict came back FAIL** while every action that mattered, `Run` included, was `rc=0`. A non-zero from a cleanup step that was not needed is not a failure of the run. Already recorded for `delete` on a missing file; now confirmed for the job-level verdict, which is the trap -- it invites reading a good run as a broken one.
- **The xruns, with every confounder removed.** This playlist has **no `..`**, ran on a **single** instance, with AHI **live**. So: not the path bug, not contention, not the `..` fixture. `RIAPP closed: buffers=97444 xruns=20998 render_max=21573 us render_total=563088 ms period=5333 us stg_total_avg=5766 us stg_dsp_avg=5689 us stg_evt_avg=29 us`.
- **The single clearest number in the investigation:** `stg_total_avg=5766 us` against `period=5333 us` -- the **average** buffer is 8 % late. Nothing in scheduling, the governor arm, the repaint policy or contention can explain an average that exceeds the budget; and `stg_dsp_avg=5689 us` vs `stg_evt_avg=29 us` puts the cost in DSP, not in the instrumentation wrapped around it. Two stages carry it (`stg[5]` 5658 us, `stg[7]` 5735 us); largest single device is Levi at avg 747 / max 15710 us.
- **Second clean run confirms the first:** 19,703 xruns / 94,489 buffers (the `..` fixture) versus 20,998 / 97,444 (`demos.rbpl`). Same order, same story. **It is the song's DSP cost on this hardware**, owned by the render-stage lane -- not the path fix, not the log fix, not this lane.
- **Consequence for the recommendation, now stable:** launch `RIAPP` (HEAD) *and* expect xruns on a real song. Those two facts are independent, which is what makes the advice hold up: the old binary has clean audio but a silent path bug, and HEAD has the fix but a loud, owned, render-stage xruns problem. Neither is a reason to switch.
- **Housekeeping:** `wt-A` (the sibling worktree, detached at `4167e32`, now prunable) is gone, so `app/riapp.c` is uncontended on `main` -- relevant only because the AHI-contention gap recorded earlier would land there. Not touched: the owner asked to commit, not to add behaviour, and per the guidelines an unrequested feature is not a commit.

## [2026-10-02] correction | A count-based index check gives a false PASS
- Disposition: Correction to a verification claim made earlier in this same session; no article content changed
- **The false pass.** Checking the index with `grep -c 'raw/articles/'` returned **206**, against **206** article files -- an apparent perfect match, which is what I reported as "index in sync". It was not. Only **204** of those references are actual row entries (`^- [raw/articles/...`); the other 12 are inline cross-links *inside other rows' summaries*, and they happened to make the totals coincide.
- **The check that finds it is per-article membership, not counting:** for each article file, `grep -q "$b" llm-wiki/index.md`. That reports two articles with **no index row of their own**:
  - `2026-09-24-codebase-review-rebirth-fidelity.md`
  - `2026-09-25-g5-key-precedence-decision.md`
  Both are substantive, both carry no `Status: superseded` / redirect marker, and neither is mentioned inline either. The first uses the *older* article header schema (a `> Source:` blockquote plus a `Full document:` link out to `docs/`), which is the likely reason it was never indexed -- the newer batch was generated against the newer shape.
- **Left unfixed on purpose, flagged to the owner.** The gaps are from 2026-09-24/25, i.e. another era of content in a file both lanes share, in the `## Reviews` and `## GUI (§12.10)` regions -- neither touched by the sibling lane's `bbd2018`. Writing index rows means writing summaries of articles this lane did not author, which is beyond "commit and push" and is an owner call on a shared file. Recorded rather than silently done.
- **Method, kept for the next ingest:** (1) verify the index by **membership per article**, never by comparing counts; (2) distinguish row entries from inline links by anchoring at `^- ` before believing a duplicate is a double-listing -- three targets initially looked double-listed and all three turned out to be inline cross-links, i.e. a second false alarm from the same counting shortcut.
- **Also confirmed this pass:** 206 articles, **0 broken links** across every article, 0 index rows pointing at a non-existent file.
- **Sibling-lane interleave:** `bbd2018` ("the riqemu1 Software Failure is two faults, one fixed", two articles) landed on top of `0e02b22` while this lane was running on the Dell. The article count moving 205 -> 206 mid-session is what surfaced it. Checked before staging: this lane's diff is exactly three wiki files and contains nothing of theirs.

## [2026-10-02] ingest | Ground the riqemu1 crash literals, cascade the r12 correction, lint
- Disposition: **New (raw source)**; **cascade updates** to two existing records
- Raw: `llm-wiki/raw/evidence/2026-10-02-riqemu1-ahi-privilege-violation-reports.md` (new; indexed in the "Ingested articles" section, since raw sources get their own row like `raw/aros-dev-portal/`)
- **Why this ingest existed.** The previous entry (`bbd2018`) committed a record whose most load-bearing literals — the two AROS requester dumps — existed only in the conversation. The Grounding Invariant requires them in a durable raw source, so they are transcribed here now, **marked as transcribed from the operator-supplied report images** rather than presented as machine-captured, and ambiguous fields flagged `[?]`.
- **Cascade, and the finding in it.** Searching the wiki for records asserting the "stale r12" framing returned **only my own two records** — that phrasing came from the `ri_build_aros.sh` comment, not from any earlier record, so there was nothing upstream to correct. But searching for *r12 claims* generally found something worse: `2026-09-23-dell-gui-first-light-classic-window` **already had the correct framing**, written three months earlier, in a parenthetical — *"note the inversion: r12 moves EXPECTED here, the r12==0 gate is v1-guest-only"*. A same-day record asserted the opposite ("the gate's invariant is unachievable in this tree") **without opening it**.
- **Two records now say so.** `2026-09-23-dell-gui-first-light-classic-window` gains the AROS-upstream confirmation of its own framing (`ABI_SPECIFICATION`: R12 for base, library-side base R12, `-ffixed-r12` sanctioned, *"Not using R12 in caller side code is however NOT an ABI requirement"*) — and is explicitly marked as *the example of what the wiki is for, against the later error of not using it*. `2026-10-02-r12moves-…` gains the note that the answer was catchable from inside this wiki, and that it needed **two** rounds of self-correction to get there.
- **Also folded into the riqemu1 record:** the explicit alignment arithmetic (both addresses **12 mod 16**, misaligned by 4 bytes, both reports carrying `0x00000008` = #GP on x86-64) and the blocked open item — the **Levi sub-split** (`ARPA/LEVSEQ/LEVVOICE/LEVMIX` inside `SLEVI`, t156 + `mut_fixM6` 14/14 + `mut_fixM7` 25/25, ASan clean) is built and mutation-proven but still has **no device number**, because riqemu1 has no usable AHI and the Dell belongs to another session.
- **Convention corrected mid-ingest:** I created `raw/evidence/` believing it already existed. It did not — the 72 `evidence/` mentions in articles are **inline code spans** pointing at the repo's `docs/evidence/`, a different tree. The real convention is a raw subdirectory per source (`raw/aros-dev-portal/`) with the `**Ingested:** / **Source:** / **Provenance:**` header and its own index row. The new file's header was rewritten to match and the index row added. The lint's own evidence count was wrong for the same reason and was recomputed.
- **Lint:** 206 articles + 3 raw source files, **0 unindexed**, **218 index rows 0 dead**, **351 links 0 broken** (articles *and* raw sources), **25 Status blocks, 0 undated among superseding claims** (0 `Outdated`/`Disputed`/`Refuted`/`Corrected` blocks lack a date; the 8 that carry no calendar date are `shipped`/`design input` blocks that cite commit refs instead — a different predicate, stated here so the "all dated" claim is not read as broader than it is).
- **Predicate note for the next lint:** "all Status blocks dated" was previously reported from a narrow predicate that only looked at the four superseding-claim states. Re-running it across *all* Status blocks surfaced those 8, which are correctly undated. Worth stating the predicate in the log each time rather than letting the summary number drift.

## [2026-10-02] new + fix | A lost audio path must not be silent (`2ecffd0`, deployed and proven)
- Disposition: New (`2026-10-02-a-lost-audio-path-must-not-be-silent.md`); also **corrects** a claim in `2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md`
- Trigger: "continue your work" after the host cold reboot. The lane had to re-establish itself first (below), and the outstanding item from last turn was the silent-null-backend gap recorded as "a decision for whoever owns `app/riapp.c`". `main` is uncontended (`wt-A` is prunable/gone), so the lane took it.
- **RED on hardware before any code.** Control first: instance A alone logs `audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us` (real backend). Then B launched while A held the card: `Process 8` + `Process 9`, two `RIAPP live panel` windows, **zero requesters**. Two lines of window state, whole defect.
- **Mechanism pinned in source, not inferred from the error number.** `err 4` is set at `audio_ahi_live.c:236` right after `AHI_AllocAudioA` returns NULL; `err 2` at `:192` right after `OpenDevice("ahi.device")` fails. Under contention the device *opens* and only the allocation fails -- that is what makes a single comparison able to tell "this machine has no sound card" from "another program is using yours".
- **The carve-out is load-bearing and was checked, not assumed:** err 2 keeps `RI_AUDIO_NULL_MSG` byte-exact, which `tests/unit/t6_w1backend.c` pins. A machine with no sound card must not be nagged. err 0 ("opened, then `au_live_run` failed") and unknown codes are loud -- fail loud, never fail silent.
- **GREEN on hardware:** same two-instance scenario, rebuilt binary -> `RIAPP 519,289 328x191 [active,no-close]`, a requester. 328x191 vs the song requester's 391x123 is the multi-line message. Log gains `RIAPP audio: sound card unusable, continuing without sound [err 4]`. Single instance afterwards: live AHI, no requester -- so the carve-out holds in **both** directions and this is not "always nag".
- **TDD as recorded, including the part that nearly went wrong.** The RED was the shipped behaviour itself (stub = "never loud") -> 10 failures on exactly the lost-path cases, absent carve-out already green. Mutants: `==`->`!=` killed, carve-out->`0` killed, carve-out->`3` killed, always-loud killed, always-quiet killed; `K == err` left the object **byte-identical** (provably equivalent, not a survival). **`return 1;` / `return 0;` first "failed to kill" only because `-Wunused-parameter` + `-Werror` left no object at all -- reported inconclusive and redone with the parameter referenced.** A kill claimed from a build that never compiled is the worst false evidence, because it looks like rigor.
- **Correction 1 -- the guest CLI does have `kill`.** Last turn's record said clearing a wedged process "needs a guest-side `kill` (the lane's CLI has no `kill` in reach)". Wrong in a way that matters: the *lane* has no kill action (`--exec/--get/--put/--run-script/--ui-*`), but the **AROS CLI on the guest does** -- `kill Process8` -> `rc=0`, `kill: object not found`. That claim would have sent someone to reboot when a `kill` was sitting there.
- **Correction 2 -- a modal requester whose process lingers windowless and poisons later launches.** `--ui-close` reports `ok` and dismisses the box, but the process stays in `status` with **no window** for minutes and still holds `ahi.device`. Consequence measured: a *subsequent single* launch then also got `err 4` and its own requester, so the next launch inherits the failure and reads as a new bug. Only signature that identifies it: present in `status`, absent from `--ui-windows`. Rule: **after any two-instance experiment, wait for `status` to clear before launching again.**
- **Method breach recorded rather than hidden:** one `--get` was taken while the app was running, for the startup AHI line (startup lines present, shutdown line not). Everything load-bearing was re-pulled after close: shutdown summary, `build=2ecffd0` from `Vk4aros:RIAPP-EV.LOG`, and the final single-instance control.
- **Host cold reboot, lane recovery (recorded because it cost time and is not obvious):** the spool and all three `serve` processes came back but the guest agent showed `disconnected last=0`; `ping 192.168.1.60` was fine while port 9292 was refused. It reconnected **on its own** within minutes -- consistent with the earlier record that a reconnect sometimes happens. Note `x86_64-aros-gcc` is not on a bare shell's PATH after a reboot; the audit sources `../Vulkan4Aros/scripts/aros_build_env.sh` itself, so probe AROS compiles through the audit rather than by hand.
- **Index repair from last turn, now committed:** the two articles with no index row are in -- `2026-09-24-codebase-review-rebirth-fidelity` under Reviews (the `ri_exp` cliff below ~ -25 that blows 808 voices up 2-3 s after their last hit) and `2026-09-25-g5-key-precedence-decision` under GUI. Verified by **per-article membership**, not counts: 207 articles, 207 row entries, 0 rows pointing at a missing file, 0 broken links.
- **Deployment:** `Vk4aros:ReIncarnation/RIAPP` = 1,094,648 B, `build=2ecffd0` read back from the ev-log; rollback preserved as `RIAPP-old-no-audio-fail` (1,093,672 B, named for what it lacks, per the rule from the fixture-hygiene article).

## [2026-10-02] ingest | Cascade: the audio fix's evidence updates three existing articles
- Disposition: **Update** (no new article — the recent material all belongs to theses already on file, and the skill says not to force an article out of what is already covered)
- Raw: n/a — the source is this session's own work on the Dell plus commits `2ecffd0` / `ab39525`; per repo convention session findings land directly in `llm-wiki/raw/articles/`
- Updated: `2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md`, `2026-10-02-one-requester-two-instances-and-a-silent-null-backend.md`, `2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md`
- **Why a second ingest of the same day.** The first pass (`ab39525`) recorded the *fix* as a new article. That left three older articles carrying claims this session had since proved wrong or sharpened, which is exactly the cascade the skill requires and the thing an index alone will never surface.
- **Cascade 1 — the xruns split is now settled, not open.** `head-misses-the-audio-deadline` left a three-way split unresolved (DSP got more expensive / instrumentation is most of the 18 ms / some of both). The confirmation run rules out the middle branch: `stg_dsp_avg=5689 us` **alone** already exceeds `period=5333 us`, so the average buffer cannot be over budget because of measurement. Answer is the first branch. Instrumentation's share is now bounded near `5766-5689 = 77 us` of event work plus the timers' own cost. Two stages over on their own (`stg[5]` 5658, `stg[7]` 5735); largest device Levi avg 747 / max 15710 us. Recorded as an **Update block** with the old claim kept.
- **Cascade 2 — a correction, kept as a correction.** `one-requester-two-instances` said clearing a wedge "needs a guest-side `kill` (the lane's CLI has no `kill` in reach)". Wrong in the way that costs most: the *lane* has no kill action, the **guest AROS CLI does** (`kill Process8` -> rc=0, "kill: object not found"). Added as a `Correction` block rather than a silent rewrite, with the narrower true claim beside it.
- **Cascade 3 — new operational section in `dell-lane-bare-path...`**: "After a host cold reboot: the lane returns, but not the way you would guess". Spool back but empty, all three `serve` processes back, guest `disconnected last=0`. **Two checks actively mislead** — `ping` succeeds at ~0.15 ms and the Dell's own `9292` is *refused*, neither of which says anything, because the server is on the host and the agent dials out. Only the lane's `status` is meaningful. **It reconnected on its own** within minutes with no `SIGUSR1` and no guest action, which revises the older "it costs a guest-side agent restart" into "a reset *may* cost one". Plus: `/tmp/ri` does not survive (the 81-TU v11 rebuild is needed before any deploy), and `x86_64-aros-gcc` is absent from a bare shell's PATH after a reboot while the audit sources `aros_build_env.sh` itself — so probe AROS compiles **through the audit**. Ends on the windowless-holder signature and the rule to wait for `status` to clear.
- **Sibling lane checked, not touched.** `70294a2` (riqemu1 crash literals + the r12 cascade) carried its own index row; verified by per-article membership, not counts.
- **Dell state, recorded here rather than in an article** — an inventory goes stale and this log is append-only, which is the right shape for state:
  - `RIAPP` = 1,094,648 B, `build=2ecffd0` (read back from `Vk4aros:RIAPP-EV.LOG`)
  - `RIAPP-old-no-audio-fail` = 1,093,672 B — rollback; lacks the audio-failure report
  - `RIAPP-old-no-dotdot-fix` = 843,448 B — the 00:15 build; lacks the `..` fix and the audio report
  - `RIAPP.prev` = 776,608 B — oldest; predates both
  - Rollback, one command: `copy Vk4aros:ReIncarnation/RIAPP-old-no-audio-fail Vk4aros:ReIncarnation/RIAPP`
  - Lane left idle: 0 RIAPP processes, 2 windows (agent console + AROS). Final verified state = single launch, live AHI, no requester.
- **Lint, run by membership not by counting** (the lesson from the previous pass): 207 articles, 207 row entries, 0 articles without a row, 0 rows pointing at a missing file, 0 broken links across every article. AUDIT 0/0 PASS.

## [2026-10-02] ingest | Grounding hole found and closed: the cited numbers had no surviving evidence file
- Disposition: Update (evidence artefact + the two articles that quote it). Follows the cascade entry above; recorded separately because it is a *defect in the record*, not new knowledge.
- **The hole.** Applying the skill's grounding invariant by hand — every load-bearing number must exist verbatim in an immutable source — the headline figures of this whole thread (97444, 20998, 5766, 5689, 5658, 5735, 15710, 21573, 563088) came back **NOT FOUND**. Cause: the capture lived in `/tmp/opencode/G.log`, and the **host cold reboot wiped `/tmp/opencode`**; the Dell copy of that log was later overwritten by the RED/GREEN runs. The numbers are faithful — they are verbatim tool output, quoted correctly in the articles — but their only local anchor was a temp file that does not survive a reboot. A wiki whose evidence evaporates on reboot is not a wiki; it is a transcript.
- **Fix: a durable evidence artefact**, following the convention the project already uses (`docs/evidence/<topic>/`, cited by existing articles) rather than inventing a new one:
  `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md` — control AHI line, RED window dump, GREEN requester geometry, the err-2-vs-err-4 source citations, the t158 RED failure list, the mutant table with every object sha, the `kill` correction, and the windowless-holder signature. Both articles now carry an `Evidence:` line pointing at it.
- **And a third run, so the claim has a live capture rather than a remembered one.** Re-ran `demos.rbpl` on the deployed `2ecffd0` binary, one instance, AHI live (`mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us`), 3 songs loaded, no requester:
  `buffers=90106 xruns=18259 render_max=10115 us stg_total_avg=5654 us stg_dsp_avg=5576 us stg_evt_avg=30 us`
  `stg_dsp_avg` is again **above** `period=5333 us`, now by 243 us on its own — the third run to say so. Two stages carry it (`stg[5]` 5576, `stg[7]` 5654); largest device Levi avg 737 / max 2564; `dstg block` avg 1370. Logged as a three-run table: 19,703/94,489, 20,998/97,444, 18,259/90,106.
- **Two tooling findings, both mine to own.**
  1. The skill's `scripts/check_evidence.py` **cannot run on this repo**: it hardcodes `root/"wiki"` and requires a `Raw:` metadata field, whereas this wiki lives at `llm-wiki/raw/articles/` with a `Source:` line and articles that *are* the record. Reported, not adapted — building an adapter is a tool change beyond an ingest, and the skill is explicit that evidence problems need a decision rather than an automatic fix. Worth doing as its own piece of work: 207 articles currently have no mechanical grounding check at all.
  2. My first link-check pass **reported ~150 false broken links** because it resolved targets against `os.path.join(base, file)` — the article's *path* — instead of its *directory*. Every link looked broken. Caught by noticing that links I had verified minutes earlier were suddenly failing; the checker was wrong, not the wiki. Re-run correctly: **0 broken links**. Standing rule worth remembering: a checker that suddenly reports mass failure is usually the checker, and the previous green result is the control.
- **Final state:** 207 articles, 207 index rows, 0 articles without a row, 0 rows pointing at a missing file, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | The -O0 xruns are a build-flag artefact: -O2 gives 0/214,676 on the real song
- Disposition: **New** (`2026-10-03-the-o0-xruns-are-a-build-flag-artefact-not-a-render-stage-regression.md`) + cascade into three existing articles
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, new section "`-O2` on a REAL song" — every figure verbatim plus a four-cell table, so the numbers have a durable anchor rather than a `/tmp` file that a reboot can take
- Updated: `2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md` (CASCADE), `2026-10-02-head-misses-the-audio-deadline-on-a-real-song.md` (UPDATE), `2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md` (UPDATE)
- **The reconciliation was one grep against logs that had been sitting on disk, readable, the whole time.** Two records disagreed on their face. `stg1`, `stg2`, `stgo2`, `stgo2b` and `stgbase` each contain `RIAPP play` and **zero** `RIAPP playlist` / `RIAPP song` lines -- so every `-O2` figure on file was measured on the **built-in demo**, and every `-O0` real-song figure on The Knife and Zombie Nation. Different workloads: never a contradiction, just a matrix with a hole in the one cell that decides the question.
- **Filled the cell.** `RIAPP-o2`, 874,968 B, `-O2` substituted into a copy of `~/bin/build_v11.sh` (the script now hardcodes `-O0`; the lane switched it deliberately, and I left both the script and the deployed `-O0` binary alone). 81 TUs, 0 undefined, `r12moves=282` -- matching the existing `-O2` profile of 873,888 B / 286. Same `demos.rbpl`, one instance, AHI live, **two runs** because a single `0` is either a fix or a quiet guest:
  `buffers=109229 xruns=0 stg_total_avg=2467 us stg_dsp_avg=2389 us render_max=4296 us arm_us=0`
  `buffers=105447 xruns=0 stg_total_avg=2805 us stg_dsp_avg=2723 us render_max=4414 us arm_us=0`
  **214,676 buffers, 0 xruns.** Stage average 46-53 % of the 5333 us period where `-O0` sits at 106-108 %.
- **The discriminator that settles it:** per-device costs halve and the *proportions do not move* -- Levi 737 -> 350 us, `dstg block` 1370 -> 660 us. A uniform ~2.1x with stable ratios is a compiler flag, not an algorithmic change. Had the ratios shifted, the story would have been "the algorithm changed" and the fix would still have been in the render stage.
- **What this does NOT retract.** The `-O0` finding stands exactly where it was claimed: its controlled pair was old-binary `-O0` against HEAD `-O0`, same flag, so "the DSP got more expensive at `-O0`" is untouched. What changes is the *cause* implied by the conclusion, and therefore the hand-off: "give it to the render-stage lane" was the wrong lever. The render stage's breakdown is right about **where** the time goes (VOICES ~94 %, FX ~1 %, Levi largest) and that stays useful as guidance for anyone optimising later -- but nothing there needs changing to make this song play.
- **A prediction confirmed.** The `-O2` record predicted "if the A,B,B,A arms were re-run at `-O2`, neither fix is measurable, because the arm would never engage". Both runs show `arm_us=0 overloads=0`, peak `load` 526/1000 and 716/1000 -- on a heavier workload than the prediction was made about, and by the same mechanism that record's own point 3 identified: at `-O0` the wake was late because the *work* was long.
- **Method findings worth keeping.**
  - **A matrix with one filled corner is not a contradiction.** Before calling anything inconsistent, ask what the *workload* was, and read it out of the log rather than inferring it from prose. This one nearly became a false alarm in the other direction too -- I was ready to write "the two records disagree" on the strength of the abstracts.
  - **"On this workload" is load-bearing.** The `-O2` record said it sixteen times and never once said *built-in demo*. A reader is entitled to take that as the workload that matters.
  - **The heaviest available workload should be the default for a performance claim.** Every `-O2` number on file came from the lightest thing the app can play, so the flag's benefit was real and its *extent* understated -- the more dangerous direction for a record to be wrong in.
  - **`arm_us=0` is a result, not an absence.** Zero arm usage is the predicted consequence of a faster build, and the cleanest available confirmation that the governor fixes were compensating for slow code.
- **Left open, and it is an owner call:** `-O2` is still not the shipping answer on its own evidence. `r12moves` 41 -> 282 is the repo's own ABI recipe check, and 282 has never run on the Dell as a shipping configuration. `RIAPP-o2` is deployed alongside `RIAPP` on the stick for exactly that comparison and has not replaced it.
- **Lint:** 208 articles, 208 index rows, 0 without a row, 0 rows to a missing file, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | -O2 as an A,B,B,A arm: removes every xrun, makes the tab cycle 2.4x slower
- Disposition: **New** (`2026-10-03-o2-is-not-a-free-win-it-removes-every-xrun-and-makes-the-tab-cycle-2-4x-slower.md`) + cascade into two records
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, section "A,B,B,A with the BUILD FLAG as the arm"
- Updated: `2026-10-03-the-o0-xruns-are-a-build-flag-artefact-not-a-render-stage-regression.md`, `2026-10-02-r12moves-is-an-inlining-counter-not-an-abi-hazard.md`
- **Answering the previous turn's open question.** Last turn closed with "is `-O2` safe to ship?" and `r12moves` 41 -> 282 named as the blocker. Measured rather than argued.
- **Design point that makes it an experiment and not a vibe:** both arms were built from one clean `git archive HEAD | tar -x` tree (`a98691a`), **not** from the working directory. The working tree holds the sibling lane's uncommitted `NOAUDIO` change; a flag comparison that silently differs by someone else's patch measures nothing. `-O0` 1,094,648 B / 41 r12, `-O2` 874,744 B / 282 r12, same source, same commit.
- **Protocol:** the existing scripted A,B,B,A (Play, five tabs, Stop) on `demos.rbpl` rather than the demo, because the demo is the workload that cannot discriminate. Every click event-verified against the event it produces -- `TR PLAY TAB page=0..4 TR STOP` in all five cells. No blind clicks.
- **Result A,B,B,A + a third B to resolve an outlier:** xruns 1590/1589 (`-O0`) against 0/0/0 (`-O2`); `overloads` 6/6 against 0/0/0; `render_max` ~92,000 us against ~4,110 us; `wake_max` ~5,820 against ~400-470; five-tab cycle 99,432/99,786 us against 300,041*/241,791/241,632 us. B1's single LEVI switch (107,029 us) **excluded by name** under this guest's re-run rule -- not dropped -- after which the `-O2` pair agrees to 0.07 % against 0.36 % for `-O0`.
- **The finding that matters: `-O2` is not a free win, it is a trade.** Every xrun goes away (0 in 12,213 buffers) and `render_max` falls 22x -- and the five-tab repaint cycle becomes **2.43x slower** (99.6 ms -> 241.7 ms), uniformly across DRUMS/LEVI/MIX/FX with SYNTH unchanged at 27 us. The flag does not remove a problem, it moves it from the audio deadline to GUI responsiveness. Which side matters is an owner call; the measurement question is now closed.
- **`r12moves=282` is not the blocker, confirmed on target.** Three clean runs, every click verified, exercising all eight inlined files: tab switches drive `rsection.mcc.o` (52, the largest), `skin_aros.o` loads at startup, `audio_ahi_live.o` runs live AHI, `fs_aros.o` loads three songs, 909 pack and log live throughout. The host-side source analysis (v11 inlining artefact, `--ffixed-r12` cannot suppress it, it is an ABI fingerprint) held up. The reason not to ship `-O2` is GUI latency and it has nothing to do with `r12moves`.
- **The prediction confirmed, with a consequence the record did not draw.** The `-O2` record predicted neither A,B,B,A fix would be measurable at `-O2` because the arm never engages. True: `overloads` 6 -> 0, `arm_us` 0 throughout. But the consequence runs the other way -- the arm going quiet does not make the repaint regression irrelevant, it makes it **visible**. At `-O0` six governor trips are what yield CPU to Intuition, so the tab cycle is fast *because* the audio path is collapsing. Recorded explicitly as a **hypothesis consistent with the data, not proven**: separating the two needs an arm-disabled `-O2` build, which is the obvious next experiment. What is established is the coupling.
- **New standing gap, and it is the more pressing one:** the **deployed binary is `-O0`**, and on the real song that is ~1,590 xruns and a 92 ms `render_max` per tab cycle. `-O0` is not viable for shipping on its own terms, quite apart from `-O2`'s cost. So the real decision is not "-O2 or nothing" but "what is the shipping configuration", and the two ends of it are both currently unsatisfactory.
- **Reproducibility gap:** `songs/local/` is git-ignored, so `demos.rbpl` and the songs it names exist only on this machine. These five runs are not reproducible from a clean clone.
- **Lint:** 209 articles, 209 index rows, 0 without a row, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | AHI on riqemu1 works: the loader ignores sh_addralign, not a miscompile
- Disposition: **New** (article + raw evidence); **superseded in its diagnosis** (`2026-10-02-riqemu1-software-failure-is-two-faults-wrong-abi-fixed-and-misaligned-movaps.md`)
- Raw: `llm-wiki/raw/evidence/2026-10-03-riqemu1-ahi-loader-alignment-crash-reports.md` (three operator-supplied requester dumps, transcribed)
- **The correction.** Yesterday's record had the `movaps`/`#GP` mechanism right and the conclusion wrong: the binaries are **not** miscompiled. The AROS ELF loader places `.rodata` at **12 mod 16** despite `sh_addralign = 16`, so every alignment-assuming move the compiler proved aligned at link time faults. Proof: faulting constant `0x4bf4d88c` − section offset `0x22a0` = load base `0x4bf4b5ec`, and `0x4bf4b5ec mod 16 = 12`; `0x22a0` **is** 16-aligned in the file, so GCC was correct. Two independent modules land on it (`ahi.device`, `ac97.audio`).
- **Why the earlier quarantine work never found it:** it changes *which* module faults, not whether the loader misplaces sections. Four experiments each produced "driver selection is not the problem" — true, and read as the end of the enquiry rather than as evidence the fault is below the driver layer.
- **Fix:** `~/Work/vms/fix-ahi-movaps.py` (host tool, outside the repo) rewrites `movaps`/`movdqa` to their unaligned twins, `.ltext` only, at offsets objdump reports as real instructions. 159 single-byte changes in `ahi.device`, 17 in `ac97.audio`. **A workaround for the loader, not a fix to it** — the real fix is honouring `sh_addralign`, and there is no AROS toolchain on this host (`x86_64-aros-gcc` absent).
- **Two wrong opcode mappings, both caught by disassembling the result rather than by re-reading the argument:** `movdqa` was skipped because `0f 6f` was mistaken for the unaligned form (`movdqu` is F3-prefixed) — the `movaps` patch then "worked" and 126 `movdqa` faulted identically, looking like progress; and `0f 29` → `0f 13` is **`movlps`, a 64-bit store**, silently truncating 16-byte stores to 8, which shipped into the guest. `verify_twins()` now counts **mnemonics not bytes**, since bytes cannot distinguish `movups` from `movlps`.
- **Proven:** `probe_ahi` full pass (`ahi.session: OPEN unit=255`, `alloc_audio: OK freq=48000 bits=16 stereo=1 maxch=128`, all seven low-level sizes `rc=0`), plus a live uncorked PulseAudio `Sink Input` named `riqemu1`. **Not proven:** RIAPP playing audible audio — transport never started, and a 6 s `parec` capture read peak 0 / 100 % zeros.
- **Open, needs a human:** the guest is **halted at boot** on `Device DH0: (data.device, unit 0) Has an unfinished transaction` from the reload resets, and needs a **host click on OK**. `sendkey ret` will not activate it and this host has no mouse-injection tool (`ydotool`/`xdotool` absent, no `hyprctl` cursor dispatcher, QEMU `mouse_move` is relative and missed).
- **Self-correction worth recording:** I reported "no requester" as a pass after reading the window list; the requester simply had not appeared yet, and the owner's screenshot showed the fault still present. A window list read too early is not a pass.
- **Method:** get the crash report as text, and failing that get the **binary** — disassembling `DEVS:ahi.device` answered in one step what four guest-side experiments could not (the constant's section offset, hence the load base). **Arithmetic beats inference.**

## [2026-10-03] ingest | A mutation kill you never ran: three ways the harness lies
- Disposition: **New** (`2026-10-03-a-mutation-kill-you-never-ran-three-ways-the-harness-lies.md`) + cascade into the testability-boundary record
- Raw: n/a -- the source is this session's own work (`9816b62` gates + the `t155` strengthening, `2ecffd0` the `t158` mutation set); per repo convention session findings land directly in `llm-wiki/raw/articles/`
- Updated: `2026-10-02-testability-boundary-aros-only-code-and-mirrored-tests.md`
- **Found by triage, not by accident.** The gating work went into a detailed commit message and never reached the wiki. Checked four things before writing: does any article mention the gates (no), does the testability record know `t155` was blind (no -- 0 mentions), does it still say "three separate investigations" (yes), and is `ri_dlist_set_clip` reachable from production (no).
- **Three harness traps, each of which printed a legitimate-looking result.**
  1. **A build that fails is not a kill.** `return 1;` / `return 0;` tripped `-Wunused-parameter` + `-Werror`, so no `.o` was written; the verdict came back with an empty `sha=` and "did the test pass? no -> killed" read it as a kill. Redone as `? 1 : 1` / `? 0 : 0` so the mutant compiles -- both die immediately.
  2. **A header mutant leaves every object byte-identical.** `ri_rsection_box_why` is `static inline` in `gui/panelui.h`, so the edit recompiles the test TU, not a library object. The build genuinely succeeds and every object-level check says "fine". Two mutants reported `STALE BINARY` with the *same* hash both times -- the binary was stale, not the objects. Fixed by deleting the test binary, rebuilding, and hashing the binary.
  3. **The wrong test cannot see the constant.** `RI_RSEC_BOX_COUNT 4->3` against `t152`, which never mentions it, reports a stale binary -- which is really wrong test selection. Against `t154`, which does reference it, killed at `:64`.
- **Plus the one that is not a trap.** After the mutation loop `t155` failed eight assertions minutes after passing, with no source change: the harness leaves the last mutant's `.o` in `/tmp/ri/build` and `ri_build_host.sh test` links `$OUT/*.o` without rebuilding library objects (documented; hit anyway). **A sudden mass failure right after a mutation run is the leftover object until proven otherwise.**
- **The distinction that makes the article worth writing.** Four survivors, four different things: three harness artefacts, one *provable equivalence* (`wake max > -> >=` -- the field only rises from 0, so at `us == max` the store rewrites the same value; replaced with two real mutants per the standing rule, never keep a mutant you cannot kill), and **one genuine hole**. **An equivalent mutant is a fact about the design worth writing down; a stale build is a fact about the harness worth nothing. Both print `SURVIVED`.**
- **Cascade: two more occurrences for the testability record, one a new shape.** (4) `t155` was not mirroring anything -- it called the production function, but its helper re-initialised and set the clip **once**, so the clear-an-existing-clip branch was unreachable. *A test can reach the code and still not reach the branch.* Strengthened with a real box then a degenerate box asserting `clip` 1 -> 0; the mutant now dies at `t155_damage_clip_build.c:180`. (5) `ri_dlist_set_clip` has **no production caller** at all, so a mutation-proven green test was covering dead code -- the same failure mode arriving from the opposite direction.
- **Annotated a stale count in that record rather than only adding to it.** It still read "three separate investigations" after two more; now noted in place. A stale tally is the article-level version of a `SURVIVED` nobody re-read, and the record is the thing meant to prevent that.
- **Gating recorded with the gate proved negatively.** `t151`/`t152`/`t154`/`t155` now in `scripts/ri_audit.sh` (157 gated tests; `t156` deliberately left to the lane mid-edit on it). A gate never observed failing is not known to work, so a gated test's expectation was inverted to a wrong-but-compiling value and the audit confirmed to exit 1 naming `t155`. The first control attempt only tripped `-Werror`, which fails for the wrong reason -- the control has to fail the way the real bug would.
- **Sibling lane's two commits checked, not touched:** `58bb3e7` (the verified `NOAUDIO` opt-out) and `0340cb5` (AHI on riqemu1 / loader `sh_addralign`). Wiki verified consistent after both: 210 articles, 210 rows, 0 without a row, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | 26 tests were never gated, and two had already gone stale
- Disposition: **New** (`2026-10-03-26-tests-were-never-gated-and-two-had-already-gone-stale.md`)
- Raw: n/a -- the source is this session's own work (the `audit:` commit); per repo convention session findings land directly in `llm-wiki/raw/articles/`
- Updated: `llm-wiki/index.md` (new row); **annotated in place** `2026-10-03-a-mutation-kill-you-never-ran-three-ways-the-harness-lies.md`, whose `157 gated tests` and "t156 deliberately left to the lane mid-edit" were both stale
- **Asked whether `scripts/ri_audit.sh` was complete; the answer had two halves pulling opposite ways.** It was the most recently touched file in the repo, had no broken references, no placeholder markers, and passed `AUDIT 0/0` from a clean checkout -- and it was **missing 26 of the 183 tests it is supposed to be gating**, written across nine days (2026-09-24 -> 2026-10-02).
- **First hypothesis was wrong and one command refuted it.** Twenty contiguous ungated numbers (`t33`-`t52`) read as an abandoned consolidation into `t1_808`/`t1_909`/`t1_fx`, which would have justified deleting them. `git log --oneline -S<tname> -- scripts/ri_audit.sh | wc -l` returns **0 for all 26**: never gated, never removed, so nothing encodes an intent because none was recorded. **An omission is indistinguishable from a decision**, which is the only reason they survived nine days.
- **Two had already gone stale, in one shape.** `t51_route`: `RI_ROUTE_MASTER` moved 4->5 when Levi took section 4 (`59a3e01`); `t51` used the **macro** everywhere it assigned comp to master and a hardcoded **literal `5`** at the one line asserting rejection, so the macro uses were updated and the literal was missed. `t107_levi_sect`: RIBBON took encoder slot 5, so `dead slot inert`/`dead slot blank` fail -- **left ungated, and it is the *only* exemption the lint carries.** The other four Levi strays (`t108_levi_algo`, `t109_levi_morph`, `t118_levi_seq_player`, `t122_levi_lfo`) passed the moment they were run and are now gated. **Of the 26: 25 closed, 1 open and named.** A first draft exempted all five, which would have shipped an audit printing `0/0` with a known-red test inside it -- disclosed, but still a number a reader has to stop and interpret.
- **One stale comment could have produced wrong code.** `route.h`'s `section_mask` said "sections > 3 (incl. master) read 0"; it returns 0 only for `section >= RI_ROUTE_NSECTIONS`, so **Levi at section 4 gets a live mask**, and a caller believing the comment would skip masking the Levi strip. Three more rotted bounds comments in `route.h`/`engine.h` were misleading rather than dangerous.
- **Fixed on the condition, not the list.** 21 gates placed by domain (`t37_engine_single` into phase 6, whose subject *is* the one-renderer contract) plus **phase 0d**: any `tests/unit/*.c` unreachable from a gate fails the audit, exemptions must be **named in-script** so "not gated" is a recorded decision, and the lint resolves its own path from `${BASH_SOURCE[0]}` not `$0` (wrong under `bash <` and under a symlink -- a self-inspecting gate that inspects nothing is worse than none). **157 gated -> 182 gated, 1 exempted.** Fails in seconds, before any build.
- **Both new gates observed failing**, per the prior article's standing rule, and the control failed **the way a real bug fails** rather than by tripping `-Werror`: lint with its exemption list emptied -> `FAIL: t107_levi_sect is not gated`, exit 1; `RI_ASSERT(0)` injected into `t37` -> audit halts in phase 6, exit 1, message names the contract. `t37` restored bit-identical to HEAD.
- **Two corrections to this session's own work, recorded rather than buried.** The consolidation hypothesis was wrong (above). And the first proof that the 21 gates ran was **invalid** -- it grepped the log for `PASS <name>`, but every gate line is `test X >/dev/null || { FAIL; exit 1; }`, so the log holds no per-test output and the grep would have "passed" a suite that ran nothing: **a green audit log is not a record of what ran**, and reaching `0/0` is the proof, by exhaustion rather than by record.
- **Scope of a green here, unchanged:** `AUDIT 0/0 PASS` with the mingw gate `SKIP`ping (no `x86_64-w64-mingw32-gcc` here) and the external `sox` checks degrading to in-tree `inspect --wav`; lane T11 stays unverified on this host.
- **Pre-existing, not touched:** `llm-wiki/index.md` has a row for `2026-09-22-laptop-abiv11-real-hardware-ahi-probe.md` with no file behind it. Flagged, left alone.

## [2026-10-03] ingest | The governor arm is load-bearing at -O0 and irrelevant at -O2
- Disposition: **New** (`2026-10-03-the-governor-arm-is-load-bearing-at-o0-and-irrelevant-at-o2.md`) + cascade resolving the hypothesis left open two commits ago
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, section "Arm-disabled cells"
- Updated: `2026-10-03-o2-is-not-a-free-win-it-removes-every-xrun-and-makes-the-tab-cycle-2-4x-slower.md`
- **Settles the coupling** the previous record raised as a hypothesis: is `-O2`'s slow repaint the arm's doing, or is `-O2` genuinely slower?
- **The arm removed with one line, at the one place that consults it** -- `audio_io/audio_ahi_live.c`, where the render task drops below the UI. `governor()` untouched, so `overloads`/`arm_us` keep reporting what *would* have tripped. Both binaries from a second clean `git archive HEAD` tree, so the arm gate is the only difference from its control: `-O2` 874,744 -> 874,896 B, `-O0` 1,094,648 -> 1,095,592 B, `r12moves` unchanged in each pair.
- **At `-O0` the arm is load-bearing in both directions, far more strongly than the hypothesis claimed.** Arm off: xruns 1,601 -> **11,955 (7.4x)**, five-tab cycle 99.6 ms -> **27,701,899 / 38,214,843 us**, with single switches of 8.4 s and 24.2 s. **No `hb:` heartbeat line at all** -- the heartbeat prints from the GUI task, which was too starved to print it. Its absence was the first sign the cells were not merely slow but broken.
- **The sharpest statement of the arm's purpose is an inversion: `render_max` FALLS when the arm is removed** (91,932 -> 9,464 us). The render task no longer yields below the UI, so it never accumulates a 92 ms single-buffer stall -- it simply never gets to run, and the xruns and GUI latency both rise instead. **The 92 ms figure was the symptom of yielding; removing the yield fixed the measurement and broke the machine.**
- **At `-O2` the arm is irrelevant:** `overloads=0` with it on or off, tab cycle unchanged (240,873 / 260,392 against 241,791 / 241,632; T1's MIX tab 357,611 us excluded by name). Nothing to mask -- it was never engaging.
- **Therefore `-O2`'s slow repaint is genuine and the damage-box full-rebuild fix is on the critical path for shipping `-O2`.** There is no configuration in which `-O2` is simply free, and the repaint work cannot be deferred as an artefact of the arm.
- **Shipping configuration is now a build plan, not a flag:** `-O2` for audio, the full-rebuild fixed for the GUI, the arm intact. Three measured facts: `-O0` is not viable (1,601 xruns, 92 ms `render_max`); `-O2` fixes audio completely (0 xruns) and costs 2.43x on the tab cycle; the arm must stay or `-O0` collapses into tens-of-second tab switches.
- **Method finding that would have wasted the experiment: the discriminating cell was not the one the hypothesis named.** Arm-disabled `-O2` is a near no-op *by construction* -- `overloads=0` there means the arm was already never yielding -- so running only the obvious cell would have "confirmed" the hypothesis by measuring nothing. The discriminating cell is arm-disabled **`-O0`**, where the arm demonstrably fires six times. Built both halves before running either.
- **Second method finding: `render_max` is not a health metric.** It improved by 10x in the arm-OFF cells while the machine got much worse. A metric that moves the right way when the system breaks will eventually be optimised against by mistake.
- **New observation, named rather than filed as two outliers:** the MIX and LEVI tabs are **bimodal** at `-O2` -- B1's LEVI 107,029 us against 52,475/52,545, T1's MIX 357,611 us against 85,516. Same phenomenon twice on two different tabs, both excluded by name and resolved by a re-run. The heaviest section redraws are intermittently several times their median cost. Not chased.
- **Lint:** 212 articles, 212 index rows, 0 without a row, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | Correction: the -O2 tab cost is not the damage-box build
- Disposition: **New** (`2026-10-03-correction-the-o2-tab-cost-is-not-the-damage-box-build.md`) + **correction marks** on the two records carrying the wrong claim
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, section "Where the -O2 tab cost actually is -- and a correction"
- Corrects: `2026-10-03-the-governor-arm-is-load-bearing-at-o0-and-irrelevant-at-o2.md`, `2026-10-03-o2-is-not-a-free-win-it-removes-every-xrun-and-makes-the-tab-cycle-2-4x-slower.md`
- **Why this pass exists.** The previous commit ended with a build plan -- "`-O2` plus the damage-box full-rebuild fixed" -- and I repeated it to the owner as the next step. Before acting on it, checked it against the tree. **It is wrong.**
- **(1) The bounded build already shipped.** `git merge-base --is-ancestor bb1c385 HEAD` -> yes, and `rsection.mcc.c:233` passes the damage box. `bb1c385` was four commits old and sitting in the history the whole time. The record that named this mechanism ("Bounding the damage-box build") documents a fix that **already landed**; two later records cited it as outstanding work.
- **(2) The metric that would move if the hypothesis were true moves the other way.** `build_avg` was already in every log: **195 us at `-O2` against 179-483 us at `-O0`**. Falsified in one read, no new instrumentation. And `box_none` -- the unattributed full-repaint bucket the older record blamed -- is **zero** in most `-O2` cells.
- **(3) The section repaint is 1-2 % of the tab-switch cost.** full_avg 4,565 us against a five-tab total of 241,791 us. The 2.43x cannot be fixed by bounding a build that is already bounded and already negligible.
- **(4) And it is not contention.** `render_total` is **half** at `-O2` (15,933 vs 31,540 ms; 2,625 vs 5,696 us per buffer). The render task does *less* work and the GUI is 2.43x slower -- a busier render task cannot be what slows the GUI.
- **A byproduct worth keeping: this re-reads the previous commit's arm result.** At `-O0`, `wake_total` is 11,547 ms against 116 ms at `-O2` -- the `-O0` render task was pathologically starved. So **part of what looked like "fast GUI at `-O0`" was the audio thread failing to get CPU at all**, which is a fairer reading of the arm result than "the arm was masking the repaint regression".
- **Where the cost has to be, stated as a bound rather than a guess:** `-O2` changes only our 81 TUs; MUI/Intuition/`graphics.library` are prebuilt and byte-identical in both arms. With the section repaint, contention and the arm all ruled out, the candidates are our own page-switch code -- `SetAttrs(MUIA_Group_ActivePage)`, the five `MUIA_RArt_Active` writes, `rail_for_tab()` -- plus the per-tab bimodality already on record. **Not chased.** The honest next probe is named: instrument those four lines the way `box_*` already instruments the section, and see whether the cost is the group switch, the five `SetAttrs`, the rail, or the bimodal tail.
- **Method findings.**
  - **Check a conclusion against the tree before building on it.** The claim was plausible, matched a real older record, and had already been repeated to the owner. One `--is-ancestor` would have caught it. This is the same shape as the `r12moves` error that record itself documents: "found by going to look rather than by re-reading my own reasoning".
  - **The metric that would falsify a hypothesis is worth reading before building the experiment for it.** `build_avg` was already logged.
  - **"98 % of the cost is elsewhere" is stronger than "the named culprit is innocent".** Ruling the build out is not locating the cost, and the record says which.
  - **A counterintuitive pair deserves its own check, not a rationalised story.** `render_total` halving while the GUI got slower is reported as the measurement; the reading offered is labelled a reading.
- The `-O0`/`-O2` trade, the arm findings and the shipping question all stand. What is retracted is the localisation and the build plan built on it.
- **Lint:** 214 articles, 214 index rows, 0 without a row, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] lint | 4 issues found, 4 auto-fixed -- the correction was unreachable from what it corrects
- Disposition: **lint** (no new article; every finding was an already-written claim whose *placement* was wrong)
- Scope: 214 articles. Checks run mechanically: index consistency by **membership per article** (never by count -- the lesson of an earlier pass), broken internal links across every article, orphan detection, and a search for forward references to work already done.
- **Issue 1 (the one that mattered): the correction article was an orphan.** `2026-10-03-correction-the-o2-tab-cost-is-not-the-damage-box-build.md` had **zero inbound links** from any article. The two records it corrects carried the correction in the **index row only** -- never in their bodies. A reader arriving at either record from a search, a See Also, or the index would have read "the damage-box full-rebuild fix is on the critical path for shipping `-O2`" with **no pointer to its retraction**. A correction that cannot be reached from the claim it corrects is not a correction. Fixed: both records now carry an in-body Correction/Resolution block naming the date, the reason and the target article.
- **Issue 2: a stale forward reference presented as an open question.** The `-O2` not-a-free-win record listed, under Standing gaps, "**The arm-disabled `-O2` build** would separate ... Not run." It had been run -- and the run is what produced the arm record and then the correction. Replaced with what actually happened, including the non-obvious part: **the discriminating half was arm-disabled `-O0`, not `-O2`**, because at `-O2` the arm never engages anyway, so the named cell was a no-op by construction. Added alongside it the honest open question in its current form: the repaint cost is 98 % outside the section repaint and unlocalised.
- **Issue 3: a resolved prediction still written as a prediction.** `2026-10-02-optimisation-level-alone-removes-every-xrun-on-the-dell.md` said the A,B,B,A arms "have not been re-run at `-O2` ... that is the obvious next experiment." It has been, on a heavier workload, with a better design (the flag as the variable). Marked **RESOLVED** in place with the measured table, and with the correction carried through so the reader of the original prediction also learns that an intermediate claim of this lane's was retracted.
- **Issue 4 (checked, reported, not changed): orphan density.** 129 of 214 articles have no inbound link from another article. Only **2** of the 46 `2026-10-*` articles were affected before this pass, and one (`2026-10-02-leviasynth-fidelity-p9e-tap-tempo.md`) remains -- it is a sibling-lane record and not this lane's to re-file. The older 2026-07/09 backlog is pre-compounding and the index is the intended navigation for it, so no mass re-linking was attempted: that would be a large, low-value diff to a shared file.
- **Why this pass found something at all, two commits after ingesting both records.** The two most recent ingests were both *additions*. Nothing about them was wrong; what was wrong is that a correction written as a third article was reachable only from the index, and that two older records still advertised the corrected claim as current. **A wiki that only ever grows accumulates superseded claims that nothing retires.** The check that catches it is not a count and not a link check -- it is asking, per article, *what does this still claim that later work overturned?*
- **Also confirmed unchanged:** 214 articles, 214 index rows, 0 without a row, 0 rows pointing at a missing file, 0 broken links across every article. The sibling lane's `a828da7` / `87fdbe9` ("26 tests were never gated, two already stale") checked, not touched. AUDIT 0/0 PASS.

## [2026-10-03] ingest | The song name was always in the log; the T: rule was always in the code
- Disposition: **New** (`2026-10-03-song-name-t-log-probe-and-zombie-nation-default.md`); **Corrected** (`2026-10-02-dell-sticky-log-location-and-size-heuristic-correction.md`)
- **Three owner-reported defects, each one mistake.**
- **"The user can't see which song is playing."** True since songs landed: the log has said `RIAPP song <path>: N bars at M BPM` all along, so the only way to find out was to pull the log off a guest by hand. Fixed as a plate caption beside "PATTERN"/"SONG MODE", deliberately **not** a ctlreg entry — a RI_STR_* control must be automatable, stepper-clampable and MIDI-mappable, none of which a read-only caption wants, and it would put a non-numeric value in a display that renders numbers.
- **"RAM: is a terrible place to store logs. We told your siblings multiple times to use T:, but somehow that never is documented."** Documented — **in the code**, which is the failure. `ri_pal_sticky.h` had carried the rule and a 40-line rationale since 2026-10-02 while the **wiki said `fallback: RAM:`**, which is why a sibling following it landed on RAM: and concluded the fallback was broken. **I did exactly that on riqemu1 the same night and pinned the log to `RAM:` myself** with `setenv RIAPP_LOG RAM:` — the documented escape hatch, used for the wrong reason, while T: was working and already held `RIAPP.LOG`. A stale wiki line is not a documentation gap, it is an active hazard that manufactures false diagnoses.
- **Also fixed:** `T:` was returned **unprobed** when no stick was mounted, so a guest without a scratch disk got a path it could not write to and the log went nowhere. `T:` is now the last **probed** entry; `RAM:` is the last resort (owner: "RAM: can be a fallback in case T: isn't available"). Same lesson as the `movaps` patch: count what is emitted, not what is meant.
- **Zombie Nation is the default demo song**, loaded from `SYS:Classes/ReIncarnation/Songs/` where `RI_PATH_SONGS` already points, and only when the command line chose nothing — an explicit `SONG=`/`PLAYLIST=` is a decision, this is a default. A miss logs one line and carries on; riqemu1 has no song library and must not raise a requester for it. Verified on riqemu1: `RIAPP song SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng: 151 bars at 140 BPM`, alongside `audio: AHI low-level mode=0x00390004 mix=48000`.
- **Placement took three attempts against the real guest**, which is the argument for having one: `PX(560)` put the label in the SYNC/MIDI legend row, `PX(900)` at y=197 put it inside the bar, and only a capture showed it. It sits at `PX(25), PX(190)` of a 1684x208 plate.
- **The test found two real bugs in my own code**: the fit dropped its ellipsis (a cut-off name looked complete) and could emit one wider than the room.
- **Test-shape lesson, and the one worth keeping.** `t159` calls the **real** `ri_art_tr_copy`/`ri_art_tr_fit`, exported for exactly this. The first version re-implemented the fit inside the test and **18 of 19 mutants survived** — a mirror of the thing under test cannot fail when the thing changes. Bounds are **canary-guarded** for the same reason: a copy that loses its bound still returns the right string for every input short enough to matter, so no value assertion can see it.
- **Honest tally:** `mut_fixM8` 7/7 killed. `mut_fixM9` **6/17**, survivors recorded rather than quietly dropped — the draw-path guards in `ri_art_bg_tr` are not reachable from a host test (they need a framebuffer) and are pinned as the contract they encode rather than as a second copy of the logic.
- `AUDIT 0/0 PASS`; t153, t156, t159, t92, t93 PASS.

## [2026-10-03] ingest | AC97 resamples to 44.1 kHz; the lane cannot be driven by injection
- Disposition: **New** (2 articles + 1 raw evidence file); **cascade updates** (`2026-09-30-riqemu1-wide-modes-parked-audio.md`, `2026-10-03-song-name-t-log-probe-and-zombie-nation-default.md`)
- Raw: `llm-wiki/raw/evidence/2026-10-03-riqemu1-lane-measurements.md` (verbatim terminal output: guest/host rates, the guest's own counters, both `screendump` headers, the device list, the exec/ui split, and the mutation tallies)
- **Playback 8.1 % slow, and it is QEMU, not the engine.** Guest `mix=48000`, host `format.rate = "44100"`, so `44100/48000 = 0.91875`. AC97 is fixed at 44.1 kHz and `start_riqemu1.sh` passes it with no rate knob. **Pitch and tempo together** — which is why it sounds like a slow synth rather than a wrong clock, and why the obvious CPU-bound reading is wrong. The guest refuses it: `xruns=0` over 35876 buffers, `render_max` 3113 us against a 5333 us period (58 %), `load=3/1000`, **pacing ratio exactly 1.00**. Fix is to run AHI at 44100; **not implemented**, because it changes the Dell's negotiated rate too and the Dell's card is a different device whose native rate should be probed rather than assumed.
- **New comparability rule, written down because it was not anywhere:** a riqemu1 number is on a resampled clock. Stage *proportions* transfer to the Dell; `xruns` does **not**. That distinction is exactly what the still-blocked Levi sub-split measurement will need.
- **Three hours lost on this lane, all three mine, and none of them a guest fault.** A black screen from my own `setsid` launch (`start_riqemu1.sh` already ends in `-daemonize`), diagnosed only because **`screendump` said `P6 0 1600`** — a PPM header with **width zero**, which is a statement about QEMU rather than about the guest. A "broken" mouse that was an **AROS requester grabbing the pointer by design**, and which I had *created* with repeated `system_reset` (hard reset, no flush) where `system_powerdown` is correct. And injected clicks that report **`injected 3 event(s)`** and arrive nowhere, while `ui-capture`/`ui-windows` work on the same socket.
- **The exec/ui split is the expensive one.** With RIAPP running, `exec` returns `rc=1` with no output and `ui-*` works; `--ui-close "#0"` restores `exec` **immediately**. That is the real reason "the log is unreadable behind the requester" was believed for hours — **there was no requester involved.** Any harness on this lane must sequence launch → measure → **close → read**.
- **A zero-peak capture recorded as proving nothing:** `peak 0 rms 0.0` was taken while `stg_playing=0`, and reading it as "audio is broken" would have been another false diagnosis of the same family as the stale `fallback: RAM:` line.
- Cascade: the 2026-09-30 wide-modes record's audio verdict is now **superseded twice over** (AHI works after the loader patch; the remaining silence was the resample, not a driver fault) with a banner, leaving its display work intact; the song-name record's standing gaps now point at both new articles instead of restating them.
- Method kept: **a protocol-level acknowledgement is not a delivered action**, and **reach for `screendump` before theorising about a black screen**.

## [2026-10-03] ingest | the Dell hands back 44100 for every rate, and BestAudioID cannot select 44100
- Disposition: **New** (1 article + 1 raw evidence file); **cascade correction** (`2026-10-03-riqemu1-ac97-resamples-48k-to-44k1-so-playback-is-8-1-slow.md`, `index.md`)
- Raw: `llm-wiki/raw/evidence/2026-10-03-dell-ahi-rate-probe.md` (verbatim v11 build line, transfer with `sha_ok=True`, the full `RI_RATE` block, **the first buggy run's `got=44100` beside `CONVERTED_TO=192000`**, the idle-lane `status` check, the three invocations, the riqemu1 `exec`/`ping` split with RIAPP live, and the gate results)
- Tooling: `audio_io/probe_rate.c`, new and gated in `scripts/ri_audit.sh` (exists, `__AROS__` guard, AROS-only `#error`, not in the host build, artifact present, **reads the mode back**, **carries the `CONVERTED_TO` tell**). Built for both lanes; the v1 gate is the audit's, the v11 build is `/home/miller/Work/vms/ri-p9/build_probe_v11.sh` (new, lane tooling outside the repo — it exists because `build_v11.sh` is RIAPP-only and the audit's build compiles the **v1** lane, so a green audit build is by construction the wrong binary for the Dell). `r12moves=24` confirms the lane.
- **THE DELL IS NOT NATIVE 48 kHz EITHER, AND I HAD WRITTEN DOWN THAT IT WAS.** Every rate comes back as **44100** — 48000, 32000, 22050, 11025, 8000, all six of them — while `list = 44100, 48000, 88200, 96000, 192000` and `range=44100-192000`. `AHIA_MixFreq` is a suggestion this driver treats as decoration.
- **The inference came from this repo's own log line.** `audio: AHI low-level ... mix=48000` records the **request**, not what the device did, and it reads exactly like a measurement. That is why the corrected rule is worth more than the correction: **a log line that echoes an argument you passed is not evidence about the device.** Corrected comparability rule: both lanes converge on 44100 — riqemu1 by resampling, the Dell by AHI declining the request — so they run on the same clock and differ in *where* they convert, which is precisely why riqemu1 is 8.1 % slow and the Dell is not.
- **THE TRAP, AND IT BLOCKS THE OBVIOUS FIX.** `AHI_BestAudioID` **refuses 44100 on a card whose own frequency list is `list[0] = 44100`** — it matches only 48000, the single rate that arrives as a conversion. Changing the requested rate and letting the existing selector find it therefore **fails to open audio on the Dell**: `AHI_INVALID_ID`, no audio at all. 44100 is reachable, but only by supplying the mode id directly. A genuine AROS driver defect: the selector neither satisfies the request nor offers a native alternative that does.
- **The 44100 fix is now safe on a measured basis** rather than an assumed one: 44100 is native on the Dell, it is what both lanes converge on anyway, and the one precondition is the selector defect above. **Still not implemented — that remains the owner's call.**
- **MY OWN PROBE REPORTED A PLAUSIBLE WRONG NUMBER FIRST.** `got=44100` on one line and `CONVERTED_TO=192000` thirty milliseconds later: the frequency-list query writes through `AHIDB_Frequency` as an output, exactly as the mode query does, so walking the list overwrote the readback with the list's last entry. `192000` is a real number from this machine and would have been filed as a finding. Fixed with a second variable plus an immediate snapshot; **recorded in the raw file so its first block is not mistaken for the measurement.**
- **A refusal is not a "no".** Retrying each rejected request against a known-good mode id is what separated *"the selector cannot match this rate"* from *"the driver cannot play this rate"* — a distinction that decided whether the fix was reachable at all.
- **Two lane facts.** AROS **`Run` spawns and returns**, so its `rc=0` means "spawned", not "ran" and its child's console output arrives after the agent has read the channel — a stdout probe must be invoked in the foreground (`RAM:probe_rate` works; `Run RAM:probe_rate` + `wait` produces nothing). And the **`exec`/ui split reproduced on demand** with RIAPP live on riqemu1 (`exec rc=1`, `ping ok=True`, `ui-windows` fine), which is why no cross-lane probe run followed: a second AHI client beside a live RIAPP risks a contention AHI cannot arbitrate. The riqemu1 contrast comes from the earlier verified readback (`alloc_audio: OK freq=48000`).
- Lane checked idle before use (`status` showed no RIAPP), `AUDIT 0/0 PASS`, and the Dell row's false premise corrected in the article, the index row and this log rather than left to be rediscovered.

## [2026-10-03] ingest | the AHI probe build contract
- Disposition: **New** (1 article + 1 raw evidence file)
- Raw: `llm-wiki/raw/evidence/2026-10-03-ahi-probe-build-transcript.md` (verbatim three-error first build with line numbers, the `dos_protos.h`/`utility_protos.h` greps, the `DOSBase` conflicting-types error quoting `proto/dos.h:20`, the v11 `cannot find -lstdcio`, the rate tag definitions, both final build lines, `AUDIT 0/0 PASS`, and the ELF `REL` table)
- Triaged against the wiki first: `DOSBase` appears in 1 unrelated article, `libstdcio` in 1 (as the v1↔v11 runtime name table), the ahi.h `-I` is already in the 2026-09-22 M1.1 record, and `r12moves` is already established. **The compile/link contract itself was recorded nowhere.**
- **EVERY FAILURE HAD A PLAUSIBLE WRONG EXPLANATION, AND TWO WERE NOT AHI PROBLEMS AT ALL.** `implicit declaration of function 'Printf'; did you mean 'SNPrintf'?` — the suggestion points at **utility** (`clib/utility_protos.h`) when the declaration is **dos** (`clib/dos_protos.h`), and taking it would have been a second wrong turn. And `struct TagItem alloc_tags[]` → `array size missing` is compile-time arithmetic, not AHI.
- **`<proto/dos.h>` DECLARES `DOSBase` AS `struct DosLibrary *`, NOT `struct Library *`,** with storage from `startup.o`. Defining it is a `conflicting types` error, so the rule is **"`AHIBase` is ours to define, `DOSBase` is ours only to assign"** — and `probe_ahi.c` never hit it because its globals block already says so. **The error appeared only AFTER adding `proto/dos.h` to fix `Printf`: fixing one error created the next,** and they were separated by a build rather than by thought.
- **`AHI_FreeAudioA` does not exist**; the compiler names `AHI_FreeAudio`, the only free call in `probe_ahi.c` too.
- **THE v11 LINK IS SHORTER BECAUSE IT IS CORRECT, NOT AS A WORKAROUND.** `cannot find -lstdcio`/`-lposixc` — the v1 lane pairs them behind a `libcrt`/`libstdlib`/`libcrtprog` rename shim, and the probe never needed them since `Printf` is a dos.library function. The v11 SDK also ships no `devices/ahi.h`, so `-I` to the source tree is required.
- **ELF `Type:` AND SIZE IDENTIFY NOTHING:** both probes are `REL` files differing by **1984 B**. So `build_probe_v11.sh` now **asserts `r12moves > 0` rather than reporting it**, which converts the wrong-lane silent failure (a `Software Failure!` requester before the first log line) into a build error. This is the audit's v1/v11 trap recurring for **measurement tools rather than the application**, and it is why a standalone probe needs its own v11 build script at all.
- **The runtime half:** AROS `Run` spawns and returns, so its `rc=0` means *spawned*, not *ran*, and a stdout probe must be invoked in the foreground. Cross-referenced from the Dell rate record rather than restated.
- Cascade: three articles cross-linked — the v11 lane traps record gains the **second-instance note** (tools not just the application, plus the link-shape table), the 2026-09-22 M1.1 record is **generalised forward** from its correct-but-unexplained `-ldos -lexec`, and the Dell rate record's See Also gains the build contract.
- Lint: 219 articles + 7 raw sources, 0 unindexed, 0 dead index rows, 534 links 0 broken.

## [2026-10-03] ingest | Cross-lane check after five sibling commits: nothing of mine broke, one reproducibility caveat added
- Disposition: **Update** (no new article; one caveat added to an existing record, plus the standing Dell inventory refreshed)
- Updated: `2026-10-03-the-o0-xruns-are-a-build-flag-artefact-not-a-render-stage-regression.md`
- **Context.** The tree was clean and the audit green on arrival -- the sibling lane had landed `49856b7` (song name, `T:` log, Zombie Nation default), `b0e8803`, `5defaff` + `1c5d672` (AC97 resamples 48k -> 44.1k on riqemu1), `33b3171` (AHI probe build contract). My previous lint fixes turned out to have been **swept into `49856b7`** rather than dropped, so they are in the history; a case-insensitive re-check confirmed all four landed (an earlier case-sensitive grep of mine briefly suggested otherwise, which was the grep's fault, not the files').
- **Everything of mine was verified against that work rather than assumed compatible.**
  - *Deployed binary*: still 1,094,648 B on the stick, so the `build=2ecffd0` claim in the `-O2` and arm records still holds.
  - *Sticky-log fallback*: the sibling's new record corrects the `fallback: RAM:` line in the 2026-10-02 sticky-log record. Already corrected **in that record's body** by them, not only in the index -- which is the practice this lane's last lint pass asked for and did not get. Nothing to add.
  - *AC97 44.1 kHz*: `mix=48000 Hz` and `period=5333 us` are named in that record, but the resampling is **riqemu1/QEMU-specific** and the guest still *produces* 48 kHz audio (3808x256/48000 = 20.3 s exactly). None of the Dell numbers are touched.
  - *My reproduction commands* that pull `RAM:RIAPP.LOG` still describe the binaries that wrote there, so they stand as history.
- **The one real finding: a reproducibility caveat.** My record states the `-O2` cell was measured on the **built-in demo** -- true of those runs, and now false of a fresh launch, because the default song became Zombie Nation the same day. Anyone reproducing that cell today with no `PLAYLIST=` would silently get a **different workload**, which is the exact unstated-variable failure the `..` records warn about. Caveat added in place, pointing at the sibling's record for the change.
- **Standing Dell inventory refreshed** (state belongs in this append-only log, not in an article that would rot):
  - `RIAPP` = 1,094,648 B, `build=2ecffd0`, **`-O0`** -- still the deployed binary, and **not viable for shipping**: ~1,590 xruns and a 92 ms `render_max` per tab cycle on the real song
  - `RIAPP-old-no-audio-fail` = 1,093,672 B -- rollback; lacks the audio-failure report
  - `RIAPP-old-no-dotdot-fix` = 843,448 B -- the 00:15 build; lacks the `..` fix and the audio report
  - `RIAPP.prev` = 776,608 B -- oldest; predates both
  - **Experiment binaries (not rollbacks, and named like flags rather than for what they lack):** `RIAPP-o2` 874,968 B, `RIAPP-f0` 1,094,648 B, `RIAPP-f2` 874,744 B, `RIAPP-f2noarm` 874,896 B, `RIAPP-f0noarm` 1,095,592 B
  - **Nine binaries on the stick is a hazard of the same shape as the naming rule this lane wrote earlier** ("name a binary for what it lacks"). The five experiment binaries should be pruned now that the `-O2`/arm question is answered; they are the ones most likely to be launched by accident, and `-f0`/`-f2`/`-f2noarm`/`-f0noarm` say nothing about what they are.
- **Lint:** 219 articles, 219 index rows, 0 without a row, 0 rows pointing at a missing file, 0 broken links across every article. AUDIT 0/0 PASS.

## [2026-10-03] update | The stick is pruned: four binaries, each named for what it lacks
- Disposition: **Update** to the standing Dell inventory recorded in the entry above (no new article; this is device state, and state goes in this append-only log)
- Context: "commit and push" with a clean tree and `77e2a04` already on `origin/main`. Nothing to commit, so rather than manufacture one I did the outstanding item this lane had flagged twice: **nine binaries on the stick**, five of them experiment artifacts.
- **Deleted, all five created by this lane and all reproducible** (81 TUs from `~/bin/build_v11.sh` with `-O2` substituted, or with the one-line arm gate in a clean `git archive HEAD` tree):
  ```
  [exec] 'delete Vk4aros:ReIncarnation/RIAPP-o2'      -> rc=0   Deleted
  [exec] 'delete Vk4aros:ReIncarnation/RIAPP-f0'      -> rc=0   Deleted
  [exec] 'delete Vk4aros:ReIncarnation/RIAPP-f2'      -> rc=0   Deleted
  [exec] 'delete Vk4aros:ReIncarnation/RIAPP-f2noarm' -> rc=0   Deleted
  [exec] 'delete Vk4aros:ReIncarnation/RIAPP-f0noarm' -> rc=0   Deleted
  ```
  `RIAPP-o2`, `RIAPP-f0`, `RIAPP-f2`, `RIAPP-f2noarm`, `RIAPP-f0noarm` -- 874,968 / 1,094,648 / 874,744 / 874,896 / 1,095,592 B.
- **Why these and not the rollbacks.** They were named like *flags*, which is the precise hazard this lane's own rule exists to prevent: **name a binary for what it lacks, not for how old it is.** `-f0`/`-f2`/`-f2noarm`/`-f0noarm` say nothing about what they are, and they were the binaries most likely to be launched by accident. The `-O2` and arm questions they answered are answered and recorded; keeping the artefacts added risk and no information.
- **Post-prune inventory (supersedes the list above):**
  - `RIAPP` = 1,094,648 B, `build=2ecffd0`, **`-O0`** -- the deployed binary. **Still not viable for shipping**: ~1,590 xruns and a 92 ms `render_max` per tab cycle on the real song.
  - `RIAPP-old-no-audio-fail` = 1,093,672 B -- the rollback. Lacks the audio-failure report.
  - `RIAPP-old-no-dotdot-fix` = 843,448 B -- the 00:15 build. Lacks the `..` fix and the audio report.
  - `RIAPP.prev` = 776,608 B -- oldest; predates both.
  - **Flagged, not touched:** `RIAPP.prev` violates the naming rule (named for age, not for what it lacks). It is not this lane's binary and pruning an owner's rollback is their call, so it stays. Four on the stick is tolerable; nine was not.
- **Stick verified healthy after the deletions** -- one instance, no requester, close-before-get:
  ```
  audio: AHI low-level mode=0x003e0001 mix=48000 Hz buffer=256 frames period=5333 us
  RIAPP song Vk4aros:ReIncarnation/songs/local/the-knife/the-knife.rbng: 104 bars at 124 BPM
  ```
  So the deletions removed only the intended files and left the app path intact.
- 219 articles, 219 index rows, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | the reboot gave the cross-lane contrast, and it disproves QEMU's AC97 rate story
- Disposition: **New** (1 raw evidence file); **cascade correction** (2 articles + 2 index rows) — a follow-up to the two ingests earlier today, not a new topic
- Raw: `llm-wiki/raw/evidence/2026-10-03-riqemu1-rate-and-qemu-ac97-source.md` (what the reboot cost; riqemu1's clean return; the full `RI_RATE` block; `hw/audio/ac97.c`'s `0xbb80`/`EACS_VRA` paths; `ac97-main.c:39` and `:147`; `audio.c:253`; the host's three 48000 sinks; and the explicitly unsettled remainder)
- **A HOST COLD-REBOOT, RESTORED. riqemu1 IS BACK AND CLEAN.** `/tmp` took the VM, its spooler, its spool dir, its serial log and every `/tmp/ri/aros/*` artifact; all rebuilt or restarted. **Two windows only — no `Smart Filesystem request`**, despite the power loss having landed while RIAPP was running, and `exec rc=0` is the positive signal that **RIAPP is not running** (that defect is RIAPP-dependent). `P6 1280 1024`, so no black screen. `zombie-nation.rbng` and `T:` survived on disk.
- **WHY riqemu1 WAS THE ONLY LANE THAT STAYED DOWN: its spooler has no systemd unit.** `spike-laptop` (9292), `spike-s6` (9294) and `spike-v4` (9091) are all units and returned at 09:45:06; riqemu1's 9295 spooler was started by hand and did not. **The Dell guest still has not re-dialled** (it does not reboot with the host) and needs `SYS:ATCPBIN agent 192.168.1.81 9292 e6320` run on its console — the host address is unchanged, so that command is still correct verbatim.
- **THE HEADLINE: QEMU'S AC97 IS NOT FIXED AT 44100, AND THE 44100 FIX IS IMPOSSIBLE ON riqemu1.** From QEMU's own source (`hw/audio/ac97.c`, 11.1.1): the reset path stores **`0xbb80` = 48000** and calls `open_voice(s, PO_INDEX, 48000)`, and `AC97_PCM_Front_DAC_Rate` is **guest-writable whenever `EACS_VRA` is set**, with `as.freq = freq` used raw. **There is a rate knob and the default is 48 kHz.**
- **AND THE GUEST DOES NOT ASK FOR 44100 EITHER.** AROS's ac97 AHI driver hardcodes 48000 at `ac97-main.c:39` and `:147`, and the probe on riqemu1 returns **`range=48000-48000 nfreq=1`, `list[0]=48000`** — one mode, and **every** rate requested (44100 included) comes back as 48000. So the fix recorded this morning, "run AHI at 44100 so AC97 never resamples", **cannot work on that lane** and was always a no-op on the Dell, which already returns 44100 for everything.
- **WHERE THE 44100 COMES FROM IS NARROWED BUT NOT SETTLED, AND THE RECORD SAYS SO.** QEMU defaults an unspecified `-audiodev` frequency to **44100** (`audio.c:253`) and `start_riqemu1.sh` passes none; all three host sinks are 48000, so the host is not it either. Whether the device's `as.freq` overrides that template default **could not be read** — `sw_pcm_init` is absent from this tree. **The decisive experiment, not run:** `-audiodev pa,id=pa0,frequency=48000`, play, read the host stream rate. If it turns 48000 the fix is one word in a launch script, not a change to ReIncarnation.
- **WHAT SURVIVED: the measurement, not the attribution.** The guest does produce 48000 at exactly real-time pace (`xruns=0`, pacing ratio 1.00) and the host does play 44100. What was wrong was saying **QEMU** was the thing converting.
- **A SHARPENED RULE: a `BestAudioID` refusal says nothing by itself.** `req=44100 mode=INVALID (BestAudioID)` reproduces on riqemu1 — where it is **correct** (`nfreq=1`, only 48000 exists) — as well as on the Dell, where it is **incorrect** (44100 is `list[0]`). Same symptom, opposite verdicts, and only `AHIDB_Frequencies` tells them apart.
- Cascade: the AC97 article carries a Status block marking the mechanism disproven and the fix replaced with the `-audiodev` candidate; the Dell rate article's cross-lane table gains an AHI-modes column and a correction on the "QEMU resamples" attribution; both index rows refreshed so the index does not keep asserting the disproved claims.

## [2026-10-03] ingest | proving the VM window is actually visible
- Disposition: **New** (1 article + 1 raw evidence file); **cascade correction** (2 articles)
- Raw: `llm-wiki/raw/evidence/2026-10-03-window-visibility-and-session-scope.md` (the second reboot and its boot id; the healthy `P6` beside an empty compositor client list; the full session env; the `foot` test; **all six `hyprctl` attempts with their exact Lua errors**; the `systemd-run --wait --pipe` block; the `--collect` unit that could not be found; the working launch's geometry; `grim`'s three failures then its one success; the luminance computation; both framebuffer sizes; the `pgrep` self-match; the three systemd units against riqemu1's missing one; the Dell journal)
- Triaged first: `-daemonize` appears in three files but only as the `setsid` black-screen cause, `grim` appears only as two luminance numbers, `pgrep` in one unrelated 2026-10-01 article, and the systemd-unit gap appears nowhere. **The `systemd-run`/`-daemonize` interaction was recorded nowhere.**
- **TWO WORDS OF INSTRUCTION, SIX ATTEMPTS, THREE OF WHICH FAILED WHILE LOOKING LIKE SUCCESS.** *Restart the vm, make sure it's visible* — and the lane was up the whole time. **The window was not.**
- **`screendump` CANNOT ANSWER "IS IT VISIBLE".** A perfectly valid `P6 1024 768 255` coexisted with the compositor listing **no QEMU window at all**. The header describes the guest's VGA framebuffer and says nothing about the host desktop — **the recorded `P6 0 1600` lesson pointing the other way**, and worth having both halves of.
- **NOTHING FROM THE AGENT'S SHELL CAN MAP A WINDOW**, proved with `foot` rather than argued: it is a terminal the owner can obviously run, and it never appeared either. So the constraint is **session scope, not GTK** — with `DISPLAY=:0`, `WAYLAND_DISPLAY=wayland-1`, XWayland pid 1486, the wayland socket and the session bus all present and correct.
- **THE COMPOSITOR CANNOT LAUNCH IT EITHER.** `hl.dsp` enumerates to an **empty list** on this build, `hyprctl lua` does not exist (the Lua entry point is `hyprctl eval`), and `dispatch`'s Lua quoting contradicts its own `--help`. All six attempts are recorded with their exact errors.
- **THE REAL CULPRIT, AND IT IS A SILENT FAILURE WITH A SUCCESSFUL EXIT CODE.** `-daemonize` and `systemd-run` are incompatible: the unmodified script under a user service reported **`Finished with result: success`**, **`Service runtime: 101ms`**, and **no QEMU** — QEMU forks, systemd declares the unit done, `KillMode=control-group` kills the orphaned daemon, and `--collect` then left `Unit riqemu1-vm.service could not be found`. **A unit that reports success when its purpose was to keep a daemon alive is exactly the shape of failure that wastes an hour.**
- **FIXED AND VERIFIED IN PIXELS, NOT BY ASSUMPTION.** Identical arguments minus `-daemonize` under a tracked user service: `qemu window: True at [1287, 38] size [1261, 1390] mapped True`, then `mean=73.3 max=255.0` sampled from a compositor capture — which has a precedent to stand on, the earlier black screen measuring **2.1** and the working case **151.4** — and checked against the single monitor `DP-1: 2560x1440` so that fitting on screen is a fact. `exec rc=0` additionally proves RIAPP is not running, so no AHI contention.
- **A GENERAL TRAP IN ONE LINE: `pgrep -f` MATCHED ITS OWN COMMAND LINE** and reported a VM dead since the reboot as `running`. And `pgrep -x` is unusable here because the binary name exceeds 15 characters, so it warns and matches nothing.
- **RECORDED UNRESOLVED, NOT SMOOTHED OVER:** the guest framebuffer changed `P6 1280 1024` to `P6 1024 768` across the reboots with the guest healthy and the agent up. On a lane where `-vga vmware` is what makes wide modes reachable at all, that is worth a look.
- **WHY ONLY THIS LANE DIES ON A REBOOT:** `spike-laptop` (9292), `spike-s6` (9294) and `spike-v4` (9091) are all systemd units and all returned at boot; riqemu1's 9295 spooler is started by hand. Six lines would fix it.
- Cascade: the AHI loader article had claimed `hyprctl` was **absent** — it is installed, and `clients`/`monitors`/`eval` all work; what is missing is a **cursor or exec dispatcher**, because `hl.dsp` is empty. Corrected in place with a Status block, and its conclusion (a requester still needs a human) left standing on the right reason. The injection article gains the running-versus-visible distinction, the `-daemonize`/`systemd-run` trap, and a superseded note that `grim` on this host takes **only a positional output file** — no `-o`, no region — with `grim --help` failing.
- Lint: 220 articles + 9 raw sources, 0 unindexed, 0 dead index rows, 559 links 0 broken.

## [2026-10-03] ingest | the display mode is a GRUB kernel argument, and grub.cfg.mine was a deliberate backup
- Disposition: **New** (1 article + 1 raw evidence file); **cascade correction** (1 article)
- Raw: `llm-wiki/raw/evidence/2026-10-03-grub-is-the-display-control-point.md` (the `P6` observations, `file` proving both "config" files are ELF, the refused `screenmode.prefs` read, the full `menu.lst` title list with `vesa=` arguments, both menuentries, the live config's `set default`, the three files' sizes and md5s, **the complete `diff grub.cfg grub.cfg.mine`**, the 2026-09-30 rationale quoted, the applied edit and `IDENTICAL` read-back, the deletion with its `object not found` proof, and **both build traps**)
- Triaged against the wiki first — and that triage is what caught the real story: **the 2026-09-30 wide-modes record already explained the one-line diff I had just "fixed".**
- **ASKED TO MAKE riqemu1 ALWAYS START AT 1280x, I DIAGNOSED IT AT THE WRONG LAYER, WAS CORRECTED, AND THE CORRECTION EXPOSED A DELIBERATE DECISION.** The useful content is the correction and the trade-off, not the fix.
- **I WENT LOOKING FOR AN INTUITION PREFERENCE BECAUSE THAT IS WHERE THE MODE IS *DRAWN*.** `SYS:Prefs/ScreenMode` and `DEVS:Monitors/VMWare` are both **ELF binaries, not config**, and no `screenmode.prefs` existed at all — the read was **refused**, not empty. So I concluded the preference was absent, built one from AROS's own writer format, and got 1280x1024 across two cold boots. **Then the owner said the thing that should have been asked first: "we had to select 1280x ourself in the grub menu, which is before any intuition prefs."** **A boot-time display property is not owned by the layer that draws it.**
- **WHAT ACTUALLY OWNS IT:** the resolution is a **kernel argument on the bootstrap**, chosen by GRUB menu entry — `kernel /boot/pc/bootstrap.xz vesa=1280x1024x32 ATA=32bit nomonitors` — and the native entry passes **no `vesa=` at all**, so the bootstrap auto-detects and **the mode varies per boot**. That is the whole explanation for a display that changes between reboots on a deterministic machine.
- **THE ONE-LINE DIFF WAS A DECISION, NOT A SLIP, AND I ALMOST REVERTED IT.** `diff grub.cfg grub.cfg.mine` differed by exactly one line; my first instinct was that a pin had been *lost* and `.mine` was the surviving source of truth. The 2026-09-30 record already had it: **`nomonitors` keeps every monitor driver out, so the mode walk finds exactly 1 mode (1280x1024x24)** — hence `.mine` is the preserved **before** state and the diff **is** the change. **A file that exists but is never read is worse than a missing file: it looks authoritative and carries the answer you want.**
- **THE TRADE-OFF IS REAL IN BOTH DIRECTIONS, and the requirement was met at a cost.** Pinned = deterministic 1280x1024 but **1 mode**; native = **23 modes** including 1920x1080 but varies per boot. Three cold boots verified at `P6 1280 1024`, the last two with **no Intuition preference present at all**, window mapped, luminance 120.3. **The wide-mode capability is the thing given up, and that call is the owner's.**
- **DELETING A GUEST FILE NEEDS A BINARY.** The Shell accepts only Run/SetEnv/echo/dir/status/wait/Break/Getenv and the agent has no delete verb, so removing my own prefs file needed a purpose-built AROS unlink tool. **Absence of complaint is not proof** — a silent no-op and a successful unlink look identical, so `object not found` is the evidence.
- **TWO BUILD TRAPS THAT IMPERSONATED OTHER PROBLEMS.** A **relative SDK path** silently broke the library shim — `libcrt.a` resolving to a nonexistent `/tmp/ri/Vulkan4Aros/...` — reported as `cannot find -lstdlib`, which reads like a missing dependency rather than a dangling symlink. And **`head` on a compile pipe killed gcc with SIGPIPE**, so the `.o` was never written and the **linker** reported it: **a missing object file named by the linker is a compile-stage failure until proven otherwise.**
- Cascade: the 2026-09-30 wide-modes record's parked lane state now carries a **Status block** marking that the GRUB line no longer holds, reproducing the trade-off as a table, recording that `grub.cfg` and `grub.cfg.mine` are now byte-identical (7148 B) so **`.mine` is no longer a backup of the superseded state**, and leaving its original finding intact rather than rewriting it.
- **Left open rather than smoothed:** the **depth**. The entry requests `x32`, the 2026-09-30 record observed **1280x1024x24**, and `screendump` emits RGB regardless, so the discrepancy is unexplained and would need a HIDD/`GfxBase` probe.
- Lint: 221 articles + 10 raw sources, 0 unindexed, 0 dead index rows, 576 links 0 broken.

## [2026-10-03] ingest | The -O2 tab cost is preemption of prebuilt MUI code
- Disposition: **New** (`2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md`) + supersession marks on two records
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, section "The page switch, split three ways"
- Supersedes in part: `2026-10-03-correction-the-o2-tab-cost-is-not-the-damage-box-build.md` (its point 4); re-reads `2026-10-03-the-governor-arm-is-load-bearing-at-o0-and-irrelevant-at-o2.md`
- **Ran the probe the correction article named**, after the host **and** Dell both rebooted. Lane reconnected on its own within ~1 min of the guest coming back -- the second time this has happened, which strengthens the "it sometimes reconnects on its own" note in the lane-traps record.
- **Rebuilt the click harness from the wiki click map rather than from memory.** `/tmp` was wiped by the host reboot, so `abba.sh` was gone. Reconstructed it from the recorded coordinates and re-verified every click against its event (`TR PLAY TAB page=0..4 TR STOP` in all four cells). **A harness that lives only in `/tmp` is not a harness, it is a transcript** -- the same lesson as the evidence file, one level up.
- **The probe:** `tab_switch()` does three things; time each with the same `ReadEClock` arithmetic the surrounding `TAB` line already uses. **`app/riapp.c` in the repo is untouched** -- instrumentation lives only in a clean `git archive HEAD` tree, exactly as the arm-disabled build did.
- **One candidate eliminated.** `tabs_us` (five `MUIA_RArt_Active` writes) is **identical** across arms: 1,356 / 1,360 against 1,322, ~1 % of the tab cost either way.
- **The bulk is `page_us`** = `SetAttrs(MUIA_Group_ActivePage, ...)`: **83.4 ms -> 200-219 ms, 2.4-2.6x**, `-O0` pair agreeing to **0.014 %**. That call is **MUI's own code**, which `-O2` does not change -- the flag covers our 81 TUs and Intuition/`MUI_MakeObject` come from the SDK unaltered. Identical code, 2.5x the time.
- **The section repaint is three orders of magnitude short of it.** `box_steps` costs ~1 us (`-O0`) and ~11 us (`-O2`) per repaint against `page_us` of 21 ms and 50 ms per switch. `box_none` is 139-600 samples at `-O0` and **zero** at `-O2`; `build_avg` 157-520 against 238-324 us. The bounded build is neither the cost nor the fix -- confirmed from an independent direction.
- **THE CORRECTION, and it is mine.** The previous pass concluded "not contention" because `render_total` is half at `-O2` (15,933 vs 31,540 ms). **Wrong measurement for the question**: that is CPU *consumed*, not *preemption pressure*. The quantity that answers it is how late the render task is, and `wake_max` -- already logged in every run -- inverts completely:
  - `-O0` 5,820 / 5,816 us -- **the audio task is the one WAITING**
  - `-O2`   398 /   436 us -- it finishes in 2.6 ms of a 5,333 us period and preempts the GUI ~187x/s
  So at `-O0` the render task overruns, the arm trips six times and pushes it **below** the UI, so MUI runs unimpeded; at `-O2` it finishes early, **never yields**, and takes the CPU from the GUI. **It IS contention**, and points 1-3 of the correction stand.
- **This also re-reads the arm record.** `arm_us=0` at `-O2` was recorded as "the arm never engages" -- a neutral fact. The fuller statement: **the arm's absence is the cause of the GUI regression it was measured against.** The arm is load-bearing at `-O0` and its correct un-necessity at `-O2` is what costs the GUI.
- **Bimodality localised.** `rail_us` is 5 us on page 4 in **every** cell and 3,209-6,812 us at `-O0` against 9,949-239,355 us at `-O2`; B2 also spikes `tabs_us` (3,835 against ~350). So the spikes are in `SetAttrs` on our own widgets **generally**, not one call -- a bounded next question instead of an open one.
- **Shipping, now fully priced.** No flag is simply better. `-O0`: not viable (~1,590 xruns, 92 ms `render_max`). `-O2`: 0 xruns, but the GUI is preempted ~187x/s by an audio task that no longer yields. If GUI latency at `-O2` is unacceptable the lever is a yield policy triggered on **GUI latency** rather than 2 s of continuous over-budget audio -- an owner decision that trades audio for responsiveness on purpose.
- **Method findings.** (1) **"CPU consumed" is not "preemption pressure"** -- the wrong proxy produced a confidently wrong "not contention" that survived a full commit and a lint pass. (2) **Instrumenting three lines answered in one pass what two experiments could not**: eliminated a candidate, attributed the bulk, and localised the bimodality -- none reachable from totals. (3) **Ruling a candidate out is a result**: `tabs_us` being *identical* across arms is as informative as a cost being high. (4) **A correction that reframes an earlier finding beats a new number** -- "not contention" was wrong, and its replacement explains both the arm result and the repaint result at once.
- **Instrumented binaries pruned from the stick after the run** (`RIAPP-p0`, `RIAPP-p2`), leaving the four that are named for what they lack.

## [2026-10-03] ingest | the LEVI sub-split is measured, and all four internals sit at the floor
- Disposition: **New** (1 article + 1 raw evidence file); **cascade update** (1 article, standing gap marked CLOSED)
- Raw: `llm-wiki/raw/evidence/2026-10-03-levi-subsplit-riqemu1.md` (the v1 build line and transfer, the empty `parec` beside the live sink input, the song identification, **the complete settled `dstg` table verbatim**, the derived shares with their components, the floor list, the 1.09-versus-2× arithmetic, the outliers, and the exec/ui drive sequence)
- Closed a gap that had been open and **explicitly named** since 2026-10-02: *"LEVI's internal split: `arp` vs `seq` vs `voice_render_sum_stereo` vs the stereo section mix."* The instrumentation was already built and mutation-proven; **what was missing was a lane, and riqemu1 became usable once AHI worked.**
- **THE ANSWER IS A LIMIT, NOT FOUR NUMBERS.** `zero` is a no-op that clears `ml/mr/sendbus` and reads **`avg=6 us`**; six of sixteen stages read exactly 6, so **6 µs per block is the instrument's resolution, not a cost.** Excess over floor: **`arp +0 seq +0 voice +2 mix +0`** — about **2 µs of LEVI's 57**. **54 % of LEVI is unattributed by the four regions the gap named.** Either the bulk of `levi_block` sits outside them or they are the wrong regions, and **this table cannot distinguish those two** — recorded as a limit rather than smoothed into an answer.
- **LEVI IS NOT THE OUTLIER HERE: 16.2 % of block, third**, behind 303a (19.9 %) and 303b (19.4 %). **That does not refute the 2026-09-02 ~30 % headline, and the reason is the useful part:** the lane's resample factor is `44100/48000 = 0.91875`, about **1.09**, and the gap is roughly **2×**, so the runs differ in configuration and a like-for-like repeat is needed. **Checking the resample bound before calling a discrepancy a refutation is what prevents both a false refutation and a false confirmation.**
- **PLAYBACK PROVEN WITHOUT CAPTURING A SAMPLE.** `parec` returned nothing — this host is PipeWire, so `@DEFAULT_MONITOR@` no longer means what it did under PulseAudio — and the evidence that settled it was `Sink Input #741  Corked: no` plus the sink moving SUSPENDED to RUNNING. **The recorded "peak 0 proves nothing" lesson arriving from the opposite direction: a capture yielding nothing at all is equally uninformative.**
- **Transport driven by `sendkey spc`** over the QEMU monitor, not a click — which is why it works where injected clicks arrive nowhere. The launch-measure-close-read sequencing held exactly as recorded: `exec rc=1` with no output for the whole time RIAPP ran, `rc=0` immediately after `--ui-close`.
- Lane note: a **fourth host reboot** (14:23, boot id `13ad72d1`) had killed riqemu1 and its spooler; both restored. `P6 1280 1024` on first boot after it, so the GRUB pin is confirmed durable across a reboot. `/tmp/opencode/start_riqemu1_visible.sh` had to be recreated — it lives in `/tmp` and this is the fourth time that has cost something.
- Also recorded: `render_max=5034 us` against a 5333 us period with `block max=3048` against `avg` 351 — one block, not a sustained stall.
- Lint: 223 articles + 11 raw sources, 0 unindexed, 0 dead index rows, 607 links 0 broken.

## [2026-10-03] ingest | The rail is flag-independent -- and it is not the -O2 cost
- Disposition: **Update** (`2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md` gains the rail probe and a full budget decomposition; no new article, because this refines a claim already made rather than opening a new thesis)
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, section "The rail, per widget"
- **Ran the probe the previous record named**: time each of `rail_for_tab()`'s five `SetAttrs(s_devbtn[d], MUIA_ShowMe, ...)` individually. Same discipline as the two probes before it -- **`app/riapp.c` in the repo untouched**, instrumentation only in a clean `git archive HEAD` tree.
- **One build stumble worth recording:** the first two attempts failed to compile because I assumed a `s_efreq` guard that does not exist in `app/riapp.c`. `eclock_open`/`s_efreq` live in `gui/widgets/rsection.mcc.c`; `riapp.c` guards on `TimerBase` and takes the frequency from `ReadEClock(&v)`'s return. **Copy the guard from the function you are instrumenting, not from the nearest one that looks similar** -- the surrounding `tab_switch` had the right shape three functions away.
- **Result: same number of writes, same total, flag-independent.**
  - `-O0`: 5 startup writes (22 us), **11** tab-switch writes, **35,215 us** total, worst single 21,722 us
  - `-O2`: 5 startup writes (21 us), **11** tab-switch writes, **33,931 us** total, worst single 8,586 us
  - **`-O2` is 3.6 % CHEAPER.** The rail is therefore **not** part of the `-O2` regression; it is a separate fixed ~34 ms per five-tab cycle at both arms.
- **The per-write cost is the real anomaly: ~3,200 us average to set one boolean visibility flag** (35,215 / 11). `MUIA_ShowMe` invalidates the object's group and the group holds the section widgets, so each write drags a relayout behind it. The `shown[]` cache already limits writes to real changes, so 11 per cycle is near-minimal for a five-device rail driven one button at a time.
- **This REFINES the previous record rather than confirming it.** That record concluded "the spikes are in `SetAttrs` on our own widgets generally, not one call" -- still true, but flag-independent, so not the `-O2` cost. Recorded as a refinement because the distinction matters: chasing the spikes would have been chasing a fixed cost that neither explains nor worsens the regression.
- **The tab-cycle budget now decomposes:**
  | component | `-O0` | `-O2` | flag-dependent? |
  |---|---|---|---|
  | `page_us` (MUI group page switch) | 83,435 us | 200,469-218,735 us | **yes -- the whole `-O2` cost** |
  | `rail_us` (five `MUIA_ShowMe`) | 35,215 us | 33,931 us | no |
  | `tabs_us` (five `MUIA_RArt_Active`) | 1,356 us | 1,322 us | no |
  | **total** | **99,390 us** | **246,200-479,966 us** | |
  Two of three components are flag-independent and together account for ~36 ms of **fixed** cost that a batching change could attack on its own merits, independently of the flag decision.
- Instrumented binaries pruned from the stick after the run (`RIAPP-r0`, `RIAPP-r2`); the stick is back to the four binaries named for what they lack.
- Sibling lane's untracked `llm-wiki/raw/evidence/2026-10-03-levi-subsplit-riqemu1.md` left untouched.
- 222 articles, 222 index rows, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | Cross-lane: the LEVI sub-split answers a gap of mine, on a lane that does not transfer
- Disposition: **Update** (cascade into two records this lane wrote; no new article — the finding is the sibling's and is already recorded)
- Updated: `2026-10-02-five-sections-one-culprit-levi-is-32-percent.md` (its standing gap), `2026-10-02-render-stage-breakdown-voices-are-94-percent-not-the-fx-chain.md` (a cross-lane caveat), `2026-10-03-correction-the-o2-tab-cost-is-not-the-damage-box-build.md` (a floor caveat on the device figures it quotes)
- **Triage finding first:** the sibling's `f672a37` ("the LEVI sub-split is measured, and all four internals sit at the floor") closes a standing gap **this lane** recorded on 2026-10-02 -- *"LEVI's own block is five calls ... timed together. Which of those carries the 398 us is not established, and the obvious next split."* A cascade update is the skill's requirement here, and the honest version is not "answered" but "**answered elsewhere, and it does not transfer**".
- **It does not transfer, and the reason is quantitative.** On **riqemu1 (ABIv1)**: `lev-arp` 6, `lev-seq` 6, `lev-voice` 8, `lev-mix` 6 us -- all four at the timer floor, 54 % of LEVI unattributed. On the **Dell**: `lev-arp` 4, `lev-seq` 4, `lev-mix` 4 us and **`lev-voice` 306 us** -- i.e. ~87 % of Dell LEVI's 350 us in one sub-stage, against ~46 % on riqemu1. Two lanes, genuinely different LEVI internal distributions.
- **The rank change is not a refutation, and the sibling says so itself.** riqemu1 puts LEVI 3rd at 16.2 % of block (303a 19.9 %, 303b 19.4 %); this lane's Dell record has LEVI 1st at ~32 %. Their record explicitly declines to call it a refutation: different song, section enablement and optimisation level, with a resample factor of only ~1.09 against a 2x difference. **The like-for-like repeat is owed on the Dell**, because every audio measurement in this lane is a Dell measurement. Recorded in the gap rather than left as a bare strikethrough.
- **A floor caveat this lane's own figures needed.** The riqemu1 record names a gap I had not applied to my own tables: **there is no `n` on the sub-stage rows**, so a sub-stage reading at the floor is indistinguishable from one that never ran. Three of the Dell `lev-*` rows read 4 us -- at that floor. Checked which of my figures depend on them:
  - `dstg levi` (350 / 737 us) and `dstg block` (660 / 1370 us) -- **above the floor, still usable**, and these are the figures the `-O2` and correction records actually quote
  - `lev-arp` / `lev-seq` / `lev-mix` at 4 us -- **at the floor, not quotable**; only `lev-voice` (306 us at `-O2`) clears it
  Recorded so that a future attempt to split Dell LEVI further adds a count before trusting a number.
- **The proportion finding survives; the internals do not.** VOICES is ~94 % of the block and the FX chain ~1 % -- that is what the render-stage record is about, and nothing here disturbs it. What is now qualified is any claim about *which* internal carries it.
- **Tree state:** sibling's WIP present and untouched (`llm-wiki/index.md`, `2026-10-03-riqemu1-ac97-...md`, untracked `llm-wiki/raw/evidence/2026-10-03-audiodev-frequency-rejected.md`). AUDIT 0/0 PASS.

## [2026-10-03] ingest | the LEVI sub-split re-measured, with a control — and the first window was an artefact
- Disposition: **New** (1 raw evidence file); **cascade correction** (2 articles — one of them the record written earlier the same day)
- Raw: `llm-wiki/raw/evidence/2026-10-03-levi-subsplit-corrected-with-a-control.md` (the control stage verbatim, the full table, the floor arithmetic, **every dump of both runs**, the `playing=` windows, the three-way comparison, and the method notes)
- **ITEM 6 FIRST, THEN ITEM 4, as asked. The audiodev fix is REJECTED BY EXPERIMENT** — see the previous log entry — and recorded in `raw/evidence/2026-10-03-audiodev-frequency-rejected.md`: `Parameter 'frequency' is unexpected`, and **no** backend (`pa`, `alsa`, `pw`, `none`, `sdl`, `coreaudio`, `jack`) accepts it, so there is **no launch-line knob at all**. **The guest is exonerated**: AROS's ac97 driver mentions a rate twice, both 48000, and **never writes a codec rate register**. The bound is recorded rather than a mechanism, because the QEMU source tree on this host is intermittently unreadable and the trace could not be closed.
- **ITEM 4 ADDED TWO STAGES.** `lev-tempo` wraps `levi_set_tempo`, the one call inside SLEVI that no sub-stage covered. `lev-probe` is a deliberate **CONTROL**: an empty open/close pair whose reading IS the cost of one stage pair, placed **outside SLEVI's interval** so the instrument does not inflate the region it measures and the before/after SLEVI numbers stay comparable.
- **THE CONTROL SETTLED TWO THINGS AT ONCE.** `lev-probe` reads **6 µs** — the same floor the six no-op stages read — so 6 µs is the instrument, not the code. And `lev-tempo` also reads 6, so **`levi_set_tempo`'s real cost is 0 µs**, closing it as a candidate exactly where inspection (three float compares and a store) had said it was trivial.
- **AND THE CONTROL EXPOSED A WRONG CONCLUSION OF MY OWN, FROM AN HOUR EARLIER.** `lev-voice avg=8 us` was not a floor artefact — it was **the song's first 32 seconds**, where the Levi part of Zombie Nation does not sound. `lev-voice` sat at 8 µs across **every** dump of run 1, which reached only `playing=5942` buffers (31.7 s). Run 2 left playing for 193 s (`playing=36200`) and got **`lev-voice 152 us`, `levi 213 us`, LEVI at 42.7 % of block**, with `lev-voice` climbing 65 → 159 → 183 → 138 → 149 → 152 as the arrangement progressed.
- **SO LEVI IS 42.7 %, NOT 16.2 %** — *above* the Dell's ~30 % headline rather than below it, which **retires the "does not refute" caveat** the first record raised. The residual inside LEVI is **37 µs (17 %)**, not 54 %.
- **THE GENERALISABLE LESSON: a cumulative average is only as meaningful as the window it accumulated.** The `dstg` table is `sum/n` over every block since start, `n` grew to 24042 in one run and 146481 in the other, and **nothing in the table says which part of the arrangement it covered**. Two runs of one song, minutes apart, differing only in how long playback was left running, gave 8 µs and 152 µs for the same stage.
- Code: `engine/engine.h` (two stages, TOTAL 17 / COUNT 18, `SUB_LAST` now LEVPROBE), `engine/engine.c` (both stages placed as documented), `app/riapp.c` (name table), `tests/unit/t156_render_stages.c` (COUNT 16 → 18). **Lesson worth keeping: `ri_build_host.sh` links every `.o` in `/tmp/ri/build`, so a `test` target after a source edit silently links a STALE `engine.o` against the new enum** — the symptom was two new stages reporting `n=0`, which looks like a broken stage and is a stale object. Fixed by building the `engine` module first. `AUDIT 0/0 PASS`, t156 PASS, v1 RIAPP built (`r12moves 0`).
- Lint: 224 articles + 12 raw sources, 0 unindexed, 0 dead index rows, 0 broken links.

## [2026-10-03] ingest | The like-for-like LEVI repeat, on the Dell -- the gap this lane owed
- Disposition: **New** (`2026-10-03-on-the-dell-levi-is-64-percent-of-the-block-and-91-percent-is-one-call.md`) + closure of the standing gap in `2026-10-02-five-sections-one-culprit-levi-is-32-percent.md`
- Evidence: `docs/evidence/audio/audio-failure-and-deadline-2026-10-02.md`, section "The like-for-like LEVI repeat, ON THE DELL"
- **This is the step the previous cascade explicitly recorded as owed**: *"why do the Dell and riqemu1 distribute LEVI differently, and which is right for the Dell?"* -- and **the repeat is owed on the Dell, because every audio measurement in this lane is a Dell measurement.** Ran it.
- `84f46cc` supplied what the repeat needed: the fifth sub-stage (`RI_ENGINE_ST_LEVTEMPO`, `levi_set_tempo` -- the previously uncovered SLEVI call) **and a control**, `RI_ENGINE_ST_LEVPROBE`, an empty T/E pair that measures the per-pair floor directly instead of leaving it inferred.
- **Binary** `RIAPP-h` 1,099,200 B, `r12moves=41`, HEAD `84f46cc`, `-O0`, ABIv11. The Knife, 104 bars @124 BPM, 4,344 playing buffers, live AHI.
- **THE ANSWER, and it is the opposite of riqemu1's:**
  ```
  block      avg=1767 us
  levi       avg=1139 us   (64 % of block)
  lev-voice  avg=1042 us   91 % of levi
  lev-mix    avg=58   us    5 % of levi
  lev-arp / lev-seq / lev-tempo   4 us each      AT FLOOR
  lev-probe  avg=4    us                              THE FLOOR CONTROL
  hb: xruns=1586 render_max=8868 us overloads=6 load=1193/1000
  ```
  internals sum to **1,116 of 1,139 us** -- so **unattributed is 23 us = 2 %**, against riqemu1's 16.2 % share and 54 % unattributed.
- **"54 % of LEVI unattributed" was never a mystery about LEVI.** It is what the same code looks like when `levi_voice_render_sum_stereo` is itself at the floor on that lane. One row at 1,042 us explains both figures -- **the totals alone would have invited a false refutation in either direction.**
- **`lev-probe` retires this lane's own floor caveat.** An empty T/E pair reads **4 us -- exactly what `lev-arp`, `lev-seq` and `lev-tempo` read**. The correction article recorded that those rows carry no `n` and so "a floor reading is indistinguishable from one that never ran". **A control that reads the floor beats an argument about what the floor means**, and that caveat is now closed for this configuration.
- **The 2026-09-02 record's "LEVI is 32 %, the outlier" was directionally right and much too low** for this song at this configuration (64 %). Recorded as a correction rather than a refutation, with the caveat kept: **one song, one configuration.** A second song on the Dell would tell us whether 64 % is "LEVI on The Knife" or "LEVI on this guest".
- **Render-stage answer at last, on the machine that matters:** the target is **`levi_voice_render_sum_stereo`** -- not LEVI as a bucket, not the arp/seq/mix split, not a timer-floor artefact. And it is itself a whole render block, so splitting *it* is the same step just performed on LEVI, with the same control discipline.
- **A wiki-convention check worth recording:** `levi-subsplit-corrected-with-a-control.md` is an **evidence** record under `raw/evidence/`, not an article under `raw/articles/`, so the Prior link points there. Guessing the article path would have produced a broken link; the link checker caught it before commit -- which is the second time this session that a checker caught an error of mine that a plausible guess would have shipped.
- Instrumented binary `RIAPP-h` left on the stick deliberately: it is HEAD's code, so unlike the `-O2`/arm experiment binaries it is not a one-off artefact. The four named rollbacks are untouched.
- 224 articles, 224 index rows, 0 without a row, 0 broken links. AUDIT 0/0 PASS.

## [2026-10-03] ingest | the LEVI residual was the sub-split measuring itself
- Disposition: **New** (1 article + 1 raw evidence file)
- Raw: `llm-wiki/raw/evidence/2026-10-03-levi-residual-is-the-instrumentation.md` (the four-run table, the control's placement shown as code, run 4's complete `dstg` table verbatim, the per-pair arithmetic, the corrected LEVI figures)
- VM restarted at the owner's instruction (the one that had actually died was `nvkcard`, the other session's), the **latest build deployed under a new RAM name** (`RAM:RIAPP_LeviProbe`, then `RAM:RIAPP_LeviProbe2`), window verified mapped, `P6 1280 1024` intact.
- **THE RESIDUAL WAS THE INSTRUMENT.** `levi 213 - (arp 6 + seq 6 + voice 152 + mix 6 + tempo 6) = 37 us` had two equally good explanations — real uncovered work, or the split charging LEVI the cost of each stage pair it contains — and **one table cannot separate them.** A **second control** was added: `lev-probe2`, an empty pair placed **inside** SLEVI where `lev-probe` already sat outside, so the same pair is measured from both sides of the boundary.
- **FOUR RUNS: RESIDUAL 37 (5 inner pairs), 38 (5), 44 (6).** One extra stage pair moved it by **+6 us, exactly one pair**, while `lev-voice` moved 152 → 176 → 189 over the same runs. **The residual tracks the number of stages, not the music** — which is what identifies it rather than merely explaining it. Both controls read 6 us.
- **SO ~83 % OF THE RESIDUAL WAS THE SPLIT MEASURING ITSELF, and the real residual is ~8 us — stable at 7, 8, 8 across three runs, about 3 % of LEVI.** Slope exactly one pair per stage; intercept barely moving.
- **THE TAX IS NOT CONFINED TO LEVI, AND THAT IS THE MOST TRANSFERABLE PART.** SLEVI's own figure includes its sub-split (`263 - 6x6 = 227 us` real; **48.9 % reported becomes 42.2 % corrected**), and **every wrapper containing N instrumented children reads about N x 6 us high** — `block`, each section leaf, and SLEVI. **Every section and block figure in every table recorded so far carries that tax and it was never subtracted.** It does not invalidate them (6 us against a 351–538 us block) but leaf-against-wrapper comparisons were comparing numbers differing by the wrapper's own child count.
- **WHERE LEVI STANDS:** 16.2 % (intro-window run) against **42.7–48.9 % reported, 42.2 % corrected**, versus the Dell's ~30 %. `lev-voice` alone at 183 us corrected is the largest single item in the block — bigger than 303a and 303b together.
- Code: `lev-probe2` added (`engine.h` TOTAL 18 / COUNT 19 / `SUB_LAST` LEVPROBE2, `engine.c`, `riapp.c` name table, `t156` COUNT 18 → 19). The **stale-object trap** bit again and was handled the same way — `ri_build_host.sh engine` before the test target. `AUDIT 0/0 PASS`, t156 PASS, v1 build `r12moves 0`.
- Left open: the remaining ~8 us inside LEVI; correcting the older tables (including the 2026-09-02 Dell figures, taken before any of this was known); and sub-6 us resolution, since both controls read 6 and that is also the floor.

## [2026-10-03] ingest | cross-lane: the floor, and so the child-count tax, is lane-dependent
- Disposition: **Update** (1 article) — no new article; this is a cross-reading of another lane's finding against this lane's
- Raw: none added. Both source records already exist: `2026-10-03-levi-residual-is-the-instrumentation.md` (mine) and the Dell repeat's own article.
- **THE OTHER SESSION'S DELL REPEAT LANDS THE CONTROL ON A SECOND MACHINE, AND IT READS 4 us WHERE riqemu1's READS 6.** `lev-voice avg=1042 us  91 % of levi` and `lev-probe avg=4 us`. **The per-pair cost is not a constant — it is a property of the machine — so the "6 us per stage" figure is riqemu1-specific and the tax is ~4 us per stage on the Dell.**
- **TWO CONSEQUENCES, IN OPPOSITE DIRECTIONS, AND BOTH MATTER.** riqemu1's LEVI figure needs the correction most: reported 48.9 % is really 42.2 %, a 6.7-point correction, because its pair cost is the higher of the two. **The Dell's 64 % is barely inflated** — six inner stages at 4 us is about 24 us against a much larger block — so **the child-count tax is NOT what explains the 16.2 % / 64 % gap**, and that gap remains a configuration and song difference exactly as the Dell record concludes.
- **THE TWO LANES CONVERGE ON THE SAME SHAPE**, reached independently from opposite starting points (riqemu1's intro-window 16.2 % and the Dell's 64 %): LEVI is 42.2 % corrected on riqemu1 against 64 % on the Dell, and `lev-voice` is **81 % of LEVI** (183/227) against **91 %**. **`levi_voice_render_sum_stereo` is the target on both machines, and it is a *render* rather than the sequencer, arpeggiator or bus mix.**
- **Housekeeping that resolved itself.** The other session's `0b04d05` committed the shared `llm-wiki/index.md` and `log.md`, which carried this lane's rows with it — so the shared-file problem that had been leaving four rows uncommitted is gone. `index.md` is committed here with this lane's four remaining rows plus a reposition of the Dell row, whose article is already in the tree.

## [2026-10-03] ingest | Correction: the LEVI residual was the split measuring itself
- Disposition: **Update** (`2026-10-03-on-the-dell-levi-is-64-percent-of-the-block-and-91-percent-is-one-call.md` gains a Correction block; no new article — the finding is the sibling's and the correction belongs on the claim it corrects)
- Evidence: `llm-wiki/raw/evidence/2026-10-03-levi-residual-is-the-instrumentation.md` (sibling's), plus `2026-10-03-the-sub-split-charges-levi-6us-per-stage-it-contains.md` (the article)
- **The sibling's `4e4086c` corrects my own record, and this is the ingest.** Their second control, `lev-probe2` **inside** SLEVI, reads 6 us exactly like `lev-probe` outside it, and the slope is one timer pair per stage with the intercept barely moving -- so the "6 us per stage" is a measurement, not a story.
- **Applied to my Dell run (5 inner pairs):**
  ```
  levi                   1,139 us
  internals sum          1,112 us
  residual as reported      27 us = 2 % of levi
  split tax, 6 us x 5 pairs = 30 us   <- EXCEEDS the residual
  corrected residual        -3 us ~ 0 %
  lev-voice corrected    1,036 us = 90 % of levi
  ```
  **My "unattributed is 23 us = 2 %" was wrong. It is approximately zero** -- the whole residual was the split measuring itself.
- **The two lanes now agree completely.** They previously appeared to disagree by 52 percentage points (2 % vs 54 % unattributed); that gap was the instrumentation, not the code. Worth stating plainly because it is the second time in two days that a cross-lane "contradiction" dissolved once both sides' *method* was read rather than their totals.
- **The headline survives, and is cleaner.** `levi_voice_render_sum_stereo` at **1,036 us is 90 %** of LEVI's 1,139 us, and LEVI is 64 % of a 1,767 us block -- with no unexplained remainder competing for attention any more.
- **A quieter consequence recorded rather than left standing:** every `levi` figure this lane has quoted since the sub-split landed carries the tax -- **5.1 % inflation at `-O2`** (350 -> 332 us corrected) and **2.6 % at `-O0`** (1,139 -> 1,109 us). The `-O2`/`-O0` ratio the flag work rests on moves from 2.3x to 2.2x. **The flag conclusion is unaffected; the absolute figures were inflated and are now corrected.**
- **The floor caveat from the previous ingest also firms up.** That caveat said the at-floor `lev-*` rows might mean "never ran". The two-control result settles it in the other direction: they run, and they cost the timer tax. Reading 4-6 us is the *floor*, not the absence of work.
- Audit green; sibling's `llm-wiki/index.md` WIP untouched (uncommitted, one modified file, not staged).

## [2026-10-04] ingest | the next split's instrumentation built, its measurement blocked by a guest crash in utility.library
- Disposition: **New** (1 article + 1 raw evidence file); **cascade addendum** (1 article)
- Raw: `llm-wiki/raw/evidence/2026-10-04-render-task-crash-and-ahi-contention.md` (the counting-not-timing reasoning, the `t136` contract and the guest's counter row, both runs' `playing`/`stopped` with the freeze-and-restart progression, the zero-activity result, the present song with both demo-song lines absent, both `Getenv` refusals, **the complete requester text for both requesters**, all three failed `sendkey` attempts, the v11 build line, and an explicit "not established" list)
- **`AUDIT 0/0 PASS`** at the start of the session and again after the change; `t136_levi_stereo` PASS with the new counter contract; v1 and v11 both built.
- **A TIME SPLIT INSIDE `levi_voice_render_sum_stereo` IS IMPOSSIBLE, SO THE INSTRUMENTATION COUNTS WORK.** The function averages ~3 us per sample and a clock read costs 4 us (Dell) to 6 us (riqemu1), both measured by the controls — **a clock read costs more than the code it would measure** — and the regions interleave inside the sample loop, so a per-block split cannot be had either. Six counters on `struct RILeviSet`, reset per block, logged as `RIAPP vcount:`, contract pinned in `t136_levi_stereo`. **The contract holds on the guest: `samples 64`, `voice_calls 512 = 64 x 8`.** The suspect it was built for is visible in the source: `RI_LEVI_NLFO`(5) x `RI_LEVI_NVOICES`(8) per sample against only 8 voice calls.
- **BOTH RUNS PRODUCED NO MEASUREMENT, AND THE REASON IS A CRASH.** `playing` froze at 3856 while `stopped` climbed without bound, then restarted from the top; **no block in either run had one active Levi voice.** The song never loaded although `zombie-nation.rbng` is present, and **neither demo-song log line appeared** — a path that reaches neither success nor its documented miss.
- **THE CRASH, AND IT IS PROBABLY THE SAME BUG CLASS THIS LANE ALREADY PAID FOR.** `Task: RIAPP render`, `Error: 0x00000008 - Privilege violation`, `Module utility.library Segment 5.text Offset 0x0000000019D2`. The loader places sections at 12 mod 16 despite `sh_addralign = 16`; `ahi.device`, `ac97.audio` and `void.audio` were patched guest-side and **`utility.library` was not**. A *fixed* offset is what an alignment-sensitive site looks like rather than a data-dependent one, so **the patch list is not complete**. Whether the counters caused it is **not established** — they do not call `utility.library` and the audit is green, but the bisect needs a guest that boots.
- **THE MOST REUSABLE FINDING: AHI CONTENTION SILENTLY CHANGES THE RENDER PATH.** *"The sound card could not be opened. Another program may already be using it. RIAPP will run without sound (offline render only)."* **A stale holder does not fail the launch, it substitutes a different render path**, and the only warning is a requester this lane cannot dismiss — which is how a run can look healthy, live uncorked stream and responsive transport, while measuring something else entirely.
- **TWO CORRECTIONS.** A crashed RIAPP **survives `--ui-close`**. And **`sendkey tab` then `ret` does NOT dismiss a `Software Failure!` requester** — `tab;ret`, `tab;tab;ret` and `spc` all left it up, which **corrects** the lane record where that sequence dismissed a `Smart Filesystem request`. **One requester dismissing once is not evidence that requesters dismiss.**
- Lane: riqemu1 **blocked on the requester** (human click on Suspend, or a guest reboot). The **v11 Dell binary with the same counters is built and waiting** (`1100260 B`, `r12moves=41`) and is where the split matters most — `lev-voice` is 91 % of LEVI on the Dell against 81 % here. `start_riqemu1_visible.sh` recreated again: the sixth `/tmp` loss on this lane.

## [2026-10-04] ingest | an unattributed 48 kHz stream — recorded, and explicitly not counted as evidence
- Disposition: **New** (1 raw evidence file); **cascade note** (1 article). No new article: one observation, and it does not yet support a thesis.
- Raw: `llm-wiki/raw/evidence/2026-10-04-a-48khz-stream-coexisted-with-the-guests-44k1.md` (both sink-input stanzas, why the second is unattributed, why it is not the guest, and what would settle it)
- **TRIAGE NOTE.** Everything from the crash work was already committed in `b1b8b40`, and the lint was clean (226 articles + 15 raw sources, 660 links, 0 broken). So this ingest is deliberately small: **one observation that was captured but never recorded**, and it bears on an open question.
- **WHAT WAS SEEN.** Two uncorked sink inputs at different rates: `Sink Input #403 s16le 44100 application.name = "riqemu1"`, and `Sink Input #440 float32le 48000` with **`application.name` not captured** — the grep took four fields and that one was not among them.
- **WHY IT IS WORTH KEEPING.** It is **the one host-side datum suggesting 48000 is reachable at all** on this machine, where: the guest's AHI readback is 48000 (`nfreq=1`), AROS's `ac97` driver hardcodes 48000 and **never writes a codec rate register**, QEMU's AC97 defaults the codec to 48000 (`0xbb80`) and takes its rate from that unwritten register, and **no `-audiodev` backend accepts a `frequency`**. The effective 44100 is bounded to QEMU's audio back end with no launch-line fix.
- **AND EXPLICITLY NOT AS EVIDENCE AGAINST THAT BOUND.** It is not the guest: the format is `float32le` where every QEMU `pa` stream on this host has been `s16le`; other lanes were live at the time; and a later full listing showed **0** sink inputs, so it cannot be re-inspected. **A 48 kHz stream on the host says nothing about what the guest's AC97 path can be made to do.** Recording it without that caveat would have weakened a correctly-bounded conclusion.
- **WHAT WOULD SETTLE IT:** `pactl list sink-inputs` in full, capturing `application.name`, `application.process.binary`, `media.name` and `application.id` per stream. That names it, and only then is this promoted to evidence or dropped. Recorded in the file so the next attempt starts from the right instrument rather than a four-field grep.
- Lane unchanged: riqemu1 blocked on the `Software Failure!` requester, needing a human click or a guest reboot; the v11 Dell binary with the work counters still built and waiting (`1100264 B`, `r12moves=41`).

## [2026-10-04] ingest | the LFO path never runs — the cost is the voice render itself
- Disposition: **New** (1 article + 1 raw evidence file); **cascade correction** (1 article)
- Raw: `llm-wiki/raw/evidence/2026-10-04-lfo-path-never-runs.md` (both v11 build lines including the erroneous `-O2` one, the dirty-tree provenance note, the song actually loaded, the `put_begin failed` refusal, the counters summed over all blocks and for the busiest, the complete settled stage table with the child-tax arithmetic, the cross-lane table, and the evidence for both lane corrections)
- `AUDIT 0/0 PASS` at session start. The Dell was **up and connected with AHI free** — no RIAPP running — so the measurement went there rather than on riqemu1, which is where `lev-voice` is 91 % of LEVI against 81 %.
- **THE HYPOTHESIS WAS GOOD AND WRONG.** The LFO block runs `RI_LEVI_NLFO`(5) x `RI_LEVI_NVOICES`(8) = **40 inner iterations per sample** against 8 voice calls — a prima facie case for the cost. **`lfo_samples 0`, `lfo_iters 0`, `fx_samples 0`**, summed over all 28 logged blocks and zero in the busiest one too. **Not one LFO is trigged anywhere in this song's Levi part**, and `levi_fx_mod` is never reached. The suspicion is not smaller than expected; it is absent — which is why it needed a counter rather than more staring at the source.
- **SO THE COST IS THE VOICES.** `lev-voice 543 us = 91 % of LEVI, 94 % of it above the floor`, while `lev-arp`, `lev-seq` and `lev-tempo` all read 4 us and `lev-mix` 6 us: **every stage but the voice render sits on the lane's own floor.** **Peak polyphony is 4 voices of 8 slots** and the idle four early out as `levi_voice_render_stereo` intends, so **cost tracks sounding voices, not the polyphony setting.**
- **BOTH LANES AGREE ONCE THE TAX IS REMOVED:** corrected LEVI is **42.2 % of block on riqemu1 and 47.4 % on the Dell** — two songs, two clock domains, one resampled and one not — with `lev-voice` at 81 % and 91 % of LEVI.
- **THREE THINGS SETTLED ALONGSIDE.** An **explicit `SONG=` did not win**: *The Knife* loaded from `Vk4aros:` despite `SONG=RAM:zombie-nation.rbng`, contradicting the code's *"an explicit SONG= always wins"* — a finding about argument passing, cause not established. The **Dell has no song library and the guest Shell has no `mkdir`**, so `put` cannot stage one (`put_begin failed: cannot open temp file`); its demo path resolves through `Vk4aros:`. And **`RI_V11_OPT=-O0` is not optional** — a rebuild that omitted it came out `-O2` (880608 B, `r12moves=286`) where the baseline is 1100264 B and 41, and the lane record's reason applied exactly: *`-O2` changes CPU cost by ~24 % by size alone and would confound the metrics being measured.*
- **TWO LANE CORRECTIONS.** **`exec` does NOT die while RIAPP runs on the Dell** (`rc=0` with Process 8 live), so that defect is **riqemu1-specific**, not a general lane law as recorded. And **a `Software Failure!` requester clears on a VM restart with nobody at the keyboard** — riqemu1 was powerdown'd and relaunched and came back clean, refining *"needs a human click, or a guest reboot"* to *a VM restart is enough*, while `sendkey` still not dismissing one stands unchanged.
- Lint: 227 articles + 17 raw sources, 0 unindexed, 0 dead index rows, 678 links 0 broken.

## [2026-10-04] ingest | songs could not open — a static path, a flat join, and ASL's dot padding
- Disposition: **New** (1 article + 1 raw evidence file); **correction** (1 article, in place, original claim retained)
- Raw: `llm-wiki/raw/evidence/2026-10-04-songs-could-not-open.md` (the owner's report and its correction, both lanes' `dir` output, the three-step library descent, pre- and post-fix song lines, the 151-dot requester line, the AROS `filereqhooks.c` excerpt, the appending-log evidence that disproved my `SONG=` claim, `RI_RAW_SPACE 0x40` against the 57 I sent, three `ok` closes with a surviving process, the offline-mode heartbeat)
- `AUDIT 0/0 PASS` after every code change. The owner reported **"riapp fails to open songs"** and corrected my first guess: `Vk4aros:` is the **Dell's own stick**. That correction is the key — a static path can only ever serve one lane.
- **THREE STACKED DEFECTS, each hiding the next.** **(1) `RI_PATH_SONGS` WAS A LITERAL** (`SYS:Classes/ReIncarnation/Songs/`) — correct on riqemu1, absent on the Dell, so it fails silently rather than loudly. Now probed `<stick>/songs/local/`, `<stick>/songs/`, `SYS:Classes/.../Songs/`, whole sticky table per step, `SYS:` last, no requester on a miss; new `ri_pal_probe_sub` declared in the shared header with a host stub and a pinned return code in `t153_sticky_log`. **It is now the only non-static path in the enum — that asymmetry was the tell.** **(2) THE LIBRARY ROOT IS NOT THE SONG'S DIRECTORY** — the layout is `<root>/<song-dir>/<song>.rbng`, so `root + leaf` can never name it; the demo path tries the flat join then descends, and the wiki already recorded why the nesting must stay (`../demo/...` relative playlist entries). **(3) ASL PADS THE FILE NAME WITH DOTS** — **151 dots**, not an overrun: `fr_File` is the file gadget's text and ASL pads a partial name out to the pattern, so the file browser's path could never open on any platform.
- **THE PROOF IS TWO ADJACENT LINES:** `.../songs/local/zombie-nation.rbng: open failed` immediately followed by `.../songs/local/zombie-nation/zombie-nation.rbng: 151 bars at 140 BPM` — the demo song now loads itself, no argument, no requester.
- **TWO OF MY OWN ERRORS, CORRECTED ON THE RECORD.** *\"An explicit `SONG=` did not win\" WAS WRONG — IT DID:* `RIAPP.LOG` **APPENDS ACROSS RUNS** and I read the first matching line, which belonged to an earlier session; my run is line 23 and loaded. The code's precedence comment was right; **anchor on the last run.** The superseded claim is left in the LFO article with the correction above it, because the reasoning error is the part worth remembering. And *my spacebar injection used the wrong scancode* — `RI_RAW_SPACE` is `0x40` (**64**), I sent **57** — so no spacebar ever reached RIAPP and every "playback started after key injection" claim from that lane is unsupported; RIAPP auto-plays, which is why songs played anyway.
- **TWO LANE FACTS.** A crashed RIAPP **survives `--ui-close`, and Q (`0x10`, the QUIT shortcut) does not stop it either** — three closes returned `ok` and the process stayed, which is how a second RIAPP launched alongside the first and AHI contention plus an interleaved log followed: **check `status` for a leftover BEFORE launching.** And **offline-render-only is silent** — a run logged `RIAPP play` and repeated draw lines while emitting NO `stg:`/`dstg`/`vcount`/`hb:` at all, since every one sits behind `if (s_live && s_lv.drv.session)`: **a heartbeat with draw lines but no stage lines is offline mode, not an idle engine and not broken counters.** That is precisely what defeated the `vus` regression.
- Lint: 228 articles + 18 raw sources, 0 unindexed, 0 dead index rows, 0 broken links.

## [2026-10-04] ingest | the songs library lives at songs/local, and the tab-cycle "cost of -O2" was the overload guard
- Disposition: **New** (1 article + 1 raw evidence file)
- Raw: `llm-wiki/raw/evidence/2026-10-04-host-bench-and-the-overload-guard.md` (both lanes' library listings, the three probe shapes, the full `-O2` and `-O0` benches, the Dell paired regression, the four-run heartbeat table, the `prio` distributions, the 1.36 s draw stall, both build identities, the close/unset transcripts)
- `AUDIT 0/0 PASS` throughout. Four Dell runs plus a host bench, on an owner's instruction to change venue, build flags and order of cuts.
- **THE LAYOUT, NOW DOCUMENTED AT THE OWNER'S REQUEST.** Zombie Nation is at **`Vk4aros:ReIncarnation/songs/local/zombie-nation/zombie-nation.rbng`** — not the `songs/` root — and riqemu1's copy is **flat**, at `SYS:Classes/ReIncarnation/Songs/zombie-nation.rbng`, with no nesting. `Vk4aros:` is the **Dell's own stick**, which is why one static `RI_PATH_SONGS` could only ever serve one lane. `local/` and `demo/` must stay together (relative playlist entries include `../demo/...`).
- **THE TAB-CYCLE PUZZLE IS SOLVED, AND IT WAS NOT THE COMPILER.** The load guard trips **only at `-O0`**: a buffer overruns budget, the render task drops to `AU_LIVE_PRI_YIELD` (−1) — **below the GUI** — which handed the GUI free CPU and made repaints look cheap. At `-O2` nothing overruns, the guard never arms, and the render holds priority 21 throughout. **`prio` across four runs: `-O0` guard-on `{'21':117, '-1':7}`, `-O0` forced-off `{'21':7}`, `-O2` guard-on `{'21':18}`.** The last two never drop. **The `-O2` figure is the honest one; the 2.4× was `-O0`'s governor.** `-O2` across the board: **xruns 5420 → 0, `render_max` 20405 → 4095 µs, `wake_max` 5817 → 47 µs**, with the owner switching tabs by hand and reporting no issues.
- **THE HOST BENCH, because Levi voice cost is deterministic DSP and needs no AROS lane.** A host clock read is ~20 ns instead of 4–6 µs, so the counts become **exactly controlled** instead of sampled. **`-O2` 0.307 vs `-O0` 1.022 µs/voice-sample (3.33×)**, and the Dell measures **3.07×** the same way — **the `-O0`/`-O2` factor belongs to the code, not the machine**, with the Dell uniformly ~6× the host. Dell regression at `r = 0.9983`: **1.97 µs/voice-sample, 13 µs fixed** — on both machines the cost is **overwhelmingly per sounding voice and almost nothing is fixed**.
- **TWO PROPOSED CUTS REFUTED.** *Operator skip is ALREADY SHIPPED* (`if (i >= RI_LEVI_NOPS || !live[i])` in `voice_pass`; 8 ops and 1 op both cost ~20.8 µs — the flat line IS the skip). *The morph bank skip has no prize* (morph 0/50/100 all ~79.8 µs) and that belief came from reading source rather than measuring. **The bit-identical cut list is therefore EMPTY**; control-rate updates are the only meaningful cut left and are **not** bit-identical, so the ledger starts there.
- **A NEW ANOMALY, REAL AND UNEXPLAINED:** `part_max = 1364359 µs` — a **1.36 second GUI stall** with 2505 xruns, in the `-O0` forced-off run. Not a governor artefact. Now the largest single anomaly in the Dell log.
- **THREE CORRECTIONS TO MY OWN CLAIMS.** Closing RIAPP **works** — "survives `--ui-close`" was a **window-indexing error** (with requesters up, `#0` is not the live panel; target the panel window). **Do not ask for reboots** — the Dell was closed, not rebooted, and came back clean. And a song-load miss can wedge startup because `EasyRequestArgs` is modal; the probe is now quiet with every shape logged for atcpbin.
- Guest left clean: `Unsetenv RIAPP_AUDIO_NOGOVERNOR` (`Getenv` then returns `object not found`), `close '#0'` ok, 0 RIAPP processes.
- **ADDENDUM — the 1.36 s stall is now ATTRIBUTED, not merely "real and unexplained".** The draw line's `box_*` fields are `avg/max/n` per caller, and they name it: `box_bar=74648/1364359/25` with `box_steps`, `box_other` and `box_none` all `0/0/0`. `box_bar` is `RI_RSEC_BOX_BAR`, the **Song Position display** (`gui/panelui.h:109`), set from `app/riapp.c` on the `RI_STALE_BAR` tick path. And **`build_max ≈ part_max`** in both expensive rows (490523/490609, 132813/1364359), so the time is the **display-list build, not the blit**. **It is a burst, not a steady cost**: the same field reads `box_bar=202/204/8 build_max=116` in the quiet windows — 202 µs normally against 74.6 ms on average across 25 refreshes in one 30 s window. This lands on the **existing `mut_fixM9` / `t93_raster_goldens` transport display-list track** rather than opening a new one, and *why it bursts* is the open part.

## [2026-10-04] ingest | the mixed build: engine at -O2, app and GUI at -O0
- Disposition: **New capability** (build script), **partial result** (host verified, target lane blocked)
- `AUDIT 0/0 PASS` before the change. No ReIncarnation lane available at the time of writing — see below.
- **THE MIXED BUILD EXISTS NOW.** `build_v11.sh` grew `RI_V11_MIXED=1`, with `RI_V11_MIX_OPT` (default `-O2`) for `engine/**` and `RI_V11_OPT` (`-O0`) for everything else. **The split is by DIRECTORY, not by hand-picked files**, so a new `engine/` file is optimised without anyone remembering to move it — the failure mode of a per-file list is a new file silently landing unoptimised. It reports what it optimised:
  ```
  AROS RIAPP v11 BUILD OK (/tmp/opencode/RIAPP-mix.v11, 1008384 bytes, build=88e2c86, r12moves=41)
    mixed: engine/ at -O2, rest at -O0 (29 TUs optimised)
  ```
  1008384 B sits between the two single-flag builds (884176 at `-O2`, 1104032 at `-O0`), and `r12moves=41` satisfies the v1 ABI gate.
- **HOST-SIDE VERIFICATION: MIXED FLAGS ARE SAFE.** 33 TUs compiled at `-O2`, 55 at `-O0`, and the suite is **8 pass / 0 fail** — `t136_levi_stereo`, `t156_render_stages`, `t153_sticky_log`, `t152_repaint_reason`, `t154_reason_and_build_split`, `t151_wake_latency`, `t158_audio_failure_loud`, `t86_pal_fslog`. This matters because a mixed build is exactly the shape that exposes optimisation-dependent behaviour: `-O2` changes aliasing assumptions and inlining across a translation-unit boundary while the rest stays unoptimised. **It does not, here.** (A first attempt showed 8 failures that were the harness missing `pcf.o`, not the flags — worth recording because "the mixed build broke 8 tests" would have been the wrong conclusion and an easy one to reach.)
- **THE DELL WENT OFF THE NETWORK DURING THE TARGET RUN, AND THAT IS NOT ATTRIBUTED.** `RIPP-MIX` was launched and the very next log read timed out, after which the machine stopped answering ARP: `3 packets transmitted, 0 received, +2 errors, 100 % packet loss` and `connect: No route to host` on 9292, with the route intact (`dev enp12s0 src 192.168.1.81`) and the host IP unchanged. **The timing is suggestive and the evidence is not:** a mixed build that passes every host test is not obviously a crash, and an unattended machine that was already showing a **1.36 s GUI stall with 2505 xruns** is equally capable of wedging itself. **This is explicitly NOT recorded as "the mixed build crashed the Dell."** It needs a powered-eye check on the machine, and until then the target half of this experiment is simply unmeasured.

## [2026-10-04] ingest | the mixed build MEASURED on the Dell — 93 % of the -O2 win, 0 xruns
- Disposition: **New result** (completes the experiment opened in the previous entry)
- Raw figures below; artefact is the same `Vk4aros:RIAPP.LOG` column the prior entries quote.
- **THE DELL WEDGE IS *IDLE-TIME*, AND THAT IS A DIFFERENT BUG FROM ANYTHING MEASURED SO FAR.** Owner: the mixed build "played fine, and we switched tabs no issues, then left it unattended for minutes and found it wedged" — no requester. So the signature is **play fine → idle → wedge**, not load-driven. Ruled out on inspection: the stopped path in `engine/live.c` is a memset loop and cannot wedge, and the GUI's 100 ms timer **is** re-armed every loop iteration (`app/riapp.c`, `CheckIO`/`WaitIO`/`SendIO` in the message block). The remaining suspect is the **AHI render task's wait loop**, which is `Wait(s_hook_mask | SIGBREAKF_CTRL_C)` with **no fallback timer** — it wakes only on an AHI interrupt. If the card stops requesting buffers when the song ends, nothing pokes the task, and an interrupt-driven loop with no fallback has nothing to break it out. **Stated as a hypothesis from code inspection, not measured.**
- **⚠ SUPERSEDED MINUTES LATER, AND THE OWNER'S OWN WIKI SAYS SO.** Consulting the wiki after writing the above turned up a **root-caused** wedge dated **2026-09-25**, and the Dell runs the exact vulnerable driver:

  ```
  [exec] 'version e1000.device' -> rc=0 (62 ms)
         e1000.device 1.1
  ```

  Proven cause (2026-09-25, reproduced on a private QEMU e1000 lane): **`e1000.device` allocates a Tx frame with `AllocMem()` per outgoing packet in its Tx soft interrupt and frees it with `FreeMem()` from the HARDWARE interrupt handler.** exec's memory lists are guarded by `MEM_LOCK` = `Forbid()`, which **does not stop interrupts**, so the handler's `FreeMem()` races task allocations and corrupts the TLSF lists — a `Software Failure!` with `Error: 0x80000008 - Privilege violation` in `tlsf_freevec`, reached from `e1000func_clean_tx_irq`. The article names the Dell's own deployed `e1000.device 1.1 (21.7.2026)` as carrying the pattern **by disassembly**, and records that the upstream e1000 rebuild **did not help because it is the same code**. A patch exists: `e1000-tx-no-alloc-in-interrupt.v{1,11}.patch`.
- **THIS EXPLAINS MY SYMPTOMS, INCLUDING THE PART I MISREAD.** The Dell "played fine, tabs fine, then wedged while unattended" — and the failure I actually observed was **not an RIAPP stall**: the machine **stopped answering ARP** (`No route to host` on 9292, with the route intact and the host IP unchanged). That is a dead NIC, not a wedged application. The reproduction's trigger is **repeated large bulk transfers** ("845 KB bulk get per round"), and **my own evidence pulls are exactly that: 535 127 B and 412 336 B single `--get`s.**
- **CORRECTION TO MY OWN RECOMMENDATION.** I proposed adding a fallback `TIMEOUT` to the AHI wait loop as the next change. **That would have been fixing the wrong system.** No amount of poking the render task revives a guest whose exec heap has been corrupted by a NIC driver; RIAPP was a bystander. The fix belongs on the transport, and the transport fix is already written and waiting to be deployed.
- **THE LESSON, WHICH IS THE PART WORTH KEEPING.** An unexplained machine-level wedge on a lane whose wiki **already has a root cause** is a **lookup failure, not a new investigation** — and the index had carried this since September. Two other Dell-lane traps from 2026-10-02 were likewise already documented (a bare-path `--exec` launch wedges the agent; `--ui-capture` can hang forever), so the lane now has **three** recorded ways to lose it and I hit a fourth. **Consult the wiki before proposing a fix for any lane-level failure, and check whether the evidence-pull size is itself the trigger.**
- **THE MIXED BUILD, MEASURED.** `RIAPP-mix.v11`, 1008384 B, `engine/` 29 TUs at `-O2` and the rest at `-O0`:

  | build | µs/voice-sample | xruns | render_max | overloads | wake_max |
  |---|---|---|---|---|---|
  | `-O0` | 6.04 | 5420 | 20405 µs | 37 | 5817 µs |
  | **`-O0` DSP + `-O0` GUI (mixed)** | **2.27** | **0** | **4099 µs** | **0** | **38 µs** |
  | `-O2` | 1.97 | 0 | 4095 µs | 0 | 47 µs |

  **The mixed build captures 93 % of the `-O2` win** and takes **2.66×** off the `-O0` DSP cost, while the app and GUI stay unoptimised for debuggability. Fits are `r = 0.9992` (mixed) and `r = 0.9983` (`-O2`).
- **WHY THE MIXED BUILD MATCHES `-O2` AND NOT `-O0`, WHICH IS THE WHOLE POINT.** The DSP translation units are compiled `-O2` in both, so their cost is the same by construction; what `-O2` also changes is the **scheduling** — at `-O0` a buffer overruns budget, the overload guard trips and drops the render task below the GUI, and xruns follow. In the mixed build the render is fast enough that **the guard never arms** (`overloads = 0`, `load` peaking at 673/1000), so the mixed build inherits `-O2`'s *runtime* behaviour as well as its *code* speed. **The 2026-09-28 debug-only rule is therefore not actually in tension with the `-O2` measurements: the scheduling win comes from the DSP being fast, not from the GUI being optimised.**
- CAVEAT: 4 slope points on the mixed run and 3 on the `-O2` run, so the 1.15× gap between mixed and pure `-O2` is within the noise of these sample counts and should not be read as a real regression. The `fixed` intercepts are meaningless where noted (negative, from extrapolating a 4-point fit below its lowest point).
- **THE BULK-GET PROTOCOL HAS NO RANGED READ, SO "JUST PULL THE TAIL" IS NOT AVAILABLE.**
  `bulk_get_begin` carries only `{path, port}` and the agent streams the whole file,
  SHA-256 verifying as it goes; the transport is already windowed lock-step with an ACK
  per window, and its own docstring records that this is *the only outbound mode the
  AROSTCP stack survives* (the 2026-08-14 unfettered-stream wedge). **So the transport is
  already doing the safest thing it can, and a helper that trims the file after the fact
  reduces the analysis artefact but NOT the wedge risk — the bytes crossed before the trim.**
  Recorded because I first wrote such a helper and briefly described it as a mitigation.
- **THE REAL LEVER IS LOG SIZE, AND THAT IS A DEFECT OF OURS.** `RIAPP.LOG` is append-only
  across every run on the stick and had reached **575 791 B** on a machine whose NIC driver
  wedges under repeated bulk transfer. A log that grows without bound is itself a bug, it
  makes every evidence pull larger than it needs to be, and it is the one thing here that
  is ours to fix rather than the driver's. **Bounded or rotated per session is the correct
  answer**, and it is now the top transport-hygiene item.

## [2026-10-04] ingest | RIAPP.LOG is now bounded by ROTATION, and getting the size right took three tries
- Disposition: **New capability** (`app/riapp.c`), **New trap** (three silent ways to ask a file's size on this stick)
- `AUDIT 0/0 PASS` after each step. Verified on the Dell against the real 662 KB log, not simulated.
- **WHY.** `RIAPP.LOG` is append-only across every run on the stick and had reached **662 006 B**. Two costs: every evidence pull must move the whole file (the bulk protocol has no ranged read), and the Dell's `e1000.device 1.1` is the driver whose Tx-interrupt/`FreeMem` race wedges the guest under exactly that kind of repeated bulk transfer (2026-09-25, proven). **An unbounded log is a standing contribution to losing the lane.**
- **ROTATE, DO NOT TRUNCATE.** The append-only behaviour is deliberate and load-bearing: lane records read several runs out of one file, and instances are told apart by a changed log line. So at startup the previous session is **renamed to `RIAPP.LOG.1`** and the current one starts clean. Two generations, bounded at ~512 KB worst case, with the run-before-this one preserved — which is what cross-run analysis actually needs. **A log is never worth failing over: if the rename fails, it appends and says so.**
- **⚠ THE TRAP, AND IT IS THE PART WORTH REMEMBERING: THREE OBVIOUS WAYS TO ASK A FILE ITS SIZE ARE ALL SILENTLY WRONG ON THIS STICK.**
  1. `Seek(f, 0, OFFSET_END)` — **returns 0** for a 600 KB file. A cap built on it never fires and looks exactly like a cap that works.
  2. `Lock()` straight onto the **file** — **fails** while the log is being appended. The roll reported `size UNKNOWN -> keeping`.
  3. The 4-arg `Examine(dir, fib, leaf, len)` that would settle it — **not in the v1 SDK** (only the 2-arg form is). And the 64-bit build's `FileInfoBlock32` has **`fib_Size`, not `fib_FileSize`**.
  **What works: read the file once and count the bytes.** No dependence on `Seek`, `Lock` or `Examine` semantics at all — which is the entire point, because each of those three produces a *plausible wrong answer* rather than an error. Bounded at 4 MB, above which the decision is already made. Once per process, so the steady-state cost is one flag check.
- **THE SECOND-ORDER LESSON, WHICH COST THREE BUILD/DEPLOY CYCLES.** The first version shipped, passed the audit, built clean, and **did nothing** — and the only reason I found out is that I later added a line logging the roll *decision*. **A silent optimisation that quietly never fires is indistinguishable from one that works.** Hence the permanent one-shot line every session now emits:
  ```
  RIAPP log: session start, 662006 B of 262144 -> ROLLING to RIAPP.LOG.1
  ```
  If that line ever reads `keeping` on a 600 KB log again, the cap is broken and says so in the artefact rather than in my head.
- **RESULT: `RIAPP.LOG` went 662 006 B -> 2 409 B on the next launch, a 275x reduction in pull size**, with `RIAPP.LOG.1` verified byte-identical at 662 006 B holding all 16 prior sessions, and the new session logging the roll, loading the song (`151 bars`) and playing.

## [2026-10-04] ingest | the owner rejected two runs on AUDIT ALONE — and the cause was my build flag, not my code
- Disposition: **Correction** (process), **New evidence** (the shipping candidate, measured)
- `AUDIT 0/0 PASS` on the code in question. The counters were clean too. **Both were wrong, and listening was the only instrument that noticed.**
- **THE FEEDBACK, verbatim in substance: the last two runs' audio was not smooth, and "whatever you did, fails based on that alone."** The log-roll change was the code under test, so that is where the finger pointed — and it was innocent.
- **THE ACTUAL CAUSE: EVERY LOG-ROLL BUILD I DEPLOYED WAS `-O0`.** Sizes say it plainly:
  ```
  1106072  RIAPP-logroll.v11
  1107224  RIAPP-logroll2.v11
  1107576  RIAPP-logroll3.v11
  1107000  RIAPP-logroll4.v11
  1011384  RIAPP-ship.v11        <- mixed: engine/ -O2, rest -O0
  ```
  Four `-O0` builds at ~1.107 MB, then one mixed build at 1.011 MB. **I verified a feature on the one configuration whose audio the owner had already rejected**, carrying the old "shipping is `-O0`" habit into the exact runs whose purpose was to be heard. The two measurements that matter here agree with the ear rather than against it: **`-O0` costs ~3x on the DSP (6.04 vs 1.97 µs/voice-sample), and the mixed build measures 0 xruns, `render_max` 4091 µs, `wake_max` 38 µs.**
- **WHY THE TELEMETRY COULD NOT CATCH IT, WHICH IS THE PART TO KEEP.** Every `-O0` log-roll run reported **`xruns=0`, `render_max` ~3525 µs, `wake_max` ~36 µs, `overloads=0`** — indistinguishable from the mixed build's numbers, and comfortably "green". **A droppy, audible artefact produced a clean telemetry report.** The DSP is not overrun, so the audio path never complains; audible smoothness is simply not one of the things these counters measure. **"All counters green" is not "it sounds right", and on this lane the two have now come apart.**
- **THE RULE THIS ESTABLISHES, WHICH SUPERSEDES THE OLD ONE.** Verification runs must be **the configuration that ships**. A feature verified on `-O0` is verified on a configuration nobody will listen to, which is not verification. Concretely: **the shipping candidate is now the MIXED build** (engine/ `-O2`, app+GUI `-O0`), it is what gets deployed by default, and any future lane run is built with `RI_V11_MIXED=1`. The log-roll change is unchanged and now rides on that build.
- **THE ROLL MECHANISM IS CONFIRMED ON THE SHIPPING CANDIDATE**, across two consecutive sessions rather than one, which is the case that matters for a size check:
  ```
  RIAPP log: session start, 662006 B of 262144 -> ROLLING to RIAPP.LOG.1
  RIAPP log: session start,  36010 B of 262144 -> keeping
  ```
  Roll once when over the cap, keep when under, and a second instance sees the first one's output rather than a stale size.

## [2026-10-04] ingest | APPROVED on audio — the mixed build is the shipping configuration, and the default now says so
- Disposition: **Decision recorded** (owner), **Process fix** (build script default), **Self-correction** (the fix's first version was itself wrong)
- `AUDIT 0/0 PASS`. Bare `build_v11.sh` with **no environment variables at all** now reproduces the approved binary: **1011384 bytes, 42 r12moves, 29 TUs optimised** — identical to the `RIAPP-ship.v11` the owner listened to and approved.
- **THE APPROVAL MATTERS MORE THAN THE NUMBER, BECAUSE IT IS THE ONLY INSTRUMENT THAT COULD HAVE GIVEN IT.** Every counter on the rejected `-O0` runs was green (`xruns=0`, `overloads=0`, `render_max` ~3525 µs, `wake_max` ~36 µs) while the audio was audibly wrong. The approved session reported `xruns=0`, `render_max` 1969→1970 µs, `wake_max=36 µs`, `prio=21`, `load` 323→336/1000. **None of those figures distinguishes the approved build from the rejected ones.** The ear did. So the accepted evidence for "this build sounds right" is a human verdict, and the honest consequence is that the counters cannot be cited as corroborating it.
- **THE DEFAULT IS NOW THE APPROVED SHAPE, NOT A THING YOU REMEMBER TO ASK FOR.** `RI_V11_MIXED=1` is the default; `RI_V11_MIXED=0` opts out to a uniform build. The failure this fixes was four consecutive `-O0` verification builds sent out by habit, two of which the owner rejected. **A build script whose default is not the approved configuration is a footgun with a track record.**
- **⚠ AND THE FIX'S OWN FIRST VERSION WAS WRONG, IN PRECISELY THE WAY IT WAS MEANT TO PREVENT.** The first attempt expressed "mixed" as string substitution of the optimisation level inside one shared flag string, and with `RI_V11_OPT` unset the substitution matched nothing: the default build came out **885 920 bytes, 301 r12moves, "81 TUs optimised"** — a **uniform `-O2`** that the script cheerfully described as mixed. Caught only because I checked the size and r12moves instead of trusting the line the script printed about itself. Rewritten as **two explicit flag sets** (`CF_DSP`, `CF_APP`) with the app/GUI level defaulting to `-O0` and NOT to `${RI_V11_OPT:--O2}`, since inheriting that default is precisely what broke it.
  **The lesson is the transferable part: a mechanism that reports its own success is not evidence that it succeeded — check the artefact.** A build script that says "mixed: engine/ at -O2, rest at -O0" is exactly as trustworthy as a telemetry report saying `xruns=0`, which is to say: not, on its own. Both of those green signals misled me within the same day, in the same lane.

- **WHERE THE BUILD-SCRIPT CHANGE LIVES, SO IT IS NOT LOST.** `build_v11.sh` is **outside this repository** (`/home/miller/Work/vms/ri-p9/`), so the mixed-default change is **not in git** and cannot be reviewed or reverted from here. That is a real gap in the record, so the change is stated here in full:
  - `MIXED=1` by default; `RI_V11_MIXED=0` selects a uniform build.
  - `RI_V11_MIX_OPT` (default `-O2`) sets the engine/ level.
  - `RI_V11_APP_OPT` (default `-O0`) sets the app + GUI level — deliberately **not** `${RI_V11_OPT:--O2}`, because inheriting that default is what made the first version emit a uniform optimised build.
  - Two explicit flag sets, `CF_DSP` and `CF_APP`, selected per source by the `engine/*` case; no string substitution on a shared string.
  - Verified: bare `build_v11.sh`, no env vars → `1011384 bytes, r12moves=42, 29 TUs optimised`, matching the approved `RIAPP-ship.v11`.
  **If that script is ever lost or reset, this entry is the specification to restore it from** — and given it governs what actually reaches the owner, it arguably belongs in the repository next to `scripts/ri_build_aros.sh` rather than outside it. Raised, not done: moving lane infrastructure is a structural change and the call is the owner's.

## [2026-10-04] lint | 8 issues found, 8 auto-fixed
- **Layout note, because it changed how the lint was run.** The karpathy-llm-wiki skill's canonical shape is `raw/` plus `wiki/` at the project root; this repo has used `llm-wiki/raw/{articles,evidence}/` with `index.md` and `log.md` beside them for 229 articles. **Restructuring a wiki this size to fit a template would be destructive, so the skill's lint RULES were applied to the existing layout rather than its directory names**, and `scripts/check_evidence.py` could not be run unmodified (it requires `wiki/`). Recorded so the next session does not "fix" the layout.
- **8 broken index links, all auto-fixed.** Every one was an index row whose target had **no `raw/articles/` prefix**, so it resolved to `llm-wiki/<slug>.md`, which does not exist. All six distinct slugs had **exactly one** match in `raw/`, which is the condition under which the skill permits an automatic path repair rather than a report. These are the 2026-10-03 `-O2`/governor/`mut_fixM9` rows written by another session on this repo.
- **The failure mode is worth naming: an index row can look perfectly formed and still point nowhere.** Every one of these had correct markdown, a real slug and a real article — only the directory was missing. **A link that renders as text is not a link that resolves**, and only resolving all of them found it.
- **Reported, not fixed (11 files):** 11 articles are absent from `log.md` by filename (2026-09-27 through 2026-10-03, including `riqemu1-cannot-be-driven-by-injection` and the AHI-rate records). They are indexed and their content is intact; they were logged by another session under a different convention. **Not force-corrected**: inventing log lines for work this session did not do and cannot verify would corrupt the operation history, which is the one file in the wiki whose value is that it is true.
- Post-lint state: **229 articles + 17 evidence files, 0 unindexed, 0 dead index rows, 635 links, 0 broken.**

## [2026-10-04] ingest | mut_fixM9 closed: 4/4 song-name guards killed by a display-list assertion
- Disposition: **Update** (closes a recorded survivor set); **New** (one real defect found by the new test)
- `AUDIT 0/0 PASS`. `t93_raster_goldens` gains `songname_checks`, a **display-list** assertion rather than a raster one — which is exactly what the earlier record said was missing ("Closing them properly needs a display-list assertion on the transport section, which t93's raster goldens are the right place for and which this change did not add").
- **THE THREE SURVIVORS, NOW 4/4 KILLED** (measured by the test's EXIT STATUS, after getting that measurement wrong twice — see below):
  ```
    drop the whole song-name block                    -> KILLED
    drop the fitted-name guard                        -> KILLED
    move the name right (PX(25) -> PX(900))           -> KILLED
    move the name up (PX(190) -> PX(197))             -> KILLED   <- the documented regression
  ```
- **WHY A DISPLAY LIST AND NOT A PIXEL HASH.** The guards are "draw only when a name is set", "draw only when the fit produced something" and "left-aligned". A pixel hash cannot attribute *which* of those broke: an absent TEXT command and a misplaced one both change the hash, uninformatively. Asserting on `RI_D_TEXT` commands pins the actual property — count, content, `x0`, `y0`.
- **⚠ A REAL DEFECT THE NEW TEST FOUND, NOT YET FIXED: A SONG NAME OVER 39 CHARACTERS IS SILENTLY HARD-CUT, WITH NO ELLIPSIS.** `art_tr.c` passes `char name[40]` to `ri_art_tr_fit`, and `ri_art_tr_copy` fills at most `cap-1` = **39** characters. Measured from this call site the ellipsis branch is therefore **UNREACHABLE AT EVERY ZOOM**:
  ```
    label cap          : 39 chars
    host advance       : 6 px  ->  233 px wide at most
    room = PX(1560)    : z0 390 px | z1 585 | z2 780 | compact 292   <- smallest is 292
    233 < 292 at every zoom, so the fit never shortens.
  ```
  So a 59-character name is emitted as 39 characters with no ellipsis — **"the name looks complete while being cut off", which is the exact failure the ellipsis was added to prevent.** The ellipsis logic is not wrong; it is unreachable because its buffer is too small to need it. Today's behaviour is pinned (so changing `art_tr` becomes a visible test failure rather than a silent golden change) and an assertion computes the room from `ri_geo_px` so that **if the buffer ever grows, this test fails and says why**. **Not fixed here: it changes the transport golden, so it is the owner's call.**
- **TWO WAYS I MEASURED THE MUTATIONS WRONG BEFORE GETTING IT RIGHT, BOTH WORTH RECORDING.**
  1. **The stale-object trap, again.** `ri_build_host.sh test` links the prebuilt `.o` files, so mutating `gui/draw/art_tr.c` and re-running the test recompiles nothing. The first mutation run reported **0 failures for all three mutants** — for a mutant set that was in fact partly dead. `art_tr.o`'s mtime is the tell.
  2. **A segfault is a kill, and grepping for `^FAIL` misses it.** After rebuilding correctly, two mutants crashed the test binary. My counter was `grep -c '^FAIL'`, which returns **0 for a crash** — so two killed mutants were reported as survivors. **Measure a test's verdict by its exit status, never by pattern-matching its output.**
- **AND ONE MUTANT SURVIVED A FIRST, WEAKER ASSERTION — WHICH IS THE POINT OF THE EXERCISE.** "Leftmost on its row" is true of a label moved to `PX(900)`, because that row is otherwise empty, so the mutant lived. The assertion now pins the **exact anchor the code documents** — `x == ri_geo_px(25, z)` and `y == ri_geo_px(190, z)`, with `PX(197)` being the documented regression. **A guard can be present, pass, and still describe the property too weakly to catch the bug it was written for.**

## [2026-10-04] ingest | CORRECTION: my room arithmetic was wrong; the fix stands, the explanation did not
- Disposition: **Disputed→Resolved** (corrects the previous entry's stated evidence), **New** (the defect is fixed)
- `AUDIT 0/0 PASS`. `t93` **6/6 mutants KILLED**, measured by exit status from a baseline of the FIXED file:
  ```
    drop the song-name block                          -> KILLED
    drop the fitted-name guard                        -> KILLED
    move the name right (PX(25)->PX(900))             -> KILLED
    move the name up (PX(190)->PX(197), documented)   -> KILLED
    revert the buffer to 40 (re-open the defect)      -> KILLED
    shrink TR_SONG_MAX below a real song name         -> KILLED
  ```
- **⚠ CORRECTION TO THE PREVIOUS ENTRY. THE ROOM FIGURES WERE WRONG.** That entry derived `PX(1560)` as **390 / 585 / 780 / 292 px** and concluded "39 chars = 233 px cannot fit the smallest room, so the ellipsis is unreachable". The rooms are actually **780 / 1170 / 1560 / 585 px**. The error came from reading `RI_GEO_BASE_SCALE_NUM/DEN` as 1/1 when it is **2/1**, which halves every derived room. A probe against the real build settled it:
  ```
    z=0 room= 780 | z=1 room=1170 | z=2 room=1560 | z=3(compact) room= 585
  ```
  **The conclusion survives, for a stronger reason than I gave.** The longest possible name is `(RI_ART_TR_SONG_MAX-1) = 63` chars = **377 px**, and the narrowest row is **585 px**. `377 < 585`, so **`ri_art_tr_fit` never shortens at any zoom at any buffer size** — the truncation is dead code from this call site, permanently, not because of a small buffer.
- **SO THE FIX IS SMALLER THAN I DESCRIBED AND MORE NECESSARY THAN IT SOUNDED.** The buffer change was still correct and was still fixing a real defect: `char name[40]` cut a 59-character name to **39 characters with no ellipsis**, and that cut was observable. What it did **not** do is make the ellipsis reachable. **The ellipsis is left in place** — the function is shared and reachable with other metrics and rooms, and deleting it would remove protection from callers that need it. The test now asserts the truth about *this* caller: the longest possible name fits the narrowest row, so a whole name is emitted at every zoom, and shrinking the buffer kills the test.
- **AND A MUTANT SURVIVED ON THE FIRST ATTEMPT BECAUSE OF THE BAD ARITHMETIC, WHICH IS THE PART TO KEEP.** The test's "shortened name must carry an ellipsis" branch **never executed** — the room was wrongly believed to be 292 px, so it looked reachable when it was not. `drop the ellipsis -> 'z'` therefore SURVIVED. The room is now computed with `ri_geo_px` and the reachability is asserted as a precondition, so **a branch that cannot run cannot hide a mutant**: the test now proves the whole-name case always holds before it would accept a shortened one.
- **TWO MORE PROCESS NOTES, BOTH FROM GETTING THIS WRONG FIRST.** (1) My mutation harness used `git show HEAD:` as its baseline while the fix was still uncommitted, so it **clobbered the working-tree fix** and one mutant reported NOT APPLIED against a stale file. Baselines must be the file under test, saved explicitly. (2) `TR_SONG_MAX` was private to `art_tr.c`, so the test could not name the capacity it was reasoning about; it is now `RI_ART_TR_SONG_MAX` in `gui/draw/art.h`, and `art_tr.c` derives from it. **A constant mirrored in a test is a constant that can drift silently.**

## [2026-10-04] ingest | instrumented the Song Position path: the burst is REPEATED IDENTICAL invalidations
- Disposition: **New** (diagnostic), addresses the open item from the 1.36 s stall entry
- `AUDIT 0/0 PASS`. Mixed build (engine/ `-O2`, app+GUI `-O0`), 1012048 B, `r12moves=42`, deployed as `RIPP-BOXREP` on the Dell after closing the previous instance.
- **THE MEASUREMENT ADDED.** `RSectionDiag` gains `dpr_rep` / `dpr_new` / `dpr_run_max` / `dpr_run_now` plus the previous invalidation's `(why, x0, y0, x1, y1)`, reported as `boxrep=repeats/new/longest-run` on the `RIAPP draw` heartbeat line. **Counted at the invalidation site in `ri_rsection_refresh_box_why`, not in the draw path** — deliberately, because a repeat is often the invalidation that never becomes a draw at all, so counting in `draw_damage` would miss exactly the repeats in question.
- **FIRST DELL READING, AND IT ANSWERS THE QUESTION FOR THE NORMAL PATH:**
  ```
  box_bar=206/212/3   build_max=124   boxrep=3/0/130
  ```
  **130 consecutive IDENTICAL Song Position invalidations, coalesced by MUI into 3 actual draws** at ~206 us each. So on this build the repeated invalidation is already being collapsed by the existing coalescing, and **a cached display list would buy almost nothing on the normal path** — the invalidation rate is far higher than the draw rate, and MUI is already absorbing it.
- **WHAT THAT LEAVES, STATED HONESTLY.** The 1.36 s stall was `box_bar` n=25 at 74.6 ms average, i.e. **25 real draws**, not 25 invalidations. **So the coalescing that makes the normal path cheap is exactly what failed during the stall**, and the stall has not been reproduced on this build. The instrumentation now records, for any future occurrence, whether those 25 draws were repeats (`dpr_rep`) or genuinely different work (`dpr_new`) — which is the measurement that decides between "cache the display list" and "do not bother". **No verdict claimed yet.**
- **ONE DESIGN NOTE WORTH KEEPING.** `dpr_run_now` is deliberately **NOT** reset per heartbeat window, while `dpr_rep`/`dpr_new` are. Zeroing it would make every window report a run of 1 and **hide a burst that spans windows** — which is precisely what a 1.36 s stall does. `dpr_run_max` therefore keeps a session high-water mark, so the longest single run survives being read. A per-window counter would have reported "1" here and been useless.

## [2026-10-04] ingest | CONSULTANT: the cache is refuted, `build_max ~= part_max` was a misread, and there is a live correctness bug
- Disposition: **Disputed** (my stall conclusion is wrong), **New** (a user-visible bug), **Correction** (two mistakes in my own instrumentation)
- Asked the third-party consultant how to proceed. It read the code, built the host harness, measured, and **refuted my plan** — on evidence from my own first `boxrep` reading. Three findings, all independently confirmed by me afterwards.
- **1. THE DISPLAY-LIST CACHE IS DEAD, AND `boxrep` ALREADY PROVED IT.** `boxrep=3/0/130` means `dpr_new=0`: every invalidation in that run repeated the previous one's `(why, x0..y1)`. But `RI_STALE_BAR` originates only from `RI_PANEL_CH_FOLLOW`, set when `ri_str_follow()` returns non-zero — i.e. **only when the song bar actually advanced**. So every one of those repeats carried a *different bar number*: identical rect, different pixels. A cache can only skip work whose **result** is unchanged, and here the result changes on every call by construction. Host-measured at `-O0` (the stall's flag): a clipped build is **24 µs**, a full build 43 µs, against the Dell's own quiet `build_max=116` — the host model reproduces the Dell. 25 draws × 24 µs = **0.6 ms of a 1.87 s window.** **Do not build the cache.**
- **2. `build_max ~= part_max` IS A MISREAD, AND IT REDIRECTS THE SEARCH.** It holds in one of the two motivating rows and **fails by 10x in the other**:
  ```
  part_max=490609  build_max=490523   <- 99.8 % build
  part_max=1364359 build_max=132813   <-  9.7 % build
  ```
  `dp_build_max` resets per window, so 132 813 µs bounds *any single build* in that window — including the 1.36 s draw. **So ≥1.23 s (90 %) of the worst draw was in `replay_dl_dmg` + `BltBitMapRastPort`, not the build.** Two different failures had been averaged into one story, and the 1.36 s one — the one with 2505 xruns behind it — is not a build problem at all. **THE LESSON, IN THE CONSULTANT'S OWN FORM AND NOW THE HEADLINE OF THIS ENTRY: check a conclusion against the tree before building on it.** My conclusion rested on a coincidence that held in one row and not the row that mattered.
- **THE CHEAPEST DISCRIMINATOR, WHICH I RAN IMMEDIATELY: RE-READ THE HEARTBEAT I HAD ALREADY CAPTURED. FREE, AND IT POINTS AT PREEMPTION.**
  ```
  xruns=0     render_max=3530 wake_max=34   load=611 overloads=0
  xruns=0     render_max=3537 wake_max=38   load=575 overloads=0
  xruns=908   render_max=8340 wake_max=5822 load=953 overloads=6
  xruns=2505  render_max=8446 wake_max=5822 load=609 overloads=17
  ```
  `wake_max` jumps **34 µs → 5822 µs** and xruns go 0 → 2505 **in the same window as the stall.** Identical code doing 24 µs of work that took 1.36 s is a 55 000x ratio; only the *absence of the CPU* produces that. This is the **third** instance of the signature this wiki already named twice ("`-O2` tab cost is preemption of prebuilt MUI code"; at `-O0` the governor drops the render task below the UI). **The BAR path is a witness to the stall, not its cause** — and it fires ~0.83 Hz on its own, so **tab switching is the wrong stimulus** and `-O0` is the wrong arm.
- **3. A REAL, USER-VISIBLE CORRECTNESS BUG, WHICH I FIXED (`e8b766b`).** All four `ri_geo_bbox` call sites passed a hardcoded zoom of **0** while the canvases are not at 0 — the transport is unconditionally `RI_GEO_ZOOM_COMPACT`. A bbox in the wrong coordinate space is not rejected, it is clamped into something that looks valid. Verified on the host, not reasoned about:
  ```
  asked (zoom 0)      : 540,52..622,92
  real  (compact = 3) : 404,38..467,70
  DISJOINT
  ```
  **So the path whose entire job is to make the Song Position display follow the song was repainting a region that does not contain it.** All four sites now pass the canvas's own zoom, and `t166_bbox_zoom` pins it as pure geometry — no framebuffer, no AROS. **Filed separately from the stall, as it deserves: this one changes what the user sees.**
- **TWO CORRECTIONS TO MY OWN NEW INSTRUMENT, ON THE CONSULTANT'S READING.** (a) `dpr_run_now` **is** reset with its siblings now: my defence of leaving it running ("a burst spanning windows would be hidden") was already satisfied by `dpr_run_max`, which is never reset — and zeroing the siblings meant each window's first invalidation was miscounted as a **repeat** when it matched the previous window's last, a spurious `dpr_rep` landing in the **quiet** windows and biasing the ratio toward "cache it". (b) `boxrep` is relabelled a **caller-spam counter, not a cache oracle**: an identical rect does not mean identical work, because two requests for the same rectangle can bracket a state change. **I had been treating it as the cache oracle, which is a category error.**
- **AND THE INSTRUMENTATION GAP THAT HID ALL OF THIS.** `blit_max` is written **only on the full-draw path**; the partial path returns without ever timing its blit. So the one number that would have split a 90 %-not-build event from a build event **was never collected.** Splitting replay vs blit on the partial path is now the top instrumentation item, and it is what would have caught this without a consultant.

## [2026-10-04] ingest | the consultant review, properly grounded in a raw source
- Disposition: **New** (1 article + 1 raw evidence file); **Grounding repair** (see below)
- Raw: `llm-wiki/raw/evidence/2026-10-04-consultant-review-and-bbox-bug.md` (the verdict, the `-O0` host harness figures, the cache-key table, the `ri_str_follow` excerpt, both `build_max` rows, the `blit_max` gap with its line number, H1–H4 with discriminators, the bbox/zoom bug with both boxes, the consultant's answers, and what the main session did with each)
- **GROUNDING REPAIR, WHICH IS WHY THIS ENTRY EXISTS.** The previous entry put the consultant's findings in `log.md` **with no raw source behind them** — its numbers (24 µs clipped build, the 0 % cache-key hit rates, the disjoint boxes) were cited but not locatable in any `raw/` file, which is exactly what this wiki's Grounding Invariant forbids. **A log entry is not evidence.** Every load-bearing figure now sits verbatim in a raw file, with anything the main session re-verified independently marked **[re-verified]** so a reader can tell the consultant's measurement from my own.
- **WHY THE REPAIR MATTERS MORE THAN THE FINDING.** An ungrounded citation is indistinguishable, to a later reader, from a measured one. The `disjoint` boxes are the sharpest case: had that claim been left in the log alone, the next session would have had to choose between re-deriving it and trusting it, and trusting a number with no source is how a wrong one survives.
- Lint state re-verified after the addition: **230 articles + 18 evidence, 0 unindexed, 0 dead index rows, 643 links, 0 broken.**

## [2026-10-04] ingest | the partial path now times replay and blit -- and the first reading says BLIT
- Disposition: **New** (closes the instrumentation gap the consultant identified)
- `AUDIT 0/0 PASS`; `t152`, `t154`, `t166` pass. Mixed build 1013240 B, `r12moves=42`, deployed as `RIPP-RPL`. Also **gated `t166_bbox_zoom`** in `ri_audit.sh`, which the audit had (correctly) refused to pass while it was ungated.
- **THE GAP, NOW CLOSED.** `blit_max` was written **only on the full-draw path**; the partial path recorded `dp_*`/`dpw_*` and returned without timing its own replay or blit. So `build_max ~= part_max` was the only split available — and that reading is true in one window (99.8 %) and false by 10x in the next (9.7 %). `RSectionDiag` gains `dp_replay_max/sum` and `dp_blit_max/sum`, and the three components now **deliberately sum to `dp_*`**. Two extra `ReadEClock` pairs on a path costing 200 us - 1.36 s. Reported as `rpl_max/rpl_avg/blt_max/blt_avg`.
- **FIRST DELL READING, AND IT SAYS BLIT — THE OPPOSITE OF WHAT I FILED.**
  ```
  part_max  part_avg  build_max  rpl_max  rpl_avg  blt_max  blt_avg  boxrep
      235        233        113       34       33       71       69   7/1/8
     2803        555        132       56       36     2596      385   7/1/8
      2950        575        123       34       33       71       69   7/1/8
  ```
  Two windows show **`blt_max` at 2596 us with `build_max` at 132 and `rpl_max` at 56** — the blit alone is 20x the build. So on the mixed build the expensive partial is **blit-dominated**, and quiet windows put the blit at ~70 us of a ~235 us partial (30 %). **Had this split existed during the 1.36 s stall, the attribution question would have been settled by the log rather than by a consultant.**
- **AND `boxrep` HAS ALREADY CHANGED SHAPE, WHICH IS WORTH WATCHING.** It now reads **`7/1/8`** — seven repeats, one first-of-a-kind, longest run 8 — where the earlier session read `3/0/130`. **The longest run collapsed from 130 to 8, and a `dpr_new` appeared.** That is consistent with the `RI_STALE_BAR` bbox fix landing: with the damage box previously pointing at a region **disjoint from the Song Position art**, the caller was invalidating a rectangle whose art never changed, so repeats were unbounded. Now that it points at the real bar, each repeat corresponds to a bar that actually advanced — **so the spam is doing the work it was always meant to do.** A number that changed because a correctness bug was fixed is exactly the kind of change worth recording rather than noting as noise.
- **A LANE NOTE, SMALL AND PRACTICAL.** `--ui-close "#0"` closed the wrong window because a small requester was at index 0, and the pre-flight guard then **refused to launch** — correctly. The panel had to be closed by index (`#1`). **The guard earning its keep by refusing is the behaviour worth keeping**, and it is why two instances never launched here.

## [2026-10-04] ingest | CONSULTANT #2: the blit is a costume; add the GAP, the one number that settles "slow vs interrupted"
- Disposition: **New** (instrumentation), **Correction** (a false premise in my own comment), **Disputed** (my "the blit is the suspect" reading)
- `AUDIT 0/0 PASS`; `t152`/`t154`/`t156`/`t166` pass. **LANE BLOCKED for the verification run: the host spool server restarted, `/tmp` was wiped, and the Dell's agent is not dialling 9292 (2 jobs queued, never picked up). Re-dialling needs the Dell console.** So the change below is built and audited but **not yet measured on target** — stated rather than implied.
- **MY "THE BLIT IS THE SUSPECT" READING IS REFUTED, AND THE PROOF IS A SUBTRACTION I ALREADY HAD.** Taking the three phases against the total leaves a remainder — wall time inside `draw_frame` attributable to **no** phase, i.e. the four intervals between the `ReadEClock` pairs, which **contain no code**:
  ```
   row  part_max  build  replay  blit    sum    GAP   gap%
     1      235     113      34    71    218     17    7.2%
     2     2803     132      56  2596   2784     19    0.7%   <- event landed INSIDE the blit timer
     4     2950     123      34    71    228   2722   92.3%   <- same event, landed in the GAPS
  ```
  **Row 4 is decisive and needs nothing new.** All three components read normal while the total was ~2950 µs — **92 % of the draw elapsed while no component timer was running.** There is no code there. That is not a slow phase, it is the CPU not being there. **Same fault, two different victims, and only the gap distinguishes them.**
- **THE BLIT'S OWN COST NEVER CHANGED.** Solving for `n` from max/avg: row 2 has `n=8` partials, `blt_avg=385` is exactly `2596/8`, and the other seven cost **69 µs — identical to the quiet windows.** One blit was interrupted; the blit did not get slower.
- **AND THE QUIET 70 µs IS NOT THE PIXEL MOVEMENT.** Traced through AROS: `BltBitMapRastPort` → `do_render_with_gc` → `int_bltbitmap` → `HIDD_BM_CopyMemBox32`, which is **a hand-written per-ULONG loop, not `memcpy`**. Measured for this project's boxes at compact zoom: the Song Position (8.4 KB) costs **0.51 µs `-O2` / 1.70 µs `-O0`** against a measured **69–71 µs**. **The pixel movement is <3 %; ~95 % is AROS call-path overhead.** The step lamp (2.9 KB) and the Song Position (8.4 KB) both measure ~70 µs — **the cost is size-independent**, so `blit_max=` (full, 197 KB) should match `blt_max=` (partial, ≤18 KB), and if it does there is nothing left to investigate about the blit.
- **`0xC0` IS A PLAIN COPY — VERIFIED TWICE, NOTHING TO WIN.** `MINTERM_TO_GCDRMD(0xC0) == 0x03 == vHidd_GC_DrawMode_Copy`, and AROS's own `pen_minterm()` truth table over all 65 536 (src,dest) pairs evaluates `0xC0` to the identity function. Incidental finding worth keeping: **`MINTERM_TO_GCDRMD(0x00)` is `vHidd_GC_DrawMode_Clear`**, so the classic `MINCOPY = 0x0000` idiom means *clear* on AROS. `0xC0` is the correct choice here and the existing code has it right.
- **`AllocBitMap` CHURN IS LOGICALLY EXCLUDED, NOT MERELY UNQUOTED.** `dp_*` is written only inside the `d->dmg_valid` branch, which is only reachable when `d->bm != NULL` — **so a `dp_*` sample proves the buffer was valid**, and the expensive windows contain 8 samples each. A real gap remains: the realloc at lines 190–201 happens **before `eclock_open()`**, so it is timed by nothing at all.
- **THE CHANGE MADE HERE, ~6 LINES, NO NEW CLOCK READ.** `dp_gap_max/sum = us − (us_build + us_replay + us_blit)`, plus `dp_blit_min` as a **minimum** rather than another average (preemption is one-sided, so a minimum is a lower bound and needs no perturbation). Reported as `blt_min`/`gap_max`/`gap_avg`. **`dp_blit_min` is deliberately never reset** — a minimum re-zeroed every window is the same average in disguise. This is the number that would have made row 4 readable without help, and it costs a subtraction rather than a ninth `ReadEClock`.
- **⚠ A FALSE PREMISE IN MY OWN COMMENT, CORRECTED.** The `boxrep` rationale claimed a repeat "is often the one that never becomes a draw at all", on the assumption that MUI coalesces redraws. **On AROS Zune it does not**: `workbench/libs/muimaster/mui_redraw.c` calls `DoMethod(obj, MUIM_Draw, 0)` **inline, with no deferral.** So requests and draws are 1:1 here, the count could equally have lived in `draw_damage`, and the comment's premise was wrong. Kept at the request site because it is cheaper and because the distinction would matter on a backend that does coalesce — **but the reason is portability, not this platform.**
- **TWO LEVERS LARGER THAN THE BLIT, FROM THE SAME REVIEW.** (a) `MUI_Redraw` being synchronous means the steps path asks for **two boxes per step change** (old lamp *and* new, `steps[2] = {s_chase_last[k], st}`), which is two complete ~235 µs draw cycles per step — **unioning them is ~470 µs → ~250–300 µs, a ~40 % cut, provably equivalent** because `t112` already shows each box's replay covers its control. (b) Bypassing the double buffer for **partials** is ~10 lines and worth **~30 % of a partial**, with no flicker risk (a partial is one rectangle painted in one pass); `platform/host/raster.c` already *is* the direct-paint model and `t112_partial_redraw` certifies it. **Neither is the stall fix** — they remove exposure, and at ~187 preemptions/s they barely touch the tail.
- **THE COST OF MY OWN INSTRUMENTATION, MEASURED: ~8 %.** Six quiet windows put the gap at **17–19 µs**, i.e. ~2.2 µs per `ReadEClock`; 8 reads on a 235 µs partial. On AROS x86-64 a read is **not** an `rdtsc` — `ReadEClock` latches and reads **8254 PIT channel 0** by port I/O, resolution ~0.84 µs. Good: the 2722 µs gap is real to ~1 µs. Bad: **PIT port I/O is itself a plausible scheduling point**, which matters before adding a ninth read.

## [2026-10-05] ingest | the GAP is live on the Dell — and it is ~36 % of EVERY quiet partial, not just the stalls
- Disposition: **New** (first on-target measurement of the new instrumentation)
- `AUDIT 0/0 PASS` at build. Mixed build 1013720 B, `r12moves=42`, deployed as `RIPP-GAP`. **Note the lane cost two host restarts and a `/tmp` wipe, so both lane helpers were recreated — the launch guard, its "never grep the command line, it contains no `RIAPP`" note intact, and the get helper with its "this does NOT reduce wedge risk, the protocol has no ranged read" note intact.**
- **THE GAP IS NOT AN ARTEFACT OF THE STALLS. IT IS THE NORMAL CASE.**
  ```
     n | part_max build rpl blt | blt_min | gap_max
     8 |      234   113  34   70 |      68 |     121
     8 |     3918  3797  34   70 |      69 |     121
     8 |      235   114  34   71 |      69 |     122
     8 |      234   113  34   70 |      68 |     121
  ```
  Quiet windows: **build+replay+blit averages 218 µs and the gap averages 122 µs — 35.9 % of the draw**, with **~104 µs of that being real unattributed time** after subtracting the ~17.6 µs the 8 `ReadEClock`s themselves cost (measured, not assumed). So **more than a third of every routine box repaint is time in which no phase is running.**
- **THE TWO NUMBERS THAT MATTER, AND THEY ANSWER DIFFERENT QUESTIONS.** `blt_min` sits at **68–69 µs in every window including the expensive one**, while `blt_max` was 70 and 3797 respectively. A **minimum** is the right statistic precisely because preemption is one-sided: `blt_min ≈ 69 µs` is a firm lower bound on the blit's real cost, and `blt_max/blt_min` is a direct amplitude readout. The blit is **~69 µs of work, occasionally interrupted**, and it is now impossible to confuse with "the blit got slower".
- **AND THE ONE EXPENSIVE WINDOW IS A DIFFERENT FAULT FROM THE 1.36 s STALL.** `build_max=3797` against a quiet 113 — **34×** — while `rpl_max` (34), `blt_max` (70) and `gap_max` (121) all read normal. So this event landed **inside the build timer**, whereas the 1.36 s stall landed in the **gaps** at 92 %. **Three events, three different victims: inside the blit, inside the gaps, inside the build.** Which is exactly why a component split without a remainder could not have classified any of them.
- **SO WHAT IS THE 122 µs GAP, AND WHY IT MATTERS MORE THAN THE TAIL.** It is constant at ~121 µs regardless of what else happened, so it is **not** preemption — preemption is variable, and this is not. It is **fixed per-partial cost that no phase timer covers**, and it is the largest single term in a routine repaint after the build itself. Candidates, none excluded yet: the `MUI_Redraw`/`DoMethod(MUIM_Draw)` inline dispatch the consultant identified as synchronous on AROS Zune; Zune's own setup between `MUIM_Draw` and our `draw_frame`; and the four `ReadEClock` gaps' own overhead beyond the 17.6 µs accounted. **~104 µs × every box repaint is a far larger prize than the 1.36 s tail**, and it is invisible to every instrument that existed before today.
- **NEXT, AND IT IS NOW A DIFFERENT QUESTION FROM BEFORE.** Not "cache the display list" (refuted), not "fix the blit" (69 µs, 3 % pixels, size-independent), but **"what is the 122 µs, and can it be removed from the common path rather than the tail?"** The consultant's two levers now sit in that light rather than beside it: the **two-box union on the steps path** (~40 % of a partial, and with a 122 µs fixed cost per box it is plausibly ~45 % of the pair), and **direct-painting partials** (which removes a whole partial for some damage entirely, and therefore a whole 122 µs with it).

## [2026-10-05] ingest | the chase box union: LOSSLESS and 26 % CHEAPER despite painting 3.6x the area
- Disposition: **New** (a real optimisation, measured before and after), **New test** (`t167`)
- `AUDIT 0/0 PASS`; `t167_chase_box_union` gated in `ri_audit.sh` (and the audit had correctly refused to pass while it was ungated, as with `t166`).
- **WHAT CHANGED.** The drum chase invalidated **two boxes per step change** — the old lamp and the new — as two separate `ri_rsection_refresh_box_why` calls. Because **`MUI_Redraw` is synchronous on AROS Zune** (`mui_redraw.c` calls `DoMethod(obj, MUIM_Draw, 0)` inline, no deferral), each call was a **complete draw cycle**, not a queued one. `app/riapp.c` now unions the two rectangles and invalidates **once**.
- **THE WIN, IN THE UNITS THAT MATTER.** A quiet box repaint on the Dell is **~234 µs, of which ~122 µs is the fixed per-partial cost the gap instrument exposed.** Two of them is **~714 µs per step change**, and the chase moves constantly.
- **CORRECTNESS PROVEN, NOT ASSUMED: `t167` COMPARES 448 LAMP PAIRS (both rows, every i≠j) AND THE UNION IS PIXEL-IDENTICAL** to two separate box replays, starting both strategies from the same fully-painted frame. **A union is only correct if it repaints both controls and nothing between them goes stale, and that is now asserted against the two-box result rather than against the code's own arithmetic.**
  - **A TEST DEFECT WORTH RECORDING, because the first run failed and the union was innocent.** The ground truth was an *unpainted* buffer, so the union "differed" in 1517 px at x=471-473 — **the region between the lamps**, which the two-separate approach leaves untouched. The union repainting it is *correct behaviour*, not a bug. Fixed by painting the section fully once and letting both strategies start from it. **A test whose baseline is wrong will condemn the right code, and the pixels it complains about are usually exactly where the correct behaviour differs from the broken one.**
- **THE COST, MEASURED — AND IT INVERTED MY OWN PREDICTION.** I expected the union to be *worse*, because the two boxes average **2764 px** while their union averages **9881 px** (3.6x the area), and the Dell's build/replay are clip-aware so cost tracks area. Host measurement over every pair:
  ```
    TWO boxes :   83.1 us/pair   (mean area 2764 px)
    ONE union :   61.2 us/pair   (mean area 9881 px)
  ```
  **The union is 26 % CHEAPER while painting 3.6x the pixels.** Cost here is dominated by **fixed per-draw overhead, not pixels** — which is the same conclusion the 122 µs gap reached independently, from the other direction. **Painting more pixels in one pass is cheaper than painting fewer in two, and the earlier "direct-paint partials" idea is now better supported still: the fixed cost, not the pixels, is what to remove.**
- **SCOPE, STATED PLAINLY.** The host figure is a **proxy** — it times `ri_raster_replay_box`, not the guest's full `draw_frame` (no `build_dl`, no `BltBitMapRastPort`, no Zune dispatch). It is good enough to reject my own pessimism and to justify shipping the change, and **the decisive number is the Dell's `part_max` and `n=` with this build.** Note too that the biggest win is on **adjacent** lamp changes (small union); far-apart ones have a larger union, and the 16-row wrap (15→0) is the worst case — which is exactly the `boxrep` run the chase produces, and worth watching on target rather than assuming.

## [2026-10-05] ingest | the union is deployed and NO on-target win is demonstrated — the run never exercised it
- Disposition: **Correction** (to the expectation set by the previous entry, not to the code), **New** (what the target lane actually spends its partials on)
- `AUDIT 0/0 PASS`. Mixed build 1013912 B, `r12moves=42`, deployed as `RIPP-UNION` after finding the guest already clear (`RIPP-GAP` had exited on its own; the launch guard reported 0 instances and the deploy went ahead single-instance).
- **⚠ NO WIN IS DEMONSTRATED, AND THE REASON IS WORTH MORE THAN THE NUMBER.** `box_steps` is **`0/0/0` in every window, both before AND after the change**:
  ```
  PRE-union  (RIPP-GAP)     box_steps non-zero windows: 0 of 14
  POST-union (RIPP-UNION)   box_steps non-zero windows: 0 of 14
  ```
  **The drum chase never fired.** Every one of the 8 partials per window was `box_bar` — the Song Position display. **So the previous entry's "~714 µs per step change, ~26 % cheaper" is a host proxy and a sound argument, but it is NOT an on-target result, and it must not be quoted as one.** `n=8` and `part_avg≈232 µs` are unchanged because nothing about the chase was in the measurement.
- **WHY THE CHASE DID NOT FIRE, AND IT IS A LANE LIMITATION, NOT A BUG.** The chase draws only for sections whose canvas is on screen and whose pattern is moving; the visible tab here is not the drums, and **`--ui-rawkey` has no modifier support**, so `Ctrl+1..4` (which is how tabs switch, per `gui/keymap.c`) **cannot be scripted**. Getting a tab onto the 808/909 rows needs a human at the Dell or an agent-side change to add modifier injection. **Recorded as the concrete blocker for verifying this change on target.**
- **WHAT THE LANE IS ACTUALLY SPENDING ITS PARTIALS ON, which is the more useful finding.** With `box_steps=0/0/0` and `box_bar` at n=8, **the Song Position display accounts for essentially all box repaints** — ~8 per 30 s window at ~232 µs, of which ~122 µs is the fixed per-partial cost. So the chase union, while correct and worth keeping, addresses a path this lane barely exercises, **while the path it does exercise is the one the 122 µs gap is charged to.**
- **SO THE PRIORITY ORDER SHIFTS AGAIN, and this time on measured evidence rather than on an argument.** The next target is **the Song Position partial**, and the lever the consultant's second review named for it is the stronger one: **direct-paint partials** — skip the offscreen friend bitmap and the `BltBitMapRastPort` for a damage box entirely. That removes the blit's ~69 µs of *measured* work (`blt_min` is a firm lower bound), removes the bitmap allocation and the buffer-vs-`wrp` divergence class of bug, and `platform/host/raster.c` already *is* that model with `t112` certifying it. **It does not remove the ~122 µs fixed cost**, which sits in the Zune/`MUI_Draw` dispatch rather than in any phase we time — and that is now the single largest term in a routine repaint, and the next thing to attribute.

## [2026-10-05] ingest | direct-paint partials: a NULL RESULT. The blit was free; the window rastport is not.
- Disposition: **New** (measurement), **New defect found in my own change**, **Recommendation** (revert — owner's call)
- `AUDIT 0/0 PASS`; `t112`/`t152`/`t155`/`t166`/`t167` pass. Mixed build 1013328 B, `r12moves=42`, deployed as `RIPP-DP2`.
- **THE MEASUREMENT, QUIET WINDOWS, 13 OF THEM:**
  ```
                        part_avg   build   replay   BLIT    gap
  through the buffer       232-234   113       34      70    121
  DIRECT-PAINT              238      117      105       0    119
  ```
  **The blit is gone (0, by construction) and the replay absorbed exactly what it cost: 34 → 105 µs, i.e. +71 against the −70 that was removed. Net zero.** The build, the gap (119 vs 121) and the total (238 vs 233, inside the run-to-run spread) are all unchanged. **Direct-painting partials buys nothing on this platform.**
- **WHY, AND IT IS A SATISFYING ANSWER.** `BltBitMapRastPort` reaches AROS's `HIDD_BM_CopyMemBox32`, a straight per-ULONG block copy — and the consultant had already measured the pixel movement at **under 3 % of the blit's 69 µs**, the rest being call-path overhead. So the blit was cheap *precisely because it is a block copy*. The alternative is not free: painting into **`wrp`** goes through the window rastport, where Zune's clip and the layer are live, and that costs about what the copy did. **The buffer was not the expensive thing. Rasterising into a window is.**
- **⚠ A DEFECT IN MY OWN CHANGE, CAUGHT BECAUSE I CHECKED AN INVARIANT.** The first direct-paint build removed the `BltBitMapRastPort` **call** but left its **clock pair**. `ub` was then computed as `eclock_us(&tr, &te)` where `tr` predated the *replay*, so **`blt` came back as a duplicate of `rpl`** (~103 vs ~100), the three components stopped summing to `dp_*`, and **the GAP was silently clamped by its own `if (acct < us)` guard** — which would have quietly understated the largest number in the table. Visible only because the first run showed `blt ≈ rpl`, which is impossible for a real blit. **An instrument that reports a phase that does not exist is worse than one that omits it**, and the guard that would have caught it (`sum(components) == total`) was the thing I had already written and not applied.
- **THE RECOMMENDATION IS TO REVERT, and the reason is risk, not cost.** There is **no measured win**, and direct painting introduces a behavioural hazard the buffered path does not have: a partial interrupted mid-repaint now leaves a visibly torn rectangle on screen, whereas before it was composited from the offscreen buffer. It would also give up the removal of one allocation and one buffer-vs-`wrp` divergence class — real, but not worth a new artefact on an unattended machine for **zero** measured gain. **Left in the tree, measured and revertible, because discarding the work is the owner's decision and the measurement is the part worth keeping.**
- **WHAT THIS LEAVES AS THE NEXT TARGET, UNCHANGED AND NOW UNAMBIGUOUS.** The blit is **not** a lever (free). The build is **not** a lever (113 µs, stable, clip-aware). The chase union is correct and cheaper per replay but this lane barely exercises it. **The ~119 µs GAP is the largest term in a routine repaint and is unchanged by everything tried** — it sits in the Zune/`MUI_Draw` dispatch rather than in any phase we time. **That is now the only remaining target for the common path, and it needs attributing before it can be removed.**

## [2026-10-05] ingest | ⚠ THE 119 µs GAP NEVER EXISTED. It was `max` where the arithmetic said `sum`.
- Disposition: **Disputed→Resolved** (my headline finding of the previous two entries was an ARTEFACT), **New test**, **Reverted** (direct-paint partials)
- `AUDIT 0/0 PASS`; `t112`/`t152`/`t155`/`t166`/`t167`/`t168` pass. Mixed build 1013880 B, `r12moves=42`, deployed as `RIPP-SUM`. **The corrected instrument is restored on target.**
- **THE BUG, IN THREE LINES.** `dp_gap` was `us - max(build, replay, blit)`. **The phases are disjoint intervals, so `us - max` equals the OTHER TWO PHASES plus the true residue.** The counter did not lose 105 µs to the scheduler — it booked the replay and the blit into a bucket labelled "no phase".
- **MY OWN LOG LINE REFUTED IT, AND I DID NOT LOOK.** Every mean on `RIAPP draw:` shares one denominator, so `part_avg == build_avg + rpl_avg + blt_avg + gap_avg`:
  ```
    buffered      233 == 113 + 34 + 70 + gap  =>  gap = 16     (logged: 121)
    direct-paint  238 == 117 + 105 + 0 + gap  =>  gap = 16     (logged: 119)
  ```
  `121 − 16 = 105 = 34 + 70`, exactly, in both configurations. **Checkable by hand, from any line already in the log.**
- **I HAD THE RIGHT NUMBER A DAY EARLIER AND THEN IMPLEMENTED A DIFFERENT FORMULA.** The 2026-10-04 entry hand-summed the same fields and recorded **17 µs / 7.2 %**, and justified the instrumentation on it. The 2026-10-05 headline — *"the GAP is 36 % of every quiet partial, ~104 µs of it real unattributed time"* — is the artefact, and it **directly contradicts my own entry from the day before.** **THE CORRECTED MEASUREMENT ON TARGET: `gap_avg = 17.6 µs` on quiet windows, where the broken formula reported 121.**
- **AND THE "IT IS NOT PREEMPTION BECAUSE IT IS FLAT" ARGUMENT WAS AN ALGEBRAIC IDENTITY, NOT AN OBSERVATION.** `us - max` is flat **exactly when the other phases are flat**, and build is the largest phase in every logged window — so the gap's flatness carried zero evidential weight. `t168` now pins this directly: hold `us` and the largest phase fixed, move a non-largest one, and the `max`-gap **cannot move** while the `sum`-gap **must**. **An argument built on a quantity that is constant by construction is not evidence.**
- **`t168_gap_accounting`, gated, host-only, and it fails on the old code.** Six recorded tuples including both Dell quiet windows and the 1.36 s stall; asserts `gap == us − sum`, asserts the **partition invariant** (`phases + gap == total`) that I had written down and never applied, and names the failure mode by showing `max` inventing 120 µs on the quiet window and the difference being *exactly* the two discarded phases. **Mirroring t152's discipline: the widget is AROS-only, but the arithmetic is portable, and a rule checkable in five lines on the host should be.**
- **THE REAL FLOOR, STATED HONESTLY.** A quiet partial is **233 µs = 112 build + 33 replay + 69 blit + 18 unattributed**, and **~14 µs of that 18 is my own eight `ReadEClock` calls** (measured at ~2.2 µs each; `ReadEClock` on AROS x86-64 reads 8254 PIT channel 0 by port I/O). So the **uninstrumented structural floor is ~2 µs**, not 119. The largest real term is now the **build at 112 µs (48 %)**.
- **THE PARTITION INVARIANT HOLDS ON TARGET, to truncation:** every quiet row satisfies `build+rpl+blt+gap == part` within 2 µs, which is what four independent `floor(sum/n)` averages must cost. **Worth stating as truncation rather than claiming exactness.**
- **DIRECT-PAINT PARTIALS REVERTED, now for three reasons instead of one.** Beyond net-zero cost and the torn-rectangle risk, **AROS explains the null result a priori:** `do_render_with_gc` branches on `rp->Layer` — the offscreen rastport has `Layer == NULL` and skips `LockLayerRom`, while a **window** rastport takes the layer path (`LockLayerRom`, `ObtainSemaphore`, a clip-rect walk) **per drawing primitive**. The buffered path pays that once, in the blit; direct painting pays it many times. **It gets worse on any larger box, so it was never a marginal call.**
- **⚠ WHAT THIS MEANS FOR THE 2026-10-05 ENTRY, WHICH IS NOW WRONG.** *"The ~119 µs GAP is the largest term in a routine repaint and is unchanged by everything tried"* is **false on both counts** — it was never the largest term, and the build, replay and blit were never the ones it accused. **The genuine remaining targets are the 1.36 s tail and idle-time wedge; the common path's 233 µs is now correctly attributed at 48 % build / 30 % blit / 14 % replay / 6 % instrumentation.**
- **THE STANDING LESSON, AND IT IS THE FOURTH TIME THIS SHAPE HAS APPEARRED.** `build_max ≈ part_max` true of one row and not the other; `boxrep`'s "MUI coalesces" premise; a dead clock pair re-timing the replay; and now `max` where `sum` was documented. **Every one was an instrument disagreeing with its own documentation, and in every case the documentation was right.** The rule that would have caught all four is the one I wrote and did not apply: **assert that your components account for your total.**

## [2026-10-05] ingest | ITEM CULL: the build was 112 µs of which 98 % was thrown away. Now 56 µs.
- Disposition: **New** (the cut), **New** (bench_build), **New** (t169), **New fact** (`ri_geo_item_box` shared), **Method** (safety test ≠ presence test; the stale-object trap fired again)
- `AUDIT 0/0 PASS`. `t112`/`t155`/`t166`/`t167`/`t168`/`t169`/`t93` pass. Mixed build 1014432 B, `r12moves=42`, deployed as `RIPP-CULL`. **Proven on target.**
- **THE ARITHMETIC THAT SET THE TARGET.** With `dp_gap` corrected (`max` → `sum`, t168), a quiet box repaint is **233 µs = 112 build + 33 replay + 69 blit + 18 unattributed**, and ~14 of that 18 is instrumentation. So the build became the **largest real term at 112 µs (48 %)** — and the replay and the blit had both already been measured as not-a-lever (the blit is a cheap block copy; direct painting traded it for a layer lock per primitive).
- **THE BUG IN THE DESIGN, MEASURED BEFORE ANY CODE CHANGED.** `bench_build` (new, host, exempt from the gate with its reason recorded) prints per section what the whole section emits against what a 64×16 damage box keeps. **A 64×16 box keeps 2.0 %: 544 of 26 766 commands across the 21 sections.** The damage clip drops commands at **push** time, so it saved the replay and never the drawing — every item in the section was still resolved, measured and emitted.
- **AND IT IS THE ITEM LOOP, NOT THE BACKGROUND.** Splitting the build by calling the same `ri_art_bg_*` dispatch `ri_draw_section` uses: | section | items | built | bg | items | kept | us before | us after | | | | | | | | | | 808 | 95 | 3605 | **174** | **3431** | 14 | 61.4 | **8.0** | | 909 | 87 | 4967 | **364** | **4603** | 47 | 92.2 | **16.2** | | SYNTH1 | 69 | 3009 | 566 | 2443 | 52 | 53.6 | **9.9** | | LEVI | 108 | 3031 | 916 | 2115 | 119 | 82.3 | **43.3** | | 21 sections | 685 | 26766 | — | — | 544 | **490** | **156** | The item loop is **70–93 %** of a build, at **35–53 commands per item**. LEVI gains least because most of its items carry no damage box, so they are drawn — the conservative branch, and worth 1.9× rather than 7.7×.
- **THE CUT: skip an item whose own box is disjoint from the clip.** Exact, not approximate: every command such an item emits lies outside the clip, and the clipped replay already drops every command that misses the box. Two deliberate non-culls: a shape with no damage box (static legends, dividers) is **drawn**, because "no box" is not "no pixels"; and the section background is left alone, being section-wide by construction.
- **ONE FORMULA, TWO CALLERS — the part that keeps it honest.** The box rule moved out of `ri_geo_bbox`'s union loop into a new `ri_geo_item_box`, and `ri_geo_bbox` now unions those. **Growing each box by `RI_GEO_BBOX_MARGIN` and then unioning equals unioning and then growing**, for axis-aligned rectangles, so the refactor changed no result — and `t112` (which is the pixel proof for `ri_geo_bbox`) not moving is the evidence, not an assumption. **Had the cull carried its own copy of the formula, the two would have been free to disagree and the parity test would have been testing the copy.**
- **ON TARGET (11 quiet windows):** | | before | **after** | | | | | | part_avg | 232.6 | **177.5** | | **build_avg** | **112** | **56.2** | | replay_avg | 33 | 34.3 | | blit_avg | 69 | 68.9 | | gap_avg | 17.6 | 17.6 | **Replay, blit and gap do not move — which is the claim.** The change is localised to the build, and the partition invariant holds with **0 mismatches across 12 rows**. `xruns=0`, `wake_max=47 µs`, `overloads=0`, `prio=21`: audio untouched.
- **⚠ `t169` PROVES SAFETY AND DELIBERATELY DOES NOT PROVE PRESENCE.** With the cull removed, every assertion still passes — the push-time clip produces the same list, which is the whole point of the equivalence. So the test is the **safety** proof (over-culling puts a hole on screen; under-culling is merely slow) and `bench_build` is the **win**. **A test claiming to be both would be claiming the cull by asserting that it happened.** It compares, for **every item of every section**, the culled build against the unclipped build filtered by `ri_dcmd_hits_box` (production, not a copy) **command for command** — including op, all four coords, rgb, img, frame, align, `pad[0]` and the text string. Per item rather than one box, because a single box would pass while one item's box is wrong: the survivors would come from its neighbours.
- **THE MUTATION SET: 4 of 5 KILLED, and the survivor is the design speaking.** | mutant | verdict | | | | off-by-one `<=` for `<` | **KILLED** (`sec=0 item=23 … culled 67, the clip alone would keep 70`) | | "no damage box" → skip | **KILLED** — so the conservative branch is load-bearing *and* covered | | `RI_GEO_BBOX_MARGIN` dropped | **KILLED** by `t169` **and** `t112` | | `ox`/`oy` dropped from the cull's test | **SURVIVED, then KILLED** — see below | | cull removed entirely | **SURVIVED by design** |
- **⚠ THE STALE-OBJECT TRAP FIRED A FIFTH TIME, AND IT FALSELY REPORTED A SURVIVOR.** Mid-run I ran the margin mutant and read `SURVIVED`. It was a **stale build**: the preceding mutant had failed to compile, `ri_build_host.sh draw` had therefore not run, and `test` links `$OUT/*.o` without rebuilding. Rebuilt properly, the margin mutant **is** killed by `t169` and `t112`. **A `SURVIVED` printed straight after a BUILD BREAK is a fact about the harness and nothing about the code.** Two more of the same run: `git checkout gui/panelgeo.c` silently reverted an uncommitted refactor (the link then failed on `ri_geo_item_box`) because only `art_section.c` had been backed up — **back up every file a mutation touches, not the one you are looking at** — and a first mutant that failed to compile was nearly recorded as a kill.
- **`M4` FOUND A REAL GAP IN MY OWN TEST, NOT JUST IN THE CODE.** Dropping `ox`/`oy` from the cull's comparison survived `t169`, `t112`, `t166` and `t167`. The reason is benign — **production only ever sets a clip on the partial path, which builds at `ox = oy = 0`** — so the offset arithmetic was unexercised and therefore untested, and would have stayed that way until someone built at an offset. `t169` grew a third arm: a non-zero origin (`37, 23`) with the box expressed in the same space as the commands, which is the only convention `ri_dcmd_hits_box` uses. `M4` is killed immediately. **"Equivalent in production" is not "tested"; the difference is a future refactor.**
- **THE PARTITION INVARIANT IS NOW CHECKED IN THREE PLACES, WHICH IS THE POINT OF HAVING WRITTEN IT DOWN.** `t168` asserts it on recorded tuples; the log analysis asserts it on every Dell row (**0 mismatches / 12**); and the build/clip split in `bench_build` is the measurement that made the build's 98 % visible at all. **The build had been the largest term for at least two sessions and no instrument could see it, because the instrument was subtracting the wrong thing.**
- **WHAT IS LEFT IN A QUIET REPAINT, HONESTLY:** 177 µs = **69 blit (39 %) + 56 build (32 %) + 34 replay (19 %) + 18 gap (10 %, ~14 of it instrumentation)**. The blit and the replay are both already measured as not-a-lever, and the gap's structural floor is ~2 µs. **So the next target is the remaining 56 µs of build, and within it the section background — 174–916 commands that are section-wide and therefore unculled — plus LEVI's 43 µs, where most items carry no damage box and are drawn.** Neither is proposed here.

## [2026-10-05] ingest | cold reboot, the lane restored, and the strip seek — which t169 caught breaking
- Disposition: **New** (lane restored), **New** (`lane/` in the repo), **New** (the 303 strip seek), **New** (`bsec` attribution), **Defect in my own change** (caught by t169), **Refuted** (my guess about the Levi's background)
- `AUDIT 0/0 PASS`. t93/t112/t155/t166/t167/t168/t169 pass. **The guest read is OUTSTANDING — the Dell's agent has not dialled since the boot** (last job 09:38:22, boot 09:55), so every host-side number below is host-measured and nothing is claimed on target.
- **THE LANE, AND TWO THINGS THE OWNER'S "LISTEN TO ALL PORTS" UNCOVERED.** Bridge listening on 9292, pairs file valid, host IP unchanged at 192.168.1.81, Dell pings at 0.13 ms. Two real gaps: **the Dell's unit declared no `--bulk-port`, so it silently inherited the 9092 default that `spike-v4.service` already claims** — and v4's own journal reads `OSError: [Errno 98] Address already in use`, i.e. one of the two lanes was left with no bulk listener. Now `--bulk-port 9192` explicitly, with a ufw rule from 192.168.1.60. **A configured flag is not an open port.** It matters here because `BULK_MIN` is 1 000 000 bytes and an RIAPP binary is 1 014 432 B — **over the line**, so every binary push uses the bulk channel. **And the correction to my own first reading: 9192 not appearing in `ss -ltn` afterwards is CORRECT** — `_bulk_listener` binds per transfer, on demand. I had inferred a second fault from an absence that is the designed behaviour.
- **THE LANE HELPERS ARE IN THE REPO NOW, IN `lane/`, AND THE DIRECTORY IS THE POINT.** These two helpers lived only in `/tmp/opencode` and died with **every** reboot — eight losses recorded. The repo rule from 2026-09-27 is "git holds everything load-bearing, `/tmp` only regenerables". They are NOT in `scripts/`, because that directory is gated to hold **exactly five** shared scripts (`test "$(ls scripts | wc -l)" = 5`) and the gate is right: these are lane plumbing for one machine. They are NOT in `tools/`, which is gated against hard-coded Amiga paths and the `RAM:` in the launch form would trip it — **a real constraint, not one to route around.** So `lane/`, and the audit passed without being touched.
- **THE NEXT TARGET, CHOSEN BY MEASURING INSTEAD OF ASSUMING.** With the item cull in, `bench_build` (extended with `us_bg` and a per-op background split) says the **section background is 37–39 % of the remaining clipped build** — 67 % of SYNTH1, 54 % of LEVI at 22.8 µs — and the op split says it is **~420–760 plain RECTs, not text** (LEVI 761 rect / 58 line / 97 image / 97 text; SYNTH1 422 rect / 144 line / 0 text). **Text was the obvious suspect and it is not the cost**; a rect is four additions and a store, and what makes these expensive is that each is one of ~28 horizontal strips with **two colour computations and a division**.
- **THE CUT: a band test plus a strip seek, in `ri_art_bg_303`.** A key is one rectangle, so a key disjoint from the damage box emits nothing the replay would keep (its two lip lines live inside that rectangle). And the strip loop now **seeks** to the first strip that can reach the box instead of walking all 28 from the top. New host helper `ri_dlist_band_missed()` in canvas.c, which **returns 0 when no clip is set — so the full-draw path is byte-for-byte unchanged and the goldens do not move.** Host: **SYNTH1 clipped build 9.95 → 8.00 µs (1.24×), its background 6.68 → 4.76 µs.**
- **⚠ t169 CAUGHT MY OWN BUG WITHIN THE SAME SESSION — the first time it has caught something I wrote.** The first seek asked for the first strip whose **top** is below the box. That drops the strip that **straddles** the box's edge, which the intersection test would have kept: `t169` reported **69 commands where the clip alone keeps 70**, at `sec=0 item=23`. The fix seeks to the first strip whose **bottom** reaches the box (`need = cy0 - st + 1`). **The symptom would have been an invisible one-strip gap in a key's colour grade** — nothing that announces itself, on a machine nobody is watching. **This is the argument for a parity test over a pixel test, and for writing the cut as "equivalent" before writing the code:** I had written "seek to the first strip whose BOTTOM edge reaches the box's top" in the comment while the arithmetic did the opposite. **The comment was the specification and the code did not match it.**
- **A MUTANT THAT SURVIVED AND SHOULD HAVE.** `cy0 - st` for `cy0 - st + 1` survives `t169` and `t112`. It is not a coverage gap: it seeks one strip **earlier**, which draws slightly more and lets the push-time clip reject it — the **safe** direction. The parity test pins the **lossy** direction, which is the one that puts a hole on screen. **A mutation in the direction the design already backstops is invisible by construction, and that is the design working rather than the test failing.**
- **⚠ MY GUESS ABOUT THE LEVI WAS REFUTED BY THE NUMBER, AND SAYING SO MATTERS MORE THAN THE CUT.** I expected the two dashed-divider loops to be the Levi's background cost — they are a contiguous Y band, the exact shape a band test skips. Band tests are in, and LEVI's background moved **22.82 → 22.04 µs: 0.8 µs, 3 %.** The cost is the `DKNOB` table — `ri_art_knob` (trig per knob) and `ri_art_disc_grad` — at **24 ns/command against SYNTH1's 8**, the most expensive per command in the build. **The remaining 22 µs is not addressed by this change and is not claimed to be.**
- **`bsec`/`bsecsum` ADDED, BECAUSE I WAS AIMING AT AN ASSUMPTION.** The item cull was aimed using a guess about which section the lane repaints, never measured — **the same mistake shape as the gap's `max`, one level up.** Each canvas is one section, so `RSectionDiag.dp_sec` is constant per widget and the heartbeat prints the argmax of `dp_build_sum` and that section's sum, with **255 when no canvas reported at all** so "nothing built" cannot be read as "section 0 built". **This is the instrument that would have caught the assumption, and it is on target and unread.**
- **METHOD, ADDED TO THE STANDING SET.** *Attribute before you cut.* I had a plausible story for the Levi's background, wrote the cut for it, and the measurement moved 3 %. The band test is still correct and still helps SYNTH1 by 24 %; the difference is that I now know the Levi's 22 µs is knobs and gradient discs, which is a **different cut with a different risk** (per-entry extents that would have to be verified rather than read off the source, and `t169` is what would verify them). **A cut aimed by inference from a background is a guess wearing a measurement's clothes.**

## [2026-10-05] ingest | `ri_art_disc_grad` rows: the knob was O(r²). LEVI's clipped build 42 → 30 µs.
- Disposition: **New** (the cut), **Rejected** (the same idea on `ri_art_panel`, with an unexplained anomaly left open), **Method** (a mutation that silently did not apply), **Mutants** (3 run, 2 killed, 1 correct survivor)
- `AUDIT 0/0 PASS`. t93/t112/t155/t166/t167/t168/t169 pass, exit status each. **Still no guest: the Dell's agent has not dialled since the 09:55 boot, so nothing here is claimed on target.**
- **THE COST WAS AN ALGORITHM, NOT A COUNT.** `bench_build` said the Levi background ran at **24 ns/command against SYNTH1's 8**. The reason is `ri_art_disc_grad`: `art_hw` walks `dx` down from `r`, so one disc is **O(r²)**, and `ri_art_knob` is **11 tick discs plus 6 gradient discs** — a few hundred commands *and* a few hundred iterations for one knob, and the Levi's `DKNOB` table is 108 knobs. **Nothing in the earlier count-based reasoning could have found this: the command count was never the problem, the per-command iteration count was.**
- **THE CUT: clamp the disc's rows to the damage box.** Exact, and simpler than the 303 strip seek that preceded it. Each row is a **one-pixel-tall** rect, so a row intersects the box iff its y lies inside it — **no straddling case, therefore no seek arithmetic here to get wrong**, which is precisely the arithmetic that was wrong the first time. The x test rejects the whole disc (`art_hw` never exceeds `r`, so `r` bounds every row's half-width). **With no clip set the loop is the original one and the goldens do not move.**
- **HOST, CLIPPED BUILD PER SECTION:** | | 303 strip seek only | **+ disc_grad rows** | | SYNTH1 | 8.00 | **6.34** | | 808 | 8.16 | **6.56** | | 909 | 16.53 | **13.23** | | LEVI | 42.01 | **30.38** | | 21 sections | 152.1 | **127.6** | and `kept` is **544 — identical to the unclipped reference**, which is the exactness check restated as a number. Background share of the remaining build **37 % → 32 %**. **Cumulative for the session's cuts: 490 → 128 µs, 3.8×.**
- **MUTANTS: `dy0 +1` KILLED, `dy1 -1` KILLED, `dy0 -1` SURVIVED.** The third seeks one row **earlier**, draws slightly more, and lets the push-time clip reject it — the **safe** direction. The parity test pins the lossy direction, which is the one that puts a hole on screen, so a mutation in the direction the design already backstops is invisible **by construction**. That is the design working, not the test failing.
- **⚠ AND A MUTATION THAT SILENTLY DID NOT APPLY, WHICH IS THE FIFTH OCCURRENCE OF THIS SHAPE.** The first two attempts at those mutants reported `SURVIVED` because my `str.replace` matched an 8-space indent where the line has 12, so **no mutation was written at all** and the "survivor" was the clean build passing. That is the 2026-10-03 lesson — *a build that fails is not a kill* — **and its converse, which is worse because it looks like rigour: a build that succeeds is not proof that anything was mutated.** Fixed by asserting the match is unique, grepping the mutated text back out of the file before running, and printing `[verified applied]` per mutant. **A mutant that did not apply and a mutant that survived print identically unless you check.**
- **⚠ REJECTED: THE SAME IDEA ON `ri_art_panel`, WITH AN ANOMALY I DID NOT EXPLAIN.** A panel is one big rectangle of horizontal strips, so the identical row clamp should apply, and it measured a real win (SYNTH1 background 4.76 → 2.07). **t169 rejected it: for SYNTH1 item 3's box `143,22..209,88` the culled build kept 237 commands where the clip alone keeps 23** — an order of magnitude the wrong way, and more commands than the box can hold. Reverted, not shipped.
  - **What is NOT understood is why, and that is the honest state.** Both counts use the same production predicate `ri_dcmd_hits_box` on the same box, so a surviving command the reference rejects means the two builds emit commands at **different coordinates** — and neither the row clamp nor the hairline seek moves a coordinate.
  - **⚠ AND THE BISECT THAT WAS SUPPOSED TO ANSWER IT MADE THINGS WORSE BEFORE IT ANSWERED ANYTHING.** Reconstructing "the original" by string surgery dropped **two of the panel's four edge lines**, which is what turned the next `t93` failure into a mystery about the goldens rather than about the panel. **A revert performed by retyping the function is not a revert.** `git checkout` on the file was the correct move and it is what recovered it.
  - **So: the panel is untouched, its win is unclaimed, and the next thing is a focused probe of the two builds' coordinates — not another guess.**
- **METHOD, THE THIRD ENTRY THIS WEEK.** *Prefer the primitive that cannot be wrong over the clever one.* The disc row clamp is a **one-pixel row range** — there is no seek formula to get wrong. The 303 strip seek was a **seek formula**, and I got it wrong the first time and t169 caught it. The panel attempt was a **seek formula over a two-pixel stride**, and it produced an anomaly I still cannot explain. **The complexity of the arithmetic is the risk, and one-pixel rows are the case where the arithmetic disappears.**
- **WHAT IS LEFT IN THE BUILD, HONESTLY.** Host clipped 127.6 µs across 21 sections, **32 % of it still background**. Per section the remaining leaders are **LEVI 30.4** (its `DKNOB` knobs are now row-clamped but 108 of them still each emit ~200 commands), **909 13.2**, **SYNTH1 6.3**, **808 6.6**, **TRANSPORT 5.5** — and TRANSPORT is the section this lane actually repaints, where `bg` is 0.83 of 5.52, so **the transport's cost is 85 % item work, not background.** The `bsec` instrument that would confirm that on target is deployed and unread.

## [2026-10-05] ingest | ON TARGET: `bsec` says TRANSPORT, 100 % of windows — and two commits of art optimisation do NOTHING for this lane
- Disposition: **Answered** (which section owns the build), **Refuted** (my own last two commits, for this lane), **Bulk path proven**, **Invariant re-confirmed**
- `AUDIT 0/0 PASS`. `RIPP-DISC`, build `9482915`, 1016016 B, `r12moves=42`, mixed. `xruns=0`, `wake_max=46 µs`, `overloads=0`, `prio=21`.
- **THE BULK LANE IS PROVEN, NOT JUST CONFIGURED.** The push and the pull both report `[bulkget] … 34725/34725 B, 1318 ms OK (sha verified)` at **1 016 016 B**, which is over `BULK_MIN` (1 000 000). So the `--bulk-port 9192` change is **exercised end to end**, which is the only thing that distinguishes "the flag is set" from "bulk works". The guest's agent also redialled on its own once the bridge was up: `ping -> ok=True (2 ms)`.
- **`bsec=13` = RI_SEC_TRANSPORT IN EVERY SINGLE WINDOW WITH A REPAINT — 16 of 16, no exceptions.** `bsecsum` 447 of `build_avg` 55 × n=8 = 440, i.e. **essentially the entire build is the transport**, and `box_bar` reads `175/177/8` throughout: the Song Position box, 177 µs worst, 8 repaints per window. `bsec` prints **255** on the idle windows, as designed, so "nothing built" is not readable as "section 0 built".
- **⚠ SO THE LAST TWO COMMITS ARE WORTH NOTHING ON THIS LANE, AND I AM SAYING SO BEFORE ANYBODY ASKS.** | | build_avg | part_avg | | | RIPP-CULL (`15bfd8d`, item cull only) | 56.2 | 177.5 | | **RIPP-DISC (`9482915`, + 303 strip seek + disc_grad rows)** | **55.4** | **175.9** | A **1.4 %** move on a run-to-run spread that has been ±2 µs all session. **The cuts are real on the host — SYNTH1 8.00 → 6.34, LEVI 42.0 → 30.4, 21 sections 152 → 128 µs — and they target SYNTH1 and LEVI, which this lane never paints.** Had I not built `bsec` first I would have read 56 → 55 as confirmation and shipped a claim that is false for the machine the owner uses. **The instrument cost one commit and prevented a wrong headline; that is now the third time on this lane that the cheap check was the expensive one.**
- **THE PARTITION INVARIANT HOLDS ON EVERY ROW, TO TRUNCATION.** 17 rows, max residual **2 µs**, which is what four independent `floor(sum/n)` means must cost. Including the 573 µs row (whose blit is 466 — an interrupted blit, and the gap correctly stays at 17 while the total goes to 573, which is the exact discrimination the gap exists for).
- **WHERE THE TRANSPORT'S 55 µs ACTUALLY IS, AND IT IS NOT THE BACKGROUND.** The host bench, unclipped, on the transport alone: 31 items, **1015 commands, of which the background is 78**; clipped, 5.52 µs of which `bg` is **0.83 (15 %)** — so **85 % is item work**. And `box_bar` says the surviving item is the Song Position display.
- **THE STRUCTURAL REASON, AND IT IS THE INTERESTING PART.** The transport's item list is full of `RI_GEO_LEGEND` entries — `{ TR(2), RI_GEO_LEGEND, 0, 80, 178, 0, 0 }` and its neighbours, **`w = 0, h = 0`**. `ri_geo_item_box` returns **2** for a legend, which is the *"no declared damage box"* case, and the item cull **deliberately draws those rather than skipping them**, because "no box" is not "no pixels". That was the right call — it is the lossy direction and `t169` proves it. **But it means ~900 of the transport's ~1015 commands are static legends, built in full on every Song Position repaint and then rejected at push.** `ri_geo_bbox` excludes legends precisely because they are static — the app never asks to repaint one — so drawing them during a damage repaint is pure waste, and **proving it needs the legend's painted extent, which nothing in the tree records.**
- **THE NEXT CUT, AND WHY IT IS SOUND WHERE THE OBVIOUS ONE IS NOT.** Not a pixel cache — the wiki has refuted that once, correctly, because `RI_STALE_BAR` makes identical rects carry different pixels. **A geometry memo**: the first build of a shape with no declared box records the union extent of the commands that item actually emitted, and later builds cull on it. Sound because a legend's extent is a property of its **geometry**, not of its content, so it cannot go stale the way a pixel cache can — and it is falsifiable by `t169`, which would report a lost command the moment a memo was too small. **The open risk is a legend whose extent depends on its text**, which is exactly the thing to measure first.

## [2026-10-05] ingest | THE REAL BOX: the transport's 55 µs is 97 LED-digit commands, and half the cost is already gone
- Disposition: **New** (`bench_trbar`), **Answered** (what is left in the lane's only hot path), **Refuted** (my legend hypothesis, before it was coded), **Next cut identified, not built**
- `AUDIT 0/0 PASS`. New bench is exempt from the gate with its reason recorded. Host-measured; no new guest run.
- **WHY A NEW BENCH, AND IT IS THE THIRD ONE FOR THE SAME QUESTION.** `bench_build` clips a **synthetic 64×16 box at the section centre** — the right shape for a step lamp and **the wrong box for this lane**, which `bsec` has now pinned to the transport's Song Position display in 16 of 16 windows. Every number `bench_build` produced for the transport was about a box nobody repaints. `bench_trbar` uses the box the app actually asks for: `ri_geo_bbox(g, (RI_SEC_TRANSPORT<<8) | RI_STR_BAR, RI_GEO_ZOOM_COMPACT)` = **`404,38..467,70`, 64 × 33 px, on a 632 × 78 canvas**.
- **MEASURED:** | | commands | time | | | | | | unclipped | **829** (629 rect / 179 line / 21 text) | 9.78 µs (11.8 ns/cmd) | | **clipped to the real box** | **97** (74 rect / 21 line / 2 text) | **4.46 µs** | | | | | | | | **kept 97 of 829 = 11.7 %, and 54.4 % of the cost removed.** **46.0 ns per SURVIVING command**, against 11.8 ns/cmd unclipped — the survivors are gradient-filled primitives, which is what `ri_art_led_digits` emits, not rects-and-lines.
- **SO: after the item cull and the disc-row clamp, THE REST OF THE TRANSPORT'S BUILD IS THE SONG POSITION DIGITS BEING LEGITIMATELY REDRAWN.** 97 commands, every one of them intersecting the damage box by construction (the push clip guarantees it), at 46 ns each. The Dell's 55 µs maps onto this at the ~12× that -O0 GUI code runs at against this host, so **~30 µs of the 55 is digits inside the box and ~25 µs is the cull's residue.** That is the honest floor of the current design, and it is a floor with a specific shape rather than an unattributed remainder.
- **⚠ MY LEGEND HYPOTHESIS WAS WRONG AND `bench_trbar` KILLED IT IN ONE RUN, BEFORE ANY CODE.** I claimed ~900 of ~1015 transport commands were `RI_GEO_LEGEND` entries being rebuilt in full. **A legend is ONE command** — `ri_art_text_c(out, cx, cy, s, col)` is a single push — so ~20 legends cannot account for 937 item commands. The real distribution is ~11 expensive items (knobs, transport keys, LED digit blocks) averaging ~83 commands each. **I inferred a structural cause from an item count and never checked what one item of that shape emits.** The `RI_GEO_LEGEND` entries do still explain part of it — they are the items with no declared damage box, so they are drawn rather than culled — but they are ~20 commands, not ~900. **A proposed cut aimed at a cause that turns out to be 2 % of the cost is a cut aimed at nothing.**
- **THE NEXT CUT, IDENTIFIED AND DELIBERATELY NOT BUILT.** On a bar advance the display changes by **one digit** — `099 → 100` — and the other two are redrawn identically. So: **make the BAR box cover only the digit positions whose rendered text differs**, instead of all three. 97 → ~33 commands, and the ~30 µs of digit repaint would fall to roughly a third. It is sound in principle and it is **not** the pixel cache the wiki refuted, because the *pixels* of the unchanged digits genuinely are unchanged here — the earlier refutation was `RI_STALE_BAR` firing on every advance with *different pixels somewhere*, which is compatible with narrowing to *only* the differing positions.
- **⚠ AND THE SUBTLETY THAT MAKES IT NOT A ONE-LINE, WHICH IS WHY IT IS NOT BUILT YET.** The box is requested at one moment and drawn at another, and the 100 Hz repaint tick is **12× faster than the bar advances** (~0.83 Hz), so the value at request time and the value at draw time can differ, and can differ by more than one digit if a repaint is missed. A box computed from the *current* value alone would be **narrower than the change that actually happened** — the exact class of bug this lane has already produced once, when the bbox was computed at zoom 0 while the canvas sat at `RI_GEO_ZOOM_COMPACT` and the box turned out **disjoint** from the display. **The fix is to remember the last-drawn bar value per canvas and union the differing digit positions between it and the current one**, which is a real state variable with a lifetime question, not an arithmetic one. That needs its own test — a "digits that differ" helper is host-provable and its output pins the box — and it should be built and proved rather than reasoned about at the end of a session.

## [2026-10-05] ingest | THE NARROWED-BOX CUT IS DEAD: the legend face is PROPORTIONAL. Measured, not assumed.
- Disposition: **Refuted** (the cut proposed one entry ago), **New** (`bench_trbar` gate), **Correction** (my own probe's first verdict was an artefact and said so)
- `AUDIT 0/0 PASS`. Host-measured. No guest run.
- **THE COMPOSITION OF THE 97 SURVIVORS, WHICH NOBODY HAD STATED.** `ri_art_led_digits` is **not a 7-segment display**. It is `ri_art_lcd_bg` + a static `"888"` ghost + one centred text string. So the real Song Position draw is **74 rects + 21 lines = the LCD background (one rect per row, plus a black frame and a highlight line), and 2 text commands — the ghost and the value.** The ghost is a literal `"888"` / `"88"` and **never changes**; the value is the only live part.
- **SO THERE IS NOTHING LEFT TO CULL BY CLIPPING, AND THAT IS A REAL RESULT.** `ri_art_lcd_bg` looked like the ideal target — `for (k = y0; k <= y1; k++) ri_draw_rect(...)`, one row per iteration, exactly the 1-px-row shape `ri_art_disc_grad` was clamped on. **But the LCD rectangle IS the damage box** (`ri_geo_bbox(RI_STR_BAR)` = `404,38..467,70`, and the display item is the same rect), so **every one of its rows genuinely intersects the box and every one is legitimately kept.** A row clamp there would remove nothing. **A cut is not available just because the shape fits the pattern that worked elsewhere.**
- **⚠ AND THE CUT PROPOSED ONE ENTRY AGO IS REFUTED BY PRODUCTION'S OWN RECORDED EXTENTS.** The proposal was "narrow the BAR box to the digit positions that changed". That needs the legend face to be **monospaced**, so that the string divides into equal cells. `bench_trbar` now reads the TEXT commands' **own** bbox — the same extents the replay and the clip both use, so this is production's answer and not a re-derivation — at six bar values:
  ```
    "888"  w=15      three digits
    "100"  w=14      three digits
    "120"  w=14      three digits
    "  1"  w=12      two blanks + one digit
    "  4"  w=13      two blanks + one digit     <-- SAME STRUCTURE, 13 vs 12
  ```
  **`"  1"` and `"  4"` have identical structure — 3 characters, 2 leading blanks, 1 digit — and measure 12 and 13 pixels.** No fixed-cell split exists, **so the narrowed box is unsound and the cut is dead.** A space is narrower than a digit, and the face is proportional.
  - **The decisive comparison had to be between strings of IDENTICAL structure**, because only those must agree if the face were fixed-width. Comparing across different strings proves nothing about cells, and that is the trap: my first version of this probe compared the Song Position readout against **whatever text it happened to see first** — the transport's `"0"` legend at `x=7`, 5 px wide — and duly printed a confident `NOT fixed-width` verdict and a `cell 1 px`. **Wrong data, right conclusion, for the wrong reason.** The verdict logic now selects readouts by *shape* (≥2 leading blanks) rather than by position.
  - **This is the same failure the lane produced twice before** — a bbox computed in the wrong coordinate space came out **disjoint** from the Song Position display (`e8b766b`), and `RI_RAW_SPACE` turned out to be `0x40`, not 57. **Verify a control's constant against its header before a measurement depends on it, and read the extent production recorded rather than the one you expected.**
- **⚠ AND A SECOND ARTEFACT IN THE SAME PROBE, WORTH RECORDING BECAUSE IT NEARLY BECAME A FINDING.** `ri_sui_set(&ui, RI_STR_BAR, v)` **does not set the bar.** `gui/secttr.c` routes it around `ri_str_set_value` and reads `ri_seq_bar_display(s->cursor, s->ppq).bar`, so the display showed `"  1"` at every value I set. The six-value sweep therefore varied nothing, and **had the probe not also printed the raw strings, it would have reported "the text does not change with the value" as a finding** — which would have been a statement about my own inability to set the value. **Print the data, not only the verdict.**
- **WHERE THE LANE'S BUILD NOW STANDS, HONESTLY.** Host, against the real box: **829 commands → 97 kept (11.7 %), 10.3 µs → ~4.5 µs, ~50 % of the cost removed**, at **~46–55 ns per surviving command** because the survivors are gradient-filled. The Dell's 55 µs maps here at the ~12× that `-O0` GUI code runs at this host, so **~25 µs is the cull's residue and ~30 µs is the LCD background plus two text commands that are inside the box by construction.**
- **SO THE REMAINING TARGET IS NO LONGER THE BOX — IT IS THE LCD BACKGROUND'S ROWS.** Not because they can be clipped (they cannot, see above) but because **they are static**: `ri_art_lcd_bg` depends only on its rectangle and two colours, never on the bar value, so for a BAR-only repaint **every one of its ~95 commands is redrawn identically.** That is the pixel-cache shape the wiki refuted once — and the refutation still stands for *cache keyed on the rect*, because `RI_STALE_BAR` means identical rects carry different pixels **somewhere**. It does **not** stand for a **split** between the static part and the live part of the same control, which is a different claim with a different falsifier: draw the static part only when the box is not a BAR box, and let `t169` say whether the pixels match. **That is the next cut, and the measurement above is what makes it separable.**

## [2026-10-05] ingest | The static/live split is UNSOUND — Zune passes `MUIM_Draw` no rect, so the damage region is undefined by contract. The LCD background is a floor.
- Disposition: **Refuted** (my own proposal, killed by the platform's contract rather than by shipping a hole), **Answered** (the LCD background is a floor), **Closed** (this line of work)
- `AUDIT 0/0 PASS`. No guest run; nothing changed in the tree for this entry — it is a conclusion, not a cut.
- **THE FACT, FROM THE PLATFORM, NOT FROM REASONING.** `src/abi/v11/AROS/workbench/libs/muimaster/mui_redraw.c:137` reads, in full:
  ```c
  _flags(obj) = (_flags(obj) & ~MADF_DRAWFLAGS) | (flags & MADF_DRAWFLAGS);
  DoMethod(obj, MUIM_Draw, 0);
  ```
  **The message carries a literal `0`.** The invalidation region lives only in Intuition's region list, and the object learns about it by intersecting `_left/_top/_right/_bottom` with `l->ClipRegion->bounds` (lines 118–124). **So a widget is never told *what* changed — only that something did.** The contract is therefore: **`MUIM_Draw` must reconstruct everything the object owns inside the invalidation region.**
- **⚠ WHICH KILLS THE STATIC/LIVE SPLIT, AND IT KILLS MY OWN REASONING.** I proposed drawing the LCD background only when the box is not a BAR box, on the grounds that *"nothing changed the bar, so the background is intact."* **That confuses application state with framebuffer validity.** Intuition does not track application state; it tracks invalidation. **The contents of a damage rect are undefined by contract**, which is the entire reason a damage rect exists — so "it should already be there" is not a fact about this program, it is an assumption about a region the toolkit has explicitly declared unreliable. **Every other cut this session was history-INDEPENDENT** — "the build emits exactly what the replay would have kept" holds whatever the canvas contained before — **and this one is history-DEPENDENT, so `t169` could not have falsified it.** `t169` compares a clipped build against a filtered full build; in the full build the LCD background is always present, so the test would have passed green while the change put holes on screen after any unrelated damage. **A green parity test is not coverage of a claim the test cannot see.**
- **AND THE SOUND VERSION OF THE SAME IDEA DOES NOT HELP, FOR A REASON WORTH WRITING DOWN.** Narrowing the *invalidation* box rather than the build would be sound — everything inside a smaller region still gets redrawn, so the contract holds. But it removes nothing here: **every `ri_art_lcd_bg` row is a full-width rect spanning `x0..x1`, so its bounding box is the whole display and it intersects EVERY possible sub-box of it.** The push-time clip keeps a command whose bbox intersects the box, so no narrowing short of the display's full extent can drop a row. **The LCD background is therefore unreachable by every box-based cut, and the only mechanism that could reach it is the one the contract forbids.**
- **SO: ~30 µs OF THE LANE'S 55 µs BUILD IS A FLOOR UNDER MUI'S DAMAGE MODEL.** Not unattributed, not deferred — **structurally required**, established from the toolkit's own source. On the Dell: `build_avg` 55 µs = **~25 µs of cull residue + ~30 µs of LCD background and two text commands that lie inside the damage box by construction.** The residue is the next legitimate target and it is ordinary work: 829 → 97 commands is 11.7 % kept, and the survivors are gradient-filled at 46–55 ns each.
- **WHAT THE SESSION'S CUTS CAME TO, HONESTLY.** Host, across all 21 sections, clipped build **490 → 128 µs (3.8×)** — and **on this lane's one section, 10.3 → 4.5 µs (~2.3×)**, of which roughly half is now unreachable. **Two of the four cuts (303 strip seek, disc-row clamp) are worth nothing here** because they target SYNTH1 and LEVI, which `bsec` proves are never repainted; the item cull is the one that carried the lane, and `disc_grad` is what made the knob cost O(1) rows instead of O(r²) on the sections that do get painted.
- **METHOD, THE FOURTH ADDITION THIS WEEK, AND THE MOST IMPORTANT.** *Distinguish cuts that are history-independent from cuts that are history-dependent, and never let a parity test stand in for the second kind.* Every cut here was justified by "the replay would have dropped it anyway", which is checkable by comparing two builds and is **independent of what was on the canvas**. The one proposal that was not on that footing — "the static part is still there" — **could not have been falsified by any test in this repo**, and reading twenty lines of Zune was enough to reject it. **Ask what a test would have to be able to see before believing a green one.**

## [2026-10-05] ingest | Decomposing the residue: the floor is 37 % and the survivors 9 %. The hoist is refuted. And my enum table was FABRICATED.
- Disposition: **Answered** (what the lane's 55 µs is made of), **Refuted** (the cull hoist), **⚠ Correction of my own record** (I refuted a correct hypothesis using an invented constant), **New** (ratio-not-microseconds, because this host is bimodal)
- `AUDIT 0/0 PASS`. t93/t112/t169 pass. Host-measured; no guest run; no production change — the hoist was reverted and the tree is where it was.
- **THE DECOMPOSITION, from three builds differing only in the clip box.** | build | clip | what it emits | | | | | | unclipped | none | everything | **9.6 µs** | | **disjoint** | `-9,-9..-1,-1` | the background + **only the items the cull cannot skip** | **3.6 µs = 37 %** | | **the real box** | `404,38..467,70` | that, plus the survivors | **4.5 µs** | | | | | | | **So the clipped build is ~37 % floor, ~9 % survivors, and ~54 % is work the item cull already removes.** The survivors — the LCD background and two text commands inside the box — are **9 %**, which independently confirms the previous entry's claim that they are a floor, and by a *much smaller* number than that entry implied when it said "~30 µs of 55".
- **⚠⚠ MY SHAPE TABLE WAS INVENTED, AND IT INVERTED THE CONCLUSION OF THE PREVIOUS ENTRY.** `bench_trbar` counted the transport's items by shape using a table **I wrote from memory**: `{ "?", "KNOB", "RECT", "OPTION", "STEPPER", "LED", "LEGEND", ... }`. The real enum in `gui/panelgeo.h` is `KNOB 0, RECT 1, LED 2, **LEGEND 3**, DIVIDER 4, **OPTION 5**, STEPPER 6`. So the 5 items with no damage box — which I reported as "OPTIONs" — **are `RI_GEO_LEGEND`.** **The previous entry's "my legend hypothesis was wrong and the bench killed it" is therefore WRONG: the hypothesis was right, and I refuted it with a fabricated constant.** The specific error was reasoning about the *cost* ("a legend is one command, ~20 legends cannot account for 937") **before establishing which items the five actually were** — the cost argument was sound arithmetic and was applied to the wrong population. Counted correctly: **14 KNOB, 3 RECT, 5 LEGEND, 8 OPTION, 1 other; 26 of 31 have a damage box and 5 do not, and the 5 are the legends.**
  - **This is `RI_RAW_SPACE` (0x40, not 57) for the second time in this lane, and the first time it reversed a conclusion rather than merely breaking a measurement.** The standing rule — *verify a control's constant against its header before a measurement depends on it* — was in the wiki and was not applied. **A table of names I am confident about is exactly as much an assumption as a number I am confident about.**
- **THE HOIST IS REFUTED, AND THE MEASUREMENT IS WHY IT HAD TO BE A RATIO.** The obvious target in the 37 % floor was the per-item `ri_ctlreg_find` + `ri_sui_value` that ran **before** the cull, so every item paid them even when the cull was about to reject it. Hoisting the cull above both changes nothing about what it justifies, so it looked free.
  - **First attempt read as a regression** (disjoint 3.62 → 4.50). **⚠ THAT READING WAS AN ARTEFACT: this host's absolute microseconds are BIMODAL by ~25 %** — disjoint is either ~3.6 or ~4.5 and `unclipped` tracks it (9.55–9.68 vs 10.17–10.53), i.e. CPU frequency, not the change. **Comparing absolute µs across runs on this machine is meaningless.**
  - **So the bench now prints a RATIO** — `floor/unclipped` and `survivors/unclipped` — because two builds **in the same process** share the frequency and the ratio is the stable quantity. | | floor/unclipped | | | | pre-hoist (3 runs) | **0.3762, 0.3753, 0.3720** | | post-hoist (4 runs) | 0.4301, 0.3682, 0.3634, 0.4166 | **Pre-hoist is tighter and slightly better. The hoist is reverted.**
  - **And there is a mechanism for why it cannot help:** the old code called `ri_geo_item_box` for every item anyway — it is the cull's own test — so hoisting saves only `ri_ctlreg_find` and `ri_sui_value`, which on a registry this size are evidently near-free, **while making `ri_geo_item_box` run for items that `if (!d) continue;` would have skipped.** Net-neutral at best, and worse for sections carrying undefined items.
- **SO WHAT IS THE 37 % FLOOR, HONESTLY?** Background (78 commands) + 5 legends (5 commands) + the per-item loop overhead for 31 items + the push-time test on what survives. **A 37 % floor for ~83 emitted commands means the floor is dominated by per-ITEM work, not per-command work** — which is the same shape as the `ri_art_disc_grad` finding (cost per *iteration*, not per command) and points at `ri_ctlreg_find` / `ri_sui_value` / `ri_geo_item_box` as the three candidates. **The hoist ruled out the first two as reachable by reordering. So the next question is what those three functions cost, measured directly rather than inferred from a ratio.**
- **METHOD, ADDED: A RATIO INSIDE ONE PROCESS IS THE ONLY STABLE TIMING QUANTITY ON THIS HOST.** Two builds in the same process share the CPU frequency; two runs do not. Every timing comparison in this lane's future should be a within-process ratio or a delta of two builds measured back to back, and **a bench that prints absolute microseconds across runs invites exactly the false regression I read** — which is a measurement instrument lying in the most expensive way available, because it looks like a result.

## [2026-10-05] ingest | the 2026-10-05 GUI/attribution session, compiled into six articles
- Disposition: New; Update; Disputed
- Raw: raw/evidence/2026-10-05-gap-accounting-and-the-item-cull.md; raw/evidence/2026-10-05-which-section-and-why-the-lcd-is-a-floor.md
- Compiled: the gap counter was max where the arithmetic said sum; the item cull that threw away 98 %; which section owns the build and why the lcd is a floor; the knob was o of r squared; a ratio not microseconds this host is bimodal; the lane restored and the bulk port was never open
- Updated: index (GUI §12.10 gained three rows, Lane infrastructure one, Ingested articles three)
- NOTE — 13 ingest log entries existed for 2026-10-05 with **0 articles and 0 index rows**: the session had been logged but never compiled. This pass closes that gap. The log entries remain as the chronological record; the articles are the compiled knowledge.
- **Disputed, retained not rewritten:** the 2026-10-05 entry "my legend hypothesis was wrong and the bench killed it" is **wrong**. The hypothesis was right; it was refuted with a **shape table written from memory** while the real enum is `KNOB 0, RECT 1, LED 2, LEGEND 3, DIVIDER 4, OPTION 5, STEPPER 6`, so the five unboxed items are `RI_GEO_LEGEND`. The error was reasoning about the *cost* (~20 one-command legends cannot account for 937 commands) before establishing *which* items the five were. Both the retraction and the original claim are kept.

## [2026-10-05] ingest | The 37 % floor is 45 % ONE LINEAR SCAN — and both "obvious" fixes are wrong
- Disposition: **New** (the measurement), **Rejected** (two replacements, one of them not fully explained), **New** (`t170`), **Reverted** (no production change — the tree is unchanged)
- `AUDIT 0/0 PASS`. t93/t112/t169/t170 pass. Host-measured; no guest run; **`gui/ctlreg.c` is exactly as it was.**
- **THE 37 % FLOOR IS NAMED, AND IT IS NOT WHAT I EXPECTED.** From the previous entry: a clipped transport build is ~37 % floor, ~9 % survivors, ~54 % removed by the cull. The floor looked like per-*item* overhead, and I guessed the registry lookups. Measured directly, inside one process: **31 × `ri_ctlreg_find()` is 45 % of the section's disjoint-clip build** — ratio **0.4527 / 0.4448 / 0.4519** over three runs, a 1.8 % spread.
- **AND THE COST IS POSITIONAL, WHICH IS THE WHOLE PROBLEM.** `RI_CTLREG_N = 485` and the lookup is a linear scan, so a repaint pays a depth proportional to where a control sits: the transport's 31 items resolve at a **mean depth of 224.2**, i.e. **6949 comparisons per repaint** before any drawing. Measured per-section index ranges: SYNTH1 0..29 (mean depth 16), 808 60..103 (82), 909 104..149 (128), MIX-* 150..181 (154–178), **TRANSPORT and everything after it sit at 190+ (192–215)**. **The controls the lane actually repaints are the most expensive in the table**, purely by position — nothing about the controls themselves is expensive.
- **⚠ ATTACK 1, A GLOBAL BINARY SEARCH: REFUTED, and cheaply.** The ids do not ascend across the table — three inversions, at section boundaries (`sorted.c`: `first inversion at 190: 2048 after 5127`). So a binary search over all 485 entries returns NULL for ids that exist. `t170` pins this by asserting the inversions are **> 0**, phrased so that if a future edit sorts the table the assertion fires and says the attack may now be viable.
- **⚠ ATTACK 2, A PER-SECTION RANGE: INSTALLED, FAILED, REVERTED, AND NOT FULLY EXPLAINED.** Written as literal bounds (no mutable state), 6 comparisons instead of 224. Installed and checked: **`ri_ctlreg_find` returned NULL from `reg_id 4833` upward** where the linear scan resolves an entry. 4833 >> 8 = 18 = LEVI.
  - **What I got wrong twice in one attempt.** First I claimed the table is "grouped by section", then, when the exhaustive test failed, I claimed it is **"NOT grouped by section"** and wrote that into the test header as the reason. **Both were wrong**: every section's entries *are* adjacent — the sections are simply **not in numeric order** (`RI_SEC_MIX_LEVI` sits at indices 182..189, *before* `RI_SEC_PAT_LEVI` at 480..484). My contiguity assertion fired and said so, which is what stopped the wrong explanation from being committed.
  - **What is established:** the failure is **inside** the LEVI run, not at a boundary, so **the per-section ids do not ascend there either**. Why they do not is **not established**, and the change is reverted rather than shipped on a plausible story.
  - **The replacement that needs no ordering assumption at all** is a **direct-mapped index**: `reg_id` is `(section << 8) | idx`, so a table indexed by both needs no sort, no ranges, and no assumption about table order. It costs run-time state and a first-call build, and it is **not started here**.
- **`t170_ctlreg_index`, GATED, EXHAUSTIVE — 65536 ids, not sampled.** It pins: the global binary search is refuted; **every entry is reachable through the shipped lookup by its own `reg_id`**; and `ri_ctlreg_find` matches a **linear scan written out in full in the test** (deliberately not shared with production — a helper both sides call could be wrong in one place and consistent) on **all 65536** possible values. Sampling would have passed both failed attacks: each breaks a *contiguous block* of ids near a section's edge, which is exactly where a sample does not look. **The whole point of the exhaustive form is that the two failures I made were both invisible to sampling.**
- **METHOD, AND THIS IS THE THIRD TIME IN TWO DAYS.** *A cheap optimisation whose premise is about the shape of a data table is a measurement, not a change.* I had "grouped by section" in my head from reading `ctlreg.c`'s layout, and both attacks rested on it. The cost of finding out was one exhaustive test. **The alternative is shipping a lookup that returns NULL for real controls — which on this path means controls stop repainting, which is a visible bug, not a subtle one.** And the deeper lesson: **when an attempt fails, the first explanation that makes it make sense is the one to distrust most.** "The table isn't grouped" made the failure fit in one sentence and was wrong.
- **WHAT IS NOW KNOWN ABOUT THE FLOOR, HONESTLY.** ~37 % of the clipped build, of which **45 % is this one linear scan** — so ~17 % of the whole clipped build, ~9 µs host, and on the Dell roughly 30 µs of the 55 µs `build_avg`. That makes it **the largest single remaining reachable term in the lane's only hot path**, larger than the 9 % of survivors and far larger than anything the cull can reach. **It is not addressed by this entry.**

## [2026-10-05] ingest | WHY both binary searches are wrong: the table ascends by SUB-PANEL, and my probe compared a reg_id against an INDEX
- Disposition: **Answered** (the open question from the previous entry), **Root cause found**, **Reverted again** (no production change), **Method** (the second bad instrument in two days)
- `AUDIT 0/0 PASS`. t170 extended and still passing. **`gui/ctlreg.c` is unchanged.**
- **THE OPEN QUESTION IS CLOSED, AND THE ANSWER IS WORSE FOR BOTH ATTACKS.** Previous entry: "the failure is INSIDE the LEVI run, not at a boundary, so the per-section ids do not ascend there either. **Why is not established.**" Established now, by probing the table directly:

  ```
  table[445] reg_id=4835 sec=18 idx=227  find=NULL
  table[446] reg_id=4822 sec=18 idx=214  find=NULL
  ```
  **The ids ascend within LEVI by SUB-PANEL, and the sub-panels are not in idx order.** So the table is grouped by section, and then by sub-panel inside LEVI, and **neither grouping ascends in `reg_id`.** Every ordering-based replacement is therefore invalid:
  - a **global** binary search — the ids do not ascend across the table (inversions at section boundaries);
  - a **per-section range** binary search — the ids do not ascend inside a section either.
  **The only replacement that needs no assumption at all remains the direct-mapped index**, `reg_id` being `(section << 8) | idx`, which needs no sort, no bounds and no ordering. It costs run-time state and a first-call build, in a codebase with **no lazy-init precedent and no threads on the GUI path** (checked), so it is real work and is not started here.
- **⚠⚠ AND THE MEASUREMENT THAT TOLD ME OTHERWISE WAS MY OWN BUG — THE SECOND IN TWO DAYS, AND THE SAME FAILURE MODE.** The probe that "proved" every section ascends did this:

  ```c
  if (!first && d->reg_id <= prev) asc = 0;
  first = 0; prev = i;          /* <-- prev = i: the INDEX, not the reg_id */
  ```
  **It compared a `reg_id` against an index**, which is meaningless, and so reported `ascending: yes` for all 21 sections. I believed it, wrote the range search on the strength of it, installed it, and had to revert it. **This is the same shape as the geometry-shape enum table that reversed the published legend conclusion: an instrument reporting a property the code does not have, believed because it printed a clean answer.**
  - The difference from the enum incident, and the reason it is worth writing down twice: the enum mistake was **reasoning from a remembered constant**, this one was **an instrument that ran and produced output**. A probe that executes looks like evidence in a way that a recollection does not, **so a probe is where the discipline has to be strongest, not where it can relax.**
  - `t170` now computes ascendingness from `reg_id`, checks **both** refutations (global *and* within-section), and **names the inversion pair** rather than only counting inversions — so if a future edit sorts the table, the assertion fires and says the attack may now be viable instead of the comment quietly becoming wrong.
- **THE MEASUREMENT LESSON, GENERALISED: A PROBE THAT COMPARES TWO DIFFERENT UNITS WILL ALWAYS RETURN A CLEAN ANSWER.** `d->reg_id <= prev` with `prev` an index compiles, runs, and returns a boolean; there is no type error because both are integers. **Nothing in the toolchain, and nothing in the output, distinguishes "ascending by reg_id" from "ascending by index".** Two cheap defences, both now in place: keep the compared quantities adjacent in the source (`prev = d->reg_id` on the same line as the comparison), and **have the test assert the property that would have caught it** — here, the named inversion pair.
- **WHERE THIS LEAVES THE TARGET.** Unchanged and still the largest reachable term: **`ri_ctlreg_find`'s linear scan is 45 % of the section's disjoint-clip build**, the transport's 31 items resolve at a mean depth of 224 of 485, and the controls the lane repaints are the most expensive in the table purely by position. Both order-based attacks are now closed with evidence. **The direct-mapped index is the only remaining approach and it has not been started.**

## [2026-10-05] ingest | BOUNDED SCAN SHIPS: build 55.4 → 45.9 µs on target. The lookup was 45 % of the floor and needed no ordering assumption.
- Disposition: **New** (the cut), **On target**, **Correction** (my own stale-binary reading), **Method** (the route that needed no assumption was the third one)
- `AUDIT 0/0 PASS`. t93/t112/t155/t166/t167/t168/t169/t170 pass. Mixed build 1016368 B, `r12moves=42`, deployed as `RIPP-SCAN`. **`xruns=0`, `wake_max=38 µs`, `overloads=0`, `prio=21`.**
- **THE CUT: `ri_ctlreg_find` scans only the section's own run.** Two literals, `RI_CTLREG_SEC_LO`/`HI`, no run-time state and nothing to initialise. The transport's run is **15 entries** instead of 485, so a lookup costs at most 15 comparisons rather than a mean of 224.
- **⚠ AND IT NEEDED NO ORDERING ASSUMPTION, WHICH IS WHY IT IS THE ONE THAT WORKS.** The previous two entries closed both binary searches with evidence: the ids invert at section boundaries **and inside a section**, LEVI being ordered by sub-panel (`table[445]` = 4835, `table[446]` = 4822). **A bounded *linear* scan does not care about order at all.** Contiguity is the only property it needs, and `t170` verifies that against the table for all 21 sections. **So of three approaches — global binary, per-section binary, per-section bounded linear — the third was correct, and the first two were the ones that sounded clever.** A bounded scan of a verified-contiguous run is the boring answer and it was available from the first measurement; two entries were spent proving the clever ones wrong first.
- **HOST, `bench_trbar`, three runs:** | | before | after | | | | | | unclipped | 9.65 µs | **7.89 µs** | | disjoint-clip floor | 3.62 µs | **2.02 µs** | | floor/unclipped ratio | 0.376 | **0.257** | | **the real box (clipped build)** | **4.51 µs** | **2.79 µs** | and the lookup itself: **31 × `ri_ctlreg_find()` 1.99 µs → 0.065 µs, a 30× cut**, its share of the floor **45 % → 3 %**. **The unclipped build improved too** (9.65 → 7.89), because the lookup is paid on that path as well — which is why the ratio, not the absolute number, is the honest comparison.
- **⚠ MY FIRST READING OF THE RESULT WAS A STALE BINARY AGAIN — THE SIXTH OCCURRENCE.** The first post-change measurement read **1.947–2.017 µs, ratio 44–45 %, i.e. exactly unchanged**, which would have meant the cut does nothing. It was `/tmp/opencode/ratio` linked against the **old `ctlreg.o`** from before the edit: I rebuilt the module and never relinked the probe. Relinked: **0.063–0.069 µs, ratio 3 %.** **The false reading was not implausible — "the optimisation does nothing" is a perfectly ordinary result — which is exactly why it needed an independent check rather than a second run.** Running the same stale binary a second time would have "confirmed" it.
- **ON TARGET, `RIPP-SCAN`, 32 quiet windows:**
  ```
    part_avg 167.0 | build 45.9 | replay 33.1 | blit 68.9 | gap 18.0
    bsec set: [13] | box_bar 163/170/3

    RIPP-DISC (linear scan)   : part_avg 175.9 | build 55.4
  ```
  **build 55.4 → 45.9 µs (1.21×), and replay, blit and gap are unmoved — which is the claim.** Partition invariant: **34 rows, 0 mismatches beyond truncation.** `bsec` still `[13]`, `box_bar` still the Song Position box.
- **⚠ AND THE HOST PREDICTED 1.62× WHILE THE DELL DELIVERED 1.21×, WHICH IS WORTH THE EXPLANATION RATHER THAN A ROUNDED-OFF.** The host cut is arithmetic — 224 comparisons to 15 — so it should transfer. It does not, and the reason is that the host's `-O2` build and the Dell's `-O0` build are bound by **different things**: on the host the scan is branch-bound and the reduction is nearly free, while at `-O0` on a 2010-era mobile chip the 485-entry table is **cold cache** and the win is bounded by memory, not by comparisons. **A comparison-count argument predicts a speedup; a cache-footprint argument predicts less, and only one machine can tell you which applies.** The lesson generalises to every count-based optimisation in this wiki: **`built 829 → 97 kept` and `224 → 15 comparisons` are exact and neither is a time.**
- **WHERE THE BUILD NOW STANDS.** 167 µs per quiet repaint = **69 blit (41 %) + 46 build (28 %) + 33 replay (20 %) + 18 gap (11 %)**. The blit and the replay are both established as not-a-lever, the gap's structural floor is ~2 µs, and the LCD background inside the box is established as a floor. **So ~46 µs of build remains, of which the ~37 %-that-was floor is now ~10 %, and the direct-mapped index is no longer needed to get most of it** — the bounded scan already has it.

## [2026-10-05] ingest | The instrumentation is 8 % of every repaint, one read was a pure duplicate, and this AROS has no cheaper clock
- Disposition: **New** (the fix), **On target**, **Closed question** (is there a cheaper clock — no), **Open** (6 of the 7 remaining reads)
- `AUDIT 0/0 PASS`. t93/t112/t168/t169 pass. Mixed build 1016256 B, `r12moves=42`, `RIPP-CLK`. **`xruns=0`, `wake_max=46 µs`, `overloads=0`, `prio=21`.**
- **THE GAP IS MOSTLY MY OWN INSTRUMENTATION, AND IT IS PAID UNCONDITIONALLY.** The corrected budget left 18 µs "unattributed" with **~14 of it the eight `ReadEClock` calls** this lane added. `draw_frame` sets `timed = 1` whenever the EClock opened, with no gate, so **every partial repaint in the shipped binary pays ~17.6 µs of measurement** — about **8–10 % of a 167 µs repaint**. It is the one term in the budget that exists only because it was put there.
- **⚠ AND IT IS NOT A CHEAP CLOCK; IT IS AN EXPENSIVE ONE, AND THERE IS NO CHEAPER ONE HERE.** `ReadEClock` on AROS x86-64 is not an rdtsc. `src/abi/v11/AROS/arch/all-pc/timer/ticks.c`:
  ```c
  void EClockUpdate(struct TimerBase *TimerBase)
  {
      outb(CH0|ACCESS_LATCH, PIT_CONTROL);   /* Latch the current time value */
      time = ch_read(PIT_CH0);               /* Read out current 16-bit time */
  ```
  **8254 PIT channel 0, by port I/O**, ~2.2 µs per call. The header comment says *"one channel of the PIT for simplicity"* and `tb_eclock_rate = 1193180Hz`.
- **THE CONSULTANT'S QUESTION — "IS THERE A CHEAPER CLOCK?" — IS NOW ANSWERED, AND THE ANSWER IS NO.** Searched the v11 tree and the SDK we build against: **no `ReadNanoseconds`, no exposed TSC counter, no cheaper monotonic API.** AROS *does* use raw `rdtsc` internally (`AROS/rom/graphics/gfxfuncsupport.c`, `__asm__ __volatile__("rdtsc" : "=A" (val))`), but it is not published. **So the available win is fewer reads, not cheaper ones** — which is the opposite of what I assumed when I first asked the question.
- **⚠ AND ONE OF THE EIGHT WAS A PURE DUPLICATE.** The read ending the replay span and the read starting the blit span were **two `ReadEClock` calls of the same instant** — nothing happens between them but the arithmetic:
  ```c
  ReadEClock(&te);  ur = eclock_us(&tr, &te);   /* end of replay */
  ...
  ReadEClock(&tr);                              /* start of blit -- SAME INSTANT */
  ```
  One read now serves both. **7 calls instead of 8, and strictly more accurate**, because two reads of one instant can differ by the PIT's own advance, whereas the shared sample makes the replay and blit spans abut exactly. **The three components still partition the total, which is the invariant `t168` pins.**
- **ON TARGET, `RIPP-CLK`, 47 quiet windows:**
  ```
    part_avg 162.8 | build 43.0 | replay 33.2 | blit 68.9 | gap 16.7
    bsec [13] | box_bar 161/165/3

    RIPP-SCAN (8 reads) : part_avg 167.0 | build 45.9 | gap 18.0
  ```
  **gap 18.0 → 16.7 µs, which is the ~2.2 µs one read predicts, and part_avg 167.0 → 162.8.** Build moves with it (45.9 → 43.0) because the build span also shortened by the sample it shares. Replay and blit unmoved. Partition invariant: **51 rows, 0 mismatches beyond truncation.**
- **⚠ SIX READS REMAIN, ~14 µs, AND DROPPING THEM IS NOT FREE.** The obvious next step is to stop splitting the phases and keep only the total plus the build span: **4 reads, ~9 µs, 5 % of the repaint.** The cost is specific and worth naming: **the on-target partition invariant goes with it**, since two components always sum. That invariant is what caught the `max`-where-`sum` bug on target, and it is now **pinned at the source by `t168`**, so the on-target copy is a redundant confirmation — but it is the copy that caught a live 119 µs phantom, and giving it up for 5 % needs to be a decision, not a tidy-up. **The phases it would drop (replay 33 µs, blit 69 µs) are both already established as not-a-lever and rock-steady, so nothing is lost in what they are telling me — only in what they would tell me next.**
- **THE BUDGET AFTER THIS, HONESTLY.** 163 µs per quiet repaint = **69 blit (42 %) + 43 build (26 %) + 33 replay (20 %) + 17 gap (10 %, ~14 of it my own clock reads)**. Blit and replay are established as not-a-lever, the LCD background inside the box is a floor, the gap's structural floor is ~2 µs. **So of the 163 µs, roughly 14 is instrumentation I chose to pay and roughly 100 is code or pixels that the damage model requires.**

## [2026-10-05] ingest | The panel anomaly does NOT reproduce, and `ri_dlist_set_clip` does not reset `n`
- Disposition: **Unreproduced** (a two-day-old unexplained failure), **New** (a documented API hazard), **Retraction of a hypothesis**
- `AUDIT 0/0 PASS`. Host-measured. No guest run, no production change beyond a comment.
- **THE ANOMALY: `culled 237 commands, the clip alone would keep 23`, FOR `sec=0 item=3 box=143,22..209,88`. IT DOES NOT HAPPEN.** On the current tree, for that exact section and box:
  ```
  unclipped 3009 | real clip keeps 239

     op   bbox-ok   hits_box
    RECT     2628        195
    LINE      362         44
    TEXT       19          0
    expect_count equivalent = 239   (bbox errors: 0)
  ```
  **The shipped clip and the reference predicate agree exactly, 239 = 239.** So the two counts that disagreed have never agreed on this tree, and **the panel cut's justification rests on a failure I cannot reproduce.**
- **⚠ AND `clip == 0 INSIDE ri_art_panel`, WHICH WAS MY EXPLANATION, IS REFUTED BY COUNT.** The hypothesis was that the panel's `dl->clip` read 0, which would have made it emit its full hairline set — and 237 is about twice the 119 a full brushed panel emits, so the arithmetic was suggestive. **Measured: a standalone `ri_art_panel` at SYNTH1's size (732×230) emits 119 commands, not 237.** The hypothesis predicted a specific number and the number is wrong.
- **⚠⚠ BUT THE MEASUREMENT THAT EXPOSED ALL OF THIS FOUND A REAL DEFECT IN THE API.** Setting a clip on a **populated** list appends instead of replacing:
  ```
  SYNTH1 unclipped 3009 | clipped to 143,22..209,88 -> 3248 kept
  ```
  **3248 = 3009 + 239.** `ri_dlist_set_clip` assigns `cx0..cy1` and `clip`, and **never touches `dl->n`.** Every caller in the tree does the right thing — `build_dl` calls `ri_dlist_init` first, and so does `t169` — so **this is a trap for the next caller, not a live bug.** It is documented at the prototype because it is exactly the shape of thing that produces "a count nobody can account for": I hit a 237-vs-23 discrepancy, and the mechanism that produces *unaccountable counts* turns out to be a list built twice and summed. **That is a candidate explanation for the anomaly, and it is NOT established** — `t169` does initialise, so it cannot explain that specific run.
- **THE HONEST STATE OF THE PANEL CUT.** The idea is sound and was measured at a real win (SYNTH1 background 4.76 → 2.07 µs): a panel is one rectangle of horizontal strips and a damage box usually covers a sliver of it. The clip-aware version **failed a parity test for a reason that does not reproduce**, and the two explanations I have since offered for it were both wrong — first "the table is not grouped", then "clip reads 0 inside the panel". **So it should be retried from a clean tree with the failure recorded as unreproduced, and the retry must be judged by `t169` alone.** It is not retried here.
- **WHAT IT IS WORTH, SO THE RETRY CAN BE JUDGED.** The transport's background is **64 horizontal bands** — `ri_art_panel(..., brushed=0)` takes its `bands = h < 64 ? h : 64` path — and that is ~0.9 µs of the host build's 2.02 µs floor, so roughly **13 µs of the Dell's 43 µs `build_avg`, about 8 % of a 163 µs repaint.** Not urgent, not free, and worth doing only once.
- **METHOD, and this is the fourth entry in this shape.** *An unexplained failure is not a specification.* Twice today a plausible mechanism was adopted and then refuted by a number, and the standing temptation in both cases was to explain the anomaly well enough to proceed. **The correct disposition for a failure that will not reproduce is to record it as unreproduced and re-derive the case from the measurement** — the panel cut's case never depended on the anomaly; the anomaly only ever looked like a reason to abandon it.

## [2026-10-05] ingest | THE PANEL ANOMALY IS SOLVED: a LINE's bbox is ±1, and the clamp used the nominal line. build 43.0 → 32.7 µs.
- Disposition: **Resolved** (a two-day-old unexplained failure, fully traced), **New** (the cut, shipped), **On target**, **⚠ Correction** (my "does not reproduce" claim was itself wrong — it was a stale build)
- `AUDIT 0/0 PASS`. t93/t112/t155/t166/t167/t168/t169/t170 pass. Mixed build, `RIPP-PAN`. **`xruns=0`, `wake_max=46 µs`, `overloads=0`, `prio=21`.**
- **⚠⚠ FIRST: MY "IT DOES NOT REPRODUCE" FROM THE PREVIOUS ENTRY WAS WRONG, AND IT WAS A STALE BUILD — THE SAME TRAP, SEVENTH OCCURRENCE.** I probed `SYNTH1 box 143,22..209,88` on the *reverted* tree, saw unclipped 3009 / clip 239 / `expect_count` 239, and concluded the failure could not happen. **But the reverted tree is the tree where it does NOT happen, because the offending code was gone.** Re-implementing the cut reproduced it **immediately**: `culled 237 commands, the clip alone would keep 239`. **The earlier reading of `23` rather than `239` was likewise a stale `art_shared.o`.** So the failure was real, reproducible, and diagnosable from the start, and I twice declared it unreproducible because I tested a tree where it was absent. **"Does not reproduce" and "I reverted it and it went away" are different claims, and only one of them is evidence.**
- **THE BUG, IN ONE LINE OF THE PLATFORM.** `gui/draw/canvas.c`, `ri_dcmd_bbox`:
  ```c
  case RI_D_LINE:
      /* 1-px strokes on both backends; grow one for raster rounding. */
      *x0 = (c->x0 < c->x1 ? c->x0 : c->x1) - 1;
      *y0 = (c->y0 < c->y1 ? c->y0 : c->y1) - 1;
      *x1 = (c->x0 < c->x1 ? c->x1 : c->x0) + 1;
      *y1 = (c->y0 < c->y1 ? c->y1 : c->y0) + 1;
  ```
  **A hairline at nominal `y` is KEPT by the clip whenever `y` is in `[cy0-1, cy1+1]`, not `[cy0, cy1]`.** My clamp used the nominal line, so it dropped the row just above and the row just below the box — **exactly the two commands missing**, at `y=21` and `y=89`, confirmed by dumping both streams side by side.
- **THE RULE, AND IT IS THE GENERAL ONE.** *When trimming a primitive, use the extent the CLIP uses, not the extent the primitive nominally occupies.* They differ by one pixel per side for a line, and by zero for a rect (which is why the `disc_grad` row clamp, on one-pixel **rects**, was right first time and the panel's **line** clamp was wrong). **The 303 strip seek was a third case — strips are rects one pixel tall — and it was wrong for a different reason (the straddling row).** Three trims, three different bugs, all caught by `t169`, and the lesson is that **a trim is only as correct as its model of what the clip considers inside**, which is a property of the *command*, not of the drawing routine that emitted it.
- **HOST, `bench_trbar`:** floor/unclipped **0.257 → 0.194** (1.33× on the floor), the real box **2.79 → 2.63 µs**.
- **ON TARGET, `RIPP-PAN`, 15 quiet windows:**
  ```
    part_avg 149.5 | build 32.7 | replay 32.9 | blit 69.0 | gap 14.1
    bsec [13] | box_bar 152/159/3

    RIPP-CLK : part_avg 162.8 | build 43.0 | gap 16.7
  ```
  **build 43.0 → 32.7 µs (1.31×), part_avg 162.8 → 149.5 µs**, replay and blit unmoved. Partition invariant **17 rows, 0 mismatches beyond truncation.**
- **THE SESSION'S TOTALS, HONESTLY.** The quiet repaint is now **149.5 µs**, from **232.6 µs** when the gap counter was first corrected — **1.56×** — and the composition is **69 blit (46 %) + 33 build (22 %) + 33 replay (22 %) + 14 gap (10 %, ~13 of it my own clock reads)**. **The blit is now 46 % of a repaint and is the only large term that has never been successfully attacked**: direct painting traded it for a layer lock per drawing primitive, and the AROS reason (`do_render_with_gc` branching on `rp->Layer`) means the buffered path pays that once. **So roughly 100 of the remaining 149 µs is established as floors — the blit, the LCD background, and the digits inside the damage box — and ~13 µs is instrumentation I chose to pay.**
- **METHOD, AND THIS IS THE FIFTH ENTRY IN THE "MY INSTRUMENT LIED" SHAPE.** *Distinguish "I reverted it and it went away" from "it does not happen."* The first is a single observation of one tree; the second is a claim about the code. I made the second from the first **twice in one entry**, and the cost was a full day of the panel cut sitting on a shelf as "unexplainable" when `ri_dcmd_bbox` had the answer in a comment. **A failure you cannot reproduce has usually been reproduced on a tree that no longer contains the code.**

## [2026-10-05] ingest | Render-stage close-out: 149 µs is three floors and my own clock. The blit is 46 % and has no caller-side lever.
- Disposition: **Assessment** (the render-stage work is finished as far as the caller can take it), **New** (the blit's cost model), **Open decision** (the remaining clock reads — owner's call)
- `AUDIT 0/0 PASS`. No guest run; **no production change in this entry.** Everything below is either a measurement or a decision.
- **THE FINAL BUDGET, from `RIPP-PAN` (15 quiet windows):**
  ```
    part_avg 149.5 | build 32.7 | replay 32.9 | blit 69.0 | gap 14.1
  ```
  **149.5 µs = 69 blit (46 %) + 33 build (22 %) + 33 replay (22 %) + 14 gap (10 %).** From **232.6 µs** when the gap counter was first corrected: **1.56×**, and `build_avg` **112 → 32.7 µs (3.4×)**, with `xruns=0` and `wake_max=46 µs` throughout.
- **⚠ AND 100 OF THE REMAINING 149 IS ESTABLISHED AS FLOORS, WHICH IS THE ACTUAL RESULT.** Not "we ran out of ideas" — each floor has a mechanism:
  - **the blit, 69 µs (46 %)**: see below.
  - **the replay, 33 µs (22 %)**: 97 surviving commands at 46–55 ns each, gradient-filled, **every one inside the damage box by construction.** The LCD background (`ri_art_lcd_bg`, one rect per row, full width) cannot be dropped by any box-based cut, because a full-width row intersects every sub-box of the display.
  - **the digits**: `ri_art_led_digits` is `lcd_bg` + a static `"888"` ghost + one text string. **The ghost never changes**, and the box cannot exclude it.
  - **the gap, 14 µs**: ~13 of it is **my own seven `ReadEClock` calls**, `timed` being set unconditionally whenever the EClock opened.
- **THE BLIT'S COST MODEL, WHICH IS NEW AND IS WHY IT IS A FLOOR.** 69 µs for 64×33×4 B = **8.4 KB, about 122 MB/s — roughly 10× slower than the pixels justify.** Traced end to end in the AROS tree: `BltBitMapRastPort` → `OBTAIN_HIDD_BM` / `RELEASE_HIDD_BM` → `do_render_with_gc` → **`LockLayerRom(L)`** → `GetRPClipRectangleForLayer` → a walk of `L->ClipRect` → `bitmap_render` → `HIDD_BM_CopyMemBox32`. **And `blt_avg` is constant to within a microsecond across every window all session** (`68.8`, `68.9`, `69.0`), with `blt_min` never resetting — so **the cost is call-path, not pixels, which the consultant's earlier "<3 % of the blit is pixel movement" already implied.** Three attacks have now failed against it: direct painting (net zero — AROS's `do_render_with_gc` branches on `rp->Layer`, so the window path pays `LockLayerRom` **per drawing primitive** while the offscreen path skips it, and the buffered path pays it once), narrowing the box (a full-width row intersects every sub-box), and reducing the blit count (already one per repaint). **Nothing the caller can pass changes any of it.**
- **SO THE ONLY REMAINING LEVER ON THE BLIT IS AN AROS-SIDE CHANGE** — a `COPY`-minterm fast path in `HIDD_BM_CopyMemBox32`, or caching `GetRPClipRectangleForLayer`. That is a **different repository**, on the hot path of every window on the machine, and it is **not a change this lane should make quietly.** It is an owner decision with a named file and a named function.
- **⚠ THE ONE OPEN DECISION, STATED AS A DECISION AND NOT TIDIED AWAY.** Six of the seven `ReadEClock` calls remain, ~13 µs, **9 % of a repaint**. The obvious move is to keep only the total and the build span: **4 reads, ~9 µs, 6 %.** The cost is specific and I will not decide it away: **the on-target partition invariant goes with it**, because two components always sum. That invariant is what caught the `max`-where-`sum` bug on a live 119 µs phantom; it is now pinned at the source by `t168`, so the on-target copy is redundant — **but it is the copy that caught a real one, and trading a standing check for 6 % is a judgement about risk, not about code.**
- **WHAT IS STILL WORTH HAVING, AND WHAT IS NOT.** Worth having: the ~13 µs of instrumentation, if the owner accepts losing the on-target partition invariant. **Not worth having, and now closed:** a display-list cache (hit rate 0 % — `RI_STALE_BAR` means identical rects carry different pixels), a narrowed per-character box (the legend face is proportional: `"  1"` is 12 px and `"  4"` is 13 px), a static/live split (Zune passes `MUIM_Draw` a literal `0`, so the damage region is undefined by contract), and a per-section binary search (the ids invert inside a section too — LEVI is ordered by sub-panel).
- **THE FOUR REFUTED CUTS AND THE TWO INSTRUMENT BUGS, AS THE LANE'S REAL RECORD.** Refuted by measurement: direct-paint partials, the display-list cache, the narrowed box, the static/live split, the cull hoist, and both binary searches. **Bugs in my own instruments that each produced a false conclusion: a `max` where `sum` was documented; a dead clock pair re-timing the replay; a geometry-shape enum written from memory; a probe comparing a `reg_id` against an index; a stale binary read as "the optimisation does nothing"; and a stale build read as "it does not reproduce."** **Five of the six are the same failure — an instrument that reported a property the code did not have, believed because it printed a clean answer — and the sixth was a harness that reported a stale artefact.** That is the honest shape of this session: **the wins were mostly found by distrusting a number, and the losses were all a number believed too early.**

## [2026-10-05] ingest | the bounded scan, the ±1 line bbox, and the render-stage close-out
- Disposition: New; Update
- Raw: raw/evidence/2026-10-05-the-bounded-scan-and-the-plus-one-line.md
- Compiled: the bounded scan that needed no ordering assumption; a line bbox is plus one and that was the anomaly; render-stage close-out three floors and my own clock
- Updated: index (Ingested articles gained three rows)
- Scope: commits `3baaadf`, `869a71f`, `9c9fc87`, `51318c5`, `e9454df` — the five since the previous compile at `dce7a6b`. One evidence record holds all the measurements verbatim, so every literal in the three articles is greppable.
- **Disputed, corrected rather than rewritten:** the previous entry declared the panel anomaly "does not reproduce". It did — on the tree where the offending code had been reverted, and the `23` was a stale object. Re-implementing reproduced it as 237 vs 239 and the cause is `ri_dcmd_bbox`'s ±1 on `RI_D_LINE`. Both the claim and its correction stand.
- **Also recorded, because it is the lane's most transferable finding:** six instrument bugs produced six false conclusions this session, and five were the same shape — an instrument reporting a property the code did not have, believed because it printed a clean answer. The sixth was a harness reporting a stale artefact.

## [2026-10-05] ingest | Box-repaint path CLOSED by owner direction; housekeeping landed; timing is now off by default
- Disposition: **Closed** (the optimisation path, by owner decision), **New** (mixed build in-repo + gated, `ri_build_v11.sh` in-repo, build-hash stamps, diagnostic switch, ENVARC doc), **Correction** (my ~13 µs estimate for the timing was 2× high)
- `AUDIT 0/0 PASS`. `RIAPP-DIAG`/`RIPP-DIAG2`, build 25587fc, `r12moves=193`, mixed, **in-repo `scripts/ri_build_v11.sh`**.
- **THE OWNER CLOSED THE PATH, AND THE ARITHMETIC IS WHY.** 150 µs per Song Position advance at ~9–10 advances/s at 140 BPM is **~1.4 ms of GUI time per second, about 0.15 % of one core** — invisible in use. The remaining ~100 µs is three floors (blit 69, replay/digits 33, timing reads). **Further gain there would not change how the app feels.** Recorded: quiet repaint **232.6 → 149.5 µs (1.56×)**, `build_avg` **112 → 32.7 µs (3.4×)**, `xruns=0`, `wake_max=46 µs` throughout.
- **THE NEXT TARGET IS LATENCY, NOT THE DAMAGE PATH**: click/knob-drag → repaint (histogram + max), the five-tab cycle total and per tab, and how often the 100 ms loop tick arrives late. **The known large items are tab switches (52–107 ms each) and full repaints (`full_avg` ≈ 4.5 ms)** — that is where the felt lag is. **Each of those three numbers gets a self-check before its number is used**, which is the owner's standing rule and the one this session most needed.
- **HOUSEKEEPING, ALL OF IT LANDING, AND THE FIRST ITEM WAS THE ONE THAT INVALIDATED EVERYTHING ELSE.** `scripts/ri_build_aros.sh` built **-O0 throughout** while every number since 2026-10-04 was measured on a **mixed** build (engine/ -O2, app+GUI -O0), so the in-repo script **disagreed with the whole dataset**. It now splits by directory prefix, and the audit gate checks **the split rather than the presence of a flag** — a gate that only looked for "-O0 somewhere" would pass a script that had lost the engine half, which is the regression worth catching. **Both new gates were verified to fire**: routing `engine/` to -O0 fails, and a stray file in `scripts/` fails.
- **`scripts/` COUNT GATE REPLACED BY AN ALLOWLIST.** It was `test "$(ls scripts | wc -l)" = 5`, which made adding `ri_build_v11.sh` — the owner's explicit direction — look like a hygiene violation. **A count cannot say WHICH files belong.** The allowlist also asserts executability.
- **`build_v11.sh` IS IN THE REPO** as `scripts/ri_build_v11.sh`. The harm was measured, not theorised: **three stale-binary readings this session**, two of which were only caught because the delta was implausible — *"nothing outside the repo can be diffed against HEAD, which is the property that would have caught all three."* The v11 path stays separate from `ri_build_aros.sh` on purpose (v1 SDK = rdx base, wrong for the Dell: a "Software Failure!" requester at startup), but **both now build the approved mixed configuration so they cannot disagree about what a logged number means.**
- **BUILD HASH STAMPED INTO BOTH LOG SETS.** The ev log had `build=`; **RIAPP.LOG — the file every on-target number is judged from — did not.** Both now open with `RIAPP LOG build=<sha> diag=<0|1>`, verified on target. "Stale binary" is only a usable explanation if the log says WHICH binary, and this session produced three readings that would have ended immediately with a hash in the file.
- **THE TIMING IS NOW OFF BY DEFAULT, AND IT COST HALF WHAT I SAID IT COST.** `RIAPP_DIAG=1` keeps all six reads and therefore the on-target partition invariant; unset means release builds pay nothing. `GetVar` is used, not `getenv` — **`getenv` pulled `__aros_getbase_StdCIOBase` into the widget TU and broke the `sections` link gate** — and it is declared locally because **the v11 SDK ships no header for it** (`audio_ahi_live.c` has always called it and gets away with it only because that TU is not `-Werror`).
  ```
  release  (diag=0):  part_avg 151.0 us
  diagnostic(diag=1):  part_avg 157.5 us | build 32.9 | gap 21.7
  partition with diag=1:  rows 26 | mismatches beyond truncation 0
  ```
  **So the timing costs 6.5 µs per repaint, 4 % — not the ~13 µs I estimated from 6 reads × 2.2 µs.** The estimate assumed every read costs the PIT's full port-I/O latency; in the repaint path they evidently cost about half that. **Another count-derived time that was not a time**, in the same family as `224 comparisons → 15` and `829 → 97 kept`.
- **`RIAPP_LOG`/`RIAPP_EVLOG` DOCS DEFECT FIXED** with an operator-facing `docs/lane/envarc.md` covering all five variables, and carrying the two facts that cost hours: **`RIAPP_EVLOG=RAM:` or the app blocks on a requester and never shows a window**, and **`RIAPP_AUDIO_NOGOVERNOR` matters because the governor drops the render task to pri −1, below the GUI — which is why `-O0` repaint timings look cheap.**
- **AROS'S BLIT PATH IS NOT TO BE TOUCHED.** An AROS graphics change affects every window and belongs upstream; **PRs #1313 and #1314 are already in review rework, and a third change for 69 µs is not justified.** The in-repo alternative is available and recorded if the blit is ever revisited: **merge all damage boxes for one canvas within a loop pass into one blit**, paying the fixed per-call cost once instead of once per box. **Not started — the repaint path is closed.**
- **EVIDENCE IS IN THE REPO.** `lane/` holds the push/get helpers (they died with `/tmp` eight times); the Dell binary and pulled logs stay in `/tmp` by design, since they are regenerable, but **nothing load-bearing is there any more** — the wiki articles and evidence records carry every number.

## [2026-10-05] ingest | LATENCY, the three numbers the owner feels — and the self-check missed two defects on its first run
- Disposition: **New** (three user-facing metrics), **Two defects in my own instrument, both found by reading the output rather than by the check**, **Answered** (what the felt lag actually is)
- `AUDIT 0/0 PASS`. `RIAPP-LAT3`, build 52fc452, `r12moves=193`, mixed, in-repo `scripts/ri_build_v11.sh`. Gated on `RIAPP_DIAG`, so a release build pays nothing.
- **THREE NUMBERS, EACH WITH A SELF-CHECK, ON THE OWNER'S DIRECTION.** One `RIAPP lat:` line per heartbeat: **(1) input→repaint**, histogram + max + mean; **(2) the five-tab cycle**, total, max, and per-tab mean; **(3) tick lateness**, count, max, four-bucket histogram, accumulated sum. The checks are arithmetic identities printed beside the numbers: **the histogram buckets must sum to the observation count**, and **`drift == latesum`** where drift is `span − ticks × period`, which is *by construction* the sum of every tick's lateness.
- **⚠⚠ AND THE SELF-CHECK MISSED TWO DEFECTS ON ITS FIRST REAL RUN — WHICH IS THE POINT OF DIRECTIVE 6, DEMONSTRATED RATHER THAN ASSERED.** First run, reporting `SELFCHK=ok`:
  ```
  late_max=2805650769 us ... span=2820903567 us drift=2806003567 SELFCHK=ok
  ```
  **`late_max` was 2.8 BILLION microseconds** — 2805 seconds — because the previous-tick stamp is a `static` initialised to zero, so the first interval is measured **from boot**. And **`cyc=0` sat beside five non-zero `per_tab` values**, because `per_tab` held the last cycle's numbers while the cycle counter had been reset, so a window with no complete cycle displayed five stale figures as though they were current.
  - **Both fixes are the same shape as the owner's rule.** Seed the first tick instead of measuring from zero; make `per_tab` an accumulator over complete cycles reported as a mean guarded on `cyc_n > 0`.
  - **AND THE REAL LESSON IS ABOUT THE CHECK, NOT THE FIELDS.** `SELFCHK=ok` was *true* on every quantity it tested: the buckets summed correctly, and the arithmetic was internally consistent. **The lie lived in a field nothing was checking.** A self-check covers the quantities it sums — **it does not cover the ones it does not** — so `drift` was printed for two runs beside a verdict that never compared it to `latesum`, and `per_tab` was printed with no condition on it at all. **After the fix the identity is exact: `drift=205418 latesum=205418 SELFCHK=ok`.** A check that cannot fail is decoration; a check that does not cover a printed field is decoration with a number next to it.
- **⚠ AND ONE MORE SELF-CHECK GAP, THIS TIME CAUGHT BEFORE IT SHIPPED.** `per_tab` was reported from `cyc_tab`, which is *not* reset by `lat_report`, while `cyc_n` is. That asymmetry was visible only because `cyc=0` and non-zero `per_tab` cannot both be true — **which is why the report prints `sumtab` next to `per_tab`**: a reader can see the two disagree without knowing what they mean. Carrying a redundant field purely so a reader can catch a broken one is cheap and it is the only reason this was caught.
- **THE THREE NUMBERS, AS MEASURED ON THE DELL (mixed build).**
  ```
  ticks=162 late=157 late_max=2867 us hist=61/101/0/0 hs=162
  input=300 in_max=2655 us in_avg=40 us ihist=298/2/0/0 is=300
  span=16405418 us expect=16200000 us drift=205418 us latesum=205418 us SELFCHK=ok
  ```
  1. **INPUT → REPAINT: max 2655–3836 µs, mean 40–52 µs.** **A click is on screen in well under 4 ms at worst, which is at the edge of perception and usually invisible.** The owner's judgement is confirmed: the damage path was never the felt lag. *(The one 50 ms sample in an earlier run was a tab switch landing in the same bucket, which is the correct behaviour — a tab switch IS an input.)*
  2. **TICK LATENESS: 157 of 162 ticks are more than 0.5 ms late, max 2867 µs, total 205418 µs of lateness in 16.4 s = 1.25 %.** So the loop runs about **1.25 % behind schedule** — it is *slightly* starved, not starved. **This is the first number on the lane that speaks to scheduling rather than to drawing.**
  3. **THE FIVE-TAB CYCLE: 165489 µs total, per tab 29241 / 31512 / 24398 / 47497 / 32841 µs** (measured in the run before the `per_tab` fix; those are per-switch values written by `tab_switch` itself, so the total is sound). **24–47 ms per tab switch, 165 ms for the cycle.** **That is 40× the worst click latency and it is the felt lag.** It also matches the independent `-O2` record of 52–107 ms per tab.
- **SO THE NEXT CUT IS AIMED, AND IT IS NOT IN THE DRAWING CODE.** A tab switch costs 24–47 ms while a click costs 2.7 ms worst case, so whatever a tab switch does that a click does not — the full repaint of two canvases (`full_avg` ≈ 4.5 ms) plus everything `SetAttrs(MUIA_Group_ActivePage)` drags with it — is where the next measurement goes. **The per-tab spread (24 ms to 47 ms) is itself a clue**: if all five tabs cost the same work, they should cost the same time, and a 2× spread means **the cost is content-dependent, not fixed overhead.**
- **METHOD, THE SIXTH ENTRY IN THE INSTRUMENT SHAPE, AND THE MOST PRODUCTIVE YET.** *A self-check must cover every field it publishes.* The gap counter's partition invariant worked because it summed **all** the phases. The latency check summed the buckets and compared nothing else, so two fields were unverified and both were wrong. **The fix was not more checking — it was making each published number either an identity or a guarded mean**, so there is nothing left to assert.

## [2026-10-05] ingest | latency: the three numbers, and the self-check's two misses
- Disposition: New; Update
- Raw: raw/evidence/2026-10-05-latency-the-three-numbers-and-two-instrument-defects.md
- Compiled: latency the three numbers the owner feels
- Updated: index (Ingested articles gained one row)
- Scope: commits `52fc452` (housekeeping + closed path) and `530f662` (latency). One evidence record carries the metric lines verbatim at each stage — first run, after seeding, after drift became a checked equality — plus the two audit-gate negative controls and the `TAB` event line.
- **Retained, not smoothed:** the latency self-check reported `ok` on its first real run while printing `late_max=2805650769 us` and `cyc=0` beside five non-zero `per_tab` values. Both the failure and the correction are in the article, because the failure is the transferable part: a check that is true on everything it tests can still be printing a lie three fields away.
- **Two open items carried into the article rather than resolved here:** the recorded tab click map is stale (5 of 6 clicks missed) and needs re-deriving from a capture before per-tab cost is measured; and the next cut is aimed at the tab switch, which costs 18.2x a full repaint.

## [2026-10-05] ingest | Review of the latency instrumentation and advisor guidance log
- Disposition: New; Update (reviews the 2026-10-05 latency record; condenses advice given 2026-10-02..05)
- Raw: llm-wiki/raw/articles/2026-10-05-review-latency-instrumentation-and-advice-log.md
- Updated: llm-wiki/index.md (Live app entry list)

## [2026-10-05] ingest | Tab switch: rack plate cached in a bitmap, plus housekeeping
- Disposition: New; Update (resolves the tab-switch target named in the review/advice log; supersedes the stall cap in the clipping/tab-stall record)
- Raw: llm-wiki/raw/articles/2026-10-05-tab-switch-bay-plate-cache-and-housekeeping.md
- Updated: llm-wiki/index.md (Live app entry list)
