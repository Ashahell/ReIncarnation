# §12.8c2 — Appendix-D patterns: E1 extraction, ledger, engine wiring

**Date:** 2026-09-25 (host lane; devices untouched)
**Scope:** review §4.4 action 1 (pattern extraction) + engine use.
**Disposition:** New (mechanical E1 extraction + TDD wiring)

## Extraction (E1, no hand transcription)

Source: ReBirth Owner's Manual PDF (FrameMaker 5.5, 232 pp.), Appendix D
pp. 205–220 (PDF index 205–220). bbox (pdftotext) for label geometry +
titles; raster column profiles (pdftoppm PGM 150 DPI, stdlib parser) for
bar heights + loop triangles; overlay review (boxes/ticks drawn back).
Scripts (scratch, rerunnable): `pcf_extract.py` (survey) +
`pcf_decode.py` (ledger + verify) + `pcf_overlay.py` (visual proof).

## Census (corrects the review's "54 patterns (0–53)")

55 patterns 0–54. Resolution from the axis label grammar (mechanical):
39 two-bar-16 → 16th; 15 straight-32 → 32nd; 1 quad-8 (pattern 53) → 8th.
Assignment cross-checked against section headers (exact agreement).
7 patterns carry no end marker (0, 1, 6, 21, 34, 42, 44 — visually
verified empty bands): loop runs the full displayed width (32), flagged
for ReBirth behavior check.

## Verification (each independent)

- Text cross-checks: pattern 3 = 12 steps, pattern 40 = 28 steps (manual
  prose pp. 206/215).
- No hit at/after any end marker (all 55, mechanical).
- Length rule via marker tip + round(), boundary cases settled by
  zoom-inspection (pat3 ▼-tip vs pat40 ◀-tip geometry).
- Overlay review across sections (16th/32nd/8th, sparse/dense), incl. the
  2 px ghost note (pattern 15 step 1, kept at velocity 1).
- Velocity: global max (pattern 44 step 1, 184 px) → 127; 773 hits.
- Ledger table cells (55) verified mechanically against the JSON.
- Review errata found en route: pattern count, no-attack-marks (attack
  stays default), tone mapping (paper: output LP — current click mapping
  kept as golden-safe stepping stone, recorded).

## Engine wiring

Table loader (magic PCFP, mirrors pcf-table contract) + install API +
per-resolution clock (16th/32nd/8th dividers — the ×2-mapping bug caught
in TDD) + length wrap + step-0 firing; neutral fallback preserved
(existing tests green unmodified); `RI_PCF_NPATTERNS` 54→55 (t25 moved
deliberately); fixtures install pattern 0; wrapper stays neutral until
the song owns pattern state (§12.9).

## Golden fallout: NONE

- Full `ri_audit.sh` 0/0. Fixtures run Amt≈0/mode 0 — the envelope is a
  proven no-op there (patterns change nothing without Amt).

## Gates

- t49 RED→GREEN (link-RED, span off-by-one, clamp collision fixed by
  design). Full `ri_audit.sh` 0/0 — all 14 phases.
