# 2026-09-24 — Custom label class: MUIC_Text inverts, own pixels instead

> Source: session work (capture forensics + custom class + device)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Label row rendered black-bg/light-text while the identical
value row rendered light/dark; layout proven correct, so MUI Text
defaults are the culprit — replaced with owned pixels.

## Forensics (all from Dell captures, no guessing)
- m31 build (values row, NO names row): no black band — knob art
  only. Black arrived exactly with the names row.
- Undo row ("0", same class): LIGHT. Values ("64"): LIGHT. Names
  ("TUNE"...): BLACK full-width band. Same MUIC_Text + FixWidth
  in all three — contents-independent cause unfindable in any
  header; layout code re-read and correct (stacked groups, no
  overlap possible).
- "Pressing increases a value": label strips visually merge with
  knob tops (zero spacing), so presses aimed at labels land on
  knobs — user aim, not a bug, and it vanishes once labels stop
  looking like buttons.

## What
- `gui/widgets/rlbl.{h,mcc.c}` (new, AROS-only): Area subclass,
  80×14 AskMinMax, Draw fills RI_PANEL909_BG + centered pen-1 text
  via graphics Text/TextLength (the knobproof calls, hand-rolled
  length — no libc in these TUs). No input handling (labels never
  eat clicks). Text attr `MUIA_RLbl_Text` (`TAG_USER+0x524C`),
  copied at creation (24-char cap).
- `app/panel909.c`: names via `ri_rlbl_create`, class disposed on
  exit. Value texts untouched (proven fine).
- Audit Phase 12 gates rlbl.*. AROS -Werror clean. Full audit 0/0.
  Binary deployed to RAM:.

## Proof (device, detached runs)
- Fresh window: dark-on-light labels, no bevel boxes, columns
  aligned; knobs/values/undo unchanged; no guru.
- Agent-verified 2026-09-24 on the committed build: label band
  1924/2040 px light bucket + dark glyph pixels; seam zone zero
  grey hits (separator still absent — m38 holds).
- Owner full-res eyeball still open (spelling crispness).

## Files
- env: `gui/widgets/rlbl.{h,mcc.c}` (new), `app/panel909.c`,
  `scripts/ri_audit.sh`
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
