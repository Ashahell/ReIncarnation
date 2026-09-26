# T4 — Audio split: portable live driver + thin AHI backend

- Status: GREEN 2026-09-26.
- `app/core/live_driver.h/.c` (new, pure C99): owns the double-buffer
  policy, f32→s16 conversion (`ri_livedrv_f32_to_s16`: clamp,
  round-half-away, no libm — twin of the removed AROS inline), xrun
  accounting, the transport request word, W capture and render timing.
  Time comes from an injected `now_us` clock (dependency inversion — no PAL
  change needed; `ri_time_us` lands in T8). All storage caller-owned.
- `audio_io/audio_ahi_live.{h,c}` keeps only AHI objects, the
  SoundFunc/PlayerFunc→`Signal` hooks, the render Task, the handshake and
  the EClock→`aros_now_us` wrapper. `render_half` is one driver call into
  the free `s_pcm` half. `AuLive` embeds `struct RILiveDriver` (its own
  cmd/xrun/buffer/render/cap atomics deleted); `au_live_request`/
  xrun-report/`au_live_run` handoff map onto the driver; the AROS f32→s16
  twin is deleted (t88 pins the portable one). `riapp.c` log reads
  `s_lv.drv.*`.
- Host: `platform/host/audio_null.c` — synchronous deterministic pull loop
  (float interleaved stereo) writing 16-bit WAV (`RI_NULL_WAV`,
  `RI_NULL_SECONDS`, defaults `/tmp/ri/null.wav`, 2 s); `late()` = 0.
  (T11 WASAPI goes async event-driven; the interface already allows it.)
- Wired: `MOD_core` (+ `core` target, in `all`), `audio_null.c` in
  `MOD_audio`; `live_driver.c` in the AROS `riapp` link.

## Tests

- `tests/unit/t88_live_driver.c` (RED: `red-t88.txt`): converter law spots
  (0/±1/0.5/clip); driver chunks at 64/128/1024 == offline float render
  through the same converter (t81 law, `memcmp` bit-exact); transport
  play/stop (stop renders silence); `cap == out`; xrun counter; fake-clock
  timing (`render_us_max == 111`); buffer count law.
- `tests/unit/t90_pal_audio_null.c`: two renders byte-identical (FNV +
  size), WAV header size law (44 + 2 s × 48 k × 2ch × 2B), `late() == 0`.
- GREEN: `PASS live_driver`, `PASS pal_audio_null`; t81/t6 neighbors PASS.
- Mutant: converter `+0.5 → +0.4`: `FAIL conv half` (killed).
- AROS: `audio_ahi_live.c`, `riapp.c`, `live_driver.c` compile clean
  (v1 SDK, `-Werror`); RIAPP re-links (see audit).
- Dell check (G9.4 by ear + 0 xruns) still open — owner lane; no behaviour
  change by construction (same session calls, same converter, same halves).
