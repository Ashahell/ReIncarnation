# G8 skin + zoom on real hardware (Dell E6320, ABIv11, 1366x768) — 2026-09-26

## Build and run
- Build: RISECT cross-built from HEAD `729a573` in a scratch worktree, so the tree's uncommitted work was not included.
  - Compiler: the v11 `x86_64-aros-gcc` 16.1, flags as for the Dell classic window (`-O0 -mcmodel=large -mno-red-zone -ffixed-r12`).
  - Includes: `sdk/Developer/include` + `include/aros/stdc`, linked with `-lmui -lamiga -lcamd`.
  - Result: 249,744 B, 0 unresolved symbols, 28 r12 moves (expected on v11).
- Install:
  - `RAM:RISECT`;
  - the 808-RI mod as 25 files into `SYS:Classes/ReIncarnation/Mods/808-RI/`. The Dell has no `tar`, so `--put-tree` fails; files were uploaded one by one.
- Matrix, on agent e6320 session 25: `RISECT <sec> zoom=<z> [mod=808-RI]`, then wait 3 s, window list, capture at 1/2 scale, close.
  - Sections 303/808/909/mix/fx/tr at zooms 0–3; 808 under 808-RI at zooms 0–3; keys at zooms 0 and 3.
  - Window sizes per run: `2026-09-26-dell-g8-matrix.json`.

## Results
- **30/30 runs** opened a window, captured, and closed via close-request. No requesters and no crashes, Classic and skinned at every zoom.
- **Fit at 1366 px**, window width in pixels:

  | Section | 1x | 1.5x | 0.75x | 2x |
  |---|---|---|---|---|
  | 303 | 760 | 1126 | 577 | clamped to 1366 |
  | 808 | 764 | 1132 | 580 | clamped to 1366 |
  | 909 | 758 | 1123 | 576 | clamped to 1366 |

  - The full keys panel is 870x626 and fits.
  - **At 2x, 303/808/909 do not fit.** They need about 1520 px, the window is clamped to the screen width, and the canvas is **center-clipped by about 75–80 px on each side**. For example, 303 WAVEFORM and 808 AC are cut at the left, and 303 EDIT STEP sits at the right edge (`img/2026-09-26-dell-303z2.png`, `…808z2-808-RI.png`).
  - Owner call: either cap zoom per screen (largest factor whose width ≤ screen width), or scroll.
- **Zoom is ignored for mix/fx/tr/keys in the proof app.** `app/sectproof.c` creates those canvases with `ri_rsection_create(…, 0)` hard-coded; only single sections use `s_zoom`.
  - The window size is identical at z0–z3 (mix 770x288, fx 700x268, tr 660x368, keys 870x626).
  - So the mix/fx/tr "z0+z2" rows in `zoom-g82.md` did not exercise 2x.
- **Compact (0.75x) legend crowding** reproduces on hardware: the 303 z3 legends run together. This matches `zoom-g82.md` and is still flagged for the owner pass.
- ~~**Skin (808-RI)** renders at all four zooms on the Sandy Bridge VESA framebuffer.~~
  **Status: Outdated (2026-09-26, later the same day).** Wrong: on the Dell
  no skin part ever loaded. Every part failed decode with -3 because the
  v11 `png.datatype` 42.5 leaves `DTA_NominalHoriz/Vert` at 0 (and the
  loader stored those IPTR attributes into 32-bit `ULONG`s). Every section
  drew Classic, which looks close to 808-RI on the dark 808 panel. The
  loader fix (IPTR + `PDTA_BitMapHeader` size) makes parts decode, but the
  first skinned run then took the Dell agent down; see "Skins on the Dell"
  below for the cause and fix.

## Not covered
- Mouse drag: no lane injects mouse drags.
- Pixel-exact crispness: agent captures are 1/2 scale only, because full scale is refused at 1366x768.

## Owner pass, one section at a time (Dell, 1x, same day)
Each section was left open for the owner, fixed where asked, and redeployed.

- **303:**
  - The keyboard block's left rim was hidden under the first white key. The block is now 176..858 Q, so every side has the same 8 Q rim.
  - Pitch Mode and Clear (buttons, LED, legends) moved 20 Q left, so the device-font legend clears the rim.
  - Everything right of the keys (Down/Up/Accent/Slide, their label strip, Note/Pause and its LEDs) moved 20 Q right.
  - Approved.
- **808:**
  - Knob drags flickered, because every change repainted the whole panel straight into the window. The canvas now paints into an off-screen friend bitmap and blits it once; it falls back to direct painting if the bitmap cannot be allocated. The owner confirmed the flicker is gone. This applies to every section.
  - New bubble help: `ri_ctlreg_help()` (pure, t60) plus a Zune `MUIM_CreateShortHelp`. Instrument buttons show "Bass Drum (BD)"; grouped knobs show "Bass Drum: Tone" (manual p. 148/151 names).
  - Tone vs Tune, and knobs present only on some instruments, match p. 37/148.
  - Approved.
- **909:**
  - The CP Level, CH/OH Decay and CC Level legends drew white: the 303 Down..Slide inverse-legend rule matched the same indices in every non-808 section. It is now scoped to the 303, as is the EDIT STEP legend rule.
  - The bottom rule now starts at the AC divider, so it no longer crosses FLAM.
  - Approved.
- **Mixers:**
  - The Master block is raised to the strips' 464 Q height (t61), with content top-aligned.
  - The agent dropped once during a screen capture (connection reset), and the owner rebooted the box.
  - Approved.
- **FX, Transport:** approved as is.

## Skins on the Dell: three bugs, fixed (same day)
1. **Picture size never read.** The v11 `png.datatype` 42.5 leaves
   `DTA_NominalHoriz/Vert` at 0, so every part failed decode with -3 and was
   drawn Classic. The loader also stored those IPTR attributes into 32-bit
   `ULONG`s. Fix: IPTR storage, with the size taken from `PDTA_BitMapHeader`
   (`bmh_Width/Height`) and `DTA_Nominal*` only as a fallback.
2. **Mods directory scan crashed (privilege violation).** `skin_scan`
   stepped through `ExAll` records by `ed_Size`, which is the FILE size and
   only valid with `ED_SIZE`. It also asked for `ED_NAME` but read
   `ed_Type`, and treated ExAll's "more entries" return as "done". With
   three entries in the Mods dir it walked into garbage. A disk-log probe
   build narrowed it: `main` was logged, `scanned` never was.
   Fix: `ED_TYPE`, walk `ed_Next`, loop while ExAll returns nonzero, and
   call `ExAllEnd` on an early stop.
3. **The flicker the owner saw during this hunt** came from a diagnostic
   build (`RISECTD`) that deliberately bypassed the double buffer. The
   shipped build keeps it, and skins blit into the off-screen bitmap
   (WritePixelArrayAlpha rc nonzero on every part).

Results (clean build, agent session 27):
- `RISECT 808 mod=808-RI`: every part decodes and blits
  (`img/2026-09-26-dell-808ri-skinned.png`, amber-rim knob strips).
- `RISECT mix mod=Stale` (808-RI with the old 332x392 Master backdrop): the
  mixers are skinned, the Master is drawn Classic, and the log says
  `mod='Stale': part(s) wrong size for the panel, first BACKGROUND.master -
  drawn Classic` (`img/2026-09-26-dell-skin-stale-master.png`).
- Two Software Failure requesters from the crashed diagnostic tasks
  (`RISECTD`/`RISECTE`) are still open on the Dell. They are harmless, and
  the next reboot clears them.
