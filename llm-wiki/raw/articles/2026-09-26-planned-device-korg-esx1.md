# Planned device: Korg Electribe ESX-1 (owner plan)

> **Status: Outdated (2026-10-07).** The owner deferred the ESX-1 ("we can
> always add the Korg later"). The next rack device is a sampler/slicer. See
> [2026-10-07-sampler-slicer-replaces-esx1.md](2026-10-07-sampler-slicer-replaces-esx1.md).

- Source: owner statement in the ReIncarnation session, 2026-09-26 ("we plan to add a Korg ElecTribe ESX1")
- Collected: 2026-09-26
- Published: 2026-09-26
- Record: `docs/superpowers/specs/2026-09-26-device-korg-esx1-requirement.md`

## Statement
The owner plans to add a Korg Electribe ESX-1. It is the first named device beyond the four ReBirth sections (303A, 303B, 808, 909), and the concrete test case for the extensible device rack (spec D-k; `2026-09-24-extensible-device-rack-requirement.md`).

## Rules recorded for current work
- No code may assume exactly four devices (controls, patterns, events, song chunks, panels, mixer strips, lane keys, skins, control plane).
- A new device takes a new lane-key block (`0x0B`/`0x0C`/`0x0D` are allocated; `0x08xx` is reserved), mixer strips in the `0x0Bsp` scheme (the strip nibble has headroom), an appended skin section token, and its own optional RBNG chunks (chunk IDs are an owner review item).
- E1 for the device is the Korg ESX-1 Owner's Manual. Nothing about its architecture is recorded as fact until it is sourced from there.
- Clean-room: no Korg samples, ROM, factory patterns, firmware or artwork. Importing Korg file formats is a legal item (like OPEN-10).

## Open (owner)
- The scope of the first slice.
- Sample formats and storage path.
- Rack UI docking and the key map.
- The MIDI CC map.
