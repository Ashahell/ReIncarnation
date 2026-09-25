/* transport.c — transport state machine (spec 2026-09-25 §1). */
#include "engine/seq/transport.h"
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks) {
    (void)cursor_ticks;
    if (!t)
        return;
    if (t->state == RI_TR_STOPPED) {
        t->state = RI_TR_PLAYING;
        t->clicks = 0u;
    }
}
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick) {
    if (!t || !cursor_ticks)
        return;
    if (t->state != RI_TR_STOPPED) {
        t->state = RI_TR_STOPPED;
        t->clicks = 1u;
        return; /* cursor held (click 1) */
    }
    if (t->clicks == 0u) {
        /* First stop while already STOPPED: arm only (clicks 0->1),
        * cursor held. The visible jump comes on the 2nd press. */
        t->clicks = 1u;
    } else if (t->clicks == 1u) {
        *cursor_ticks = loop_start_tick;
        t->clicks = 2u;
    } else {
        *cursor_ticks = song_start_tick;
        t->clicks = 0u;
    }
}
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks) {
    (void)cursor_ticks;
    if (!t)
        return;
    if (t->state == RI_TR_STOPPED) {
        t->state = RI_TR_RECORD;
        t->clicks = 0u;
    } else if (t->state == RI_TR_PLAYING) {
        t->state = RI_TR_RECORD;
        t->clicks = 0u; /* law established locally, never assumed */
    } else {
        t->state = RI_TR_PLAYING;
        t->clicks = 0u; /* law established locally, never assumed */
    }
}
/* Shared fallback lives in the header (ri_ppq_or_default); no local copy. */
uint64_t ri_seq_tick_of_bar(uint32_t ppq, uint64_t bar) {
    return bar * 4u * (uint64_t)ri_ppq_or_default(ppq);
}
uint64_t ri_seq_bar_at_tick(uint64_t tick, uint32_t ppq) {
    return tick / (4u * (uint64_t)ri_ppq_or_default(ppq));
}
/* Pure projection: (tick, ppq) -> 1-based panel struct. No clamp, no
 * song knowledge — the caller clamps the tick first (see test). */
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq) {
    struct RIBarPos d;
    uint64_t p = (uint64_t)ri_ppq_or_default(ppq);
    uint64_t inbar;
    d.bar = (uint16_t)(ri_seq_bar_at_tick(tick, (uint32_t)p) + 1u);
    inbar = tick - ri_seq_tick_of_bar((uint32_t)p, (uint64_t)(d.bar - 1u));
    d.beat = (uint8_t)(inbar / p + 1u);
    d.sixteenth = (uint8_t)((inbar % p) / (p / 4u) + 1u);
    return d;
}
/* Seek takes song_bars: the caller owns the bar count, so this path
 * never re-derives bars from ticks and never touches the tempo map. */
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_bars) {
    uint64_t cur_bar, max_bar, target;
    if (!t || !cursor_ticks)
        return;
    /* Normalize FIRST (policy): empty song -> tick 0, no primitive called. */
    if (song_bars == 0u) { *cursor_ticks = 0u; t->clicks = 0u; return; }
    song_bars = song_bars > RI_SEQ_MAX_BARS ? RI_SEQ_MAX_BARS : song_bars;
    max_bar = song_bars - 1u; /* last VALID start; end boundary is not a bar */
    cur_bar = ri_seq_bar_at_tick(*cursor_ticks, ppq);
    if (cur_bar > max_bar)
        cur_bar = max_bar; /* cursor invariant repair: never seek FROM past-end */
    if (delta_bars < 0) {
        /* Unsigned-only: -(INT32_MIN) is computed in int64_t, then bounded. */
        int64_t dneg = -(int64_t)delta_bars; /* 1..2147483648, always positive */
        uint64_t dist = (uint64_t)dneg;
        target = (dist > cur_bar) ? 0u : cur_bar - dist;
    } else {
        uint64_t dist = (uint64_t)(int64_t)delta_bars;
        /* max_bar - cur_bar cannot underflow (cur_bar <= max_bar above). */
        target = (dist > max_bar - cur_bar) ? max_bar : cur_bar + dist;
    }
    *cursor_ticks = ri_seq_tick_of_bar(ppq, target);
    t->clicks = 0u; /* cursor intent replaces the stop sequence */
}
void ri_loop_clamp(struct RILoop *loop, uint64_t song_bars) {
    if (!loop)
        return;
    song_bars = song_bars > RI_SEQ_MAX_BARS ? RI_SEQ_MAX_BARS : song_bars;
    if (song_bars == 0u) {
        loop->on = 0u; loop->start_bar = 0u; loop->len_bars = 0u;
        return;
    }
    if (loop->len_bars == 0u) {
        loop->on = 0u; /* zero length = loop off, never an empty region */
        return;
    }
    if ((uint64_t)loop->start_bar >= song_bars) {
        loop->start_bar = (uint16_t)(song_bars - 1u);
        loop->len_bars = 1u;
        return;
    }
    if ((uint64_t)loop->start_bar + (uint64_t)loop->len_bars > song_bars)
        loop->len_bars = (uint16_t)(song_bars - (uint64_t)loop->start_bar);
}
void ri_loop_stage(struct RILoopPending *p, const struct RILoop *loop, uint64_t bar_now,
                   uint64_t song_bars) {
    if (!p || !loop)
        return;
    song_bars = song_bars > RI_SEQ_MAX_BARS ? RI_SEQ_MAX_BARS : song_bars;
    if (bar_now > RI_SEQ_MAX_BARS)
        bar_now = RI_SEQ_MAX_BARS;
    p->loop = *loop;
    ri_loop_clamp(&p->loop, song_bars);
    p->apply_bar = (bar_now >= RI_SEQ_MAX_BARS) ? (uint64_t)RI_SEQ_MAX_BARS + 1u : bar_now + 1u;
    p->valid = 1u;
}
int ri_loop_poll(struct RILoopPending *p, struct RILoop *live, uint64_t bar_now) {
    if (!p || !live || !p->valid)
        return 0;
    if (bar_now < p->apply_bar)
        return 0;
    *live = p->loop;
    p->valid = 0u;
    return 1;
}
