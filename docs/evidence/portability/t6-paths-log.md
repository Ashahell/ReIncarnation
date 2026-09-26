# T6 — Paths, dir scan, logging (portability plan §3.6)

- Status: GREEN 2026-09-26.
- Interfaces: `platform/pal/ri_pal_fs.h` (`RI_PATH_MODS/SONGS/TEMP/PREFS`,
  `ri_pal_path` / `ri_pal_path_join` / `ri_pal_list_dirs` /
  `ri_pal_read_file` / `ri_pal_write_file`), `platform/pal/ri_pal_log.h`
  (`ri_log` variadic, `ri_log_format` vsnprintf core, `ri_pal_log_sink`).
- Backends:
  - host (`platform/host/fs_host.c`, `log_host.c`): stdio + dirent + stderr;
    wired into `MOD_gui`, host-tested.
  - AROS (`platform/aros/fs_aros.c`, `log_aros.c`): DOS `Lock`/`ExAll`
    (fixed `ED_TYPE` / `ed_Next` / `ExAllEnd` walk), `Open`/`Read`/`Write`,
    `PutStr` sink. AROS compile-only gated (see audit).
- Call-site moves (all 8 hard-coded literals gone; gate pins zero):
  - `app/sectproof.c`: `mod_log` + both `RISECT.LOG` traces via a
    `pal_temp_file` helper; `skin_apply` MODS prefix + `skin_scan` Lock via
    `ri_pal_path(RI_PATH_MODS)` (scan keeps its Classic-first/16-cap logic;
    full `ExAll`→`ri_pal_list_dirs` migration deferred to device proof).
  - `app/riapp.c` `rlog`: variadic now, formatted by `ri_log_format`
    (vsnprintf) and sunk preformatted — the RawDoFmt 32-bit packing trap is
    gone; log file via `RI_PATH_TEMP` + `RIAPP.LOG`. Callers pass numbers
    only (verified); `%lu`/`%lx` values are identical under vsnprintf.
  - `audio_io/audio_ahi_play.c`: `AUPLAY_TMP` define replaced by
    `auplay_tmp()` (`RI_PATH_TEMP` + `auplay.wav`), used for render, open
    and delete (the delete site was a third use found by the AROS compiler).
- `stdio` users (`rbng.c`, `rbnm.c`, `pcf.c`) stay on stdio (plan: only path
  construction moves; they take caller paths already — no change needed).

## Tests

- `tests/unit/t86_pal_fslog.c` (RED: `docs/evidence/portability/red-t86.txt`
  — the 8-site gate list; code RED was the gate itself):
  join (incl. trailing-slash + truncation + NULL), read/write round-trip +
  miss, `list_dirs` find + miss, `log_format` parity vs `snprintf`
  (`n=%d s=%s u=%lu x=%lx`, incl. 3000000000ul and 0xdeadbeef) + truncation.
- GREEN: `PASS pal_fslog` (wired). Standalone run also passed pre-wiring.
- Mutant: host join separator `'/' → '\\'`: `FAIL join /tmp/ri\x.wav` (killed).
- AROS: `fs_aros.c`, `log_aros.c`, `audio_ahi_play.c`, `riapp.c`,
  `sectproof.c` compile clean under the v1 SDK (`-Werror`, r12 ABI flags).
- Audit: `t86_pal_fslog` + path-literal gate + AROS compile lines added.
