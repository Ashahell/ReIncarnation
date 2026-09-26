# Dell owner pass, skin format 1, and the Dell skin-loading fixes (§12.10 G8)

- Source: ReIncarnation session, 2026-09-26
- Collected: 2026-09-26
- Published: 2026-09-26
- Commits: `3cc129c` (Dell G8 proof), `1208e96` (owner pass fixes), `79e5424` (skin format 1 + Dell skin fixes)
- Evidence: `docs/evidence/gui/dell-g8-zoom-skin.md`; spec `docs/superpowers/specs/2026-09-26-skins-design.md`
- Cross-post: Vulkan4AROS llm-wiki `raw/articles/2026-09-26-aros-datatypes-exall-bubblehelp-lessons.md`

## Dell G8 proof (ABIv11, 1366x768)
- RISECT is cross-built with the v11 gcc 16.1 (`-O0 -mcmodel=large -ffixed-r12`, SDK include plus `include/aros/stdc`, `-lmui -lamiga -lcamd`); 0 unresolved symbols.
- The Dell has no `tar`, so `--put-tree` fails; upload files one by one.
- Agent captures are 1/2 scale at minimum (scale 1 is refused at 1366x768). 1-pixel lines whose y positions share a parity vanish from those captures, so the owner judges those on the screen.
- 30/30 runs were clean. At 2x, the 303/808/909 need about 1520 px, so their windows are clamped and about 75–80 px is clipped on each side. The proof app hard-codes zoom 0 for mix/fx/tr/keys.

## Owner pass (one section at a time, approved)
- **303:**
  - the keyboard block gets a rim on every side (176..858 Q);
  - Pitch Mode/Clear moved 20 Q left, and everything right of the keys moved 20 Q right.
- **808:**
  - flicker fixed with an off-screen friend-bitmap double buffer, blitted once per draw, with a direct-paint fallback;
  - bubble help via `ri_ctlreg_help` plus Zune `MUIM_CreateShortHelp` (p. 148/151 names);
  - Tone vs Tune and the knobs present per instrument match p. 37/148.
- **909:**
  - the 303 inverse-legend and EDIT STEP rules had leaked to the 909 (the same indices); they are now scoped to the 303;
  - the bottom rule no longer crosses FLAM.
- **Mixers:** the Master block height equals the strips (464 Q).
- **FX and transport:** approved as is.

## opencode G8 report, verified
- All 9 commits exist and audit 0/0.
- Gaps found:
  - `t78_engine_taps` passes but is not wired into the audit;
  - the zoom matrix overclaims (mix/fx/tr never ran at 2x);
  - 2x does not fit 1366.
- The stated commit range was wrong.

## Skin format 1 (owner-approved)
- **Header:** `FORMAT=1`, `NAME=`, `VERSION=`, as the first three keys in that order. `NAME` equals the mod directory name (case-insensitive).
- **Named keys:**
  - section tokens `303 808 909 mix-303a … pat-909` (single source ctlreg, append-only);
  - kind tokens `knob … meter`;
  - an unknown token is ignored after its line is syntax-checked (a later build's device).
- **Role parts:** `knob.frame` / `knob.frame.small` (no section has more than two knob sizes).
- **Load-time size check:** `ri_skin_expect` from panelgeo. A wrong-size part stays unbound, is drawn Classic and is reported (`nstale`, first key).
- **Text rules:** printable ASCII, lines ≤255 bytes. New parse codes: -7 header, -8 charset.
- **Shipped skins:**
  - t75 checks every shipped skin image's PNG IHDR against the panel;
  - 808-RI was regenerated, fixing the Master backdrop that today's height change had made stale (332x392 → 332x464).
- **Per-module skin choice:** an owner requirement, designed (per-section table, refcounted loads, optional song chunk) but not built.

## Dell skin fixes
1. **Skins never loaded on the Dell.** The v11 `png.datatype` 42.5 leaves `DTA_NominalHoriz/Vert` at 0, and the loader stored those IPTRs into `ULONG`s. Fix: IPTR storage and `PDTA_BitMapHeader` for the size.
2. **Privilege violation in `skin_scan`.** It stepped `ExAll` records by `ed_Size` (the file size) under `ED_NAME`. Fix: `ED_TYPE`, walk `ed_Next`, loop while ExAll returns nonzero, `ExAllEnd` on an early stop. It was narrowed with a disk/RAM-log probe build (`main` was logged, `scanned` never).
3. **Faders and scale lines** are drawn lighter over dark skins (owner).
4. **Correction:** the earlier claim that 808-RI rendered on the Dell was false; the evidence doc is marked Outdated.
