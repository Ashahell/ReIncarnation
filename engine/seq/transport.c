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
