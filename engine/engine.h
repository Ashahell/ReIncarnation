/* engine.h — the one renderer (spec §5; §12.3 skeleton, 303s first).
 * Event walker + section voices + stereo buses behind one call, shared by
 * the CLI/export sinks and the live backend. Sections render only when
 * enabled; 808/909/FX/mixer bits are reserved for later slices (events
 * targeting them are ignored, never misrouted).
 * Render contract (§4): no allocation, no IO, bounded loops; the event
 * list is caller-owned (pointer + count) and MUST be sample-sorted (all
 * producers sort; unsorted input misbehaves exactly like the legacy
 * walkers did — the contract is unchanged, not new).
 * Pan law (skeleton): every section centre, unity (gL = gR = 1.0), so a
 * 303A-only mono fold (L+R)/2 is bit-identical with the legacy mono path.
 * Per-section pan arrives with the mixer slice; D1 holds by construction
 * (fixed voice order, f64 master with a single final f32 rounding).
 */
#ifndef RI_ENGINE_H
#define RI_ENGINE_H
#include <stdint.h>
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"

#define RI_ENGINE_BLOCK 64u
#define RI_ENGINE_S303A 0x01u
#define RI_ENGINE_S303B 0x02u
/* Reserved: RI_ENGINE_S808 0x04u, RI_ENGINE_S909 0x08u, inserts/mixer later. */

struct RIEngine {
    struct RB303Voice v303a, v303b;
    const struct RIEvent *ev; /* caller-owned, sample-sorted */
    uint32_t nev, evpos;
    uint64_t cursor, total;
    uint32_t sections; /* RI_ENGINE_S* bits */
    float scratch[RI_ENGINE_BLOCK]; /* section bus: storage lives here */
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
#endif
