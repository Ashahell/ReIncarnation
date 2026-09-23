# 2026-09-23 — Knob blit on device: BGRA root cause via calibration, orange pointers proven (eyeball + bytes)

> Source: session evidence (spike results, calibration captures, pixel censuses, audit 0/0), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. Closes the "go" deliverable from the art-recipe slice: computed
knob frames reach the Dell screen through cybergraphics
WritePixelArrayAlpha — after a real root-cause detour (lavender
ghost knobs) resolved by a calibration experiment, not guessing.

## What
- `gui/knob_blit.c` (new, AROS-only): frame staging (RGBA→words),
  lazy cybergraphics open, per-pixel-alpha blit, negative-origin
  clipping (doc centers near edges keep position instead of
  shifting the row), distinctive negative sentinels. Plain clib
  call with a `CyberGfxBase` global (standard pattern — the
  ICD's hand-rolled LVO vector was not needed here).
- `app/knobproof.c` (new, AROS-only): SmartRefresh 160×88 window
  at origin, 4 frames at doc centers with panel-default values
  (tune 64 / level 100 / decay 64 / flamres 64 — distinct pointer
  angles), exit code = failed-paint count (0 observed).
- Audit: guard-list + host-leak grep + AROS compile lines for
  both files (Phase 12 area).

## Proof (systematic-debugging Phases 1–4, textbook)
- **Symptom:** first blit showed lavender ghost knobs (bodies
  ~#9595b8, blue-tinted pointers) instead of charcoal + orange.
- **Phase 1 (no fix attempts):** read the SDK `.conf` (10th arg
  semantics), read the ICD's proven call shape, measured the
  ghost pixels numerically.
- **Phase 3 (single hypothesis, minimal test):** predicted a
  byte-order/alpha-layout mismatch; built a 6-swatch
  calibration binary (solid red/green/blue/white/black/half —
  scratch, never committed) + ran it on the lane.
- **Calibration verdict:** red/green/black showed background,
  blue/white exact, half gray blended EXACTLY 50% (149) —
  6/6 consistent with exactly one layout: words are BGRA
  (B high, A low), per-pixel alpha honored. No other
  layout explains all six (each alternative breaks ≥1 swatch).
- **Phase 4 (one-line fix):** packing `(a<<24|r<<16|g<<8|b)` →
  `(b<<24|g<<16|r<<8|a)` with a do-not-revert comment citing
  this proof. Rebuilt, redeployed, re-ran.
- **Verification:** eyeball shows dark knobs with bright orange
  pointers at four distinct angles; pixel census finds 32
  pixels of byte-exact `#e07b2e` + charcoal bodies `#2b2b2b`
  at doc centers within 1px. Close path green again.
- Full `ri_audit.sh` 0/0 over the frozen tree (0 FAIL lines).

## Files
- env: `gui/knob_blit.c` (new), `app/knobproof.c` (new),
  `scripts/ri_audit.sh` (guard + leak + compile lines)
- Dell scratch: `/home/miller/Work/ri_build/dell2/` (calibration
  sources/binaries/captures — method + values above, binaries
  cleaned after proof)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

Fidelity position advanced: knob visuals now match the reference
language on real hardware (dark + orange, measured). Remaining
art work: panel background/composition around the knobs (the
proof window is bare Intuition gray), then the MCC integration
that retires the MUIC_Knob stock look in the real panel.
spirv-val vacuous (no SPIR-V in this repo).
