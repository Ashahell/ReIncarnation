# 2026-09-24 — Separator seam + step hit-rate: repaint-all + 32px targets

> Source: owner reports (bar back; step clicks mostly dead) + capture
> forensics on the Dell lane
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. Two independent defects, one shared shape (assumed layout,
found painting/input): a stale-pixel seam and 14 px click targets.

## Separator bar: verdict + fix
- Bisect from existing captures: Intuition proof windows (our
  blits, no MUI) have NO bar; the MUI app does. Our palette holds
  no such grey — it is MUI-side paint, not our art.
- Layout measures uniform (pitch exactly 80, all four knobs same
  structure), so the bar is PAINTING, not layout: knobs repaint
  only on value change, so any text-driven relayout leaves stale
  pixels at frame abutments (grey bar at the exact 80 px seam).
- Fix (`app/panel909.c`): repaint all four knobs (idempotent full
  frames) whenever any text refresh fires. Harmless under every
  sub-cause.
- Confirming question for the owner (decides stale vs structural):
  cover/uncover or slightly resize the window — a stale seam
  repaints away, a structural one persists.

## Step clicks: verdict + fix (CORRECTED — FixWidth does nothing)
- RI-STEPS buttons render ~14 px wide (no Fix size ever set):
  most clicks land BETWEEN buttons. "Sometimes works" = hit-rate,
  not logic (button + notify + loop all proven shapes).
- ~~Fix (`app/stepproof.c`): FixWidth/Height 32 via the rstp shell
  (shell stays the construction point, knob FixWidth pattern).
  Window grows to ~512 + chrome.~~ — REFUTED ON DEVICE: stock
  MUIC_Numericbutton ignores Fix sizes exactly like knob.mui (E0
  lock note); window stayed 268 px. Sizing a stock class is
  impossible; m37 builds the custom RStp class instead (32 px
  AskMinMax + own art + click toggle), which 2.10-full needs
  anyway for chase LEDs.

## Files
- env: `app/panel909.c` (repaint-all), `app/stepproof.c` (targets)
- Dell scratch: both binaries redeployed to RAM:
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Full `ri_audit.sh` 0/0. Pending: owner refresh-test (seam) +
step click sequence on fresh instances (close both windows first).

## Resolution 2026-09-24: bar absent on fresh instance
Fresh m36-build window (post-reboot lane): no grey concentration
anywhere near the old seam (≤1 noise hit/column vs 40–50 before).
Verdict: transient stale-pixel artifact of the long-lived,
heavily-dragged prior instance — fresh opens paint clean and
repaint-all guards text-driven shifts. Refresh-test moot. If it
ever reappears, suspect a non-text relayout trigger (e.g. manual
window resize) and report it as new evidence.
