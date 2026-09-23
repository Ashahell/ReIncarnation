# WAV export — evidence ledger (Module 2.5/2.13, TC-2.13.4 host scope)

**Date:** 2026-09-23. **Status:** PARTIAL (16/24-bit WAV +
44.1 kHz offline + plain AIFF locked; AuRender/live rate +
external validation stay OPEN, named below — no parity claimed
beyond what is pinned here).
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

- AuRender/live rate: the engine DSP takes float sr everywhere and
  the offline tool renders at both rates, but `AudioObject` carries
  no rate (RI_AUDIO_SR locked; the AHI path negotiates its own) —
  threading rate through the live path touches Dell-verified 2.7
  code and rides its own slice with on-device proof.
- External validation (sox/Audition per the TC text): no sox on
  the host lane; the host gate is structural parse + golden bytes.
  Device/external import validation rides the on-device pass.
  (ffmpeg 9.0.1 parses both AIFF goldens and decodes dc-aiff to
  bytes identical with the WAV golden's PCM — the closest
  available external check, recorded 2026-09-23.)

## Plain AIFF (`--format aiff`, `t32_aiff`)

FORM/AIFF + COMM + SSND, big-endian PCM, 16|24-bit, 44100|48000 Hz
(no MARK/INST; AIFF-C/sowt out — different container). Contract
owners in `audio_io/audio.{c,h}` (`auf_aiff_header`,
`au_render_song_to_aiff`, `AuRenderToFileAiff`); the render tool
dispatches through `write_audio` (same float PCM, same depths).
80-bit extended rates verified against ffmpeg-generated
references (44100 = 40 0E AC 44 00.., 48000 = 40 0E BB 80 00..).
Goldens `dc-aiff.wav` + `first-light-aiff.wav` + `.sha256`
(sha256 + determinism re-render in audit).

## 44.1 kHz offline render (`--rate`, `t31_rate441`)

`--rate 48000|44100` (default 48000): every render mode renders
AND writes at `g_rate` (map/clocks/voices/phases/durations/layer
rates/syncs/allocation + 1 s tails = `g_rate` samples; the retired
`RI_TAIL_SMP` constant documented the old fixed tail). Default-rate
outputs byte-identical (math-dc + first-light proven in audit).
Goldens `dc-441.wav` (96000 samples) + `first-light-441.wav`
(114975 samples = song duration × 44100/48000, ratio 0.9187 vs
exact 0.91875 — duration scaling honored). Engine sanity at
44100: delay sync exact (16538), 303/PCF DC unity, 808 BD
finite/in-band; clock already pinned by t21_seq (exact 115200
ticks both rates, cited not duplicated).
