/* t198_devout_wire — R6c: the event-to-bytes translation.
 *
 * The engine already resolves everything R6c needs: `ri_p303_note()` puts a
 * real MIDI note in `RIStep.note` before the scheduler runs, `e->s808.slot`
 * and `RI_LANE_TO_RB909_VOICE` resolve lanes to sounds, and `RIEvent` carries
 * the accent, slide and note-off flags. So R6c is NOT a mapping problem. It
 * is a TRANSLATION problem: four flags into three bytes.
 *
 * The laws, and why each is here rather than at the call site:
 *
 *  - ACCENT IS VELOCITY, and the velocity must be THE SAME live as it is in
 *    an exported SMF. Two conventions that drift mean a loop exported from a
 *    live take changes its accents when it comes back in, which nobody hears
 *    as "wrong" and everybody notices as "this loop feels different".
 *  - SLIDE IS LEGATO and emits NOTHING. Rest plus slide keeps the gate high
 *    and slews the pitch; re-attacking is the audible defect.
 *  - NOTE-CONTINUE (rest + slide) IS the slide. Treating it as a fresh
 *    note-on is exactly the re-attack above.
 *  - AN EVENT R6 DOES NOT EMIT MUST PRODUCE NO BYTES. Flams, accents,
 *    pattern changes, meters and transport are internal; a DAW recording
 *    them as notes is a DAW full of ghosts.
 *  - The live path never invents a device map. The CALLER resolves
 *    lane -> sound, because the 808's is live and user-remappable and the
 *    909's is static, and a function that guessed would be wrong for one of
 *    them.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/sched.h"
#include "midi_io/midi_devout.h"
#include "project/smf_export.h"

int main(void) {
    static struct RIDevOut d;
    uint8_t buf[16];
    struct RIDevEvent x;
    struct RIEvent ev;

    /* --- a plain note ------------------------------------------------- */
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_ON;
    ev.value = 36u;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "a note-on translates");
    RI_ASSERT(x.note == 36u, "note 36 (%u)", (unsigned)x.note);
    RI_ASSERT(x.is_off == 0u, "and it is not an off");
    RI_ASSERT(x.is_legato == 0u, "nor a slide");

    /* --- accent is velocity ------------------------------------------ */
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_ON;
    ev.value = 36u;
    ev.flags = RI_EVFLAG_ACCENT;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "an accent translates");
    RI_ASSERT(x.vel > ri_devout_velocity(0u),
        "an accent is louder than a plain note (%u vs %u)", (unsigned)x.vel,
        (unsigned)ri_devout_velocity(0u));

    /* --- AND the same velocity the EXPORTER uses ----------------------- */
    /* One convention, two paths. If these drift, a loop exported from a
     * live take comes back with different accents and nobody can say why. */
    RI_ASSERT(ri_devout_velocity(RI_EVFLAG_ACCENT) == ri_smf_velocity(RI_SMF_ACCENT),
        "live and exported accents agree (%u vs %u)",
        (unsigned)ri_devout_velocity(RI_EVFLAG_ACCENT),
        (unsigned)ri_smf_velocity(RI_SMF_ACCENT));
    RI_ASSERT(ri_devout_velocity(0u) == ri_smf_velocity(0u),
        "and so do plain notes (%u vs %u)",
        (unsigned)ri_devout_velocity(0u), (unsigned)ri_smf_velocity(0u));

    /* --- note off ------------------------------------------------------ */
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_OFF;
    ev.value = 36u;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u && x.is_off == 1u,
        "a note-off translates to an off");

    /* --- NOTE_CONTINUE is the slide, and emits nothing ---------------- */
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_CONTINUE;
    ev.value = 38u;
    ev.flags = RI_EVFLAG_LEGATO;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "a note-continue translates");
    RI_ASSERT(x.is_legato == 1u, "and is recognised as the slide it is");
    RI_ASSERT(x.is_off == 0u, "the gate stays high, so it is not an off");

    /* --- and the whole thing through the byte producer --------------- */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 0u, 5u);
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_CONTINUE;
    ev.value = 38u;
    ev.flags = RI_EVFLAG_LEGATO;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "translated");
    RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 5u, RI_DEVOUT_303A, &x, 0u) == 0u,
        "and a legato slide emits NO bytes at all");
    RI_ASSERT(ri_devout_legato(&d) == 1u, "counted, not silently dropped (%u)",
        (unsigned)ri_devout_legato(&d));

    /* --- events that must produce nothing ----------------------------- */
    {
        static const uint32_t skip[] = {
            RI_EV_FLAM, RI_EV_ACCENT, RI_EV_PATTERN_CHANGE, RI_EV_AUTOMATION,
            RI_EV_PARAM, RI_EV_METER, RI_EV_TRANSPORT
        };
        uint32_t k;
        for (k = 0u; k < sizeof skip / sizeof skip[0]; k++) {
            memset(&ev, 0, sizeof ev);
            ev.type = skip[k];
            ev.value = 36u;
            RI_ASSERT(ri_devout_translate(&ev, &x) == 0u,
                "event type %u produces no note", (unsigned)skip[k]);
        }
    }

    /* --- disabled and unclaimed still refuse, after translation -------- */
    ri_devout_init(&d, 1u);
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_ON;
    ev.value = 36u;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "translated");
    RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 5u, RI_DEVOUT_303A, &x, 0u) == 0u,
        "an unclaimed channel still refuses it");
    ri_devout_init(&d, 0u);
    ri_devout_assign(&d, 0u, 5u);
    RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 5u, RI_DEVOUT_303A, &x, 0u) == 0u,
        "and disabled still refuses it");

    /* --- MELODIC: the engine's note is used AS IT IS ------------------ */
    /* ri_p303_note() already resolved RIEvent.value to a MIDI note. An emit
     * that "helpfully" remapped it would play the wrong pitch, and a 303 at
     * the wrong pitch is the kind of thing that gets blamed on the synth. */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 0u, 5u);
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_ON;
    ev.value = 46u;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "translated");
    {
        uint32_t w = ri_devout_emit(&d, buf, sizeof buf, 5u, RI_DEVOUT_303A, &x, 0u);
        RI_ASSERT(w == 3u, "a melodic note emits (%u)", (unsigned)w);
        RI_ASSERT(buf[0] == 0x95u, "channel 5, note-on (%02X)", buf[0]);
        RI_ASSERT(buf[1] == 46u, "and the note is the ENGINE's 46, unremapped (%u)",
            (unsigned)buf[1]);
    }

    /* --- drums carry the ACCENT too ----------------------------------- */
    /* Routing drums through ri_devout_drum() would have pinned every drum
     * at one velocity, so an accented kick and a plain one would land in a
     * DAW identically. */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 0u, 6u);
    memset(&ev, 0, sizeof ev);
    ev.type = RI_EV_NOTE_ON;
    ev.value = 0u;                       /* drums carry a LANE here */
    ev.flags = RI_EVFLAG_ACCENT;
    RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "an accented 808 hit translates");
    {
        uint32_t w = ri_devout_emit(&d, buf, sizeof buf, 6u, RI_DEVOUT_808, &x, RB808_BD);
        RI_ASSERT(w == 3u, "the 808 hit emits (%u)", (unsigned)w);
        RI_ASSERT(buf[1] == 36u, "bass drum is 36 (%u)", (unsigned)buf[1]);
        RI_ASSERT(buf[2] == ri_devout_velocity(RI_EVFLAG_ACCENT),
            "and it carries the ACCENT, not a fixed velocity (%u vs %u)",
            (unsigned)buf[2], (unsigned)ri_devout_velocity(RI_EVFLAG_ACCENT));
    }
    /* And the 909 through the same path, on its own voice. */
    RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 6u, RI_DEVOUT_909, &x, RB909_OH)
        == 3u && buf[1] == 46u, "the 909 open hat is 46 (%u)", (unsigned)buf[1]);

    /* --- refusals that only the device path can make ------------------- */
    RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 6u, RI_DEVOUT_808, &x, 200u) == 0u,
        "an unknown 808 sound is refused");
    RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 6u, RI_DEVOUT_909, &x, 200u) == 0u,
        "an unknown 909 voice is refused");
    /* With a NON-ZERO note, so only the DEVICE check can refuse this. My
     * first version reused an event whose note happened to be 0, and the
     * melodic fallback refused it for a completely different reason -- the
     * test could not tell the two paths apart. */
    {
        struct RIDevEvent y;
        memset(&y, 0, sizeof y);
        y.note = 46u;
        RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 6u, 9u, &y, RB808_BD) == 0u,
            "a device with no note map is refused, not guessed");
        RI_ASSERT(ri_devout_emit(&d, buf, sizeof buf, 6u, 200u, &y, RB808_BD) == 0u,
            "and so is a wildly out-of-range one");
    }

    /* --- the lane->sound step is the CALLER's, and 808's is live ------- */
    /* The 808's mapping is `e->s808.slot[lane]`, which the user can change;
     * the 909's is static. A helper that guessed would be wrong for one of
     * them, so R6c takes the resolved sound as an argument and never looks. */
    RI_ASSERT(ri_devout_resolves_lane() == 0u,
        "R6 does not resolve lanes: the engine already does");

    /* --- nulls --------------------------------------------------------- */
    RI_ASSERT(ri_devout_translate(0, &x) == 0u, "NULL event");
    RI_ASSERT(ri_devout_translate(&ev, 0) == 0u, "NULL output");
    RI_ASSERT(ri_devout_emit(0, buf, sizeof buf, 5u, RI_DEVOUT_303A, &x, 0u) == 0u, "NULL sink");

    RI_RESULT("devout-wire");
}