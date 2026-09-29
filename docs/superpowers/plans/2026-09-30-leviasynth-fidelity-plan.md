# Leviasynth fidelity plan: look, sound and full feature set (owner 2026-09-30)

**Owner request (2026-09-30):**
- Improve the Leviasynth so it looks and sounds as close to the real hardware as possible, and emulates all of its features.
- Keep the current font, in a contrasting colour.
- Use reference pictures; research first; plan first; use the audit script and update it if needed.

**Parent documents:**
- `docs/superpowers/specs/2026-09-28-device-asm-leviasynth-requirement.md` (requirement, E1 sources, clean-room rules);
- llm-wiki records `2026-09-28-planned-device-asm-leviasynth.md`, `2026-09-28-leviasynth-v1-implementation.md`, `2026-09-29-leviasynth-v2-arp-seq-matrix-lfo-reverb.md`.

## 1. Sources (E1)

Kept **out of the repo**; working copies are in the session scratchpad only.

- **E1a:** Leviasynth Keyboard Owner's Manual, 173 pp. (ASM; distributor copy `files.kraftmusic.com/media/ownersmanual/ASM_Leviasynth_Keyboard_Owners_Manual.pdf`, created 2026-01-18). Cited as "manual p. N" below.
- **E1b:** Leviasynth Desktop Owner's Manual, 174 pp. (same host). It only confirms the shared engine.
- **E1c:** Sound On Sound review (Gordon Reid, July 2026), Audiofanzine and MusicRadar reviews, and the Synth Anatomy firmware 1.2 note (sequencer Entropy Deviation). Used for architecture cross-checks.
- **Reference picture:** the manual's cover render of the keyboard top panel, rasterised at 400 dpi for the layout only. We measured control positions and section boxes from it, and the Q coordinates in `gui/panelgeo.c` come from it, by a linear map (§4.1).

## 2. Clean-room rules (restated; they bind every phase)

- Function and arrangement come from the manual; **all pixels, waves, algorithms, phrases, presets and scales are our own**. No ASM text, waveform data, algorithm tables, phrases, patches or firmware.
- **No ASM logo, no "LEVIASYNTH" wordmark, no "Polytouch" mark.** The logo and wordmark areas show our own neutral text ("LEVI" plus a plain description).
- Waveform-group names in the manual are ASM's. Our wave set uses our own names and our own single-cycle definitions (a generated, authored set).
- **Font:** the house legend face (S2, `gui/draw/font_legend`). Section titles and button-cap legends use a teal that contrasts with the dark panel; knob legends use a light grey (owner 2026-09-30: "keep using the current font, in an appropriate contrasting color").

## 3. Gap analysis: hardware vs. today (`origin/main` 2026-09-30)

| Area (manual) | Hardware | Today (v1/v2 slices) | Phase |
|---|---|---|---|
| Panel (cover, pp. 15–30) | Top panel of 9 section boxes: left column, CV/Gate, Arp & Seq Control, Main Systems, Master Control (8 LED-ring encoders + display), Osc Env Level & Bias, Digital Filter, Analog Filter, Algorithm, Module Select; ribbon; 61 keys; wheels | Generic 909-idiom panel: option rows, 16 step pads, a 13-key strip | **P1** |
| Page UI (pp. 20–25) | Module Select + 8 encoders + display pages; Osc Group Edit; Page Recall | none (every parameter needs its own panel control) | **P1** (pages for today's parameters), grows each phase |
| Voices (p. 12, 87) | 16 voices × 8 osc | 6 voices × 8 ops | P6 (Dell CPU budget first) |
| Osc modes (pp. 43–48) | Phase Mod, Freq Mod, PW Mod, HTE Sync, PD Square, PD Saw, PD Saw Pulse; mode = how it modulates others | 7 modes present (own definitions) | P2 (re-check behaviour vs. pp. 43–48) |
| Osc params (pp. 35–41) | Wave (300+, invertible), Semitone ±36 / Ratio 0.25–64 / Freq 0–10 kHz, Cent/Fine, Initial Level 0–128, Env Level ±128, Feedback 0–100, Keytrack ±200 %, Phase 0–360°, Direct Out, Keyscale, velocity, PolyAT, Pan (stereo) | sine only; one global modulator ratio; fixed index | **P2** |
| Osc envelopes (pp. 38–40) | DAHDSR per osc; Fast/Slow ranges; BPM sync; curves; quantize; legato/reset/freerun; loop (2–50/∞) with stage-loop ranges; 4 trigger sources | one shared DAHDSR (A/D/S/R + loop) | **P2** |
| Osc Env Level & Bias (pp. 54–56) | 4 bias knobs over all osc envelopes | none | P2 |
| Algorithms (pp. 57–61) | 140+ presets; Single / Morph (8 slots, 100 steps each) / Custom (grid, Direct Out, solo/mute) | 8 own presets + custom routing, 2-bank morph | **P3** |
| Digital filter (pp. 62–65) | 18 types (2 SEM morphing SVF, 3 HP, 2 BP, 10 LP, Vowel); Morph/Drive, Drive pre/post, vowel order; Env1/Vel/PolyAT/Keytrack/LFO1 amounts; level | 4 SVF taps | **P4** |
| Analog filter (pp. 65–67) | 4-pole LP, pre-drive, self-oscillation near 110; Env2/LFO2/Vel/PolyAT/Keytrack | 24 dB LP stage + drive | P4 |
| VCA (pp. 68–70) | OSCs level, D.Filt level, VCA level, Patch level, LFO3 amount, velocity, PolyAT, initial level | voice trim | P4 |
| Envelopes 1–5 (pp. 71–75) | 5 mod envelopes, same feature set as osc envelopes; Env1 → D.Filt, Env2 → A.Filt, Env3 → VCA pre-wired | none (only osc envelope) | **P5** |
| LFOs 1–5 (pp. 76–82) | 11 waves incl. Step LFO (2–64 steps), Fast/Slow, Trig Sync Poly/Single/Off, delay, fade-in, quantize, level, smooth, BPM, one-shot, phase, phase stagger | 5 LFOs (sine / 3-step), rate only, not panel-bound | P5 |
| Mod Matrix (pp. 124–127) | 32 routes; manual source and destination lists (envs, LFO±, osc envs, keytrack, AT, velocity, voice mod, wheels, ribbon, MPE, pedals, CV, CC → osc/filter/VCA/FX/env/LFO/voice/matrix depth/macro/arp/seq/CV/CC) | 32 slots, 8 op-contour + keytrack + 5 LFO sources, 5 destination kinds | P5 |
| Macros (pp. 120–123) | 8 per patch, assignable, named | none | P5 |
| FX (pp. 83–86) | Pre-FX and Post-FX (9 types: Chorus, Flanger, Rotary, Phaser, Lo-Fi, Tremolo, EQ, Compressor, Distort); Delay types; Reverb types (+ freeze); bypass | reverb core only (no send wired) | **P7** |
| Voice (pp. 87–96) | 9 polyphony modes (Poly Rotate/Reassign, Mono/Lo/Hi, Unison/Lo/Hi, Unison Poly), density/limit, unison detune, Analog Feel, random phase, panner (+ maps); pitch bend, Vintage Digital, bit depth, vibrato; glide/glissando; Osc Stereo; key lock, scales, microtuning; VoiceMod | fixed poly, lane = voice | **P6** |
| Ribbon (pp. 97–98) | mod source (abs/rel), theremin mode, trigger source, sequencer step selector | none | P8 |
| Arp (pp. 99–104) | division, octave mode/range, gate 5–150 %, 9 modes, length, 64+64 phrases, Entropy/Swing/Ratchet/Chance, tap rhythm, clock lock, step offset, latch/sustain | 8 own modes, rate, on/off | **P8** |
| Sequencer (pp. 105–119) | 2 polyphonic note tracks + 8-lane Macro track, up to 128 steps, real-time/step record, MultiTrig/Drift/Probability/Entropy per step, parallel/series play | 16-step chord lanes + phrase window | P8 |
| Performance | Chord mode, octave buttons, Single/Multi (Upper/Lower/Both, Dual/KeySplit, balance), glide button, tap tempo | none | P9 |
| Patches (pp. 139–146) | browse/save/favorites/init/random | song-embedded only | P10 (RBNG chunk IDs = owner review) |
| CV/Gate, MPE, Polytouch hardware | jacks, MPE, per-key pressure | — | Out of scope for hardware parts. MIDI poly-AT/MPE are handled via the interop spec (`2026-09-29-interop-requirement.md`); the jacks are drawn but not emulated |

## 4. Phases

Each phase lands as TDD slices:
- a behavioural RED;
- a mutant killed for every invariant;
- `bash scripts/ri_audit.sh` = `AUDIT 0/0 PASS` (clean worktree, private `/tmp/ri`);
- a Dell proof, and the owner's verdict by eye/ear;
- a wiki ingest.

### P1: hardware panel + page UI (this round)

1. **Geometry:** RI_SEC_LEVI becomes **1756 × 560 Q** (the Drums row is 284 + 1472 = 1756, so window fit does not change). Top panel in y 0–345, keybed band in y 350–555.
   - Coordinates map from the reference raster: `xQ = round((x_ref − 75) × 0.6245)`, `yQ = round((y_ref − 48) × 0.6245)`.
2. **Art (`art_levi.c`), hardware order and styling:**
   - a dark graphite panel with rounded section boxes;
   - teal section titles; teal legends inside black button caps; light-grey knob legends;
   - aluminium knobs with a dark pointer; encoders with a white LED-dot ring (lit dots = value);
   - a black LCD with teal/white page text; a 2-digit 7-segment algorithm display;
   - Osc 1–8 caps in the hardware's per-oscillator colour order (teal, green, yellow, orange, amber, red, magenta, blue);
   - a ribbon strip and a keybed with pitch/mod wheels;
   - CV/Gate jacks drawn (not emulated).
3. **Hardware controls with no engine yet** are drawn dim (unlit caps, dark LED rings, `C_DISABLED` knob faces) and are **not hit-testable**, so nothing pretends to work. Each later phase turns its controls live.
4. **Live controls in P1** (today's engine parameters, moved to their hardware homes):
   - Digital Filter: Cutoff, Resonance (Drive/Morph and Env 1 stay dim until P4);
   - Analog Filter: Cutoff, Resonance, Pre-Drive (today's drive stage);
   - Osc Env Level & Bias: Attack, Decay, Release (Env Level stays dim until P2);
   - Algorithm: encoder + 2-digit display + ALGO EDIT (algorithm page);
   - Arp & Seq: ARP ON, SEQ ▶ (seq on);
   - Module Select: all module keys and OSC 1–8 select the page; lit = selected;
   - Master Control: 8 encoders drive the selected page; the display shows page title, parameter names and values;
   - Ribbon: the 16 chord steps (ribbon step mode);
   - Keybed: 13 active pitch keys for step entry;
   - STEP EDIT block (our own addition, labelled as such): lane 1–6, BACK/STEP, step readout.
5. **Pages** are a pure table (`gui/sectlevi.c`: module → 8 slots → parameter index, name, value text), host-tested. The encoders send the **target** parameter through `ri_sui_ctl_idx()`, so the control plane and automation keys are unchanged.
6. **Tests:**
   - t61 geometry laws for the new table (inside the section, no overlaps between live items, encoder/knob rows aligned);
   - t107 (or a new Levi page test) for the page table: every live slot targets an allow-listed id, and encoder writes land on the target;
   - t92/t93 re-pin for sec 18 only;
   - host raster property checks for the Levi art (teal titles present, LCD dark, dim controls never lit).
7. **Audit:** register any new test in `scripts/ri_audit.sh` (clean hunk).

### P2: oscillators + osc envelopes

- **Per-op parameters:** wave (own set: classic sine/triangle/saw/square/trisaw plus authored families, target 300+ generated and named by us, invertible), pitch mode (Semitone/Ratio/Frequency) with Cent/Fine, Initial Level, Env Level, Feedback (FM/PM only), Keytrack, Phase, Direct Out, velocity, PolyAT.
- **Per-op DAHDSR:** Fast/Slow ranges, curves, quantize, legato/reset/freerun, loop count + stage loop, BPM sync; the Osc Env Level & Bias knobs.
- **New control ids** in the 0x0E block. A per-op block needs an **id-space decision:** 8 ops × ~24 params does not fit 256 ids with everything else, so the owner is asked to choose a second block (e.g. 0x0F) for Levi ops.
- **Tests:** per-mode spectra laws (e.g. PM vs FM differ with a triangle modulator; HTE Sync resets; PD modes change the carrier only), envelope timing laws, bit-identical legacy path until switched.

### P3: algorithms

- A bank of own topologies (target ≥ 64 authored; the hardware's 140+ are theirs and are not copied).
- Single / Morph (8 slots, 100 steps per slot, OFF/Silence rules, p. 59) / Custom (grid with Direct Out, solo/mute).
- 3-digit numbering display rule (a dot for 100+, "C" for custom).

### P4: filters + VCA

- 18 digital types from our own designs by family:
  - SVF morph LP-BP-HP and LP-NO-HP;
  - HP ×3; BP ×2;
  - LP ladder 12/24 compensated/uncompensated, gate, 1-pole, 8-pole, and three other characters;
  - Vowel with 8 orders.
- Drive pre/post; the analog 4-pole with pre-drive and self-oscillation; keytrack centred on C2; Env/LFO/velocity/PolyAT amounts.
- The VCA module levels.
- Self-oscillation and NaN/inf laws (see the 2026-09-28 filter NaN record).

### P5: modulation

- Env 1–5 with pre-wiring (Env1 → D.Filt, Env2 → A.Filt, Env3 → VCA).
- LFO 1–5 full: 11 waves including Step, trig sync, delay/fade, quantize, one-shot, phase stagger.
- The matrix, grown to the manual's source and destination lists.
- 8 macros.

### P6: voice

- Voice count by Dell budget: measure render time for 8/12/16 voices × 8 ops, keep xruns at 0, ledger the number.
- Polyphony modes; unison/detune/density; Analog Feel; random phase; panner; bend; Vintage Digital/bit depth; vibrato; glide/glissando; Osc Stereo; scales, microtuning and key lock.

### P7: FX

Pre-FX and Post-FX with 9 types, delay types, reverb types with freeze, and the send wiring into the engine (the reverb core exists).

### P8: arp, sequencer, ribbon

- Arp to the manual's parameter set, with 64 own phrases.
- 2 poly note tracks + a macro track, up to 128 steps with per-step MultiTrig/Drift/Probability/Entropy, and step record.
- Ribbon as mod source, theremin and step selector.

### P9: performance

Chord mode, octave buttons, Single/Multi with Upper/Lower/Both, Dual/KeySplit and balance, tap tempo.

### P10: patches

Init/random/browse/save/favourites, stored in a Levi patch chunk. **The chunk IDs are an owner review item** (file format).

## 5. Open owner decisions

1. The control-id space for per-oscillator parameters (P2): a second block for Levi (0x0F…)?
2. Voice count on the Dell after measurement (P6).
3. RBNG chunk IDs for Levi patches (P10; already open in the requirement).
4. Whether the drawn CV/Gate jacks stay (decorative) or are omitted.

## 6. Risks

- **CPU:** 16 × 8 operators at 48 kHz on the Dell. Measure before promising 16 voices.
- **Control-id and allow-list pins** (t60/t77/t83/t106) move with every new parameter. Each move is deliberate and noted in the commit.
- **Shared tree:** other sessions touch engine files. Build and audit only in clean worktrees with a private `/tmp/ri`.
