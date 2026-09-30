# Leviasynth fidelity P6a (allocator) + P6b (voice params), host-complete (owner 2026-09-30)

- Source: ReIncarnation session, 2026-09-30 (opencode lane; owner voice-count + stereo verdicts in chat)
- Collected: 2026-09-30
- Published: 2026-09-30
- Plan: `docs/superpowers/plans/2026-09-30-leviasynth-fidelity-plan.md` (P6a plan + status in-doc; P6b plan in-doc, status pending commit)
- Requirement + E1: [2026-09-28-planned-device-asm-leviasynth.md](2026-09-28-planned-device-asm-leviasynth.md)
- Prior: [2026-09-30-leviasynth-fidelity-p1-p5.md](2026-09-30-leviasynth-fidelity-p1-p5.md)
- Commits: `a5c3827` (P6a, pushed? no — not pushed at collection). P6b **uncommitted** (Dell proof blocked; see the lane record).
- Owner calls 2026-09-30: **8 voices, stereo.** P6b = params mono; P6c = stereo + scales/microtuning/vintage; P6d = 6→8 voices (pattern lanes stay 6, songs identical).

## Host measurement (P6 gate; O2, 48 kHz, 8 carriers + 32 matrix routes + 5 LFOs + 2 filters)

- 6v 7.94 us/sample (38% of the 20.83 us budget), 8v 10.58 (51%), 12v 15.87 (76%), 16v 21.16 (102% — over budget even on the fast host; the Dell builds `-O0` on older silicon, worse). Recommendation recorded: 8 voices.

## P6a status: done (`a5c3827`; 18 mutants killed; audit 0/0; Dell 0 UND, never deployed)

- Allocator `RILeviAlloc` in the set (mode/density/limit, rotate cursor, 16-note held list). `levi_note_on/off` for live notes; direct `levi_trigger/release` stay lane==voice for songs, bit for bit.
- 9 modes: Rotate (scan from cursor, steal advances), Reassign (first reusable = silent or released-ringing, else oldest-held), Mono/Lo/Hi on voice 0 with held-note fallback, Unison over the limit, UnisonPoly (density per note up to the limit, steal oldest).
- Legato/reset (P2 flags live): mono overlapping legato note with no reset flag retunes only (op freqs + filter keytrack; envs/LFOs/menvs keep running); any reset flag forces full retrigger; poly/unison always full-retrigger except unison voice 0. E0: legato = retune-only; reset wins; reusable = silent or released-ringing.
- Keys `0x0E58..5A` (polymode 0..8 clamped, density `val*8/127+1`, limit `val*6/127+1` clamped to `RI_LEVI_NVOICES`); registry rows 129..131; VOICE page slots 0..2 live; engine section-wide apply rides the existing `0x0E` branch (no `engine.c` change).
- Tests: t134 (rotate steal-advance, reassign, mono priorities + fallback, unison, unison-poly clamp, legato vs reset incl. retune==trigger pitch law, bit-identical direct path, finite/bounded 9-mode storm, keys/allow/pages); t60 range to `0x0E5A`, t77 mapped 186 / allow 1013 (message arithmetic kept); t92/t93 unchanged.

## P6b status: host-done, uncommitted (19 mutants killed; audit 0/0; ASan/UBSan clean; Dell 0 UND)

- Keys `0x0E5B..67` (13): DETUNE, AFEEL, RNDPH, PAN, WIDTH, PANMODE, BENDRNG, VIBRATE, VIBAMT, VIBDLY, GLIDE, GLTIME, GLCURVE. Registry rows 132..144 (`Voice` group); VOICE pages 1/2 + 2/2.
- DSP per voice, one semitone offset per sample (`vpitch`, exact 1.0 when every feature rests — legacy path bit-identical): detune ordinal spread ±50 c full scale; analog feel (two slow-sine wander, ±6 c pitch + cutoff wobble); random start phase (static voice+op hash, retrigger-identical); vibrato (0.1–20 Hz exp, 0–4 st, 0–5 s ramped delay, trigger-reset phase); glide (portamento on every trigger incl. legato retunes, 0–5 s quadratic, parabolic ease, gliss semitone steps; time 0 = instant); bend range 0–24 st default 2 (source reads 0 until P9, inert); pan/width/mode stored for P6c.
- VoiceMod live: VMOD = per-voice hash bipolar, VMOD+ = ordinal unipolar (deterministic, no RNG).
- Matrix `DM_VOICE` (28, 10 params: DETUNE, PAN, AFEEL, BEND, VIBAMT, VIBRATE, GLIDETGL, GLTIME, GLCURVE, PANWIDTH); t133 module knob now spans to VOICE (deliberate UI move).
- E0: detune static per voice index (doubles as analog spread in poly); glide retriggers from current pitch every note; vibrato resets phase on full triggers only; bend = 2^(src×range/12) with P9.
- Tests: t135 (defaults bit-identical; per-feature move/inert laws; VoiceMod deterministic + per-voice; DM_VOICE route; keys/allow/pages incl. VOICE 2 pages; extremes bounded). Gap ledgered: pan/width/mode/bend maps unobservable in mono (P6c stereo + P9 bend cover them).

## Dell proof: blocked, code exonerated (detail in the lane record)

- P6a, P6b and a clean-tree P5b rebuild (`8bcce7f`) all take Software Failure at startup on the post-reboot Dell — common factor is lane state, not the diff. P6b held uncommitted per the definition of done.

## Method findings (portable)

- `render_sum` mixes every active voice: per-voice test laws need isolated sets (same trap twice: vmod and rndphase blocks compared a solo against a duet).
- FM is chaotic: different slide paths never reconverge bit-exactly — a "glide settles" law is unachievable; assert progress-state completion instead.
- Matrix modulation saturates clamps: a VMOD→cutoff test at the 12 kHz default clamps both voices to 18 kHz (identical); lower the base cutoff for headroom.
- Audit Phase 0a bans the substring `free(`: `alloc_free` tripped it; renamed `alloc_reuse`.
- Stale objects again: rebuild the owning module (`dsplevi`) before reading any test result; `mut.sh` pattern (apply → rebuild module → run → restore → rebuild), with compilable-only mutants (unused-variable/function `-Werror` traps).
- Gliss-vs-glide mutant lesson: a mutant that SWAPS two behaviors survives a differ-law; use a never-true guard so the feature goes dead.
- Debug prints with unspecified argument evaluation order lie (`set_value` return vs loaded field in one `printf`).
- Stuck SPACE from `sendkey` (lost release) storms PLAY/STOP — same stuck-key trap as the 2026-09-26 live-ahi record.
