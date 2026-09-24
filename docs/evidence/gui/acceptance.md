# GUI acceptance — Task 12 (gate G12), TC-2.9.x–2.11.x

Host side (this commit): `tests/unit/t1_knob.c` green (`PASS knob`);
MCC shells + `app/main.c` AROS-compile-clean. Everything below needs
the on-device pass — **unchecked = gate red on device, expected until
the AROS run**. Spec §13: functional workflow parity is the
requirement; Classic does NOT promise pixel/feel parity with reference
artwork. Layout numbers are E0 design acceptance thresholds.

## Owned-UX numbers (P-18, TC-2.9.2 — host-verified in t1_knob)

- [ ] Knob: 150 px drag on either axis = full 0..127 (±5% host band holds on device; vertical = ReBirth, horizontal added per owner)
- [ ] Shift-fine: ×0.1 (1500 px full, ±10% host band holds on device)
- [ ] Fader: 100 px = full travel, same fine rule
- [ ] Commit-on-release: one drag = one undo unit (single commit event)
- [ ] Right-click: resets control to panel default (`ri_panel_default_ctl`, host-pinned)
- [x] Click down/up on a custom knob: no crash, value steady (Dell
  2026-09-24, m22 — full drag needs a hold-and-drag primitive or a
  human hand; remote click = down/up with no move)

## Silhouette + artwork (TC-2.9.x)

- [x] Control centers ±2 px vs panel-909-geometry doc (v2 lock
  2026-09-23/24: 40/110/180/250 @ y52, pitch 70 from hardware 1.35
  ratio; Dell m22 re-measurement 0 px x-error net of MUI chrome
  offset, y via pointer-tip geometry; compact 33 px numbers struck
  in the geometry doc — see it, not here)
- [x] Custom RKnB class renders measured 909 art on device (Dell
  2026-09-24, m22: four olive knobs, `#e37c3b` pointers byte-exact,
  tick rings, MUIC_Knob retired)
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
