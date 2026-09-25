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
#include "engine/fx/fx.h"
#include "engine/fx/route.h"

#define RI_ENGINE_BLOCK 64u
#define RI_ENGINE_S303A 0x01u
#define RI_ENGINE_S303B 0x02u
#define RI_ENGINE_S808 0x04u
#define RI_ENGINE_S909 0x08u
#define RI_ENGINE_PAN_CENTER 64u /* detent: exact unity (gL = gR = 1) */
#define RI_ENGINE_TEMPO_DEFAULT 140.0f /* delay clock until transport owns it */

struct RIEngine {
    struct RB303Voice v303a, v303b;
    struct RB808Set s808; /* §12.7a/m64: drum sections */
    struct RB909Set s909;
    uint64_t tag808[RI_808_NSOUNDS]; /* trigger sample per sound */
    uint64_t tag909[RI_909_NVOICES]; /* trigger sample per voice */
    const struct RIEvent *ev; /* caller-owned, sample-sorted */
    uint32_t nev, evpos;
    uint64_t cursor, total;
    uint32_t sections; /* RI_ENGINE_S* bits */
    float scratch[RI_ENGINE_BLOCK]; /* section bus: storage lives here */
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
    float tempo; /* delay clock, 20..500 BPM */
    float *dline; /* caller-owned delay line, NULL = dry */
    uint32_t dcap;
    struct RiFXDelay delay;
    uint8_t dret_pan; /* stereo return pan */
};

void ri_engine_init(struct RIEngine *e);
void ri_engine_defaults(struct RIEngine *e); /* first-light knob defaults */
void ri_engine_load(struct RIEngine *e, const struct RIEvent *ev,
    uint32_t nev, uint64_t total, uint32_t sections);
void ri_engine_apply_event(struct RIEngine *e, const struct RIEvent *ev);
/* Render up to n frames of stereo. Returns frames rendered (0 at end).
 * Chunk-agnostic: splitting n renders sample-identical output. */
uint32_t ri_engine_render(struct RIEngine *e, float *out_l, float *out_r,
    uint32_t n, float sr);
/* Mono sink: (L+R)/2 per sample. Centre-unity makes 303A-only mono
 * bit-identical with the legacy single-voice path. */
uint32_t ri_engine_render_mono(struct RIEngine *e, float *out, uint32_t n,
    float sr);
/* Insert routing (§12.8): radio assign (returns previous owner, -2 bad).
 * Sections 0..3 (303A/303B/808/909); comp additionally RI_ROUTE_MASTER. */
int ri_engine_assign_insert(struct RIEngine *e, uint32_t unit, int owner);
/* Knob set by FX control id (RI_FXID_*; DELAY_MIX is accepted but stays
 * wet — send topology has no dry path). Unknown ids ignored. */
void ri_engine_fx_set(struct RIEngine *e, uint32_t id, uint8_t value);
int ri_engine_set_pan(struct RIEngine *e, uint32_t section, uint8_t v);
int ri_engine_set_send(struct RIEngine *e, uint32_t section, uint8_t v);
void ri_engine_set_tempo(struct RIEngine *e, float bpm);
/* Delay line (caller-owned, cap >= 64; NULL buf detaches = dry).
 * Returns 0 ok, 2 bad arg. Attaching syncs time + forces wet. */
int ri_engine_set_delay(struct RIEngine *e, float *buf, uint32_t cap);
/* Master/section comp gain-reduction meter in dB (<= 0). */
float ri_engine_comp_gr(const struct RIEngine *e);
/* Bind 909 sample layers (non-owning, idle-swap; the pack loader owns
 * the data). Unbound voices render silence. Returns 0 ok, 2 bad. */
int ri_engine_909_bind(struct RIEngine *e, uint32_t voice,
    const struct RISampleLayer *layers, uint32_t n);
#endif
