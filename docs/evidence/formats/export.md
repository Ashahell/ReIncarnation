# WAV export — evidence ledger (Module 2.5/2.13, TC-2.13.4 host scope)

**Date:** 2026-09-23. **Status:** PARTIAL (16/24-bit at 48 kHz locked;
44.1 kHz + AIFF stay OPEN, named below — no parity claimed beyond
what is pinned here).
**Spec:** WBS TC-2.13.4 (WAV at 44.1/48 kHz, 16/24-bit, valid
header, no clipping past genuine peaks).

## Writers (two sinks, one contract)

- Offline fixtures: `tools/render.c` `write_wav` (float PCM in,
  `--depth 16|24`, default 16). The depth-16 path converts through
  the same `f32_to_s16` as before — all pre-existing goldens
  re-render byte-identical (audit Phase 1 proves it every run).
- Engine export: `audio_io/audio.c` `au_render_song_to_wav_depth`
  (+ `AuRenderToFileDepth`; `AuRenderToFile` delegates depth 16).
  Header + packing contract owned by `auf_wav_header` /
  `auf_f32_to_s24` (declared in `audio_io/audio.h`; the render
  tool calls them — one contract, no third implementation).
- Rounding twins (deliberate): float→int truncates toward zero, so
  −1.0 maps to −32767 (16-bit) / −8388607 (24-bit), one LSB shy of
  symmetric full scale — identical rounding, more bits (`t28`
  pins both edges; changing either twin alone would diverge them).

## Pinned (`tests/unit/t28_wavdepth.c`, audit Phase 1)

- Committed 16-bit golden structurally valid
  (`tests/golden/303/math-dc.wav`: magic, sizes, fmt, rate).
- Header builder matrix 2 depths × 3 totals field-exact.
- s24 packing exact bytes (FS+/FS−/zero/clamps).
- New 24-bit golden `tests/golden/303/dc-24.wav` + `.sha256`
  (288000 data bytes, structural parse in-test; sha256 +
  determinism re-render in audit).
- Depth wrapper null-fail-closed (rc 2).

## OPEN (not silent)

- 44.1 kHz export: needs a resampler (no rate conversion exists
  anywhere in the tree) — own slice when scheduled.
- AIFF: no writer exists — own slice when scheduled.
- External validation (sox/Audition per the TC text): no sox on
  the host lane; the host gate is structural parse + golden bytes.
  Device/external import validation rides the on-device pass.
