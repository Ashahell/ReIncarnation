/* gui/miditrans.c — MIDI clock to transport bodies (M3). See header. */
#include "gui/miditrans.h"
#include "gui/sectui.h"
#include "gui/secttr.h"
#include "gui/ctlreg.h"
#include "engine/seq/transport.h"
#include "midi_io/midi_follow.h"

void midi_trans_init(struct RIMidiTrans *t) {
    if (!t)
        return;
    midi_sync_init(&t->sync);
    t->playing = 0u;
    t->pad[0] = t->pad[1] = t->pad[2] = 0u;
    t->expected = 0;
    t->lat_ms = 0;
    t->pad2 = 0;
}

void midi_trans_set_lat(struct RIMidiTrans *t, int16_t ms) {
    if (t)
        t->lat_ms = ms;
}

void midi_trans_set_source(struct RIMidiTrans *t, uint32_t source) {
    if (t)
        midi_sync_set_source(&t->sync, source);
}

/* Latency offset in engine ticks at a tempo (E0 default 0 = exact). */
static int64_t lat_ticks(const struct RIMidiTrans *t, float bpm) {
    double ticks;
    if (!t || t->lat_ms == 0 || bpm <= 0.0f)
        return 0;
    ticks = (double)t->lat_ms * (double)bpm * 96.0 / 60000.0;
    return ticks > 0.0 ? (int64_t)(ticks + 0.5) : -(int64_t)(-ticks + 0.5);
}

void midi_trans_apply(struct RIMidiTrans *t, const struct RIFollowIntent *it,
    struct RISectUI *tru, uint64_t *cursor) {
    uint32_t kind;
    if (!t || !it || !tru || !cursor)
        return;
    if (t->sync.source != RI_SYNC_MIDI)
        return; /* M3 is opt-in: Internal ignores MIDI transport */
    kind = it->kind;
    if (kind == RI_FOLLOW_PLAY_START) {
        *cursor = 0u;
        t->expected = lat_ticks(t, midi_sync_tempo(&t->sync));
        ri_sui_press(tru, RI_STR_PLAY);
        t->playing = 1u;
    } else if (kind == RI_FOLLOW_CONTINUE) {
        t->expected = (int64_t)*cursor + lat_ticks(t, midi_sync_tempo(&t->sync));
        ri_sui_press(tru, RI_STR_PLAY);
        t->playing = 1u;
    } else if (kind == RI_FOLLOW_STOP) {
        ri_sui_press(tru, RI_STR_STOP);
        midi_sync_note_drop(&t->sync);
        t->playing = 0u;
    } else if (kind == RI_FOLLOW_SEEK) {
        *cursor = it->seek_tick;
        t->expected = (int64_t)it->seek_tick;
    }
}

float midi_trans_tempo(struct RIMidiTrans *t, struct RISectUI *tru,
    uint32_t nf8, uint32_t locked, float bpm, uint64_t cursor) {
    int64_t err;
    float trim;
    uint32_t want_lock;
    if (!t)
        return 0.0f;
    midi_sync_update(&t->sync, locked, bpm);
    want_lock = (uint32_t)(t->sync.source == RI_SYNC_MIDI && locked);
    if (tru && tru->section == RI_SEC_TRANSPORT) {
        /* Knob read-only + TAP disabled while following; the display
         * shows the measured tempo (follower write, not a user edit). */
        ri_str_set_tempo_lock(&tru->u.tr, want_lock);
        if (want_lock) {
            int want = (int)(bpm + 0.5f);
            int16_t *shown = &tru->u.tr.tempo;
            if (want < 20)
                want = 20;
            if (want > 500)
                want = 500;
            if (*shown != (int16_t)want)
                *shown = (int16_t)want;
        }
    }
    if (!want_lock)
        return 0.0f;
    if (t->playing)
        t->expected += (int64_t)nf8 * 4;
    /* Phase servo: the integral of tempo error is position error, so a
     * converging estimate alone leaves a standing offset. Trimming the
     * session tempo toward the clock (bounded, gentle) drives the
     * residual to zero instead of merely stopping its growth. */
    err = t->expected - (int64_t)cursor;
    trim = (float)err * RI_MTRANS_TRIM_BPM_PER_TICK;
    if (trim > RI_MTRANS_TRIM_MAX_BPM)
        trim = RI_MTRANS_TRIM_MAX_BPM;
    else if (trim < -RI_MTRANS_TRIM_MAX_BPM)
        trim = -RI_MTRANS_TRIM_MAX_BPM;
    return bpm + trim;
}

int midi_trans_tempo_locked(const struct RIMidiTrans *t) {
    return t ? midi_sync_knob_locked(&t->sync) : 0;
}

const struct RIFollowSync *midi_trans_sync(const struct RIMidiTrans *t) {
    return t ? &t->sync : 0;
}
