# 2026-09-23 — v2 art lock: 70px pitch from hardware ratio, pointer-tip geometry, exact orange on device (TC-2.9.1)

> Source: session evidence (TR-09 photo measurement, lane captures ×2, pixel censuses, audit 0/0), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. Closes the v2 fidelity loop opened by the knob-art recipe:
hardware pitch ratio measured on the official TR-09 photo,
geometry re-locked, proven on device twice (eyeball + bytes).

## What
- **Hardware ratio** (TR-09 photo, adjacent TUNE/LEVEL knobs):
  ~74 px pitch on ~55 px bodies = **1.35** → pitch 70 for our
  52 px art bodies (was 33, overlapping). Centers
  (40,110,180,250 @ y52), window 296×96. Same even/aligned/
  consistent rules, new numbers; compact E0 struck visibly.
- **Fidelity fixes from measurement**: warm olive face (was
  neutral gray), short thick pointer r15–24 (was r6–22),
  11-tick ring r29–35 with bottom gap (new), SE shadow (new),
  hub removed (none on hardware).
- `app/knobproof.c` blits at centers−40; `gui/knob_blit.c`
  unchanged contract (80px frames).

## Proof
- x-centers 0px error on all four knobs; y-center 52 via
  pointer-tip geometry (the body-bbox 2px offset was tick-gap
  bias — diagnosed, superseded, documented as a cautionary
  tale about adorned-centroid measurement).
- Pointer angles correct per value (64 → vertical; 100 →
  12° math = 77.6° knob, after a convention confusion caught
  mid-analysis).
- Pointer color on device byte-exact `#e37c3b` (57 px);
  tick rings visible ringing all four knobs; eyeball confirms
  the reference language live.
- Full `ri_audit.sh` 0/0 over the frozen tree (0 FAIL lines).
- Process note: test+table landed in one batch (RED logically
  certain but unwatched — recorded once, not repeated).

## Files
- env: `tests/unit/t29_layout.c` (v2 asserts),
  `gui/panels.c` (v2 table), `app/knobproof.c` (v2 geometry),
  `docs/evidence/gui/panel-909-geometry.md` (v2 lock + struck
  history); art internals unchanged since m17 pins
- Dell scratch: `/home/miller/Work/ri_build/dell2/` (v2 binary,
  winlists, captures — values above)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Remaining art: panel composition around knobs (background,
labels, sections), then MCC integration retiring MUIC_Knob.
spirv-val vacuous (no SPIR-V in this repo).
