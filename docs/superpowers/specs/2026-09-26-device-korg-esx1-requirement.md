# Planned device: Korg Electribe ESX-1 (owner requirement, 2026-09-26)

**Status:** planned — recorded, not designed. No code yet.
**Owner statement (2026-09-26):** "we plan to add a Korg ElecTribe ESX1".
**Parent requirement:** the extensible device rack (spec D-k; owner requirement 2026-09-24; llm-wiki `raw/articles/2026-09-24-extensible-device-rack-requirement.md`). The ESX-1 is the **first named device beyond the four ReBirth sections** (303A, 303B, 808, 909), so it is the concrete test case for the rack architecture.

## What this means for current work (applies now)

1. **No new code may assume exactly four devices.** Controls, patterns, events, song chunks, panels, mixer strips, automation lane keys, skins and the control plane are keyed by device instance, never by a hard-coded section count. Where today's code is fixed to four, leave a single named constant or table, and never scatter literals.
2. **Keys and IDs:** lane-key blocks `0x0B` (strips), `0x0C` (808), `0x0D` (909) are allocated (`engine/seq/autolane.h`). A new device takes a **new block**. Record the allocation in the automation spec's block table before code lands. `0x08xx` stays reserved.
3. **Mixer:** the ESX-1 needs its own strip or strips (level, pan, send, inserts) in the `0x0Bsp` scheme. `s` is a 4-bit strip nibble today (1..5 used), so there is headroom, but record the numbering in the rack design.
4. **Skins:** format 1 already ignores unknown section tokens (a later build's device), so older builds tolerate an `esx1` token. Allocate the token in `gui/ctlreg.c` (append-only) with the device.
5. **Songs (RBNG):** a device instance needs its own optional chunks; readers skip unknown optional chunks (spec §13). The chunk IDs are an **owner review item** (file format).

## Fidelity and legal (apply when designing)

- **Evidence order for this device:**
  1. **E1** = the Korg ESX-1 Owner's Manual (obtain it; cite printed pages).
  2. The hardware.
  3. **E0** = ledgered defaults.

  Nothing about the ESX-1's architecture (parts, sequencer, effects, sampling limits) is written here as fact until it is sourced from E1; verify every number.
- **Clean-room (spec §1):** no Korg samples, ROM content, factory patterns, firmware or panel artwork. Measure proportions, author original art (as for 808-RI), and ship no factory sample content. User-supplied samples are the user's own. Any import of Korg file formats (patterns/samples from a real unit) is an **OPEN-10-style legal item**: design the importer only after an owner and legal decision.
- **Naming:** use "ESX-1" descriptively; no Korg logo or trade dress in the UI or skins.

## Open questions (owner; do not decide)

- The scope of the first ESX-1 slice: drum parts only, or sampler + sequencer + effects parity?
- Sample import formats and limits (WAV/AIFF), and where user samples live on AROS (`SYS:`/`PROGDIR:` path; ties into the portability plan's `ri_pal_path`).
- How it docks in the rack UI: its own window or panel section; the focus and keyboard map (Appendix E has no ESX-1 keys).
- The MIDI mapping (Appendix C covers ReBirth only; the ESX-1 has its own CC map — E1).

## Where it is tracked

- `docs/2026-09-24-improvement-todo.md`, section "Planned devices".
- Memory: `planned-device-esx1` (linked from `extensible-device-rack`).
- llm-wiki: `raw/articles/2026-09-26-planned-device-korg-esx1.md`.
