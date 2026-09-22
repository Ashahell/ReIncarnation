/* snapbuild.h — snapshot event-build (WBS 2.1 builder completion).
 * Composes songsteps (song -> steps) with the walker (steps -> events)
 * into caller-owned event storage. Steps scratch is a fixed stack array
 * (256 steps x 2 B; no heap per Phase-0a, no caller scratch burden).
 * start_tick is always 0 (song-relative placement is the loop cursor's
 * job, RiSeqLoopPos). opts may be NULL (= straight).
 */
#ifndef RI_SNAPBUILD_H
#define RI_SNAPBUILD_H
#include <stdint.h>

struct RISong;      /* project/rbng.h */
struct RITempoMap;  /* engine/seq/clock.h */
struct RISchedOpts; /* engine/seq/sched.h */
struct RIEvent;     /* engine/seq/sched.h */

uint32_t ri_snapshot_build_events(const struct RISong *song,
    const struct RITempoMap *map, uint32_t ppq, uint16_t device,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap);
#endif
