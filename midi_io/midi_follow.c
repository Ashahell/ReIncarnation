/* midi_follow.c — MIDI clock-in follower core bodies (R1 interop).
 * Integer interval math (deterministic twins); float only at the BPM
 * readout. No allocation, no IO. */
#include "midi_io/midi_follow.h"

void midi_follow_init(struct RIFollow *f) {
    uint32_t i;
    if (!f)
        return;
    f->last_us = 0u;
    for (i = 0u; i < RI_FOLLOW_LOCK_N; i++)
        f->ring[i] = 0u;
    f->sum_us = 0u;
    f->n = 0u;
    f->streak = 0u;
    f->have_tick = 0u;
    f->stop_latched = 0u;
    f->pad[0] = f->pad[1] = 0u;
}

static void intent_none(struct RIFollowIntent *it) {
    it->kind = RI_FOLLOW_NONE;
    it->pad[0] = it->pad[1] = it->pad[2] = 0u;
    it->seek_tick = 0u;
}

int midi_follow_tick(struct RIFollow *f, uint64_t now_us,
    struct RIFollowIntent *it) {
    uint64_t dt;
    if (!f || !it)
        return 2;
    intent_none(it);
    f->stop_latched = 0u; /* traffic resumes the take */
    if (!f->have_tick) {
        f->last_us = now_us;
        f->have_tick = 1u;
        return 0;
    }
    if (now_us <= f->last_us)
        return 0; /* non-monotonic stamps never move the estimate */
    dt = now_us - f->last_us;
    f->last_us = now_us;
    if (dt > 0xFFFFFFFFu)
        dt = 0xFFFFFFFFu;
    if (f->n > 0u) {
        uint64_t avg = f->sum_us / f->n;
        uint64_t dev = dt > avg ? dt - avg : avg - dt;
        if (dev > avg / 4u) {
            f->streak = 0u; /* wild tick: rejected, estimate stands */
            return 0;
        }
    }
    if (f->n < RI_FOLLOW_LOCK_N) {
        f->ring[f->n] = (uint32_t)dt;
        f->sum_us += dt;
        f->n++;
    } else {
        f->sum_us -= f->ring[0];
        {
            uint32_t i;
            for (i = 0u; i + 1u < RI_FOLLOW_LOCK_N; i++)
                f->ring[i] = f->ring[i + 1u];
            f->ring[RI_FOLLOW_LOCK_N - 1u] = (uint32_t)dt;
        }
        f->sum_us += dt;
    }
    if (f->streak < 0xFFFFFFFFu)
        f->streak++;
    return 0;
}

int midi_follow_start(struct RIFollow *f, struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    it->kind = RI_FOLLOW_PLAY_START;
    f->stop_latched = 0u;
    return 0;
}

int midi_follow_continue(struct RIFollow *f, struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    it->kind = RI_FOLLOW_CONTINUE;
    f->stop_latched = 0u;
    return 0;
}

int midi_follow_stop(struct RIFollow *f, struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    it->kind = RI_FOLLOW_STOP;
    f->stop_latched = 1u;
    return 0;
}

int midi_follow_spp(struct RIFollow *f, uint32_t spp_beats,
    struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    it->kind = RI_FOLLOW_SEEK;
    it->seek_tick = spp_beats * 24u; /* 16ths at engine PPQ=96 */
    return 0;
}

int midi_follow_poll(struct RIFollow *f, uint64_t now_us,
    struct RIFollowIntent *it) {
    uint64_t gap, limit;
    if (!f || !it)
        return 2;
    intent_none(it);
    if (!f->have_tick || f->stop_latched || now_us <= f->last_us)
        return 0;
    gap = now_us - f->last_us;
    limit = f->n > 0u ? f->sum_us / f->n * (uint64_t)RI_FOLLOW_MISSED_TICKS
        : (uint64_t)RI_FOLLOW_COLD_US;
    if (gap > limit) {
        it->kind = RI_FOLLOW_STOP;
        f->stop_latched = 1u;
    }
    return 0;
}

float midi_follow_bpm(const struct RIFollow *f) {
    uint64_t avg;
    if (!f || f->n == 0u)
        return 0.0f;
    avg = f->sum_us / f->n;
    if (avg == 0u)
        return 0.0f;
    return 60000000.0f / ((float)avg * 24.0f);
}

int midi_follow_locked(const struct RIFollow *f) {
    if (!f)
        return 0;
    return f->streak >= RI_FOLLOW_LOCK_N ? 1 : 0;
}
