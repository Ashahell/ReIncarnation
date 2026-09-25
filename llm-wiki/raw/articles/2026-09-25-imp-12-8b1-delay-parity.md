# §12.8b1 — Delay Steps/triplet/fb-infinite (parity laws)

**Date:** 2026-09-25 (host lane; devices untouched)
**Scope:** review §3.2 delay parameter set (Steps, triplet, feedback) +
§2.5 fb cap. Pan/return-stereo + routing matrix + sustain-transport stay
open (§12.8b2 / §12.9).
**Disposition:** New (TDD RED-first)

## Production changes

- Steps model: 1–32 steps + triplet flag as the single source
  (straight = beats/4, triplet = beats/3); BEATS table maps to steps
  {2,3,4,6}; direct sync snaps to grid (all in-tree callers exact);
  new IDs STEPS/TRIPLET.
- fb = knob/127 (was ×0.8 cap); 127 = bit-exact infinite sustain.
- Sustain-after-stop is structural (no self-clear; proven by test);
  transport wiring is §12.9.

## TDD record

`t47_fx_delay_parity` (6-FAIL RED → PASS): 32-step echo at exactly
192000, triplet/straight equivalence at 24000, bit-identical infinite
repeats, silence-in sustain at the recirculation point. (One arithmetic
correction mid-RED: 4 steps = 24000, caught before GREEN.)

## Deliberate re-baselines (each root-caused)

- fx-delay + fx-chain only (fb law 0.277→0.347 / 0.239→0.299): same
  lengths, first diffs at the SECOND echo (first is feedback-free —
  mechanism-grade proof), RMS ~1.0, peaks healthy, no NaN.

## Gates

- 6/6 FX tests green. Full `ri_audit.sh` 0/0 — all 14 phases.
