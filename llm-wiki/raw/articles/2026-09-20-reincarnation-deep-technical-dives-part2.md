# ReIncarnation — Deep Technical Dives, Part 2

**Ingested:** 2026-09-20 into ReIncarnation `llm-wiki`
**Source:** user-provided deep-dive part 2 text, stored verbatim below (only this header added).
**Status:** design input — companion to the Comprehensive Plan and Part 1 dives.

---

ReIncarnation — Deep Technical Dives, Part 21. The PCF (Pattern Controlled Filter)
The PCF is the most under-documented classic feature — a 12 dB/oct resonant filter (LP/BP) driven by 54 fixed envelope patterns, and it single-handedly gave ReBirth its "live filter sweeps" identity. Getting the pattern table right is a parity-critical, pure data problem.
1.1 Signal path
plain
bus audio ──► [12 dB SVF, LP or BP mode] ──► out
                    ▲ cutoff
                    │
             base_cutoff + env_pattern[i] · env_amt
                    ▲
             i = (song_position_beats · pattern_length) mod 54
The envelope is not retriggered per note — it free-runs against the song timeline. That's the crucial behavior: the PCF breathes on its own clock, and playing different patterns against the same PCF pattern gives you the classic "the filter is playing a melody against the bassline" effect.
1.2 The filter core
A state-variable filter (Chamberlin form) gives us both LP and BP from one structure, matches the 12 dB/oct spec, and is cheap enough to run on every bus:
plain
// per sample:
low  += f · band
high  = x − low − q·band
band += f · high
out   = (mode == LP) ? low : band


f = 2·sin(π·fc/SR)   (Chamberlin; stable for fc < ~SR/6 — oversample 2× in power mode)
q = 1/Q, Q from Resonance knob (classic range ~0.7..8)
ReBirth's PCF resonance gets nasty near max — that's partly the SVF's q·band term pushing high negative and partly intentional saturation. Keep a tanh clipper on the input (same family as the 303 ladder) so max-resonance sweeps bark instead of going silent.
1.3 The 54 patterns — data table, not code
Each pattern is 16 steps of an 8-bit value (0–127, 64 = neutral/center). 54 patterns × 16 steps × 1 byte = 864 bytes — a single const table in the binary, PCFPatterns[54][16]. The shapes, reconstructed from RB-338 v2.0.1's documented behavior and community reverse-engineering, fall into families:
Table
Family	Steps	Count	Behavior
Saw up	16	~6	linear 0→127 over the bar; variants with rests (0s) or plateau
Saw down	16	~6	mirror
Triangle	16	~8	up-down ramps of various slopes and phase offsets
Square/pulse	16	~6	hold low, jump high, hold; some inverted; some with one-step blips
Envelopes	16	~10	fast attack → exp-ish decay; the "pluck" family
Random-walk	16	~8	seeded pseudo-random steps within a band — ReBirth used a fixed PRNG seed per pattern (deterministic!), so pattern #37 always plays the same "random" line
Triggered	16	~10	mostly 0 with single accents on steps 1/5/9/13 etc. — the "stutter" family
Two hard parity rules:
Determinism. Same song position ⇒ same PCF value, always. No real-time randomness; the "random" patterns use a fixed LFSR seeded at song start. (This matters for WAV export to be bit-identical to live playback.)
Step timing. Pattern steps advance on 16th-note grid, quantized to the transport clock — not per audio buffer. The engine computes i from the transport's exact beat position (float) so pattern values interpolate only at step boundaries, never within.
1.4 Amplitude and response curve
Raw table values don't map linearly to cutoff:
plain
fc = base_fc · 2^( (value − 64)/64 · env_amt_octaves )
   env_amt_octaves = knob, range ±4 octaves
Neutral value 64 ⇒ fc = base_fc (no sweep). The 2^ curve is mandatory — linear Hz mapping sounds mechanical; the exponential mapping is what makes slow saw-up patterns "open like a filter on a synth."
Plus ENV modulation via accent: in v2.0, the PCF's envelope could optionally take the 303 accent bus as an extra trigger — implement as value = table[i] + accent_bus · accent_amt clamped 0..127.
1.5 API surface
c
/* engine/fx/pcf.h */
struct PCF {
    struct SVF svf;
    uint8_t pattern;        /* 0..53 */
    uint8_t mode;           /* LP | BP */
    float base_fc, q, env_amt, accent_amt;
    float beat_pos;         /* fed from transport each buffer */
};


void pcf_render(struct PCF *p, float *in, float *out, uint32_t n, float sample_rate);
uint32_t pcf_pattern_count(void);           /* 54 — skins/mods can't add these */
const uint8_t *pcf_pattern_table(void);     /* exposed for the GUI pattern picker */
The GUI picker is a 54-tile grid showing mini-waveforms of each pattern — reproduce the original's "select by thumbnail" feel.
2. The TR-909 Multi-Layer Sampler Voice
The 909 is sample-based, but the tune knobs are not pitch shifters — they crossfade between recorded layers of the same drum hit at different knob positions. This is the entire reason 909 samples can't be replaced by one-shots.
2.1 Voice structure
c
/* engine/dsp/rb909.h */
#define RB909_MAX_LAYERS 4


struct SampleLayer {
    const float *data;      /* interleaved or mono, engine-owned, never freed mid-note */
    uint32_t frames;
    float rate;             /* original sample rate */
    uint8_t lo, hi;         /* tune-knob position range this layer covers, 0..127 */
};


struct RB909Voice {
    struct SampleLayer layers[RB909_MAX_LAYERS];
    uint8_t layer_count;
    uint8_t accent_capable;     /* BD, SN, HT/LT/MT, OH/CH, RS, CP: yes; CY/RD: tune only */
    uint8_t flam_capable;       /* SN, HT/LT/MT, RS: v2.0 flams */
    float gain, tune;           /* 0..127 knob positions, not Hz */
    float decay;                /* per-voice decay override (BD/SN/HT/LT/MT) */
    float attack;               /* BD only */
    float tone, snap;           /* SN only */
    /* --- runtime, render thread only --- */
    float pos, cur_rate;
    uint8_t cur_layer;
    float amp_env;
    float accent_level;
};
2.2 The layering model
Knob position 0–127 selects layers by range; within a layer, a fine crossfade to the next layer over ~8 knob positions prevents zipper noise:
plain
layer_select(tune):
    for each layer where lo ≤ tune ≤ hi:
        weight = triangular_window(tune, layer)   // peaks at layer center
    normalize weights, sum of weights ≤ 1 (rest = silence)
At render time, all active layers are summed with linear interpolation resampling:
plain
sample = lerp(data[⌊pos⌋], data[⌊pos⌋+1], frac(pos)) · layer_weight
pos   += layer.rate · cur_rate_mult / sample_rate
cur_rate_mult is 1.0 for almost everything — except: the original 909 voices are not tuned to concert pitch on the sample; the tune knob's "middle" is the sample's natural rate. In classic mode we reproduce the exact v2.0 offsets (documented per voice — BD natural ≈ +0 semitones relative to sample, hats sit where they sit; power mode can expose true pitch control).
2.3 Accent — the 909's double dynamic
v2.0 gave the 909 two accent levels (the dim/green LED pair on the step buttons). Behavior:
No accent: layer set + amp 1.0
Accent 1: switch to accent-layer set if present (mods supply them), amp ×1.15, tiny lowpass open (≈ +1 kHz shelf) — the hardware does this via an analog accent path
Accent 2 (flam): two hits ~30–40 ms apart, second hit at ×0.75 amp — only on flam-capable voices; on others it equals accent 1
The amp/timing constants go in a per-voice struct so mods can voice their 909 differently (within limits — the format's S909 chunk maps directly onto this struct).
2.4 Per-voice knob semantics (parity table)
Table
Voice	Knobs	Notes
Bass Drum	Tune, Attack, Decay	Tune = layer crossfade + subtle pitch; decay up to ~2.5× sample length
Snare	Tune, Tone, Snap, Decay	Tone = layer select (head vs. rim layers); Snap = noise-bleed layer gain
Hi/Mid/Low Tom	Tune, Decay	Straight layer fades
Rim / Clap	Tune, (Clap: retrigger count fixed at 3 handclaps)	Clap decay fixed; Tune fades the clap-noise layer
Closed/Open Hat	Tune, Decay (CH only)	OH ignores decay (fixed length); hats are the classic 6-square-osc through HP — sampled here but layered
Crash / Ride	Tune	No accent (accent button does nothing — reproduce this quirk, v2.0 players know it)
2.5 Render contract
c
void rb909_render(struct RB909Voice *v, const struct RBStepEvent *ev,
                  float *out, uint32_t n, float sample_rate);
One voice instance per drum sound per section (the 909 plays monophonically per voice — retrigger cuts the previous hit; the clap's internal 3-hand retrigger is inside the sample/layer, not the voice). Memory rule: sample data lives in the mod, owned by the skin/sound manager; voices hold pointers only, and mods can only be swapped when all voices are idle (audible click guard).
3. The Song File Format — FORM RBNG
3.1 Design principles
IFF, big-endian, like the mod format — Wanderer thumbnails via datatype for free.
Chunked and forward-compatible: unknown chunks are skipped (version stamp decides).
Deterministic re-render: everything time-based is stored in ticks/16ths, never wall-clock; reopening a song renders bit-identically (PCF PRNG seeded from the file).
Self-contained: embeds the mod reference (name + hash), not the mod itself; embeds tempo map, automation, knob tweaks.
plain
FORM RBNG                        /* Reincarnation Song */
├── VERS  { UWORD format=1, UWORD revision, ULONG engine_min }
├── NAME  { song title }
├── AUTH  { author }
├── TMAP  { tempo map:
│            UWORD ppq (=96, fixed), ULONG tempo_events,
│            per event: ULONG abs_tick, UWORD bpm_x100, UBYTE flags (ramp|jump) }
│          /* v2.0.1 had fixed BPM + shuffle; the map exists so power mode
│             can do tempo ramps without a new format version */
├── SECT  { per-section chunks: ID, name, UBYTE device_type (303A|303B|808|909) }
├── PATT  { pattern storage, per section:
│            UWORD count (≤32 classic), UBYTE steps (≤16),
│            per pattern: UWORD length, then per step:
│            { UBYTE note (0xFF=rest, else semitone from C-2), UBYTE flags
│              (bit0 accent, bit1 slide, bit2 accent2/flam, bit3-5 octave,
│               bit6-7 reserved), UBYTE velocity (0=default) } }
├── SONG  { arrangement: ULONG steps, per step: UBYTE sect_idx[4],
│            UBYTE patt_idx[4], UBYTE flags (loop_marker, mute_mask) }
├── AUTO  { automation: per section, per knob: UWORD event_count,
│            per event: ULONG abs_tick, UBYTE knob_id, UBYTE value 0..127 }
│          /* knob-tweak-into-song recording — quantized to ppq/24 */
├── PCFP  { UBYTE pcf_pattern[4], UBYTE pcf_mode[4], UBYTE pcf_env_amt[4] }
├── MIXR  { per-bus: UBYTE fader, pan, mute, solo; master: UBYTE fader,
│            FX slot order, per-FX params (delay time, fb, distortion shape...) }
├── MODR  { STRPTR mod_name, ULONG mod_crc32, UWORD mod_vers }
│          /* on load: warn "song was made with mod X — load it? [search]" */
├── SHFL  { per-303: UBYTE shuffle 0..75% in 1% steps (v2.0 feature) }
├── CPRG  { STRPTR attribution (user-entered) }
└── (future chunks: SMTC for SMPTE offsets, LYR for lyric/vocal markers...)
3.2 The shuffle quirk (must be exact)
v2.0's per-303 shuffle delays even-numbered 16ths by a percentage of the 16th duration. ReBirth applied it only to the 303 sections and quantized it to the audio buffer in a way that produced a subtle "shuffle pump." Implement it as a tick-level offset in the sequencer's event scheduler (not in the DSP), clamped to < one 16th:
plain
tick_of_even_16th += shuffle_pct · (ppq/4) / 100
Storing it per-303 in SHFL preserves the v2.0 songs where 303A shuffles 62% and 303B is straight — that contrast is a sound.
3.3 Automation — the hidden gem
v2.0 recorded knob movements during playback into the song. Storage above uses knob IDs from the same stable ID table as the skin format (MAPS) — one ABI for GUI, automation, and MIDI-learn. MIDI CC learn maps CC→knob_id, so a MIDI mapping file is just an AUTO-shaped chunk of pairs; keep them in DEVS:ReIncarnation/MIDI/ as tiny IFFs.
3.4 Round-trip guarantee (CI-enforced)
The format has one hard requirement, tested in CI: serialize → deserialize → serialize must be byte-identical, and AuRenderToFile of the same song twice must be sample-identical. Any feature that breaks either gets redesigned before merge. This is what makes the format trustworthy for a community that will still be opening these files in 2046.
3.5 What a "project" is, in AROS terms
Songs: PROGDIR:Songs/, user songs in SYS:Storage/ReIncarnation/ or anywhere.
The datatype gives a "Play" tooltype — double-click a song in Wanderer to hear it (render-to-file preview hook).
ARexx: ADDRESS REINCARNATION OPENSONG 'path', PLAY, EXPORTWAV 'path' — the whole transport is scriptable, Amiga-style.
Where this leaves the spec
All classic audio paths are now fully specified: 303 (ladder + accent/slide), 808 (from the earlier plan — synth voices), 909 (layered sampler + accent/flam quirks), PCF (54-pattern deterministic table), FX (delay/distortion/compressor parameter ranges from the earlier plan), sequencer (pattern/song/shuffle/automation), file formats (RBNM mod, RBNG song), and the audio.library contract under it all.
