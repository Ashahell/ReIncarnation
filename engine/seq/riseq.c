/* riseq.c — master clock object (WBS 2.1, TC-2.1.1).
 * Sample counter: plain uint64 bump (single-writer render thread by
 * contract — no atomics, no locks). Tick mapping delegates to clock.c
 * (exact rational); this unit owns the map and the counter only.
 * Portable C99, stdint.h only. No allocation anywhere (Phase-0a).
 */
#include "engine/seq/riseq.h"

#define RISEQ_SR 48000u            /* locked engine rate (RI_AUDIO_SR) */
#define RISEQ_DEFAULT_BPM_NS 500000000ULL /* 120 BPM bring-up map */

void RiSeqInit(struct RISeq *s, struct AudioObject *ao, uint32_t ppq) {
    if (!s)
        return;
    s->ao = ao;
    s->ppq = ppq ? ppq : 96u;
    s->samples = 0ULL;
    s->seg.start_tick = 0ULL;
    s->seg.ns_per_quarter = RISEQ_DEFAULT_BPM_NS;
    s->map.segs = &s->seg;
    s->map.n = 1u;
    s->map.ppq = s->ppq;
    s->map.sr = RISEQ_SR;
}

void RiSeqAdvanceFrames(struct RISeq *s, uint32_t frames) {
    if (s)
        s->samples += (uint64_t)frames;
}

uint64_t RiSeqMasterClock(const struct RISeq *s) {
    return s ? s->samples : 0ULL;
}
