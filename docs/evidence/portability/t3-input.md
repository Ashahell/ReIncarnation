# T3a — Input normalisation: positional key tables (part of plan T3)

- Status: GREEN 2026-09-26. Full canvas-event move
  (`rsection.mcc.c` hit-test/drag/150-px law/repeat → `app/core/canvas_events.c`)
  is deferred to T3b (this record covers the key tables + `ri_pal_input.h`).
- `platform/pal/ri_pal_input.h`: `RI_KEY_*` aliases pinning every Appendix-E
  raw code `gui/keymap.c` dispatches on (= Amiga raw values, keymap unchanged);
  `ri_pal_key_map` (AROS/host identity) + `ri_key_from_win32` (Set-1 make
  codes, extended `0xE0xx` as `0x100|xx`) + `ri_key_from_sdl` (`SDL_scancode.h`).
- `platform/host/key_tables.c`: pure C99, one table per backend, physical-position
  mapping. Sources: IBM Set-1 (Microsoft docs), `SDL_scancode.h` USB-HID IDs.
  E0 where hardware docs are silent: none — all entries are standard positions.
- Wired into host build (`MOD_gui`); AROS glue keeps calling `ri_key_decode`
  directly (identity — no behaviour change).

## Tests

- `tests/unit/t85_pal_keys.c` (RED: `docs/evidence/portability/red-t85.txt`,
  141 FAIL lines against stub bodies returning 0):
  - AROS/host identity over the 68-code used set;
  - spot positions Tab/Return/Space/Up/Down on all three backends;
  - completeness: every used code reachable from Win32 AND SDL tables;
  - alias pins (`RI_KEY_TAB == 0x42` etc.).
- GREEN: `PASS pal_keys`.
- Lesson (stale-object trap): `ri_build_host.sh test` links the existing
  `OUT/*.o` without rebuilding — after editing `key_tables.c` you must
  `bash scripts/ri_build_host.sh gui` first, or the test re-runs the old
  object (observed: 140 FAILs against already-fixed source).
- Mutant: Win32 Tab `0x0F → RI_KEY_SPACE`: `FAIL win32 tab` +
  `FAIL win32 covers 0x42` (2 FAILs, killed).
- Regression: t70/t71 (keymap/panelui) untouched and still PASS (see audit).
