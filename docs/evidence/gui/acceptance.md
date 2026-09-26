# GUI acceptance — Task 12 (gate G12), TC-2.9.x–2.11.x, ReBirth-101

Status 2026-09-26 (G8.3): the Task-12 knob-widget document rewritten for the
section canvases (G2–G7), skins (G8.1) and zoom (G8.2). Every ticked row
points to evidence; every unticked row says why. Spec §13: functional
workflow parity is the requirement; Classic does NOT promise pixel/feel
parity with reference artwork. Layout numbers are E0 design acceptance
thresholds.

Machine proof lanes: riqemu1 (QEMU `sendkey` real keys, `MIDISEND` CAMD
scripts, monitor screendumps). No lane injects mouse drags; riqemu1 has no
audio device (G6b open) and the Dell is unavailable — hearing, drags and
the human hand stay open below.

## Section canvases (G2–G7 + G8.1–G8.2)

- [x] 303/808/909/mixers+master/FX/transport+patterns laid out, behaving,
  rendered — `section-canvas-proof.md`, `img/2026-09-25-*`
- [x] Keyboard + focus bar (G5) — `keyboard.md`, `2026-09-25-rikeys-trace.txt`
- [x] Live state on the stand-in clock (G6a) — `live-state.md`,
  `2026-09-25-rilive-trace.txt`
- [x] Remote MIDI Standard Mapping (G7) — `midi.md`,
  `2026-09-26-riremote-trace.txt`
- [x] Skins: Classic vs 808-RI vs Template vs missing + live Ctrl+M cycling
  (G8.1) — `skins-g81.md`, `img/2026-09-26-skin-*`
- [x] Zoom 1x/1.5x/2x/0.75x Classic + skinned (G8.2) — `zoom-g82.md`,
  `img/2026-09-26-zoom-*`

## Owned-UX numbers (P-18, TC-2.9.2 — host-verified in t1_knob)

- [x] Knob: 150 px drag on either axis = full 0..127 (host ±5% band
  + owner device-tested m25–m29: horizontal full range, vertical to
  max with grab, both directions)
- [x] Reversal inside one press follows at once (acc clamped m29;
  owner-approved — no fresh click needed)
- [x] Shift-fine: ×0.1 (owner device-confirmed single steps m32)
- [ ] Fader: 100 px = full travel, same fine rule (needs a drag: human)
- [x] Commit-on-release: one drag = one undo unit (single commit
  event — UNDO counter proves exactly-once per gesture, m34,
  owner-approved)
- [x] Right-click: resets control to panel default (owner confirmed
  LEVEL→100 by readout m32; table host-pinned)
- [x] Click down/up on a custom knob: no crash, value steady (Dell
  2026-09-24, m22 — full drag needs a hold-and-drag primitive or a
  human hand; remote click = down/up with no move)
- [x] 150 px screen travel holds at every zoom (structural: canvas feeds
  raw screen deltas; t76 pins the law zoom-independent) — true drags
  still need a hand (no lane injects mouse)

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
- [x] Artwork authored at 2x, filtered down, no shimmer on static panels
  (G8.2: `zoom-g82.md` — 1x/1.5x/2x/0.75x captures Classic + skinned)

## Step LEDs + chase (TC-2.10.x)

- [x] LED update ≤ 1 frame (33 ms) of pattern position (G6a: taps land on
  the playhead from the ≤100 ms IntuiTicks pass; running light captured
  on 808/909 step 6 — `live-state.md`)
- [x] 16th-note chase stutter-free at 174 BPM (86.2 ms/step) (G6a: BD taps
  exactly 4 steps apart at 0.5 s/120 bpm on the stand-in clock; audio-clock
  binding is G6b)
- [x] 303 accent red glow / 909 flam green glow read correctly (G6a
  captures; accent/slide flags traced 2026-09-26, see ReBirth-101 row 2)

## Zoom (TC-2.11.x)

- [x] 1x / 1.5x / 2x all crisp (downscale from 2x masters) (`zoom-g82.md`)
- [x] Knob travel stays 150 px of *screen* travel at every zoom (above)

## Dummy panel (TC-2.9.5 generality proof)

- [ ] Dummy 2-control panel instantiates through the same MCC path
  with zero framework edits. Reason: the registry/geometry/sectui/canvas
  are compile-time tables — a 19th section needs 5 DATA touch-points
  (RI_SEC enum + COUNT, panelgeo entry, sectui union member + dispatch
  arm, rsection bg arm or default, ctlreg rows) and zero LOGIC changes
  (no new dispatch mechanism). The 6 shipped panel families (303, 808,
  909, mixer, FX, transport, pattern) already prove the canvas is fully
  data-driven. A runtime registry would remove even the data edits —
  deferred to the extensible-device-rack design (owner, review §5.6).

## ReBirth-101 tutorial workflow (≥4/5 to pass)

Machine lanes prove the GUI halves; hearing needs audio (G6b + Dell),
drags/clicks need a human hand. Evidence 2026-09-26 (`2026-09-26-rb101-*`).

- [ ] 1. Dial a 303 cutoff sweep and hear it — PARTIAL: CC 25 applied
  0/64/127 through the real CAMD path (`MIDISEND rc=0`, remote trace
  takes every message, 0 ignored; value mapping is G7-proven CC38 on
  byte-identical code). No 303 canvas in remote mode for the pointer,
  no audio for the ear. Awaits: Dell/human.
- [ ] 2. Program a 16-step 303 pattern (pitch mode + slide flag) —
  PARTIAL: 16 pitches entered through real keys in Step mode (trace
  EDIT STEP 1→16, slide on step 5, accent on step 9; capture EDIT 16).
  Pitch MODE itself has no keyboard binding (panel switch, mouse-only),
  so pitch-mode auto-advance is unproven. Awaits: human.
- [ ] 3. Program a 909 accent + flam step — PARTIAL: BD/SD/CH low taps
  applied through real keys while playing (trace change-lines N29/N42/N55,
  CH lamp in readout + capture). Accent + flam LEVELS are double-click /
  selector interactions (mouse-only). Awaits: human.
- [ ] 4. Ride a bus fader + mute/solo the 808 — PARTIAL: CC 17 rode the
  808 bus fader 0→127 (cap top vs bottom pixels,
  `img/2026-09-26-rb101-fader-hi-lo.png`). Mute buttons are not
  automatable (p. 72, registry-pinned) → mouse-only. Solo does not
  exist in ReBirth (the mixer mute, p. 56, is the ReBirth control; the
  row's "solo" is out of E1). Awaits: human for mute.
- [x] 5. Switch patterns under transport without stopping — PROVEN:
  `kp_enter` (ST 1) then pattern key `3` → PAT A1→A3 with ST still 1,
  then `kp_0` stops (trace N20→N22).

Score: 1 / 5 full, 4 / 5 partial (pass ≥ 4 full). The tutorial stays
OPEN pending the human/audio pass; nothing machine-provable is left
untried (the `sendkey comma` keycode anomaly is recorded in `keyboard.md`
— avoided in the scripts, uncharacterized).

## Sign-off

- Tester: ____________________ Date: __________ Build: __________
- Tester: ____________________ Date: __________ Build: __________
