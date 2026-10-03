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
};

void ri_engine_init(struct RIEngine *e);
void ri_engine_defaults(struct RIEngine *e); /* first-light knob defaults */
void ri_engine_load(struct RIEngine *e, const struct RIEvent *ev,
    uint32_t nev, uint64_t total, uint32_t sections);
void ri_engine_apply_event(struct RIEngine *e, const struct RIEvent *ev);
/* Render up to n frames of stereo. Returns frames rendered (0 at end).
 * Chunk-agnostic: splitting n renders sample-identical output. */
void ri_engine_set_clock(struct RIEngine *e, uint64_t (*now_us)(void));
const struct RIEngineStages *ri_engine_stages(const struct RIEngine *e);

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
