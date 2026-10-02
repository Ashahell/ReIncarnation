# GUI round 3: S1–S3 landed by OpenCode; interoperability requirement (Ableton)

- Source: ReIncarnation repo commits `2c636e8`, `c8fe22d`, `3d933bd`, `9e7e8c2` (OpenCode, following plan `31694d5`) and `b2c35ee` (Claude), 2026-09-28/29
- Collected: 2026-09-29
- Published: 2026-09-29
- Related: `2026-09-28-rack-pass-2-and-gui-round3-plan.md`
- Specs/plans: `docs/superpowers/plans/2026-09-28-gui-round3-opencode-prompt.md`, `docs/superpowers/specs/2026-09-29-interop-requirement.md`

## S1: no grey left (`2c636e8` art, `c8fe22d` evidence)

- `ri_art_tab`:
  - a moulded dark key `0x3A3C40`;
  - a 3 px LED strip, `C_MIX_GREEN` lit and `C_MIX_GREEN_OFF` dim;
  - a 1 px latch when active and a sink when pressed;
  - a palette label; `RI_ART_TAB_H` 24.
- t93 `tab_checks`: green ≥ 20 only when on, off shows 0 green, label contrast ≥ 60, active ≠ inactive, pressed ≠ unpressed. Mutants killed.
- Dell `RAM:RIAPPS1`: `open=1 rack=1 tabs=5`. There are five tabs, because the Levi session added its own tab (`3bb7a5e`).
- Captures of all five pages came from page-open variants (agent clicks still do not reach tabs).
- Owner question pending: "does anything still look like stock software grey?"

## S2: real lettering (`3d933bd`)

- `gui/draw/font_legend`: a clean-room face in S (cap 5), M (cap 7) and L (cap 9), ASCII 32..126, with 1-px tracking. Rows are uniform `uint16`, because L needs more than 8 px.
- The display list carries the face in TEXT `pad[0]` via `ri_draw_text_face`, with `dlist cur_face`, and the hash now covers the pad bytes.
- `draw_section` sets the face from the zoom; the compact zoom (z3) uses S.
- Host: glyph bits replayed. AROS: `BltTemplate` from `AllocRaster` planes, with a 1 px fallback.
- New test **t111** (pixels, bounds, advance, width, unique uppercase, zoom map; mutants killed) and a t93 text-face parity check.
- **Every section re-pinned** in t92/t93, all 21 sections at all four zooms plus the skin pin, because all legends moved.
- Dell `RAM:RIAPPS2`: 303 and Mix captures.
- Mouse-only tab keys remain (owner decision #2).
- Interim state seen while the session was closed (2026-09-28 ~23:20): uncommitted stub glyph tables with every legend already switched to the face, which would have blanked all labels. It was not committed from outside the session, and the session later landed the tables itself.

## S3: dirty-rect redraw (`9e7e8c2`, with S3 hunks carried by `fba990e`)

- **Pure code:**
  - `ri_geo_bbox`: the union of the knob/rect/option/stepper/LED boxes for one control, plus 3 px;
  - `ri_geo_wide`: 20 selectors, plus Loop Start (which drags Len);
  - `ri_dcmd_bbox` / `hits_box`;
  - host `ri_raster_replay_box`.
- **AROS:**
  - per-canvas damage; `DRAWUPDATE` for drag and arrow ids, while keys and "wide" controls still redraw in full;
  - clipped partial replay into the off-screen bitmap, then a blit of only the damaged box;
  - EClock max/mean per path in the diag;
  - `refresh_box` for meters and chase lamps (old and new step boxes).
- **Tests:**
  - t61, plus new **t112**: for 189 controls driven min→max, F0 + box replay == F1.
  - Mutants killed: margin 0; zeroed or widened table rows.
- **Dell `RAM:RIAPPS3`:** idle windows n=0; full draws at open average ~2.2 ms, max 3.9 ms (z0). The owner's 909 TUNE drag, for partial-draw numbers and feel, is pending.
- All S1–S3 audits used a private `/tmp/ri`, as the plan's §4.9 requires.

## S4 in progress (uncommitted as of 2026-09-29)

The working tree shows the master-fader work: `engine/seq/ctlplane.*`, `engine/live.*`, `engine/engine.*`, `gui/panelctl.c`, `app/riapp.c` and a new `tests/unit/t113_master_live.c`, for the live-only control path.

## Interoperability requirement (`b2c35ee`)

The owner asked "can ReIncarnation work with Ableton?". The answer was: not directly today; MIDI sync across machines, then Link or a plugin after a Windows/macOS port.

The owner then had it documented (`docs/superpowers/specs/2026-09-29-interop-requirement.md`).

**Today:**
- MIDI clock only drives the Sync LED (p. 145); the tempo does not follow it.
- Nothing is transmitted (the G7 receiver "never transmits", p. 127).
- WAV comes only from the offline renderer and live **W** capture.
- There is no MIDI file support, no Link and no plugin.

> **Update 2026-10-02 (still true of the shipped app, but the gap is now half-built).**
> P1's *tempo does not follow it* line above is still accurate for what RIAPP does today —
> nothing applies a follower's decision. What changed is that the decision source now exists:
> `midi_io/midi_follow.{c,h}` (`47debed`, `50dc90a`, `622907d`) is a pure clock-in follower
> core, a byte-level realtime wire parser and a sync-source state machine, all tested on the
> host and all returning *intents* for a caller to apply. CAMD wiring and transport
> application are the open half. The `midi.h` `midi_clock_*` / `midi_mmc_cmd` pair this
> article's spec quotes as "neither is wired" is still unwired and untouched — there are now
> two clock models, and only the follower one can drive tempo.
> See [MIDI interop R1: the clock-in follower core, the realtime wire parser, and the sync-source state](2026-10-02-midi-interop-r1-follower-core-wire-parser-sync-source.md).

**Requirements by priority:**
- **P1:** MIDI clock in (follow: 24 ppqn, Start/Continue/Stop/SPP, jitter-smoothed, phase-locked, dropout timeout, latency offset); clock out (lead, latency-compensated, a sender task only); MMC in/out; settings and LEDs.
- **P2:** per-device note in and out (separate from the one-channel G7 remote); CC out using the Appendix C numbers; SMF type 1 export/import.
- **P3:** stem export per mixer strip plus FX returns and master; loop-exact renders.
- **P4 (after the port):** Ableton Link (UDP multicast; AROSTCP is fragile; licence GPLv2+/proprietary, or a clean-room protocol); VST3/AU/CLAP plugin (host transport, control IDs as parameters, RBNG state, block adaptation); optional RTP-MIDI.

**Not possible:** ReWire (discontinued; Ableton removed it in Live 11).

**Constraints:**
- the realtime contract: no I/O in the render path;
- Classic unchanged by default;
- MIDI flood, hot-unplug and dropout robustness;
- determinism: either reproducible offline or marked live-only;
- the device-instance keying of the extensible rack.

**Owner decisions (7):**
1. The classification of the out features (Classic extension or W3).
2. Whether to record the followed tempo history.
3. Where sync settings live.
4. The SMF mapping.
5. The Link licence.
6. Plugin formats and licences.
7. The priority against GUI round 3 and the new devices.
