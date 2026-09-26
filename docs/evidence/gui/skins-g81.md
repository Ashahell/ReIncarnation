# Skins G8.1 — device proof (riqemu1, 2026-09-26)

Build: `RISECT` with `gui/skin.c` + `gui/skin_aros.c` + RSection bg/knob hooks
(`C:Break N C` stops a detached instance; always `Run >NIL:` detached —
a foreground run wedges the single-threaded agent and needs a lane reset).
Agent session 26, screen 1280x1024 throughout.
`png.datatype` present on the lane (`SYS:Classes/DataTypes`, 57096 B) —
design E0-11 holds, no format re-rule.

## Same section, three mods (`RISECT 808 mod=<name>`)

- `img/2026-09-26-skin-808-classic.png` — Classic (no mod): procedural brown
  panel, red/white knobs. Baseline.
- `img/2026-09-26-skin-808-808ri.png` — 808-RI: near-black lacquer backdrop
  (sampled ~(23,19,26) modulo palette), amber knob rings + pointers, cream
  legends; steps/keys/selector stay procedural (no skin parts for them —
  per-part fallback inside one render).
- `img/2026-09-26-skin-808-template.png` — Template (manifest documents every
  key, zero images): pixel-identical Classic look, full fallback.
- `img/2026-09-26-skin-808-missing.png` — missing mod: Classic look.

All four readouts identical (`SEL 1 ROW … BD 0 LC 0 CH 0 G 0 EV 0/0`):
same binary, same state machine under every skin.

## MOD log (`RAM:RISECT.MOD`, one line per selection)

- `2026-09-26-rimod-808ri.txt`: `mod='808-RI' active`
- `2026-09-26-rimod-missing.txt`: `mod='NoSuch' not found (rc=2) — Classic, song dirty`
  (rc = negated loader code: -2 manifest unreadable; §17 row 3 path)
- `2026-09-26-rimod-cycle1.txt`: `mod=Classic: procedural (no files)`
- `2026-09-26-rimod-cycle2.txt`: `mod='808-RI' active`

## Live cycling (`RISECT keys mod=808-RI`, QEMU `sendkey ctrl-m`)

`img/2026-09-26-skin-keys-cycle.png` — keys panel after one Ctrl+M:
Transport + 4 pattern sections + Synth 1 all Classic, readout alive
(`FOCUS 0 … BPM 120 …`). Cycle order on this lane: 808-RI → Classic →
808-RI (installed scan: Classic first, then directory order).

## Bugs the lane caught (both fixed, in this commit)

1. `mod=` never applied in single-section modes (skin setup lived only in
   the keys/live branches) — first capture came out Classic.
2. The Ctrl+M poll fired in single-section modes (no panel exists) and
   clobbered the fresh skin with `skin_apply("")` on the first iteration
   (stale MOD log gave it away) — poll now gated on panel modes.
3. ARGB word packing into WritePixelArrayAlpha paints ghost-blue
   (knob_blit's calibration comment says exactly this) — loader converts
   once per zoom via the host-tested `ri_skin_swizzle_blit`; first-draw
   vs sustained renders differed until fixed. Never trust a scaled view:
   full-res pixels are the verdict.
