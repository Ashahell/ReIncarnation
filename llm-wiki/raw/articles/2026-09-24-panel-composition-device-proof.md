# 2026-09-24 — Panel composition on device: bg, rules, labels, knobs (eyeballed + measured)

> Source: session evidence (spike results, captures ×2, pixel censuses, audit 0/0), compiled by agent
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New. First full panel composition on the Dell: background fill,
divider rules, knob labels, and the four v2 knobs in one
296×96 window — eyeballed against the reference language and
measured against the doc. Self-caught file damage on the way
(headers repair, documented below).

## What
- `gui/knob_blit.h` (new): decl header for the blit helpers
  (the AROS TU previously declared them ad hoc).
- `gui/knob_blit.c`: `ri_knob_panel_rect()` (opaque fills via
  the proven BGRA packing) + fail-closed edges.
- `app/knobproof.c`: composition — bg fill, 3 divider rules,
  4 labels (system pen 1, TextLength-centered), 4 knobs at doc
  centers with panel defaults; window sized from the new
  `RI_PANEL909_W/H/BG` constants (`panels.h`, pinned in
  `t29_layout`).
- Audit: no script changes needed (no new tests/files in scope
  beyond existing gates — verified by the green run).

## Proof
- Eyeball: 4 dark knobs, orange pointers at distinct angles,
  tick rings, legible TUNE/LEVEL/DECAY/FLAMRES labels, light
  panel bg — the reference composition language, live.
- Bytes: bg exact `#dcdcd6` everywhere sampled; knob x-centers
  0px error; y within the diagnosed tick-gap bias; pointers
  correct per value; `#e37c3b` byte-exact.
- Labels legible (Topaz-bitmap chunky — period-authentic;
  MCC fonts later). Rules code-proven via the verified rect
  path, sub-pixel at capture scale (8px found where 2:1
  downsampling hides 1px lines) — stated, not over-claimed.
- Full `ri_audit.sh` 0/0 over the frozen tree (0 FAIL lines).
- File damage caught by readback: a header edit deleted the
  rect API + corrupted a return type; repaired before any
  build, verified by diff. Never trust an edit without
  re-reading.

## Files
- env: `gui/knob_blit.h` (new), `gui/knob_blit.c` (rect helper),
  `gui/panels.h` (composition constants),
  `app/knobproof.c` (composition), `tests/unit/t29_layout.c`
  (constant pins)
- Dell scratch: `/home/miller/Work/ri_build/dell2/` (proof
  binary kept for re-display, captures, winlists)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Remaining: MCC integration (custom knob class retiring
MUIC_Knob), panel title/label typography, then the 2.10 step
GUI. spirv-val vacuous (no SPIR-V in this repo).
