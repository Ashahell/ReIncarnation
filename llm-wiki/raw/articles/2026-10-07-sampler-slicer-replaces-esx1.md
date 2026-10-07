# The Korg ESX-1 is deferred; the next rack device is a sampler/slicer (owner decision, 2026-10-07)

- Source: ReIncarnation session (advisor lane), 2026-10-07. Owner decision after the advisor's assessment of what the ESX-1 would add.
- Collected: 2026-10-07
- Published: 2026-10-07
- Spec: `docs/superpowers/specs/2026-10-07-device-sampler-slicer-requirement.md`; the ESX-1 requirement is marked DEFERRED in `docs/superpowers/specs/2026-09-26-device-korg-esx1-requirement.md`
- Related: [2026-09-26-planned-device-korg-esx1.md](2026-09-26-planned-device-korg-esx1.md), [2026-09-24-extensible-device-rack-requirement.md](2026-09-24-extensible-device-rack-requirement.md), [2026-09-27-trademark-hygiene.md](2026-09-27-trademark-hygiene.md)

## The question

The owner asked for a brutally honest answer: would the Korg ESX-1 add anything
to the current set of synths?

## The assessment

- **As a synth: almost nothing.**
  - The ESX-1 is a sample-playback groovebox. Each part plays a sample through
    a filter and an amp envelope.
  - Leviasynth (8 voices × 8 operators, morph banks, mod matrix, two filters,
    FX) already goes far beyond that, and the two 303s cover bass.
- **Overlap:**
  - its drum parts overlap the 808 and 909;
  - its step and motion sequencing overlap patterns, song mode and automation
    lanes.
- **Clean-room cost:**
  - no Korg samples, ROM or factory patterns may ship, so a clean-room ESX-1
    would arrive as an empty sampler with an ESX-1 layout;
  - its character (the tube output stage and the factory library) would need
    hardware measurement or is off limits.
- **What it would add:** sampling and slicing of the user's own audio. That is
  the one category missing from the rack today, and it is cheap on the Dell's
  CPU.

## The decision (owner, 2026-10-07)

- "document that we'll drop the Korg for now and add a sampler/slicer device
  (sample parts, one slicing part, per-part filter and envelope). We can
  always add the Korg later."
- The sampler/slicer becomes the next rack device and the test case for the
  extensible rack.
- The ESX-1 is **deferred, not cancelled**. Its look and workflow could later
  become a skin or variant on top of the sampler.

## What carries over

- **Rack rules:**
  - no four-device assumptions;
  - a new automation lane-key block (`0x0E00`/`0x0F00` are Levi's,
    `0x08xx` reserved);
  - its own mixer strip;
  - an appended skin token;
  - its own RBNG chunks, whose IDs are an owner review item.
- **Realtime:**
  - sample loading, decoding and slice analysis run on the control plane;
  - the render reads preallocated immutable buffers.
- **Samples:** user samples belong to the user; bundled demo content follows the
  CC0/BY manifest pipeline.

## Open questions (owner)

- Part count and the per-part controls beyond filter and envelope.
- The slicing method (fixed or transient), and repitch versus stretch.
- Formats, rates and memory limits on the Dell.
- Where samples live, and whether songs embed or reference them.
- How the device is sequenced.
- The panel and docking.
- The MIDI map.
- Whether to reuse an existing filter model.
