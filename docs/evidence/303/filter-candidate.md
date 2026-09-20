# 303 filter candidate — E0, UNVALIDATED

**Status: UNVALIDATED until M2.2 A/B.** This golden proves determinism +
skeleton, NOT parity. No parity claim may cite this row until TC-2.2.x pass.

## Claim

First-light 303 voice implements the spec Appendix B candidate recurrence
verbatim, per-sample, in this exact update order:

```
x  = tanh(DRIVE * (in - k * s2))   // input saturation + resonance feedback tap
s0 += g * (x        - tanh(s0))
s1 += g * (tanh(s0) - tanh(s1))
s2 += g * (tanh(s1) - tanh(s2))
out = s2
```

with `DRIVE = 1.0` [HYPOTHESIS — ledger row to be added at M2.2],
state init all zero, parameters interpolated per-sample (never per-block
stepped), stability clamp `fc < fs/6`.

## Executor decisions (E0, recorded here — all hypotheses, none normative)

| # | Decision | Value | Locked by |
|---|----------|-------|-----------|
| D1 | `g` form under 2x oversampling | Appendix B literal: `g = tan(π·fc/fs)` at the BASE rate, applied per substep (tan via `ri_sin` quotient — no libm `tan` in engine/; audit 0b). CORRECTION 2026-09-20: first reading used the oversampled rate in `g` and measured −0.57 dB at f/fc=1/8 (probe `/tmp/ri/run/task-4`, not committed); base-rate form measures −0.08 dB. Base-rate also keeps `g ≤ tan(π/6)` under the `fc < fs/6` clamp, which is evidently the clamp's design point. | M2.2 TC-2.2.1 |
| D2 | Oversample method | linear-interp input between prev/current sample, two ladder substeps per output sample, emit second substep | M2.2 TC-2.2.1 |
| D3 | Feedback-loop HPF on `k·s2` tap | one-pole HP, 150 Hz (spec §9 E2 default) — IN SCOPE | M2.2 TC-2.2.1 |
| D4 | Post-chain | one-pole HP 44.486 Hz + one-pole HP 24.167 Hz — IN SCOPE | M2.2 TC-2.2.1 |
| D5 | Post allpass 14.008 Hz + notch 7.5164 Hz/BW 4.7 | DEFERRED to M2.2 (skeleton omits them; math fixture isolates the core ladder) | M2.2 |
| D6 | Math-fixture stimulus point | `--math` modes drive the core ladder directly (DC/sine at fixed fc/k), bypassing VCO/envelope/post-chain; isolates the recurrence under test | — (scaffold) |
| D7 | Voice chain around the ladder | VCO saw + 50% square ±1.0f; amp env τ 350 ms (P-01); accent env τ 60 ms, VCA clamp 1.2 (P-02); slide τ runtime field, default 0.040 s (P-03 tension: 40 vs 60 resolved by M2.2 A/B); reso coeff 0..~3.8 (P-04); decay-knob τ 80 ms..4 s (P-06); accent reso boost ~15%; no env retrigger on slide (§8 table) | M2.2 A/B |

All P-01–P-06 numeric values are spec Appendix A hypotheses, repeated here
as implemented defaults — not measurements.

## What this golden proves

- D1: same binary/arch ⇒ identical PCM (double-render SHA-256 equality).
- Skeleton: scheduler → 303 voice → WAV file path executes end to end.
- DC convergence + sine passthrough of the core ladder (gates below).

## What this golden does NOT prove

- Parity with any reference device (no A/B has run; candidate UNVALIDATED).
- Audio quality of the voice chain (musical fixture present but ungated).

## Gates (Task 4)

- DC 0.5 in, fc=1000 Hz, k=0: mean of last 1000 samples within 1e-4 of 0.5.
- Sine 1 kHz, fc=8 kHz, k=0: output RMS within ±0.5 dB of input.
- THD recorded as lagging indicator, NOT gated (candidate unvalidated).
- Event-stream golden pinned from day one; `tools/compare` is the single
  source of truth for event/WAV diffs (exit 1 on any difference).

## Evidence class / confidence

E0 (design decision + implementation candidate). Confidence: LOW.
Source: `tests/golden/303/math-dc.wav`, `math-sine.wav` (+`.sha256`),
`first-light.wav` (+`.sha256`, `+.events`).
Method: `tools/render` one-renderer path, re-verified by `ri_audit.sh`
phase 1 (golden-missing-is-broken + re-render-compare).
