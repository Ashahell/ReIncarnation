# 2026-09-22 — WBS 2.1 converter→walker feed pin (integration, TDD)

> Source: session evidence (test output, mutant run, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Integration pin (hand-derived expectations, mutant-caught, audit
0/0). Proves the builder output drives the Task-7 walker correctly —
the composition contract both snapshot work and TC-2.1.2-PCM depend on.

## What
- `tests/unit/t21_schedfeed.c`: hand-built 5-step song → converter →
  `ri_sched_emit_timed` (NULL opts = straight, 120 BPM map, device 0)
  → assert count (10) + all 60 fields (sample/type/value/flags/dev/
  voice/seq). Expectations derived from reading the walker contract,
  never from running it — including the subtle bits: NULL opts still
  emits a zero-offset FLAM event; same-sample triple sorts to
  NOTE_ON,ACCENT,FLAM while KEEPING emit seq (6,8,7), which the test
  pins as the §8-sort proof.
- No production code changed (converter + walker frozen green).
- Audit Phase 6b runs it (4 seq tests now).

## Proof
- PASS on first run with zero implementation touch — expected: both
  units frozen; the test pins their composition.
- Load-bearing proven by mutant (mask REST bit in converter → rest
  becomes a note step → 16 failures, exit 1) with revert verified.
  (Mutant intent was ACCENT; the mask hit REST instead — caught
  anyway, recorded honestly.)
- Full `ri_audit.sh` 0/0.

## Files (uncommitted)
- `tests/unit/t21_schedfeed.c`, `scripts/ri_audit.sh` (Phase 6b).