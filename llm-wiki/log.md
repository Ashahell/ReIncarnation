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
