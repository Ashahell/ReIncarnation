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
- **Skin (808-RI)** renders at all four zooms on the Sandy Bridge VESA framebuffer.

## Not covered
- Mouse drag: no lane injects mouse drags.
- Pixel-exact crispness: agent captures are 1/2 scale only, because full scale is refused at 1366x768.
