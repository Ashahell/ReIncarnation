/* t212_notein_app — R5b: the app actually plays the note.
 *
 * R5a (`midi_io/note_in.c`) resolves a MIDI message into "device 2, lane 0,
 * accent 0". **Nothing called it.** This is the wiring: a resolved note has
 * to become an engine event on the right device, at the right moment, in
 * the right order.
 *
 * THE ENGINE IS DRIVEN BY EVENTS, NOT BY A SECOND NOTE PATH. `RI_EV_NOTE_ON`
 * already reaches `rb303_note(v, pitch, slide, accent)` for devices 0 and 1
 * and `rb808_trigger`/`rb909_trigger` for 2 and 3. So the note tap for a
 * live note is a real `RIEvent`, not a new call into the voices: a second
 * path into the machines would be a second answer to "when does a 303
 * sound", and it would be wrong in a way nothing would report.
 *
 * **THE EVENT IS STAMPED AT THE NEXT RENDER CURSOR, NOT AT "now".** MIDI
 * arrives in a drain between audio buffers; the engine is mid-song. Handing
 * it an event at the drain's own sample position would place notes in the
 * gap between buffers, where the cursor will never be. So the live tap
 * queues, and `ri_notein_pump` stamps from the engine's cursor at the moment
 * it is called.
 *
 * **THE ORDER IS THE §8 KEY, INCLUDING `seq`.** Two live notes in one drain
 * must not be reordered, and the engine STOPS on an out-of-order event
 * (t205) -- so a live note with a stale `seq` is a truncated song, not a
 * cosmetic difference.
 *
 * **A LIVE NOTE DOES NOT DISTURB THE SONG.** It goes in its own ring beside
 * the pattern's events rather than being merged into them, so a note the
 * user plays cannot renumber or overwrite a scheduled one. The two are
 * merged at the cursor, where both are already positioned.
 *
 * **AN UNCLAIMED OR UNRESOLVABLE NOTE IS DROPPED HERE AND COUNTED, NOT
 * QUEUED.** Queuing a note that will never resolve costs a slot and hides
 * the fault; the count is what makes it visible.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "midi_io/note_in.h"
#include "midi_io/note_in_app.h"

static struct RIEngine E;
static struct RINoteIn NI;
static struct RINoteLive LIVE;
static struct RIEvent OUT[16];

static void setup(void) {
    ri_engine_init(&E);
    ri_engine_defaults(&E);
    ri_notein_init(&NI);
    ri_notein_live_init(&LIVE);
}

static void bind_all(void) {
    ri_notein_bind(&NI, 1u, RI_NOTEIN_DEV_303A);
    ri_notein_bind(&NI, 2u, RI_NOTEIN_DEV_303B);
    ri_notein_bind(&NI, 10u, RI_NOTEIN_DEV_808);
    ri_notein_bind(&NI, 11u, RI_NOTEIN_DEV_909);
}

int main(void) {
    uint32_t got;

    setup();
    bind_all();

    /* --- 1. A RESOLVED NOTE BECOMES AN ENGINE EVENT ------------------ */
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 10u; ev.note = 36u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "resolved");
        RI_ASSERT(ri_notein_push(&LIVE, &ev) == 0u, "and pushed");
        RI_ASSERT(LIVE.queued == 1u, "one queued (%u)", (unsigned)LIVE.queued);
        /* PUMPED AT THE CURSOR, which starts at 0. */
        got = ri_notein_pump(&LIVE, &E, OUT, 16u);
        RI_ASSERT(got == 1u, "pumped one (%u)", (unsigned)got);
        RI_ASSERT(OUT[0].type == RI_EV_NOTE_ON, "a NOTE_ON (%u)",
            (unsigned)OUT[0].type);
        RI_ASSERT(OUT[0].device == 2u, "on the 808 (%u)", (unsigned)OUT[0].device);
        /* THE DRUM PATH KEYS THE LANE IN `voice`, not in `value`: that is
         * how `ev->voice = ev->value = lane` arrives from the scheduler, and
         * the engine reads `ev->voice`. */
        RI_ASSERT(OUT[0].voice == 0u, "lane 0 in voice (%u)",
            (unsigned)OUT[0].voice);
        RI_ASSERT(OUT[0].sample == 0u, "at the cursor (%llu)",
            (unsigned long long)OUT[0].sample);
        RI_ASSERT(LIVE.queued == 0u, "and the queue is empty (%u)",
            (unsigned)LIVE.queued);
    }

    /* --- 2. THE 303 PUTS THE PITCH IN `value` AND THE FLAGS IN FLAGS - */
    {
        struct RINoteInEvent ev;
        setup();
        bind_all();
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 60u; ev.vel = 127u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "resolved");
        /* Slide it, so the flag has to survive the crossing. */
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 64u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "and the overlapping one");
        RI_ASSERT(ev.slide == 1u, "which is a slide");
        ri_notein_push(&LIVE, &ev);
        got = ri_notein_pump(&LIVE, &E, OUT, 16u);
        RI_ASSERT(got == 1u, "pumped (%u)", (unsigned)got);
        RI_ASSERT(OUT[0].device == 0u, "on the 303A (%u)", (unsigned)OUT[0].device);
        RI_ASSERT(OUT[0].value == 64u, "with the PITCH in value (%u)",
            (unsigned)OUT[0].value);
        /* THE SLIDE AND ACCENT FLAGS CROSS AS ENGINE FLAGS. `rb303_note`
         * reads exactly these two bits, and re-deriving them here would be
         * a second place where a flag means something. */
        RI_ASSERT((OUT[0].flags & RI_EVFLAG_SLIDE) != 0u,
            "the slide survives (flags %u)", (unsigned)OUT[0].flags);
    }

    /* --- 3. THE 909 GETS ITS OWN VOICE NUMBER ------------------------ */
    /* Lane 6 is the 808's low cowbell and the 909's low ride. If the live
     * path passed the raw lane to the engine it would play the wrong voice
     * on one of them -- R5a fixed this in `ri_notein_voice` and this is
     * where that value is actually used. */
    {
        struct RINoteInEvent ev;
        uint32_t lane6 = ri_notein_lookup(56u);   /* GM cowbell */
        setup();
        bind_all();
        RI_ASSERT(lane6 == 6u, "the cowbell is lane 6 (%u)", (unsigned)lane6);
        memset(&ev, 0, sizeof ev);
        ev.ch = 11u; ev.note = 56u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "resolved");
        ri_notein_push(&LIVE, &ev);
        got = ri_notein_pump(&LIVE, &E, OUT, 16u);
        RI_ASSERT(got == 1u, "pumped (%u)", (unsigned)got);
        RI_ASSERT(OUT[0].device == 3u, "on the 909 (%u)", (unsigned)OUT[0].device);
        /* THE LIVE PATH CARRIES THE LANE, NOT THE 909 VOICE: the engine
         * does `voice = RI_LANE_TO_RB909_VOICE[lane]` itself, and doing it
         * here too would be the same map in two places. */
        RI_ASSERT(OUT[0].voice == lane6, "carrying the LANE (%u)",
            (unsigned)OUT[0].voice);
    }

    /* --- 4. A DRUM NOTE-OFF NEVER BECOMES AN EVENT ------------------- */
    {
        struct RINoteInEvent ev;
        setup();
        bind_all();
        memset(&ev, 0, sizeof ev);
        ev.ch = 10u; ev.note = 36u; ev.on = 0u;
        RI_ASSERT(ri_notein_note(&NI, &ev) != 0u, "refused by R5a");
        RI_ASSERT(ri_notein_push(&LIVE, &ev) != 0u, "and refused here");
        RI_ASSERT(LIVE.queued == 0u, "nothing queued (%u)",
            (unsigned)LIVE.queued);
        RI_ASSERT(LIVE.dropped == 1u, "but DROPPED AND COUNTED (%u)",
            (unsigned)LIVE.dropped);
    }

    /* --- 5. AN UNRESOLVED NOTE IS NOT QUEUED ------------------------- */
    setup();
    bind_all();
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 10u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) != 0u, "GM 60 is no instrument");
        RI_ASSERT(ri_notein_push(&LIVE, &ev) != 0u, "so it is not queued");
        RI_ASSERT(LIVE.queued == 0u, "the ring stays empty (%u)",
            (unsigned)LIVE.queued);
    }

    /* --- 6. THE RING IS BOUNDED AND OVERWRITES OLDEST --------------- */
    /* A flood must not grow a heap. The oldest goes, because a stuck note
     * from a minute ago is worse to keep than the newest hit. */
    {
        struct RINoteInEvent ev;
        uint32_t i;
        setup();
        bind_all();
        for (i = 0u; i < RI_NOTEIN_LIVE_CAP + 4u; i++) {
            memset(&ev, 0, sizeof ev);
            ev.ch = 10u; ev.note = 36u; ev.vel = (uint8_t)(i & 127u);
            ev.on = 1u;
            ri_notein_note(&NI, &ev);
            ri_notein_push(&LIVE, &ev);
        }
        RI_ASSERT(LIVE.queued == RI_NOTEIN_LIVE_CAP,
            "the ring is bounded at %u (%u)", (unsigned)RI_NOTEIN_LIVE_CAP,
            (unsigned)LIVE.queued);
        RI_ASSERT(LIVE.dropped >= 4u, "and the overflow was counted (%u)",
            (unsigned)LIVE.dropped);
        /* A PUMP OF A SHORT OUTPUT IS SAFE AND LOSES NOTHING: it returns
         * what fitted and leaves the rest, unlike the earlier R8e stem
         * buffer that silently refused everything. */
        got = ri_notein_pump(&LIVE, &E, OUT, 4u);
        RI_ASSERT(got == 4u, "four into four (%u)", (unsigned)got);
        RI_ASSERT(LIVE.queued == RI_NOTEIN_LIVE_CAP - 4u,
            "and the rest waits (%u)", (unsigned)LIVE.queued);
    }

    /* --- 7. A FULL-RING PUMP WITH NO ROOM COUNTS AND KEEPS ---------- */
    setup();
    bind_all();
    {
        struct RINoteInEvent ev;
        uint32_t i;
        for (i = 0u; i < RI_NOTEIN_LIVE_CAP; i++) {
            memset(&ev, 0, sizeof ev);
            ev.ch = 10u; ev.note = 36u; ev.vel = 100u; ev.on = 1u;
            ri_notein_note(&NI, &ev);
            ri_notein_push(&LIVE, &ev);
        }
        got = ri_notein_pump(&LIVE, &E, OUT, 0u);
        RI_ASSERT(got == 0u, "no room, nothing pumped (%u)", (unsigned)got);
        RI_ASSERT(LIVE.queued == RI_NOTEIN_LIVE_CAP,
            "and the ring is untouched (%u)", (unsigned)LIVE.queued);
    }

    /* --- 7b. THE STAMP IS THE CURSOR, NOT ZERO ----------------------- */
    /* PE2 SURVIVED because every case pumped at cursor 0 -- and the
     * cursor starts at 0, so "stamp at the cursor" and "stamp at zero" are
     * the same number until the song has played. The whole point of
     * stamping from the cursor is that MIDI arrives BETWEEN buffers, so
     * this has to be checked once the engine has actually moved. */
    setup();
    bind_all();
    {
        static struct RIEvent EV2[8];
        static float L[64], R[64];
        struct RINoteInEvent ev;
        uint32_t i;
        memset(&ev, 0, sizeof ev);
        ev.ch = 10u; ev.note = 36u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "resolved");
        ri_notein_push(&LIVE, &ev);
        /* Queue a SECOND note BEFORE the first pump. My first version pumped
         * between them, so the second pump had one note left and `OUT[0]`
         * was stale from the first pump -- which then read as "the snare
         * came out first". */
        memset(&ev, 0, sizeof ev);
        ev.ch = 10u; ev.note = 38u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "and a second note");
        ri_notein_push(&LIVE, &ev);
        /* PLAY SOMETHING, so the cursor is no longer 0. */
        E.cursor = 4096u;
        got = ri_notein_pump(&LIVE, &E, OUT, 16u);
        RI_ASSERT(got == 2u, "both pumped (%u)", (unsigned)got);
        RI_ASSERT(OUT[0].sample == 4096u && OUT[1].sample == 4096u,
            "the event is stamped at the CURSOR, 4096, not at zero (%llu)",
            (unsigned long long)OUT[0].sample);
        /* TWO NOTES, ONE CURSOR. Each takes its own seq: the engine STOPS
         * on an out-of-order event (t205), so two events at one sample
         * sharing a seq have no defined order. */
        RI_ASSERT(OUT[0].seq != OUT[1].seq,
            "distinct seq at one cursor (%u and %u)", (unsigned)OUT[0].seq,
            (unsigned)OUT[1].seq);
        RI_ASSERT(OUT[0].seq < OUT[1].seq,
            "and ascending, which is arrival order");
        /* Arrival order is what it claims to be: the note pushed first
         * comes out first. */
        RI_ASSERT(OUT[0].voice < OUT[1].voice,
            "the bass drum (lane 0) before the snare (lane 1): %u then %u",
            (unsigned)OUT[0].voice, (unsigned)OUT[1].voice);
        (void)EV2; (void)L; (void)R; (void)i;
    }

    /* --- 7c. A 303 RELEASE IS REFUSED, NOT RESTRUCK ------------------ */
    /* PJ2 SURVIVED because no test pushed one. Queuing a release as a
     * NOTE_ON would re-strike the note the player is lifting their finger
     * off -- the single most audible thing this module could get wrong. */
    setup();
    bind_all();
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "the note-on resolves");
        ri_notein_push(&LIVE, &ev);
        RI_ASSERT(LIVE.queued == 1u, "and queues (%u)", (unsigned)LIVE.queued);
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 60u; ev.on = 0u;
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u,
            "the release resolves in R5a (the gate is known)");
        RI_ASSERT(ri_notein_push(&LIVE, &ev) != 0u,
            "but this ring REFUSES it: a 303 needs a NOTE_OFF event, and"
            " queueing it as a note-on would re-strike the note");
        RI_ASSERT(LIVE.queued == 1u, "so only the note-on waits (%u)",
            (unsigned)LIVE.queued);
        RI_ASSERT(LIVE.dropped == 1u, "and the release is counted (%u)",
            (unsigned)LIVE.dropped);
    }

    /* --- 8. NULLs ----------------------------------------------------- */
    ri_notein_live_init(0);   /* must not crash */
    RI_ASSERT(ri_notein_push(0, 0) != 0, "NULL push");
    RI_ASSERT(ri_notein_pump(0, &E, OUT, 16u) == 0u, "NULL pump");
    RI_ASSERT(ri_notein_pump(&LIVE, 0, OUT, 16u) == 0u, "NULL engine");
    RI_ASSERT(ri_notein_pump(&LIVE, &E, 0, 16u) == 0u, "NULL out");
    {
        struct RINoteInEvent ev;
        memset(&ev, 0, sizeof ev);
        ev.ch = 1u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;
        ri_notein_bind(&NI, 1u, RI_NOTEIN_DEV_303A);
        RI_ASSERT(ri_notein_note(&NI, &ev) == 0u, "resolved");
        RI_ASSERT(ri_notein_push(&LIVE, 0) != 0, "NULL event");
    }

    RI_RESULT("note-in-app");
}