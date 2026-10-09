# §12.10 G7: Remote MIDI Control (Standard Mapping) over CAMD — and an AROS camd fix

> **Status: two items Outdated (2026-10-08); the receiver is extended (2026-10-09).**
>
> 1. **The local camd diff no longer needs to go anywhere — it is upstream.**
>    Jaime Dias fixed the same `mysprintf()` bug on 2026-10-05 as
>    `28ec43a517` "camd: format cluster names through a va_list", an ancestor of
>    `origin/master`. Our local diff is **retired as superseded** (note kept
>    beside the file) and the upstream diff is carried verbatim, with a credit
>    header, in both ABI carriages.
> 2. **The `debugdriver` block is also fixed upstream, by a second bug.**
>    `cb8c4c5f39` "camd: skip arena hunks when scanning a driver for
>    MidiDeviceData": the ELF arena loader stores hunk size 0, so the unscanned
>    `LoadDriver()` walked ~4 GB off the end of memory. That — not the driver —
>    is why loading `DEVS:Midi/debugdriver` blocked the first camd open. The
>    driver is un-parked and the first open completes.
> 3. **The receiver is no longer the only one.** M4 (`ff736d2`) keeps G7 exactly
>    as it was on channel 1 and adds a second device (the Leviasynth, channel 2)
>    with its own map, because `ri_ctlreg_by_cc` is global and one channel
>    cannot serve two instruments. **G7's one-channel law (p. 134) still holds
>    for G7**: it is now one channel per device, not one channel overall.
>
> Proof of the two CAMD fixes on the Dell, SELFTEST before and after the owner's
> reboot: senders `connected: 0, 0` → `1, 1`; cluster name garbage
> (`00 61 6D 64`) → readable (`6D 30 70 6F`, "m0po…"); echo `got 0` rc=20 →
> `got 2` rc=0. Detail in [2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md](2026-10-09-levi-wiring-and-midi-w0-w4-m0-m4.md).

- Source: ReIncarnation commit `d576987`; ledger `docs/evidence/gui/midi.md`; trace `docs/evidence/gui/2026-09-26-riremote-trace.txt`; capture `docs/evidence/gui/img/2026-09-26-riremote-camd.png`
- Collected: 2026-09-26
- Published: 2026-09-26

## What landed

- `gui/midimap.{h,c}` (t73):
  - Control Change → the registry control with that Appendix C controller (all 99 pinned);
  - Note On → "Various Switches" (p. 199, every mode), Pattern Selection (p. 200–201, 13 notes per section from 12) and the focused section's switches (p. 202–204);
  - one channel (p. 134); running status, realtime interleave, SysEx skip;
  - MIDI LED on any non-SysEx message (p. 144);
  - Sync LED from MIDI clock, red on the downbeat and green on the other beats (p. 145).
- `app/midisend.c` (`MIDISEND <cluster> <script>` / `SELFTEST`) and `RISECT remote`, which puts a CAMD receiver on cluster `ri.remote`.

## E0 decisions

- CC value laws: linear for knobs and faders; a switch is on at ≥ 64; selectors split 0..127 into equal bands.
- Velocity 0 = note-off.
- Manual typo p. 199: the Stop/Record note numbers are swapped; the key column wins, so 70 = Stop, 71 = Record.
- Note 64 swaps which of the two options is on.
- Section notes win over pattern notes, the same rule as the typewriter keys.
- MIDI LED 100 ms per message; Sync LED lit for the first quarter of each beat.

## Proof

Through real camd.library on riqemu1: MIDISEND played a script into the cluster that `RISECT remote` listens on.

- BD steps 1/5/9/13 were set by note.
- CC 38 put the 808 BD Level at maximum; CC 17 put the 808 mixer at 30.
- Note 88 lit the 808 PCF LED.
- 909 pattern 8 was selected and the focus followed.
- A channel-2 Play was ignored; the channel-1 Play started playback.
- The Sync LED went green on beat 2.
- Stop and MIDI Stop stopped everything.

## AROS finding (fixed on the AROS side)

- **The bug:** `workbench/libs/camd/strings.c` `mysprintf()` used `&fmt+1` as its varargs stream (m68k-only). On x86-64 every cluster name was garbage, so no two CAMD links could ever connect — application routing and hardware drivers both.
- **The fix:** `va_list` + `VNewRawDoFmt(fmt, RAWFMTFUNC_STRING, …)`, built with `make workbench-libs-camd` and deployed to riqemu1. Diff in `docs/evidence/gui/`; it still has to enter the Vulkan4AROS v1 patch series and upstream. → **Done 2026-10-08** as `28ec43a517`; the diff is in both ABI carriages and the local copy is retired.
- **Exposed, still open:** loading `DEVS:Midi/debugdriver` blocks the first camd open. The driver is parked on the lane (`SYS:debugdriver.parked`). → **Closed 2026-10-08**, see the Status block above: the cause was a second upstream camd bug (`cb8c4c5f39`), not the driver.

## Lane

riqemu1 GRUB default pinned to "VESA 1280x1024-32bpp", because an unpinned reset came back at 800x600. The owner's rule: always reboot riqemu1 at 1280x1024.
