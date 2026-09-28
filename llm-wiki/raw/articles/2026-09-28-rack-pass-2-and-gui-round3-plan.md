# Rack pass 2 (`48adb49`) and the GUI round-3 OpenCode plan (`31694d5`)

- Source: ReIncarnation session, 2026-09-28 (Dell E6320 lane, ABIv11)
- Collected: 2026-09-28
- Published: 2026-09-28
- Related: `2026-09-28-mix-fx-rack-bay.md` (pass 1), `2026-09-28-gui-hardware-look-pass.md`
- Evidence: `docs/evidence/gui/hw-look/` (README section "Rack pass 2", `dell-rack2-*.png`, `host-rack-furniture.png`)

## Owner asks after pass 1

"the flat black is a step in the right direction". The owner then asked for:
- a brushed look;
- screws in the corners like the synths;
- separation lines between the panels;
- a slider per active module on Mix;
- rail buttons that look like power buttons with an indicator LED;
- a rail that shows only the active tab's buttons.

## What was built

**Portable art** (`gui/draw/art_shared.c`, `art.h`):
- `ri_art_bay`: a brushed dark plate `0x2A2B2F`. Each row gets a hashed tone, and a few longer bright streaks are keyed to the bay's own box.
- `ri_art_rack_rail`: vertical grain, rolled edges, 1U holes and a screw in the first and last hole.
- `ri_art_seam`: module edges; two neighbours form a dark groove.
- `ri_art_power`: a moulded cap whose power glyph is the LED (`C_MIX_GREEN` with a glow when on, dim `0x44524A` when off). The label dims when off and the cap sinks 1 px while pressed.
- Constants: `RI_ART_RAIL_W` 18, `RI_ART_SEAM_W` 2, `RI_ART_POWER_H` 26.

**t93 `rack_checks()`:** property checks rather than hash pins, because the owner is still tuning the look.
- The bay is fully covered, has a mean luma < 70 and shows ≥ 40 tone changes down a column.
- The rail holes are `0x0C0C0D` and a screw head is visible.
- The power glyph is green only when on, the label is bright only when on, and the pressed hash differs from unpressed.
- Mutants were killed: the off glyph forced green, and the bay made flat.

**Mixer strip names:** the headers now read "TB-303 A", "TB-303 B", "TR-808" and "TR-909" (they were all "MIX"). The t92/t93 pins for sections 4–7 were re-pinned deliberately: 16 pins each.

**`ri_rsection_replay(rp, dl)`:** the canvas replay loop, made public. App art now uses the same exact-RGB path. It needs a section canvas set up on the same screen, which supplies the pens and the direct-RGB switch.

**App classes** (`app/riapp.c`):
- `RArt` is an Area subclass for rails, seams and power buttons. The power button uses `MUIA_InputMode RelVerify` with `MUIA_ShowSelState FALSE`, and redraws on `MUIA_Selected`.
- `RBay` is a Group subclass that overrides `MUIM_DrawBackground`. It builds the whole bay list and `art_clip`s it to the requested box.
- The old `RLed` class is removed; the power glyph replaces it.

**Mix tab:**
- The canvases are `C_MXA`, `C_MXB`, `C_MIX` (808, owns the board) and `C_MX9`, bound with `ri_sui_bind_board`.
- `s_mixslot[d]` hides a strip with its device.
- `meter_round` feeds all four strips; the 303B and 909 meters come from `ri_live_meters_read` `sec_peak[1]`/`[3]`.

**Rail per tab:** a Register notify on `MUIA_Group_ActivePage` returns `RIAPP_ID_TAB`, and `rail_for_tab()` sets `MUIA_ShowMe`.
- Synths shows 303A/303B and Drums shows 808/909.
- Mix and FX show all four, because those pages serve every device.

## Proof

- ABIv1 `ri_build_aros.sh riapp` OK. ABIv11 Dell build: 0 unresolved, 420848 bytes.
- Dell log: `open=1 rack=1`.
- Captures:
  - the Synths rail shows only the two 303 buttons;
  - Mix shows four named strips on the brushed bay with seams and rails;
  - FX shows the rail screws.
  - Mix and FX were captured from check builds that open on those pages.
- Audit `AUDIT 0/0 PASS`. `RAM:RIAPPPWR` was left running for the owner.
- **Not verified:** clicking power buttons (agent clicks do not reach the app). The owner is to check toggling and strip hiding by hand.

## Lessons

- **The shared `/tmp/ri` build dir holds another session's stale objects.** The audit's portable gate failed with `undefined reference to levi_trigger`, although HEAD's `engine.c` has none. Fix: run the audit with a private `/tmp/ri`:

  ```
  unshare -r -m sh -c "mount --bind <scratch>/ri /tmp/ri && bash scripts/ri_audit.sh"
  ```

  It then passed.
- **The shared tree build broke on foreign WIP** (a `riapp_core.c` stringop-overread `-Werror`). Build and audit only in a clean worktree at HEAD plus your own files.
- **The Vulkan4Aros main checkout was on another session's branch** (`t8-workgroup-size`). Wiki cross-posts go through a worktree of `origin/main`.

## GUI round 3 plan for OpenCode (`31694d5`)

`docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md` (730 lines) covers seven steps:
- **S1:** no grey. The window root becomes an RBay, the Register is replaced with an RArt tab strip plus a PageMode group, and there is a new `ri_art_tab`.
- **S2:** a clean-room bitmap legend face in S/M/L sizes by zoom. The face id is carried in `ri_dcmd.pad[0]`; AROS draws it with `BltTemplate` from `AllocRaster` planes.
- **S3:** dirty-rect redraw, via `ri_geo_bbox` per control plus a partial replay of the intersecting commands into the off-screen bitmap. A host parity test checks that full equals partial for every control.
- **S4:** a master fader, through a live-only control path. It waits for the Levi WIP files to land.
- **S5:** `ri_zoom_fit` auto-fit, a View menu for zoom, and the choice kept in `ENVARC:`.
- **S6:** lane clicks. The Dell agent's `atcp_ui_click_at` sends pointer-pos, down and up in one chain with zero timestamps. The plan is a probe app, fixed on the server side first; the agent is redeployed only with the owner's OK.
- **S7:** per-section skins as designed. The song chunk needs the owner's approval.

The plan's §4 is the GUI working method. The owner decisions it lists are:
- font licence;
- tab keys;
- zoom default and persistence;
- master level as a live-only control;
- skins: the Ctrl+M gesture and the song chunk;
- agent redeploy;
- skinnable furniture.
