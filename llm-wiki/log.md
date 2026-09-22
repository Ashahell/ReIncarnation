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
