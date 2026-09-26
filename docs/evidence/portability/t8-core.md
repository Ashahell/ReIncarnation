# T8 — Portable application core: session move (rest of plan T8)

- Status: GREEN (host) 2026-09-26. Device re-proof open (Dell lane).
- `app/core/riapp_core.h/.c` (new, pure): `RIAppCore` (4 banks, track,
  control plane, live session, 512-event scratch) + `ri_core_init`
  (incl. `ri_live_set_ctl` binding and caller engine mask), `ri_core_demo`
  (moved verbatim: 303 line + 808 beat), bank accessors (clamped),
  `ri_core_play/stop` (null-backend path), `ri_core_meters` (levels +
  sixteenths, 1 on read / 0 on busy-or-null).
- `app/riapp.c` keeps: MUI window/canvases, sync shadows, `rlog`, AHI
  open/run/close + the s_live branch, null-path inline renders, arg parse.
  All session/bank/track/ctl/demo/meter logic now goes through the core;
  init order preserved (demo → AHI open → init at negotiated device rate).
- NOT moved (inherently shell): canvas layout, sync orchestration
  (MUI-derived values), log sink, event pump. The ~900-line estimate
  counted those; the portable part is ~200 lines.

## Tests

- `tests/unit/t95_riapp_core.c` (RED: `red-t95.txt`): bank defaults +
  demo content (16-length, BD/SD/CH hits, step-0 key), play renders
  audibly, core ctl send→drain, meters land, null guards, stop silences.
- GREEN: `PASS riapp_core`.
- Mutant: deleted BD line → `FAIL drum hits 6` (killed). (A `==5u`
  tautology mutant was rejected at compile time by `-Werror` — recorded
  as a process note: mutants must compile.)
- AROS: `riapp.c` + `riapp_core.c` compile clean; RIAPP re-links with the
  core object (349840 bytes, 0 UND).
- Headless (T8 WAV/PNG proof) is unchanged and still green (see audit).
