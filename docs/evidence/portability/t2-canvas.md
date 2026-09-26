# T2 — Display list: all art out of `rsection.mcc.c`

- Status: GREEN (host) 2026-09-26. AROS device pixel-identity open (owner lane).
- `gui/draw/` (new, pure C99): `canvas` (dlist + string pool + FNV hash),
  `art_shared` (palette + knob/meter/fader/keys/glyphs/digits/gr/tr/arrow),
  `art_303/808/909/mix/fx/pat/tr` (backgrounds), `art_section`
  (`ri_draw_section`: skin IMAGE decisions, per-item painters, chase lamps,
  focus bar). Two content decisions use build-time text metrics (`tm`):
  909 option shortening and `text_at` anchoring — AROS passes its metrics,
  keeping them bit-identical with the old direct path.
- Transcription catch: the 909 bar table first landed with 12 invented
  entries (CH/OH/CC/RC splits); corrected to the original 10 (HI HAT,
  CYMBAL) by re-reading the source. Lesson: transliterations need a
  table-by-table diff, not memory.
- `rsection.mcc.c` is now a replayer (`rsection_replay.inc`): build with
  AROS metrics + active skin + panel, replay via pens (palette
  reverse-map), the friend bitmap, blits by part index
  (`ri_skin_aros_blit_idx`, factored out of `blit`), centred text via
  TextLength. Deleted ~600 lines of painters; `RI_RSECT_RGB`/C_* now live
  in `gui/draw` (single source; pens resolve through `ri_art_rgb`).
- Host: `platform/host/raster.c` (software RGBA replay, clean-room 5x7
  face, straight-alpha part blits, libpng snapshots) + `raster.h`.
- String-pool trap: dynamic legend buffers die with the art stack frames,
  so TEXT commands copy into a caller pool (both backends replay
  synchronously, but the pool makes the list self-contained to next clear).

## Tests

- `tests/unit/t92_draw_hash.c` (RED: `red-t92.txt`, 72 pin FAILs): all 18
  sections × zooms build non-empty bounded lists; 72 hashes pinned.
  Mutant: 303 rim `PX(176)→177` breaks exactly the 8 303/SYNTH2 pins.
- `tests/unit/t93_raster_goldens.c` (RED: `red-t93.txt`, 74 FAILs): pixel
  hashes for all sections × zooms + skinned 808 (IMAGE path; differs from
  procedural `b89a2b83` vs `c0e692e5`); PNGs in
  `docs/evidence/gui/host-raster/`. Mutant: text baseline shift breaks pins.
- GREEN: `PASS draw_hash`, `PASS raster_goldens`.
- AROS: draw TUs + replayer compile clean (v1 SDK, `-Werror`; two real
  findings: `from_n` orphaned, `blit()` locals orphaned, sign-compare);
  RISECT/RIAPP re-link (234136 / 349360 bytes, 0 UND).
- T8 PNG half closed by the same rasterizer: headless writes
  `/tmp/ri/panel.png` (732×230, hash `30c59a04` == t93 sec0 z0 pin);
  audit pins WAV + PNG determinism.
- Equivalence proof on AROS (captures before/after on riqemu1 + Dell):
  OPEN — needs the lanes; construction is pixel-identical (same font path,
  same pen RGB, same blits, same metrics decisions).
