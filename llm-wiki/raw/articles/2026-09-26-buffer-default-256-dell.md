# Buffer default 256, by ear and by numbers (§12.11 G9b Step 1)

- Source: ReIncarnation session, 2026-09-26 (agent-driven Dell E6320 takes, ABIv11; owner listening)
- Collected: 2026-09-26
- Published: 2026-09-26
- Commits: `ab654b0` (ladder + soak + P-21), `a8fb862` (default 1024 → 256 on owner approval)
- Evidence: `docs/evidence/audio/live-render-task.md` (table + analysis)

## Method

Same agent-driven script at every size (`RIAPP <frames>`: click
(100,60), Space, W, three C presses, Q; adjacent press/release pairs —
a 1 s hold auto-repeats, proven by a 12x "RIAPP play" storm). Each take
verified from `RAM:RIAPP.LOG` (one play, one record-start, cutoffs
64→48→32→16, wrote-on-quit) before the WAV was compared.

## Ladder (scripted takes, ~8.6 s each)

| frames | buffers | xruns | render max | WAV frames | peak | RMS |
|-------:|--------:|------:|-----------:|-----------:|-----:|-----|
| 1024 | 612 | 0 | 2830 us | 416768 | 17286 | −17.9 dBFS |
| 512 | 1212 | 0 | 1455 us | 413184 | 17286 | −17.9 dBFS |
| 256 | 2441 | 0 | 766 us | 416256 | 17286 | −17.9 dBFS |
| 128 | 4879 | 0 | 410 us | 416256 | 17286 | −17.9 dBFS |
| 64 | 9767 | 2 | 251 us | 416256 | 17286 | −17.9 dBFS |

Headroom (max/period) 13–19 % at every size; mean load ~9–12 %.

## Findings

- Render is identical at every size (peak/RMS equal, zero adjacent
  repeated half-blocks). Honesty note: W capture is task-side,
  pre-device — it proves the render, not what AHI delivered. Device
  loops surface as xruns. Per-take live==offline bit-exactness needs
  buffer-index logging of control drains (not yet built); host t81
  proves live==offline for identical control histories.
- "Worse at 1024 with PlayerFreq" is a hypothesis, not a finding: with
  pass length equal to the half length, the `AHI_SetSound(NONE)` queue
  may race the pass start (inaudible in captures, 0 xruns either way).
  Discriminating test: a forced-PlayerFreq-at-1024 owner A/B (open).
- 5-minute soak at 256: 59038 buffers, 0 xruns, max 773 us. Caveat: 9
  unexplained level moves mid-log (no matching agent job) — not
  input-clean, but 0 xruns under control traffic stands.

## Decision (owner, by ear)

256 "sounds fine", approved. `RIAPP_DEV_FRAMES` is now 256 (5.33 ms).
Spec P-21: task pri 10, floor 128-clean/64-marginal (1–2 xruns/~10k).

## Records

- AROS-general lessons cross-posted to Vulkan4AROS (`efb04adc`):
  DYNAMICSAMPLE double buffer, PlayerFreq pass length, device-as-library
  base, RawDoFmt packed LONGs, rawkey release `CODE|0x80`.
