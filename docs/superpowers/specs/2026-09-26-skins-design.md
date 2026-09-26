# Skins ("Mods", appearance half) — design (§12.10 G8.1)

**Date:** 2026-09-26. **Status:** proposed — owner review required before the
on-disk format and directory layout are treated as final (both are
non-reversible; the code below implements this proposal and is changeable).
**Scope:** appearance half of a Mod only. The drum-sound half stays in the
engine owner's scope; this doc references it where the two meet (MODR, RBNM).

## Authority

- E1 (Owner's Manual ch. 11, p. 88–97, per G8 prompt): a Mod changes the look
  of the panel and the drum sounds; it never changes function, control
  placement, the synth sound or the FX. The Mod selection is saved in the song.
- Spec §13: Classic default + LOCKED second skin "808-RI"; blank template +
  AmigaGuide SDK; skins load through `picture.class` datatypes, applied only via
  MCC subclassing (no new BOOPSI classes); `SYS:Classes/ReIncarnation/Mods/`;
  artwork authored at 2x, filtered down; frame counts declared per control
  (min 32, classic look 64); SHA-256 = content identity, song MODR stores
  name + SHA-256 + vers; readers skip unknown optional chunks, fail closed.
- Spec §17 row 3: missing mod on load → warn "locate it?", else default mod +
  song dirty, never silent substitution. Mod switch idle-only (§13 909 note).

## Model on top of RSection (E0-1)

Geometry stays in `panelgeo` (Q units, zoom indices 0–3). A skin may only
restyle, never move: the lookup key is (section id, control kind, part name,
state); positions/sizes always come from geometry. A skin supplies, per
section: a background image; per control kind: knob frame strip (one frame per
knob position, ReBirth-style), fader cap + track, key/step up/down/lit frames,
LED on/off, 7-seg digit strip, lever states, meter scale art; plus legend
font/colour overrides. **Fallback (E0-2): per part** — any missing part renders
through today's procedural Classic path, so a partial skin is legal and a
skin with zero images is valid (pure legend/font restyle).

## On-disk form — PROPOSED, needs owner sign-off (non-reversible)

- Directory per mod: `SYS:Classes/ReIncarnation/Mods/<Name>/` containing
  `Skin.manifest` (line-based `KEY=VALUE` text, `\\n` terminated, `#` comments;
  unknown keys ignored; any malformed line fails the whole load closed) plus
  image files named by the manifest (`PART.<section>.<kind>.<part>=file,NFRAMES`
  for strips, `BACKGROUND.<section>=file` for backdrops).
- Images: IFF ILBM or PNG, decoded at load through `datatypes.library` →
  `picture.class` (AROS loader); the host core never touches files — it takes
  the manifest text plus a caller-supplied `read_file(name, buf, cap)` callback
  (dependency injection, no IO in core) and RGBA32 pixels per part.
- Identity: SHA-256 (existing `project/sha256.c`) over the manifest bytes then
  each referenced part file's bytes in manifest order. Compared against the
  song MODR sha on load (name + vers select, sha verifies).
- RBNM relation: the drum-sound half lives in RBNM (engine scope). The
  appearance half does not duplicate it; a mod directory MAY ship a `.rbnm`
  sidecar, referenced by manifest key, which the engine loads through its own
  path. This doc does not define that sidecar.

## Zoom (E0-3, feeds G8.2)

Masters authored at 2x. 1x/1.5x/0.75x are produced by a pure box downscaler
(premultiplied-alpha RGBA, no libm) run once at load/zoom-change, never per
frame. Scaled parts are cached per zoom index; switching zoom re-runs the
downscaler from the 2x masters (no resample-of-resample).

## Memory + lifecycle (E0-4)

Load on select, release on switch (all frees in GUI/loader code — never in
`engine/`). Switch allowed only while the transport is stopped (idle-only);
requesting a switch while playing returns BUSY and keeps the current skin.

## Missing-mod path (spec §17 row 3)

On song load, `rbng_missing_warn()` already produces the warn string; the
selection UI shows it in a requester with Locate/Skip. Skip (decline) loads
Classic and marks the song dirty (app-level flag; the skin core returns the
decision enum, it does not touch song state). Never substitute silently.

## Selection UI (E0-5)

Minimal: proof-app switch `RISECT … mod=<name>` plus Ctrl+M (`RI_KM_SELECT_MOD`,
already decoded) cycling the installed list in the live panel; a full Mods menu
is deferred to the owner pass. Installed list = subdirectories of the Mods dir
presenting a valid manifest (Classic is always entry 0, needs no files).

## "808-RI" without tracing (clean-room, spec §1)

Original art generated procedurally by a host tool (`tools/mkskin.c`, manual
build line, documented) into the skin format: darker 808-style backdrop,
amber legends, chunkier knob frames. Never measured from ReBirth pixels beyond
the standing geometry (panelgeo, already E0-locked). Blank template = manifest
with every key documented, zero images (exercises the per-part fallback).

## E0 list (owner may overrule)

E0-1 restyle-only keying · E0-2 per-part fallback · E0-3 box downscale at
load/zoom-change · E0-4 load-on-select/release-on-switch, idle-only ·
E0-5 Ctrl+M cycles list, menu deferred · E0-6 manifest is line-based text
(proposed) · E0-7 identity = sha over manifest + parts in order (proposed) ·
E0-8 sidecar `.rbnm` reference key only (engine defines it) ·
E0-9 size-varying parts take size-suffixed names (`knob.frame.<w>x<h>`, 2x-master
px dims — the 808 has KNOB items in two sizes, one strip cannot serve both) ·
E0-10 skin lookup uses the geometry section (SYNTH2→SYNTH1, same as the canvas:
both synths share skin art) · E0-11 skin images are PNG/RGBA (libpng-written;
ILBM cannot carry per-pixel alpha) — pending lane proof that png.datatype is
present on riqemu1, else re-ruled.

## Tracker state

File format (E0-6/E0-7) + directory layout are non-reversible → recorded in
`docs/2026-09-24-improvement-todo.md` §12.10 as "skins format awaiting owner
review"; reversible parts (core, loader, template, 808-RI generator, UI cycle)
proceed under TDD now.
