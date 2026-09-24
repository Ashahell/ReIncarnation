# 2026-09-24 — Warp-target basis fix: outer- vs content-relative coords

> Source: owner feel report (hyper-responsive + dead reversal) on v2
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Single-variable fix for both v2 feel symptoms; device proof
PENDING owner re-test on a fresh instance.

## Root cause
The warp target added window borders to IDCMP mouse coords. But
IDCMP MouseX/Y are relative to the window's OUTER origin
(screen = LeftEdge + Mouse), while borders offset the
RastPort/content space (where `_left`/`_top` live — measured exact
in m22, untouched). Every warp therefore landed off by (10,25),
and every subsequent event measured from the stale anchor folded
that bias into the accumulator: values ran away within events
("too responsive", fine-tuning impossible) and reversing had to
unclimb the accumulated bias ("back and forth broken"). One basis
error, both symptoms — no other change made.

## What
- `gui/widgets/rknb.mcc.c` SELECTDOWN only: warp target drops the
  `BorderLeft`/`BorderTop` terms, with a comment naming the trap.
  Warp policy (every move), accumulator, fail-soft all unchanged.
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0. Fixed binary
  deployed to RAM: (v2 basis fix).

## Proof pending (owner, fresh instance — close the old window first)
- Normal sensitivity returns (150 px ≈ full range either axis);
  fine positioning possible again.
- Back-and-forth within one press tracks both ways.
- Shift-fine, right-click LEVEL unchanged.

## Files
- env: `gui/widgets/rknb.mcc.c` (two terms deleted + trap comment)
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
