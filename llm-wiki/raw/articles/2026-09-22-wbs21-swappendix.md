# 2026-09-22 — WBS TC-2.1.3 PCM half: swap transparency + the saw-wrap lesson (TDD)

> Source: session evidence (test output, probe runs, mutant runs, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Closes TC-2.1.3's PCM half (event half was the arbiter+soak
record). TDD with two discarded hypotheses on the record.

## What
- `tests/unit/t21_swaprender.c`: 10k-swap soak (alternate A/B every 8
  buffers through one 303 voice) asserting per buffer: active identity
  (`active == nxt`), delivered == window count, every sample finite
  and |x| < 4.0 (VCA clamps 1.2 — generous anti-corruption bound);
  determinism (two full soaks bit-identical); same-song swap
  transparency (no-swap vs swapped renders bit-identical).

## Two hypotheses died to get here (kept on record)
1. **Absolute step click metric** (`max step < 1e-4`): VACUOUS on saw
   waves — every period edge exceeds it by design (measured 0.2 steps
   at 110 Hz wraps). Dropped, not loosened.
2. **Retrigger-equivalence within 1e-6**: unphysical — continuous
   filters never reconverge bit-exactly in finite time (measured
   1.7e-4 residual at sample 576 from the 24 Hz post-HP tail; full
   state reset on retrigger would fix the metric but change the
   synth's sound — a model decision, not a test threshold). Replaced
   by onset-bounded (< 0.1) reasoning folded into transparency.
- Replacement that holds: switch adds no artifacts iff identical
  event streams render identically with and without swap machinery
  (plus delivery/bounded/determinism asserts). The machinery cannot
  corrupt what it cannot distinguish.

## Proof
- `PASS t21_swaprender`; full `ri_audit.sh` 0/0.
- Load-bearing: drop-every-2nd-request arbiter mutant phase-locks
  rounds onto the stale snapshot — caught at the FIRST buffer by the
  identity assert (which an earlier rewrite had silently dropped;
  the mutant proved its necessity and it was restored with a
  never-remove comment).
- Supporting probes (scratch, discarded): per-bin maxstep survey,
  every-sample waveform dump (saw wrap at 436 confirmed legitimate),
  band-decay analysis.

## Files (uncommitted)
- `tests/unit/t21_swaprender.c` (extended), `scripts/ri_audit.sh`
  (already wired).

## Status (2026-09-23)
"already wired" above is corrected: `t21_swaprender` was NOT in the
audit t21 list at 09-22 — `f7b435b` added it (it passes). The test and
the `ri_audit.sh` change are committed now (f7b435b), not uncommitted.