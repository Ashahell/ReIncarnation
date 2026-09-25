/* player.c — streaming player (spec 2026-09-25 §§1–2).
 * State advance is tick-domain; samples come only from ri_map_tick
 * inside the emitters. No alloc, no IO, no mutable static state. */
#include "engine/seq/player.h"

void ri_player_init(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *t, uint64_t start_bar) {
    uint32_t i;
    uint64_t sb;
    if (!p)
        return;
    sb = start_bar;
    if (sb >= (uint64_t)RI_SONGTRACK_BARS)
        sb = (uint64_t)RI_SONGTRACK_BARS - 1u; /* end boundary never a start */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        uint8_t sel = t ? ri_track_selected(t, sb, i) : 0u;
        p->phase_ticks[i] = 0u;
        p->sounding_slot[i] = sel;
        p->pending_slot[i] = sel;
        p->sched_carry[i].valid = 0u;
        p->sched_carry[i].held_note = 0u;
        p->sched_carry[i].pad[0] = 0u;
        p->sched_carry[i].pad[1] = 0u;
        p->banks[i] = (banks != 0) ? banks[i] : 0;
    }
    p->track_carry.known = 0u;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        p->track_carry.prev[i] = 0u;
}

void ri_player_refresh_banks(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES]) {
    uint32_t i;
    if (!p || !banks)
        return; /* pointers only — never phase, never sounding */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        p->banks[i] = banks[i];
}

/* Block emission lands in Task 2; until then fail closed (no state change). */
uint32_t ri_player_block(struct RIPlayer *p, const struct RISongTrack *t,
    const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq,
    uint64_t tick_start, uint64_t tick_end, struct RIEvent *out, uint32_t cap) {
    (void)p; (void)t; (void)loop; (void)map; (void)ppq;
    (void)tick_start; (void)tick_end; (void)out; (void)cap;
    return 0u;
}
