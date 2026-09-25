/* player.h — streaming player (spec 2026-09-25 §1).
 * Pure, no alloc, no IO, no mutable static state.
 * Banks are non-owning (caller-owned, live-read per block); the track
 * is read-only; the cursor stays in transport (ticks in, ticks out). */
#ifndef RI_PLAYER_H
#define RI_PLAYER_H
#include <stdint.h>
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/songtrack_emit.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

struct RIPlayer {
    uint64_t phase_ticks[RI_SONGTRACK_INSTANCES];
    uint8_t sounding_slot[RI_SONGTRACK_INSTANCES];
    uint8_t pending_slot[RI_SONGTRACK_INSTANCES];
    struct RITrackCarry track_carry;
    struct RISchedCarry sched_carry[RI_SONGTRACK_INSTANCES];
    const struct RIPatternBank *banks[RI_SONGTRACK_INSTANCES];
};
void ri_player_init(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *t, uint64_t start_bar);
void ri_player_refresh_banks(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES]);
uint32_t ri_player_block(struct RIPlayer *p, const struct RISongTrack *t,
    const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq,
    uint64_t tick_start, uint64_t tick_end, struct RIEvent *out, uint32_t cap);
#endif
