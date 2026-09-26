# Skins ("Mods", appearance half) — design (§12.10 G8.1)

**Date:** 2026-09-26. **Status:** APPROVED (owner, 2026-09-26) as **format 1**,
with the owner-requested changes: version header, named sections/kinds,
role-named parts with a load-time size check, ASCII + line-length rule
(see "On-disk form"). Directory layout unchanged.
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

## On-disk form — format 1 (APPROVED 2026-09-26)

- Directory per mod: `SYS:Classes/ReIncarnation/Mods/<Name>/` containing
  `Skin.manifest` plus the image files it names.
- Manifest text: printable ASCII plus TAB/CR/LF only; lines at most 255
  bytes; `#` comment lines; unknown keys ignored; any malformed line fails
  the whole load closed.
- **Header** (required, the first three keys, in this order):
  - `FORMAT=1` — a reader refuses any other number (future formats never
    half-load);
  - `NAME=<name>` — `[A-Za-z0-9._-]{1,63}`, must equal the directory name
    (case-insensitive, as AROS file systems compare): songs store this name
    in MODR;
  - `VERSION=<1..65535>` — raised whenever the art changes; stored as MODR
    `vers`.
- **Keys use names, never numbers:**
  - `BACKGROUND.<section>=file`
  - `PART.<section>.<kind>.<part>=file,NFRAMES`
  - section tokens (single source `gui/ctlreg.c`, append-only): `303 808
    909 mix-303a mix-303b mix-808 mix-909 master pcf delay dist comp
    transport pat-303a pat-303b pat-808 pat-909` (both synths share `303`);
  - kind tokens: `knob fader switch button led step selector display meter`;
  - a section or kind this build does not know is **ignored** after its
    line is syntax-checked — it belongs to a later build's device (the
    extensible rack), so older builds still load newer skins.
- **Parts are named by role, never by pixel size:** `knob.frame` = the
  section's largest knob, `knob.frame.small` = its other knob size (no
  section has more than two; t75 pins it). Reserved roles: `fader.cap`,
  `step.lit`, `digit.strip`.
- **Size check at load:** art is authored at 2x (2x-master px == panel Q).
  The expected size of every checked part comes from `panelgeo`
  (`ri_skin_expect`); a part whose image differs is left unbound, drawn
  through Classic and **reported** (`nstale`, first key; the proof app logs
  `mod='…': part(s) wrong size for the panel, first <key> - drawn Classic`) —
  never cropped or stretched. t75 also checks every shipped skin image
  against the panel, so stale art fails the audit, not a user.
- Images: PNG/RGBA (recommended) or IFF ILBM, decoded through
  `datatypes.library` → `picture.class`; the host core never touches files.
- Identity: SHA-256 over the manifest bytes then each **loaded** part file's
  bytes in manifest order (ignored lines contribute through the manifest
  bytes only).
- RBNM relation: the drum-sound half lives in RBNM (engine scope); a mod
  directory MAY ship a `.rbnm` sidecar referenced by manifest key, defined
  by the engine owner.

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
E0-5 Ctrl+M cycles list, menu deferred · E0-6 manifest is line-based text,
format 1 header (APPROVED) · E0-7 identity = sha over manifest + parts in order (APPROVED) ·
E0-8 sidecar `.rbnm` reference key only (engine defines it) ·
E0-9 (superseded by format 1) size-varying parts are named by role
(`knob.frame` / `knob.frame.small`) and size-checked at load ·
E0-10 skin lookup uses the geometry section (SYNTH2→SYNTH1, same as the canvas:
both synths share skin art) · E0-11 skin images are PNG/RGBA (libpng-written;
ILBM cannot carry per-pixel alpha) — pending lane proof that png.datatype is
present on riqemu1, else re-ruled.

## Per-module skin choice (owner requirement 2026-09-26, next step)

Users may want a different skin per module (e.g. the 808 in 808-RI, the
mixers Classic). Format 1 already allows it: a skin may cover any subset of
sections, and missing parts fall back per part. What changes is the
selection side:
- The active skin becomes a per-section table (`RI_SEC_COUNT` entries, NULL =
  Classic) instead of one global pointer. The canvas looks up its own
  section, and SYNTH2 follows SYNTH1 unless the user splits them.
- Loaded skins are shared: each mod directory is loaded once and
  reference-counted across the sections that use it, and released when the
  last section switches away (idle-only, as today).
- The song MODR table already holds up to 16 entries. A per-section
  assignment needs a new optional chunk (section token -> mod name),
  written only when the choice is not uniform; readers that skip it get the
  first MODR mod everywhere (fail-closed, never a silent substitute).
- UI: Ctrl+M cycles the focused section's skin; a "whole panel" choice sets
  every section.
- Tests: pure assignment table + refcount lifecycle in the core (host),
  then the Dell/QEMU lane proof.
- Consistent with the extensible device rack: new devices get a section
  token and join the table.

## Tracker state

Format 1 + directory layout approved by the owner 2026-09-26; skins format
review closed.
