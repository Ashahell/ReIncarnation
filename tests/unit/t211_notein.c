/* t211_notein — R5: an external sequencer plays the ReBirth voices.
 *
 * R6 (outbound) shipped long ago. This is the other half: a DAW's MIDI track
 * playing the 303s, 808 and 909 directly, one channel per device. The
 * Leviasynth already does this (channel 2, its own chart, `RI_MCHAN_LEVI`).
 *
 * OWNER DECISIONS 2026-10-11, all four taken together:
 *
 *  1. **THE SAME SHAPE AS R6f/R6e.** The drums claim their channel
 *     implicitly and are counted; the 303s are opt-in through ENVARC
 *     settings. R6e already decided this for the outbound side, and an
 *     inbound map that disagreed with it would mean a device that is silent
 *     on the way out and loud on the way in.
 *
 *  2. **GM PERCUSSION LAYOUT** for the 808 and 909. Every DAW already knows
 *     it, so a GM drum track plays correctly with no setup -- which is the
 *     whole point of choosing it.
 *
 *  3. **A SLIDE IS LEGATO FROM OVERLAP.** A DAW has no slide message, so
 *     the only signal a piano roll actually carries is a note-on arriving
 *     while the previous note is still held. This is the same information
 *     the engine's own gate state already has.
 *
 * **A CLAIMED CHANNEL IS NOT AN ANIMATED CHANNEL.** An unclaimed channel is
 * refused and counted, never defaulted: silently binding a channel nobody
 * asked for is how a remote-control message ends up playing a drum.
 *
 * **A NOTE IS RESOLVED TO A DEVICE BY THE CHANNEL, NEVER BY THE PITCH.**
 * GM percussion numbers overlap the 303's pitch range, so a note that
 * looks melodic on one channel is a drum on another, and the only thing
 * that knows which is the channel it arrived on.
 *
 * **AN ACCENT IS A VELOCITY THRESHOLD, AND THE THRESHOLD IS NAMED.** R6e
 * fixed the outbound convention (112 accented, 64 plain). Inbound is the
 * inverse and undetermined, so it is a single documented constant rather
 * than a comparison scattered through the handler.
 *
 * **UNRECOGNISED IS REFUSED AND COUNTED, NOT DROPPED.** A note that
 * resolves to nothing -- a GM number with no instrument here, a 303 pitch
 * with no gate -- is a note the caller asked for and did not get. That is
 * exactly the kind of thing that must be visible.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/note_in.h"

static struct RINoteIn N;

/* The GM percussion numbers this build speaks, as the owner chose them. */
#define GM_BD   36u
#define GM_SD   38u
#define GM_LT   41u
#define GM_MT   45u
#define GM_HT   48u
#define GM_RS   49u
#define GM_CP   54u
#define GM_CB   56u
#define GM_CY   58u
#define GM_OH   46u
#define GM_CH   42u

static void reset(void) {
    ri_notein_init(&N);
}

int main(void) {
    struct RINoteInEvent ev;

    /* --- 1. THE GM LAYOUT IS THE OWNER'S CHOICE, STATED --------------- */
    /* A complete map is a CLAIM: it says which GM numbers this build
     * speaks. A GM number with no instrument here is refused and counted
     * (law below), not silently ignored -- so the gaps in this table are
     * as much a part of the answer as the entries. */
    reset();
    RI_ASSERT(ri_notein_lookup(GM_BD) == 0u, "GM 36 is the 808 bass drum"
        " (slot %u)", (unsigned)ri_notein_lookup(GM_BD));
    RI_ASSERT(ri_notein_lookup(GM_SD) == 1u, "GM 38 is the snare");
    RI_ASSERT(ri_notein_lookup(GM_LT) == 2u, "GM 41 is the low tom");
    RI_ASSERT(ri_notein_lookup(GM_HT) == 4u, "GM 48 is the high tom");
    RI_ASSERT(ri_notein_lookup(GM_RS) == 8u, "GM 49 is the rimshot");
    RI_ASSERT(ri_notein_lookup(GM_CP) == 10u, "GM 54 is the handclap");
    RI_ASSERT(ri_notein_lookup(GM_CH) == 11u, "GM 42 is the closed hat");
    /* A GM number with no instrument in this build is NO INSTRUMENT, not
     * slot 0. Returning 0 for an unmapped note would play the bass drum
     * for a cowbell nobody asked for. */
    RI_ASSERT(ri_notein_lookup(60u) == RI_NOTEIN_NO_INSTRUMENT,
        "GM 60 has no instrument here (%u)",
        (unsigned)ri_notein_lookup(60u));
    RI_ASSERT(ri_notein_lookup(0u) == RI_NOTEIN_NO_INSTRUMENT,
        "and neither does note 0");
    RI_ASSERT(ri_notein_lookup(128u) == RI_NOTEIN_NO_INSTRUMENT,
        "a note past 127 is not an instrument (%u)",
        (unsigned)ri_notein_lookup(128u));
    /* The table is a PERMUTATION of the instruments this build has, so
     * every one of them is reachable from some GM number. A layout that
     * left the 909's ride unreachable would be a map that quietly cannot
     * play the machine. */
    {
        uint32_t n, hits;
        for (n = 0u; n < 128u; n++)
            if (ri_notein_lookup(n) != RI_NOTEIN_NO_INSTRUMENT) {
                hits = 0u;
                hits = ri_notein_instruments();
                RI_ASSERT(hits > 0u, "the map is not empty");
                break;
            }
        RI_ASSERT(ri_notein_instruments() >= 10u,
            "at least ten instruments are reachable (%u)",
            (unsigned)ri_notein_instruments());
    }

    /* --- 2. DRUMS CLAIM THEIR CHANNEL IMPLICITLY --------------------- */
    reset();
    RI_ASSERT(ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808) == 0,
        "the drum channel binds");
    RI_ASSERT(N.drums_implicit, "and it is recorded as IMPLICIT (R6e)");
    RI_ASSERT(N.drums_claimed == 1u, "counted once (%u)",
        (unsigned)N.drums_claimed);
    /* The 303s are OPT-IN: binding is not claiming, and a default would
     * collide with the remote channel. */
    reset();
    RI_ASSERT(ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A) == 0, "303A");
    RI_ASSERT(!N.drums_implicit, "a melodic bind is not implicit");
    RI_ASSERT(N.drums_claimed == 0u, "and claims no drum");

    /* --- 3. AN UNCLAIMED CHANNEL IS REFUSED, NEVER DEFAULTED ---------- */
    reset();
    RI_ASSERT(ri_notein_bind(&N, 5u, RI_NOTEIN_DEV_303B) == 0, "bind 5");
    RI_ASSERT(ri_notein_unclaimed(&N, 9u), "channel 9 was never bound");
    /* Re-binding the same device to a new channel releases the old one, so
     * a setting change cannot leave a channel live that the panel no longer
     * names. */
    RI_ASSERT(ri_notein_bind(&N, 7u, RI_NOTEIN_DEV_303B) == 0, "rebind");
    RI_ASSERT(ri_notein_unclaimed(&N, 5u), "the old channel is released");
    RI_ASSERT(!ri_notein_unclaimed(&N, 7u), "and the new one is live");

    /* --- 4. TWO DEVICES CANNOT SHARE A CHANNEL ----------------------- */
    reset();
    RI_ASSERT(ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808) == 0, "808 on 10");
    RI_ASSERT(ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_303A) != 0,
        "and a 303 cannot take the same channel");

    /* --- 5. A NOTE RESOLVES BY CHANNEL, NEVER BY PITCH ---------------- */
    /* GM percussion numbers overlap the 303's pitch range, so pitch alone
     * cannot say which machine plays it. */
    reset();
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808);
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "GM 36 on the drum channel");
    RI_ASSERT(ev.dev == RI_NOTEIN_DEV_808, "plays the 808 (%u)",
        (unsigned)ev.dev);
    RI_ASSERT(ev.inst == 0u, "at the bass drum (%u)", (unsigned)ev.inst);
    /* THE SAME PITCH ON A MELODIC CHANNEL IS A 303 NOTE. */
    reset();
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "note 36 on the 303 channel");
    RI_ASSERT(ev.dev == RI_NOTEIN_DEV_303A, "plays the 303 (%u)",
        (unsigned)ev.dev);

    /* --- 6. THE 303 TAKES THE PITCH AS IT COMES ----------------------- */
    reset();
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "any pitch is a 303 pitch");
    RI_ASSERT(ev.pitch == 60u, "unchanged (%u)", (unsigned)ev.pitch);
    RI_ASSERT(ev.slide == 0u, "and it is not a slide on its own (%u)",
        (unsigned)ev.slide);

    /* --- 7. A SLIDE IS LEGATO FROM OVERLAP (OWNER DECISION) ---------- */
    /* A note-on while the previous note is still held. The engine's own
     * gate state already knows this, so nothing extra is carried. */
    reset();
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "first note");
    RI_ASSERT(ev.slide == 0u, "a first note is not a slide");
    /* Still held: the next note-on IS a slide. */
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 64u; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "overlapping note");
    RI_ASSERT(ev.slide == 1u, "overlap is a SLIDE (%u)", (unsigned)ev.slide);
    /* Released in between: not a slide. */
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 64u; ev.on = 0u;
    ri_notein_note(&N, &ev);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 67u; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "next note");
    RI_ASSERT(ev.slide == 0u, "a released gate is NOT a slide (%u)",
        (unsigned)ev.slide);
    /* A DRUM IS NEVER A SLIDE: a 16th roll is exactly the overlap a 303
     * slide detects, and a slide would lengthen every drum hit. */
    reset();
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    ri_notein_note(&N, &ev);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "a rolled bass drum");
    RI_ASSERT(ev.slide == 0u, "is NOT a slide (%u)", (unsigned)ev.slide);

    /* --- 7b. A ROLL IS NOT A SLIDE (ND SURVIVED §7 ALONE) ----------- */
    /* ND -- a drum overlap becoming a slide -- survived §7, and §7's
     * assertion was too weak: it checked `slide == 0` on the SECOND hit,
     * but the module never tracked drum gates, so `held[dev]` was still 0
     * and the mutation had nothing to change. The drum path has to be
     * exercised three times, because that is what a roll IS. */
    reset();
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "roll hit 1");
    RI_ASSERT(ev.slide == 0u, "hit 1 is not a slide (%u)", (unsigned)ev.slide);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_SD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "roll hit 2 (no note-off between)");
    RI_ASSERT(ev.slide == 0u, "hit 2 is NOT a slide (%u)", (unsigned)ev.slide);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "roll hit 3");
    RI_ASSERT(ev.slide == 0u, "hit 3 is NOT a slide (%u)", (unsigned)ev.slide);
    RI_ASSERT(N.slides == 0u, "and the slide counter stays at zero (%u)",
        (unsigned)N.slides);
    /* A drum note-off is refused: a drum has no gate to release. */
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.on = 0u;
    RI_ASSERT(ri_notein_note(&N, &ev) != 0u, "a drum release is refused");
    /* AND THE 303's COUNTER DOES MOVE, so the counter is not simply dead. */
    reset();
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;
    ri_notein_note(&N, &ev);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 62u; ev.vel = 100u; ev.on = 1u;
    ri_notein_note(&N, &ev);
    RI_ASSERT(N.slides == 1u, "the 303's slide counter counts (%u)",
        (unsigned)N.slides);

    /* --- 7c. AN UNINITIALISED STRUCT IS REFUSED, NOT GUESSED ---------- */
    /* NI -- the ready guard dropped -- survived, because nothing ever
     * called an entry point on a struct that had NOT been initialised. The
     * failure mode it guards is a caller who forgets ri_notein_init and
     * finds channel 0 quietly playing the 303A. */
    {
        struct RINoteIn raw;
        struct RINoteInEvent e2;
        memset(&raw, 0, sizeof raw);
        memset(&e2, 0, sizeof e2);
        e2.ch = 10u; e2.note = GM_BD; e2.vel = 100u; e2.on = 1u;
        /* THE ENCODING IS WHAT MAKES A ZEROED STRUCT SAFE. Device 0 is
         * 303A, so a plain device index would read every entry as "bound to
         * 303A" and channel 10 would quietly play a bass line. Storing the
         * index one higher means a zeroed array means nothing claimed, and
         * the module is SILENT rather than wrong. */
        RI_ASSERT(ri_notein_note(&raw, &e2) != 0u,
            "a zeroed struct resolves nothing");
        RI_ASSERT(raw.refused == 1u, "and counts the refusal (%u)",
            (unsigned)raw.refused);
        RI_ASSERT(ri_notein_unclaimed(&raw, 10u),
            "and claims no channel");
        RI_ASSERT(ri_notein_unclaimed(&raw, 1u),
            "including channel 1 -- NOT bound to 303A");
        ri_notein_init(0);   /* must not crash */
    }

    /* --- 8. AN ACCENT IS A NAMED VELOCITY THRESHOLD ------------------- */
    /* R6e fixed the outbound convention (112 accented, 64 plain). Inbound
     * is the inverse, so it is ONE constant rather than a comparison
     * scattered through the handler -- the same reason ri_smf_velocity is a
     * single function. */
    reset();
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 60u; ev.vel = 127u; ev.on = 1u;
    ri_notein_note(&N, &ev);
    RI_ASSERT(ev.accent == 1u, "a hard hit is an accent (%u)",
        (unsigned)ev.accent);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 60u; ev.vel = 1u; ev.on = 1u;
    ri_notein_note(&N, &ev);
    RI_ASSERT(ev.accent == 0u, "a soft hit is not (%u)",
        (unsigned)ev.accent);

    /* --- 9. AN UNRESOLVABLE NOTE IS REFUSED AND COUNTED -------------- */
    reset();
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = 60u; ev.vel = 100u; ev.on = 1u;   /* GM 60: no instrument */
    RI_ASSERT(ri_notein_note(&N, &ev) != 0u, "an unmapped GM note is refused");
    RI_ASSERT(N.refused == 1u, "and counted (%u)", (unsigned)N.refused);
    /* An unclaimed channel is the same class of thing. */
    reset();
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_808);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    ev.ch = 3u;    /* never bound */
    RI_ASSERT(ri_notein_note(&N, &ev) != 0u, "a note on an unclaimed channel");
    RI_ASSERT(N.refused == 1u, "and counted (%u)", (unsigned)N.refused);
    /* A note-off for something never sounded is a refusal too: counting it
     * is how a stuck note is found rather than heard. */
    reset();
    ri_notein_bind(&N, 1u, RI_NOTEIN_DEV_303A);
    memset(&ev, 0, sizeof ev);
    ev.ch = 1u; ev.note = 60u; ev.on = 0u;
    RI_ASSERT(ri_notein_note(&N, &ev) != 0u, "a release with no note held");
    RI_ASSERT(N.refused == 1u, "counted (%u)", (unsigned)N.refused);

    /* --- 10. THE 909 IS A SEPARATE DEVICE, NOT THE 808 --------------- */
    reset();
    ri_notein_bind(&N, 10u, RI_NOTEIN_DEV_909);
    memset(&ev, 0, sizeof ev);
    ev.ch = 10u; ev.note = GM_BD; ev.vel = 100u; ev.on = 1u;
    RI_ASSERT(ri_notein_note(&N, &ev) == 0u, "GM 36 on the 909");
    RI_ASSERT(ev.dev == RI_NOTEIN_DEV_909, "plays the 909 (%u)",
        (unsigned)ev.dev);
    /* AND ITS OWN VOICE, WHICH IS NOT THE 808's. The 808's lane order is
     * BD SD LT MT HT LC MC RS CL CP CH and the 909's is BD SD CH OH CR RD
     * LT MT HT RS CP -- so lane 2 (low tom) is voice 2 on the 808 and
     * voice 6 (LT) on the 909. NC, which gave both machines the 808's
     * numbers, SURVIVED my first version of this assertion because I had
     * written it with `|| 1`, which can never fail. A tautology in a test
     * is a test that reports nothing. */
    {
        uint32_t bdl = ri_notein_lookup(GM_BD);
        uint32_t ltl = ri_notein_lookup(GM_LT);
        uint32_t cbl = ri_notein_lookup(GM_CB);
        /* Same LANE, different VOICE NUMBER, for the low tom. */
        RI_ASSERT(ri_notein_voice(RI_NOTEIN_DEV_808, ltl) !=
            ri_notein_voice(RI_NOTEIN_DEV_909, ltl),
            "the low tom is voice %u on the 808 and %u on the 909",
            (unsigned)ri_notein_voice(RI_NOTEIN_DEV_808, ltl),
            (unsigned)ri_notein_voice(RI_NOTEIN_DEV_909, ltl));
        /* And the bass drum, which both machines number 0, agrees. */
        RI_ASSERT(ri_notein_voice(RI_NOTEIN_DEV_909, bdl) ==
            ri_notein_voice(RI_NOTEIN_DEV_808, bdl),
            "the bass drum is voice %u on both",
            (unsigned)ri_notein_voice(RI_NOTEIN_DEV_909, bdl));
        /* THE COWBELL IS THE CLEAREST CASE: lane 6 is the 808's LOW
         * COWBELL and the 909's LOW RIDE. */
        RI_ASSERT(cbl == 6u, "the cowbell is lane 6 (%u)", (unsigned)cbl);
        RI_ASSERT(ri_notein_voice(RI_NOTEIN_DEV_808, cbl) !=
            ri_notein_voice(RI_NOTEIN_DEV_909, cbl),
            "lane 6 is voice %u on the 808 and %u on the 909 -- low cowbell"
            " against low ride", (unsigned)ri_notein_voice(RI_NOTEIN_DEV_808, cbl),
            (unsigned)ri_notein_voice(RI_NOTEIN_DEV_909, cbl));
        /* A voice out of range is refused, not clamped to 0. */
        RI_ASSERT(ri_notein_voice(RI_NOTEIN_DEV_808, 200u) == 0xFFFFFFFFu,
            "an instrument past the last has no voice (%u)",
            (unsigned)ri_notein_voice(RI_NOTEIN_DEV_808, 200u));
    }

    /* --- 11. NULLs ---------------------------------------------------- */
    RI_ASSERT(ri_notein_bind(0, 1u, RI_NOTEIN_DEV_303A) != 0, "NULL bind");
    RI_ASSERT(ri_notein_bind(&N, 0u, RI_NOTEIN_DEV_303A) != 0,
        "channel 0 is the REMOTE and cannot be a note channel");
    RI_ASSERT(ri_notein_bind(&N, 1u, 99u) != 0, "a device past the last");
    RI_ASSERT(ri_notein_unclaimed(0, 1u), "NULL unclaimed");
    RI_ASSERT(ri_notein_note(0, &ev) != 0, "NULL note");
    RI_ASSERT(ri_notein_note(&N, 0) != 0, "NULL event");
    RI_ASSERT(ri_notein_instruments() > 0u, "the count is available");

    RI_RESULT("note-in");
}