/* autolane_emit.h — lane emission (spec 2026-09-26 §2.4).
 * Separate from autolane.h so the model never pulls scheduler/clock
 * headers (the songtrack_emit.h split precedent). */
#ifndef RI_AUTOLANE_EMIT_H
#define RI_AUTOLANE_EMIT_H
#include "engine/seq/autolane.h"
#include "engine/seq/sched.h"  /* RIEvent, RI_EV_* */
#include "engine/seq/clock.h"  /* RITempoMap */

/* Emission carry: lane index of the first event not yet emitted
 * (songtrack R1 lesson — a cap-dropped event is re-sent, never lost).
 * Reset to 0 after any lane mutation (edits shift indices). */
struct RIAutoCarry { uint32_t next; };

/* Render-safe publish (R3): double-buffered lane. The GUI thread owns
 * the back lane, the render thread reads only the front lane, and the
 * swap happens at block boundaries through a staged request:
 * - GUI: mutate ri_auto_pub_back(), then ri_auto_pub_request();
 * - render: ri_auto_pub_apply() at each block start, then read
 *   ri_auto_pub_front();
 * - GUI (off the render thread): ri_auto_pub_resync() copies the new
 *   front over the back before the next mutation batch.
 * Publish rate law: at most one request per render block (a second
 * request overwrites the staged one — fail-soft, never torn).
 * Single-writer per side (sequencer snapshot contract): GUI writes back+request,
 * render writes front+apply-clear. Host-testable as pure calls. */
struct RIAutoPub {
    struct RIAutoLane lanes[2];
    volatile uint32_t front;  /* 0/1: render side reads lanes[front] */
    volatile uint32_t staged; /* 0/1 staged back, or RI_AUTO_PUB_NONE */
};
#define RI_AUTO_PUB_NONE 2u
void ri_auto_pub_init(struct RIAutoPub *p,
    struct RIAutoEv *ev0, uint32_t cap0,
    struct RIAutoEv *ev1, uint32_t cap1);
struct RIAutoLane *ri_auto_pub_back(struct RIAutoPub *p);
const struct RIAutoLane *ri_auto_pub_front(const struct RIAutoPub *p);
void ri_auto_pub_request(struct RIAutoPub *p);
void ri_auto_pub_apply(struct RIAutoPub *p);
void ri_auto_pub_resync(struct RIAutoPub *p);
/* Carry across a publish: re-index by tick (indices differ between
 * lanes). The render loop calls this with its window cursor when it
 * observes a new front. */
void ri_auto_carry_reindex(struct RIAutoCarry *c,
    const struct RIAutoLane *lane, uint32_t from_tick);

uint32_t ri_auto_chase(const struct RIAutoLane *l, const struct RIAutoPass *p, uint32_t tick,
                       const struct RITempoMap *map, uint32_t ppq, struct RIEvent *out, uint32_t cap, uint32_t *seq);
uint32_t ri_auto_emit_range(const struct RIAutoLane *l, const struct RIAutoPass *p, struct RIAutoCarry *c,
                            uint32_t first, uint32_t count, const struct RITempoMap *map, uint32_t ppq,
                            struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
#endif
