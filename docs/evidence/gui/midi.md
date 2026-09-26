# Remote MIDI Control — Standard Mapping (§12.10 G7) — ledger

**Status:** implemented; proven through real camd.library on riqemu1 2026-09-26, after fixing an AROS camd bug that blocked all CAMD routing on x86-64 (below).
**Source (E1):** ReBirth RB-338 2.0.1 Owner's Manual chapter 13 (p. 127–134), p. 144–145 (MIDI / Sync LEDs), Appendix C (p. 195–204).
**Code:** `gui/midimap.{h,c}` (t73), `gui/panelui.h` (mixer/FX pointers), `app/sectproof.c` (`RISECT remote`: CAMD receiver on cluster `ri.remote`), `app/midisend.c` (`MIDISEND <cluster> <script>` / `SELFTEST`).

## Mapping (E1)

| Message | Effect | Manual |
|---------|--------|--------|
| Control Change n | the panel control whose Appendix C controller is n (registry `ri_ctlreg_by_cc`; all 99 pinned in t73) | p. 195–198 |
| Note 64–96 | Various Switches, every mode: focus (65–68), Options toggles (95 Program Synth, 96 Select Patterns, 64 swap), transport (69 Play, 70 Stop, 71 Record, 72/73 Bar −/+), FX enables (74 PCF, 75 Delay, 76 Dist, 77 Comp), per-section Mix/Dist/PCF/Comp (78–93), Master Comp (94) | p. 199 |
| Note 12–63, Select Patterns on | per section 13 notes from 12: on/off, Bank A–D, Pattern 1–8 (focus follows) | p. 200–201 |
| Note 12–32 / 12–39, Program Synth on | focused synth: pitch C…C, Octave Down/Up, Accent, Slide, Note/Pause, Back, Step, Pitch Mode; focused rhythm section: Step 1–16 on/off, Instrument AC…(12) | p. 202–204 |
| any message except SysEx | MIDI LED | p. 144 |
| MIDI clock | Sync LED: red on the downbeat, green on the other beats | p. 145 |

The receiver listens on one channel only (p. 134) and never transmits (p. 127).

## Decisions (owner-confirmed 2026-09-26; was E0)

- **CC value laws:**
  - a knob or fader maps 0..127 linearly onto its range;
  - a switch is on at ≥ 64 (the panel is pressed only when its state differs);
  - an n-position selector splits 0..127 into n equal bands.
- **Note On with velocity 0 is a note-off** and does nothing; all switches act on Note On.
- **Manual typo, p. 199:** the table reads "B3 70 Record" and "A#3 71 Stop". The key column runs in order (C#4, C4, B3, A#3, A3) and the number column does not, so the numbers are swapped. Keys win: **A#3 = 70 = Stop, B3 = 71 = Record.**
- **Note 64 "Select Pattern/Program Synth":** swaps which of the two options is on.
- **Overlapping pattern and section notes:** the focused section's switches win when both options are on. Same rule as the typewriter keys (`keyboard.md`).
- **LED timings:**
  - the MIDI LED stays lit 100 ms per message;
  - the Sync LED lights for the first quarter of each beat (6 of 24 clocks), counted from MIDI Start, in 4/4;
  - MIDI Stop turns it off.

## Proof on riqemu1 (real camd.library)

`RISECT remote` shows the Transport, 808 + mixer, 909 + mixer and the Pattern sections, with a CAMD receiver link on cluster `ri.remote`. `MIDISEND` plays `2026-09-26-remote1.mid.txt`, then `…remote2.mid.txt`, through a CAMD sender link on the same cluster. That is the real library path, with no MIDI hardware. Trace `2026-09-26-riremote-trace.txt`, capture `img/2026-09-26-riremote-camd.png`:

- Program Synth on, focus 808, instrument BD, steps 1 5 9 13 by note: BD row `x...x...x...x...`, SEL8 1.
- CC 38 = 127: 808 BD Level at maximum. CC 17 = 30: 808 mixer fader at 30. Note 88: 808 PCF LED lit.
- Select Patterns on, note 63: 909 pattern 8 and the focus moved to the 909 (orange bar).
- Play on channel 2: ignored (`MIDI 13/1` — one ignored). Play on channel 1: playback starts, running light moves.
- MIDI Start + 26 clocks: Sync LED green on beat 2. MIDI LED times out between bursts.
- Stop (70) + MIDI Stop: stopped, both LEDs off. 43 messages in, 43 through CAMD.

## AROS bug found and fixed: camd.library cluster names on x86-64

- **Symptom:** a CAMD receiver never got anything. In-process too: one sender, one receiver, same cluster name, `MidiLinkConnected() = 0`, `GetMidi()` empty. `NextCluster()` listed several clusters named `camd…` / `PrNK` — never the requested name.
- **Root cause:** `workbench/libs/camd/strings.c` `mysprintf()` read its varargs as `void *start = &fmt + 1` — stack-passed arguments, which is m68k-only. On x86-64 they arrive in registers. So:
  - `NewCluster()` copied the cluster name from stack garbage;
  - `FindName()` never matched;
  - every `AddMidiLink()` created a private cluster, and no two CAMD links could ever talk.
  - The same function builds the `devs:Midi/%s` driver paths and the `%s.in.%ld` / `%s.out.%ld` driver clusters, so hardware MIDI is affected too.
- **Fix:** read the arguments through a `va_list` with `VNewRawDoFmt(fmt, RAWFMTFUNC_STRING, string, args)`. Diff: `2026-09-26-camd-mysprintf-x86_64.diff`. It is applied to the v1 AROS tree, `make workbench-libs-camd` built it (53,408 B), and it is deployed to riqemu1 `DH0:Libs/camd.library` (original kept as `camd.library.orig`). The self-test then saw the right cluster name, both senders connected, and both messages with the correct bytes.
- **Handoff:** the diff belongs in the Vulkan4AROS v1 patch series (`src/abi-patches/v1/aros/`, currently on another session's branch) and upstream AROS.
- **Second issue, exposed by the fix and not solved here:** with paths now formatted correctly, camd's init really loads `DEVS:Midi/debugdriver`, and the first `OpenLibrary("camd.library")` then blocks. On riqemu1 the driver is parked at `SYS:debugdriver.parked`. Real hardware drivers (e.g. USB-MIDI on the Dell) need that driver-load path investigated before MIDI hardware can be used.

## Lane state changes (riqemu1)

- `DH0:Libs/camd.library` replaced (backup `.orig`).
- `DEVS:Midi/debugdriver` parked at `SYS:debugdriver.parked`.
- GRUB default pinned to "VESA 1280x1024-32bpp" (backup `DH0:boot/grub/grub.cfg.orig`), because an unpinned reset came back at 800x600.

## ReBirth-101 CC sweeps (2026-09-26)

`RISECT remote` + `MIDISEND ri.remote` scripts `2026-09-26-rb101-cc25.txt`
(CC 25 0/64/127, 303 cutoff) and `2026-09-26-rb101-cc17.txt` (CC 17 0/127,
808 bus fader). Trace `2026-09-26-rb101-remote.txt`: all 7 messages taken,
0 ignored. Fader cap top (127) vs bottom (0) pixels in
`img/2026-09-26-rb101-fader-hi-lo.png`. Value mapping is G7-proven CC38 on
byte-identical code (midimap/sectmix/sectui/ctlreg untouched since `d576987`);
the cutoff knob has no canvas in remote mode, so its pointer + the hearing
stay open. Mute buttons are not automatable (p. 72); Solo is not a ReBirth
control (mixer mute, p. 56).
