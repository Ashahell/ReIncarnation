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
#### P6d plan (8 voices; before code 2026-09-30)

- `RI_LEVI_NVOICES` 6→8 (owner call 2026-09-30). Pattern lanes stay 6 (`PAT_LEVI` 6 lanes, `SELECT` 0..5, `LSTEP`): songs bit-identical (lanes 0..5 direct, voices 6–7 idle); lanes 6–7 are live/allocator-only; engine lane guard admits 0..7.
- Allocator follows automatically (rotate/reassign/unison over `NVOICES`, steal oldest); `ulimit` default 8; UDENSITY still ≤8; VoiceMod VMOD+/detune/spread ordinals recenter via the `(N-1)/2` macros (P6a–c code already macro-based).
- E0: ulimit UI map scales with N (`val*N/127+1`) — a stored ULIMIT value from a 6-voice song maps higher under 8 voices (deliberate, minor; the key is two phases old and defaults dominate).
- Tests: new t137 (8-voice rotate fill + steal, unison 8, reassign, mono voice 0, pattern-lane compat incl. lane 8 refused, default limit 8, init/active of voices 6–7); deliberate constant moves in t103 (`NVOICES` 8, bad-voice 8), t108/t109 (bad 8), t134 (loops + counts to 8).
- **Status: P6d done 2026-09-30** (one-line count change + tests):
  - Allocator/limits/ordinals all macro-driven — no DSP edits. Songs bit-identical (lanes 0..5 direct; t95-style twin renders in t137).
  - Proof: audit 0/0 clean worktree, Dell 0 UND + 0 r12, ASan clean; riaudio lane (Dell still crash-loops everything): Levi tab live capture, PLAY/STOP ink, host wav peak 0.855 with 0 xruns over 5199 buffers.
- **Tests:** t137 (7 targeted mutants: count pin via full rebuild, ulimit default, unison bound, steal origin, reassign-reuse, upoly clamp, set_param bound; allocator laws re-verified at 8 via t134); t129 bad-voice 8 (caught by the audit, not by me — noted).

#### P6c plan (stereo + scales/microtuning/vintage; before code 2026-09-30)

- Keys `0x0E68..74` (13): VINTAGE, SCALE, MICRO, KEYLOCK, SPREAD, OSCPAN1..8. Registry rows 145..157 (`Voice` group), allow-list + engine section-wide apply, VOICE pages 3/4 and 4/4 (page 3: 8 osc pans; page 4: VINTAGE/SCALE/MICRO/KEYLOCK/SPREAD + 3 dead reserved).
- Stereo: `levi_voice_render_stereo` + `sum_stereo`; per-op pan = clamp(vpan + oppan×width + spread×ordinal (+ matrix DO_PAN / DM_VOICE PAN+PANWIDTH)); modes BALANCE linear (default, center l=r=1 → dual mono, bit-identical through the strip), POWER equal-power via ri_sin, WIDE 150% law; engine section-4 stereo branch (inserts per channel, meter/send on mid, strip pan as balance reusing `engine_pan_gains`); new `scratchR` bus. P5b DO_PAN goes live.
- Scales (own 16: chromatic + theory-standard names, masks authored): KEYLOCK quantizes trigger notes to nearest degree (ties lower), in trigger + legato retune (both engine paths).
- Microtuning (own 8×12 cent tables, table 0 = equal): pitch mult 2^(c/1200) at trigger/retune.
- Vintage (own law): bits = 16−floor(15a), decim = 1+floor(31a²), a = val/127; quantize + hold; exact bypass at val 0. Post-pan per voice.
- E0: SPREAD static per voice index (unison spreads, poly static image); width 0 collapses ops to voice pan; keylock off = chromatic; micro table 0 inert; vintage default inert.
- **Status: P6c done 2026-09-30** (engine, keys, UI, tests):
  - Stereo: per-op pan = clamp(vpan + (oppan + matrix DO_PAN) × width + spread×ordinal) through BALANCE (linear, default, center l=r=1)/POWER (equal-power ri_sin)/WIDE (150% law); dual-mono filters (state parameterized, verified identical); `sum_stereo` + section-4 stereo branch (inserts per channel, meter/send on mid, strip pan as balance — center renders bit-identical, t104 unchanged green). P5b DO_PAN live. `voice_pass` takes optional R accumulation (NULL = legacy path exactly); mono render + mono sum frozen as the bit-identity reference (vintage/stereo live in the stereo path only).
  - Scales (own 16 masks, computed not hand-rolled) + quantize (nearest, ties lower) in trigger + legato retune; microtuning (own 8×12 cent tables, 2^(c/1200)); vintage (bits 16−15a linear, decim 1+31a², exact bypass at 0, post-gain output degradation).
  - Keys `0x0E68..74` (13); rows 145..157; VOICE 4 pages (3: 8 OSCPAN, 4: VINTAGE/SCALE/MICRO/KEYLOCK/SPREAD + 3 dead reserved).
  - Proof deviation as P6b (Dell crash-loops everything): audit 0/0 clean worktree, Dell 0 UND + 0 r12, ASan/UBSan clean, riqemu1 window opens no crash; VOICE page pixels still deferred (tab nav failed: keys need focus, 2 click misses — same module, still pending).
- **Tests:** t136 (dual-mono identity, hard-pan isolate, width collapse, 3 modes differ, spread, DO_PAN + DM_VOICE PAN routes, vintage bypass/degrade/1-bit quanta law, scale snap + whole-tone tie + clamp, micro move + 114 c ratio law, panmode clamp, keys/pages incl. 4 VOICE pages, null guards, extremes; 19 mutants killed); t135 page count 2→4 and `0x0E75` boundary, t60 range to `0x0E74`, t77 mapped 212 / allow 1039 deliberate moves.

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
- Split (2026-09-30): P7a delay + chain framework (done); P7b reverb types + freeze (this slice); P7c pre/post 9-type engines; P7d BPM-sync wiring (needs the P8 clock — flags stored until then).

#### P7b plan (reverb types + freeze; before code 2026-10-01)

- Reuse the v2 core (`ri_reverb_*`, untouched — t123 pins it): two instances (L/R) with static backing in `RILeviFx` (6 lines × 2048+ × 2ch); own type tunings (4: ROOM/HALL/PLATE/CHAMBER — authored tap sets, never the core's); own predelay lines (250 ms max, static).
- Params: type, predelay 0..250 ms, time→comb fb 0..0.95, tone (wet LP), hi-damp (2nd wet LP), lo-damp (wet HP), dry/wet, freeze (fb→1.0 exactly + input muted; finite by unity recirculation), bypass (exact dry). Damps are wet-EQ (no loop hooks in the shared core — own interpretation, ledgered).
- Chain: delay → reverb → out (post-FX slot stays a pass-through until P7c).
- Matrix `DM_REVERB` (30): TIME, TONE, HIDAMP, LODAMP, DRYWET (prompt's list exactly).
- Keys `0x0E7E..86` (9): RTYPE/RPREDLY/RTIME/RTONE/RHIDAMP/RLODAMP/RDRYWET/RFREEZE + RBYPASS on the FXREV row (same single-truth pattern as FXDLY). Registry rows 165..171; REVERB pages 1/2 (existing 8) + 2/2 (FREEZE + 7 dead reserved).
- E0: bypassed by default; freeze sustains bounded content; predelay line keeps shifting under freeze; FTZ is globally on (explicit flushes belt-and-braces).
- **Status: P7b done 2026-10-01** (engine, keys, UI, tests):
  - Reuses the v2 core untouched (t123 green): dual-mono instances with static backing, 4 own tap sets, 250 ms predelay lines, wet-EQ damps (own interpretation — no loop hooks in the shared core), freeze as held wet-mix drone (tails resume on release), chain delay → reverb, `DM_REVERB` (30) lead-voice driven, keys `0x0E7E..86`, rows 165..172, REVERB 2 pages.
  - Proof: audit 0/0 clean worktree; Dell 0 UND + 0 r12 (audit gate); riaudio lane (Dell still crash-loops everything): window opens no crash, SPACE→TR PLAY inks, AHI mode 0x003e0001, buffers=6184 xruns=0. Deviations: no host wav this round (lane QEMU writes no wav file); REVERB page pixels deferred (standing tab-nav-focus gap); lane 909 pack missing (unrelated silence).
  - Adjacent fixes the slice forced: t79/t116 whole-engine memcmp now hole-aware (reverb line pointers are absolute per instance); `engine/fx/reverb.c` added to the AROS sections + riapp lists and `build/portable.mk` (audit caught the RISECT link, not me).
- **Tests:** t139 (bypass/dry identity, tail outlives release, max-decay bound, rtype clamp + 4 types differ, predelay shift, tone/hi/lo shape with 1.2× margin laws, freeze sustains/flat/bounded, DM route, chain order, keys/pages/texts, null guards, extremes; 21 mutants killed, 2 benign survivals ledgered: DSP fb-clamp under the UI door, freeze-fb obsolesced by the drone redesign); t60 range to `0x0E86`, t77 mapped 229 / allow 1056, t133 last module REVERB, t136/t138 `0x0E87` boundary deliberate moves.

#### P7c plan (pre/post 9-type engines; before code 2026-10-01)

- One shared 9-type mod engine, two instances (pre/post) in `RILeviFx` (own algorithms throughout; no shared `engine/fx` or mixer touched — route owners, t51 green). State per slot: type, preset, p1/p2 (0..1 normalized), drywet, bypass; one 2400-float/channel modulated-delay line (chorus/flanger share the max ~50 ms); LFO phase; 4 phaser allpass states; EQ/comp/dist states; `mxm[3]` + on-flag (lead voice drives, P7a pattern).
- 9 own types (generic names): CHORUS (15–35 ms modulated delay; P1 rate 0.05–8 Hz, P2 depth), FLANGER (1–8 ms + fixed 0.55 fb; P1 rate, P2 depth), ROTARY (dual-rate tremolo + cross-pan; P1 rate, P2 horn/drum balance), PHASER (4 allpass stages; P1 rate, P2 depth), LOFI (bitcrush 16→4 + decim 1→16; P1 bits, P2 decim), TREMOLO (sine AM 0.5–15 Hz; P1 rate, P2 depth), EQ (one-pole bass/treble shelves ±12 dB; P1 bass, P2 treble), COMP (peak follower 10/100 ms, makeup at set; P1 thresh −40..0 dB, P2 ratio 1..20), DISTORT (asymmetric soft clip + tone LP; P1 drive, P2 tone).
- Presets: 4 own factory tuples per type → (p1, p2, drywet); setting PRESET writes the knobs (normal preset behavior). Table `[9][4]` shared by both slots.
- Chain: pre → delay → reverb → post inside `sum_stereo` (pre on the summed mix before delay; post after reverb).
- Matrix `DM_PREFX` (31) / `DM_POSTFX` (32): P1, P2, DRYWET each (additive; p1/p2 offsets ±0.5 full-scale, drywet full).
- Keys `0x0E87..94` (14): pre TYPE/PRESET/P1/P2/DRYWET/PREBYPASS + post TYPE/PRESET/P1/P2/DRYWET/POSTBYPASS; FXPRE (row 64) and FXPOST (row 67) rebound as panel-ON/engine-bypass (P7a/b single-truth pattern). Rows 173..182; PREFX/POSTFX pages all live except PARAM 3–5 (dead reserved — prompt wants Param 1/2 only).
- E0: bypassed by default (songs bit-identical); flanger fb fixed 0.55; tremolo/rotary rates in Hz (no BPM flags — nothing musical needs the clock here); preset names are positions 1–4 (no fake hardware names); FTZ on + per-loop flushes.
- **Status: P7c done 2026-10-01** (engine, keys, UI, tests):
  - One shared 9-type mod engine, two slots (pre before delay, post after reverb, inside `sum_stereo` — no shared routing touched, t51 green). Own algorithms: modulated-delay chorus/flanger (shared 50 ms line, R anti-phase), dual-rate rotary, 4-stage phaser, bitcrush+decim lo-fi, sine tremolo, one-pole bass/treble shelves, peak-follower comp with makeup, asymmetric distort + tone. 4 own factory tuples per type; PRESET writes p1/p2/drywet.
  - `DM_PREFX` (31) / `DM_POSTFX` (32): P1, P2, DRYWET (prompt list exactly), lead-voice driven. Keys `0x0E87..92` (14); FXPRE/FXPOST rows rebound as panel-ON/engine-bypass; rows 173..182; PREFX/POSTFX pages live except PARAM 3–5 (dead reserved); per-type P1/P2 value texts; FXPRE/FXPOST MODE-style press toggles (FXDLY/FXREV stay press-dead on the automation path — pre-existing since P7a, noted).
  - Proof: audit 0/0 clean worktree; Dell 0 UND + 0 r12 (audit gate); riaudio lane: window opens no crash, SPACE→TR PLAY inks, AHI mode 0x003e0001, buffers=10943 xruns=0. Deviations: no host wav (lane QEMU writes none); mod page pixels deferred (standing tab-focus gap); lane 909 pack missing (unrelated silence).
- **Tests:** t140 (bypass/dry identity, bypass-knobs-inert, all 9 types audible both slots, P1/P2 sweeps move every type, preset==hand-set + factory content pinned, type clamp, DM routes, chain order both ends, keys/pages/texts incl. per-type units, null guards, extremes; 27 mutants killed, 1 benign survival ledgered: flanger-fb removal — E0 constant, behavior class preserved); t60 range to `0x0E92`, t77 mapped 241 / allow 1068, t133 last module POST-FX, t136/t138/t139 `0x0E93` boundary, t107 FXPRE/FXPOST toggle comment deliberate moves.

#### P7a plan (delay + framework; before code 2026-09-30)

- New files `engine/dsp/levi_fx.{h,c}` (own code; registered in `ri_build_host.sh`, `ri_build_aros.sh`, `portable.mk` or the audit fails the build): `RILeviFx` chain state living in `RILeviSet` (no allocation, render-safe): stereo delay lines (2 s max, static, cleared in `levi_init_set`), delay params, pre/post/reverb param slots (DSP in P7b/c).
- Insert point: end of `levi_voice_render_sum_stereo` (per-device, post-vintage, pre-strip). No shared mixer/engine-structure change (route owners, t51 untouched) — no owner question.
- Delay DSP (own 4 types: CLEAN digital, ANALOG dark, TAPE dark + wow, PINGPONG cross-stereo): time 1 ms..2 s (UI map; BPM flag stored, sync arrives with the P8 clock), feedback 0..95 % bounded (no runaway at any corner), wet tone + feedback tone (one-pole lows), dry/wet, bypass (exact dry). Denormal flush on every loop read.
- Matrix `DM_DELAY` (29): TIME, FEEDBACK, WETTONE, FBTONE, DRYWET.
- Keys `0x0E76..7D` (8): DTYPE, DTIME, DFEEDBACK, DWETTONE, DFBTONE, DDRYWET, DBYPASS, DBPM. Registry rows 158..165 (`Delay` group), allow-list + engine section-wide apply, DELAY page (all 8 slots).
- E0: 2 s lines (768 KB static BSS in the set; explicit clear); bypassed by default (songs bit-identical); feedback clamp 0.95; tones are one-pole lows (wet 200..18k, loop 100..8k); BPM flag inert until P8; pingpong crosses L→R/R→L with centered dry.
- **Status: P7a done 2026-10-01** (engine, keys, UI, tests):
  - `levi_fx.{h,c}` (new; all 3 build lists); device insert at the end of `sum_stereo`; `DM_DELAY` (5 params) folded per-voice, lead-voice drives the device; FXDLY row rebound as the panel ON/engine bypass (no mirror state).
  - Proof: audit 0/0 clean worktree, Dell 0 UND + 0 r12, ASan/UBSan clean; riaudio lane: window opens, PLAY inks, host wav peak 0.855, 0 xruns. DELAY page pixels deferred (same standing gap).
- **Tests:** t138 (bypass/dry identity, late tail, max-fb bound, single echo, wet+loop darken, 3 mono topologies differ, pingpong cross laws, BPM inert, dtype clamp, DM TIME route, matrix-fb bound, keys/pages/texts, time-map range, null passthrough, extremes; 16 mutants killed); t60 range to `0x0E7D`, t77 mapped 220 / allow 1047, t133 last module DELAY, t136 `0x0E7E` boundary deliberate moves.

### P8: arp, sequencer, ribbon

- Arp to the manual's parameter set, with 64 own phrases.
- 2 poly note tracks + a macro track, up to 128 steps with per-step MultiTrig/Drift/Probability/Entropy, and step record.
- Ribbon as mod source, theremin and step selector.
- Split (2026-10-01): P8a device tempo + all BPM sync (this slice — closes P7d too); P8b device arp parameter set + phrases (this slice); P8c device sequencer tracks + step record (this slice); P8d ribbon (state/sources/theremin/triggers/step-select); P8e step-LFO editor (values/keys/semi/UI). Matrix arp/seq destinations ride with P8b/c.

#### P8a plan (device tempo + BPM sync incl. P7d; before code 2026-10-01)

- Tempo source: the engine already owns `tempo` (20..500, default 140) fed by song/MIDI; the device caches it per render call (`levi_set_tempo`, clamped 20..500, default 140) pushed by the engine section render — no MIDI plumbing in the device, no owner question (uses the interop lane's tempo, ledgered).
- Refresh model (own): at the top of every `sum_stereo` call, recompute ONLY flagged units from retained UI values + cached tempo (unconditional, no dirty flag — tempo changes are rare but knob edits on flagged units must apply instantly; ~600 flag branches per block worst case, negligible vs 8-voice render).
- Laws (own): musical time `beats(ui) = ui/127*4` (0..4 beats) → seconds `beats*60/bpm`. ENV segments (delay/attack/hold/decay/release) use it when the per-op/per-menv BPM flag is on; LFO delay/fade likewise; LFO rate becomes `bpm/60/beats_per_cycle` with `beats_per_cycle = 4*2^(-ui/127*7)` (4 beats..1/32 per cycle); delay time snaps to the nearest straight 16th (`beats` → 0.25 grid, clamp 1 ms..2 s) when `dbpm` is on (no new keys — the stored time is the musical length, snapped at the door).
- Flags: LFO BPM already stored (`LP_BPM`, 0x10 keys exist — just bind the dead page slots). ENV BPM is new: per-op `opbpm[8]` + per-menv `mebpm[5]` in the voice (off = absolute, default off — songs bit-identical); keys `0x0E93..9F` (8 op + 5 menv, section-wide apply to all voices); NO new registry rows (panel slots are dynamic: `SLOT_OPBPM` (-48, inside the OP range as param 32) + `SLOT_MEBPM` (-150, in the free gap), resolved against the page's op/env in the 7 slot consumers).
- E0: flags default off (bit-identical songs); tempo default 140 matches the engine default; ENV BPM at ui=0 is instant (same as the absolute map); delay snap is stepped (same as knob moves); LFO/delay/ENV recompute from retained UI (no render-state feedback); MIDI-clock derivation stays in the interop lane (device reads the pushed tempo only).
- **Status: P8a done 2026-10-01** (engine, keys, UI, tests — closes P7d):
  - Device tempo cache (`levi_set_tempo`, 20..500 clamped, default 140) pushed per block by the engine section render. Unconditional per-render refresh of flagged units from retained UI (no dirty flag — ~600 branches per block worst case).
  - Laws: `beats(ui)=ui/127*4` → seconds at tempo; ENV segments + LFO delay/fade use it when flagged; LFO rate = `bpm/60` per `4*2^(-ui/127*7)` beats; delay snaps to straight 16ths at the door when `dbpm` (idempotent, no new keys).
  - Flags: 13 new keys `0x0E93..9F` (8 op + 5 menv, section-wide to all voices, no registry rows — panel slots are dynamic `SLOT_OPBPM`/`SLOT_MEBPM`); LFO BPM slot bound to the existing 0x10 key; per-type texts SYNC/FREE.
  - Proof: audit 0/0 clean worktree; riaudio lane: window opens, SPACE→TR PLAY, AHI 0x003e0001, buffers=11882 xruns=0. Deviations: no host wav; page pixels deferred (standing gap); 909 pack missing (unrelated).
- **Tests:** t141 (pure helpers exact incl. snap floor/ceiling, tempo clamp/default/null, tempo-moves-synced/never-free for LFO rate, LFO delay, op ENV, menv ENV (via ENV1→cutoff prewire), delay snap; flags section-wide + range ends; UI round-trip set+key+texts; null guards; extremes at 20/140/500; 17 mutants killed, 1 ledgered survival: engine tempo-push removal — no engine-render unit law, lane-covered via PLAY at song tempo); t60 range to `0x0E9F`, t77 allow 1081 (+13 with page-slot ownership rule), t136/t138/t139/t140 `0x0EA0` boundary deliberate moves.

#### P8b plan (device arp params + phrases; before code 2026-10-01)

- Device live-arp, DSP-contained: per-block `levi_arp_block(s, sr, n)` (engine calls it before the Levi sum, next to the tempo push). Chord = allocator held notes (strikes must NOT pollute it — see below); latch keeps the last chord after release. The player song path stays v2 (songs bit-identical); tap rhythm deferred (live input timing needs a device wall clock that doesn't exist — app territory, ledgered).
- Strike path (own, minimal P6a surgery): `levi_note_on` body becomes `note_on_core(s, note, holdleit)`; `levi_note_strike` = core without held insert; `levi_arp_release_note` = release voices sounding the note without held removal or mode re-fire. t134 re-verifies the wrapped path.
- Note selection reuses the v2 stepper (modes 0..7 bit-identical — struct inits zero the new fields) + 9th mode PHRASE (root + rule-authored offset, 100 = rest). Factory phrases are RULE-computed, zero bytes: major 2-octave scale, `deg = (step*(3+family)+variant) % 15`, rest on `(step*7+variant*3+family) % 13 == 0`, accessor `ri_levi_arp_phrase(idx, step)` pinned absolutely. User bank `uphr[64][16]` in the set, zero-init (unison), no edit UI yet (rides P8c step record).
- Timing layer (own): division from STEPSQ(rate) × tempo; octave transpose (`octpos % range`, UP/DOWN/OFF); gate 5..150% (<100 explicit OFFs, ≥100 legato chain — order-safe); swing ≤50% odd-step shift; ratchet 1+round(×3) sub-hits; chance skip + entropy octave-leap via set-held LCG (defaults 0 = v2-identical); length 1..16 cycle wrap (default 127 = effectively off); stepoff start rotation; clocklock restarts phase on chord change (default 1) vs free-run.
- Params in the set (defaults = v2-identical sound when enabled): arpon 0, arprate 64, octmode 0, octrange 0, gate 127, mode 0, len 127, phrase 0, entropy/swing/ratchet/chance 0, latch 0, clock 1, stepoff 0. Keys `0x0EA0..AC` (13; division rides ARPRATE 0x0E12); rows 183..195; ARP 2 pages (page 1 fills the 7 dead slots, page 2 new); per-param texts.
- Matrix `DM_ARP` (33): MODE, DIVISION, SWING, GATE, OCTAVE, OCTMODE, LENGTH, PHRASE, ENTROPY, RATCHET, CHANCE (prompt order exactly); voice `axm[11]` + flag, lead-voice fold per block (1-block lag, P5b precedent).
- E0: arp off default (bit-identical songs); block-granular strikes (sub-block timing inaudible at musical divisions); gate release-all-sounding-note on unison overlaps; user phrases default unison; phrase offsets clamp 0..127.
- **Status: P8b done 2026-10-01** (engine, keys, UI, tests):
  - Strike path: `note_on_core(hold)` wrap (t134 re-green), `levi_note_strike` (no held insert), `levi_arp_release_note` (no held removal/mode refire). Per-block `levi_arp_block` (engine hook next to tempo push): absolute-clock grid, swing-deferred strikes, chance/rest advance, length rewind, latch buffer, ratchet sub queue (16), gate countdowns with stale-note guard, last-chord cleanup on empty unlatch.
  - Stepper: PHRASE mode + rule factory (zero bytes) + `rewind`; modes 0..7 bit-identical (t113-115 green untouched).
  - Proof: audit 0/0 clean worktree; Dell 0 UND + 0 r12 (audit gate); riaudio lane: window opens, SPACE→TR PLAY, AHI 0x003e0001, buffers=16474 xruns=0. Deviations: no host wav; ARP page pixels deferred (standing gap); 909 pack missing (unrelated). Adjacent fix: `levi_arp.c` into the AROS sections list (audit caught it, P7b pattern).
- **Tests:** t142 (off-identity, UP order, determinism, division density, gate trim, octave reach, phrase rule + nonzero row, swing shift, ratchet multiply, chance thin via strike counter, length wrap, latch sustain/quiet, entropy leaps, stepoff rotate, clocklock grid restart, DM route-fill + gate fold, factory pins, mode clamp, full key sweep, row bind, 2 pages + texts, null guards, extremes over all 9 modes; 22 mutants killed, 1 ledgered survival: engine arp-block hook removal — no engine-render unit law, lane-covered like the tempo push); t60 range to `0x0EAC`, t77 mapped 254 / allow 1094, t133 last module ARP, t136/t138/t139/t140/t141 `0x0EAD` boundary deliberate moves.

#### P8c plan (device sequencer tracks + step record; before code 2026-10-01)

- Runtime-only tracks (NO song-format change — persistence rides the P10 owner review): 2 note tracks × 128 steps (4 notes + vel/gate/trig/prob/drift/entropy each) + 8-lane × 128 macro track, all in the set (~3 KB BSS). Zero = empty/rest.
- Per-block `levi_seq_block(s, sr, n, playing, tick, ppq)` (engine hook next to arp; transport pushed synchronously by the live task next to the tempo push — same single-threaded ownership, no shared-struct surgery beyond 3 additive engine fields). Steps song-locked: `idx = (tick/step_ticks) % trklen`.
- Playback (own): parallel/series (alternate loops), division 1/2/4/8 per quarter, swing ≤50%, gate = step × track/127, prob = step × master/127 threshold, drift ticks→samples (linear tempo approx — tempo curves ignored, ledgered), trig 1..4 sub-hits via 16-queue, entropy ±12 pitch wobble, transpose ±24, macro knobs set per step. Strikes reuse the arp strike/release path (no held insert); own gate countdowns.
- Record: realtime (armed + playing: held chord split round-robin to tracks at each boundary, empty held writes rests) and step (armed + stopped: held written to STEPCUR every block — idempotent live preview). Captures note + vel 100 + gate 100 + trig/prob/drift/entropy from the 4 step-default knobs + macro positions. Measured gate and per-step UI editing deferred (ledgered).
- Params (defaults = silent until recorded): 15 keys `0x0EAD..BB` (RATE/MODE/SWING/GATE/PROB/DRIFT/TRANSPOSE + TRKLEN/RECARM/STEPCUR/CLEAR + STRIG/SPROB/SDRIFT/SENTR); 15 rows; SEQ 2 pages (page 1 fills the 7 dead slots, page 2 new); SEQLEN row untouched (v2 song window). CLEAR is momentary (nonzero clears). Matrix `DM_SEQ` (34): RATE/SWING/GATE/PROB/DRIFT/TRANSPOSE/TRKLEN/MODE (8, mirror knobs); voice `sxm[8]`.
- E0: tracks empty = rests; block-granular strikes; tick rewind/seek restarts (step index jumps — no carry); linear tick→sample approx; ppq 0 falls back to 96; macro playback overwrites knob positions (record first).
- **Status: P8c done 2026-10-01** (engine, keys, UI, tests):
  - Runtime-only tracks (no format change): 2×128 note steps (4 notes + vel/gate/trig/prob/drift/entropy) + 8×128 macro lanes. Transport pushed synchronously by the live task (3 additive engine fields + setter, next to tempo push); steps song-locked (`idx = tick/step`).
  - Playback: parallel/series, division 1/2/4/8, swing ≤50%, gate = step×track, prob = step×master threshold, drift ticks→samples (linear approx), trig sub-queue (64), entropy ±12, transpose ±24, macro set per striking step only (rests don't stomp knobs — poison fix). Strike path shared with arp; own gate countdowns; seek/restart/clear drain futures; schedule-once traversal robust to any tick granularity (hitch cap 16).
  - Record: realtime (crossed steps, empty held writes rests) + stopped cursor preview; captures note/vel 100/gate 100/trig/prob/drift/entropy from step-default knobs + macro positions. Measured gate + per-step UI editing deferred.
  - 15 keys `0x0EAD..BB`, 15 rows, SEQ 2 pages, `DM_SEQ` (8 mirror knobs).
  - Proof: audit 0/0 clean worktree; riaudio lane: window opens, SPACE→TR PLAY, AHI 0x003e0001, buffers=11322 xruns=0. Deviations: no host wav; SEQ pixels deferred (standing gap); 909 pack missing (unrelated).
- **Tests:** t143 (off-identity, realtime split + defaults, step record, playback order/content, trig ratio, prob mute, drift deferral, entropy wobble, transpose, series alternation, division density, length wrap both directions, clear incl. drains, macro capture/play, swing checkpoint, mode clamp, stopped silence, restart + clear drain (planted futures), DM route-fill + gate fold, gate knob, factory-adjacent pins, full key sweep, row bind, 2 pages + texts, null guards, extremes; 26 mutants killed, 2 ledgered survivals: live transport push + engine seq-block hook — no engine-render unit laws, lane-covered); t60 range to `0x0EBB`, t77 mapped 269 / allow 1109, t133 last module SEQ, t136/t138/t139/t140/t141/t142 `0x0EBC` boundary deliberate moves.

#### P8d plan (ribbon; before code 2026-10-01)

- Device ribbon state (no live-input path exists anywhere — the panel/API drives it; app touch wiring later, ledgered): `rbn_pos` (0..127, default 64), `rbn_touch`, `rbn_mode` (0 off, 1 abs, 2 rel, 3 theremin), `rbn_last` (relative baseline). API `levi_ribbon_touch/move/release(s, pos)` (2 bad on NULL) + keys `0x0EBC` (MODE), `0x0EBD` (POS), `0x0EBE` (TOUCH, momentary: nonzero = touched).
- Sources (P5b ids already exist, no DM work): computed once per `sum_stereo` top (P8a refresh pattern) into per-voice fields (voice renders can't see the set): ABS bipolar `pos/127*2-1`, ABS+ unipolar `pos/127`, REL `clamp((pos-last)/64)` with block-granular consume. Mode off zeroes all three (sources inert).
- Theremin (mode 3 + touched): touch/move retunes all active voices to `pos` (clamped note, via `alloc_retune` — quantize/micro apply; instant, no glide). Held list untouched (release behavior unchanged).
- ENV triggers: touch starts menvs with source 7; release releases menvs with source 8 (`menv_release` is idle-safe). Op envelopes have no trigger sources (menvs only).
- Seq step selector: touch jumps `seq_lastk` to the mapped step (`pos/127*trklen - 1`, restart-like); move doesn't (jitter); harmless stopped.
- UI: `M_RIBBON` 36 (`NMOD` 37, appended — existing ids stable) with 1 page (MODE/POS/TOUCH + 5 dead); rows 211..213; texts OFF/ABS/REL/THEREMIN, %, ON/OFF; TOUCH press toggles (bound-switch pattern).
- E0: off default (bit-identical songs); POS key never implies touch (songs stay silent); ribbon position has no MIDI/panel producer yet.
- **Status: P8d done 2026-10-01** (engine, keys, UI, tests):
  - Ribbon state + API (touch/move/release) + keys `0x0EBC..BE` + rows 211..213 + `M_RIBBON` 36 (appended, ids stable) with 1 page (MODE/POS/TOUCH); per-type texts; TOUCH press toggles.
  - Sources computed per block into voice copies (render has no set pointer — P8a refresh pattern): ABS bipolar, ABS+ unipolar, REL consumed; off zeroes.
  - Theremin (mode 3 + touched) retunes actives; touch/release drive menv trig 7/8; touch jumps seq grid.
  - Proof: audit 0/0 clean worktree; riaudio lane (restarted after PC reboot — anonymous session, no pairs file needed): window opens, SPACE→TR PLAY, AHI 0x003e0001, first post-boot PLAY 9 xruns (cold AHI transient), steady PLAY 4143 buffers 0 xruns. Deviations: no host wav; RIBBON pixels deferred (standing gap); 909 pack missing (unrelated).
- **Tests:** t144 (defaults/off-identity, ABS/ABS+/REL incl. consume, touch/release trigger laws, theremin retune/glide, mode/pos clamps, mode-0 silence, touch-key path, seq jump, twin-set audible law, full key sweep, row bind, module/page/slots/texts incl. press toggle, null guards, extremes over 4 modes; 20 mutants killed, 1 benign survival: engine POS clamp — refresh re-clamps, P7b defense class); t60 range to `0x0EBE`, t77 mapped 272 / allow 1112, t107 module clamp 36, t136–t143 `0x0EBF` boundary deliberate moves.

#### P8e plan (step-LFO editor; before code — queued after P8d)

- Per-LFO 64 step values in `struct RILeviLFO` (`int8_t sval[64]`, all voices + the shared copy written by one setter: 45 tables x 64 B ~ 2.9 KB), plus a per-instance `sown` (table in use) and `semi` (LP_SEMI). `RI_LEVI_MAXSTEPS` (64) already reserved the size.
- **Default stays the analytic ramp** (P5a ledger): while `sown == 0` the step wave reads `lfo_step_value(n, k)` exactly as before, so songs are bit-identical. The first write *materialises* the current ramp into the table (7-bit quantised, repeating every `n`) and sets `sown = 1` — continuity: editing one step leaves the others where they were. A table once owned is static: a later STEPS/matrix change does not re-lay it (RAMP does).
- Values are 7-bit bipolar: `v = (val - 64) / 63` clamped to +-1, so 0 = -1, 64 = 0, 127 = +1. SEMI LOCK (LP_SEMI 14) snaps a written value to the 1/12 grid (a semitone over a two-octave bipolar range; 25 levels, E0).
- Keys: `RI_LEVI_LSKEY(l, f)` = `0x1300 | l << 2 | f` for LFO 0-3 (STEP cursor, VALUE, RAMP) + `0x1400 | f` for LFO 4 (LFO indices 0-4 = LFO 1-5 in the panel) — cursor+value like the P8c sequencer editor rather than 320 per-step keys, so a recorded edit is small. Allow-list: two block rules, 15 keys (`0x1300-0x130E`, `0x1400-0x1402`). **Amended from the sketched `0x13`/`0x14` split:** the "0x13xx is macro-route spill (`RI_LEVI_MRKEY` reaches 0x13FF), so the step block lives in `0x1400`" note was wrong — `ri_auto_allowed` tests the block rules *before* the flat allow table, so a key in a block the editor owns is judged by that block's rule and cannot collide with the table (t77 gained the matching block-owner exemption for `0x13`/`0x14`). LFO 5 then takes its own block at `0x1400`.
- UI: LFO page count 2 -> 3. Page 2 slot 6 becomes live SEMI LOCK, slot 7 a `STEP EDIT` gate (0/1) that switches to page 3 and back; page 3 = STEP cursor + VALUE + RAMP + 5 dead. **Amended from the sketched press-nav:** encoder-press routing is not proven on-target (standing pixel/press gap), so the bound-gate pattern (ARPON/SEQON/FXPRE) is used instead — same effect, proven path.
- **E0 ledger** (own laws, no manual statement): semitone grid = 1/12 of the bipolar range; first edit materialises the ramp (continuity); the step table is device-wide (all voices + shared, like `ui[]`) while phase/stagger stay per-voice; only steps `< n` sound (n = STEPS + matrix offset), so the 64-step grid can hold steps the LFO does not read yet; the panel VALUE knob mirrors the last written value (no engine->panel read path exists, same standing gap as the P8c per-step display); the step table is runtime state (song persistence rides the P10 owner review, like the P8c tracks).
- **Status: P8e done 2026-10-01** (engine, keys, UI, tests):
  - Engine: `int8_t sval[RI_LEVI_MAXSTEPS]` + `sown` + `semi` in `struct RILeviLFO` (8 voice copies + the shared copy = 45 tables, 2.9 KB), per-instance cursors `RILeviSet.lsc[RI_LEVI_NLFO]`, device-wide setter `levi_set_lfo_stepctl(set, lfo, field, val)` (0 ok / 2 refuse). While `sown == 0` the step wave reads `lfo_step_value(n, k)` exactly as in P5a (songs bit-identical); the first VALUE write materialises the ramp and sets `sown` on every copy; RAMP with a non-zero value clears `sown` on every copy (an undo, no refill) and RAMP 0 leaves an owned table alone. Cursor clamped to 63 on write, held inside `n` on read (`if (k >= n) k = n - 1`), so shrinking STEPS under a held index stays in range.
  - Keys/dispatch: `RI_LEVI_LSKEY(l, f)` = `0x1300 | l << 2 | f` (LFO 1-4) + `0x1400 | f` (LFO 5); fields STEP 0 / VALUE 1 / RAMP 2, field 3 refused; two allow rules (`RI_AUTO_BLK_LEVILS0/1`); `RI_LEVI_LP_SEMI` 14 with `RI_LEVI_LP_N` 14 -> 15, so `LFOKEY(4,14)` is allowed and `(4,15)` refused; dispatch in `engine_automation` before the LEVIMOD branch (`lf` from the block, field `lo & 3`); `app/riapp.c` song mirror for both blocks.
  - UI: LFO page count 2 -> 3; page 2 slot 6 = SEMI LOCK (`LFOKEY(l,14)`, FREE/LOCK), slot 7 = the STEP EDIT gate; page 3 = STEP cursor / VALUE / RAMP with slots 3-6 dead and slot 7 the gate again; `RI_SLEVI_LFEDIT 214` SWITCH row (UI-only, `RI_SLEVI_NCTL` 215), `SLOT_LFS(f) = -(144+f)`.
  - Proof: audit 0/0 PASS; ASan/UBSan clean on t145 + the five boundary tests; mutation 46/46 killed; riaudio lane (fresh boot, `SetEnv RIAPP_EVLOG RAM:` first — without it the app blocks on a requester asking for the log name and never shows a window): window "RIAPP live panel" 0,0 974x680, panel draws, AHI 0x003e0001 48 kHz / 256 frames / 5333 us, SPACE -> TR PLAY, heartbeat 4951 buffers 0 xruns render_max 2593 us load 165/1000 overloads 0, guest wav peak 13354 (0.41 FS) with 33% of 5.2M samples non-zero (the first 39 s of the capture are the idle pre-PLAY silence). Deviations: LFO page-3 pixels deferred (standing tab-nav/focus gap, same as VOICE/DELAY/SEQ/RIBBON); 909 pack missing (unrelated lane note).
- **Tests:** t145 (defaults/unowned identity incl. an audible same-age twin law; first-write materialisation; panel bottom-end clamp; an owned step change is audible (twins differing only in `stepk == 1`); RAMP restores the ramp; RAMP 0 keeps an owned table; shrunken `n` holds the index; SEMI grid with 25 levels; device-wide table; steps beyond `n` stored but unread; clamps/refusals; both block rules; LP_SEMI as param 14; engine dispatch; panel pages 1-3 with texts and the gate press; finite extremes) — 46 mutants, all killed, after strengthening two weak laws (the step-index clamp needed a trigger-advanced `stepk`, since `ri_levi_lfo_step` alone does not advance it; the RAMP-0 law had asserted `sown == 0` where the law is `sown == 1`). t77 allow 1132 / mapped 272 + block-owner exemption, t132 `page_count == 3` and `LFOKEY(4,14)`, t129 unknown-block probe `0x1300 -> 0x1500`, t79 delivery boundary.

### P9: performance

Chord mode, octave buttons, Single/Multi with Upper/Lower/Both, Dual/KeySplit and balance, glide button, tap tempo.

Scope note (2026-10-01): the coverage-table row plus the ledgered deferrals that name P9 — "velocity > env is stored for P9" (P2), "PolyAT amounts need P9 pressure" (P4), "bend source still 0 until P9" (P6b), "performance sources read 0 until their data arrives" (P5b). Own laws throughout (clean room; manuals stay out of the repo). Split a–e like P6/P7/P8:

- **P9a performance signals** — the data: note velocity, release velocity, poly/mono aftertouch, mod wheel, pitch bend; matrix sources 27..30/33/34 come alive.
- **P9b performance amounts** — the six dim panel slots ("VEL>ENV", "POLYAT" on the digital filter, analog filter and VCA) go live, plus the stored per-op VELENV (param 28) and mod-env VELCRV (param 29).
- **P9c keyboard zones** — octave bias, Single/Multi, Upper/Lower/Both, Dual/KeySplit, balance.
- **P9d glide button + chord mode** — momentary glide hold; chord mode plays the pushed lane chord from one key.
- **P9e tap tempo** — transport TAP control (the one app-level slice).

Still no live-input producer anywhere in the tree (P8d ledger), so a–d are driven by the device API + panel exactly like the ribbon; the MIDI/interop touch wiring rides the interop work. Per-zone *patches* (the real instrument's two layers) ride P10 and its chunk-ID owner review — E0 maps Single/Multi onto zones over the one patch.

#### P9a plan (performance signals; before code 2026-10-01)

- Live API on the set (device-wide; 0 ok, 2 on NULL; values clamped, no refusals beyond that): `levi_note_vel(s, note, vel)` allocator note-on carrying velocity (`levi_note_on` = `vel 127`, so songs and every existing caller are bit-identical), `levi_note_rel_vel(s, note, vel)` note-off carrying release velocity (`levi_note_off` = `vel 127`), `levi_press(s, p)` channel aftertouch, `levi_polyat(s, note, p)` per-key aftertouch on the voices sounding `note`, `levi_wheel(s, w)` mod wheel, `levi_bend(s, semis)` pitch bend in semitones clamped to ±24.
- Voice state (copied per block in the stereo sum's refresh, the P8d pattern — the render has no set pointer): `nvel`/`nveloff` (raw note-on and release velocity 0..127, kept raw for the P9b amount laws) plus the normalised copies `vel01`, `veloff01`, `pat01`, `mpat01`, `wheel01` and `bsrc`.
- Sources: 27 POLYAT `pat/127`, 28 MONOAT `press/127`, 29 VELON `nvel/127`, 30 VELOFF `nveloff/127`, 33 WHEEL `wheel/127`, 34 BEND `clamp(s->bend / vbendrng, -1, 1)` — the P6b bend range is the normaliser (its UI 0..127 maps 0..24 semitones, default 2), so bend range 0 reads 0 (fail closed) and ±range reads full scale.
- Bend pitch: `bsrc * vbendrng` joins the P6b per-sample semitone offset `vsemi` that detune, vibrato, glide and analog feel already feed (amended at code time 2026-10-01: the first sketch had a flagged per-block frequency retune; the offset path is the law the engine already has, costs one add, and composes instead of fighting a glide in flight). Bend therefore moves every sounding voice per sample and needs no retune pass, dirty flag or shared frequency helper.
- E0 ledger: no route = no sound change (bit-identical songs — the velocity/pressure amounts all default to none); live default velocity 127; a voice's release velocity starts as its note-on velocity and is re-stamped by the note-off that releases it (mono modes never release, so their voice keeps the note-on velocity; a velocity-less note-off writes the same 127, so songs are untouched); the per-voice pressure slot is cleared when the voice fires, so a voice reused for another key starts at no pressure; aftertouch and wheel are absolute levels (the caller returns them to 0 — no spring return invented); poly aftertouch is per key (the voice sounding that key), channel pressure reaches every voice; the per-block copies mean a signal set mid-block is heard from the next block (the P8d ribbon cadence); song lanes (`levi_trigger`/`levi_release`, lane==voice) keep 127/127 so nothing there moves.
- **Status: P9a done 2026-10-01** (engine + sources; no keys, no panel rows — every P9 amount/zone UI rides P9b/P9c, so this slice is device-API only, the P8d pattern):
  - Engine: `levi_note_vel` / `levi_note_rel_vel` / `levi_press` / `levi_polyat` / `levi_wheel` / `levi_bend` on the set (0 ok / 2 on NULL, values clamped, nothing refused beyond that; `levi_note_on`/`levi_note_off` are the velocity-127 wrappers, so every existing caller and every song lane is untouched). Set state `pvel` (pending note velocity), `rvel` (pending release velocity), `press`, `wheel`, `pat[RI_LEVI_NVOICES]`, `bend` (semitones, clamped to ±24); voice state `nvel`/`nveloff` (raw 0..127) plus the per-block normalised copies `vel01`, `veloff01`, `pat01`, `mpat01`, `wheel01`, `bsrc` (bend against the P6b range, clamped ±1).
  - Stamps: `levi_trigger` writes `nvel`/`nveloff` from the pending note velocity and clears the reused voice's key-pressure slot; each `levi_note_off` branch re-stamps `nveloff` with the release velocity (mono voice 0, unison every voice it fired, poly the sounding voice) — mono/unison modes that never release keep the note-on velocity.
  - Matrix: sources 27 POLYAT, 28 MONOAT, 29 VELON, 30 VELOFF, 33 WHEEL, 34 BEND (`RI_LEVI_MS_N` 43) added to the source table, the UI list and the `levi_mod_apply` source vector, so any destination can be driven by any performance signal. Pitch: `bsrc * vbendrng` joins the P6b per-sample `vsemi` offset (no retune pass, no dirty flag, composes with detune/vibrato/glide/analog feel).
  - Tests: t146 (16 law groups) — see the mutation and test notes below.
  - Proof: audit 0/0 PASS; ASan/UBSan clean on t146 + t140/t142/t143/t144/t145; mutation 37/37 killed. On-target: the audit's AROS RIAPP link gate passes (0 unresolved, 0 r12 base moves) and the guest binary carries all six new entry points (`x86_64-aros-nm`: `levi_note_vel`, `levi_note_rel_vel`, `levi_press`, `levi_polyat`, `levi_wheel`, `levi_bend`). **Lane run deferred** — the single spool session is held by another lane (`spike_server.py serve --port 9091` since 16:06 with the `nvkb1` guest connected), and a guest dials in once per boot, so the riaudio boot cannot be driven; the VM is left booted and the run proof moves to the P9b pass. Standing gaps unchanged (panel pixels, encoder-press nav).
  - **Tests:** t146 (`tests/unit/t146_levi_perfsig.c`, 16 law groups): API contracts and NULL refusals; the clamps (velocity 200 → 127, release 200 → 127, pressure 200 → 127, wheel 200 → 127, bend ±40 → ±24); note velocity stamped at fire and kept across a release (a velocity-less note-on reuses the last note velocity); the release stamp in all three release branches (mono, unison every voice, poly); the per-voice key-pressure clear on voice reuse; poly aftertouch per key (one block of latency, the other voice untouched) and channel pressure reaching every voice; the six normalised copies and the one-block latency of each; bend against the range (default 2 semitones, ±range reads ±1, past the range clamps at ±1, range 0 reads 0, range 24 reads `bend/24`) and the ±24 clamp on the setter; the pitch law measured against a key a semitone-equal number up (7 semitones of bend over the 24 semitone range within 1e-3 of note 67, float-tolerant because the pitch is a table lookup times a power); six source laws as same-age twin sets routed to the VCA level at depth 100 (the level route scales the amp by `1 + x`, the D.FILTER cutoff is barely audible in the default patch), read as **peak** rather than samples so the bend pitch move cannot pass for a source move; the no-route bit-identity law; source ids, names and UI indices.
  - 37 mutants, all killed: the six API bodies (velocity/release/pressure/wheel/bend stamps, per-key pressure routing to the wrong voice, the release-velocity stamp in each of the three branches, the release path writing the note velocity, the bend setter never moving the value); all seven init defaults; all six per-block refresh lines (constant, silent, and the pressure read from voice 0 for all); the bend normalisation (fixed 2 semitones, unnormalised, both clamps widened) and the pitch law (inverted, range dropped); all six `src[...]` lines (silent, and VELOFF reading the note velocity).
  - Three laws had to be strengthened because a mutant survived honestly, all three worth naming: the audible twins were first routed to the D.FILTER cutoff, which a voice barely moves in the default patch (the twin pair passed on a 0.8 % sample difference — a law thin enough to pass for the wrong reason), so the destination became the VCA level and the comparison became peak amplitude; the bend clamp laws first bent 7 semitones over the default 2 semitone range, which is 3.5 before the clamp, so *widening* the clamp to ±2 still clamped to ±1 and the mutants survived — they now bend 3 semitones (1.5 before the clamp); and the BEND source law first compared samples, which differ anyway because the same signal also moves the pitch, so the source mutant survived — it now reads the level like every other source law. A fourth law (the pending note velocity surviving a release) was added because a stray `pvel` write in the release path was invisible until the next velocity-less note-on.
  - Test traps hit while writing t146 (each one cost a probe): a release with no render in between is instant (the envelope never left stage D, so the tail carried no signal); `levi_set_param_ui(..., RI_CTL_LEVI_VBENDRNG, 2)` means 0.378 semitones, not 2 (the UI maps 0..127 to 0..24, the default is already 2) — two clamp mutants hid behind that; a matrix source of 0 on a depth-100 route silences the voice, so twins keep their source off the closing stop; the set's own level destination is `RI_LEVI_DM_VCA` param 0 (`LEVEL`), and `*evlevel *= 1 + x` makes it the one destination a source is guaranteed to move audibly.

#### P9b plan (performance amounts; before code 2026-10-01)

- Six new device-wide amount rows + keys, filling the dim panel slots P5 already drew: digital filter VEL/POLYAT, analog filter VEL/POLYAT, VCA VEL/POLYAT (keys `0x0EBF..0x0EC4`, rows `RI_SLEVI_DVEL..RI_SLEVI_VPAT`, `RI_SLEVI_NCTL` 215 -> 221). Amounts bipolar −1..+1 (UI 64 = none), like the P5 ENV amounts: cutoff/VCA level scale by `(1 + x)`, pan by `x` (unipolar amount 0..1 over a one-sided range? decided at code time, pinned in the tests).
- Stored-then-inert params go live: per-op `RI_LEVI_OP_VELENV` (28) multiplies the oscillator envelope level by `(1 + a(2·vel01 − 1))` with `a = ui/127`; mod-env `RI_LEVI_ME_VELCRV` (29) does the same with `a = (ui − 64)/64`. Both default to none (0 / 64), so voices are unchanged until set.
- E0 ledger: velocity authority is multiplicative on levels (a soft note closes its envelope, a hard one opens it further) rather than an additive offset; the amount is stored per device (section-wide apply) like every other P4/P5 amount; PolyAT amount reads the per-key pressure source (channel pressure only where the law says so — decided at code time, pinned).
- Panel: the six dim slots become live rows with names already drawn ("VEL>ENV", "POLYAT"); no new module, no page move.

#### P9c plan (keyboard zones; before code 2026-10-01)

- Device-wide performance fields on the set (`p_mode`, `p_sel`, `p_split`, `p_oct`, `p_bal`) behind one setter `levi_perf_set(s, field, val)` (0 ok / 2 bad field, values clamped), plus panel keys/rows and one new module page `M_PERF` (OCTAVE / MODE / SELECT / SPLIT / BALANCE + 3 dead).
- Zone routing happens in the allocator (live, arp and sequencer strikes) and never in `levi_trigger`/`levi_release` (song lanes, bit-identical). A note is first shifted by the octave bias (`note + 12·oct`, clamped 0..127), then offered to the zones: SINGLE (default) sounds it once at unity; MULTI + KEYSPLIT sends notes below the split key to the lower zone and the rest to the upper; MULTI + DUAL sends every note to both zones (two voices, the stack); SELECT gates which zones accept a note-on (LOWER / UPPER / BOTH).
- Balance: `gUpper = bal/64`, `gLower = (128 − bal)/64` (UI 64 = both unity, 0 = lower only, 127 = upper only), stored per voice at the trigger as `zgain` and applied in the voice render (1.0f default = exact identity).
- E0 ledger: **note-off is never gated** (a released key always stops its voice — zones only decide what may sound); the octave bias moves new notes only (held notes do not follow the knob; the ribbon theremin is the retune path); a zone-gated note-on returns 0 voices applied; arp/seq strikes pass through the zones (the keyboard split covers what the arp plays); per-zone patches ride P10.

#### P9d plan (glide button + chord mode; before code 2026-10-01)

- Glide button: `levi_glide_hold(s, on)` sets a device-wide hold copied into each voice as `gforce` at fire time; the glide decision becomes `v->vglide || v->gforce` (P6b time/curve untouched), so the button is a momentary override of the mode and releasing it hands the voices back to the mode. Panel key/row on the VOICE page (a free slot) or the PERF page.
- Chord mode: `levi_chord_mode(s, on)` + `levi_chord_set(s, on_flags, notes, n)` (n <= 6 lanes — the app pushes a row; no lane storage in the device). With chord mode on, one played key fires the whole pushed chord as held notes transposed by `played − chord root` (clamped 0..127) at the played velocity; any key release releases the whole chord.
- E0 ledger: chord mode is off by default (songs bit-identical); an empty chord row makes a played key silent in chord mode (no fallback to the single note); strikes never enter chord mode (arp/seq play their own notes); the root is the lowest `on` lane and the transposition follows the played key, so the keyboard still transposes the chord.

#### P9e plan (tap tempo; before code 2026-10-01)

- Transport section (app level, the only non-device slice): a TAP control that on press feeds a millisecond clock into a pure tempo estimator (mean of the last min(4, n−1) intervals, gaps over 2 s restart the series, result clamped 20..500) and writes the session tempo — the existing `RI_STR_TEMPO` read in `app/riapp.c` already drives `ri_live_set_bpm`, so the tempo reaches the engine and the device tempo push for free.
- E0 ledger: one tap alone changes nothing (a tempo needs two); the estimator lives in `gui/secttr` as a pure function of injected milliseconds so it is unit-testable without a clock; TAP is momentary (a press, not a latch).

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
