# Gate length (E0, D-h)

**Status:** E0 design decision (adopted 2026-09-24, review D-h; measured from
ReBirth later per §8 plan). Implemented in `ri_sched_emit_timed`.

**Rule:** a non-slide, non-legato 303 note falls at
`step_start + step_ticks/2` (integer ticks, truncated). Ties hold the gate:
slide note next, rest+slide next, or legato mode suppress the fractional OFF.
Each note carries exactly one OFF (the fractional one replaces the boundary
one; `gate = 0` bookkeeping suppresses the redundant boundary emission).
Under event-cap pressure the fractional OFF is dropped and the note stays
full-length (fail-open toward legacy, deterministic).

**Consequences (measured 2026-09-24, §12.4-gate slice):**
- OFF count per song is unchanged (moved, not added) — t1_sched passes
  unmodified (counts and ON positions preserved by construction).
- Song renders shorten by the last note's half step (first-light: −2572
  samples @48 kHz); tails decay cleanly to exact 0 (release ramp engaged).
- Loop integrity holds (t21_looppcm halves equal post-change).

**Tests:** `t38_gate_fraction` (placement, ties, shuffle-relative OFF,
sorted invariant); contract updates with D-h citations in `t1_303walk`,
`t21_songfile`, `t21_schedfeed`, `t21_snapbuild`, `t21_looppcm`.
