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
