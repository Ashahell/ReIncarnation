# §12.4c — Log-domain glide: RC on pitch CV (τ untouched, OPEN-01 safe)

**Date:** 2026-09-24 (host lane; devices untouched)
**Scope:** review §4.1 slide (last non-measurement item of §12.4; osc/filter/
taper stay held for measurement per the review).
**Disposition:** New (TDD RED-first; curve change, same time constant)

## Design

Slide slews `logfreq` (log2 Hz) toward `log_target` with the unchanged
`slide_a` (τ = 40 ms default — OPEN-01 untouched, only the domain moves):
constant octave rate, glide time independent of interval direction in pitch
space. Voice carries the invariant `freq == pow2(logfreq)` while gliding;
settled notes skip the conversion (exactness guard — `pow2(log2(x))` would
add a rounding ulp to every sustained note). `log_target` refreshes in
`note`/`slide_to`; the tune bend recomputes both logs (params.c).

## TDD record

- `ri_log2` kernel first: link-RED → weak-Taylor runtime RED (298 FAILs at
  u→1) → atanh-series GREEN (exact powers, ±1e-6 vs libm on 0.1–64,
  total: −Inf ≤ 0, NaN/Inf propagate, denormals via exact 2^25 step).
  t3 extended; t2 green throughout.
- `t40_logslide`: up 55→440 and down 440→55 hit the log-domain τ-points
  (204.65/118.16 ±2%) — RED showed the linear 63% points (298.37/196.63).
  Reach bound corrected 5τ→7τ (3 octaves need e^−7 against a small target).
- t22_303slide passes UNCHANGED (same τ, same reach shape).

## Catches (process)

- Stray `}` from a slide_to edit broke the build — read, don't assume.
- t37 segfaulted post-change: stale `engine.o` (struct grew; `test` doesn't
  rebuild dependents). Fresh `all` → PASS. The standing all-before-test
  rule, violated and paid for — second time this program the suite caught
  a build-hygiene fault rather than a code fault.

## Deliberate re-baselines (each root-caused)

- first-light set: first diff at sample 20581 = exactly step 4, the song's
  first slide; same length; events identical; peaks/tails healthy.
- sched-check.wav: events 12/12 identical; first diff in a slide region;
  `compare` IDENTICAL on the new pair.

## Gates

- Full `ri_audit.sh` 0/0 — all 14 phases (`ri_build/audit_124k.log`).
- Owner A/B flagged (cumulative with 12.4b: sustain + glide both reshaped).
