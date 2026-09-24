# 2026-09-24 — Grab revision: edge-trigger yanked, warp-every-move instead

> Source: owner feel report (horizontal worse, vertical slightly
> better) + diag5/diag6 runtime numbers
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New (revises m26's edge-triggered design before it was committed
proof — the m26 article stands as the trail). Device proof PENDING
owner re-test.

## What the numbers said
- diag5 exit-mask 0xEF: window/screen/rp/Box all healthy; only the
  border-sanity bit clear.
- diag6 raw bytes: BorderLeft=10, BorderTop=25 (sane titled-window
  values — the bitmask threshold was wrong, not the data),
  screen 1024×768 confirmed.
- Consequence: with sane geometry the edge warp could only fire on
  HUGE drags — exactly the owner's long drags — teleporting the
  pointer hundreds of px back to start mid-drag. Horizontal never
  needed extension, so every yank was pure harm ("worse");
  vertical got range at the price of fighting ("slightly better").

## Revision
- Warp-back-to-start after EVERY mousemove while dragging (not
  just near edges): pointer hovers near the knob, physical motion
  accrues 1:1 into the accumulator, edges become unreachable, no
  teleports (per-event jump = one event's delta, tiny). Fail-soft
  unchanged (no input.device → pure absolute behavior).
- Edge-margin logic + target-margin checks deleted (dead weight
  once warp is unconditional).
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0. Binary
  deployed to RAM: (v2).

## Proof pending (owner, fresh v2 instance)
- Pointer stays near the knob while the KNOB pointer rotates.
- Horizontal full range in ~150 px; vertical reaches max.
- Shift-fine, right-click LEVEL unchanged.

## Files
- env: `gui/widgets/rknb.mcc.c` (warp policy only)
- Dell scratch: diag5/diag6 sources + `diag6.bin` (kept: the
  numbers that convicted the edge trigger)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
