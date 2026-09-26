# Live state (§12.10 G6a) — ledger

**Status:** G6a implemented and proven on riqemu1 2026-09-25 with a STAND-IN clock. G6b (bind the render task's sample position + engine meter taps) is open.
**Code:** `gui/livestate.{h,c}` (position law, meter/GR scales — t72), `gui/panelui.c` (`ri_panel_live`, taps, held delete — t72), `gui/secttr.c` (`ri_str_follow`), `gui/widgets/rsection.mcc.c` (running light), `app/sectproof.c` (`RISECT live`).

## Laws

- **Position is a projection of a sample count, never a timer beat:** 16ths = floor(samples × bpm × 4 / (60 × sr)), integer-exact. At 48 kHz that is 6000 samples per 16th at 120 bpm and 5625 at 128 bpm. This removes the stepproof `timer.device` beat, which review §2.9 measured drifting +347 µs per fire.
- **Pattern mode:** each section loops its own pattern length ("Each one loops independently", p. 147). The playhead is `16ths mod length`; −1 when stopped.
- **Song mode:** the Bar display follows start + 16ths / 16 (4/4). With the Loop on and playback started inside it, the position wraps to the Loop Start (p. 73). It is clamped at bar 999 (E1: playback continues to 999).
- **Tap recording (p. 32–33, 43–44):**
  - taps only while playing ("With playback activated"), at the section's current playhead;
  - a synth Tab makes the step a Note (tapping only adds);
  - a drum key sets an off step to a single click's state — 808 on, 909 low (E0: the manual doesn't say which 909 level a tap records); it never lowers an existing hit;
  - `-` sets the AC row;
  - Shift+key or Shift+Tab deletes, and KEEPS deleting at every step the playhead reaches while held; key-up ends it.
- **Running light:** the step lamp at the playhead burns white while playing, over the programmed-step lamps (TR-808/909 chase).
- **Meters:** linear peak → 0..127 on a dB scale from −36 dBFS (the Master meter's lowest mark, p. 23) to 0 dBFS (CLIP), in 1 dB rungs. The threshold table is precomputed, so there is no libm. Comp level reduction: 0 dB → 0, 20 dB → 127 (E0 full scale — the p. 164 meter shows only its centre 0).

## Proof on riqemu1 (real keys, stand-in clock)

`RISECT live` shows the Transport, the 808 and 909 with their mixers, and the four Pattern sections on one panel. **The clock is a STAND-IN:** Intuition `CurrentTime()` wall time converted to samples, because riqemu1 has no audio device. **The meters are a stand-in too:** a hit at the playhead = 0 dBFS, then decay. Keys came from QEMU `sendkey`:

1. `ctrl-f`, then `kp_enter` (play), then focus down twice to the 808.
2. `a` ×4 at 0.5 s, which put BD on steps **10, 14, 2, 6** — exactly four 16ths apart, as 0.5 s at 120 bpm implies.
3. Focus down to the 909, then `k` ×3 at 0.3 s, which put CH on **14, 0, 2**.
4. `shift-k` held 1 s: CH deleted at every step reached (0 and 2); step 14, outside the hold, was kept.
5. `kp_0`: stop (playheads −1).

Full per-change trace: `2026-09-25-rilive-trace.txt`. Capture: `img/2026-09-25-rilive-playing.png` shows the running light on 808/909 step 6, BD lamps on 3/7/11/15, and the 808 mixer meter lit.

**Known latency:** a tap records at the playhead from the last main-loop pass. The loop is woken by IntuiTicks (~10 Hz, so at most 100 ms stale), comparable to the latency the manual itself warns about (p. 33, 44). G6b replaces the tick wake-up with the render task's position.

## G6b (open)

- **Render-task position:** feed `ri_panel_live` from the render task's sample position (`RiSeqMasterClock` / the AHI PlayerFunc count) instead of `CurrentTime()`.
- **Engine meter taps:** per-section and per-FX-unit peak taps (engine/mixer), through `ri_live_meter_level`; the Master meter from the mixer's P-16 meter; comp GR from `ri_engine_comp_gr` through `ri_live_gr_level`.
- **Ownership:** both need engine hooks (the song track / engine session's code) and an audio device on the lane (the Dell's HDA, or QEMU AC97/HDA with an AHI driver on riqemu1).

## ReBirth-101 taps (2026-09-26)

`RISECT live`: `ctrl-f`, `kp_enter`, focus down ×3 (909), `a`/`s`/`k`
1.5 s apart, `kp_0`. Trace `2026-09-26-rb101-909-taps.txt`: all three
applied at focus 3 while ST 1 (N29 BD, N42 SD, N55 CH at step 11 — the
readout shows 808-BD + 909-CH only, so BD/SD are trace-proven, CH also
readout-proven with the lamp in `img/2026-09-26-rb101-909-taps.png`).
First attempt without `ctrl-f` recorded nothing (60 lines, all dots) —
taps need the program option, as documented. Accent + flam LEVELS have
no keyboard binding (double-click/selector only) — human rows.
