/* midi_io/note_in_app.c — R5b/R5c. See note_in_app.h for the laws. */
#include <string.h>
#include "engine/seq/sched.h"   /* RI_EV_*, RI_EVFLAG_* */
#include "midi_io/note_in_app.h"

void ri_notein_live_init(struct RINoteLive *l) {
    if (!l)
        return;
    memset(l, 0, sizeof *l);
    l->cap = RI_NOTEIN_FLOOD_CAP;
}

int ri_notein_live_set_cap(struct RINoteLive *l, uint32_t cap) {
    if (!l || cap == 0u || cap > RI_NOTEIN_RING_CAP)
        return 1;   /* unchanged: a bad value must not leave a session
                     * with no cap at all */
    l->cap = cap;
    return 0;
}

/* THE OLDEST SLOT HOLDING THIS DEVICE, or `cap` when there is none.
 *
 * The scan is bounded by the ring depth, and a note arrives at most a few
 * thousand times a second, so 64 comparisons is not a cost worth avoiding
 * -- and it is the honest way to honour "drop the OLDEST of THIS device"
 * rather than "drop the oldest overall", which under a per-device budget
 * would empty the wrong channel. */
static uint32_t oldest_of_device(const struct RINoteLive *l, uint32_t dev) {
    uint32_t i;
    for (i = 0u; i < l->queued; i++) {
        const struct RINoteLiveSlot *s =
            &l->ring[(l->head + i) % RI_NOTEIN_RING_CAP];
        if (s->dev == dev)
            return i;
    }
    return l->queued;
}

static void slot_set(struct RINoteLiveSlot *s, const struct RINoteInEvent *ev) {
    s->dev = ev->dev;
    s->type = ev->on ? RI_EV_NOTE_ON : RI_EV_NOTE_OFF;
    /* THE DRUM LANE GOES IN `voice`, NOT IN `value`. That is how the
     * scheduler's `ev->voice = ev->value = lane` arrives and how the engine
     * reads it (`lane = ev->voice`); putting it in `value` alone would
     * leave the engine's lane check reading 0 and every drum would be the
     * bass drum. */
    s->voice = (ev->dev == RI_NOTEIN_DEV_303A || ev->dev == RI_NOTEIN_DEV_303B)
        ? 0u : ev->inst;
    /* THE 303 PITCH GOES IN `value`, WHICH IS WHERE rb303_note READS IT --
     * and for a RELEASE it is the pitch the gate was HELD at, which
     * `ri_notein_note` already resolved. `rb303_release` takes only the
     * voice, but the note tap records the value, and a release recorded
     * against the wrong pitch is a record nobody can use. */
    s->value = ev->pitch;
    s->flags = (uint8_t)((ev->accent ? RI_EVFLAG_ACCENT : 0u) |
        (ev->slide ? RI_EVFLAG_SLIDE : 0u));
}

int ri_notein_push(struct RINoteLive *l, const struct RINoteInEvent *ev) {
    uint32_t at;
    if (!l || !ev)
        return 1;
    /* A NOTE THAT DID NOT RESOLVE IS NOT THIS RING'S BUSINESS. `ri_notein_
     * note` refuses a drum release, an unmapped GM note and an unclaimed
     * channel, and a refused note must not become an event here: `ev.dev`
     * is left at 0, which is 303A, so without this check a refused drum
     * release would be queued as a 303A strike -- the exact note the
     * player is trying to stop.
     *
     * The caller says whether the note resolved, rather than this module
     * inferring it: "resolved to 303A" and "never resolved" are the same
     * numbers. */
    if (!ev->resolved) {
        l->dropped_unresolved++;
        return 1;
    }
    if (ev->dev >= RI_NOTEIN_DEV_COUNT) {
        l->dropped_unresolved++;
        return 1;
    }
    /* A DRUM HAS NO GATE TO RELEASE, so a drum release is still refused
     * (R5c). The 303 release is NOT refused: R5b refused it, and the
     * consequence was a live 303 note that never stopped. */
    if (0) {
        l->dropped[ev->dev]++;
        return 1;
    }

    /* AT THE DEVICE'S CAP: DROP ITS OLDEST, ADMIT THE NEWEST. The cap is
     * per device, so a flood on the 808 cannot consume the 303A's budget --
     * otherwise one loud channel silences a melodic one, and the symptom is
     * "my bass line disappeared". */
    if (l->pending[ev->dev] >= l->cap) {
        uint32_t k = oldest_of_device(l, ev->dev);
        if (k >= l->queued) {
            /* No slot of this device found, which cannot happen when the
             * pending count is accurate -- and if it ever does, refusing is
             * better than writing past the ring. */
            l->dropped[ev->dev]++;
            return 1;
        }
        /* Shift the tail down one and drop the head of that device. */
        {
            uint32_t i;
            for (i = k; i + 1u < l->queued; i++)
                l->ring[(l->head + i) % RI_NOTEIN_RING_CAP] =
                    l->ring[(l->head + i + 1u) % RI_NOTEIN_RING_CAP];
        }
        l->queued--;
        /* AND THE DEVICE BUDGET COMES DOWN WITH IT. The slot removed from
         * the ring WAS this device's pending note, so not decrementing here
         * is how pending[dev] ends up climbing past the cap forever while
         * the ring stays the right size. */
        l->pending[ev->dev]--;
        l->dropped[ev->dev]++;
    }
    if (l->queued >= RI_NOTEIN_RING_CAP) {
        /* The ring is full across ALL devices, which with a per-device cap
         * can only happen if the caps were raised to the ring depth and
         * every device is at it. Refuse rather than overwrite: which note
         * to lose is not this module's choice to make silently. */
        l->dropped[ev->dev]++;
        return 1;
    }
    at = (l->head + l->queued) % RI_NOTEIN_RING_CAP;
    slot_set(&l->ring[at], ev);
    l->queued++;
    l->pending[ev->dev]++;
    return 0;
}

uint32_t ri_notein_pump(struct RINoteLive *l, uint64_t stamp, uint32_t seq,
    struct RIEvent *out, uint32_t cap) {
    uint32_t n = 0u, i;
    if (!l || !out || cap == 0u)
        return 0u;
    /* THE STAMP IS THE CALLER'S WINDOW ORIGIN, not "now" and not the
     * engine's cursor. See the header: `ri_engine_load` zeroes the cursor on
     * every call and the live session calls it once per buffer, so a cursor
     * stamp is 0 forever and lands below `s0` on every buffer but the first.
     * The caller knows its window; this module does not and must not guess. */
    for (i = 0u; i < l->queued && n < cap; i++) {
        const struct RINoteLiveSlot *s =
            &l->ring[(l->head + i) % RI_NOTEIN_RING_CAP];
        memset(&out[n], 0, sizeof out[n]);
        out[n].type = s->type;
        out[n].device = s->dev;
        out[n].voice = s->voice;
        out[n].value = s->value;
        out[n].flags = s->flags;
        out[n].sample = stamp;
        /* SEQ FROM THE CALLER'S BASE, ASCENDING. The §8 key ends in `seq`,
         * so two live notes at one stamp must have distinct values or their
         * order is whatever the array happens to say -- and the engine STOPS
         * on an out-of-order event (t205). */
        out[n].seq = seq + n;
        /* THE PER-DEVICE BUDGET IS RELEASED HERE AND NOW, and only for what
         * actually went out: a partial pump leaves the rest pending, so a
         * caller that pumps every buffer does not leak budget. */
        l->pending[s->dev]--;
        n++;
    }
    if (n == 0u)
        return 0u;
    l->head = (l->head + n) % RI_NOTEIN_RING_CAP;
    l->queued -= n;
    l->pumped += n;
    /* RETURNED IN ARRIVAL ORDER, which is already §8 order for one cursor
     * because every event here shares a sample and differs only in `seq`,
     * assigned ascending. Sorting it again would be a second answer to a
     * question this already answered. */
    return n;
}
