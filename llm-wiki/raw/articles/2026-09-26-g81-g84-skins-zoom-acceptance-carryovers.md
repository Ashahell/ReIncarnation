# §12.10 G8 complete code-side: skins, zoom, audit wiring, acceptance machine pass + carry-overs C1–C4

- Source: ReIncarnation commits `c14486e` (G8.1) `494b49c` (G8.2) `9b537c8` (C1) `0488f23` (G8.3) `bb19ae7` (C4) `2503557` (C2-engine) `a9528ad` (C3-record)
- Collected: 2026-09-26
- Published: 2026-09-26

## What landed

- **G8.1 skins** (`c14486e`, 50 files): design `docs/superpowers/specs/2026-09-26-skins-design.md` (format E0-6/E0-7 + dir layout AWAITING OWNER REVIEW); pure core `gui/skin.{h,c}` (fail-closed manifest, ANY-wildcard lookup, per-part Classic fallback, premultiplied box downscale, sha identity, MODR get/set mirroring the codec, Ctrl+M cycle, value→frame, sized names, ARGB→blit swizzle) + `tests/unit/t75_skin.c` (RED-first, 9 killing mutants); AROS loader `gui/skin_aros.c` (datatypes PDTM_READPIXELARRAY, zoom cache per change, single-owner tracking); RSection background + knob hooks; `RISECT mod=` + installed-scan + MOD log + Ctrl+M poll; `tools/mkskin.c` → `skins/808-RI/` (17 bg + 7 strips, PNG/RGBA, self-verified) + `skins/Template/` (blank). Lane: Classic vs 808-RI vs Template vs missing-mod + live cycling (`docs/evidence/gui/skins-g81.md`).
- **G8.2 zoom** (`494b49c`): `RISECT zoom=0..3`, pure `ri_skin_zoom_num()` + `tests/unit/t76_zoom.c` (table vs panelgeo, sizes/pixels per zoom, P-18 drag-law invariance; mutant-proven); loader scales by the helper. Lane matrix `docs/evidence/gui/zoom-g82.md` (crisp everywhere; compact legend crowding noted; true drags need a hand).
- **C1 audit wiring** (`9b537c8`): Phase 12 runs t60–t73 + t75–t76, RICtlDef/RI_SEC_NAMES single-source grep gates, AROS guard list + `ri_build_aros.sh sections` link gate; all three gate types mutant-proven (one in the full audit).
- **G8.3 acceptance** (`0488f23`): `acceptance.md` rewritten for the canvases; ReBirth-101 machine-driven (row 5 PROVEN, rows 1–4 partial with GUI halves proven; Solo out of E1; score 1/5 + 4/5 partial). Dummy row recorded (5 data touch-points, zero logic).
- **C4 Stop law** (`bb19ae7`): E1 p. 145 adopted (first stopped Stop → Loop Start; strictly before locator → song start; locator := loop_start_tick); arm retired; t58 repinned (edge-mutant-proven), t69 unchanged.
- **C2 engine taps** (`2503557`): `sec_meter[4]` + `fx_meter[4]` in RIEngine + accessors, `tests/unit/t78_engine_taps.c` (mutant-proven); audio paths byte-identical (t37/t52 green). App binding blocked on an audio lane (stand-in stays).
- **C3 record** (`a9528ad`): `0023-camd-mysprintf-varargs.diff` (series format, clean-apply verified) + placement note. Series commit needs the t8-workgroup-size owner; upstream PR needs prs.py (absent); debugdriver hang needs lane debug.

## Rulings (ledger `.superpowers/sdd/2026-09-25-gui-parity-plan/progress.md`)

R-G8.1-1 size-suffixed knob parts (808 has two KNOB sizes) · R-G8.1-2 lookup uses geometry section (SYNTH2→SYNTH1) · R-G8.1-3 PNG/RGBA via libpng (png.datatype confirmed on-lane) · R-G8.1-4 format stays owner-review · R-G8.1-5 blit words are BGRA-order (knob_blit's calibration comment; core stays ARGB, loader converts once per zoom) · C4 locator := loop_start_tick · work on main in place (owner-directed, all G1–G7 precedent).

## Lane lessons (riqemu1, session 26)

- Always `Run >NIL:` detached — a foreground run wedges the single-threaded agent (needs lane reset: move spool jobs out, wait 15 s for SFS, `system_reset`, re-verify 1280x1024).
- `C:Break <n> C` stops a detached RISECT (Status → PID).
- `RAM:` files held open (`MODE_NEWFILE`) cannot be fetched or copied (rc 20) — Break first, then get.
- Trace lines log only on change; the MIDI LED bumps changes for every message (N-per-message proves arrival, not application); the live readout shows 808-BD + 909-CH only (909 BD/SD taps are trace-proven).
- `sendkey comma` mis-delivers (0x37 not 0x38); `.`/`/` and all letters verified positionally correct.
- Never trust a scaled view: full-res pixels are the verdict (the ghost-blue episode).
- `put-tree` failed silently once; individual puts (25/25) worked.

## Open (owner / other lanes)

Skins format review · G6b audio binding (Dell or riqemu1 sound surgery) · ReBirth-101 human/audio pass · upstream camd PR + debugdriver hang · compact legend crowding · runtime registry (rack) · pitch-mode key binding (mouse-only today) · 909 accent/flam levels (mouse-only) · mixer mute (mouse-only).
