/* songsteps.c — song-to-steps converter (WBS 2.1 builder step 1).
 * Portable C99, stdint.h only. No allocation, bounded loops only.
 */
#include "engine/seq/songsteps.h"
#include "project/rbng.h"
#include "engine/seq/sched.h"

uint32_t ri_song_to_steps(const struct RISong *song, struct RIStep *out,
                          uint32_t cap) {
    uint32_t n, i;
    if (!song || !out || cap == 0u)
        return 0u;
    n = (uint32_t)song->nsteps;
    if (n > cap)
        n = cap;
    if (n > RI_RBNG_MAX_STEPS)
        n = RI_RBNG_MAX_STEPS;
    for (i = 0u; i < n; i++) {
        out[i].note = song->steps[i].note;
        out[i].flags = song->steps[i].flags;
    }
    return n;
}
