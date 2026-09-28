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
