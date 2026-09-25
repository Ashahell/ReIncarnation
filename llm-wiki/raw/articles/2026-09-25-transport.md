# §12.9a transport state machine (m65)

**Date:** 2026-09-25 (host lane; devices untouched)
**Scope:** transport states/transitions, bar mapping, loop model,
record/display state. OUT: song content, emit, automation, GUI.
**Disposition:** New (plan docs/superpowers/plans/2026-09-25-transport-implementation.md; TDD t58)

## What landed

- `engine/seq/transport.{h,c}` (stdint-only): RITransport state +
  clicks, stop-click law, record matrix, bar helpers, clamped seeks,
  one-way display, loop clamp + next-bar staging.
- `RISeq` owns transport/loop/staging/cursor_ticks (init neutral).
- t58: full matrix + law probes + property loops + layer guard +
  reentrancy.

## Laws (normative, from the reviewed plan)

Ownership / cursor-ticks / geometry (0-based, end boundary invalid,
999 ceiling) / ppq normalization / unsigned-only integer safety /
stop-click law / seek-to-last-valid-bar / loop canonical forms. Full
text in the plan's Transport laws section.

## Mutation proof

- (a) play clicks=1 → t58:24/25 FAIL (play clicks, law).
- (b) riseq.h in transport.h → compile break (include cycle,
  incomplete types) — layering enforced by the compiler itself.
- (c) max_bar=song_bars → t58:83/88 FAIL (clamps end, at-end holds).

## Gates

- t58 RED (missing header, then behavioral per task) → GREEN.
- Full `ri_audit.sh` 0/0 (baseline 0/0 before).

## Plan deviations (2, both recorded)

- ppq1 pin: plan said (2,1,1); correct value is (1,2,1) (raw ppq=1
  would say bar 25 — the pin proves normalization either way).
- Case-8-style compat: n/a (no emit in this slice).
