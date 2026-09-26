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

uint32_t ri_auto_chase(const struct RIAutoLane *l, const struct RIAutoPass *p, uint32_t tick,
                       const struct RITempoMap *map, uint32_t ppq, struct RIEvent *out, uint32_t cap, uint32_t *seq);
uint32_t ri_auto_emit_range(const struct RIAutoLane *l, const struct RIAutoPass *p, struct RIAutoCarry *c,
                            uint32_t first, uint32_t count, const struct RITempoMap *map, uint32_t ppq,
                            struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
#endif
