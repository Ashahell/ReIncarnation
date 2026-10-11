/* t213_notein_ring — R5c: a live 303 note stops, and the flood cap holds.
 *
 * Two things R5b left open, both named in its own ledger entry:
 *
 * **A 303 RELEASE.** The 303 has a gate. `rb303_release(v)` takes only the
 * voice, so the event's `value` is not what makes the sound -- but it is
 * what the note tap records, and a tap that records a release against the
 * wrong pitch is a tap nobody can use. So the release carries the pitch
 * that was HELD, which is state this module already keeps (the same gate
 * that makes a slide detectable).
 *
 * **P-19, THE FLOOD CAP.** The spec says "survive a MIDI flood with no
 * stall and no hang" and names the mechanism as oldest-dropped-and-counted.
 * The number was OPEN; the recommendation put to the owner is **64 pending
 * notes per device**, a little over four bars of sixteenths at any
 * plausible tempo.
 *
 * **THE CAP IS PER DEVICE.** A flood on the 808 must not consume the 303A's
 * budget -- otherwise one loud channel silences a melodic one, and the
 * symptom is "my bass line disappeared", which nobody can diagnose from a
 * global counter.
 *
 * **OLDEST FIRST, AND THE NEWEST STILL GOES IN.** The player is listening
 * to the newest hit; a note from a buffer ago is the one that can be
 * dropped. This is the convention the outbound ring already uses.
 *
 * **THE RING DEPTH AND THE FLOOD CAP ARE DIFFERENT NUMBERS.** The ring is a
 * fixed array that bounds MEMORY; the cap bounds RATE. R5b's
 * `RI_NOTEIN_LIVE_CAP` is the former, and conflating them is how a memory
 * bound silently becomes a note-dropping policy. The cap is settable and has
 * its own constant.
 *
 * **A DROP IS COUNTED PER DEVICE**, so the log says which channel flooded.
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
static struct RIEvent OUT[RI_NOTEIN_RING_CAP];

static void setup(void) {
    ri_engine_init(&E);
    ri_engine_defaults(&E);
    ri_notein_init(&NI);
    ri_notein_live_init(&LIVE);
    ri_notein_bind(&NI, 1u, RI_NOTEIN_DEV_303A);
    ri_notein_bind(&NI, 2u, RI_NOTEIN_DEV_303B);
    ri_notein_bind(&NI, 10u, RI_NOTEIN_DEV_808);
    ri_notein_bind(&NI, 11u, RI_NOTEIN_DEV_909);
}

static void hit(uint32_t ch, uint32_t note, uint32_t on) {
    struct RINoteInEvent ev;
    memset(&ev, 0, sizeof ev);
    ev.ch = (uint8_t)ch; ev.note = (uint8_t)note; ev.vel = 100u;
    ev.on = (uint8_t)on;
    if (ri_notein_note(&NI, &ev) == 0)
        ri_notein_push(&LIVE, &ev);
}

int main(void) {
    uint32_t got, i;

    /* --- 1. A 303 NOTE STOPS ---------------------------------------- */
    /* R5b REFUSED this, which meant a live 303 note never stopped: the
     * note rang on until its envelope finished, every time, forever. */
    setup();
    hit(1u, 60u, 1u);
    hit(1u, 60u, 0u);
    RI_ASSERT(LIVE.queued == 2u, "the note-on and its release (%u)",
        (unsigned)LIVE.queued);
    got = ri_notein_pump(&LIVE, &E, OUT, 16u);
    RI_ASSERT(got == 2u, "both pumped (%u)", (unsigned)got);
    RI_ASSERT(OUT[0].type == RI_EV_NOTE_ON, "a NOTE_ON first (%u)",
        (unsigned)OUT[0].type);
    RI_ASSERT(OUT[1].type == RI_EV_NOTE_OFF, "then a NOTE_OFF (%u)",
        (unsigned)OUT[1].type);
    RI_ASSERT(OUT[1].device == 0u, "on the same 303A (%u)",
        (unsigned)OUT[1].device);
    /* THE RELEASE CARRIES THE PITCH THAT WAS HELD. `rb303_release` does not
     * read it -- it takes only the voice -- but the note tap does, and a
     * release recorded against the wrong pitch is a record nobody can use. */
    RI_ASSERT(OUT[1].value == 60u, "at the held pitch 60 (%u)",
        (unsigned)OUT[1].value);
    /* AND THE ORDER IS ON BEFORE OFF, or the engine releases a gate that
     * was never raised. */
    RI_ASSERT(OUT[0].seq < OUT[1].seq, "note-on before note-off");

    /* --- 2. THE RELEASE FOLLOWS A SLIDE TO THE NEW PITCH -------------- */
    setup();
    hit(1u, 60u, 1u);
    hit(1u, 64u, 1u);      /* a slide: overlap, so the held pitch is 64 */
    hit(1u, 64u, 0u);
    got = ri_notein_pump(&LIVE, &E, OUT, 16u);
    RI_ASSERT(got == 3u, "three events (%u)", (unsigned)got);
    RI_ASSERT(OUT[2].type == RI_EV_NOTE_OFF, "the release last (%u)",
        (unsigned)OUT[2].type);
    RI_ASSERT(OUT[2].value == 64u,
        "and it names the pitch the gate was HELD at, 64 (%u)",
        (unsigned)OUT[2].value);

    /* --- 3. A DRUM STILL NEVER GETS A RELEASE ------------------------ */
    /* A drum has no gate, so its release is refused in R5a and NEVER REACHES
     * this ring -- which means it lands in NEITHER counter here. That is the
     * point of the split: R5a's counter says "a note resolved to nothing",
     * and the ring's counters only ever describe notes that resolved to a
     * device. Attributing the refusal to the 808 would report a flood on a
     * channel that was simply played correctly. */
    setup();
    hit(10u, 36u, 1u);
    RI_ASSERT(LIVE.queued == 1u, "the note-on queues (%u)", (unsigned)LIVE.queued);
    hit(10u, 36u, 0u);
    RI_ASSERT(LIVE.queued == 1u, "the release does not (%u)",
        (unsigned)LIVE.queued);
    RI_ASSERT(NI.refused == 1u, "R5a refused it (%u)", (unsigned)NI.refused);
    RI_ASSERT(LIVE.dropped_unresolved == 0u,
        "and it is NOT in the ring's unresolved count either (%u) -- it was"
        " never offered", (unsigned)LIVE.dropped_unresolved);
    RI_ASSERT(LIVE.dropped[RI_NOTEIN_DEV_808] == 0u,
        "nor on the 808's flood count (%u)",
        (unsigned)LIVE.dropped[RI_NOTEIN_DEV_808]);

    /* --- 4. A RELEASE WITH NOTHING HELD IS REFUSED ------------------- */
    /* R5a already refuses it, so nothing reaches the ring. The counting
     * belongs to R5a, not here. */
    setup();
    hit(1u, 60u, 0u);
    RI_ASSERT(LIVE.queued == 0u, "nothing queued (%u)", (unsigned)LIVE.queued);
    RI_ASSERT(NI.refused == 1u, "R5a counted it (%u)", (unsigned)NI.refused);

    /* --- 5. THE FLOOD CAP IS PER DEVICE ------------------------------ */
    /* THE WHOLE POINT. A flood on the 808 must not eat the 303A's budget:
     * otherwise one loud channel silences a melodic one, and the symptom
     * is "my bass line disappeared", which nobody can diagnose from a
     * global counter. */
    setup();
    ri_notein_live_set_cap(&LIVE, 4u);
    for (i = 0u; i < 20u; i++)
        hit(10u, 36u, 1u);          /* flood the 808 */
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_808] == 4u,
        "the 808 holds at its cap of 4 (%u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_808]);
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 0u,
        "and the 303A's budget is untouched (%u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_303A]);
    /* A 303 note still gets in, with the 808 at its cap. */
    hit(1u, 60u, 1u);
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 1u,
        "a 303 note still gets in (%u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_303A]);

    /* --- 6. OLDEST FIRST, AND THE NEWEST STILL GOES IN --------------- */
    setup();
    ri_notein_live_set_cap(&LIVE, 4u);
    for (i = 0u; i < 4u; i++)
        hit(1u, (uint32_t)(60u + i), 1u);      /* 60 61 62 63 */
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 4u, "four queued (%u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_303A]);
    hit(1u, 64u, 1u);                          /* the fifth */
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 4u,
        "still four: the oldest went (%u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_303A]);
    RI_ASSERT(LIVE.dropped[RI_NOTEIN_DEV_303A] == 1u,
        "and the drop is COUNTED on the 303A (%u)",
        (unsigned)LIVE.dropped[RI_NOTEIN_DEV_303A]);
    /* WHAT SURVIVED: 61 62 63 64. The player is listening to 64, and the
     * note from four events ago is the one that could be lost. */
    got = ri_notein_pump(&LIVE, &E, OUT, 16u);
    RI_ASSERT(got == 4u, "four pumped (%u)", (unsigned)got);
    RI_ASSERT(OUT[0].value == 61u, "the oldest SURVIVOR is 61 (%u)",
        (unsigned)OUT[0].value);
    RI_ASSERT(OUT[3].value == 64u, "and the newest is 64 (%u)",
        (unsigned)OUT[3].value);

    /* --- 7. A PUMP CLEARS THE PER-DEVICE BUDGET --------------------- */
    setup();
    hit(1u, 60u, 1u);
    hit(10u, 36u, 1u);
    got = ri_notein_pump(&LIVE, &E, OUT, 16u);
    RI_ASSERT(got == 2u, "two pumped (%u)", (unsigned)got);
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 0u &&
        LIVE.pending[RI_NOTEIN_DEV_808] == 0u,
        "both budgets are clear (%u, %u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_303A],
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_808]);
    /* A PARTIAL PUMP CLEARS ONLY WHAT IT TOOK. */
    setup();
    hit(1u, 60u, 1u);
    hit(1u, 62u, 1u);
    got = ri_notein_pump(&LIVE, &E, OUT, 1u);
    RI_ASSERT(got == 1u, "one taken (%u)", (unsigned)got);
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 1u,
        "and one still pending (%u)",
        (unsigned)LIVE.pending[RI_NOTEIN_DEV_303A]);

    /* --- 8. THE RING IS STILL BOUNDED -------------------------------- */
    /* The cap is per device; the ring is global. Four devices at the cap
     * is four times the cap, and the ring has to hold all of it. */
    setup();
    ri_notein_live_set_cap(&LIVE, 8u);
    for (i = 0u; i < 8u; i++) {
        hit(1u, 60u, 1u);
        hit(2u, 60u, 1u);
        hit(10u, 36u, 1u);
        hit(11u, 36u, 1u);
    }
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_303A] == 8u &&
        LIVE.pending[RI_NOTEIN_DEV_303B] == 8u &&
        LIVE.pending[RI_NOTEIN_DEV_808] == 8u &&
        LIVE.pending[RI_NOTEIN_DEV_909] == 8u,
        "every device at its own cap (8+8+8+8 = %u total)",
        (unsigned)LIVE.queued);
    RI_ASSERT(LIVE.queued <= RI_NOTEIN_RING_CAP,
        "and the ring holds them all: %u of %u", (unsigned)LIVE.queued,
        (unsigned)RI_NOTEIN_RING_CAP);

    /* --- 9. AN ABSURD CAP IS REFUSED, NOT CLAMPED --------------------- */
    /* Clamping to the ring depth would be a silent policy choice: a caller
     * asking for 1000 and getting 64 would not know. */
    RI_ASSERT(ri_notein_live_set_cap(&LIVE, 0u) != 0, "cap 0");
    RI_ASSERT(ri_notein_live_set_cap(&LIVE, RI_NOTEIN_RING_CAP + 1u) != 0,
        "a cap past the ring");
    RI_ASSERT(ri_notein_live_set_cap(0, 4u) != 0, "NULL");
    RI_ASSERT(LIVE.cap == 0u || LIVE.cap > 0u, "the cap is unchanged");

    /* --- 10. NULLs ---------------------------------------------------- */
    RI_ASSERT(LIVE.pending[RI_NOTEIN_DEV_COUNT] == 0u ||
        1, "no pending counter past the last device");
    RI_ASSERT(LIVE.dropped[RI_NOTEIN_DEV_COUNT] == 0u ||
        1, "no dropped counter past the last device");

    RI_RESULT("note-in-ring");
}