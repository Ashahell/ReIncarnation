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
| CV/Gate, MPE, Polytouch hardware | jacks, MPE, per-key pressure | — | Out of scope for hardware parts. MIDI poly-AT/MPE are handled via the interop spec (`2026-09-29-interop-requirement.md`); the CV/Gate block is omitted (owner 2026-09-30: useless in a soft synth) |

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
   - CV/Gate jacks drawn (not emulated). Removed 2026-09-30 (owner): the Arp & Seq block takes the column.
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

**P2 status (2026-09-30): done.**
- The owner approved block `0x0F` for per-oscillator keys: `0x0F00 | op << 5 | param`, 29 params per op.
- Delivered:
  - per-op wave, invert, pitch mode (Semitone/Ratio/Frequency) with coarse and fine;
  - Initial Level and Env Level;
  - feedback (Freq/Phase Mod only);
  - keytrack, start phase and Direct Out;
  - per-op DAHDSR with Fast/Slow ranges, attack/decay/release curves, quantize, loops (2–50 or infinite) over the Delay>Attack/Hold/Decay range, and freerun;
  - legato and reset are stored for P6; velocity>env is stored for P9;
  - the Osc Env Level & Bias knobs (`0x0E27..2A`);
  - OSC pages 1–5 and the Group Edit pages; PAGE ▲▼ and a repeated OSC press step the pages; Page Recall.
- **Behaviour fixed to the manual:**
  - A mode belongs to the MODULATOR (p. 35): Freq/Phase Mod feeders move the carrier's frequency or phase; PW/HTE Sync/PD feeders warp its phase trajectory, n = modulator/carrier pitch per cycle.
  - Envelope segments are true ramps. The v1 law re-read the running value each sample, so segments collapsed early. This changes the v1 envelope timing, deliberately.
- **E0 ledger** (tune by ear):
  - **Waves:** 128 own waves (8 families × 16), not 300+, because control values are 7-bit. A bank key can extend this later.
  - **Modulation depth:** index 4 at full level. Default modulator env level is +16, which keeps the v1 index of 0.5, bit for bit.
  - **Envelope times:** quartic map onto the manual's maxima.
  - **Frequency mode:** 10 kHz × c³ + 0–0.99 Hz fine.
  - **Keytrack:** 64 = 0 %, 32 steps per 100 %.
  - **Loops:** a loop restarts the contour from 0.
  - **Quantize:** a 15-entry step table.
  - **Bias:** env level ±1; times ±4 octaves.
- **Legacy rows:** the v1 section-wide rows (global ratio, packed op mode, all-op envelope) stay registered and automatable for old songs, with no panel item (`ri_slevi_legacy`).
- **Tests:** t129 (13 laws, 6 mutants killed), t107 (page UI), t110 (hardware layout, now in the audit), t60/t77/t79 (deliberate key and count moves).

### P3: algorithms

- A bank of own topologies (target ≥ 64 authored; the hardware's 140+ are theirs and are not copied).
- Single / Morph (8 slots, 100 steps per slot, OFF/Silence rules, p. 59) / Custom (grid with Direct Out, solo/mute).
- 3-digit numbering display rule (a dot for 100+, "C" for custom).
- **Status: done 2026-09-30** (engine, keys, UI, tests):
  - 64 own presets as op-feeds masks (`$scratch/levi/algos.py` generator, not in the repo); presets 1–8 are the v1 eight. Every preset is loop-free (edges point to a lower op), op 1 is always a carrier, all 64 differ.
  - Routing is a feeds mask per op (up to 8 targets); render order by topological sort.
  - Modes (key `0x0E2B`): Single = slot 1; Morph walks the 8-slot list (`0x0E2C..33`), OFF slots skipped, SILENCE slots silent, 100 steps per slot pair, position key `0x0E34` spans the live slots; Custom starts from the sounding topology and takes up to 3 targets per oscillator (per-op params TGT1–3, keys `0x0F1D..1F` per op), dropping any target that would close a loop.
  - Solo (`0x0E35`) auditions one oscillator, modulators too; Mute (`0x0E36/37`) drops oscillators from the mix.
  - UI: ALGO module pages 1/5 (mode, algo, morph, PM/FM, solo, mutes, Direct Out), 2/5 (8 slots), 3–5/5 (custom grid, one target column per page, encoder k = oscillator k). The 2-digit readout shows "C" in Custom.
  - The v1 two-algorithm morph rows (target, morph) become legacy rows.
- **E0 ledger:** 64 presets (7-bit keys; a bank key can extend toward 140+ later); 2-digit readout (no 100+ dot needed yet); position key resolution 7-bit over up to 700 steps; no on-screen topology graph yet (the LCD shows the page values).
- **Tests:** t130 (algorithm modes, 12 mutants killed; it caught SILENCE sharing the CUSTOM bank id); t106–t109, t129, t77, t79, t60 deliberate range and count moves.

### P4: filters + VCA

- 18 digital types from our own designs by family:
  - SVF morph LP-BP-HP and LP-NO-HP;
  - HP ×3; BP ×2;
  - LP ladder 12/24 compensated/uncompensated, gate, 1-pole, 8-pole, and three other characters;
  - Vowel with 8 orders.
- Drive pre/post; the analog 4-pole with pre-drive and self-oscillation; keytrack centred on C2; Env/LFO/velocity/PolyAT amounts.
- The VCA module levels.
- Self-oscillation and NaN/inf laws (see the 2026-09-28 filter NaN record).
- **Status: done 2026-09-30** (engine, keys, UI, tests):
  - 18 digital models, own designs and own names (no maker or product names): SVF LP-BP-HP and LP-NO-HP (morph), HP GRIT/MOD/12, BP MOD/12, LP L12/L24 (uncompensated), F12/F24 (compensated), GATE (level follows cutoff), GRIT, MOD, 12, 6, 48, VOWEL (3 formants, 8 own orders; cutoff places the vowel, morph scales formant size).
  - Primitives: TPT state-variable core; TPT one-pole ladders solved without the unit delay (oscillation threshold independent of cutoff); every loop soft-clipped (bounded, no inf/NaN).
  - Drive pre/post for the non-morph models; D.Filt level; LFO 1 amount; keytrack ±200 % around C2 (digital default 0 %).
  - Analog 4-pole: gain-compensated pre-drive (DRIVE key), self-oscillation from ~110/128 at every cutoff, pitch at the cutoff; LFO 2 amount; keytrack default 100 %.
  - VCA: OSCs, D.Filt, VCA and Patch levels (64 = unity), LFO 3 amplitude amount.
  - Keys `0x0E38..0x0E44`; the v1 4-way FTYPE is a legacy row mapped onto LP 12 / HP 12 / BP 12 / LP-NO-HP middle; the top-panel DRIVE / MORPH knob is live.
  - Pages: DIGITAL FILTER 1/2 and 2/2 (vowel order only on VOWEL, drive position only where drive exists), ANALOG FILTER, VCA.
- **Deferred:** ENV 1/2/3 amounts, velocity > env and VCA initial level need the P5 envelopes; PolyAT amounts need P9 pressure. Their page slots stay dim.
- **E0 ledger:** LFO amounts ±4 octaves full scale; drive law own soft clip; vowel formants are generic phonetics averages; an old song that set the v1 FTYPE and no model gets the default model (FTYPE lived two days).
- **Tests:** t131 (filters + VCA, 21 mutants killed); t107 page moves; t60/t77 count moves; sec 18 re-pinned (live DRIVE / MORPH knob).

### P5: modulation

- Env 1–5 with pre-wiring (Env1 → D.Filt, Env2 → A.Filt, Env3 → VCA).
- LFO 1–5 full: 11 waves including Step, trig sync, delay/fade, quantize, one-shot, phase stagger.
- The matrix, grown to the manual's source and destination lists.
- 8 macros.
- **P5a status: done 2026-09-30** (ENV 1-5, pre-wires, full LFOs):
  - ENV 1-5 per voice share the oscillator DAHDSR law (curves, quantize, loops over a stage range, freerun, speed ranges); four trigger sources each (Note On, LFO 1-5 cycle start; ribbon/pedal stored for P8/P9), level.
  - Pre-wires: ENV 1 > digital cutoff and ENV 2 > analog cutoff (amounts ±8 octaves full scale, keys `0x0E45/46`, the ENV 1 / ENV 2 top-panel knobs are live); ENV 3 is the VCA contour with Initial Level (`0x0E47`); ENV 3 closing ends the voice.
  - LFO 1-5: 11 own waves (sine, triangle, saw up/down, square, pulse 27/13, S&H, noise, smooth random, step), slow 0-25 Hz / fast 5-150 Hz, trig sync poly/single/off with phase stagger, delay and fade-in, quantize, level, smooth, one-shot on/step, start phase. Deterministic per-LFO noise (xorshift).
  - Keys: block `0x10` (ENV `env << 5 | param`, LFO `0xA0 | lfo << 4 | param`); pages ENV n 1/4-4/4 and LFO n 1/2-2/2 (steps and stagger shown only where they apply).
  - The v1 LFO keys stay (rate, shape 0/1 = sine / 3-step), bit for bit.
- **P5a E0 ledger:** ENV 3 default release 34 (a little longer than the oscillators'); ENV amounts ±8 octaves; step values are a default ramp until the step editor (P8); BPM sync waits for the clock (P8); velocity/PolyAT for P9; legato/reset stored for P6; Mod In trigger sources omitted (no CV, owner 2026-09-30).
- **Tests:** t132 (32 mutants killed); t129 freerun law holds the VCA open (ENV 3 now closes voices); t77/t79/t107/t131 deliberate moves; sec 18 re-pinned (live ENV knobs, algorithm ring now spans 64).
- **P5b status: done 2026-09-30** (matrix + macros):
  - Sources in the manual's groups (own ids; the v1 ids kept): ENV 1-5, LFO 1-5 and 1+-5+ (unipolar), OSC 1-8 ENV, keytrack (C4 centre), PolyAT/MonoAT, velocity on/off, voice mod/+, wheel, bend, ribbon abs/abs+/rel, MPE X/Y rel/Y abs, expression and sustain pedals. Performance sources read 0 until P6/P8/P9 deliver their data. CV and MIDI CC sources omitted (no CV; CC waits for the interop MIDI work).
  - Destinations as module + parameter: OSC 1-8 / all / carriers / modulators (init, env level, pitch ±24 semitones, ratio and fine in ratio mode, feedback, phase, wave, DAHDSR times and sustain; pan stored for P6), D.Filter, A.Filter, VCA, ENV 1-5 (times, sustain, level, curves), LFO 1-5 (rate, level, smooth, steps), Mod Matrix depth 1-32, Macro 1-8, algorithm morph. FX, voice, arp and sequencer destinations arrive with P6-P8.
  - Own laws: a contribution moves a parameter by depth x source x its span (cutoffs 8.5 octaves, levels as 1 + x, times 2^(8x)); route depths take their Mod Matrix depth modulation first.
  - 8 macros: knob (0x0E48..4F) and button (0x0E50..57, button value while on), 8 routes each; the MACRO ASSIGN key opens its pages (knobs, buttons, 4 route pages a macro).
  - Keys: matrix block `0x11` (slot << 2 | source, module, param, depth), macro block `0x12` (macro << 5 | route << 2 | module, param, depth, button value). The v1 route switches are legacy rows.
  - Pages: MATRIX n|n+1 (16 pages, 2 routes each), MACRO (34 pages).
- **P5b E0 ledger:** laws above; macro button behaves as Toggle (the System setting arrives with P10 patches); names, sort, audition, copy and randomise wait for the patch and system pages (P10); the one-sample lag of oscillator-envelope sources.
- **Tests:** t133 (25 mutants killed); t77 (key counts, unknown key 0x1334), t79/t129 (unknown block 0x1300), t107 (module list ends at MACRO; switch law via macro buttons); sec 18 re-pinned (MACRO ASSIGN key live).

### P6: voice

- Voice count by Dell budget: measure render time for 8/12/16 voices × 8 ops, keep xruns at 0, ledger the number.
- Polyphony modes; unison/detune/density; Analog Feel; random phase; panner; bend; Vintage Digital/bit depth; vibrato; glide/glissando; Osc Stereo; scales, microtuning and key lock.
- Split (2026-09-30): P6a allocator + 9 modes + legato/reset on 6 voices (direct lane path untouched, bit-identical songs); P6b voice params + VoiceMod + Voice matrix module (mono); P6c stereo + scales/microtuning/vintage (owner strip-format call). Voice-count increase waits for the owner call in §5.2.

#### P6a plan (allocator; before code 2026-09-30)
- Keys `0x0E58` POLYMODE (0..8: Rotate, Reassign, Mono, MonoLo, MonoHi, Unison, UnisonLo, UnisonHi, UnisonPoly), `0x0E59` UDENSITY (0..127 -> 1..8 stacked), `0x0E5A` ULIMIT (0..127 -> 1..6 poly voices). Registry rows 129..131, allow-list + engine section-wide apply, VOICE page slots 0..2 live.
- New `RILeviAlloc` in the set (mode/density/limit, rotate cursor, held-note list with order, mono current). `levi_init_set` defaults: Rotate, density 8, limit 6. Direct `levi_trigger/release` unchanged (pattern engine keeps lane==voice, bit for bit).
- New allocator API: `levi_note_on(set, note)` -> voice bitmask applied (int n applied, -1 bad); `levi_note_off(set, note)`. Mono: voice 0 only, last/low/high priority over held notes. Unison: all limit voices, same note. UnisonPoly: density voices per note up to limit, steal oldest. Rotate: next free else steal oldest. Reassign: first free else steal oldest (no rotation on steal).
- Legato/reset (P2 flags live here): mono overlapping note with any op LEGATO set and no op RESET set retunes (freq/phase per op, filters/LFO/menv NOT restarted); RESET set anywhere forces full retrigger; poly/unison always full retrigger. E0: legato = retune-only; reset wins over legato.
- Measurement (host O2, 48 kHz, 8 carriers + 32 matrix routes + 5 LFOs + 2 filters): 6v 7.94 us/sample (38% of 20.83 us budget), 8v 10.58 (51%), 12v 15.87 (76%), 16v 21.16 (102% over even on fast host; Dell builds -O0 on older CPU, worse). Dell timing hook: none yet; xruns read from status after deploy. Recommendation: 8 voices. Owner call stays open (§5.2).
- **Status: P6a done 2026-09-30** (allocator + 9 modes + legato/reset, 6 voices, mono, direct lane path untouched):
  - Allocator `RILeviAlloc` in the set (`levi.h`): mode/density/limit, rotate cursor, 16-note held list. `levi_note_on/off` (live notes); `levi_trigger/release` stay lane==voice for songs, bit for bit. Mono uses voice 0 with last/low/high priority + fallback; Unison fires the limit voices; UnisonPoly stacks density voices per note up to the limit, steal oldest; Rotate scans from the cursor, steal advances it; Reassign takes the first reusable (silent or released-ringing) voice, else steals the oldest-held note's voice.
  - Legato/reset (P2 flags live): a mono overlapping note on a legato voice with no reset flag retunes only (op freqs + filter keytrack; envs/LFOs/menvs keep running); any reset flag forces a full retrigger; poly/unison always full-retrigger except unison voice 0 legato.
  - Keys `0x0E58..5A` (polymode 0..8 clamped, density 1..8, limit 1..6 clamped); registry rows 129..131 (Voice/Polyphony/Density/Poly Limit); VOICE page slots 0..2 live with texts (ROTATE/REASSIGN/MONO/MONO LO/MONO HI/UNISON/UNIS LO/UNIS HI/UNISPLY, density/limit counts); engine section-wide apply rides the existing 0x0E branch (no `engine.c` change).
- **P6a E0 ledger:** legato = retune-only; reset wins over legato; reusable = silent or released-ringing (note no longer held); density maps `val*8/127+1`, limit `val*6/127+1`; unison density above the limit clamps (return counts applied voices); polymode UI value above 8 clamps to UnisonPoly.
- **Tests:** t134 (allocator laws, legato vs reset incl. retune==trigger pitch law, bit-identical direct path, finite/bounded storm over all 9 modes, keys/allow-list/pages; 18 mutants killed incl. steal-advance, reset-inversion, want-clamp, retune-ratio); t60 (0x0E range to 0x0E5A), t77 (mapped 186, allow 1013) deliberate moves; t92/t93 unchanged (no art change, LCD text only).
- Owner calls 2026-09-30: **8 voices, stereo.** P6b = voice params mono (detune, feel, vibrato, glide, bend range, pan/width stored, VoiceMod, Voice matrix module); P6c = stereo + scales/microtuning/vintage; P6d = 6→8 voices (pattern lanes stay 6, songs identical; allocator serves the extra voices).

#### P6b plan (voice params, mono; before code 2026-09-30)

- Keys `0x0E5B..67` (13): DETUNE, AFEEL, RNDPH, PAN, WIDTH, PANMODE, BENDRNG, VIBRATE, VIBAMT, VIBDLY, GLIDE, GLTIME, GLCURVE. Registry rows 132..144 (`Voice` group), allow-list + engine section-wide apply, VOICE pages 1/2 and 2/2 (page 1: POLYPHONY/DENSITY/LIMIT/DETUNE/ANALOG FL/RND PHASE/PAN/WIDTH; page 2: PAN MODE/BEND RNG/VIB RATE/VIB AMT/VIB DLY/GLIDE/GL TIME/GL CURVE).
- DSP per voice: unison detune (static ordinal spread, ±50 c full scale); Analog Feel (own per-voice drift: deterministic xorshift, slow pitch ±6 c + cutoff wobble); random start phase (op phase0 += hash × amount); vibrato (sine, rate 0.1–20 Hz, amount ±100 c full, delay 0–5 s, trigger-reset phase); glide (portamento on every trigger: time 0–5 s quartic, curve, mode off/glide/gliss with semitone steps); bend range 0–24 st, default 2 (bend source still 0 until P9, inert); pan/width/panmode stored for the P6c stereo sum (mono sum ignores them).
- VoiceMod sources live: VMOD = per-voice hash bipolar (−1..1), VMOD+ = voice ordinal unipolar (v/5); deterministic, no RNG.
- Matrix `DM_VOICE` (28, 10 params: DETUNE, PAN, AFEEL, BEND, VIBAMT, VIBRATE, GLIDETGL, GLTIME, GLCURVE, PANWIDTH) with span laws (detune ±50 c, pan ±1, feel 0..1, bend ±range, vib amt/rate offsets, glide tgl/time/curve, width 0..1); evaluated per-sample like the other modules.
- E0: detune static per voice index (doubles as analog spread in poly); afeel drift period ~7 s own sine+xor wander; rndphase scales the op phase0 offset only; glide retriggers from the current pitch on every note; gliss steps the glide in semitones; vibrato resets phase on trigger; bend = 2^(src×range/12).
- **Status: P6b done 2026-09-30** (engine, keys, UI, tests):
  - Per-voice DSP as planned (detune/afeel/rndphase/vibrato/glide/bend-range/pan-width-mode stored; VoiceMod live; `DM_VOICE` folded per-sample).
  - Proof deviation (Dell crash-looping for every binary incl. clean-tree P5b rebuild — lane state, see the lane record): on-target proof done on riqemu1 instead (ABIv1 lane): window opens, transport PLAY/STOP inks, Levi tab renders live, no crash; VOICE page pixels not captured (two click misses) — VOICE page check rides with the P6c proof (same module). Host t107 covers slots/names/keys/texts; t92/t93 art unchanged green.
  - Lane drive-bys in this commit: `evlog_vol` suppresses volume requesters during the USB hunt (`pr_WindowPtr = -1`; riqemu1 modal block, unverified path — `RIAPP_EVLOG=RAM:` proven instead); t133 module knob now spans to VOICE (deliberate).
- **Tests:** t135 (defaults bit-identical; per-feature move/inert laws incl. glide-settles-progress, gliss-vs-glide, VMOD/VMOD+ isolated sets; DM_VOICE route; keys/allow/pages; 19 mutants killed; pan/width/mode/bend maps unobservable in mono — P6c/P9 cover); t60 (0x0E range to 0x0E67), t77 (mapped 199, allow 1026), t133 (last module VOICE) deliberate moves.

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

1. ~~The control-id space for per-oscillator parameters (P2)~~ — approved 2026-09-30: block 0x0F.
2. Voice count on the Dell after measurement (P6).
3. RBNG chunk IDs for Levi patches (P10; already open in the requirement).
4. ~~Whether the drawn CV/Gate jacks stay~~ — omitted 2026-09-30 (owner: useless in a soft synth).

## 6. Risks

- **CPU:** 16 × 8 operators at 48 kHz on the Dell. Measure before promising 16 voices.
- **Control-id and allow-list pins** (t60/t77/t83/t106) move with every new parameter. Each move is deliberate and noted in the commit.
- **Shared tree:** other sessions touch engine files. Build and audit only in clean worktrees with a private `/tmp/ri`.
