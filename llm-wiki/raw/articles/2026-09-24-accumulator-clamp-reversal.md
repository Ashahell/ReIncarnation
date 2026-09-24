# 2026-09-24 — Accumulator clamp: reversal bites at once (TDD)

> Source: owner feel report (motion fine, reversal dead past clamp)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Root-caused (overshoot unwind), pinned, implemented, host-green,
AROS-clean, deployed; device proof PENDING owner re-test.

## Root cause
With the pointer pinned by the grab there is no spatial feedback,
so travel overshoots deep past the value clamp; reversing must
unwind all of it before anything moves. Releasing re-anchors
(acc=0 at current), which is why a fresh click always worked.

## What
- `gui/knob_logic.[ch]`: `ri_knob_clamp_acc(start, adx, ady, fine)`
  scales (dx,dy) toward zero when dx+dy exceeds the travel mapping
  start→[0,127] (×10 span in fine). NULL-safe, eff==0 no-op, NaN
  start falls through unchanged (comparisons false).
- `gui/widgets/rknb.mcc.c`: acc fields go double (LONG truncation
  would strand the clamped extremes by a unit); clamped every move
  before the value call; wrapper takes doubles.
- `tests/unit/t1_knob.c`: 2b section (hi/lo/fine/inside pins with
  exact travels 74.41/75.59/750).
- `docs/autodoc/gui.doc` + audit `check_sig` track the new symbol.
- Process notes: stale-object trap BOTH lanes — host `test` links
  cached module objects (`all` first), and the AROS lane link used
  a cached `knob.o` (undefined `ri_knob_clamp_acc` at link —
  rebuild deps, relink).

## Proof
- RED (implicit decl) → GREEN (`all` + `test t1_knob` PASS knob).
- AROS -Werror compile clean. Full `ri_audit.sh` 0/0. Binary
  (51,648 B) deployed to RAM:.

## Proof pending (owner, fresh instance — close the old window first)
- Drag past an extreme, reverse inside one press: value follows
  back immediately (no dead zone).
- Normal travel, fine, right-click unchanged.

## Files
- env: `gui/knob_logic.[ch]`, `gui/widgets/rknb.mcc.c`,
  `tests/unit/t1_knob.c`, `docs/autodoc/gui.doc`,
  `scripts/ri_audit.sh`
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
