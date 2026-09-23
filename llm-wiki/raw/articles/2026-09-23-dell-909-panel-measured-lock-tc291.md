# 2026-09-23 — Dell 909 panel: window opens on explicit open, compact geometry measured twice, doc locked (TC-2.9.1 device evidence)

> Source: session evidence (spike results, winlists, pixel censuses ×3, host tests, audit 0/0), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. Closes the loop opened by m15 (empty window proven) toward
TC-2.9.1 device evidence: the 909 panel window now opens with
four knobs, measured twice pixel-identical, geometry doc locked
E0→measured. Two systematic-debugging cycles (H2a open
mechanics, E0→E2 sizing), each single-variable, each decisive.

## What
- **H2a (root cause, proven):** open-at-creation
  (`MUIA_Window_Open` in the creation tag list) produces a
  complete object but NO window on this lane; explicit
  post-creation `SetAttrs(win, MUIA_Window_Open, TRUE)` opens
  immediately. Tested by moving exactly that call (all else
  identical): absent → `'RI-909' 0 0 288x88` listed. First
  rebuttal of "MUI doesn't work on the Dell" — it does, with
  explicit open.
- **Sizing experiments** (scratch `panel909_dbg.c` variants, never
  committed): E0 cells (InnerLeft 84 + FixWidth 224) → 288×88
  window; E1 bare knobs + post-hoc SetAttrs fix → 160×88; E2 bare
  knobs + creation-time FixWidth/Height → 160×88 identical.
  Mechanism identified: knob.mui renders its ~32 px intrinsic
  regardless of FixWidth (object or cell level); cells added only
  slack. No third mechanism hypothesized without evidence.
- **Measured lock** (two independent runs, pixel-identical down
  to component pixel counts): 160×88 window at (0,0); knob
  indicator bars at screen x 30/63/96/129 (exact 33 px pitch),
  body row y≈52, ~32 px visuals, all four knobs identical
  footprints (105–108 px). Bodies agree with indicators within
  1 px; pointers move with value, bodies don't (centers are
  value-invariant by construction).
- **Reconciliation** (integrity decision, recorded not hidden):
  the E0 224-pitch/1024-canvas premise is refuted on device, so
  the doc locks the measured compact panel (even pitch, aligned
  row, consistent size — principled order, not accident), with
  the E0 numbers struck through visibly. The ±2 px contract now
  guards REGRESSION (re-measurement must match), which is
  falsifiable; a doc-set-from-screen with no design rule would
  not be, and was rejected for that reason. 1024-canvas full
  panel + ReBirth-style art stay queued (M2.4).
- **Lifecycle re-proven:** second `ui-close` via closerequest
  (108 ms) → window gone, agent alive — close path green on two
  builds.
- `app/panel909.c` carries the measured E2 configuration
  verbatim (bare knobs + creation FixWidth, explicit open);
  `ri_panel909_knob_rect` + `t29_layout` + geometry doc locked
  to (30,63,96,129 @ y52, d32); silhouette acceptance box
  CHECKED with this device evidence.

## Proof
- Markers s0–s5 (scratch DIAG build): all objects built, loop
  entered — yet no window. Eliminated silent-exit/crash classes
  before any fix attempt (systematic-debugging Phase 1).
- Winlists: absent → `288x88` (H2a) → `160x88` (E1, E2
  identical). Captures + connected-component censuses name
  every landmark above to the pixel, twice.
- TDD host side: RED (missing type+function) → GREEN
  (t29_layout + all GUI tests); full `ri_audit.sh` 0/0 over the
  frozen final tree (0 FAIL lines) — no mid-flight edits this
  time (m10/m12 discipline held: the earlier audit that overlapped
  edits is void and so recorded).
- Self-caught along the way: s3–s5 markers dropped in a scratch
  re-sync (drew a false contradiction for ten minutes — the app
  was fine, my instrumentation was incomplete); a `SetAttrs`
  intuition-protos include gap; a `#define`-eating replaceAll
  (same class as the m13 TAIL hit — readback discipline caught
  both); a `head`-truncated build verdict that faked a link
  failure (never pipe a build gate through `head`).

## Files
- env: `app/panel909.c` (measured E2 config), `gui/panels.c`
  + `gui/panels.h` (compact accessor), `tests/unit/t29_layout.c`
  (compact asserts), `scripts/ri_audit.sh` (unchanged lines,
  re-verified)
- docs: `docs/evidence/gui/panel-909-geometry.md` (lock +
  struck E0), `docs/evidence/gui/acceptance.md` (silhouette box
  checked)
- Dell scratch: `/home/miller/Work/ri_build/dell1/` (binaries,
  winlists, captures, markers — values above, not committed)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Fidelity position restated: layout fidelity proven on device;
ReBirth-style knob art stays its own scheduled task under
never-pixel-copy. This session proves positions, not art.
spirv-val vacuous (no SPIR-V in this repo).
