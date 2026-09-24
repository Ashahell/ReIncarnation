# 2026-09-24 — Owner numeric approval: full range + readouts verified

> Source: owner on-device numeric checks (readout build)
> Collected: 2026-09-24
> Published: 2026-09-24

## Disposition
New (verdict capsule). The m31 readout row did its job: every
remaining knob behavior was confirmed by NUMBER, not eyeball.

## Approved on device
- All four knobs range 0–127 by drag (both axes).
- Readouts track live under each knob.
- Right-click LEVEL → 100 exactly (TC-2.9.2 by number).
- Shift-fine single steps.
- Overall: "all looks fine".

## Still open (do not claim)
- Commit-on-release (no app listeners yet); fader travel.
- Acceptance boxes for the above stay unchecked; everything else
  in Owned-UX + silhouette is now checked.

## Files
- env: `docs/evidence/gui/acceptance.md` (right-click + fine boxes
  checked with this verdict)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`
",
