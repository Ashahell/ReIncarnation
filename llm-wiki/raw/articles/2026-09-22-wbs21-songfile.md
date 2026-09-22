# 2026-09-22 — WBS 2.1 file-level song path pin (RBNG→builder→walker, TDD)

> Source: session evidence (test output, mutant run, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Integration pin (hand-derived expectations, mutant-caught, audit
0/0). Proves the real file codec feeds the builder→walker chain —
the file leg the snapshot work depends on.

## What
- `tests/unit/t21_songfile.c`: song built in code (140 BPM/96ppq,
  4 steps) → `rbng_write_song` → `rbng_read_song` (round-trip step
  equality) → `ri_song_to_steps` (count) → `ri_sched_emit_timed`
  (120 BPM map, NULL opts) → 6 events with exact
  (sample,type,value,flags). File IO under `/tmp/ri/run/t21`
  (audit-created, t6 pattern); codec errors assert via err string,
  never crash.
- No production code changed (codec, converter, walker frozen green).
- Audit Phase 6b runs it (5 seq tests now; mkdir t21 alongside).

## Proof
- PASS on first run with zero implementation touch — expected: all
  three units frozen; the test pins their composition across the file
  boundary.
- Load-bearing proven by test-side mutant (drop one ACCENT bit in the
  written song → 6 cascading failures: count, flags, downstream
  positions) with revert verified. The mutant proves the event
  asserts track FILE content, not hardcoded values.
- Full `ri_audit.sh` 0/0.

## Files (uncommitted)
- `tests/unit/t21_songfile.c`, `scripts/ri_audit.sh` (Phase 6b).