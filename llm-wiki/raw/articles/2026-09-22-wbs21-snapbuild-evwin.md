# 2026-09-22 — WBS 2.1 snapshot-build + event windowing (playback halves, TDD)

> Source: session evidence (test outputs, mutant runs, audit), compiled by agent
> Collected: 2026-09-22
> Published: 2026-09-22

## Disposition
New. Two TDD micro-cycles (RED watched, mutants caught, GREEN verified,
audit 0/0). Completes the snapshot path to playable: song → events →
per-buffer windows. The swap-soak (TC-2.1.3) and storm (TC-2.1.4) build
on these two functions.

## Cycle 1: `ri_snapshot_build_events` (builder completion)
Composes songsteps + walker into caller storage (fixed 512 B stack
scratch for steps — no heap per Phase-0a, no caller scratch burden);
start_tick always 0 (placement is the cursor's job); NULL opts =
straight. `tests/unit/t21_snapbuild.c` pins the hand-derived 10-event
table + truncation + NULL edges. Mutant (drop last step → count 6 vs
10) caught, reverted.

## Cycle 2: `ri_events_in_window` (playback side, in riseq)
Copies events with `s0 <= sample < s1`, order preserved (caller keeps
lists sorted), up to cap; NULL/empty/degenerate → 0. Pure.
`t21_evwin.c` pins full/mid/single/past-end/degenerate/cap/NULL cases —
notably the half-open end ([100,300) excludes 300). Mutant (closed end
`<=`) caught by exactly the boundary asserts (full 5-vs-4, mid 3-vs-2),
reverted.

## Proof
- Both PASS first correct run; both mutants caught with exit 1.
- Full `ri_audit.sh` 0/0 (Phase 6b runs 7 seq tests now).

## Files (uncommitted)
- `engine/seq/snapbuild.h`, `engine/seq/snapbuild.c`,
  `engine/seq/riseq.h`, `engine/seq/riseq.c`, `tests/unit/t21_snapbuild.c`,
  `tests/unit/t21_evwin.c`, `scripts/ri_audit.sh` (Phase 6b).