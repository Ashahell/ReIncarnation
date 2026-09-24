# 2026-09-24 — Owner requirement: extensible device rack (user-selectable active devices)

> Source: owner statement in session; recorded as review §5.6 + decisions D-k/D-l
> Collected: 2026-09-24
> Published: 2026-09-24
> Doc: [../../../docs/2026-09-24-improvement-opportunities.md](../../../docs/2026-09-24-improvement-opportunities.md) §5.6, §11, §12

## Disposition
New (requirement) + Update (architecture review verdict). No code change.

## Requirement (owner, verbatim intent)
"Ultimately, we want to be able to add other devices and the user should be
able to determine which devices are active and which aren't."

## Current state (code facts)
- `engine/framework/ridevice.h`: static table, `RI_DEVICE_COUNT 4u`, vtable =
  `render(ctx, out, n, sr)` only; dummy-device proof TC-2.1.5/TC-2.9.5.
- Section = hard-coded index elsewhere: `RIEvent.device` 0..3, control-ID
  blocks 0x03xx/0x04xx/0x09xx, 4-bus `RiMixer`, fixed panel table.
- Spec already locks the concept (§6, WBS §0: RIDevice = DSP + panel +
  control map + mod hooks), Appendix D signatures sketch-not-ABI (OPEN-07),
  W3/Power Mode as separate engine/capability table (§3).

## Adopted-for-proposal model (review §5.6)
- **Device class** (type ID + version, control table, pattern model, panel
  desc, mod slots, channels, Classic/Power capability) vs **device instance**
  (303A/303B = two instances of one class).
- **Rack** = ordered active instances; **Classic** = default preset 303, 303,
  808, 909.
- Addressing `(instance, control)`; events, automation, patterns, mixer
  channels, MIDI learn keyed by instance.
- Mixer channel per active instance; insert exclusivity routes to instances.
- Activation (add/remove/enable/disable, GUI + ARexx) built off the render
  path, applied via snapshot swap at a block boundary; disabled = zero CPU,
  state kept.
- RBNG `RACK` chunk; missing class → instance disabled + warning (mod-style);
  unknown class chunks preserved.
- Classic rack bit-identical to fixed-section engine (golden-pinned);
  non-Classic racks = Power Mode.
- Bounded max instances (e.g. 16), static allocation; compiled-in classes now,
  loadable device libraries only after OPEN-07 freeze.

## Decisions proposed
- D-k: extensible device rack as above.
- D-l: compile-time class registry now; loadable devices after ABI freeze.

## Effect on work order
Step 2 (control registry) → per-class control tables; step 3 → integrated
stereo engine built on the rack with the Classic preset.
