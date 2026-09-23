# 2026-09-23 — WBS 2.9 first panel: TC-2.9.1–2.9.3 host pins + right-click default (TDD)

> Source: session evidence (test output ×4, audit 0/0 ×2), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. M2.3's last leg (2.3 ✅ 2.4 ✅, this slice = 2.9 partial: one
panel). Gap analysis of TC-2.9.1–2.9.3 against landed Task 12 code
(`knob_logic.*`, `panels.*`, `t1_knob`, Phase 12) found two genuine
holes: TC-2.9.2's right-click→default had NO accessor (the MCC shell
had nothing host-tested to call), and TC-2.9.1's contract names a
panel-geometry doc that did not exist. TC-2.9.3 was fully covered by
t1 — it gets a composition pin, not new code.

## What
- **Right-click default accessor** (production change, the one the
  pins demanded): `int ri_panel_default_ctl(const struct RIPanelDesc
  *p, unsigned int ctl_id)` in `gui/panels.c` (+ decl in `panels.h`)
  — returns the control's `def_value`, or -1 fail-closed on NULL
  panel / unknown id (the shell ignores -1). Five lines, mirroring
  the existing lookup style.
- **Two pins** (t22 CHECK-macro pattern, `return fails != 0`):
  - `t29_paneldefault` (TC-2.9.2 + TC-2.9.1 inventory): full 909
    default table (tune 64 / level 100 / decay 64 / flamres 64),
    spot defaults (303A cutoff 96, mixer bus1 127, transport tempo
    120), fail-closed edges; first-panel inventory (909 index 3 =
    exactly tune/level/decay/flamres at 0x0900..0x0903) — the code
    side of the geometry-doc contract.
  - `t29_chase` (TC-2.9.3 composition): full 16-step bar maps
    0..15→0..15 with 16→0 wrap; 174 BPM step (86.2 ms) exceeds the
    33 ms LED frame; toggle involution; lag-bound edges.
- **Panel-909 geometry doc** (`docs/evidence/gui/panel-909-geometry.md`,
  first panel): E0 HYPOTHESIS — rect (64,96)-(960,672) @1024×768,
  four centers (176/400/624/848, 320), ±2 px contract, aspect
  1.5556 ±1%, 2x-authored zoom rule with screen-px drag travel.
  Pixel centers live in MCC shells (not host-testable); the doc and
  the pin cite each other against drift.
- `acceptance.md`: right-click row added (unchecked — on-device gate
  stays red per the file's own rule; the `- [ ]` audit grep still
  passes on the remaining boxes).
- `scripts/ri_audit.sh` Phase 12: two `test t29_*` lines after the
  `t1_knob` line, TC-tagged FAIL echoes.

## Proof
- TDD RED first, watched: `t29_paneldefault` build-RED (`implicit
  declaration of 'ri_panel_default_ctl'` — feature missing, not a
  typo); `t29_chase` PASS first run as green pin over frozen Task 12
  code.
- GREEN after the 5-line accessor: both pins exit 0, `t1_knob` still
  PASS (no regression).
- Full `ri_audit.sh` 0/0 TWICE: pre-change baseline, then final over
  the committed tree (0 FAIL lines).
- Honesty: mouse-capture-outside-window is MCC-shell platform
  behavior, not host-pinnable — out of pin scope, stated not waved;
  one self-caught slip (stray declaration written into panels.c +
  comment-only header) fixed before the GREEN run — final code
  verified, not the draft.

## Files
- env: `gui/panels.c` (accessor impl), `gui/panels.h` (decl),
  `scripts/ri_audit.sh` (Phase 12 t29 lines)
- tests: `tests/unit/t29_paneldefault.c` `tests/unit/t29_chase.c` (new)
- docs: `docs/evidence/gui/panel-909-geometry.md` (new),
  `docs/evidence/gui/acceptance.md` (right-click row)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

No external websites consulted: contracts are WBS TC-2.9.1–2.9.3 +
spec §13 + the P-18 owned-UX numbers already in `knob_logic.h` — all
local and authoritative; nothing was open that needed the web.
spirv-val vacuous (no SPIR-V in this repo — noted honestly per
standing).
