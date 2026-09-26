# T8 — Portable application core: headless proof (partial)

- Status: PARTIAL 2026-09-26 (WAV GREEN; panel PNGs + `riapp_core` session
  move open, both need the T2 canvas).
- `platform/host/main_headless.c` (new): demo fixture (303 pattern + 808
  hits, 120 BPM) through the portable live driver in 256-frame chunks,
  pulled by the host null backend into `/tmp/ri/null.wav` (2 s, 48 kHz).
  Exit != 0 unless 375 buffers and 0 xruns.
- Result: `headless: /tmp/ri/null.wav rate=48000 frames=256 buffers=375
  xruns=0`; WAV 384044 bytes, peak 19168 (audible), sha256
  `a9ec1715…f9ec` stable across runs (deterministic).
- `build/portable.mk`: `headless` target builds the core + runs it
  (`make -f build/portable.mk headless` GREEN); `test` runs t84.
- Audit wires the same: build + run + `xruns=0` + re-render `cmp`
  (determinism gate).
- Open: `app/core/riapp_core.c` (session wiring, demo song, key bindings,
  recording, meter publishing, section layout from `riapp.c`/`sectproof.c`),
  shrinking the AROS shells to window + event pump; headless panel PNGs
  via the T2 host rasterizer.
