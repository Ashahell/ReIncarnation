# §12.8c1 — PCF envelope + integer clock + HP drop (no patterns yet)

**Date:** 2026-09-25 (host lane; devices untouched)
**Scope:** review §4.4 (envelope shape, clock, HP) minus the 54 patterns
(Appendix-D extraction stays open) + §2.7 (clock precision).
**Disposition:** New (TDD RED-first with per-family revert-checks)

## Production changes

- Envelope: neutral hit every 16th at velocity 64 (instant attack E0),
  Decay knob → tau 0.05·2^((v−64)/16) s; fc law continuous in env
  (`base·2^(amt·env/64)`); new ID PCF_DECAY + wrapper echo/apply.
- Clock: integer `pos_smp` + pure-function step index (exact at 1e9
  samples ≈ 5.8 h); `pcf_restart` (deterministic: SVF + clock + env);
  tempo clamps widened to D-e 20–500.
- HP mode dropped (→ band); t1_fx clock assert moved to new state.

## TDD record

`t48_pcf_envelope` (compile/link RED → PASS): decay collapse/sustain
bounds, long-horizon exact step, restart determinism, HP==band
bit-exact, cross-render retrigger. Revert-checks on decay/clock/HP
(all FAIL → restore → PASS). One void check caught (stale objects
masked a break — rebuild discipline).

## Golden fallout: NONE predicted

- All fixtures run Amt≈0/mode 0: envelope is a proven no-op there
  (fc = base every sample). Pending full-gate confirmation.

## Gates

- 7/7 FX tests green. Full `ri_audit.sh` 0/0 — all 14 phases, zero
  golden fallout (fixtures run Amt≈0/mode 0 as predicted).
