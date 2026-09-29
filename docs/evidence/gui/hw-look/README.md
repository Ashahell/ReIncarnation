# GUI hardware-look review (2026-09-28)

The owner asked for "as modern as possible" and for synths that resemble the original hardware in colours, knob, button and slider shapes, so that the user feels like working with real hardware.

The images here are host renders at 2x zoom (`after-*` / `before-*`) plus a Dell capture (`dell-after.png`, half scale, ABIv11). They come from the portable display-list layer (`gui/draw/`) that both the AROS canvas and the host rasterizer replay.

## Critical findings (before)

- **Everything was flat.** Single-colour rectangles with a 1-px bevel, no shadows, no depth: it looked like a 1990s form GUI, not an instrument.
- **Knobs:**
  - flat discs with a hairline pointer and thin tick lines, with no skirt, cap, highlight or shadow;
  - the **TB-303 knobs were light grey**, while the TB-303 (and the ReBirth rendering of it) has black knobs with a white indicator.
- **Buttons and keys:** grey rectangles with no moulding, rounded edges or finger feel; the 303 keybed was flat white and black blocks.
- **LEDs:** flat dots, lit and unlit barely distinguishable on some panels, no glow.
- **Faders:** a 2-px line plus a flat rectangle; no groove, ridges or shadow.
- **LED displays:** a flat dark rectangle, no bezel or glass.
- **Panels:**
  - no finish, edge or screws;
  - the 303 panel was a warm beige where the hardware is cool brushed aluminium;
  - the header strips (mixer, FX, pattern) were flat.
- **808:** the sound switches were a flat block; the step keys had a square "lamp" instead of an LED; the instrument-select pointer rendered as a dashed bar.

## What changed

**Colour math** (`ri_art_mix`, `ri_art_shade`, `ri_art_luma`) and **graded discs**, used by:
- **Knob:**
  - a drop shadow on the panel;
  - a darker skirt;
  - a graded body (light top-left, dark bottom-right);
  - a concave cap;
  - a small glint;
  - a solid pointer bar 2·(r/11)+1 px wide;
  - the printed scale as 11 dots (larger at 0, 5 and 10), in a colour that contrasts with the panel.
- **Button/key:** dark moulded outline with softened corners, graded face, top highlight and bottom inner shadow (every step key, pattern key, transport key and lever).
- **LED:** lit = glow halo + graded core + specular; unlit = a dark lens in a bezel with a glint. Used for 303/808/909/transport LEDs, the 808 step LEDs set into the key tops, the 909 legend-box LEDs and the compressor GR row.
- **Fader:** a recessed groove with a lit lip; the cap has a shadow, graded moulding, finger ridges and a white index line.
- **LED display:** black bezel, smoked glass and a top glint (303 EDIT STEP, tempo, bar/start/length, pattern steps, FX values).
- **Panels:**
  - 303 and 909: brushed-aluminium hairlines; the 808 and the mixer/FX/transport/pattern plates: a satin grade;
  - all get rolled edges and corner screws on the three instruments;
  - the header strips are moulded.
- **Instrument details:**
  - 303: panel colour cooler silver (`0xCACCC7`), knobs black (`0x2C2C2C`) with white pointers, ivory graded white keys and glossy ebony black keys;
  - 808: slide-lever sound switches; moulded cream instrument keys with a red lit bar.
- **Display-list capacity:** raised to 24576 commands (the 909 at 2x now emits ~8.2k). AROS: `s_dl_back`; tests t92/t93 likewise.

## AROS replay: exact colours

The shaded art needs arbitrary RGB. On this AROS, `RPTAG_FgColor` is ignored: the first Dell run painted every canvas one flat orange. Rects, spans and axis lines now go through cybergraphics `FillPixelArray` with the exact ARGB on hi/truecolor screens. Text and diagonal lines (pointers, palette colours) keep pens. Palette screens use the nearest `C_*` pen.

## Proof

- Host: t92 (display-list hashes, 72 pins) and t93 (raster pixel hashes, 72 pins plus the skin pin) were **re-pinned deliberately** for this change. `docs/evidence/gui/host-raster/` snapshots were refreshed by t93.
- Dell (ABIv11, 1366x768): the RIAPP panel renders the new look (`dell-after.png`).
- The owner's by-eye verdict is pending.

## Not done (candidates for the next round)

- A real text face: the host rasterizer uses a 5x7 test font; AROS uses the system font. Hardware legends want a condensed sans (Helvetica-like) at fixed sizes.
- Rotary "chicken-head" pointer shapes (the TB-303 uses round knobs with a line, so this is correct for the 303).
- Performance: a 2x redraw of the 909 is about 8k primitives. Knob drags repaint the whole section, so dirty-rect redraws are the next step if drags feel slow on real hardware.

## Mix and FX tabs: rack bay (2026-09-28, second pass)

Owner verdict on the first pass: the Mix and FX panels were fine but sat on window grey, so they did not read as devices.

- Both pages are now a rack bay (`rack_page` in `app/riapp.c`):
  - a dark bay background (Zune penspec `2:r…`, `0x1A1B1E`);
  - a steel rack rail at each side, drawn by a small self-drawing Area class (`RRail`, pens per screen like `RLed`), with rolled edges and slotted holes on a 1U pattern;
  - the modules touch, their tops are aligned like racked units, and the group is centred in the bay.
- Dell captures (ABIv11, half scale): `dell-rack-mix.png`, `dell-rack-fx.png`. The FX capture came from a check build that opens on the FX page, because agent clicks do not reach the Register tabs.

## Rack pass 2: brushed bay, screws, seams, strips per device, power buttons (2026-09-28)

Owner asks after pass 1: give the bay a brushed look, add corner screws and lines between the panels, a fader per active device on Mix, power buttons with an indicator LED in the device rail, and a rail that shows only the active tab's devices.

- **Art in the portable layer** (`gui/draw/art_shared.c`):
  - `ri_art_bay`: a brushed dark plate (row-by-row tones plus longer bright streaks, rolled top edge).
  - `ri_art_rack_rail`: vertical grain, rolled edges, 1U slotted holes, and a screw in the first and last hole, so a screw sits at each corner of the rack.
  - `ri_art_seam`: module edges; two neighbours meet as a dark groove, which gives the separation lines.
  - `ri_art_power`: a round moulded cap whose power glyph is the LED (green and glowing when on, dim when off), a label that dims when off, and a cap that sinks while pressed.
- **Host check:** t93 checks these properties rather than hashes, because the look is still being tuned. The bay is dark, fully covered and brushed; the rail has holes and a screw head; the power glyph is green only when on, the label dims when off, and the pressed state differs. Mutants were killed. `host-rack-furniture.png` is a host preview.
- **App** (`app/riapp.c`):
  - `RBay` is a Group that paints the brushed plate as its own background and for every child without one, clipped to the box it is asked for.
  - `RArt` is a self-drawing Area for rails, seams and power buttons. Both replay through `ri_rsection_replay` (the canvas colour path: exact RGB on truecolor screens).
- **Mix tab:** four strips (TB-303 A, TB-303 B, TR-808, TR-909) sharing one mixer board. The strip headers are now named, and the t92/t93 mixer pins were re-pinned deliberately. A strip hides when its device is switched off, and each strip's meter is fed from the live snapshot.
- **Rail:** Synths shows the two 303 buttons and Drums shows the 808 and 909. Mix and FX show all four, because those pages serve every device.
- Dell captures (ABIv11, half scale): `dell-rack2-synths-rail.png`, `dell-rack2-mix.png`, `dell-rack2-fx.png`. The Mix and FX captures came from check builds that open on those pages.

## S1: No grey left (2026-09-28, GUI round 3)

Owner ask (round-3 prompt §7 S1): nothing on the RIAPP window is stock MUI
grey — the transport surround, the window background, and the stock Zune
tab bar. The whole window must read as one piece of equipment.

- **Art** (`gui/draw/art_shared.c`, `gui/draw/art.h`):
  - `ri_art_tab` (new, host-checked in t93 `tab_checks`): a moulded dark
    key (face `0x3A3C40`), 3 px LED strip above the label (`C_MIX_GREEN`
    lit when active, `C_MIX_GREEN_OFF` dim when not), 1 px latch when
    active, sink + darker face when pressed, palette label (`C_TEXT_INV`
    active, `C_MIX_TEXT` otherwise). `RI_ART_TAB_H` 24; width = text + 2×14
    (in `RArt` `MUIM_AskMinMax`).
  - Ledger (E0): transport keeps its `C_TR_PANEL` satin module colour (an
    instrument finish, not MUI grey) and sits racked in seams on the dark
    plate; tab keys are mouse-only until the owner decides tab keys (#2).
- **App** (`app/riapp.c`):
  - `RArt` gains `RART_TAB` + `MUIA_RArt_Active`; pressed sinks via
    `MUIA_Selected` like the power cap.
  - The stock `MUIC_Register` is gone: pages are a `MUIC_Group`
    `PageMode` group (`s_pages`), tabs are an RBay strip of five `RART_TAB`
    keys (`s_tabs`, `tab_strip`, `tab_switch` with `RIAPP_ID_TAB0+g`).
  - The window root is a vertical `bay_group` (spacing 4, inner 6); the
    transport rides in `rack_slot` seams. Nested RBays (root, rail, strip,
    Mix/FX bays) each paint their own box.
  - Log line adds `tabs=`: `RIAPP panel: tabbed … (open=1 rack=1 tabs=5)`.
- **Tests:** t93 `p_tab`/`tab_checks` (green ≥20 only when on, off 0,
  label contrast ≥60, active≠inactive, pressed≠unpressed). Mutants killed
  (LED always off; pressed ignored). t92/t93 section pins unmoved (no
  section art changed).
- **Proof:** Dell ABIv11 `RAM:RIAPPS1`, `open=1 rack=1 tabs=5`. All five
  pages via page-open variants (agent clicks do not reach tabs until S6):
  `dell-s1-synths/drums/levi/mix/fx.png` plus crop-zoom `dell-s1-tabs.png`;
  host preview `host-s1-tabs.png`.

## S2: Real lettering (2026-09-28, GUI round 3)

Owner ask (round-3 prompt §7 S2): panel legends as printed hardware
legends — a crisp condensed grotesque at fixed sizes, identical on host
and AROS.

- **Face** (`gui/draw/font_legend.{h,c}`, new, in all build lists):
  in-house clean-room bitmaps (no traced font): 1-px monolinear strokes,
  flat-sided O, straight-legged R, G with spur, narrow M. S (cap 5, z0 +
  compact 3), M (cap 7, z1), L (cap 9, z2); full ASCII 32..126,
  proportional advance, 1-px tracking. Uniform `uint16_t rows[12]`
  (bit15 = left; L needs >8 px) — ledgered deviation from the prompt's
  `uint8_t` sketch. `ri_face_width` sums advances (trailing gap kept).
- **Display list** (`platform/pal/ri_pal_draw.h`, `gui/draw/canvas.{h,c}`):
  TEXT carries the face in `pad[0]` (0 = system); `ri_draw_text_face`;
  `ri_dlist_set_face` state read by `ri_art_text_c`; `ri_dlist_hash` covers
  `pad` so face switches move t92 pins. `ri_art_text_at` measures with the
  face when set. `ri_draw_section` sets the face from the zoom.
- **Host** (`platform/host/raster.c`): face glyph blits centred by
  `ri_face_width`, baseline at `cy + cap/2`.
- **AROS** (`gui/widgets/rsection_replay.inc`): per-glyph `BltTemplate`
  from one packed `AllocRaster` plane per face (never `AllocBitMap` with
  NULL friend); `SetDrMd(JAM1)` + exact pens; same placement math as host;
  1-px exact-fill fallback with a one-time `kprintf`; planes freed in
  `ri_rsection_dispose_class`. RArt power/tab labels keep the system font.
- **Tests:** new `t111_legend_face` (pixels, bounds, advance law, width
  sum, uppercase uniqueness, zoom map; mutants killed: zeroed M-A row →
  `pixels 2 ch 65`; narrowed M-A width → `bounds 2 ch 65`). t93
  `textface_checks` ("CUTOFF 303" face-M left edge == `cx - W/2`, right ==
  `x0 + W - 1 - gap`). t92/t93 re-pinned for **every** section + skin
  (all legends moved — expected, verified section list 0..20 × z0..z3).
- **Proof:** host sheet `host-legend-faces.png`; Dell ABIv11 `RAM:RIAPPS2`
  (`open=1 rack=1 tabs=5`): `dell-s2-303.png`, `dell-s2-mix.png` (Mix via
  page-open variant) and crop-zoom `dell-s2-legends.png`. Half-scale text
  is unreadable in captures — the owner reads the Dell.

## S3: Dirty-rectangle redraw (2026-09-29, GUI round 3)

Owner ask (round-3 prompt §7 S3): knob drags repaint only the knob's box
(~8k commands on the 909 today), meters/chase stop refreshing every canvas.

- **Pure** (`gui/panelgeo.{h,c}`, `gui/draw/canvas.{h,c}`):
  - `ri_geo_bbox(g, reg_id, zoom, …)` (+3 margin): union of the knob/rect/
    option/stepper hit shapes plus value-following LEDs carrying the low
    byte; static legends/dividers excluded. `ri_geo_wide(reg_id)`: selectors
    repaint whole sections (bank swaps content, Algo swaps the voice UI),
    transport Loop Start drags Loop Len (`ri_loop_clamp`); 21 wide in all.
  - `ri_dcmd_bbox`/`ri_dcmd_hits_box`: per-op extents (face-aware TEXT,
    IMAGE hits everything, CLIP never); host `ri_raster_replay_box` skips
    misses and clips the rest to the box (same contract as AROS).
- **AROS** (`gui/widgets/rsection.{h,mcc.c,_replay.inc}`): damage state per
  canvas; drag/arrow ids go `DRAWUPDATE`, keys/wide/unknown go full;
  partial path rebuilds the list (cheap), replays clipped into the
  off-screen bitmap (`BltTemplate` only when fully inside, else 1-px
  exact fills; skin sub-blits), blits the box; EClock max/mean per path in
  the diag. `ri_rsection_refresh_box` for meters/chase (old+new step boxes
  with shadows, full fallback).
- **App** (`app/riapp.c`): mixer meters via meter boxes; 808/909 chase via
  step boxes; heartbeat logs `RIAPP draw: full/part max+avg+n` every ~30 s
  (live or not).
- **Tests:** t61 (resolve-all, section containment, +3 literal law, MX
  coverage, wide classification); new t112 parity: 189 controls min→max,
  F0 + box-replay == F1 whole-frame (RED: stubbed bbox → `tested 0
  controls`; margin 0 → 909 + mixer fails; zeroed/widened glyph-adjacent
  mutants in t61/t112). t92/t93 pins unmoved (no art change).
- **Dell:** ABIv11 `RAM:RIAPPS3` (`open=1 rack=1 tabs=5`, AROS link clean
  for RIAPP + RISECT). Idle windows report n=0; window-open full draws
  ~2.2 ms avg / 3.9 ms max (z0). Owner drag of 909 TUNE pending for the
  part-side numbers + feel verdict.
- **Note:** `fba990e` carried the panelgeo/riapp/t61 S3 hunks verbatim
  (sibling sweep, acknowledged there); this commit lands the remainder
  (decls, canvas, raster, AROS shell, audit, t112, evidence).

## S4: Master fader on the Mix tab (2026-09-29, GUI round 3)

Owner ask (round-3 prompt §7 S4): a MASTER strip at the right end of the
Mix row — level fader, L/R meters, Comp switch — that really changes the
output level. Unblocked: the Levi mixer/engine WIP has landed.

- **Live-only path (no format churn):** master strip level rides lane key
  `0x0B50` (`RI_CTL_MASTER_LEVEL`, compile-checked against the lane
  table), a key no lane can ever hold (`ri_auto_allowed` stays shut, t60
  keeps asserting non-automatable). `ri_ctl_send_live` admits exactly the
  live-only set (today: master only); the render task applies it like any
  automation; `ri_live_record_touch` sounds it now but writes no lane.
- **Engine** (`engine/engine.{h,c}`): `master` (default 127) +
  `master_applied` slew + `master_meter[2]`; post-everything P-17 stage
  that skips the multiply at unity (neutral path bit-identical, t81 +
  goldens green); post-master L/R taps (mono twins).
- **Meters** (`engine/live.{h,c}`): `master_peak[2]` in the snapshot
  (init/stop/update paths); GUI feeds both MASTER meter channels.
- **Bridge** (`gui/panelctl.c`): MASTER FADER reg → live-only key (only
  FADER on MASTER; meters/comp ride their old paths).
- **GUI** (`app/riapp.c`): `C_MST` canvas on the shared board
  (`s_panel.mix[4]`), in `val_canvas`, sixth Mix module after a 6 px
  double-seam gap (`rack_page_gap`), never hidden; startup burst adopts
  the panel def (100); MASTER L/R meter boxes + chase skip.
- **Tests:** new `t115_master_live` (plane admit/refuse/drain; unity
  bit-neutral; 64 scales by (64/127)^2 ±0.02 RMS; master peaks publish as
  twins; master touch sounds but writes no lane; cutoff still records).
  Mutants killed (automation drop → ratio 1.0; lane touch → no-send;
  open gate → comp/junk admitted). t60/t81/t82/t83/t92/t93 green.
- **Dell:** ABIv11 `RAM:RIAPPS4` (`open=1 rack=1 tabs=5`, AROS link clean,
  0 UND): MASTER strip renders (fader at 100, meters dark at rest).
  On-device WAV comparison rode the host RMS test + ear proof: no `W`
  key is wired (capture facility exists, no GUI binding — ledgered).
  Owner moves MASTER and judges loudness.
- **E0 ledger:** master level is monitoring, not song data (no live-only
  history exists offline); default 100 (registry def).
- **Owner decisions 2026-09-29** (supersede above where noted): master
  is song data (S4b landed: allow-list `0x0B50`, recorder writes, ATRK
  carries; deviation from the manual p. 72-73); tab keys Ctrl+1..4
  (shipped, this round); zoom Fit default + menu + ENVARC persist (S5);
  font licence, skins gesture/chunk, agent rebuild, furniture skins
  still open.

## Owner visual + wiring pass (2026-09-29, Dell verdicts)

Three small fixes plus a wiring audit, all on the Dell as `RAM:RIAPPS6`:
- Mixer strip names + MASTER read one face above (`dell-v2-mix.png`).
- 909 row padded to the 808 width; right edges coincide
  (`dell-v2-drums.png`, crop-zoom `dell-v2-drums-edge.png`).
- 303 Note/Pause toggle narrowed 132 → 110 Q (`dell-v2-synths.png`).
- t116 proves 189 controls wired end-to-end (21 wide classified);
  transport tempo was display-only and now drives the session.
- Ctrl+1..4 switch tabs (owner-confirmed); a click/pop on the keypress is
  under diagnosis (prime suspect: xrun in the tab-show relayout; evlog
  buffer counts + xruns will confirm).

## S5: auto-fit zoom + zoom choice (2026-09-29, code complete, Dell partial)

- **Fit:** new pure `gui/zoomfit.c` — largest 2/1/0 whose geometry-derived
  content (widest page per tab via `ri_tab_devices`, transport compact,
  rail/tab/root furniture) plus chrome fits the screen; fail-closed 0.
  Hand-computed pins: Mix binds width everywhere (954 content at z0);
  Dell 1366x768 → 0, riqemu1 1280x1024 → 0, 800x600 → 0 fallback,
  1920x1080 → 1 (height-bound), 2560x1440 → 2.
- **App:** Fit measures the frontmost public screen at startup (transport
  stays compact); View menu (1x/1.5x/2x/Fit, checkmarked, fail-soft);
  InitChange + `MUIA_RSection_Zoom` per canvas + ExitChange; choice
  persists in `ENVARC:ReIncarnation/zoom` (PAL PREFS path, ledgered
  deviation from the literal `ENVARC:RIAPP/zoom`); S2 legends follow zoom.
- **Tests:** new `t121_zoomfit` (content sizes, five screens, chrome
  mutation, parse/format). Mutants killed (ascending loop; dropped root
  inner — the latter was a real model bug the test caught). t92/t93 pins
  unmoved (S5 paints no pixels). t60/t61/t70/t76 green.
- **Dell:** ABIv11 `RAM:RIAPPZ5` then `RAM:RIAPPB4` boot live-audio
  clean, log proves `RIAPP zoom: mode=-1 zoom=0 screen=1366x768`
  (`open=1 rack=1 tabs=5`) and `RIAPP zoom: View menu built
  (1x/1.5x/2x/Fit)`; a 29,236-buffer run closed with 0 xruns
  (`render_max=66 us`). `dell-s5b4-synths.png`: Synths at Fit fills the
  screen. `dell-s5b4-mix-fit.png`: Mix at Fit (page-open check-build)
  renders all six strips with headers + MASTER meters.
- **z2 overflow finding (2026-09-29, check-builds, not committed):**
  explicit zoom 2 on 1366x768 (`mode=2 zoom=2` logged, Mix page) opens
  the window screen-clipped (1366x768 per `ui-windows`) and drops
  rail/strip/pages — transport only (`dell-s5z2-mix-broken.png`). The
  Mix-at-Fit capture above exonerates pre-open `tab_switch`: zoom-2
  overflow is the cause. This validates Fit-default; explicit 2x on a
  small screen needs an owner decision (cap at Fit vs ledgered
  overflow), not silent broken controls.
- **Guard (owner 1.5x breakage, same day):** new pure `ri_zoom_clamp`
  (t121 pins: Dell 1.5/2 → 0, HD 2 → 1, QHD 2 keeps, unknown screen
  keeps, bad want → 0) wired into both the menu path and startup, so an
  overflowing zoom can never break the window again — including a
  persisted one. `ri_zoom_parse` tolerates trailing newline
  (shell-written persist files). Host + audit + ABIv1/ABIv11 green;
  Dell deploy pending lane recovery.
- **Correction:** the View menu is a RMB pull-down (stock MUI —
  there is no visible menu bar, so captures can never show it); menu
  construction now follows the canonical nested `MUIA_Family_Child`
  pattern (the `test.c` shape) after `OM_ADDMEMBER` left the strip
  empty. Owner proof pending: right-click the window → View → a zoom;
  persist try across restart; z2 Mix look.
- **E0 ledger:** chrome 32x72 fail-safe generous; furniture width
  estimates never bind; explicit zooms may overflow small screens.
