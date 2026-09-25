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

/* Slot law, one enforcement point for every bulk writer: validate ALL
 * values before the first write (capture refuses the same way). */
static int track_row_ok(const uint8_t row[RI_SONGTRACK_INSTANCES]) {
    uint32_t i;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        if (row[i] > RI_SONGTRACK_MAX_SLOT)
            return 0;
    return 1;
}

/* Clip rows [0, len) that a paste would write: all valid, or refuse. */
static int track_clip_ok(const struct RITrackClip *clip, uint64_t len) {
    uint64_t b;
    for (b = 0u; b < len; b++)
        if (!track_row_ok(clip->slot[b]))
            return 0;
    return 1;
}

/* Clamp [start, start+len) into the song. Never evaluates start+len on an
 * unbounded len (unsigned addition is modulo and would wrap the check). */
static uint64_t track_clamp_len(uint64_t start, uint64_t len) {
    if (start >= (uint64_t)RI_SONGTRACK_BARS)
        return 0u;
    if (len > (uint64_t)RI_SONGTRACK_BARS - start)
        return (uint64_t)RI_SONGTRACK_BARS - start;
    return len;
}

static void track_clip_fill(const struct RISongTrack *t, uint64_t start,
    uint64_t len, struct RITrackClip *clip) {
    uint64_t k;
    uint32_t i;
    clip->len = (uint16_t)len;
    for (k = 0u; k < len; k++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            clip->slot[k][i] = t->slot[start + k][i];
}

int ri_track_init_song(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES]) {
    uint32_t b, i;
    if (!t || !slots || !track_row_ok(slots))
        return 2;
    /* Slot half of the E1 p. 75 promise: pattern-mode selections travel
     * into the song (the automation slice owns the knob half). */
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = slots[i];
    return 0;
}

int ri_track_init_loop(struct RISongTrack *t, const uint8_t slots[RI_SONGTRACK_INSTANCES],
                       uint64_t start_bar, uint64_t len_bars) {
    uint64_t b, end, len;
    uint32_t i;
    if (!t || !slots || !track_row_ok(slots))
        return 2;
    len = track_clamp_len(start_bar, len_bars);
    if (len == 0u)
        return 0; /* range repair: an empty range is a no-op, not a refusal */
    end = start_bar + len; /* bounded: both <= 999 here */
    /* E1 p. 176: ALL measures inside the loop take the pattern-mode
     * selections; the writes replace whatever was there. */
    for (b = start_bar; b < end; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = slots[i];
    return 0;
}

void ri_track_copy(const struct RISongTrack *t, uint64_t start, uint64_t len,
                   struct RITrackClip *clip) {
    uint64_t n;
    if (!t || !clip)
        return;
    clip->len = 0u; /* never leave a stale clipboard behind a refused op */
    n = track_clamp_len(start, len);
    if (n == 0u)
        return;
    track_clip_fill(t, start, n, clip);
}

void ri_track_cut(struct RISongTrack *t, uint64_t start, uint64_t len,
                  struct RITrackClip *clip) {
    uint64_t b, n;
    uint32_t i;
    if (!t || !clip)
        return;
    clip->len = 0u;
    n = track_clamp_len(start, len);
    if (n == 0u)
        return;
    track_clip_fill(t, start, n, clip);
    for (b = start; b + n < (uint64_t)RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = t->slot[b + n][i]; /* close the gap */
    for (b = (uint64_t)RI_SONGTRACK_BARS - n; b < (uint64_t)RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[b][i] = 0u; /* freed end: slot 0, never silence */
}

int ri_track_paste(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip) {
    uint64_t len, b;
    uint32_t i;
    int64_t s;
    if (!t || !clip || clip->len > RI_SONGTRACK_BARS)
        return 2; /* a clip longer than the song is malformed caller data */
    len = track_clamp_len(at, clip->len); /* drops overflow, never grows */
    if (len == 0u)
        return 0;
    if (!track_clip_ok(clip, len))
        return 2; /* validated before the shift: all-or-nothing */
    for (s = (int64_t)((uint64_t)RI_SONGTRACK_BARS - len) - 1; s >= (int64_t)at; s--)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[(uint64_t)s + len][i] = t->slot[(uint64_t)s][i];
    for (b = 0u; b < len; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[at + b][i] = clip->slot[b][i];
    return 0;
}

int ri_track_paste_replace(struct RISongTrack *t, uint64_t at, const struct RITrackClip *clip) {
    uint64_t len, b;
    uint32_t i;
    if (!t || !clip || clip->len > RI_SONGTRACK_BARS)
        return 2;
    len = track_clamp_len(at, clip->len);
    if (len == 0u)
        return 0;
    if (!track_clip_ok(clip, len))
        return 2;
    for (b = 0u; b < len; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            t->slot[at + b][i] = clip->slot[b][i];
    return 0;
}
