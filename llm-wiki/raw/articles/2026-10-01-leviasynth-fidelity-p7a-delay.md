# Leviasynth fidelity P7a: device delay + FX framework (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P7 split + §P7a plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p6c-p6d.md](2026-10-01-leviasynth-fidelity-p6c-p6d.md)
- Commit: `f420cfd`. Not pushed at collection.

## Scope (P7 split; rest to follow)

- P7a (this slice): delay + chain framework. P7b: reverb types + freeze. P7c: pre/post 9-type engines. P7d: BPM-sync wiring (needs the P8 clock — flags stored until then).

## P7a status: done (t138, 16 mutants)

- New `engine/dsp/levi_fx.{h,c}` (own algorithms; all 3 build lists — host, AROS sections + riapp, `build/portable.mk` — or the audit fails the build). Per-device insert at the end of `sum_stereo` (post-vintage, pre-strip): no shared mixer/engine-structure change (route owners, t51 untouched), so no owner question.
- Delay DSP (own 4 types: CLEAN, ANALOG loop-sat + darker loop, TAPE wow-interp + dark loop, PINGPONG cross-stereo): time 1 ms..2 s, feedback clamped 0.95 (no runaway at any corner), wet + loop one-pole tones, dry/wet, exact-dry bypass, denormal flush on every loop read. 2 s stereo lines (768 KB static BSS, explicit clear in init).
- `DM_DELAY` (29, 5 params) folded per-voice, lead (first active) voice drives the device (E0).
- FXDLY panel row rebound as panel-ON/engine-bypass (single truth, no ALGO/SLOT1-style mirror trap); keys `0x0E76..7D` (7 new + FXDLY); rows 158..164; DELAY page all 8 slots live with manual-unit texts.
- E0: bypassed by default (songs bit-identical); BPM flag inert until P8; pingpong crosses with centered dry; matrix spans (time ×2^4x, fb additive clamped, tones ×2^2x, dry/wet additive).
- Proof: audit 0/0 clean worktree, Dell 0 UND + 0 r12, ASan/UBSan clean (fresh objects — stale ASan objects failed first); riaudio: window opens, PLAY inks, host wav peak 0.855, 0 xruns. DELAY page pixels deferred (standing gap, same module family).

## Method findings (portable)

- P4's filter key is inconsistently named (`RI_CTL_LEVI_DLTYPE` engine vs `RI_SLEVI_DTYPE` UI) — a blind rename pass clobbered 9 filter-UI references before neighbor tests caught it. Lesson: mechanical renames near same-prefix families need per-line context verification (the `only` guard that does nothing is worse than no guard — it asserts diligence that isn't there). Repaired + verified zero P4-name diffs.
- SELECTOR rows never reached the generic store path in `sectlevi.c` (only KNOB/SWITCH) — a latent gap back to P6c (PANMODE/GLIDE/VSCALE/VMICRO unsettable via encoders, no test caught it). Fixed by unifying on registry min/max (verified identical for the old special cases) instead of growing the special-case table.
- Test-design laws must respect the math under test: pingpong ≡ clean on dual-mono input (prove crossing asymmetrically); echoes must fall inside the render window (153 ms delay needs >100 ms renders); clamp laws need asymmetric comparators; UI-clamped paths need direct engine-door laws.
- Stale ASan objects fail the same way stale host objects do — rebuild the whole ASan set from current source, not just the file under test.
