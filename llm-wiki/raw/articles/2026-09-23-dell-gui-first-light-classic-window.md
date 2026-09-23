# 2026-09-23 — Dell GUI first light: Classic window opens, runs, closes clean (TDD layout accessor + v11 lane)

> Source: session evidence (host tests, cross-build log, spike results, winlists, pixel census), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. First GUI binary on Dell hardware (E6320, ABIv11, agent
e6320 session 1, spike 9292 — server + agent + ping all live, no
bring-up needed). Establishes the GUI delivery pipeline
(cross-build → artifact gates → put → run → capture → measure →
close) with the trivially-verifiable artifact before any widget
code: the existing `app/main.c` empty window. Widget
instantiation (knob centers on screen) is the scheduled next
step, not this one.

## What
- **Layout accessor** (TDD, host): `ri_panel909_knob_rect(index,
  zoom)` in `gui/panels.c` (+ `struct RIPanelRect` + decl in
  `gui/panels.h`) — panel-909-geometry doc centers at 1024×768,
  56 px knobs at 1x, scaled via `ri_zoom_factor`, fail-closed
  zeros on bad index/zoom. Pin `t29_layout` (doc centers exact,
  2x scaling, edges), wired in Phase 12. RED watched
  (missing type + function); GREEN all four GUI tests; full
  `ri_audit.sh` 0/0.
- **v11 Dell build** (by hand from the m1-1 recipe — enshrine the
  script after one more proven use, per promotion discipline):
  v11 `x86_64-aros-gcc`, `-std=gnu99 -O0`, `-mcmodel=large
  -mno-red-zone -mno-ms-bitfields -fno-strict-aliasing
  -ffixed-r12`, v11 SDK includes, `-nostartfiles -no-pie`,
  `startup.o` from v11 `Developer/lib`, `-ldos -lexec
  -lintuition` from the core-build `Development/lib`.
  `app/main.c` + `gui/panels.c` + `gui/knob_logic.c` compile
  warning-free first try → `ri_classic` 28,048 B
  (`/home/miller/Work/ri_build/dell1/`, scratch).
- **Artifact gates** (v11 lane — note the inversion: r12 moves
  EXPECTED here, the r12==0 gate is v1-guest-only):
  `task.resource` string count 0, ELF64 AROS relocatable, r12
  moves 168 (live convention, m1-1 probe had 225).
- **Dell run** (agent e6320, all rc=0/PASS): put 28,048 B in
  901 ms (`sha_ok`); `Run >NIL: RAM:ri_classic` rc=0 in
  107 ms; window list shows exactly `ReIncarnation Classic
  (6 panels)` at 0,0 1024×768 on a 1024×768 screen
  (`idcmp_close: true`, background) — the panel-count contract
  wired into the title proves `panels.c` linkage live on
  device, and the geometry matches the doc canvas assumption.
  512×384 capture: 93% flat gray (backdrop/window fill
  indistinguishable at this scale — honestly noted, not
  claimed as proof; the winlist is the proof).
- **Clean close**: `--ui-close` on the title → window gone
  (`['', 'AROS']`), agent alive, no alert — the
  IDCMP_CLOSEWINDOW → CloseWindow → exit(0) path works on
  hardware.

## Fidelity position (user directive, governing constraint)
Layout fidelity now (centers/proportions per the geometry doc,
measured on device starting with this window's canvas),
style fidelity later: MUIC_Knob stock look is NOT ReBirth art,
and custom knob artwork (2x-authored, original pixels) is its
own scheduled task. Policy from spec §1 + acceptance.md holds
throughout: style-matched, never pixel-copying. This session
proves positions; it makes no art claim.

## Files
- env: `gui/panels.c` (accessor), `gui/panels.h` (rect + decl),
  `tests/unit/t29_layout.c` (new), `scripts/ri_audit.sh`
  (Phase 12 t29 line)
- Dell artifacts: in `/home/miller/Work/ri_build/dell1/`
  (volatile scratch — binary, winlists, captures; values above,
  not committed)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Next: instantiate the four MUIC_Knob widgets at the accessor
rects in the Dell window → screendump → measure centers vs doc
(±2 px, TC-2.9.1 first device evidence).
