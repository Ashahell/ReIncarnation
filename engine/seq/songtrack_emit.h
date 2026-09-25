/* songtrack_emit.h — song track change emission (spec 2026-09-25 §2).
 * Separate from songtrack.h so the model (and the RBNG codec header,
 * which includes the model) never pulls scheduler/clock headers.
 * The t59 guard greps this file: never name the codec header here. */
#ifndef RI_SONGTRACK_EMIT_H
#define RI_SONGTRACK_EMIT_H
#include "engine/seq/songtrack.h"
#include "engine/seq/sched.h"  /* RIEvent, RI_EV_* */
#include "engine/seq/clock.h"  /* RITempoMap */

/* Cross-window change cache: the caller owns it, one per play session.
 * known bit i = prev[i] holds what was last EMITTED for instance i. */
struct RITrackCarry { uint8_t known; uint8_t prev[RI_SONGTRACK_INSTANCES]; };

uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    struct RITrackCarry *carry, const struct RITempoMap *map, uint32_t ppq,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq);
uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RITrackCarry *carry, struct RIEvent *out, uint32_t cap);
int ri_song_ended(uint64_t bar_now);
#endif
