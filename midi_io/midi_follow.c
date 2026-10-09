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
    f->spp_pend = 0u;
    f->spp_lsb = 0u;
    f->play_arm = 0u;
    f->playing = 0u;
    f->padf[0] = f->padf[1] = 0u;
    f->spp_ignored = 0u;
}

static void intent_none(struct RIFollowIntent *it) {
    it->kind = RI_FOLLOW_NONE;
    it->pad[0] = it->pad[1] = it->pad[2] = 0u;
    it->seek_16ths = 0u;
}

int midi_follow_tick(struct RIFollow *f, uint64_t now_us,
    struct RIFollowIntent *it) {
    uint64_t dt;
    if (!f || !it)
        return 2;
    intent_none(it);
    f->stop_latched = 0u; /* traffic resumes the take */
    if (f->clocks < 0xFFFFFFFFu)
        f->clocks++;
    if (f->play_arm) {    /* M1: an armed Start/Continue fires on this F8 */
        it->kind = f->play_arm == 1u ? RI_FOLLOW_PLAY_START : RI_FOLLOW_CONTINUE;
        f->play_arm = 0u;
        f->playing = 1u;
    }
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
    if (f->playing)
        return 0; /* Start while in play is ignored (MIDI law) */
    f->play_arm = 1u;
    f->stop_latched = 0u;
    return 0;
}

int midi_follow_continue(struct RIFollow *f, struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    if (f->playing)
        return 0; /* Continue while in play is ignored (MIDI law) */
    f->play_arm = 2u;
    f->stop_latched = 0u;
    return 0;
}

int midi_follow_stop(struct RIFollow *f, struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    it->kind = RI_FOLLOW_STOP;
    f->stop_latched = 1u;
    f->play_arm = 0u; /* a stopped take drops a pending arm */
    f->playing = 0u;
    return 0;
}

int midi_follow_spp(struct RIFollow *f, uint32_t spp_beats,
    struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    if (f->playing) { /* E0: SPP arrives only while stopped; count and ignore it running */
        if (f->spp_ignored < 0xFFFFFFFFu)
            f->spp_ignored++;
        return 0;
    }
    it->kind = RI_FOLLOW_SEEK;
    /* MIDI PPQN counts QUARTER notes, so a beat is four sixteenths. The
     * intent speaks sixteenths (the panel/transport layer owns the engine
     * PPQ and converts); M1 had this at ppq/4 per beat, a quarter of the
     * real distance (M3 lane proof: SPP 5120 located bar 320 of a 151-bar
     * song). */
    it->seek_16ths = spp_beats * 4u;
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
        f->play_arm = 0u; /* the take ends: a new Start is needed */
        f->playing = 0u;
    }
    return 0;
}

int midi_follow_rt(struct RIFollow *f, uint8_t byte, uint64_t now_us,
    struct RIFollowIntent *it) {
    if (!f || !it)
        return 2;
    intent_none(it);
    switch (byte) {
    case 0xF8u:
        return midi_follow_tick(f, now_us, it);
    case 0xFAu:
        return midi_follow_start(f, it);
    case 0xFBu:
        return midi_follow_continue(f, it);
    case 0xFCu:
        return midi_follow_stop(f, it);
    case 0xFEu: /* active sense */
    case 0xF6u: /* tune request */
        return 0;
    case 0xF2u:
        f->spp_pend = 1u;
        return 0;
    default:
        break;
    }
    if (byte < 0x80u && f->spp_pend == 1u) {
        f->spp_lsb = byte & 0x7Fu;
        f->spp_pend = 2u;
        return 0;
    }
    if (byte < 0x80u && f->spp_pend == 2u) {
        uint32_t beats = (uint32_t)f->spp_lsb + ((uint32_t)(byte & 0x7Fu) << 7);
        f->spp_pend = 0u;
        return midi_follow_spp(f, beats, it);
    }
    if (byte >= 0x80u)
        f->spp_pend = 0u; /* status aborts a pending SPP */
    return 0; /* channel voice bytes: G7 owns notes/CC */
}

void midi_sync_init(struct RIFollowSync *s) {
    if (!s)
        return;
    s->source = RI_SYNC_INTERNAL;
    s->following = 0u;
    s->pad[0] = s->pad[1] = 0u;
    s->measured_bpm = 0.0f;
    s->held_bpm = 0.0f;
}

int midi_sync_set_source(struct RIFollowSync *s, uint32_t source) {
    if (!s || (source != RI_SYNC_INTERNAL && source != RI_SYNC_MIDI))
        return 2;
    s->source = (uint8_t)source;
    s->following = 0u;
    return 0;
}

int midi_sync_update(struct RIFollowSync *s, uint32_t locked, float bpm) {
    if (!s)
        return 2;
    if (s->source == RI_SYNC_MIDI && locked) {
        s->following = 1u;
        s->measured_bpm = bpm;
        s->held_bpm = bpm;
    } else {
        s->following = 0u;
    }
    return 0;
}

int midi_sync_note_drop(struct RIFollowSync *s) {
    if (!s)
        return 2;
    s->following = 0u;
    return 0;
}

int midi_sync_knob_locked(const struct RIFollowSync *s) {
    if (!s)
        return 0;
    return (s->source == RI_SYNC_MIDI && s->following) ? 1 : 0;
}

float midi_sync_tempo(const struct RIFollowSync *s) {
    if (!s)
        return -1.0f;
    if (s->source == RI_SYNC_MIDI && s->following)
        return s->measured_bpm;
    if (s->source == RI_SYNC_MIDI)
        return s->held_bpm;
    return 0.0f;
}

float midi_follow_bpm(const struct RIFollow *f) {    uint64_t avg;
    if (!f || f->n == 0u)
        return 0.0f;
    avg = f->sum_us / f->n;
    if (avg == 0u)
        return 0.0f;
    return 60000000.0f / ((float)avg * 24.0f);
}

uint32_t midi_follow_clocks(const struct RIFollow *f) {
    return f ? f->clocks : 0u;
}

int midi_follow_locked(const struct RIFollow *f) {
    if (!f)
        return 0;
    return f->streak >= RI_FOLLOW_LOCK_N ? 1 : 0;
}
