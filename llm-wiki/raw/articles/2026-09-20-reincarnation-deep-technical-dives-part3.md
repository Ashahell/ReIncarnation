# ReIncarnation — Deep Technical Dives, Part 3

**Ingested:** 2026-09-20 into ReIncarnation `llm-wiki`
**Source:** user-provided deep-dive part 3 text, stored verbatim below (only this header added).
**Status:** design input — companion to the Comprehensive Plan and Parts 1–2.

---

ReIncarnation — Deep Technical Dives, Part 31. The TR-808 — Voice-by-Voice Synthesis Recipes
The 808 is fully synthesized — no samples — and each voice is a small analog circuit with its own personality. Recipes below follow the well-documented circuit analyses (the 808 service manual and the public "TR-808 circuit modeling" literature). All oscillators are float phase accumulators; all envelopes are one-pole exponential decays unless stated.
Global convention: every voice renders into a mono buffer, multiplied by accent_gain = accent ? 1.0 : 0.66 (the 808's accent is a simple level push + slight envelope lengthening) and velocity from the step (0..127, default 100 → ×1.0).
c
struct RB808Voice {
    float phase1, phase2;       /* osc accumulators */
    float env_amp, env_pitch;   /* one-pole states */
    float noise_state;          /* for filtered noise voices */
    float freq, tune;           /* tune knob 0..127 → per-voice curve */
    float decay;                /* decay knob */
    uint8_t triggered;
};
1.1 Bass Drum (BD) — the voice that built genres
The 808 kick is a pitched sine with an exponential pitch sweep, plus a click transient. This is the single most important drum synthesis in electronic music, and it's gotten wrong constantly (usually by using too long a sweep or forgetting the click).
plain
Trigger:
  phase     = 0
  env_pitch = 1.0                /* full pitch start */
  env_amp   = 1.0
  click     = short noise burst  (2–3 ms, highpassed, ×0.3)


Per sample:
  f  = f_start · (1 + (f_end/f_start − 1) · (1 − env_pitch)^2 )
       f_start ≈ 170·2^(tune_curve/12) Hz, f_end ≈ 48 Hz
  env_pitch : one-pole decay, τ ≈ 22 ms          (fixed; the TUNE knob sweeps f_start)
  env_amp   : one-pole decay, τ from Decay knob  (classic range 180 ms .. 2.8 s)
  osc: y = sin(2π · phase),  phase += f/SR
  out = (y · env_amp + click) · accent_gain
Parity notes: the tune knob maps logarithmically across roughly −7..+7 semitones of f_start; the decay knob's longest setting (~2.5 s tail) is where "808 boom" lives; and the sine has a subtle 2nd harmonic when accent hits (model as y + 0.08·y·|y| — cheap and audibly right). Self-oscillation tail: when decay is at max, the pitch env's f_end floor keeps the tail from going subsonic mud below ~35 Hz — clamp f to 35 Hz.
1.2 Snare (SD)
The classic recipe: two band-passed triangle oscillators + noise through a bandpass, summed.
plain
Osc part:  two sines/triangles at ~185 Hz and ~330 Hz (both tuneable ±10% via Tune)
           through a bandpass ~1.8 kHz, Q≈1
Noise part: white noise → bandpass ~1.8 kHz, Q≈1.4


Envelope: common one-pole, τ from Decay knob (25 ms .. 400 ms)
Mix:      60% osc + 40% noise at trigger, noise share rises as envelope decays
          (the "snap" tail — model as mix_noise = 0.4 + 0.6·(1−env))
Tone knob: crossfades noise bandpass 1.4 kHz ↔ 2.3 kHz (the "snare wire" brightness)
1.3 Toms (LT/MT/HT) & Congas (LC/MC/HC)
One shared recipe, different fixed oscillator frequencies and envelope ranges. Toms are triangle-ish (slightly rounded square works at these pitches), congas are more sine-like with a faster pitch drop.
plain
Tom:   f_start → f_end sweep (sweep ratio ≈ 1.6:1, τ_pitch ≈ 45 ms)
       LT ≈ 75→50 Hz,  MT ≈ 115→75 Hz,  HT ≈ 155→100 Hz
Conga: tighter, punchier: sweep ratio 1.35:1, τ_pitch ≈ 30 ms
       LC ≈ 200 Hz, MC ≈ 250 Hz, HC ≈ 310 Hz
Envelope: τ from Decay knob (80 ms .. 1.1 s toms; 60 ms .. 500 ms congas)
Tune knob: ±1 octave on f_start
1.4 Rimshot (RS) & Clap (CP)
Rimshot: a very short (≈5 ms) band-passed pulse — a sine at ~1.7 kHz through BP 1.7 kHz Q≈8, env τ ≈ 2 ms — plus a click of noise. No knobs on the 808; ReBirth's mod layer can adjust level only.
Clap: the famous "handclap = multiple band-passed noise bursts." Render three noise bursts through a bandpass ~1.1 kHz, Q≈2.5, 9 ms apart, each burst 2 ms long, then a longer fourth decay (τ ≈ 120 ms) — the last burst carries the tail. This 3-tap + tail structure is what separates an 808 clap from generic noise.
1.5 Hi-hats (CH/OH) — six detuned squares
The 808 hat is six square-wave oscillators (metallic cluster ~f, 1.48f, 2.26f, 2.92f, 3.94f, 5.3f where f ≈ 826 Hz) summed, then through a high-pass filter at ~7 kHz. Open hat additionally has a longer envelope and a touch of the sum leaking through a low-passed path.
plain
y = Σ sqr(2π·f_k·phase)            six fixed ratios above
y → one-pole HP (7 kHz) → env
CH: env τ ≈ 35 ms
OH: env τ from Decay knob (200 ms .. 1.2 s); add 10% unfiltered bleed
Tune: shifts all six ratios together, ±30%
Accent on hats: opens the HP slightly (the 808's hats get brighter) — HP freq ×1.15 on accent
1.6 Cymbal (CY) & Cowbell (CB)
Cymbal: like the hats but ~5× longer envelope (τ ≈ 1.6 s), and the oscillator cluster is the same six-square recipe at a different base (f ≈ 340 Hz) — this shared "metallic generator" is a real 808 hardware economy; reproduce it, it explains why 808 hats and cymbals sound related.
Cowbell: two square waves (≈540 Hz and ≈800 Hz) through a bandpass ~800 Hz, Q≈4, env τ ≈ 80 ms. The slightly detuned, ringing squares are instantly recognizable.
1.7 Accent & voice count
The 808 section is 11 voices (BD, SD, LT, MT, HT, RS, CP, CH, OH, CY, CB) + congas (3) = 14 classic; ReBirth v2.0's panel grouped congas with toms. All are cheap (< ~10 ops/sample) — the whole section costs less than one 303 filter. Render all voices every buffer into the section bus, then section-level accent and the mixer strip apply.
2. Transport & Sequencer Scheduler
This is the invisible machinery that makes slides land, shuffles swing, the PCF breathe in time, and knob tweaks record — without drift. One clock, everything derived.
2.1 The master clock
A single 64-bit sample counter is the only source of truth. Everything else — ticks, beats, 16ths, pattern steps — is a pure function of it.
plain
PPQ            = 96 ticks per quarter note (fixed, matches RBNG format)
ticks          = samples · PPQ · BPM / (60 · SR)          — inverted as needed
beat_pos       = ticks / PPQ                               (float, unbounded)
bar_pos        = beat_pos / 4
Tempo comes from the TMAP chunk interpolated at tick resolution; the scheduler converts ticks→samples with a running float accumulator and never recomputes from wall time, so shuffle, swing, and tempo ramps can't drift: every event's sample position is computed once, deterministically.
2.2 Event pipeline
plain
 tick position (float)
    │
    ▼
 ┌──────────────┐   per section   ┌───────────────┐
 │ SONG arranger │ ──────────────► │ PATTERN layer  │  (song mode: resolves which
 │ (which pattern│                 │ (which steps   │   pattern each section plays
 │  at which tick)│                 │  fire at tick) │   at each song step)
 └──────────────┘                 └───────┬───────┘
                                          │ step events (note, accent, slide, flam)
                                          ▼
                              ┌───────────────────────┐
                              │ TIMING/SHUFFLE stage   │  (per-303 shuffle offsets,
                              │ resolves sample-exact  │   flam delay, slide legato)
                              │ trigger times          │
                              └───────────┬───────────┘
                                          │ timestamped events
                    ┌─────────────────────┼─────────────────────┐
                    ▼                     ▼                     ▼
            ┌──────────────┐      ┌──────────────┐      ┌──────────────┐
            │ 303 voices    │      │ 808/909      │      │ AUTOMATION   │
            │ (slide/accent │      │ triggers     │      │ (knob events,│
            │  semantics)   │      │              │      │  MIDI out)   │
            └──────────────┘      └──────────────┘      └──────────────┘
                                          │
                                          ▼
                              ┌───────────────────────┐
                              │ PCF pattern clock      │  (beat_pos → 54-pattern index,
                              │ (free-running, NOT     │   independent of pattern
                              │  note-triggered)       │   retriggers)
                              └───────────────────────┘
2.3 303 note semantics — the tricky part
The scheduler resolves raw pattern steps into voice control:
Rest with slide flag (v2.0 style): the previous note keeps sounding, pitch slews (glide). Scheduler emits NOTE_CONTINUE; the voice keeps gate high.
New note, no slide: hard retrig — envelopes reset, pitch jumps.
New note with slide: legato — pitch slews to new note, envelopes do not retrigger (the acid "sweep into the next note").
Accent: sets the accent envelope amplitude for that note (or for the continuation).
Shuffle applies only to the trigger time of even 16ths (see §3.2 of the song-format dive): slide durations consequently stretch/shrink — compute slide as a rate (τ_slide fixed), not a fixed duration, so shuffled slides still land.
Flams (909, accent-2 steps): emit a second trigger event at t + 30..40 ms (voice-specific constant from the mod table) with amp ×0.75 — the scheduler emits it as a separate timestamped event, keeping the voice render code trivial.
2.4 The audio-thread boundary
The scheduler runs on the render thread (it must — sample-exact timing), consuming an immutable snapshot of the song:
c
struct SeqSnapshot {
    /* built by GUI thread under AuLock(), consumed lock-free by render thread */
    const struct RBNGSong *song;
    int64_t start_sample;       /* transport start in master clock */
    uint32_t loop_points;       /* song-mode loop region */
    float bpm;
    /* precomputed: sample→tick table for current tempo map (fixed BPM → constant) */
};
Each render callback (64–256 frames):
Compute tick range [tick_begin, tick_end) for this buffer.
Walk each section's event list for events in range (events are pre-sorted per section at snapshot build).
For each event: compute sample offset within buffer, apply shuffle, emit to voice.
Update beat_pos for PCF and automation.
Render voices into bus.
Zero allocation, zero locking, bounded work per buffer. Worst case: all 16 steps of all sections fire in one buffer — pre-allocate an event scratch pool per render thread (16 steps × 4 sections + flams + automation ≈ 100 events — trivial).
2.5 Recording path (knob tweaks, MIDI in)
GUI/MIDI → audio (live tweaking): knob changes go through AuSetBusAttr lock-free snapshots; they apply at the next buffer boundary (≈1–2 ms — feels instant).
Audio → song (recording tweaks): the automation recorder runs on the GUI thread: it polls the current knob state at 30 Hz during RECORD, quantizes to ppq/24, and writes AUTO events into the in-memory song. Why GUI thread: song data is GUI-owned; render thread never writes song structures.
MIDI in: CAMD delivers events on its own task; a small bridge task timestamps them against the master clock (via AuQueryAttr(AUQA_MasterClock)) and injects them into the same snapshot pipeline as pattern events — so MIDI-played slides sound identical to sequencer-played ones.
2.6 Transport edge cases (the polish that makes it feel like ReBirth)
Pattern/song switch mid-bar: finishes current pattern instance, switches at the next 16th boundary — matches v2.0's "no mid-pattern cutoffs."
Loop region in song mode: loop points quantized to pattern steps; the scheduler pre-wraps the event cursor (no re-seek, no click).
Tempo change while playing: new SeqSnapshot with updated tempo map; sample→tick conversion continues from the exact sample where the change applied — no audible jump, no drift.
WAV export: identical snapshot, AuRenderToFile feeds the same scheduler — export is bit-identical to live playback (the CI round-trip guarantee from the song-format dive).
2.7 Determinism guarantee
Given: same RBNG file + same mod + same engine version ⇒ same PCM, always. The only nondeterminism allowed in the whole engine is the power-mode extras (which are opt-in per song flag in VERS flags). Classic mode is fully deterministic — including the PCF's fixed-seed "random" patterns.
The spec is now complete
Every subsystem is fully specified end-to-end:
Table
Layer	Specified
DSP	303 ladder+accent/slide · 909 layered sampler+flam quirks · 808 all 14 voices · PCF 54-pattern table · delay/dist/compressor FX ranges
Engine	audio.library API · SMP render contract · scheduler + master clock · automation/MIDI paths
Formats	RBNM mod · RBNG song · MIDI-map files · control ID ABI shared across GUI/automation/MIDI/skins
Integration	AROS conventions (Zune GUI, IFF datatypes, ARexx, catalogs, AmigaGuide)
Recommended build order for implementation, given these specs: master clock + scheduler skeleton → 303 voice (M2.2 proof point) → audio.library glue → 808 → 909 → PCF → FX → GUI → formats, because the scheduler and the 303 are the two highest-risk items and everything else hangs off them.
