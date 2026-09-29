# Leviasynth fidelity, phase P1: hardware panel and page UI (2026-09-30)

Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md`.

## What the owner asked

"Look and sound as closely like the actual hardware as possible … emulate all features … keep using the current font, in an appropriate contrasting color."

P1 is the look and the page UI. Sound phases P2–P10 follow the plan.

## Reference

- The top-panel render on the cover of the Leviasynth Keyboard Owner's Manual, rasterised at 400 dpi.
- It is used for arrangement and measurement only and is **not in the repo** (clean-room).
- Control positions were measured on it and mapped `xQ = (x-75)*0.6245`, `yQ = (y-48)*0.6245` into a 1756 × 560 Q section (the Drums row width, so window fit is unchanged). The mapping lives in `RI_GEO_LEVI` (`gui/panelgeo.c`) and the art tables in `gui/draw/art_levi.c`.
- Colours were sampled from the render: panel `0x202121`, section frames and title bands black, titles `0x22BCB9` teal.

## What changed

**Panel (`gui/draw/art_levi.c`)** — the hardware order:
- a left column (volume, balance, Single/Multi, Lower/Both/Upper, Glide, Ribbon, octave, Chord);
- CV/Gate;
- Arpeggiator & Sequencer Control;
- Main Systems;
- Master Control (8 LED-ring encoders around the LCD);
- Osc Env Level & Bias, Digital Filter and Analog Filter;
- Algorithm (white 2-digit readout, encoder, ALGO EDIT);
- Module Select (Oscillator Group Edit, the Env/Filter/VCA/FX/LFO chain, OSC 1–8 in their colour order);
- the ribbon, then the keybed with pitch/mod wheels.

**Clean-room:** no maker logo or product wordmark. The mark slot reads "LEVI". A new audit gate rejects maker/product marks in any panel string.

**Font:** the house legend face. Titles and cap legends are teal, knob legends near-white, and labels fall back to small print or a short form when they do not fit. The `&` glyph was redrawn (the generated one read as a 9).

**Live controls:**
- Digital Filter Cutoff/Resonance; Analog Filter Cutoff/Resonance/Pre-Drive; Osc Env Attack/Decay/Release;
- the Algorithm encoder;
- ARP ON and SEQ ▶;
- every Module Select key and OSC 1–8;
- the 8 encoders;
- the ribbon (16 chord steps);
- the 13 active keys;
- STEP EDIT (our own block for the chord editor, labelled as ours).

Hardware controls whose engine comes in a later phase are drawn at lower brightness and are not hit-testable.

**Page UI (`gui/sectlevi.c`):**
- A module key or OSC key selects a page.
- The 8 encoders edit the page's slots in the manual's knob order.
- The LCD shows the title and name/value per slot.
- An encoder sends its **target** parameter (`ri_sui_ctl_idx`), so automation keys and the control plane are unchanged.
- Group MODE edits op k with encoder k.

**Zoom:** the dense panel takes one zoom step above the app zoom when the whole window still fits (`ri_zoom_levi`). On the Dell, the app runs at 1x and the Levi page at 1.5x (log `RIAPP zoom: levi=1`).

## Tests

- **t107:** page UI behaviour. Mutants killed: encoder target send, group-mode op index, switch midpoint.
- **t121:** Levi zoom step. Mutant killed: never step up.
- **t61:** new Levi geometry laws (knob row, encoder columns, ribbon steps, OSC row, keybed rows). Page-reached controls need no panel item; the rotary Algorithm encoder reaches every value.
- **t112:** page-linked Levi controls redraw in full, because they repaint the LCD and rings. The wide count moved 21→35, deliberately.
- **t93:** Levi raster laws (teal titles, a legend on every live cap, a dark LCD with teal text, encoder rings lit only on live slots). Three mutants killed.
- **t92/t93:** section 18 re-pinned (4 pins each); no other section moved.
- `AUDIT 0/0 PASS` (clean worktree, private `/tmp/ri`).

## Dell (ABIv11, 1366x768)

- `RAM:RIAPPLV`: `open=1 rack=1 tabs=5`, `levi=1`.
- Clicking the Levi tab shows the panel (`dell-p1-levi-tab.png`).
- Clicking DIGITAL FILTER turns the LCD to the Digital Filter page, with the live encoder rings lit (`dell-p1-dfilt-page.png`).

## Images

- Host renders: `host-before-z2.png` (the old panel), `host-p1-z0/z1/z2.png`.
- Dell captures (half scale): `dell-p1-*.png`.
