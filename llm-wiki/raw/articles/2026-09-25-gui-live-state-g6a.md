# §12.10 G6a: live state — playheads from a sample count, running light, taps at the playhead, meters

- Source: ReIncarnation commit `9c65a84`; ledger `docs/evidence/gui/live-state.md`, trace `docs/evidence/gui/2026-09-25-rilive-trace.txt`, capture `docs/evidence/gui/img/2026-09-25-rilive-playing.png`
- Collected: 2026-09-25
- Published: 2026-09-25

## What landed

- `gui/livestate.{h,c}` (t72):
  - 16ths = floor(samples × bpm × 4 / (60 × sr)), integer-exact (6000 samples per 16th at 120 bpm, 5625 at 128) — a projection of a sample count, never a timer beat (review §2.9: the old timer beat drifted +347 µs per fire);
  - per-section loop lengths (p. 147);
  - Song-mode bar follow with loop wrap, clamped at 999;
  - meter scale −36..0 dBFS in 1 dB rungs (precomputed table, no libm);
  - comp GR scale 20 dB full (E0).
- Panel (`ri_panel_live`, t72): playheads (−1 when stopped).
- Taps record at the playhead while playing (p. 32–33, 43–44):
  - synth Tab = note;
  - drum key = a single click's state (808 on, 909 low — E0), never lowering a hit;
  - `-` = AC;
  - Shift-held delete keeps deleting at every step reached.
- RSection: a white running light over the programmed-step lamps.

## Proof (riqemu1, real keys via QEMU sendkey, STAND-IN clock)

- The clock is Intuition `CurrentTime()` wall time, because riqemu1 has no audio device; the meters are stand-ins too (hit = 0 dBFS, then decay).
- BD taps at 0.5 s landed on steps 10, 14, 2, 6 — exactly 4 16ths apart at 120 bpm.
- CH taps landed on 14, 0, 2.
- A 1 s held Shift+K deleted CH on every step reached (0, 2) and kept 14.
- `kp_0` stopped playback (playheads −1).
- The whole trace was fetched with `--get`, because exec-lane `Type` truncated it at 80 lines.
- Known latency: taps use the playhead from the last main-loop pass, woken by IntuiTicks (~10 Hz, ≤ 100 ms).

## Open (G6b)

- Feed `ri_panel_live` from the render task's sample position.
- Add engine per-section / FX meter taps, and route `ri_engine_comp_gr` through `ri_live_gr_level`.
- Both need engine hooks (the engine session's code) and a lane with audio (the Dell's HDA, or QEMU AC97/HDA with an AHI driver).
