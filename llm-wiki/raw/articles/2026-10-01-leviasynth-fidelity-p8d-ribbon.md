# Leviasynth fidelity P8d: ribbon (owner 2026-10-01)

- Source: ReIncarnation session, 2026-10-01 (opencode lane)
- Collected: 2026-10-01
- Published: 2026-10-01
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (§P8d plan + status, E0 ledger)
- Prior: [2026-10-01-leviasynth-fidelity-p8c-seq.md](2026-10-01-leviasynth-fidelity-p8c-seq.md)
- Commit: P8d (this session; unpushed at collection).

## Scope (P8 split; rest to follow)

- P8d (this slice): ribbon state/sources/theremin/triggers/step-select. P8e: step-LFO editor. No live-input path exists (app touch wiring later).

## P8d status: done (t144, 20 mutants)

- Ribbon state (`pos`/`touch`/`mode`/`last`) + `touch/move/release` API + keys `0x0EBC..BE` + rows 211..213 + `M_RIBBON` 36 (appended) with 1 page + per-type texts + TOUCH press toggle.
- Sources per block into voice copies (ABS bipolar, ABS+ unipolar, REL consumed; off zeroes); theremin retunes actives; touch/release drive menv 7/8; touch jumps seq grid.
- E0: off default; POS never implies touch; no producer yet.
- Proof: audit 0/0; riaudio (restarted post-reboot): window opens, SPACE→PLAY, first PLAY 9 xruns (cold transient), steady 4143 buffers 0 xruns. Deviations: no host wav; RIBBON pixels deferred; 909 pack missing.
- Adjacent: none (no missing-TU link this time).

## Method findings (portable)

- Twin-set audible laws, always: consecutive renders differ by time evolution alone (proven: no-change renders differ). Compare same-age twins, never before/after.
- Rebuilt the mutant harness from scratch after `/tmp` wipe (`set -u`, module map, KILLED/SURVIVED/BUILD-FAIL, restore+rebuild). Verify harness behavior with a deliberate syntax probe — a silently-failing harness is worse than none.
- Lane restart after reboot: priv spool server had no pairs file (guest is anonymous — serve with `--spool` only); QEMU HDA warns on `adc` (harmless, playback-only wav backend path); first post-boot PLAY xruns are cold-driver transient, steady runs are the proof number.
- Vacuous-law triage before mutating: the RBNABS-audible law passed with the source forced to zero (time drift). Fix the law first (twins), then it kills.
- Strengthen-then-kill is the rhythm: 3 laws were added purely because a mutant survived honestly (mode-clamp range ends only tested at index 0; SERIES text; touch-key path).
