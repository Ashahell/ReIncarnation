# Planned device: ASM Leviasynth (owner requirement, 2026-09-28)

**Status:** planned — recorded, not designed. No code yet.
**Owner statement (2026-09-28):** "we need you to add a new synth: the ASM
Leviasynth … be as faithful to the original hardware and sound as possible".
**Parent requirement:** the extensible device rack (spec D-k; owner
requirement 2026-09-24; llm-wiki
`raw/articles/2026-09-24-extensible-device-rack-requirement.md`). The
Leviasynth is a polyphonic algorithmic synth — the first device that is
neither a 303-style mono synth nor a drum machine, so it is the concrete
test case for rack generality (classes, not just instances).

## E1 evidence (researched 2026-09-28; manual PDFs kept OUT of the repo)

- **E1a** = Leviasynth Keyboard Owner's Manual v1.2.1 (ASM downloads;
  read in `/tmp`, never committed). Cited below as manual p. N.
- **E1b** = Leviasynth Algorithms chart PDF (topology reference).
- **E1c** = Gordon Reid, Sound On Sound July 2026 review (architecture
  overview, free portion).
- **E1d** = KVR product page (specs: 16 voices, 8 osc/voice, 13 env, 5
  LFO, 32-slot matrix, Duo 8+8, $2499, keyboard + desktop).

## Architecture facts (E1-verified, functional — no ASM text copied)

- 8 oscillators/voice; each osc: Mode (7: Phase Mod, Freq Mod, PW Mod,
  HTE Sync, PD Square, PD Saw, PD Saw Pulse — mode changes how it
  modulates others, never itself), Waveform (323 single-cycle, per-osc
  invertible), pitch Semitone ±36 / Ratio 0.25–64 / Frequency 0–10 kHz
  (per-osc Pitch Mode, p. 40), Initial Level 0–128, Env Level ±128,
  Feedback 0–100 (dead in PW/HTE/PD modes), Keytrack ±200% (manual
  pp. 35–36, 43).
- Operator amplitude contour: six-stage DAHDSR per operator.
- Algorithms: 144 factory + user-creatable 8-op topologies (carriers vs
  modulators); algorithm morphing across up to 8 slots in 100 steps,
  forward/backward, freezable (E1c + manual p. 57+).
- Filters per voice: digital stage, 18 modes (2× SEM-inspired morphing
  SVF, 48 dB/oct LP, morphing formant); analog 24 dB/oct LP bank;
  resonant + self-oscillating + overdrive on both.
- Modulation: 5 contour generators (6-stage, loopable, syncable,
  15-minute max stages) + 5 LFOs (100 s/cycle floor, smooth or
  quantized to 3 steps) + 32-slot mod matrix; operator contours as
  sources.
- Arp (64 factory + 64 user phrases, Entropy variations) + 3-track
  sequencer (2 note tracks + 8-lane macro track); 4 FX sections in
  series (pre / Delay / Reverb / post; 9 types pre+post); Duo mode
  (bi-timbral 8+8); poly aftertouch + MPE + ribbon (performance only).

## What this means for current work (applies now)

1. **No new code may assume exactly four devices** (standing rule from
   the Korg doc — the Leviasynth is where it gets tested for real):
   `RI_VIS_MAX`, `RI_TAB_*` groups, `c_voice_canvas`, rail chips,
   `s_panel.synth[]`, mixer strips, control-ID blocks, RBNG chunks.
   First breakage candidates are enumerated in the v1 slice below.
2. **Keys and IDs:** lane-key blocks `0x0B` (strips), `0x0C` (808),
   `0x0D` (909) allocated; `0x08xx` stays reserved. The Levi voice
   takes a **new block `0x0E`**. Recorded here before code lands.
3. **Mixer:** one strip (level, pan, send, inserts) in the `0x0Bsp`
   scheme; strip nibble has headroom (1..5 used).
4. **Skins:** append-only token `levi` in `gui/ctlreg.c` with the device.
5. **Songs (RBNG):** one optional chunk family for Levi patterns; chunk
   IDs are an **owner review item** (file format, as with Korg).
6. **Determinism + kernels:** DSP in float with the `ri_*` kernels only
   (FTZ policy stands); no tables copied from anywhere; our own
   single-cycle waves (see clean-room).

## Fidelity and legal (apply when designing)

- Evidence order: E1 (above) → hardware → E0 ledgered defaults. No
  architecture fact above was written without an E1 source; verify
  every number against the manual before code.
- **Clean-room (spec §1):** no ASM waveforms (323), ROM content,
  factory algorithms/patches, firmware, panel artwork, or manual text
  in the repo. Our single-cycle waves are authored; our algorithm
  topologies are authored (a small starter set, not the 144).
  The manuals live in `/tmp` for reference only.
- **Naming:** "Leviasynth" descriptively (as "ESX-1"); no ASM logo,
  trade dress, or "Leviathan"-adjacent branding in UI or skins.

## Proposed v1 slice (owner decision required, not started)

**Levi bass/lead mono voice** (fits today's architecture with no new
pattern kind): 2–3 operator FM core (Freq Mod + Phase Mod first; PD /
HTE / morphing later), one DAHDSR amp contour, resonant lowpass,
step-programmable on the 303 pattern model (16 steps pitch+gate),
Synths-tab row, rail chip, mixer strip, demo line. Polyphony, the
remaining modes, morphing, arp/sequencer and FX stay later slices.
First 4-device-assumption breaks: `RI_VIS_MAX` 4→5, tab rows, rail
chips, `c_voice_canvas`, `s_panel.synth[2]`, `0x0E` lane block, `levi`
skin token, RBNG chunk IDs (owner review).

## Open questions (owner; do not decide)

- v1 mono-voice scope as above, or polyphony ambitions now (needs a
  chord/note pattern model we do not have)?
- Which factory character to chase first with the FM core (bass/lead/pad)?
- RBNG chunk IDs for Levi patterns (file format review).
- Demo content for the Levi voice (owner taste, as with 909).

## Where it is tracked

- `docs/2026-09-24-improvement-todo.md`, section "Planned devices".
- llm-wiki: ingest with this round (karpathy-llm-wiki skill).
