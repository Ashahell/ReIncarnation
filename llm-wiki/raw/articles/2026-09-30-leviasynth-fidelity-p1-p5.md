# Leviasynth fidelity plan P1–P5: hardware panel, oscillators, algorithms, filters, modulation (owner 2026-09-30)

- Source: ReIncarnation session, 2026-09-30 (owner requests and verdicts in chat)
- Collected: 2026-09-30
- Published: 2026-09-30
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (phases P1–P10, per-phase status, E0 ledgers)
- Requirement + E1: [2026-09-28-planned-device-asm-leviasynth.md](2026-09-28-planned-device-asm-leviasynth.md)
- Prior: [2026-09-29-leviasynth-v2-arp-seq-matrix-lfo-reverb.md](2026-09-29-leviasynth-v2-arp-seq-matrix-lfo-reverb.md)
- Commits: `825b460` (P1), `8fab26c` (P2), `d683c91` (P3 + CV/Gate removal), `48a741c` (P4), `139859c` (P5a), `8bcce7f` (P5b). Not pushed at collection time.

## Owner request and decisions

- Request: make the Levi look and sound as close to the hardware as possible, emulate all features, research, use reference pictures, keep the current font in a contrasting colour, plan first, use the audit script.
- Decisions: per-oscillator params get their own control block `0x0F` (approved); panel look "close enough" (approved); P2, P3, P4 sound "sounds fine" on the Dell.
- CV/GATE section: "useless in a soft synth", removed (P3 commit); the Arp & Seq block takes the column. Plan open decision 4 closed.
- Clean-room holds throughout: no maker marks, manual text, waveforms, algorithm tables, filter or product names; manuals stay in scratch, out of the repo. Own names for waves, algorithms, filter models, LFO waves, vowel orders.

## P1 — hardware panel + page UI (`825b460`)

- Section 18 canvas 1756×560 Q generated from the manual cover render: xQ = (x−75)·0.6245, yQ = (y−48)·0.6245.
- MODULE SELECT keys, OSC keys, 8 encoders around the LCD, page model; encoders send the page's target (automation keys unchanged).
- Traps: labels over 23 characters need the face path (`levi_text`); encoder ring had to stay inside its damage box (t112); overlapping keys → white-key hit band is the lower part only (t61).

## P2 — oscillators + envelopes (`8fab26c`)

- Per-op keys `0x0F00 | op << 5 | param` (32 params); 128 own waves (8 families × 16, PolyBLEP); Semitone/Ratio/Frequency pitch modes; init/env level, feedback, keytrack, phase, Direct Out.
- Mode belongs to the MODULATOR (FM/PM feeders move frequency/phase; PW/Sync/PD feeders warp phase).
- Full DAHDSR per op: curves, quantize, counted/infinite loops over a stage range, freerun, Fast/Slow ranges; Osc Env Level & Bias knobs `0x0E27..2A`.
- Finding: the v1 envelope law re-read the running value every sample, so segments collapsed early; fixed to true ramps (deliberate timing change). Defaults keep the v1 FM index bit for bit.

## P3 — algorithms (`d683c91`)

- 64 own loop-free topologies (edges point to a lower op; op 1 always a carrier; all distinct); presets 1–8 are the v1 eight. Routing is a feeds mask per op with topological render order.
- Modes Single / Morph (8-slot list, OFF skipped, SILENCE silent, 100 steps per pair) / Custom (up to 3 targets per op, loop targets dropped); Solo auditions one op (modulators too), Mute drops ops. Keys `0x0E2B..37`; custom targets are per-op params TGT1–3.
- Bug caught by the new test: slot value SILENCE (64) equalled the CUSTOM bank id (64), so a SILENCE slot played the custom grid; fixed with a separate bank id.
- UI state trap: the ALGO row and SLOT 1 row are the same thing; the panel model must mirror them or a resync overwrites the algorithm.

## P4 — filters + VCA (`48a741c`)

- 18 own digital models (two morphing SVFs, 3 HP, 2 BP, 10 LP, a 3-formant vowel filter with 8 own orders); analog 4-pole with gain-compensated pre-drive; keytrack around C2 (digital 0 %, analog 100 %); LFO 1/2/3 pre-wired amounts; OSCs / D.Filt / VCA / Patch levels (64 = unity). Keys `0x0E38..44`; v1 FTYPE becomes a legacy row mapped onto the models.
- Finding: a ladder with a one-sample delayed feedback self-oscillates at a cutoff-dependent resonance (at 5 kHz already at 90/128, at 200 Hz only near 113/128). Solving the loop without the delay (each TPT stage is y = a·x + c) makes the threshold cutoff-independent; with k at full resonance 4.65 the analog filter self-oscillates from ~110/128 as the manual states, at the cutoff pitch. Every feedback loop soft-clipped → bounded, no inf/NaN.

## P5 — modulation (`139859c`, `8bcce7f`)

- ENV 1–5 per voice on the oscillator DAHDSR law, trigger sources (note on, LFO cycle start), level; pre-wires ENV 1 → digital cutoff, ENV 2 → analog cutoff (±8 octaves, top-panel knobs live), ENV 3 → VCA with Initial Level. ENV 3 closing now ends the voice (a deliberate sound change: before, only the oscillator envelopes did).
- LFO 1–5: 11 own waves, slow 0–25 Hz / fast 5–150 Hz, trig sync poly/single/off with stagger, delay/fade, quantize, level, smooth, one-shot on/step, start phase; deterministic noise (xorshift). v1 LFO keys unchanged, bit for bit.
- Matrix: 32 routes; sources in the manual's groups (performance sources read 0 until P6/P8/P9); destinations as module + parameter so every field fits a 7-bit key value; route depths take Mod Matrix depth modulation first; 8 macros with 8 routes, knob/button.
- Key blocks: `0x10` ENV/LFO params, `0x11` matrix routes (slot << 2 | field), `0x12` macro routes (macro << 5 | route << 2 | field); macro knobs/buttons `0x0E48..57`.

## Method findings

- Mutation testing per phase (P3 12, P4 21, P5a 32, P5b 25 mutants killed). A mutant that fails to compile (unused variable, empty `if` body, unused function) leaves stale objects and reports a false PASS; also a mutant in a file of another build module (autolane is in `sched`) reports a false PASS unless that module is rebuilt.
- A failed module build (implicit declaration) left stale `sectlevi.o` and produced confusing unrelated failures ("pat kind"); always check the build output before reading test failures. Hash pins collected from a stale build must be re-collected after a full rebuild.
- Tests that measure brightness by zero crossings fail on 1-pole filters (the fundamental dominates); slope-energy over energy works. An envelope stage with zero time is skipped within the same tick, so "start" counts must watch the attack stage, not delay.
- Every phase: clean-worktree audit (0/0 PASS), Dell ABIv11 build (0 UND), deploy as `RAM:RIAPP`, owner sound check.

## Deferred (per plan)

- Velocity, aftertouch, wheels, ribbon, MPE, pedals (P9); tempo sync and step-LFO editor (P8); voice modes, pan, legato/reset behaviour (P6); FX (P7); patches, macro names/sort/audition (P10). MIDI CC and CV sources omitted.
