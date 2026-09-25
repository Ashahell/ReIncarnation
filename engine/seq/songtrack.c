/* songtrack.c — song track model (spec 2026-09-25 §1, laws §Capture).
 * Reads and single writes REFUSE out-of-range input (fail-closed);
 * nothing here wraps, clamps up, or allocates. */
#include "engine/seq/songtrack.h"

void ri_track_init(struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = 0u;
}

uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance) {
    if (!t)
        return 0u;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 0u; /* end boundary is never a valid start */
    return t->slot[bar][instance];
}

int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot) {
    if (!t)
        return 2;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 2;
    if (slot > RI_SONGTRACK_MAX_SLOT)
        return 2;
    t->slot[bar][instance] = slot; /* one slot per (bar, instance): overwrite */
    return 0;
}

int ri_track_is_empty(const struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return 1;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            if (t->slot[b][i] != 0u)
                return 0;
    return 1;
}
