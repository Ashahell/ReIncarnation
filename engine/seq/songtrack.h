/* songtrack.h — dense pattern-selection track (spec 2026-09-25 §1).
 * Pure, no alloc, no IO, no mutable static state.
 * One direction only: songtrack -> transport (geometry constants). */
#ifndef RI_SONGTRACK_H
#define RI_SONGTRACK_H
#include <stdint.h>
#include "engine/seq/transport.h"

#define RI_SONGTRACK_BARS      RI_SONG_BARS /* single source; never a forked 999 */
#define RI_SONGTRACK_INSTANCES 4u           /* 303A 303B 808 909 (Classic) */
#define RI_SONGTRACK_MAX_SLOT  31u          /* 4 banks x 8 (p. 147) */

struct RISongTrack { uint8_t slot[RI_SONGTRACK_BARS][RI_SONGTRACK_INSTANCES]; };

void     ri_track_init(struct RISongTrack *t);
uint8_t  ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance);
int      ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot);
int      ri_track_is_empty(const struct RISongTrack *t);
#endif
