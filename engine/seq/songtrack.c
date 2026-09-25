/* songtrack.c — song track model (spec 2026-09-25 §1, laws §Capture).
 * Reads and single writes REFUSE out-of-range input (fail-closed);
 * nothing here wraps, clamps up, or allocates. */
#include "engine/seq/songtrack.h"
#include "engine/seq/songtrack_emit.h"

uint32_t ri_track_emit_measure(const struct RISongTrack *t, uint64_t bar,
    struct RITrackCarry *carry, const struct RITempoMap *map, uint32_t ppq,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    uint32_t i, added = 0u;
    uint64_t sample;
    if (!t || !carry || !map || !out || !n || !seq)
        return 0u;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS)
        return 0u; /* end boundary is never a valid start; carry left cold */
    sample = ri_map_tick(map, ri_seq_tick_of_bar(ppq, bar));
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        uint8_t cur = ri_track_selected(t, bar, i);
        uint8_t bit = (uint8_t)(1u << i);
        if ((carry->known & bit) && cur == carry->prev[i])
            continue; /* recorded truth: no change, nothing to say */
        if (*n >= cap)
            continue; /* cap: later instances drop first; stays PENDING */
        out[*n].sample = sample;
        out[*n].type = RI_EV_PATTERN_CHANGE;
        out[*n].device = (uint16_t)i;
        out[*n].voice = 0u;
        out[*n].value = (uint16_t)cur; /* slot NUMBER, never pattern data */
        out[*n].flags = 0u;
        out[*n].seq = (*seq)++;
        (*n)++;
        added++;
        /* The carry mirrors what was EMITTED: a change the cap dropped keeps
         * its old prev/known bit and is re-sent at the next measure. */
        carry->prev[i] = cur;
        carry->known = (uint8_t)(carry->known | bit);
    }
    return added;
}

uint32_t ri_track_emit_range(const struct RISongTrack *t, uint64_t bar_first,
    uint64_t bar_count, const struct RILoop *loop, const struct RITempoMap *map,
    uint32_t ppq, struct RITrackCarry *carry, struct RIEvent *out, uint32_t cap) {
    uint32_t n = 0u, seq = 0u;
    uint64_t ls = 0u, llen = 0u, k;
    struct RILoop l;
    int looping = 0;
    if (!t || !carry || !map || !out || cap == 0u || bar_count == 0u)
        return 0u;
    /* Normalize FIRST (transport law, reused): a raw loop past the song end
     * is clamped exactly as transport clamps it; zero length -> OFF. */
    if (loop) {
        l = *loop;
        ri_loop_clamp(&l, (uint64_t)RI_SONGTRACK_BARS);
        looping = (l.on && l.len_bars > 0u) ? 1 : 0;
    }
    if (looping) {
        ls = (uint64_t)l.start_bar;
        llen = (uint64_t)l.len_bars;
        if (bar_first >= ls + llen)
            bar_first = ls + ((bar_first - ls) % llen); /* fold in, keep phase */
    } else if (bar_first >= (uint64_t)RI_SONGTRACK_BARS) {
        return 0u; /* past the end with no loop: nothing to emit */
    }
    /* No call needs more bars than the song holds. This bound also makes
     * bar_first + k overflow-proof (both < 2000 after the fold). */
    if (bar_count > (uint64_t)RI_SONGTRACK_BARS)
        bar_count = (uint64_t)RI_SONGTRACK_BARS;
    for (k = 0u; k < bar_count; k++) {
        uint64_t bar = bar_first + k;
        if (looping && bar >= ls + llen)
            bar = ls + ((bar - ls) % llen); /* wrap PRESERVES PHASE */
        if (bar >= (uint64_t)RI_SONGTRACK_BARS)
            break; /* truncated before the end boundary */
        ri_track_emit_measure(t, bar, carry, map, ppq, out, &n, cap, &seq);
    }
    return n;
}

int ri_song_ended(uint64_t bar_now) {
    return bar_now >= (uint64_t)RI_SONGTRACK_BARS ? 1 : 0;
}

void ri_track_init(struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = 0u;
}

uint8_t ri_track_selected(const struct RISongTrack *t, uint64_t bar, uint32_t instance) {
    if (!t)
        return 0u;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 0u; /* end boundary is never a valid start */
    return t->slot[bar][instance];
}

int ri_track_capture(struct RISongTrack *t, uint64_t bar, uint32_t instance, uint8_t slot) {
    if (!t)
        return 2;
    if (bar >= (uint64_t)RI_SONGTRACK_BARS || instance >= RI_SONGTRACK_INSTANCES)
        return 2;
    if (slot > RI_SONGTRACK_MAX_SLOT)
        return 2;
    t->slot[bar][instance] = slot; /* one slot per (bar, instance): overwrite */
    return 0;
}

int ri_track_is_empty(const struct RISongTrack *t) {
    uint32_t b, i;
    if (!t)
        return 1;
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            if (t->slot[b][i] != 0u)
                return 0;
    return 1;
}
