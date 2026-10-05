# The rail becomes a page group (no relayout on tab switch); RIAPP log lines end in a newline; stick deploy (2026-10-05)

- Source: ReIncarnation session (advisor lane), 2026-10-05. Commit `d1ebf16` (branch `claude/rail-pages`) on top of `cc0b565`; deploy of `cc0b565` to the Dell stick.
- Collected: 2026-10-05
- Published: 2026-10-05
- Related: [2026-10-05-tab-switch-bay-plate-cache-and-housekeeping.md](2026-10-05-tab-switch-bay-plate-cache-and-housekeeping.md), [2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md](2026-10-02-dell-lane-bare-path-launch-wedges-agent-and-ui-capture-hangs.md)

## Stick deploy (cc0b565)

- Built with `scripts/ri_build_v11.sh . <out>` from the repo checkout. Building from a `git archive` export stamps `build=?`, because there is no `.git` to hash.
- `Vk4aros:ReIncarnation/RIAPP` = `cc0b565`, 1047464 B, r12moves=193, mixed build (29 TUs at -O2). The previous binary is kept as `RIAPP.prev`.
- The startup log reads `RIAPP LOG build=cc0b565 diag=0`, 909 pack 11 voices, AHI 48000 Hz / 256 frames.
- The logs now live on the stick: `Vk4aros:RIAPP.LOG` and `Vk4aros:RIAPP-EV.LOG` (see `docs/lane/envarc.md`: `RIAPP_LOG` / `RIAPP_EVLOG`). They are no longer in `RAM:`.

## Rail as a page group (owner decision 2026-10-05)

- **Before.** `rail_for_tab` toggled `MUIA_ShowMe` on the five power buttons. That relayouted the whole window: `rail_us` 5–37 ms, outliers up to 134494 µs on MIX.
- **Change.**
  - `tab_rail` builds a `MUIA_Group_PageMode` group with one RBay page per tab. Each page holds its own copies of that tab's power buttons (`s_railbtn[tab][dev]`); SYNTHS has 303A/303B, DRUMS 808/909, LEVI Levi, MIX and FX all five.
  - Every copy notifies the same `RIAPP_ID_DEV0 + d`.
  - `rail_leds_show` mirrors the LED into every copy.
  - `s_devbtn[d]`/`s_devled[d]` keep the first copy for the existing checks.
  - `rail_for_tab` only sets the rail's `MUIA_Group_ActivePage` to the main page.
- **Dell, one cycle during Zombie Nation:**

| switch to | us | page_us | rail_us |
|---|---|---|---|
| DRUMS | 24832 | 21780 | 2764 |
| LEVI | 23632 | 20614 | 2734 |
| MIX | 31980 | 29897 | 1852 |
| FX | 42402 | 37545 | 4614 |
| SYNTHS | 34803 | 30749 | 3815 |

  The cycle took about 158 ms (about 181 ms after the plate cache alone; about 320 ms before both).
- **Verification:**
  - Captures of each tab show the same rail contents as before.
  - Power toggle from the SYNTHS copy: `VIS dev=0 show=0 mask=1e`, and the MIX page then draws 5 canvases (303A strip hidden). The capture shows the MIX copy's 303A LED dark.
  - Toggle back from the MIX copy: `VIS dev=0 show=1 mask=1f`.
- **Remaining tab-switch cost:** `page_us` 20–37 ms, i.e. the new page's canvases (`sec` 1–6 full draws). That is the legitimate floor.

## Log-line newlines

- **Bug.** Five `rlog` formats in `app/riapp.c` lacked a trailing `\n`: `RIAPP stg:`, `RIAPP stg[%d]`, `RIAPP dstg n=`, `RIAPP dstg %-6s` and `RIAPP vcount:`. On the Dell they ran together into one line.
- **Fix.** All five now end in `\n`.
- **New audit gate.** After the RIAPP build, an embedded python3 scan fails the audit on any `rlog(` format literal in `app/riapp.c` that does not end in `\n`. Negative control: removing the `\n` from `RIAPP dstg n=%lu` is caught at line 3172.
- **Verification:** AUDIT 0/0 PASS (isolated paths), including `MIXED VERIFIED: 29/52` and the new gate line.

## Lane notes

- **The capture trap struck again.** `--ui-capture` hung the agent mid-job (11 actions, one capture among them) for more than 10 minutes. It needed an owner agent restart, and the Dell came back rebooted.
  - Rule kept: one capture per job, never in the middle of a long action list.
  - Prefer ev-log evidence (`TAB`, `VIS`) over pictures where it suffices.
- **Tab click map holds on the current layout:** y=165; x = 57 / 127 / 188 / 235 / 287. The 303A power button on the rail is at (37,125).
