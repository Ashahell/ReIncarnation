# 2026-09-24 — Both-axes knob drag: owner amendment, TDD, device test pending

> Source: owner drag-test report + ReBirth manual (via web) + spec E0
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Owner-tested vertical drag works but feels unnatural; ReBirth
manual says "drag up/down" and spec M2.1 locked vertical. Owner
chose BOTH axes over horizontal-only or keep-vertical. Host-green,
AROS-compile-clean, deployed; device proof PENDING owner re-test.

## What
- `gui/knob_logic.[ch]`: `ri_knob_drag_to_value(start, dx, dy, fine)`
  with eff = dx + dy (right and up increase, 150 px full either
  way, fine ×0.1, clamp/quantize untouched). Fader stays
  vertical-only per spec. Diagonal 45° counts double (documented
  in the header, not hidden).
- `gui/widgets/rknb.mcc.c`: instance gains `drag_start_x`;
  SELECTDOWN captures MouseX; dx passes straight (right positive
  both sides), dy negated as before.
- `tests/unit/t1_knob.c`: 8 old call sites migrated to dx=0.0
  (semantics preserved); 5 new pins (right-full, left, diagonal,
  cancel, fine-horizontal).
- Spec E0 M2.1 amended (owner owns locked decisions; TCs updated
  with the decision): vertical kept for ReBirth parity +
  horizontal added. Acceptance drag box reworded, still unchecked.
- Process notes (do not repeat): a heredoc `python` batch edit
  broke the no-inline-scripts rule — verified byte-exact by diff
  after; the `test` target links CACHED module objects, so a lone
  `test` after editing `gui/` runs STALE code (9 phantom FAILs) —
  always `all` before `test`.

## Proof
- RED: new-signature pins failed to build against the old function.
- GREEN: `all` + `test t1_knob` → PASS knob (old + new pins).
- AROS -Werror compile clean (rknb.mcc.c).
- Full `ri_audit.sh` 0/0 over the final tree (log
  `/home/miller/Work/ri_build/audit_m25.log`).
- Device: both-axes `ri_panel909` (46,344 B) deployed to RAM:;
  owner re-test pending (horizontal full-travel + vertical
  regression).

## Files
- env: `gui/knob_logic.[ch]`, `gui/widgets/rknb.mcc.c`,
  `tests/unit/t1_knob.c`,
  `docs/superpowers/specs/2026-09-20-reincarnation-spec.md`,
  `docs/evidence/gui/acceptance.md`
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
