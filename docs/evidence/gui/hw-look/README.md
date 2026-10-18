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
  font licence DECIDED 2026-09-29 (in-house face kept, below), skins gesture/chunk, agent rebuild, furniture skins
  still open.
- **Owner approvals 2026-09-29 (all approved):** zoom-clamp stands
  (overflowing picks clamp to Fit); RMB View menu works; S4b
  record → save → reload carries master moves; MASTER moves the whole
  mix by ear; tempo follows audibly; 909-drag smooth; legends sharp; no
  grey left; click/pop on Ctrl+keys resolved. GUI round 3 CLOSED.
- **Owner decision 2026-09-29: font.** Keep the in-house clean-room face
  (§9 item 1 closed, E0 stands); no third-party face licensed.

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
  (shell-written persist files). Host + audit + ABIv1/ABIv11 green.
- **Guard Dell proof (2026-09-29, `RAM:RIAPPB4`):** persist round-trip
  works (`ENVARC:ReIncarnation/zoom` holds `1`, owner's 1.5x); startup
  logs `RIAPP zoom: mode=1 zoom=0 screen=1366x768` and
  `dell-s5guard-clamp.png` shows a healthy Fit window. Earlier mystery
  solved: the persist dir never existed (app crashed/wedged in
  `ExitChange` before first persist); the lane `MakeDir` fixed it.
  Note: the owner's 1.5x choice stays stored — every boot clamps it to
  Fit until they pick otherwise.
- **Correction:** the View menu is a RMB pull-down (stock MUI —
  there is no visible menu bar, so captures can never show it); menu
  construction now follows the canonical nested `MUIA_Family_Child`
  pattern (the `test.c` shape) after `OM_ADDMEMBER` left the strip
  empty. Owner proof pending: right-click the window → View → a zoom;
  persist try across restart; z2 Mix look.
- **E0 ledger:** chrome 32x72 fail-safe generous; furniture width
  estimates never bind; explicit zooms may overflow small screens.

## S6 lane clicks + S7 skin proof (2026-09-29, Dell)

- **Cause (probe RED):** the click chain sent pointerpos inside
  `IND_ADDEVENT` (which ignores every class but RAWMOUSE/RAWKEY —
  pointer never moved), omitted the button qualifier (Intuition
  delivered code 0, dropped the release), and serialized releases as
  JSON `true` (digit-only parser read `up=0`: every release a second
  press). Fixed in the agent (moves via `IND_WRITEEVENT`, qualifier
  mirrored, both bool parsers); server gained `--ui-press/--ui-release`
  and a `qQ` qualifier on `--ui-rawkey` (Vulkan4AROS `72a3fe4a`…`4f03fd7d`).
- **Probe GREEN:** press `0x68` + release `0xE8` at the targeted
  position (`log-bool.txt` pattern).
- **Acceptance, no owner input:** tab clicks switch pages (Synths→Drums
  `TAB page=1`, Mix→Synths single click); rail power toggles 303B
  (`VIS dev=1 show=0 mask=1d` / `show=1 mask=1f`); knob drag emits
  `CTL 0104=0 → 25`. Clicks need the window active (first click
  activates); tab x-map (evlog-grounded): 70/130/194/250.
- **S7 rendering** (seeded whole-panel 808-RI check-build, uncommitted):
  `dell-s7-808ri-synths/mix/drums.png` — dark 808-style strips with
  amber headers where bound; Classic Levi/MASTER (no `mix-levi` part,
  stale `bg08`) and Classic 909 (`bg03` stale, t75) beside them =
  per-part fallback visibly working. Ctrl+M mechanics proven via evlog
  (`SKIN sync=` lines) + host t70/t71.
- **Lane notes:** two agents on one pair ping-pong every ~30s (mutual
  redial); `--bye` the holder to drain to one; Break won't kill agents;
  `RAM:CLICK.LOG`-style files lock while open (Break/close first, then
  get); e6320 needs a manual redial after every reboot; a private
  pair+port (`e6320x`/9294, `spike-s6.service`) isolates proof traffic.

## Post-reboot lane repair + S6 acceptance on one agent (2026-10-18)

- **Root cause of the dead lane (not the reboot):** every agent on the
  Dell predated the bounded exec lane. Both servers refuse
  `exec_result` without `proto>=2`
  (`ProtocolErr: … predates the bounded exec lane`), so *all* shell/UI
  jobs failed on both lanes while file `get`/`put` kept working — the
  exact split that hid it. Builds seen: `SYS:ATCPBIN` 121,864 B
  (`556c72c3…`) and `RAM:ATCPNEW` 119,136 B (`fe1fc3ea…`, the approved
  click-fixed build, which still had no `proto`).
- **Fix:** rebuild from `origin/main` **with** `-DATCP_BOUNDED_EXEC=1`
  (`scripts/build_atcpbin_fix.sh`, 0 unresolved GLOBAL UND) →
  144,752 B, md5 `1bb124840131e0afb70ba3a03f48025a`. That tree already
  contains the click fixes (`4f03fd7d` is an ancestor of main), so
  one binary serves input + exec.
- **Persistence:** `S:Startup-Sequence` executes `S:User-Startup`,
  which runs `Run SYS:ATCPBIN agent 192.168.1.81 9292 e6320` — so the
  boot-visible name is `SYS:ATCPBIN`, not `S:`. Copied the new build
  there (`Copy RAM:ATCPX SYS:ATCPBIN`, read-back hash verified
  identical) and restarted it: next boot dials the capable agent with
  no hand work.
- **S6 acceptance, main lane, persisted binary:** tab clicks
  `TAB page=1` → `page=0` → `page=3` (Mix capture
  `dell-s7-mainlane-mix.png`); rail power toggle
  `VIS dev=2 show=0 mask=1b` → `show=1 mask=1f` → `show=0`, render
  follows (`dell-s7-mainlane-rail-toggle.png`); knob drag `CTL 021b=8`.
  Coordinates re-grounded on this build: tabs y≈167 (Synths 70,
  Drums 130, Mix 250), rail y≈128 (303 x≈33, 909 x≈100), 808 CUT OFF
  knob ≈(420,256).
- **Lane hazard (self-inflicted, recorded):** a directory `get`
  (`--get SYS:`) wedged the old agent's command lane — `ping` stopped
  answering and only an owner restart recovered it. On these agents use
  plain file `get`/`put`; list directories with `exec "List …"`, which
  only the bounded-exec build can answer.

## S7 skin cycle closed: one Ctrl+M, one skin (2026-10-18)

- **Four presses, four lines, no oscillation** (this is what the earlier
  1789-line run failed to show): `Classic → 808-RI → Template → Stale →
  Classic`, each `SKIN sync=… current=…`. Press+release back-to-back in
  one job is the whole requirement; the old oscillation was a held key
  repeating at ~3 Hz plus interleaved owner input, not a code defect.
- **Captures** (Drums page, 808 section focused via a click at (700,200)):
  `dell-s7-cycle-A-808RI.png`, `-B-template.png`, `-C-stale.png`,
  `-D-classic.png`. Pixel diffs: A↔B differ (4,944 px), B↔D identical,
  A↔C identical.
- **The identical pairs are explained, not defects:**
  - `Template` holds only `Skin.manifest` (no images) → every part falls
    back to Classic, so it *must* render identically (fallback proven).
  - `Stale/bg02.png` is byte-identical to `808-RI/bg02.png`
    (md5 `6146b2fcfe0ef164211cbb160b35e134`) → identical render is
    correct; the two sets are duplicates on disk.
  - `Classic` is the built-in fallback, not a Mods directory: `Mods/`
    holds exactly `Stale`, `Template`, `808-RI`, and the app reports
    `installed=4` (3 dirs + Classic).
- **Acceptance script:** `docs/evidence/gui/lane-click-proof.md` — the
  coordinate map, the three acceptance jobs, the skin-cycle job, the
  three lane failure signatures (refused exec = wrong build; silent ping
  = wedged lane; no session = nobody dialed) and the traps.

## RIAPP now boots with the machine (2026-10-18)

- Guest install: `SYS:Classes/ReIncarnation/RIAPP` (1,019,528 B, read-back
  md5 `6b76833e5d0b1285492f84c14bb1d1c9`, identical to the host build) and
  `SYS:WBStartup/RIAPP` (the script, kept in the repo as
  `docs/evidence/gui/riapp-wbstartup.script` (it is a guest artifact, not a
  shared build script, so it lives outside `scripts/`'s allowlist gate)).
- **Why WBStartup and not `S:User-Startup`:** the startup-sequence runs
  User-Startup *before* Wanderer, so a GUI app launched there has no
  Intuition to open a window on. Wanderer runs `SYS:WBStartup` scripts
  after the desktop is up.
- Prefs need no help: `ENVARC:ReIncarnation/zoom` already exists, and the
  startup-sequence copies `ENVARC:` into `ENV:` every boot. The script's
  `MakeDir` guard only covers a first boot with no archive.
- Proven without a reboot by running the boot path itself: `Execute
  SYS:WBStartup/RIAPP` started `Process 11`, the `RAM:RIAPP` instance was
  broken, and the survivor answers lane clicks (`TAB page=1`) and renders
  (`dell-bootstart-sys.png`).
- **CORRECTION (2026-10-18, later the same day): there was no guard at
  all.** The first reading of this was "a single-instance guard warns but
  does not enforce": a requester appeared over the second instance and the
  app carried on. Reading the source settled it — no such guard exists.
  That requester is the **audio-failure box**: the second instance got as
  far as opening its window and then failed to open `ahi.device`
  exclusively ("check `status` for a running RIAPP and close it before
  starting another"). The lesson is the one worth keeping: a box that
  *mentions* the real cause is not a guard, and reading the low-res
  capture instead of the source produced a confident wrong answer.
  The real hazard stands though — on a lane where two instances CAN share
  the audio device (null backend, or a device that allows it), both go on
  writing `ENVARC:ReIncarnation/`. Fixed below with an actual claim.

## A real single-instance guard (2026-10-18, later)

The correction above says there was no guard. Now there is one, and the
lane cost four bugs that only AROS can teach:

- **The claim** (`ri_pal_claim_acquire/refresh/release`, `platform/`):
  `CreateDir` is the atomic test-and-set (a lock directory on `RAM:`, new
  `RI_PATH_RUN`), plus a heartbeat file the holder keeps **open** for as
  long as it lives. 0 claimed / 1 busy / 2 undecidable, and the app runs
  on 2 — an unopenable claim must never be the reason the instrument will
  not open. Every decision logs its reason (`ri_pal_claim_last`), because
  a lock that decides silently cannot be told from a broken one on a lane.
- **Four AROS facts, all found by the lane, none guessable:**
  1. `Open(path, MODE_READWRITE)` does **not** create a missing file;
     `MODE_NEWFILE` does. The heartbeat was silently never written.
  2. A file another process holds open for writing cannot be re-opened
     `MODE_NEWFILE` by a second process. So the holder keeps its handle
     and writes through it — and a contender's inability to create the
     heartbeat is direct evidence that a live holder has it.
  3. `Rename()` returns **FALSE (0) on success** (AmigaDOS convention),
     the opposite of every other call here. A loop written the obvious way
     stops at the first *successful* rename and then reports the claim as
     un-displaceable forever. Displacement is now *verified* by
     `CreateDir` succeeding, so no return-code convention is trusted.
  4. `Examine()` answers "here it is" for a name that is not there, so it
     cannot decide "absent"; the existence test is the write instead.
- **Two dead-looking claims, opposite answers** (the part that matters):
  heartbeat **absent** → nobody is heartbeating → takeover (this is the
  crash-between-create-and-write path, and what stops a leftover claim
  from wedging the instrument forever); heartbeat **present but
  unreadable** → an I/O problem is not evidence of death → hold.
- **Proof:** a scratch probe (`RAM:CLAIMPROBE`, never committed) walks the
  AROS decision table on the Dell — take `0`, second holder `1`, refresh
  `0`, crashed-without-heartbeat `0`, crashed-with-stale-heartbeat `0`
  (1600 s old), heartbeat-occupied `1` — each with its reason string.
  End to end, a second `Run` of RIAPP shows the requester over the live
  panel (`dell-s7-instance-claim-refusal.png`), owns no window, touches
  no prefs, and exits when dismissed; `RUN claim rc=0 taken` pins the
  winner's decision in the evlog.
- **Cost of a dirty machine:** three zombie instances and a locked evlog
  made every later run look like a code bug. The evlog can only be owned
  by one process (same locking rule), so a second instance's log lines
  vanish — break everything by number first, and read the *tail* of the
  session log, not the first match.

## S7 completed: a song now chooses the skins (2026-10-18)

- **The gap:** the SKAS chunk had a codec and tests, but nothing in the
  app ever called it — `skinassign_to_song` / `ri_skinassign_from_song`
  had no call sites, so per-section skins were a session-only UI choice
  that died with the app. There is no save path in the app at all, so only
  the load side is wired.
- **The policy, and why it needed its own function:** the reader resolves
  "song carries no SKAS" to *all Classic*, which is right for a cold
  table and wrong for a live session — calling it on every load would
  throw away the user's Ctrl+M choice each time an ordinary song played.
  `ri_skinassign_apply_song` (`gui/skinsect.c`) is the guarded door:
  `nskin == 0` → table untouched, no change reported. It reports
  **changed** (did the effective table move, SYNTH2 rule included), not
  `from_song`'s **dirty** (does the song name a mod that is not
  installed) — two different questions, one flag at the call site.
- **Tests RED first** (t128): the unguarded version fails exactly
  "uniform song keeps the user's choice" / "still not classic" / "skas
  reports the change". Two mutants die on the right lines — inverted
  `nskin` guard, and a snapshot taken after the call instead of before.
- **Dell proof** (captures `dell-s7-song-skin-applied.png`,
  `dell-s7-song-skin-policy.png`): a song carrying `SKAS 808→808-RI`
  loads and the 808 renders skinned (`SKIN song=… n=1 changed=1`); a
  two-entry playlist then cycles skinned → uniform → skinned and the log
  reads `SKIN song=tiny-plain.rbng none` with **no** `sync=` line after
  it, and the skinned reload reports `changed=0` — the choice survives,
  and the registry is not churned. Captures confirm the 808 is still
  amber after several uniform loads.
- **Lane facts earned doing it:** the playlist auto-advance is the only
  mid-session song load reachable without a file requester, and it needs
  `song_bars > 0` — a song whose track is zeroed reports `bars=0` and
  `song_poll_end` returns early, so the fixture truncates the track to
  two bars instead of clearing it. `Break <name>` does not resolve a
  path; `Break <number>` does.
