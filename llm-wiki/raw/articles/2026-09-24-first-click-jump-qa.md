# 2026-09-24 — First-click jump: warp arms past 3px + owner Q&A (buttons, FLAMRES)

> Source: owner reports (jump, buttons question, FLAMRES question)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. One real fix (warp arming) + two questions answered from
code evidence. Device proof of the fix pending owner re-test.

## The jump: press-tremor yank
Unthrottled warp-every-move yanked the pointer on the first tiny
motion of every drag (press-tremor → immediate pin). Fix
(`rknb.mcc.c` only): grab ARMS past 3 px accumulated travel
(`RKNB_WARP_ARM_PX`, `warp_armed` set on crossing, cleared on
release/hide/press); below that the pointer tracks 1:1, and the
single transition yank stays ≤3 px ≈ invisible. No host-logic
change (threshold lives in the dispatcher; mapping untouched).

## Are the missing buttons expected? Yes
The "buttons" were the MUIC_Text label row rendering black +
beveled (m39 forensics). The custom label class paints flat text,
so they are gone by design — never interactive, never will be.

## What FLAMRES means (honest: placeholder with a reserved slot)
- Slot 0x0903, fourth 909 voice control, default 64; intended:
  flam timing/resolution (spec E1: per-step per-sound flam +
  resolution knob). NOT on original TR-909 hardware (no flam
  there) — our extension.
- Today: `rb909_set_param` documents it as placeholder (flam
  delay is trigger-time scheduler state, no voice field yet), so
  the knob moves pointer + readout and changes no sound. Label
  stays: the slot is real and will be wired; renaming it now
  would fake hardware parity we don't have.

## Files
- env: `gui/widgets/rknb.mcc.c` (arming only)
- Dell scratch: `/home/miller/Work/ri_build/dell2/ri_panel909`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Full `ri_audit.sh` 0/0. Pending: owner first-click check on a
fresh instance (press should feel 1:1, no jump).

## Approval 2026-09-24: jump gone, counter question, FLAMRES re-asked
- First press feels direct, no jump — owner: "looks ok". APPROVED.
- Counter on bare click: yes by design (see below) — owner noticed,
  not objected.
- FLAMRES asked again (earlier answers apparently never displayed):
  re-answered in full below; label stays.
",
