# 2026-09-23 — Knob art recipe: pointer-angle contract + procedural frames + C port (TDD, eyeballed)

> Source: session evidence (web references ×2, PIL iterations + eyeball ×3, test output, audit 0/0), compiled by agent
> Collected: 2026-09-23
> Published: 2026-09-23

## Disposition
New. Answers the shape+color verdict on MUIC_Knob stock visuals
with original art, not restyled stock: pointer-angle contract
(`ri_knob_pointer_mdeg`, exact integer) + procedural 909-style
knob frames (dark charcoal body, orange pointer, 64×64 @2x) +
integer-only C port (`gui/knob_art`) + pin (`t29_knobart`,
audit Phase 12). AROS blit + on-device proof are the scheduled
next step, not this one.

## What
- `ri_knob_pointer_mdeg(value)`: 0..127 → -135000..+135000
  millidegrees (270° sweep, 0 = up), round-half-up, clamped.
  Pinned exact (ends, mid 1063, unit steps 2125/2126,
  monotone, clamp edges).
- PIL prototype (`/home/miller/Work/ri_build/knobart/`,
  scratch): v1 → v2 (4px pointer, rim-light arc) with eyeball
  verification each round (sweep directions correct at
  0/64/127; magnified review of disc/pointer/hub/rim).
- C port: Q15 360-entry sine table (cardinals exact, norm-checked
  in-test), squared-distance geometry (no fp, no libm), exact
  structural pins (transparent corners, hub, rim, orange pointer
  at 3 heights for value 64, off-pointer body, determinism,
  value-sensitivity). Colors as reviewable header defines from
  measured photo anchors.
- Port fidelity measured, not assumed: 87% pixels identical to
  PIL; remainder = two documented divergences (gradient
  truncation ±1-3, 3-vs-4px pointer simplification — the latter
  invisible at 32px display size, revisit on eyeball evidence).
- Wired: `MOD_gui` + audit AROS-compile line + Phase 12 test
  line. Cleanup per standing request: stale Dell binaries +
  `.o` + regenerable WAV artifacts removed (kept latest
  measured binary, captures, ffmpeg refs, knobart workdir).

## Proof
- TDD RED first (implicit declaration), GREEN all GUI tests.
- Full `ri_audit.sh` 0/0 over the frozen tree (0 FAIL lines).
- References consulted where genuinely needed: TR-909 photo
  (Gear4Music, fetched + measured + viewed — shape/color
  verdict confirmed visually) and ReBirth RB-338 screenshot
  (vintagesynth, fetched + viewed — small dark knobs, orange
  pointers, gray/black panels confirm the shared design
  language). McGill-spec fetch failed transport (recorded, not
  needed — ffmpeg refs already covered the 80-bit constants).
- Honesty: one pin expectation corrected pre-GREEN (pointer-low
  pixel sits in the hub-ring zone by draw order — test bug,
  code matches the documented order); PIL-vs-C divergences
  documented, not hidden; AROS blit + device proof explicitly
  open.

## Files
- env: `gui/knob_logic.c` + `gui/knob_logic.h` (angle fn),
  `gui/knob_art.h` + `gui/knob_art.c` (new: table + renderer),
  `scripts/ri_build_host.sh` (MOD_gui), `scripts/ri_audit.sh`
  (AROS line + Phase 12 line)
- tests: `tests/unit/t29_knobart.c` (new: angle + frame pins)
- wiki: this article + `llm-wiki/log.md` + `llm-wiki/index.md`

spirv-val vacuous (no SPIR-V in this repo).
