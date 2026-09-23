# GUI acceptance — Task 12 (gate G12), TC-2.9.x–2.11.x

Host side (this commit): `tests/unit/t1_knob.c` green (`PASS knob`);
MCC shells + `app/main.c` AROS-compile-clean. Everything below needs
the on-device pass — **unchecked = gate red on device, expected until
the AROS run**. Spec §13: functional workflow parity is the
requirement; Classic does NOT promise pixel/feel parity with reference
artwork. Layout numbers are E0 design acceptance thresholds.

## Owned-UX numbers (P-18, TC-2.9.2 — host-verified in t1_knob)

- [ ] Knob: 150 px vertical drag = full 0..127 (±5% host band holds on device)
- [ ] Shift-fine: ×0.1 (1500 px full, ±10% host band holds on device)
- [ ] Fader: 100 px = full travel, same fine rule
- [ ] Commit-on-release: one drag = one undo unit (single commit event)
- [ ] Right-click: resets control to panel default (`ri_panel_default_ctl`, host-pinned)

## Silhouette + artwork (TC-2.9.x)

- [ ] Control centers ±2 px @1024×768 vs panel-geometry doc
- [ ] Proportions ±1% (style-matched type, never pixel-copying)
- [ ] Artwork authored at 2x, filtered down, no shimmer on static panels

## Step LEDs + chase (TC-2.10.x)

- [ ] LED update ≤ 1 frame (33 ms) of pattern position
- [ ] 16th-note chase stutter-free at 174 BPM (86.2 ms/step)
- [ ] 303 accent red glow / 909 flam green glow read correctly

## Zoom (TC-2.11.x)

- [ ] 1x / 1.5x / 2x all crisp (downscale from 2x masters)
- [ ] Knob travel stays 150 px of *screen* travel at every zoom

## Dummy panel (TC-2.9.5 generality proof)

- [ ] Dummy 2-control panel instantiates through the same MCC path
      with zero framework edits

## ReBirth-101 tutorial workflow (≥4/5 to pass)

- [ ] 1. Dial a 303 cutoff sweep and hear it
- [ ] 2. Program a 16-step 303 pattern (pitch mode + slide flag)
- [ ] 3. Program a 909 accent + flam step
- [ ] 4. Ride a bus fader + mute/solo the 808
- [ ] 5. Switch patterns under transport without stopping

Score: ___ / 5 (pass ≥ 4)

## Sign-off

- Tester: ____________________ Date: __________ Build: __________
- Tester: ____________________ Date: __________ Build: __________
