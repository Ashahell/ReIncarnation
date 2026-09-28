# GUI hardware-look pass: shaded knobs, moulded keys, glowing LEDs, faders, panels

- Source: ReIncarnation session, 2026-09-28 (owner: "as modern as possible", synths as close to the hardware as possible, "be extremely critical")
- Collected: 2026-09-28
- Published: 2026-09-28
- Commit: `adc51ee`
- Evidence: `docs/evidence/gui/hw-look/` (README, before/after host renders at 2x, Dell capture)

## Review findings (before)
- The whole GUI was flat: single-colour rects with 1-px bevels, no shadows or depth.
- Knobs were flat discs with a hairline pointer and thin tick lines. The **TB-303 knobs were light grey**, where the hardware (and ReBirth) uses black knobs with a white line.
- Keys and buttons were plain rectangles; the 303 keybed was flat blocks.
- LEDs were flat dots; faders a 2-px line plus a box; LED displays bare; panels unfinished (the 303 panel was a warm beige where the hardware is cool brushed aluminium); header strips were flat.
- On the 808, the sound switches were flat blocks, the step "lamps" were squares, and the instrument-select pointer rendered as a dashed bar.

## Changes (display-list layer `gui/draw/`, shared by AROS and host)
- Colour math: `ri_art_mix`, `ri_art_shade`, `ri_art_luma`, and a graded disc `ri_art_disc_grad`.
- **Knob:**
  - drop shadow, skirt, graded body, concave cap, glint;
  - a solid pointer bar, drawn with x and y offsets so rounding leaves no gaps;
  - the printed scale as 11 dots (larger at 0, 5 and 10), in a colour that contrasts with the panel.
- **Button/key:** moulded outline with softened corners, graded face, top highlight, bottom inner shadow.
- **LED:** lit = halo + graded core + specular; unlit = a dark lens in a bezel with a glint. This includes the 808 step LEDs set into the key tops.
- **Fader:** recessed groove with a lit lip; ridged, shaded cap with a white index line.
- **LED display:** black bezel, smoked glass, top glint.
- **Panels:** brushed (303, 909) or satin (808, mixer, FX, transport, pattern) plates with rolled edges; corner screws on the three instruments; moulded header strips.
- 303: cool silver `0xCACCC7`, black knobs `0x2C2C2C` with white pointers, ivory and ebony keys. 808: slide-lever switches and moulded cream instrument keys.
- Display-list capacity went from 8192 to 24576 commands (the 909 at 2x emits ~8.2k).

## AROS finding: exact colours need FillPixelArray
- `SetRPAttrs(rp, RPTAG_FgColor, ARGB)` is **ignored** by this AROS's RectFill/Draw: the first Dell run painted every canvas one flat orange (the last pen).
- Fix: on hi/truecolor screens, rects, spans and axis lines go through cybergraphics `FillPixelArray(rp, x, y, w, h, ARGB)`, which also works into the friend-bitmap double buffer. Text and diagonal lines (palette colours) keep pens; palette screens use the nearest `C_*` pen.

## Proof
- Host: t92 (72 display-list pins) and t93 (72 raster pins plus the skin pin) were re-pinned deliberately; the host-raster snapshots were refreshed.
- Dell (ABIv11): RIAPP renders the new look. Audit 0/0.
- The owner's by-eye verdict is pending.

## Next candidates
- A condensed hardware-style legend face.
- Dirty-rect redraws if knob drags feel slow (~8k primitives per 909 redraw at 2x).
