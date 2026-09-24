# 2026-09-24 — Commit-on-release observable: per-knob undo counter

> Source: session work (TDD helper + MCC attr + app wiring + device)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. The last listener-less 2.9 behavior is now observable: every
completed gesture fires exactly one commit event into a visible
counter.

## What
- `gui/panels.[ch]`: `ri_ctl_format_count(buf, n)` (arbitrary counts
  to 65535, saturates, NULL-safe) — pinned in t29_paneldefault
  (0/7/12345 exact, 999999→65535); RED was implicit-decl.
- `gui/widgets/rknb.h` (new, AROS-only): class API +
  `MUIA_RKnB_Commits` (`TAG_USER+0x524B`, read-only LONG).
- `gui/widgets/rknb.mcc.c`: instance `commits`; OM_GET serves the
  attr; SELECTUP bumps it through OM_SET on release-commit so MUI
  fires notifies. Counting rule itself was already pinned (t1
  gesture tests); the attr/plumbing is AROS-only by nature.
- `app/panel909.c`: UNDO readout row + per-knob commit notifies +
  loop accumulation. Uses rknb.h (manual externs retired).
- Audit Phase 12 now gates rknb.h (AROS-only + out of host build).
- Two self-caught file damages repaired pre-build by re-reading:
  a helper insert ate `ri_ctl_format_value`'s signature (orphaned
  body spotted in readback), and a layout edit dropped the
  `if (!win) return 8` guard (restored). Readback discipline pays.
- AROS -Werror clean (both TUs). Full `ri_audit.sh` 0/0. Binary
  deployed to RAM:.

## Proof (device, detached runs)
- Fresh window: UNDO 0 alongside the 64s.
- Bare click (no move) on a knob: UNDO 1 (begin+end = one unit).
- Drag-release: +1 each; three gestures → UNDO 3.
- No guru; old instance closed via gadget first.

## Files
- env: `gui/panels.[ch]`, `tests/unit/t29_paneldefault.c`,
  `gui/widgets/rknb.h` (new), `gui/widgets/rknb.mcc.c`,
  `app/panel909.c`, `scripts/ri_audit.sh`
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
