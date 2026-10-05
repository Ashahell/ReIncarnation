# Tab switch: the rack plate was redrawn per child box — cached in a bitmap; plus housekeeping (2026-10-05)

- Source: ReIncarnation session (advisor lane), 2026-10-05. Commit `3b38c4e` on local branch `claude/tab-switch` (worktree `../ReIncarnation-claude`), based on `eedca38`; not merged or pushed.
- Collected: 2026-10-05
- Published: 2026-10-05
- Related: [2026-10-05-review-latency-instrumentation-and-advice-log.md](2026-10-05-review-latency-instrumentation-and-advice-log.md), [2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md](2026-10-03-the-o2-tab-cost-is-preemption-of-prebuilt-mui-code.md), [2026-10-05-latency-the-three-numbers-the-owner-feels.md](2026-10-05-latency-the-three-numbers-the-owner-feels.md)

## Probe

- `tab_switch()` now writes `TAB page=.. us=.. page_us=.. tabs_us=.. rail_us=.. sec=.. art=.. bay=.. built=.. kept=.. xruns+..` to `RIAPP-EV.LOG`.
- The three phases partition the switch:
  - `page_us`: `MUIA_Group_ActivePage`;
  - `tabs_us`: the five `MUIA_RArt_Active` writes;
  - `rail_us`: `rail_for_tab`, i.e. `MUIA_ShowMe` on the rail buttons.
- The counters show who drew:
  - `sec`: rsection `MUIM_Draw` calls (`ri_rsection_draw_calls()`);
  - `art`: RArt draws;
  - `bay`: RBay `MUIM_DrawBackground` requests;
  - `built`/`kept`: plate commands built vs kept after the clip.
- **Click map re-verified** from a scale-2 Dell capture of the current layout. Tabs sit at y=165 and x = 57 / 127 / 188 / 235 / 287 (SYNTHS, DRUMS, LEVI, MIX, FX). The older map (y=166, x 56/122/180/238/288) still lands inside each tab.

## Finding

- **The cost was the rack plate.** It was not the canvases, and not the tab keys (`tabs_us` 227–373 µs).
- **How the plate is drawn.** `ri_art_bay` draws the brushed plate as one RECT per pixel row plus streaks. RBay's `MUIM_DrawBackground` rebuilt the whole plate for every requested box, then clipped it, then replayed each surviving row as a RectFill through the layer.
- **Before the fix, Dell during Zombie Nation, two cycles:**

| switch to | us | page_us | rail_us | sec | art | bay | built | kept |
|---|---|---|---|---|---|---|---|---|
| DRUMS | 64069 / 32758 | 53551 / 24270 | 10145 / 8116 | 4 | 6 | 18 | 5134 | 2811 |
| LEVI | 41666 / 32570 | 32236 / 26595 | 9060 / 5606 | 1 | 4 | 13 | 3821 | 2650 |
| MIX | 148431 / 177757 | 122075 / 42951 | 26042 / 134494 | 6 | 30 | 59 | 30137 | 12377 |
| FX | 42053 / 38088 | 41721 / 37756 | 5 / 5 | 4 | 12 | 31 | 24015 | 8739 |
| SYNTHS | 36967 / 33209 | 29656 / 26159 | 6991 / 6729 | 4 | 11 | 21 | 5302 | 2832 |

- **Fix.** RBay renders its plate once per size into a friend bitmap (`rbay_plate`), freed on `MUIM_Cleanup`.
  - The plate is drawn at origin (0,0); its grain is keyed relative to the bay's own box, so the pixels are unchanged.
  - Each background request is one `BltBitMapRastPort` of the clamped box.
  - The old per-request build stays as the fallback when the bitmap can't be allocated.
- **After the fix**, `built` is 815 once per plate and `kept` 0. Second cycle:
  - DRUMS 30210
  - LEVI 25544
  - MIX 37079 (first cycle 39626)
  - FX 52194 (first cycle 27501)
  - SYNTHS 37095 (first cycle 61015, with a `rail_us` 36854 outlier)

  Cycle totals were about 314–333 ms before and about 181 ms after. xruns+0 throughout. A capture of MIX shows the plate unchanged.
- **Remaining cost:**
  - `page_us` 20–30 ms: the new page's canvases (`sec` 1–6 full draws). This is legitimate work.
  - `rail_us` 5–37 ms: `MUIA_ShowMe` on the rail power buttons relayouts the whole window (`art` 30 on MIX). Avoiding it means a rail design change, which is the owner's call. Options: a rail page group switched alongside the pages, or dim instead of hide.

## Housekeeping in the same commit

- **Governor: one mechanism.**
  - The 2 s continuous arm (`RI_LIVEDRV_ARM_US`, opencode `3dadda7`) is kept.
  - The 1200‰ per-buffer stall cap (`4167e32`) is dropped; the sample is clamped at 4000 only to keep the smoothing in range.
  - The t88 single-stall case still passes, via the arm.
- **`ri_dlist_set_clip` starts a fresh build.** It empties `n` and the text spool, which closes the "list built twice comes back longer" trap. t155 pins the new contract.
- **Object-level mixed-build gate.**
  - Every RIAPP object in `ri_build_aros.sh riapp` is compiled with `-frecord-gcc-switches`.
  - `.GCC.command.line` is read back with `x86_64-aros-readelf -p`: `engine/` objects must record `-O2`, the rest `-O0`. The build prints `AROS RIAPP MIXED VERIFIED: 29 engine objects at -O2, 52 app/GUI objects at -O0`, and the audit requires that line.
  - Negative control: routing `engine/` to `-O0` fails the build with `FAIL: .../engine.o (engine) not built at -O2`.
  - The misleading `NO2` echo is removed.
  - The -O0 rationale comment now names the owner's debug-build rule (2026-09-28), not GUI latency.
- **Dell environment.** `RIAPP_DIAG` existed only in `ENV:` (this boot), not `ENVARC:`; it is unset now.
- **Verification:** AUDIT 0/0 PASS with paths isolated in the worktree. The probe binary runs on the Dell as `RAM:RIAPPT`; the stick's `Vk4aros:ReIncarnation/RIAPP` is unchanged.

## Method notes

- **Path redirects leaked into a commit once.** The worktree's `/tmp/ri` → `/tmp/ri-claude` redirect leaked into `scripts/ri_audit.sh`, because that file was also an own file. Revert the redirects in every own file before committing, then `git show HEAD | grep -c ri-claude` must print 0.
- **The same anti-pattern, a third time.** The damage-path build was fixed by the item cull, the box-repaint by the bounded build, and now the background by the plate cache. Each was "build the whole thing, then discard everything outside the box". Look for it first wherever a per-box draw request exists.
