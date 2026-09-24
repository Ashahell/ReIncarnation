# §12.2 — 303 control-ID contract: wave/volume split, 303B dispatch, Tune

**Date:** 2026-09-24 (host lane; devices untouched)
**Scope:** review §2.2 (CRITICAL panel/engine ID mismatch); spec parity rows already adopted.
**Disposition:** New (second improvement-review slice; TDD RED-first)

## The bug (confirmed in code before touching)

- Panel 303A row +5 emitted 0x0305 labeled "volume"; engine `RI_CTL_303A_WAVE`
  is 0x0305. Turning "volume" below 64 selected saw, above 64 square.
- `rb303_set_param` switched on 0x030x only: all six 303B panel rows (0x0310–0x0315)
  went nowhere; `render.c` dropped non-0x0300 automation the same way.
- Engine had no Tune; panel had no Waveform control (ReBirth has both, §4.1).
- The user manual (`docs/ReIncarnation.guide`) documented the wrong map too
  (0x0305/0x0315 as volume) — contract doc fixed with the code.

## TDD record (RED watched: 19 FAILs → PASS)

`t36_303_ctls.c`: (a) 0x0305 flips wave / leaves level, 0x0306 carries volume
per the linear table law (100 → 100/127 — first draft wrongly asserted >0.9,
corrected to the documented taper); (b) every 303B ID behaves exactly like its
303A twin on a twin voice; (c) Tune center 64 = concert pitch (69 → 440.0),
±12 st = octave both directions, clamped ±24 st (127 → 1760, 0 → 110), live
notes bend by the exact new/old ratio on freq AND target; (d) panel/engine
one-table contract — all 16 engine IDs reachable from their panel, every panel
row whitelisted to the handled map, waveform/volume/tune defaults pinned
(0/100/64). IDs alone were added first to turn a compile error into a runtime
RED (staged GREEN, documented).

## Production changes

- `engine/dsp/rb303.h`: 303B ID block (0x0310–0x0317) + `RI_CTL_303A_TUNE`;
  voice gains `tune_st` (init 0).
- `engine/dsp/params.c`: dispatch normalizes the section block (shared
  implementation, per the review's fix shape); TUNE case with ±24 clamp and
  ratio bend; TU-local `clampf` (repo convention) + `kernels.h` (allowlisted).
- `engine/dsp/rb303.c`: tune applied in `rb303_note`/`rb303_slide_to`
  (tune 0 = ×1.0 exact — goldens bit-identical, verified by direct cmp).
- `gui/panels.c`: +5 renamed waveform (def 0 = saw), +6 volume (def 100),
  +7 tune (def 64), both sections — 8 rows each.
- `docs/ReIncarnation.guide`: rows corrected/added (audit's "303A volume" /
  "303B volume" strings kept — gate stays green).
- `tools/render.c`: comment-only — 0x031x deliberately NOT forwarded to the
  single 303A voice (would mistarget); second voice and 303B routing arrive
  with the integrated engine (§12.3).

## Held

Full single-source codegen (generate panels/guide/greps from one table):
engine IDs are canonical in `rb303.h`, the panel uses BASE+offset, and t36
pins the contract behaviorally. Codegen when a third consumer appears (YAGNI).

## Gates

- t36 PASS; neighbors green (t1_303math/walk, all t22_303*, t1_knob).
- 303 first-light re-render byte-identical (events too).
- `ri_audit.sh` 0/0 (`ri_build/audit_122.log`).
