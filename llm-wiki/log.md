# ReIncarnation llm-wiki — log

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
