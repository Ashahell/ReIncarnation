# Planned device: sampler/slicer (owner requirement, 2026-10-07)

**Status:** planned. Recorded, not designed; no code yet.

**Owner decision (2026-10-07):**
- Drop the Korg ESX-1 for now and add a **sampler/slicer device**: sample
  parts, one slicing part, and a filter and envelope per part.
- "We can always add the Korg later."

**Parent requirement:** the extensible device rack (spec D-k; owner requirement
2026-09-24; llm-wiki `raw/articles/2026-09-24-extensible-device-rack-requirement.md`).
This device replaces the ESX-1 as the next rack device. The ESX-1 is **deferred,
not cancelled**: `docs/superpowers/specs/2026-09-26-device-korg-esx1-requirement.md`.

## Why this device and not the ESX-1 (advisor assessment, 2026-10-07)

- **Synthesis is already covered.** The ESX-1 is a sample-playback groovebox,
  not a synth, and it would add little synthesis to the rack:
  - Leviasynth covers polyphonic, FM, phase-distortion and pad sounds;
  - the two 303s cover bass.
- **The clean-room rule cuts out its sounds.** No Korg samples, ROM or factory
  patterns may ship, so a clean-room ESX-1 would arrive as an empty sampler
  with an ESX-1 layout. Its drums would overlap the 808 and 909.
- **Its character is expensive to reproduce.** Much of the ESX-1's sound is the
  tube output stage and the factory library, and both would need hardware
  measurement or are off limits.
- **The missing category is samples, not synthesis.** Nothing in the rack
  plays the user's own audio today: vocal chops, breaks, loops, one-shots.
  Slicing a loop into tempo-following hits is also missing.
- **The rack gains the same test.** A generic sampler is a different device
  class with its own data (sample files), load paths and memory use. It
  exercises the rack as well as the ESX-1 would, without the Korg evidence
  chain (manual, hardware) or the Korg-format legal questions.
- **The ESX-1 can come later.** Its look and workflow could still be added as a
  skin or a variant on top of this device.

## Scope (owner, 2026-10-07)

- **Sample parts:** each plays one sample, as a one-shot or held/looped
  according to the design.
- **One slicing part:** a loop cut into slices that follow the song tempo.
- **Per part:** a filter and an amplitude envelope.

Everything else is a design decision for the owner, listed below. Nothing
here fixes numbers.

## Rules that apply now (inherited from the rack and ESX-1 requirement)

1. **No four-device assumptions.** Controls, patterns, events, song chunks,
   panels, mixer strips, automation lane keys, skins and the control plane are
   keyed by device instance.
2. **Automation keys:** a new lane-key block. `0x0E00` (Levi) and `0x0F00`
   (Levi per-op) are taken, `0x08xx` stays reserved. Record the allocation in
   the automation spec's block table before code lands.
3. **Mixer:** own strip or strips in the `0x0Bsp` scheme. Record the strip
   numbering in the rack design.
4. **Skins:** a new section token, appended in `gui/ctlreg.c`.
5. **Songs (RBNG):** own optional chunks, since readers skip unknown optional
   chunks (spec §13). The chunk IDs are an **owner review item**.
6. **Realtime (spec §4, LOCKED):**
   - **Loading:** all sample loading, decoding and slicing analysis happens on
     the control plane, never in the render path.
   - **Rendering:** the render reads preallocated, immutable sample buffers
     handed over at buffer boundaries. There is no allocation, IO or locking
     in the render path.
7. **Clean-room and samples:**
   - Ship no third-party factory content.
   - User samples belong to the user.
   - Any bundled demo content follows the existing open-source sample rules:
     the classic-01 manifest pipeline, CC0/BY only, no GPL/NC/factory
     (llm-wiki `2026-09-27-trademark-hygiene`).
8. **Portability:** sample paths go through `ri_pal_path` (portability plan).
   AROS and host behave the same.
9. **Performance:** the Dell is the target (ABIv11). Measure the device on the
   host first, then A/B it on the Dell, as for Levi.

## Open questions (owner; do not decide)

- **Number of sample parts.**
- **Per-part controls** beyond filter and envelope: pitch/tune, start/end,
  reverse, loop mode, velocity, choke groups.
- **The slicing method:** fixed divisions, transient detection, or both; slice
  count limits; how slices map to steps or notes; time-stretch versus
  repitch when the tempo differs.
- **Sample formats and limits:** WAV/AIFF, bit depths, sample rates
  (resampling to the engine rate), and the maximum memory per sample and per
  song on the Dell.
- **Where user samples live** on AROS (`SYS:`/`PROGDIR:`, or the songs
  library), and whether songs embed samples or reference them.
- **Sequencing:** its own step pattern like the 808/909, notes from the song
  track, or both.
- **The panel:** how the device docks in the rack (tab, section, rail chip),
  plus focus and keyboard map.
- **MIDI mapping** (interop requirement).
- **The filter model:** reuse an existing engine filter (Levi's digital or
  analog filter, or the 303 ladder) or build a new one.

## Where it is tracked

- `docs/2026-09-24-improvement-todo.md`, section "Planned devices".
- Memory: `planned-device-sampler-slicer` (the ESX-1 memory is marked deferred).
- llm-wiki: `raw/articles/2026-10-07-sampler-slicer-replaces-esx1.md`.
