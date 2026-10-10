/* engine.h — the one renderer (spec §5; §12.3 skeleton, 303s first).
 * Event walker + section voices + stereo buses behind one call, shared by
 * the CLI/export sinks and the live backend. Sections render only when
 * enabled; 808/909 have no voices yet (events targeting them are
 * ignored, never misrouted).
 * Render contract (§4): no allocation, no IO, bounded loops; the event
 * list is caller-owned (pointer + count) and MUST be sample-sorted (all
 * producers sort; unsorted input misbehaves exactly like the legacy
 * walkers did — the contract is unchanged, not new).
 * Pan law: linear wings with an exact centre detent (knob 64 ->
 * gL = gR = 1.0), so a 303A-only mono fold (L+R)/2 is bit-identical
 * with the legacy mono path. Per-section pan + sends + insert routing
 * (§12.8) live here; D1 holds by construction
 * (fixed voice order, f64 master with a single final f32 rounding).
 */
#ifndef RI_ENGINE_H
#define RI_ENGINE_H
#include <stdint.h>
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/levi.h"
#include "engine/fx/fx.h"
#include "engine/fx/route.h"
#include "engine/mixer/mixer.h"

#define RI_ENGINE_BLOCK 64u
#define RI_ENGINE_S303A 0x01u
#define RI_ENGINE_S303B 0x02u
#define RI_ENGINE_S808 0x04u
#define RI_ENGINE_S909 0x08u
#define RI_ENGINE_SLEVI 0x10u /* 5th instance (owner 2026-09-28, option A) */
#define RI_ENGINE_PAN_CENTER 64u /* detent: exact unity (gL = gR = 1) */
#define RI_ENGINE_TEMPO_DEFAULT 140.0f /* delay clock until transport owns it */
/* Live-tap units for ri_engine_fx_peak (C2): DIST/PCF render per-section
 * when inserted, DELAY renders when a line is attached, COMP per-section
 * or on the master. */
#define RI_ENGINE_FX_DIST 0u
#define RI_ENGINE_FX_PCF 1u
#define RI_ENGINE_FX_DELAY 2u
#define RI_ENGINE_FX_COMP 3u
#define RI_ENGINE_FX_COUNT 4u

/* DSP sub-stage breakdown (Dell 2026-10-02). ri_live_render's DSP stage turned
 * out to be 98 % of the whole render, so the next question is what inside it;
 * and when that was answered the answer was "the voices", so the five section
 * engines are timed separately rather than as one bucket.
 *
 * Counted PER BLOCK, not per buffer: the block loop runs RI_ENGINE_BLOCK
 * samples at a time and a 256-frame buffer is several blocks, so a per-buffer
 * reading would be ambiguous.
 *
 * The five section stages are CONDITIONAL on e->sections, so their n[] is the
 * number of blocks in which that section was enabled -- not necessarily the
 * block count. The six unconditional stages and TOTAL always run once per block.
 * RI_ENGINE_ST_LEAF_FIRST..LEAF_LAST bracket the conditional ones and the
 * RI_ENGINE_ST_ALWAYS ones are outside that span, so a caller can tell the two
 * apart without hard-coding a list, exactly as the heartbeat needs to.
 *
 * now_us is injected and NULL by default, for the same reason as the session
 * stages: no OS call in the engine, and no cost to an offline caller. */
#define RI_ENGINE_ST_ZERO   0u  /* clear ml/mr/sendbus */
#define RI_ENGINE_ST_DELAY  1u  /* shared delay send, return and pan */
#define RI_ENGINE_ST_COMP   2u  /* master compressor, stereo-linked */
#define RI_ENGINE_ST_MASTER 3u  /* master fader ramp */
#define RI_ENGINE_ST_METER  4u  /* master meter feed */
#define RI_ENGINE_ST_LIMIT  5u  /* soft limiter and the output write */
#define RI_ENGINE_ST_S303A  6u  /* section 1: TB-303 A, rb303 + engine_section */
#define RI_ENGINE_ST_S303B  7u  /* section 2: TB-303 B */
#define RI_ENGINE_ST_S808   8u  /* section 3: sampler drums */
#define RI_ENGINE_ST_S909   9u  /* section 4: PCM drums */
#define RI_ENGINE_ST_SLEVI  10u /* section 5: the Levi synth, polyphonic */
/* Levi sub-stages, inside SLEVI: a third level of nesting, so SLEVI must keep a
 * timestamp of its own -- the wrapper/leaf trap t156 caught twice already.
 * Numbered BEFORE TOTAL so TOTAL stays last: it is the outermost wrapper, and
 * an index layout that puts the enclosure in the middle is one every caller
 * has to special-case. LEVMIX leads the group out because it is what writes the
 * section bus, so it is the only one the others' results depend on. */
#define RI_ENGINE_ST_ARPA    11u /* levi_arp_block: arpeggiator step advance */
#define RI_ENGINE_ST_LEVSEQ  12u /* levi_seq_block: the device's own sequencer */
#define RI_ENGINE_ST_LEVVOICE 13u /* levi_voice_render_sum_stereo: the voices */
#define RI_ENGINE_ST_LEVMIX  14u /* engine_section_stereo: sum onto the buses */
/* LEVTEMPO and LEVPROBE added 2026-10-03, for the measurement that found 54 % of
 * SLEVI's cost unattributed by the four above (LEVI 57 us against arp+seq+voice
 * +mix = 26 us on riqemu1). Two stages, two different jobs:
 *
 *   LEVTEMPO wraps levi_set_tempo, which is the ONLY call inside SLEVI that no
 *   other sub-stage covers. Inspection says it is trivial -- three float
 *   compares and a store -- which is exactly why it needs a number rather than
 *   an argument: a trivial function that turns out to cost real time is a
 *   finding, and one that reads at the floor closes the question.
 *
 *   LEVPROBE is a deliberate CONTROL: an open/close pair with nothing between
 *   them. Its reading IS the cost of one stage pair, so subtracting it from any
 *   other sub-stage separates "this stage is cheap" from "this stage cannot be
 *   measured". It sits OUTSIDE SLEVI's own interval on purpose: measuring the
 *   instrument inside the region it measures would inflate SLEVI and make the
 *   old and new SLEVI numbers incomparable.
 *
 * LEVPROBE is therefore NOT inside SLEVI despite being in the LEVI sub-span;
 * the sub-span's invariant is about ordering (SUB_FIRST..SUB_LAST), and a test
 * that needs it to be literally nested should say so rather than infer it. */
#define RI_ENGINE_ST_LEVTEMPO 15u /* levi_set_tempo: the one uncovered SLEVI call */
#define RI_ENGINE_ST_LEVPROBE 16u /* CONTROL: empty T/E pair OUTSIDE SLEVI */
#define RI_ENGINE_ST_LEVPROBE2 17u /* CONTROL: empty T/E pair INSIDE SLEVI */
#define RI_ENGINE_ST_TOTAL  18u /* one whole block; ENCLOSES every stage above */
#define RI_ENGINE_ST_COUNT  19u
/* The LEVI sub-span: these run only when SLEVI does, and only inside it. It
 * starts immediately after the top-level stages end, which is the invariant
 * the test pins rather than a magic constant. */
#define RI_ENGINE_ST_SUB_FIRST RI_ENGINE_ST_ARPA
#define RI_ENGINE_ST_SUB_LAST  RI_ENGINE_ST_LEVPROBE2
/* The conditional span: stages in here run only when their section is enabled. */
#define RI_ENGINE_ST_LEAF_FIRST RI_ENGINE_ST_S303A
#define RI_ENGINE_ST_LEAF_LAST  RI_ENGINE_ST_SLEVI
/* How many stages are unconditional, i.e. run once per block, always. */
#define RI_ENGINE_ST_ALWAYS 6u

struct RIEngineStages {
    uint64_t sum_us[RI_ENGINE_ST_COUNT];
    uint32_t max_us[RI_ENGINE_ST_COUNT];
    uint32_t n[RI_ENGINE_ST_COUNT];
};

/* Drum-tail A0 paired samples: one (stage us, active voice-samples) pair
 * per sampled block, for the 808 and the 909. The us halves come from the
 * section stages' own clock pairs (RI_ESTAGE_E_STORE into drum_last808_us /
 * drum_last909_us — no extra reads over what the stages already pay); the
 * active halves are the vc_voice_active counters. ri_engine_drum_sample
 * keeps every 256th full 64-sample block, then stops at 512: 512 samples
 * span 512 x 256 x 64 samples (~175 s at 48 kHz), enough for a whole song.
 * Partial (event-split) slices are not blocks and never tick the stride.
 * Recording needs an injected clock (live runs have one); with now_us NULL
 * nothing is recorded. Plain data, no IO: the app dumps the ring at close. */
#define RI_DRUMDIAG_N 512u
#define RI_DRUMDIAG_STRIDE 256u
struct RIDrumDiag {
    uint32_t us808, a808, us909, a909;
};

/* ---------------------------------------------------------------------
 * The note tap (R6d).
 *
 * THE ENGINE HAS NO IDEA A WIRE EXISTS. It records notes into this ring and
 * `app/` drains it; it includes nothing from midi_io, names no channel and
 * sends nothing itself. That is what keeps the render's audio path free of
 * camd and keeps the confinement gate (no AROS outside platform/aros) honest.
 *
 * THE TAP RECORDS THE RESOLVED SOUND, NEVER THE LANE. The 808's lane->sound
 * map is `e->s808.slot[lane]` and the user can remap it; the 909's is the
 * static `RI_LANE_TO_RB909_VOICE`. A tap that recorded the lane would hand
 * the caller a number it cannot interpret without re-implementing the
 * engine's own resolution -- and the app's copy of that resolution would be
 * wrong the moment a lane moved.
 *
 * `ri_engine_load` MUST NOT CLEAR THIS. The live session reloads the engine
 * every 256-frame buffer (the drum-ring A0 note above), so a reset in load
 * would cap the ring at one block's notes. t199 pins that.
 * ------------------------------------------------------------------- */
#define RI_NOTETAP_CAP 256u

/* A note, or a total accent that arrived after the note-on it accents. */
#define RI_NOTEK_NOTE         0u
#define RI_NOTEK_LATE_ACCENT  1u

struct RINoteTapRec {
    uint64_t sample;   /* the cursor the note landed on */
    uint8_t  kind;     /* RI_NOTEK_* */
    uint8_t  device;   /* 0/1 303A/303B, 2 808, 3 909, 4 Levi */
    uint8_t  sound;    /* RESOLVED sound; meaningless for a melodic note */
    uint8_t  note;     /* the engine's own MIDI note, where it has one */
    uint8_t  flags;    /* RI_EVFLAG_* (low 8) */
    uint8_t  is_off;
    uint8_t  pad;
};

struct RINoteTap {
    struct RINoteTapRec rec[RI_NOTETAP_CAP];
    uint32_t head, tail, dropped;
};

struct RIEngine {
    struct RIEngineStages estg;
    uint64_t (*now_us)(void); /* injected clock; NULL disables stage timing */
    struct RB303Voice v303a, v303b;
    struct RB808Set s808; /* §12.7a/m64: drum sections */
    struct RB909Set s909;
    struct RILeviSet slevi; /* 5th instance (polyphonic synth) */
    uint64_t tag808[RI_808_NSOUNDS]; /* trigger sample per sound */
    uint64_t tag909[RI_909_NVOICES]; /* trigger sample per voice */
    const struct RIEvent *ev; /* caller-owned, sample-sorted */
    uint32_t nev, evpos;
    uint64_t cursor, total;
    uint32_t sections; /* RI_ENGINE_S* bits */
    float scratch[RI_ENGINE_BLOCK]; /* section bus: storage lives here */
    float scratchR[RI_ENGINE_BLOCK]; /* Levi stereo right bus (P6c) */
    /* Insert routing (§12.8): radio-exclusivity owners + one voice each.
     * Neutral (no owners, sends 0, pans centre, delay detached) renders
     * bit-identical with the pre-routing engine. */
    struct RIRoute route;
    struct RiFXDist dist;
    struct PCF pcf;
    uint8_t pcf_base, pcf_q, pcf_amt, pcf_mode, pcf_pattern, pcf_decay;
    struct RiFXComp comp;
    uint8_t pan[RI_ROUTE_NSECTIONS]; /* 0..127 per section */
    uint8_t send[RI_ROUTE_NSECTIONS]; /* 0..127 post-insert mono send */
    uint8_t level[RI_ROUTE_NSECTIONS]; /* strip fader 0..127, 127 = unity */
    float lvl_applied[RI_ROUTE_NSECTIONS]; /* zipless slew state */
    uint8_t master; /* S4b song-data fader 0..127, 127 = unity */
    float master_applied; /* zipless slew state */
    float tempo; /* delay clock, 20..500 BPM */
    uint8_t tr_playing; /* transport mirror for device sequencers (P8c) */
    uint8_t tr_pad[3];
    uint64_t tr_tick;   /* song position in ticks */
    uint32_t tr_ppq;    /* ticks per quarter (0 = 96) */
    float *dline; /* caller-owned delay line, NULL = dry */
    uint32_t dcap;
    struct RiFXDelay delay;
    uint8_t dret_pan; /* stereo return pan */
    /* Live taps (C2): post-insert section peaks + per-FX-unit peaks.
     * Fed during render only; never affect audio. Decay follows the
     * P-16 20 dB/s ballistics at the init rate (48 kHz default). */
    struct RiMeter sec_meter[RI_ROUTE_NSECTIONS];
    struct RiMeter fx_meter[RI_ENGINE_FX_COUNT];
    struct RiMeter master_meter[2]; /* S4: post-master L/R peaks */
    /* Drum-tail A0: last full-block stage spans (E_STORE targets) + the
     * sampled (us, active) ring drained at close. Zero-cost when no clock
     * is injected (sample returns early) and two stores per block when
     * one is — the same discipline as the vc_* counters. */
    uint32_t drum_last808_us, drum_last909_us;
    struct RIDrumDiag drum_ring[RI_DRUMDIAG_N];
    uint32_t drum_n, drum_tick;
    /* R6d note tap. Embedded, like the C2 meters: one instance in the
     * session, drained by the caller, never read by the engine. */
    struct RINoteTap notetap;
    /* t205: out-of-order events refused. Counted so a caller assembling its
     * own event array finds out rather than wondering. */
    uint32_t ev_unsorted;
    /* R8c stem tap. NULL by default; caller-owned, caller-lifetime. */
    struct RIStemTap *stems;
};

/* ---------------------------------------------------------------------
 * R8c: the stem tap on the mix the APP actually uses.
 *
 * `ri_mix_render` IS NOT IT. Its only non-test callers are `tools/bench.c`
 * and `tools/render.c`; `engine/mixer/mixer.c` is linked into both AROS
 * ABIs and into the portable build and has NO APPLICATION CALLER. The app
 * mixes in `engine_section()` (mono, sections 0-3) and
 * `engine_section_stereo()` (the Levi, section 4).
 *
 * A stem is EXACTLY WHAT THE MIX ADDS: post-fader, post-pan, pre-master,
 * taken from the same term the accumulator is summing rather than a
 * recomputation of the gain. So:
 *
 *  - **THE STEMS SUM TO THE MIX TO FLOAT ACCUMULATION PRECISION, NOT
 *    BIT-EXACTLY.** The master accumulates in `double` and these buffers are
 *    `float`. Five narrowed stems differ by about 6e-8 relative, roughly
 *    -144 dBFS -- at or below a 24-bit stem's quantisation and far below a
 *    16-bit one's. t204 pins the bound. An equality would be a law the code
 *    cannot keep.
 *  - **A NULL SLOT IS SKIPPED, NOT WRITTEN.** A holed tap array is a
 *    legitimate configuration -- a 303-only render has no 808 -- and
 *    attaching a tap must not resurrect a section the song has switched off.
 *  - **THE MONO STEMS ARE IMMUNE TO `scratchR`** (t206): `engine_section`
 *    never reads it, so a mono stem is its mono sample mirrored to both
 *    sides. An earlier slice claimed mono sections exported stale `scratchR`;
 *    that claim was withdrawn.
 * ------------------------------------------------------------------- */
struct RIStemTap {
    float *l[RI_ROUTE_NSECTIONS];
    float *r[RI_ROUTE_NSECTIONS];
};

void ri_engine_init(struct RIEngine *e);
void ri_engine_defaults(struct RIEngine *e); /* first-light knob defaults */
void ri_engine_load(struct RIEngine *e, const struct RIEvent *ev,
    uint32_t nev, uint64_t total, uint32_t sections);
void ri_engine_apply_event(struct RIEngine *e, const struct RIEvent *ev);
/* Render up to n frames of stereo. Returns frames rendered (0 at end).
 * Chunk-agnostic: splitting n renders sample-identical output. */
void ri_engine_set_clock(struct RIEngine *e, uint64_t (*now_us)(void));

/* R6d: drain the note tap, oldest first. 0 when empty, NULL engine or
 * NULL output. A partial drain leaves the rest in order. */
uint32_t ri_engine_note_read(struct RIEngine *e, struct RINoteTapRec *out,
    uint32_t cap);
uint32_t ri_engine_note_pending(const struct RIEngine *e);
uint32_t ri_engine_note_dropped(const struct RIEngine *e);
/* Explicit, once-at-startup drain. NOT called by ri_engine_load -- see
 * RI_NOTETAP_CAP above for why that would be a bug. */
void ri_notetap_reset(struct RINoteTap *t);

/* Out-of-order events seen by `ri_engine_render`, which STOPS rather than
 * rendering them (see t205). `ri_engine_load` documents its array as
 * sample-sorted; this is what happens when a caller does not honour it.
 * Without the guard `run = next - cursor` underflows and the slice loop
 * writes past the caller's output buffer. The array is NOT silently
 * repaired -- reordering it would render music the caller did not
 * describe. */
uint32_t ri_engine_ev_unsorted(const struct RIEngine *e);

/* R8c: attach (or NULL to detach) the per-section stem tap. Caller-owned and
 * caller-lifetime: the engine holds the pointer and never frees it. */
void ri_engine_set_stems(struct RIEngine *e, struct RIStemTap *t);
const struct RIEngineStages *ri_engine_stages(const struct RIEngine *e);
/* Drum-tail A0 sampler: record one (us, active) pair per 256th full block
 * (see the RIDrumDiag note above). Called once per block-slice by the
 * render loop; also directly drivable. Render-contract safe: plain stores,
 * no clock reads of its own, no IO. */
void ri_engine_drum_sample(struct RIEngine *e, uint32_t cc);
/* Start a fresh drum-tail record (fresh song). The render loop never calls
 * this: the live session reloads the engine per buffer, so only an explicit
 * per-song reset keeps one song's record. NULL-safe. */
void ri_engine_drum_reset(struct RIEngine *e);

uint32_t ri_engine_render(struct RIEngine *e, float *out_l, float *out_r,
    uint32_t n, float sr);
/* Mono sink: (L+R)/2 per sample. Centre-unity makes 303A-only mono
 * bit-identical with the legacy single-voice path. */
uint32_t ri_engine_render_mono(struct RIEngine *e, float *out, uint32_t n,
    float sr);
/* Insert routing (§12.8): radio assign (returns previous owner, -2 bad).
 * Sections 0..RI_ROUTE_NSECTIONS-1 (303A/303B/808/909/Levi); comp
 * additionally RI_ROUTE_MASTER. */
int ri_engine_assign_insert(struct RIEngine *e, uint32_t unit, int owner);
/* Knob set by FX control id (RI_FXID_*; DELAY_MIX is accepted but stays
 * wet — send topology has no dry path). Unknown ids ignored. */
void ri_engine_fx_set(struct RIEngine *e, uint32_t id, uint8_t value);
int ri_engine_set_pan(struct RIEngine *e, uint32_t section, uint8_t v);
int ri_engine_set_send(struct RIEngine *e, uint32_t section, uint8_t v);
/* Section level fader (mixer strip Level, E0 P-17 law (v/127)^2, 127 =
 * unity = the pre-fader engine, so the neutral path stays bit-identical).
 * Post-insert, pre-meter/send/pan; zipless slew over RI_MIX_RAMP_SMP.
 * Returns 0 ok, 2 bad arg. */
int ri_engine_set_level(struct RIEngine *e, uint32_t section, uint8_t v);
/* S4b master song-data fader (P-17 law, unity default = bit-identical
 * neutral path). Post-everything gain + L/R meter taps. Returns 0 ok. */
int ri_engine_set_master(struct RIEngine *e, uint8_t v);
void ri_engine_set_tempo(struct RIEngine *e, float bpm);
/* Transport mirror for device sequencers (P8c). */
void ri_engine_transport(struct RIEngine *e, uint32_t playing,
    uint64_t tick, uint32_t ppq);
/* Delay line (caller-owned, cap >= 64; NULL buf detaches = dry).
 * Returns 0 ok, 2 bad arg. Attaching syncs time + forces wet. */
int ri_engine_set_delay(struct RIEngine *e, float *buf, uint32_t cap);
/* Master/section comp gain-reduction meter in dB (<= 0). */
float ri_engine_comp_gr(const struct RIEngine *e);
/* Live taps (C2): held linear peak of a section bus (0..3 =
 * 303A/303B/808/909) or an FX unit (RI_ENGINE_FX_*). 0 before any
 * render, 0 on bad index/NULL. Render-contract safe (read-only). */
float ri_engine_section_peak(const struct RIEngine *e, uint32_t section);
float ri_engine_fx_peak(const struct RIEngine *e, uint32_t unit);
/* S4: held linear peak of the post-master bus (ch 0 = L, 1 = R). */
float ri_engine_master_peak(const struct RIEngine *e, uint32_t ch);
/* Bind 909 sample layers (non-owning, idle-swap; the pack loader owns
 * the data). Unbound voices render silence. Returns 0 ok, 2 bad. */
int ri_engine_909_bind(struct RIEngine *e, uint32_t voice,
    const struct RISampleLayer *layers, uint32_t n);
#endif
