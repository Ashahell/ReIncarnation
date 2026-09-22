/* snapbuild.c — snapshot event-build (WBS 2.1 builder completion).
 * Portable C99. No allocation (stack scratch + caller storage),
 * bounded loops only (delegated bounds: converter caps steps, walker
 * caps events).
 */
#include "engine/seq/snapbuild.h"
#include "engine/seq/songsteps.h"
#include "engine/seq/sched.h"

uint32_t ri_snapshot_build_events(const struct RISong *song,
    const struct RITempoMap *map, uint32_t ppq, uint16_t device,
    const struct RISchedOpts *opts, struct RIEvent *out, uint32_t cap) {
    struct RIStep tmp[RI_SCHED_MAX_EVENTS];
    uint32_t nsteps;
    if (!song || !map || !out || cap == 0u)
        return 0u;
    nsteps = ri_song_to_steps(song, tmp, RI_SCHED_MAX_EVENTS);
    if (nsteps == 0u)
        return 0u;
    return ri_sched_emit_timed(map, 0ULL, ppq, tmp, nsteps, device,
                               opts, out, cap);
}
