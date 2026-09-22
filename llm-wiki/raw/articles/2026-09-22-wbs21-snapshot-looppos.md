# 2026-09-22 — WBS 2.1 snapshot contract + loop cursor (TC-2.1.2 math half, TDD)

> Source: session evidence (test output, mutant run, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. TDD work unit (RED watched, mutant-caught, GREEN verified, audit
0/0). Covers the snapshot data contract + cursor math; the PCM
double-render half of TC-2.1.2 waits on the song→events builder (next).

## What (minimal)
- `RISeqSnapshot` (immutable event list + loop bounds + length;
  caller-owned, pointer-store load = GUI-side safe) + `RiSeqLoadSnapshot`
  + `RiSeqLoopPos` (maps MasterClock into `[loop_start, loop_end)`;
  iteration counts completed wraps; pre-loop/no/degenerate snapshots
  pass through with iter 0).
- `tests/unit/t21_seqloop.c`: hand-built snapshot, loop [9600, 19200):
  passthrough, start, mid, end-1/end wrap edge, second wrap,
  degenerate. All exact integer asserts, deterministic, instant.

## Proof
- RED watched: unknown `RISeqSnapshot` type + implicit `RiSeqLoopPos`
  (feature-missing, not typo).
- Load-bearing proven by mutant (cursor returns raw samples →
  `FAIL wrap2 pos 28800 iter 2`, exit 1) with revert verified.
- GREEN: `PASS t21_seqloop` + `PASS t21_seq` (rerun — header changed)
  + full `ri_audit.sh` 0/0 (Phase 6b now runs both tests).

## Files (uncommitted)
- `engine/seq/riseq.h`, `engine/seq/riseq.c`,
  `tests/unit/t21_seqloop.c`, `scripts/ri_audit.sh` (Phase 6b).