# ReIncarnation llm-wiki — log

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
