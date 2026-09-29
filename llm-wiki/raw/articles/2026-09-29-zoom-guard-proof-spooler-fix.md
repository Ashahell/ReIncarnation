# GUI round 3: zoom-clamp guard Dell proof + spooler pairs.json fix

- Source: ReIncarnation repo commits `80fe279`, `e8ca820` (OpenCode), Dell lane session 1, 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-gui-round3-s4b-menu-captures.md`
- Evidence: `docs/evidence/gui/hw-look/` (README, `dell-s5guard-clamp.png`), `tests/unit/t121_zoomfit.c`

## Guard Dell proof (`RAM:RIAPPB4`, ABIv11, 0 UND)

- Persist round-trip works: `ENVARC:ReIncarnation/zoom` holds `1` (1 byte, app format — the owner's 1.5x).
- Startup logs `RIAPP zoom: mode=1 zoom=0 screen=1366x768`; `dell-s5guard-clamp.png` shows a healthy Fit window (transport + rail + tabs + both 303 rows).
- Earlier mystery solved: the persist dir never existed — the 1.5x pick wedged `ExitChange` before the first persist write (no error line, no file); the lane `MakeDir` fixed it. The owner's 1.5x stays stored; every boot clamps to Fit until they pick otherwise.
- `ri_zoom_parse` tolerates trailing newline (shell-written persist files, t121 pins).

## Spooler refused the agent (lane infra, same day)

- Symptom: e6320 dial-ins refused after the host restart.
- Root causes (two): (1) a hand-started `serve` on :9292 conflicted with the `spike-laptop.service` unit; (2) the unit's `ExecStartPre` printf mangled the JSON quotes, so `pairs.json` held `[{name:e6320,spool:/tmp/spike_spool_laptop}]` and the bridge crash-looped (`FATAL: pairs.json ... Expecting property name enclosed in double quotes`).
- Fix: killed the hand-started serve; `pairs.json` is now copied from the stable master `~/.config/spike/pairs.e6320.json` (no inline shell quoting — the 2026-09-27 unit line proved fragile). Service `active`, 1 pair, listening on :9292; agent redialed (session 1) and drained the queued job OK.
- Lesson: never hand-start a second serve on a unit-owned port; keep generated JSON out of shell-quoted unit lines.

## Open (owner)

- S5: RMB View-menu try, z2-Mix look verdict, explicit-2x guard accepted?
- S4b: record -> save -> reload carries; MASTER ear check.
- Click/pop-on-Ctrl cause; tempo/drag/legend verdicts.
