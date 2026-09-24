# 2026-09-24 — MCC knob class: custom Numeric subclass, device crash, staged bisection, lane wedge (TC-2.9.2)

> Source: session evidence (spike results, guru captures ×4, staged
> diagnostics, audit log), compiled by agent
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
RESOLVED (was: code-complete unproven + wedged lane). Two defects,
both found by evidence: a type-confusion crash (_win vs _window,
decoded from the on-site guru disassembly against my own object
file) and a flags-gated Draw (initial show-time Draw carries no
MADF_DRAWOBJECT on this Zune — gated variant blank, ungated
paints). Custom knobs render on device, click-tested, full audit
0/0. Drag needs a human or a hold-and-drag agent primitive
(click = down/up with no move); notify has no app listener yet.

## What
- `gui/widgets/rknb.mcc.c` rewritten from MUIC_Knob skin to a real
  custom class: subclass of MUIC_Numeric (spec §13 reuse note kept —
  min/max/value/default + notify inherited), BOOPSI dispatcher with
  OM_NEW (default/value capture), OM_SET (value tracking + repaint),
  MUIM_AskMinMax (fixed 80×80, matches RI_KNOB_PX), MUIM_Show/Hide
  (IDCMP_MOUSEBUTTONS|MOUSEMOVE event handler), MUIM_Draw (blits
  the measured 80 px frame via `ri_knob_blit_one`), MUIM_HandleEvent
  (vertical drag through host-tested `ri_knob_drag_to_value`,
  Shift-fine via qualifier, right-click restores default TC-2.9.2,
  gesture begin/move/release for the one-undo-unit commit rule).
- `app/panel909.c`: uses `ri_rknb_create(default)` with per-control
  defaults from `ri_panel_default_ctl` (fail-closed to mid),
  FixWidth/Height 80, class disposed after app. MUIC_Knob is fully
  retired (no instantiation left in the tree).
- `gui/knob_blit.h`: gained the missing `<stdint.h>` (latent gap —
  prior consumers included it themselves).
- Blind hardening (reasoned, NOT device-verified): Draw path no
  longer calls back into the dispatcher — instance `cur` caches
  the last OM_NEW/OM_SET value (every programmatic change flows
  through OM_SET, so it stays exact). Render must not reenter.

## Proof (completed — see Resolution above; the trail stays)
- AROS compile clean under the audit's own flags (-Werror):
  rknb.mcc.c + panel909.c. Four header/API errors caught locally
  first (stdint, proto/utility.h, RemEventHandler name, forward
  decls) — the edit/understand loop stayed on host.
- On-device `ri_diag1` (class create → knob create → dispose):
  **rc=0**. Creation path (OM_NEW/OM_SET, GetTagData/FindTagItem,
  MUIMasterBase/UtilityBase) was INNOCENT — the crash needed a
  window open, which pointed at Show (event-handler target).
- Crash needs a window open (Show/Draw/window path): `ri_panel909`
  opens its RI-909 window (348×121, content-sized) then dies —
  guru names task WHd_panel909, PC inside a function (NOT a clean
  NULL call — the early NULL-base theory is REFUTED by the window
  existing at all).
- Reference fidelity unchanged: ReBirth RB-338 screenshot viewed
  (dark 909 section, small dark knobs) — our hardware-measured
  olive/orange art stays the target.

## Resolution (same day, post-reboot lane)
- `_window` → `_win` at both handler sites; rebuilt, redeployed.
- `ri_diag2` detached: window opened, lived, self-closed, NO guru
  (Show + EHN add + Draw ×4 + Hide + remove all survive).
- Red-rect variant (gated Draw): blank. Green-rect variant
  (ungated): 4 boxes, correct geometry. Verdict: draw on ANY
  MUIM_Draw — production Draw paints unconditionally now.
- `ri_panel909`: four olive knobs, orange pointers up (value 64),
  tick rings, pitch-80 MUI layout; x-centers 54/134/214/294,
  60 orange px. Click on knob 1 (down/up): no crash, pointers
  steady (global capture diff = pointer/refresh noise).
- Detached-only throughout (the wedge lesson held).
- Remaining: real drag (needs hold-and-drag primitive or human),
  app-side notify listeners, MCC typography, 2.10 step GUI.

## Lane wedge postmortem (doctrine addition)
- `ri_diag2` (window open, Delay, close, exit) was run FOREGROUND.
  It crashed → guru modal blocked the agent's synchronous exec →
  no reply → every later job (even ping) queues forever.
- Modal requesters are UNREACHABLE remotely: measured-aim mouse
  click on the Kill button (357,462), RETURN press+release, ESC —
  zero effect across all three mechanisms.
- **Rule: NEVER foreground-run ANY binary on the Dell (GUI or
  diagnostic) — always `Run >NIL:` detached, then observe via
  ui-windows/capture. A detached crash still shows its guru but
  the channel stays free.**
- USR1-dropping the server was REJECTED (agent is single-threaded
  and blocked; a drop without on-site redial kills the lane with
  no recovery path).

## Recovery runbook (next session / on-site)
1. On-site: click Kill on the two guru requesters (WHd_panel909,
   diag2) or restart the Dell agent
   (`SYS:ATCPBIN agent 192.168.1.81 9292 e6320`); stale queued
   jobs (move/click/rawkey) will replay harmlessly.
2. Confirm lane: ping + ui-windows.
3. Detached bisection matrix (all `Run >NIL:`, NEVER foreground):
   ri_diag2 variant A (Draw returns 0 without blitting) isolates
   the blit-in-Draw; variant B (Show skips AddEventHandler)
   isolates the EHN; variant C (neither) isolates AskMinMax/layout.
   Sources: `/home/miller/Work/ri_build/diagrknb.c`,
   `diagrknb2.c` (keep; never commit).
4. When clean: re-run `ri_panel909` detached, capture, eyeball +
   measure (x-centers, `#e37c3b` pointers, tick rings), then drag
   test via ui-click sequences, then commit the proof.

## Files
- env: `gui/widgets/rknb.mcc.c` (custom class), `app/panel909.c`
  (class wiring), `gui/knob_blit.h` (stdint) — UNPROVEN, see above
- Dell scratch: `/home/miller/Work/ri_build/dell2/` (binaries,
  guru captures incl. `gurufunc.png`/`guruerr.png`, winlists)
- build scratch (keep): `/home/miller/Work/ri_build/diagrknb*.c`
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Remaining: device proof of the class (render + drag + notify),
then MCC typography, then the 2.10 step GUI. spirv-val vacuous
(no SPIR-V in this repo).
