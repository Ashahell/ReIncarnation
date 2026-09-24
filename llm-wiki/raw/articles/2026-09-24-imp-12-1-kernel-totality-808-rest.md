# §12.1 — Kernel totality + 808 voice rest + linear section sum

**Date:** 2026-09-24 (host lane; Dell/QEMU untouched)
**Scope:** review §2.1 (CRITICAL exp overflow), §2.3 (voices never die), §2.4 (clip step);
spec D-j adopted first (`docs/superpowers/specs/2026-09-20-reincarnation-spec.md` §20 + OPEN-10 + §2.3 item 6 + parity rows).
**Disposition:** New (first improvement-review slice; TDD throughout)

## TDD record (all RED watched before GREEN)

| Test | File | RED | GREEN |
|------|------|-----|-------|
| kernel totality | `tests/property/t3_kernels_total.c` | 731 FAIL (exp(−100)=−60.1-class garbage, sin(500)=−7.8e15, pow2(100)=2³², sin(FLT_MAX)=−Inf) | PASS |
| long silence | `tests/unit/t33_808_silence.c` | 253 FAIL (tails ramp to full-scale) | PASS |
| storm linearity | `tests/unit/t34_808_storm_linear.c` | 1246 FAIL (mix≠solo-sums where \|m\|>1) | PASS |
| voice rest | `tests/unit/t35_808_deactivate.c` | 395 FAIL (`active` never clears) | PASS |
| prior contract | `tests/property/t2_kernels.c` | — (never red) | PASS throughout |

Two test-side corrections during RED (documented, not hidden): t3's finite-sweep
wrongly covered the overflow zone (89/90 correctly return +Inf — bounds fixed);
t34's clip-region guard moved from mix-peak to sum-peak (mix is clipped by
construction); t35's death window narrowed 4096→last-64 with a 1.5e-5 bound
(slow decays legitimately span 13× threshold across 4096 samples; one-window
growth is the rigorous bound).

## Production changes

- `engine/dsp/kernels.{c,h}`: total kernels (D-j). exp: 0 ≤ −87, +Inf ≥ 88.72283,
  k to ±128. sin: int64 reduction to |x|<5.7e19, bounded 0 beyond. pow2: 0/Inf
  at −127/+128. In-domain paths textually unchanged → 303 first-light re-renders
  byte-identical (verified by direct cmp, then audit Phase 1).
- `engine/dsp/rb808.{c,h}`: voices deactivate past −100 dBFS (`RI_808_REST_LEVEL`,
  clap on tail level, transients gated by t minimums); section sum linear
  (clip branch deleted; ReBirth manual p. 23 — clipping only at int conversion).
- 909's milder clip kept for its own slice (§12.6) — one thing at a time.

## Regression the suite caught (real, fixed)

Inflating `ri_scale2` to 129 iterations cost **2.4× storm CPU** (t1_808 0.149→0.351,
t21 0.46→1.30, all budgets RED vs detached-HEAD baseline). Fixed by keeping the
historical 32-iteration loop for |k|≤32 (wide bound totality-only): bit-identical
and speed-identical on the common path. Lesson recorded in the todo file.

## Deliberate 808 golden re-baseline (each diff root-caused, tail-only)

| Golden | First diff | Cause |
|--------|-----------|-------|
| bd/lt/mt/ht | 0.75–0.98 s | pitch-sweep exp now exact past t≈22τ (old clamp put wrong audible pitch in tail) |
| oh | 1.13 s of 2.0 s | bleed exp now exact 0 (old clamp put audible hash in tail) |
| storm | byte 45 (first sample) | linear sum (clip removed) |
| sd/lc/mc/hc/rs/cl/cp/ch/cy/cb | — | byte-identical (no unintended blast radius) |

Ledger notes appended to the five voice rows; storm recorded here (no storm ledger
required). New `.wav.sha256` sidecars generated from the audit-built renderer.

## Held (deviations with justification, not omissions)

- Multiplicative envelopes: deactivation bounds t < ~50 s always (max tau 4.0 →
  death at 46 s; float t exact there); absolute-time total exp cannot overflow;
  per-sample cost goes to the §12.11 bench with numbers.
- Integer sample-count time: subsumed by the same bound.
- Filter-state conjunction in the rest rule: level-bound proven output-safe
  (extra SD/OH terms decay faster than env by construction); frozen states reset
  at trigger and cost nothing while inactive.

## Gates

- Full host suite: only the 3 budget tests failed, all fixed (see above); rest green.
- `ri_audit.sh` 0/0 post-re-baseline (log `ri_build/audit_121final.log`).
- spirv-val vacuous (no shader content). AROS lanes untouched (host math only).
