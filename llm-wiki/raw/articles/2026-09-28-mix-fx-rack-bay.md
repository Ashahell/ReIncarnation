# Mix and FX tabs as a rack bay (`8a61acf`)

- Source: ReIncarnation session, 2026-09-28 (Dell E6320 lane, ABIv11)
- Collected: 2026-09-28
- Published: 2026-09-28
- Related: `2026-09-28-gui-hardware-look-pass.md` (first pass), `2026-09-27-rail-led-bitmap-saga.md` (the RLed class pattern)
- Evidence: `docs/evidence/gui/hw-look/dell-rack-mix.png`, `dell-rack-fx.png`, README section "Mix and FX tabs: rack bay"

## Owner verdict that started it

"the panels themselves are fine but they're surrounded by grey which doesn't make them look and feel like devices."

## What changed (`app/riapp.c`)

- `rack_page(mods, n)` builds both pages as: rail | spacer | modules | spacer | rail.
  - The page group's background is `RIAPP_BAY_SPEC` `"2:r1A1A1A1A,1B1B1B1B,1E1E1E1E"` (`0x1A1B1E`), a Zune RGB penspec. Spacers inherit it.
  - Each module sits in a vertical slot `{module, Rectangle}`, so the tops align like racked units and the bay shows below the shorter ones. The module row has `MUIA_Weight 1` against the spacers' 100, so it stays at the tallest module and is centred vertically.
  - The modules touch (group spacing 0).
- `RRail` is a self-drawing Area class like `RLed`: `RIAPP_RAIL_W` 18 px wide with unbounded height, and pens obtained per screen in `MUIM_Setup`. It draws a steel face `0x8E9296`, a highlight `0xC9CDD1` on the left edge and a shadow `0x3E4145` on the right edge. The slotted holes `0x0C0C0D` follow a 1U pattern: three holes per 44 px, with gaps 16/16/12.
  - It is created alongside RLed in `rail_leds_make` and deleted in `rail_leds_drop`. If the class cannot be made, a bay-coloured rectangle takes its place.
- The startup log line now reports `open=` (`MUIA_Window_Open`) and `rails=`.

## Proof

- The ABIv1 build and `ri_audit.sh` (0/0 PASS) ran in a clean worktree at HEAD plus `app/riapp.c`.
- On the Dell (ABIv11, 0 unresolved symbols), the log showed `open=1 rails=1`.
  - Mix capture: a dark bay, rails with holes, and the mixer centred.
  - FX capture (a check build with `MUIA_Group_ActivePage 3`): four units touching, tops aligned, centred.
- `RAM:RIAPPRACK` was left running for the owner's by-eye verdict.

## Lessons

- **Agent clicks do not switch Register tabs.** The events are injected but no page changes, so capture other pages with a throwaway ActivePage build.
- **The first capture after `Run` can show an older instance**, because same-titled windows overlap at 0,0. Capture again, or close stale instances first with `Break <n> C` (RIAPP handles Ctrl-C).
- **All RIAPP instances append to one `RAM:RIAPP.LOG`.** Tell instances apart by a changed log line.
- **Another session's uncommitted Levi work broke the shared-tree AROS build** (`riapp_core.c` stringop-overread). Build and audit in a clean worktree carrying only your own files.
