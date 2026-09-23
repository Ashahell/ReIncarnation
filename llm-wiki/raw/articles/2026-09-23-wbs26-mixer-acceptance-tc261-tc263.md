# 2026-09-23 — WBS 2.6 mixer acceptance: TC-2.6.1–2.6.3 pins (no production change)

> Source: session evidence (test output ×4, audit 0/0 ×2, golden re-render compare), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. Gap analysis of TC-2.6.1–2.6.3 against landed Task 11 code
(`mixer.c`, `t1_mixer`, mixer Phase) found no unwired surface (the
mixer owns its setters; sends share the fader law by construction).
The slice is three per-TC contract pins (`t26_gainstage/matrix/
meter`, wired in the mixer audit phase after `t1_mixer`) + ledger
lock. All three went green first run (property pins, t22
precedent) — no production code written.

## What
- `t26_gainstage` (TC-2.6.1): 9 fader-law anchors ±0.5 dB with
  exact ends (v=0 → 0, v=127 → 1.0) + send-law-equals-fader-law
  rendered ratios at sends 16/64/112 (same fader/trajectory, so
  the ratio isolates the send law — the "one law, three knob
  kinds" contract).
- `t26_matrix` (TC-2.6.2): exhaustive RENDERED 16×16 mute/solo
  matrix (distinct DCs 1/2/3/4, 512-sample settle, tail == audible
  sum within 1e-4 — beyond t1's predicate table + 2 tails) +
  live solo-toggle transients both directions bounded by 9/64
  (three simultaneous per-bus 1/64 slews — the ledger's single-bus
  rule generalized, derived not fitted; an absolute 0.02 bound
  would have been unachievable by construction and was corrected
  pre-run).
- `t26_meter` (TC-2.6.3): 20 dB/s ±2 (measured 20.00, matches the
  ledger's 0.0999454 to 7 figs) + sr-fallback identical + hotter
  peak adopted exactly. Ballistics feel stays GUI-milestone
  subjective sign-off — stated, not waved.
- `docs/evidence/mixer/engine.md` → LOCKED at TC-2.6.1/2.6.2/2.6.3
  with t26 citations.
- Mixer audit phase: three `test t26_*` lines after the `t1_mixer`
  line, TC-tagged FAIL echoes.

## Proof
- TDD: pins written first; all green on frozen code (property
  pins). No RED-to-GREEN production cycle exists in this slice —
  stated plainly (contrast 2.3/2.4/2.9, where pins demanded code).
- Full `ri_audit.sh` 0/0 TWICE (pre-change baseline + final tree,
  0 FAIL lines); mixer goldens re-render identical (no production
  change, nothing to regen).
- Audit-grep strings (`P-17`, `(v/127)`, `P-16`, E0 fader record,
  RED-evidence FAIL lines) live in other files, untouched —
  verified by the green final run.

## Files
- tests: `tests/unit/t26_gainstage.c` `t26_matrix.c` `t26_meter.c`
  (new; no `engine/` file touched)
- env: `scripts/ri_audit.sh` (mixer phase t26 lines)
- ledgers: `docs/evidence/mixer/engine.md` → LOCKED
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

No external websites consulted: contracts are WBS TC-2.6.1–2.6.3 +
spec §13 + ledger P-16/P-17 — all local and authoritative; nothing
was open that needed the web. spirv-val vacuous (no SPIR-V in this
repo — noted honestly per standing).
