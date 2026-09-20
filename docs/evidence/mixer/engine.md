# Mixer + RIDevice registry engine note (Task 11, gate G11)

**Date:** 2026-09-20. **Status:** implemented, `t1_mixer` green.
**Spec:** §13 mixer (4 section buses + master, mute/solo, sends, meters;
P-16/P-17 under TC-2.6.x) + §6 device framework (static table of 4,
TC-2.1.5/TC-2.9.5). No parity claim anywhere in this file — P-16/P-17
are E0/HYPOTHESIS values owned by this project, verified by TCs.

## Equations

- Fader/master/send law (E0, P-17): `gain = (v/127)^2`, v = 0..127;
  v = 0 is exactly 0. One law, three knob kinds
  (`ri_fader_gain`); see `docs/evidence/sequencer/fader-law.md`.
- Mute/solo audibility: no solo active → audible iff !mute; any solo
  active → audible iff solo && !mute (mute wins over solo).
- Zipless toggles: per-bus `applied` gain + `master_applied` slew toward
  target at most 1/64 per sample (full traverse in 64 samples, 1.3 ms @
  48 kHz). Worst single-bus toggle step on a full-scale input is
  1/64 = 0.015625 (measured 0.015625, bound 0.02).
- Sends: post-fader (`bus × applied × send_law`) into a mono send bus;
  mute gates the send (proven: muted send renders exact 0).
- Meter (P-16): peak-hold, per-sample hold multiplier
  `coef = 2^(-log2(10)/sr)` via `ri_pow2` (20 dB/s); taps the
  post-master mono sum. Measured: 1.0 + 1 s silence → 0.0999454
  (rate exactly 20.00 dB/s vs 20 ± 2). Denormal snap below 1e-30
  (pcf.c precedent).
- Registry: file-static `RIDevice[4]` (`303A/303B/808/909`, render NULL
  at init); `ri_device_count` = 4; out-of-range get returns NULL
  (fail-closed). Registration = storing a struct through the returned
  pointer; removal = restoring it — the TC-2.1.5 dummy proof uses only
  the three registry calls, zero framework edits.

## Measurements (`t1_mixer` output)

- Anchors v=16/127: gain 0.015872 (−35.99 dB) / 1.0 (0 dB); all 9
  within ±0.5 dB. RED mutant (linear law): exactly the 7 non-unity
  anchors fail (7.0–18.0 dB errors), all other sections green —
  archived as `red-t1_mixer.txt`.
- Meter: 1 s decay to 0.0999454 → 20.00 dB/s (gate 20 ± 2).
- Mute transient worst step 0.015625 (bound 0.02); mute/unmute tails
  exact (0.0 / 1.0).
- D1: re-init + re-render bit-identical (master and send buses).

## Goldens (`tests/golden/mixer/`)

| File | Fixture | Peak / RMS (s16 LSB) |
|------|---------|----------------------|
| mix-four.wav | 110/138.59/164.81/220 Hz buses, faders 127/96/64/112, bus1 muted | 20691 / 10474 |
| mix-solo.wav | same inputs/faders, solo bus2 only | 2912 / 2059 |

Double-render `cmp`-clean 2/2 (D1). four-vs-solo: 95960/96000
samples differ (solo path live, audit asserts non-identical). Both
above the audibility floor (peak ≥ 1000, rms ≥ 100).

## Deferred (not silent)

Pan (needs a stereo master; Classic is mono end to end) and channel
inserts (ride the RIFX wrapper; land with the GUI task) ship no knobs.
Meter 0..127 log level for `RI_EV_METER` needs a log kernel — Task 12
GUI scope. The plan's file-structure note ("fader table" in params.c)
was superseded by the exact-formula E0 above; cost if wrong: none for
D1 (two multiplies, bit-deterministic), TC-2.6.1 re-fits anchors only.
