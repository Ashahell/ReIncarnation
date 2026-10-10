/* gui/secttr.c — Transport panel front-panel behaviour (§12.10 G3/G4). */
#include "gui/secttr.h"
#include "gui/ctlreg.h"

int ri_str_init(struct RISectTr *s) {
    if (!s)
        return 2;
    s->section = RI_SEC_TRANSPORT;
    s->song_mode = 0;
    s->midi_led = s->sync_led = s->clkout_led = 0;
    s->tempo = 120;
    s->shuffle = 0;
    s->tr.state = RI_TR_STOPPED;
    s->tr.clicks = 0;
    s->loop.on = 0;
    s->loop.start_bar = 0;
    s->loop.len_bars = 4;
    s->cursor = 0;
    s->ppq = RI_PPQ_DEFAULT;
    s->song_bars = RI_SEQ_MAX_BARS;
    s->tap_iv[0] = s->tap_iv[1] = s->tap_iv[2] = s->tap_iv[3] = 0u;
    s->tap_last = 0u;
    s->tap_n = 0u;
    s->tap_have = 0u;
    s->tempo_lock = 0u;
    return 0;
}

static int seek(struct RISectTr *s, int32_t bars) {
    uint64_t before = s->cursor;
    if (!s->song_mode)
        return 0;
    ri_tr_seek_bars(&s->tr, &s->cursor, s->ppq, bars, s->song_bars);
    return s->cursor != before;
}

int ri_str_press(struct RISectTr *s, uint32_t idx) {
    struct RITransport t0;
    uint64_t c0;
    if (!s)
        return 0;
    t0 = s->tr;
    c0 = s->cursor;
    switch (idx) {
    case RI_STR_MODE:
        s->song_mode = (uint8_t)!s->song_mode;
        if (!s->song_mode && s->tr.state == RI_TR_RECORD)
            ri_tr_record(&s->tr, &s->cursor);   /* record -> play: no recording in Pattern mode */
        return 1;
    case RI_STR_PLAY:
        ri_tr_play(&s->tr, &s->cursor);
        break;
    case RI_STR_STOP:
        ri_tr_stop(&s->tr, &s->cursor, ri_seq_tick_of_bar(s->ppq, s->loop.start_bar), 0);
        break;
    case RI_STR_REW:
        return seek(s, -10);
    case RI_STR_FF:
        return seek(s, 10);
    case RI_STR_RECORD:
        if (!s->song_mode)
            return 0;
        ri_tr_record(&s->tr, &s->cursor);
        break;
    case RI_STR_LOOP:
        s->loop.on = (uint8_t)!s->loop.on;
        ri_loop_clamp(&s->loop, s->song_bars);
        return 1;
    default:
        return 0;
    }
    return t0.state != s->tr.state || t0.clicks != s->tr.clicks || c0 != s->cursor;
}

/* Tap tempo (P9e). Pure: the only state it owns is the tap window, and the
 * only thing it writes is the tempo, through the display's own 20..500
 * clamp — so a wild estimate lands on 20 or 500, never on a broken
 * readout, and the caller repaints only when the number moved.
 *
 * The average is over the kept intervals, not the last one: bpm =
 * 60000 * n / sum, rounded by adding half the sum before dividing. Four
 * intervals is enough to shrug off one clumsy tap without the display
 * lagging behind a deliberate change. */
int ri_str_tap(struct RISectTr *s, uint32_t ms) {
    uint32_t d, sum = 0u, i;
    int bpm;
    if (!s || ms == 0u)
        return 0;                     /* no clock: ignore, never guess */
    if (s->tempo_lock)
        return 0;                     /* M3: TAP disabled while following */
    if (!s->tap_have) {               /* first tap of a run: reference only */
        s->tap_have = 1u;
        s->tap_n = 0u;
        s->tap_last = ms;
        return 0;
    }
    if (ms <= s->tap_last) {          /* a clock that went backwards: restart */
        s->tap_n = 0u;
        s->tap_last = ms;
        return 0;
    }
    d = ms - s->tap_last;
    s->tap_last = ms;
    if (d > RI_STR_TAP_GAP_MS) {      /* a long pause starts a new measurement */
        s->tap_n = 0u;
        return 0;
    }
    if (s->tap_n < RI_STR_TAP_IV)
        s->tap_iv[s->tap_n] = d;
    else {                           /* window slides: drop the oldest */
        for (i = 1u; i < RI_STR_TAP_IV; i++)
            s->tap_iv[i - 1u] = s->tap_iv[i];
        s->tap_iv[RI_STR_TAP_IV - 1u] = d;
    }
    s->tap_n = (uint8_t)(s->tap_n < RI_STR_TAP_IV ? s->tap_n + 1u : RI_STR_TAP_IV);
    for (i = 0u; i < s->tap_n; i++)
        sum += s->tap_iv[i];
    bpm = (int)((60000u * s->tap_n + sum / 2u) / sum);
    return ri_str_set_value(s, RI_STR_TEMPO, bpm);
}

int ri_str_set_value(struct RISectTr *s, uint32_t idx, int v) {
    int old;
    if (!s)
        return 0;
    switch (idx) {
    case RI_STR_TEMPO:
        if (s->tempo_lock)
            return 0;                 /* M3: read-only while following */
        old = s->tempo;
        s->tempo = (int16_t)(v < 20 ? 20 : v > 500 ? 500 : v);
        return s->tempo != old;
    case RI_STR_SHUFFLE:
        old = s->shuffle;
        s->shuffle = (int16_t)(v < 0 ? 0 : v > 127 ? 127 : v);
        return s->shuffle != old;
    case RI_STR_LOOP_START:
        old = s->loop.start_bar;
        s->loop.start_bar = (uint16_t)((v < 1 ? 1 : v > (int)RI_SEQ_MAX_BARS ? (int)RI_SEQ_MAX_BARS : v) - 1);
        ri_loop_clamp(&s->loop, s->song_bars);
        return s->loop.start_bar != old;
    case RI_STR_LOOP_LEN:
        old = s->loop.len_bars;
        s->loop.len_bars = (uint16_t)(v < 1 ? 1 : v > (int)RI_SEQ_MAX_BARS ? (int)RI_SEQ_MAX_BARS : v);
        ri_loop_clamp(&s->loop, s->song_bars);
        return s->loop.len_bars != old;
    default:
        return 0;
    }
}

int ri_str_reset(struct RISectTr *s, uint32_t idx) {
    const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_TRANSPORT << 8) | idx));
    return d && idx != RI_STR_BAR ? ri_str_set_value(s, idx, d->def_v) : 0;
}

int ri_str_step(struct RISectTr *s, uint32_t idx, int dir) {
    int d = dir > 0 ? 1 : -1;
    if (!s || !dir)
        return 0;
    if (idx == RI_STR_BAR)
        return seek(s, d);
    if (idx == RI_STR_TEMPO || idx == RI_STR_LOOP_START || idx == RI_STR_LOOP_LEN)
        return ri_str_set_value(s, idx, ri_str_value(s, idx) + d);
    return 0;
}

int ri_str_value(const struct RISectTr *s, uint32_t idx) {
    if (!s)
        return 0;
    switch (idx) {
    case RI_STR_MODE: return s->song_mode;
    case RI_STR_TEMPO: return s->tempo;
    case RI_STR_SHUFFLE: return s->shuffle;
    case RI_STR_BAR: return ri_seq_bar_display(s->cursor, s->ppq).bar;
    case RI_STR_LOOP: return s->loop.on;
    case RI_STR_LOOP_START: return s->loop.start_bar + 1;
    case RI_STR_LOOP_LEN: return s->loop.len_bars;
    case RI_STR_MIDI: return s->midi_led;
    case RI_STR_SYNC: return s->sync_led;
    case RI_STR_CLKOUT: return s->clkout_led;
    default: return 0;
    }
}

int ri_str_led(const struct RISectTr *s, uint32_t idx, uint32_t which) {
    if (!s)
        return 0;
    switch (idx) {
    case RI_STR_MODE: return which ? s->song_mode : !s->song_mode;  /* 0 = Pattern LED, 1 = Song LED */
    case RI_STR_PLAY: return s->tr.state != RI_TR_STOPPED;
    case RI_STR_RECORD: return s->tr.state == RI_TR_RECORD;
    case RI_STR_LOOP: return s->loop.on;
    case RI_STR_MIDI: return s->midi_led;
    case RI_STR_SYNC: return s->sync_led;
    case RI_STR_CLKOUT: return s->clkout_led;
    default: return 0;
    }
}

void ri_str_clkout_set(struct RISectTr *s, int sending, uint32_t sent_total) {
    if (!s)
        return;
    /* DERIVED, and deliberately not reachable from ri_str_indicator_set():
     * this lamp is a report about bytes that actually left, so letting a
     * caller switch it on by hand would be asserting something the code
     * never observed. `sent_total` is carried so a caller can log the
     * running count alongside the lamp without keeping its own copy. */
    s->clkout_led = (uint8_t)(sending && sent_total > 0u ? 1 : 0);
}

int ri_str_goto_loop(struct RISectTr *s, int end) {
    uint64_t bar, before;
    if (!s || !s->song_mode)
        return 0;
    bar = (uint64_t)s->loop.start_bar + (end ? s->loop.len_bars : 0u);
    if (bar > (uint64_t)s->song_bars - 1u)
        bar = (uint64_t)s->song_bars - 1u;
    before = s->cursor;
    s->cursor = ri_seq_tick_of_bar(s->ppq, bar);
    s->tr.clicks = 0;   /* cursor intent replaces the stop sequence (engine seek law) */
    return s->cursor != before;
}

int ri_str_follow(struct RISectTr *s, uint64_t start_tick, uint64_t sixteenths) {
    uint64_t q, bar, bar0, before;
    if (!s || !s->song_mode || s->tr.state == RI_TR_STOPPED)
        return 0;
    q = (uint64_t)ri_ppq_or_default(s->ppq) / 4u;
    bar0 = ri_seq_bar_at_tick(start_tick, s->ppq);
    bar = ri_seq_bar_at_tick(start_tick + sixteenths * q, s->ppq);
    if (s->loop.on && s->loop.len_bars > 0u && bar0 >= s->loop.start_bar &&
        bar0 < (uint64_t)s->loop.start_bar + s->loop.len_bars && bar >= (uint64_t)s->loop.start_bar + s->loop.len_bars)
        bar = s->loop.start_bar + (bar - s->loop.start_bar) % s->loop.len_bars;
    if (bar > (uint64_t)s->song_bars - 1u)
        bar = (uint64_t)s->song_bars - 1u;   /* E1: playback continues to 999 */
    before = ri_seq_bar_at_tick(s->cursor, s->ppq);
    s->cursor = ri_seq_tick_of_bar(s->ppq, bar);
    return bar != before;
}

void ri_str_indicator_set(struct RISectTr *s, uint32_t idx, int v) {
    if (!s)
        return;
    if (idx == RI_STR_MIDI)
        s->midi_led = (uint8_t)(v != 0);
    else if (idx == RI_STR_SYNC)
        s->sync_led = (uint8_t)(v < 0 ? 0 : v > 2 ? 2 : v);
}

void ri_str_set_tempo_lock(struct RISectTr *s, uint32_t locked) {
    if (s)
        s->tempo_lock = locked ? 1u : 0u;
}
