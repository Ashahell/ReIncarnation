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

## Proposed v1 slice (owner decision 2026-09-28: POLYPHONIC voice)

Full polyphonic Levi voice with chord/note patterns — accepted cost of
a new pattern kind first. Slices: (1) Levi pattern kind (chord steps)
+ engine polyphonic emission; (2) FM DSP voice bank (modest count,
Dell-budgeted) + DAHDSR + resonant lowpass; (3) control table (0x0E),
panel row + chord step editor, rail chip, mixer strip, skin token,
RBNG chunks (owner review); (4) demo content + MIDI + Dell xrun proof.
Mono step-row fitting explicitly rejected (owner wants the real thing).

## v2 program (owner decisions 2026-09-28: panel + full feature set)

- **Look:** match the real hardware as close as possible with a
  DIFFERENT font. Own art only (clean-room holds: arrangement and
  function from the manual, no ASM assets, no logo/trade dress).
  Hardware block order per the Owner's Manual top-panel map
  (MASTER CONTROL / ALGORITHM / OSC groups / DIGITAL FILTER / ANALOG
  FILTER / VOICE / ARP+SEQ / MODULE SELECT / ENV blocks).
- **Features, in order:** (1) algorithms + operator modes (8-op
  topologies incl. custom, morph; PM/FM/PWM/HTE Sync/3×PD per
  operator); (2) filters + envelopes (18-mode digital + analog 24 dB
  LP w/ pre-drive, both resonant/self-oscillating; per-op DAHDSR +
  loopable contour gens); (3) arp + sequencer (8-mode arp w/ Entropy,
  3-track seq); (4) mod matrix (32-slot) + macros; (5) FX
  (pre/delay/reverb/post) + performance (chord/ribbon/voice modes).
  Architecture refs: SOS Jul 2026 (Reid), web research 2026-09-28.
- v1 scope above is DONE (slices landed 3b–3c-iv + demo + NaN fix);
  v2 slice (1) takes E1 from the Algorithms chart (`/tmp`, kept out
  of repo) with own topologies.

## Open questions (owner; do not decide)

- RBNG chunk IDs for Levi patterns (file format review).
- (Closed 2026-09-28: polyphony — POLYPHONIC; character — approved
  for now; demo — full Bm-G-D-A part; LED — approved; timbre —
  deferred to Dell ears; rail chip — fixed c69de53, retest pending;
  strip proof — pending; USB stick — error 42 then gone, re-seated.)

## Where it is tracked

- `docs/2026-09-24-improvement-todo.md`, section "Planned devices".
- llm-wiki: ingest with this round (karpathy-llm-wiki skill).
