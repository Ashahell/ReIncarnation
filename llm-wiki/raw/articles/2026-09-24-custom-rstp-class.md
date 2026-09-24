# 2026-09-24 — Custom RStp class: stock Numericbutton can't size, build our own

> Source: session work (device-measured FixWidth refusal + custom class)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. The m36 FixWidth fix was refuted on device (window stayed
268 px — stock MUIC_Numericbutton ignores Fix sizes exactly like
knob.mui). Correct fix: custom class, which 2.10-full needs anyway.

## What
- `gui/widgets/rstp.mcc.c` rewritten (was a MUIC_Numericbutton
  skin): Numeric subclass, 32 px AskMinMax, own Draw (off dark
  fill / on warm red + near-black 1 px edge, via proven
  `ri_knob_panel_rect`), armed click-toggle on SELECTUP-inside
  (press-then-release-inside; drag-off disarms), value notify
  inherited. m21/m22 lessons applied from the start (_win target,
  unconditional Draw, no Draw reentry... except OM_GET readback
  which terminates in super with no cycle).
- `gui/widgets/rstp.h` (new, AROS-only): class API, audited.
- `app/stepproof.c`: builds via the shell (construction point
  kept), FixWidth SetAttrs dropped (class owns geometry now),
  disposes the class on exit.
- m36 article's step fix struck visibly (refuted, not deleted).
- AROS -Werror clean (both TUs). Full `ri_audit.sh` 0/0. Binary
  deployed to RAM:.

## Proof (device, detached runs — APPROVED 2026-09-24)
- Fresh RI-STEPS window 540 wide with 16 chunky 32 px custom
  buttons (visually confirmed in capture — dark squares row),
  readout 0, no guru.
- Owner click test pre-reboot (same binary): each button clicked
  separately doubles the readout (bits toggle exactly) — the
  m35 click sequence subsumed.
- Old instance closed via gadget first.

## Files
- env: `gui/widgets/rstp.mcc.c` (rewrite), `gui/widgets/rstp.h`
  (new), `app/stepproof.c`, `scripts/ri_audit.sh` (gate rstp.h),
  m36 article (struck correction)
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_stepproof`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Next: beat clock for live chase (no source in tree yet) + chase
highlight + accent/flam glows in the RStp renderer.
",
