# 2026-09-24 — Pointer grab: vertical edge-clamp diagnosed, warp-back implemented

> Source: owner device report (horizontal fine, vertical needs
> beyond-window motion) + AROS input docs + spike agent source
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Owner-tested asymmetry explained by screen geometry, fixed
with the spec-mandated pointer grab. Host-green, AROS-clean,
deployed; device proof PENDING owner re-test.

## Root cause (no code run — geometry is the evidence)
Drag math reads ABSOLUTE pointer position. Knob centers sit at
screen y≈66; full up-travel needs 75 px but the pointer clamps at
the screen edge after 66. Horizontal has 1024 px of room, so it
felt fine. The spec ALREADY requires "pointer grab so drags
continue outside the window" (M2.1) — it was never implemented.

## What
- `gui/widgets/rknb.mcc.c`: drag state becomes an accumulator
  (`acc_dx/acc_dy` from per-move deltas; value derives from it,
  never from absolute position). input.device opens once per class
  (fail-soft NULL = old edge-limited behavior). Within 8 px of any
  screen edge mid-drag, the pointer warps back to the drag-start
  screen point (`IECLASS_NEWPOINTERPOS`/`IND_ADDEVENT` — same call
  the spike agent uses) and the anchor re-seeds there, so the
  warp's own mousemove contributes zero. Warp skipped when the
  start itself hugs an edge (no warp loop). Open/close paired in
  class create/dispose.
- No host-logic change (mapping already pinned in m25); no new
  pure logic, so no new host test — same standing rule as all MCC
  shells. Device proof is the gate.
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0 over the
  final tree. Binary (50,816 B) deployed to RAM:.

## Proof pending (owner, on the fresh instance — close BOTH
## stacked RI-909 windows first, then I launch one)
- Up-drag from mid-knob reaches max WITHOUT leaving the screen
  area feeling (pointer visibly snaps back mid-drag, value keeps
  climbing — the snap-back IS the grab working).
- Down/left/right unchanged; Shift-fine unchanged; right-click
  LEVEL still jumps to ~78°.

## Files
- env: `gui/widgets/rknb.mcc.c` (grab only)
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
