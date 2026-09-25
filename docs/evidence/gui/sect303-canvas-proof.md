# 303 section canvas — riqemu1 proof (§12.10 G4)

**Date:** 2026-09-25. **Lane:** riqemu1 (QEMU, ABIv1 live ISO, private; Dell unavailable — stick backup).
**Binary:** `RI303`, built by `bash scripts/ri_build_aros.sh sect303` → `/tmp/ri/aros/RI303` (v1 recipe: v1 build-pc SDK, `startup.o`, `-nostartfiles -no-pie`, `-lmui -lamiga -lstdcio -lposixc -lintuition -lgraphics -lutility -ldos -lexec -lautoinit`; 0 unresolved, 0 `mov %rax,%r12`).
**Code:** `gui/widgets/rsec303.mcc.c` (canvas), `app/sect303proof.c` (proof window + readout).

## Proven

- **Render from registry + measured geometry.** Window 748 × 265 (canvas 732 × 230 at 1× = `ri_geo_px(1464/460)`) + readout row. Layout matches the manual's p. 153 figure: Waveform left, six knobs Tune…Accent in TB-303 order, EDIT STEP display right, Pitch Mode/Clear block, 13-key keyboard with black keys over white, Note/Pause toggle with note/pause LEDs, Down/Up/Accent/Slide under a black label bar, Back/Step. Screenshot: `img/2026-09-25-ri303-demo.png`.
- **Palette-independent drawing.** First build drew nothing (RGB-pen call ignored on the lane's screen); pens are now obtained per screen in `MUIM_Setup` (`ObtainBestPen`) and released in `MUIM_Cleanup`.
- **State → pixels.** `RI303 demo` runs the manual's “programming from scratch in Pitch Mode” sequence (p. 42) through the same `ri_s303_press` calls a click makes, then Cutoff 30 / Reso 110 / Waveform: the capture shows EDIT STEP **04**, Pitch Mode LED lit, SQR selected, Cutoff and Reso pointers moved; readout `STEP 4 … PM 1 … CUT 30 WAVE 1`.
- **Event plumbing up to IDCMP.** Handler installed in `MUIM_Setup` (lifecycle counters Setup 1 / Show 1); with `IDCMP_INTUITICKS` temporarily added it received ticks (46 → 482), so the handler path is live.

## Not proven here (and why)

- **Mouse clicks on the lane.** Neither the agent's `ui_click` (input.device `IND_ADDEVENT` chain: pointer-pos + LBUTTON down/up) nor QEMU monitor `mouse_move`/`mouse_button` (usb-tablet is the absolute device) produced a single `IDCMP_MOUSEBUTTONS` in the window; monitor clicks landed on the backdrop. Remote click injection is therefore not a valid instrument on this lane; knob drag, button presses and right-click default need a human hand on the riqemu1 window (GTK display on the host) or the Dell. Earlier device notes (m22: “remote click = down/up with no move”) never proved a click either.

## Bug caught by the readout

- `GetAttr()` stores an `IPTR`; the first loop used a `LONG` and smashed the neighbouring `st` pointer (Software Failure in `format_readout`, requester not dismissable remotely → lane reset). Fixed; rule recorded in the code comment.
