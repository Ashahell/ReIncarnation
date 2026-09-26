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
    s->snap.v = 0;
    s->pending.v = 0;
    /* Init before sharing; plain stores safe here. */
    s->seg.start_tick = 0ULL;
    s->seg.ns_per_quarter = RISEQ_DEFAULT_BPM_NS;
    s->map.segs = &s->seg;
    s->map.n = 1u;
    s->map.ppq = s->ppq;
    s->map.sr = RISEQ_SR;
    s->transport.state = RI_TR_STOPPED;
    s->transport.clicks = 0u;
    s->loop.on = 0u;
    s->loop.start_bar = 0u;
    s->loop.len_bars = 1u;
    s->loop_staged.valid = 0u;
    s->loop_staged.apply_bar = 0ULL;
    s->cursor_ticks = 0ULL;
}

void RiSeqAdvanceFrames(struct RISeq *s, uint32_t frames) {
    if (s)
        s->samples += (uint64_t)frames;
}

uint64_t RiSeqMasterClock(const struct RISeq *s) {
    return s ? s->samples : 0ULL;
}

void RiSeqLoadSnapshot(struct RISeq *s, const struct RISeqSnapshot *snap) {
    if (s)
        ri_atomic_ptr_store_rel(&s->snap, snap);
}

void RiSeqRequestSnapshot(struct RISeq *s, const struct RISeqSnapshot *snap) {
    if (s)
        ri_atomic_ptr_store_rel(&s->pending, snap);
}

const struct RISeqSnapshot *RiSeqBeginBuffer(struct RISeq *s) {
    const void *staged;
    if (!s)
        return 0;
    staged = ri_atomic_ptr_load_acq(&s->pending);
    if (staged) {
        ri_atomic_ptr_store_rel(&s->snap, staged);
        ri_atomic_ptr_store_rel(&s->pending, 0);
    }
    return (const struct RISeqSnapshot *)ri_atomic_ptr_load_acq(&s->snap);
}

uint64_t RiSeqLoopPos(const struct RISeq *s, uint64_t *iteration) {
    uint64_t samples, len, off;
    const struct RISeqSnapshot *snap;
    if (iteration)
        *iteration = 0ULL;
    if (!s)
        return 0ULL;
    samples = s->samples;
    snap = (const struct RISeqSnapshot *)ri_atomic_ptr_load_acq(&s->snap);
    if (!snap || snap->loop_end <= snap->loop_start)
        return samples;
    if (samples < snap->loop_start)
        return samples;
    len = snap->loop_end - snap->loop_start;
    off = samples - snap->loop_start;
    if (iteration)
        *iteration = off / len;
    return snap->loop_start + (off % len);
}

uint32_t ri_events_in_window(const struct RIEvent *ev, uint32_t n,
    uint64_t s0, uint64_t s1, struct RIEvent *out, uint32_t cap) {
    uint32_t i, m = 0u;
    if (!ev || !out || cap == 0u || s1 <= s0)
        return 0u;
    for (i = 0u; i < n; i++) {
        if (m >= cap)
            break;
        if (ev[i].sample >= s0 && ev[i].sample < s1)
            out[m++] = ev[i];
    }
    return m;
}
