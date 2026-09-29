# GUI round 3: S4b song-data master, S5 menu fix, Dell captures, z2-overflow finding

- Source: ReIncarnation repo commits `77bc4ea`, `827c149`, `e7d8ef6` (OpenCode, following plan `31694d5`), 2026-09-29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-29-gui-round3-s4-s5-master-zoomfit.md`, `2026-09-29-gui-round3-s1-s3-and-interop-requirement.md`
- Specs/plans: `docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md`
- Evidence: `docs/evidence/gui/hw-look/` (README decision record, `dell-s5b4-synths.png`, `dell-s5b4-mix-fit.png`, `dell-s5z2-mix-broken.png`), `tests/unit/t115_master_live.c`

## S4b: master as song data (`77bc4ea`, owner reversal)

- Owner 2026-09-29 reverses the S4 monitoring E0: master strip level is song data on lane key `0x0B50` (deviation from the manual p. 72-73).
- Allow-list admits `0x0B50` (sorted; ATRK carries it both directions); registry MASTER Level automatable with LEVEL bind on `RI_ROUTE_MASTER` (`auto_id 0x0B50`); panel bridge rides the normal send; `record_touch` records in RECORD (PLAY still sounds without writing); the now-unused live-only plane API (`ri_ctl_live_only`, `ri_ctl_send_live`) is removed.
- TDD RED->GREEN (t60/t77/t115 all FAIL first: 2+1+11); mutants killed (allow-list drop, registry aut drop). t78/t80/t81/t82/t83/t92/t93/t105/t116 green. ABIv1 RISECT+RIAPP clean (-Werror, 0 UND); ABIv11 0 UND; portable OK. Full audit green except pre-existing t75 bg03.
- Dell `RAM:RIAPPB4` boots live-audio clean (`AHI 0x003e0001`, `open=1 rack=1 tabs=5`).
- Owner proofs pending: record master moves -> save -> reload carries; ear check MASTER still moves the whole mix.

## S5 View menu fix (`827c149`)

- Dell capture showed no menu bar; twofold cause: `OM_ADDMEMBER` after creation is not the Zune shape (canonical is nested `MUIA_Family_Child`, verified in `test.c`), and the expectation was wrong anyway — stock MUI menus are an RMB pull-down, never a visible strip, so captures can never show it.
- Fix: `MenuitemObjects` nested in `MenuObject` nested in `MenustripObject` via `MUIA_Family_Child`; notify pattern (`MUIA_Menuitem_Trigger`/`MUIV_EveryTime`, verified against `test.c`) unchanged; plus a build log line.
- Dell `RAM:RIAPPB4` log proves the build, verbatim: `RIAPP zoom: View menu built (1x/1.5x/2x/Fit)`. Prior instance closed `buffers=29236 xruns=0 render_max=66 us`.
- Owner proof pending: right-click the window -> View -> a zoom; persist try across restart.

## S5 Dell captures (`e7d8ef6`)

- `dell-s5b4-synths.png`: Synths at Fit fills the 1366x768 screen.
- `dell-s5b4-mix-fit.png`: Mix at Fit (page-open check-build, uncommitted) renders transport + rail + tab strip + six strips with headers + MASTER meters.
- Lane health notes: a `ui-capture` job earlier failed with `ConnectionResetError: [Errno 104] Connection reset by peer` (agent `e6320`, session 3, empty log); the lane recovered (session 4) and `ui-windows`/`ui-capture` pass since.

## z2-overflow finding (check-builds, not committed)

- Explicit zoom 2 on 1366x768 (`mode=2 zoom=2` logged, Mix page) opens the window screen-clipped (`1366x768` at `0,0` per `ui-windows`) and drops rail/strip/pages — transport only (`dell-s5z2-mix-broken.png`).
- The Mix-at-Fit capture exonerates pre-open `tab_switch`: zoom-2 overflow is the cause.
- Validates Fit-default; explicit 2x on a small screen needs an owner decision (cap at Fit vs ledgered overflow), not silent broken controls.
- Cumulative run history from `RAM:RIAPP.LOG`: `buffers=110972 xruns=8`, then `buffers=29236 xruns=0`, then `buffers=33107 xruns=0` (all `render_max` 65-68 us, `period=5333 us`).

## Open (owner)

- S5: RMB View-menu try, persist across restart, z2-Mix look verdict, explicit-2x guard decision.
- S4b: record -> save -> reload carries; MASTER ear check.
- Click/pop-on-Ctrl cause (evlog buffer counts + xruns); tempo/drag/legend verdicts.
