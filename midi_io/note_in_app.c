/* midi_io/note_in_app.c — R5b. See note_in_app.h for the laws. */
#include <string.h>
#include "engine/seq/sched.h"   /* RI_EV_*, RI_EVFLAG_*, ri_event_less */
#include "midi_io/note_in_app.h"

void ri_notein_live_init(struct RINoteLive *l) {
    if (!l)
        return;
    memset(l, 0, sizeof *l);
}

int ri_notein_push(struct RINoteLive *l, const struct RINoteInEvent *ev) {
    struct RINoteLiveSlot *s;
    if (!l || !ev)
        return 1;
    /* A NOTE THAT DID NOT RESOLVE IS NOT THIS RING'S BUSINESS. `ri_notein_
     * note` refuses a drum release, an unmapped GM note and an unclaimed
     * channel, and a refused note must not become a NOTE_ON here: `ev.dev`
     * is left at 0 on the way out, so without this check a refused drum
     * release would be queued as a 303A strike -- the exact note the
     * player is trying to stop.
     *
     * So the caller says whether the note resolved. That is a boolean the
     * caller already has, and inferring it from the event's fields would be
     * guessing at the difference between "resolved to 303A" and "never
     * resolved", which are the same numbers. */
    if (!ev->resolved) {
        l->dropped++;
        return 1;
    }
    /* THE FOUR DEVICES, NO MORE. A resolved note names a device R5a already
     * checked, but this ring is reachable from anywhere, so it checks. */
    if (ev->dev >= RI_NOTEIN_DEV_COUNT) {
        l->dropped++;
        return 1;
    }
    /* A 303 RELEASE IS A NOTE_OFF EVENT AND THIS RING DOES NOT CARRY ONE.
     * Refused rather than queued as a note-on, which would re-strike the
     * note the user is releasing. Recorded as a known gap in the ledger:
     * a real 303 needs its gate released, and closing that is R5c. */
    if (!ev->on &&
            (ev->dev == RI_NOTEIN_DEV_303A || ev->dev == RI_NOTEIN_DEV_303B)) {
        l->dropped++;
        return 1;
    }
    if (l->queued == RI_NOTEIN_LIVE_CAP) {
        /* OVERWRITE THE OLDEST. A flood must not grow a heap, and the
         * newest hit is the one the player is listening to; a note from a
         * buffer ago is the one that can be dropped and counted. */
        l->ring[l->head].dev = ev->dev;
        l->ring[l->head].voice = (uint8_t)ev->inst;
        l->ring[l->head].value = (uint8_t)ev->pitch;
        l->ring[l->head].flags = (uint8_t)((ev->accent ? RI_EVFLAG_ACCENT : 0u) |
            (ev->slide ? RI_EVFLAG_SLIDE : 0u));
        l->head = (l->head + 1u) % RI_NOTEIN_LIVE_CAP;
        l->dropped++;
        return 0;   /* the newest still went in */
    }
    s = &l->ring[(l->head + l->queued) % RI_NOTEIN_LIVE_CAP];
    s->dev = ev->dev;
    /* THE DRUM LANE GOES IN `voice`, NOT IN `value`. That is how the
     * scheduler's `ev->voice = ev->value = lane` arrives and how the
     * engine reads it (`lane = ev->voice`); putting it in `value` alone
     * would leave the engine's lane check reading 0 and every drum would
     * be the bass drum. */
    s->voice = (ev->dev == RI_NOTEIN_DEV_303A || ev->dev == RI_NOTEIN_DEV_303B)
        ? 0u : ev->inst;
    /* THE 303 PITCH GOES IN `value`, WHICH IS WHERE rb303_note READS IT. */
    s->value = ev->pitch;
    s->flags = (uint8_t)((ev->accent ? RI_EVFLAG_ACCENT : 0u) |
        (ev->slide ? RI_EVFLAG_SLIDE : 0u));
    l->queued++;
    return 0;
}

uint32_t ri_notein_pump(struct RINoteLive *l, struct RIEngine *e,
    struct RIEvent *out, uint32_t cap) {
    uint32_t n = 0u, i, seq;
    if (!l || !e || !out || cap == 0u)
        return 0u;
    /* STAMPED FROM THE CURSOR, NOT FROM "NOW". The drain happens between
     * audio buffers; the engine's cursor is the only position that is
     * about to be rendered. An event stamped anywhere else lands in the gap
     * between buffers, where the cursor will never be, and the note is
     * silent -- which is the whole failure this avoids. */
    seq = e->cursor;
    for (i = 0u; i < l->queued && n < cap; i++) {
        const struct RINoteLiveSlot *s =
            &l->ring[(l->head + i) % RI_NOTEIN_LIVE_CAP];
        memset(&out[n], 0, sizeof out[n]);
        out[n].type = RI_EV_NOTE_ON;
        out[n].device = s->dev;
        out[n].voice = s->voice;
        out[n].value = s->value;
        out[n].flags = s->flags;
        out[n].sample = e->cursor;
        /* SEQ IS THE CURSOR, NOT A RUNNING COUNT. The §8 key ends in
         * `seq`, so two live notes at one cursor must have distinct values
         * or their order is whatever the array happens to say -- and the
         * engine STOPS on an out-of-order event (t205). */
        out[n].seq = seq + i;
        n++;
    }
    if (n == 0u)
        return 0u;
    l->head = (l->head + n) % RI_NOTEIN_LIVE_CAP;
    l->queued -= n;
    l->pumped += n;
    /* RETURNED IN ARRIVAL ORDER, which is already §8 order for one cursor
     * because every event here shares a sample and differs only in `seq`,
     * which was assigned ascending. Sorting it again would be a second
     * answer to a question this already answered. */
    return n;
}
