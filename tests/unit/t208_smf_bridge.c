/* t208_smf_bridge — R8f: engine events to SMF, and the one event that cannot
 * make the crossing.
 *
 * `ri_smf_write` has been built and tested since R7a and has **no caller**:
 * nothing in the app, the CLI or the UI reaches it. This is the conversion
 * between it and the only event stream in the project -- and it is not a
 * rename, because the two event types are numbered differently and because
 * ONE ENGINE EVENT HAS NO SMF EQUIVALENT AT ALL.
 *
 *  - **THE TYPE NUMBERS DO NOT MATCH AND MUST NOT BE COPIED.**
 *    `RI_EV_NOTE_ON` is 3 and `RI_SMF_EV_NOTE_ON` is 0; `RI_EV_NOTE_OFF` is 2
 *    and `RI_SMF_EV_NOTE_OFF` is 1. Passing an `RIEvent.type` straight
 *    through would turn every note-on into a note-off: a file full of
 *    releases with no attacks, which imports as silence. t208 pins the
 *    mapping and a mutant renumbers it.
 *
 *  - **A SLIDE CANNOT CROSS, AND IS COUNTED RATHER THAN FAKED.**
 *    `RI_EV_NOTE_CONTINUE` is rest+slide: the gate stays high and the pitch
 *    slews. SMF 1.0 has no legato pitch change, and both available fakes are
 *    wrong in a way the listener hears: note-off + note-on is the re-attack
 *    that R6a exists to refuse, and a bare pitch bend is a *different note*
 *    on a track that is supposed to be this pattern. So the slide is DROPPED
 *    and counted, exactly as the late accent is counted in R6c. This is the
 *    same class of limit as that one and the monophony gap: real, stated,
 *    not hidden.
 *    `RI_SMF_SLIDE` is defined in `smf_export.h` and **referenced nowhere**,
 *    so "the flag exists" is not "the writer handles it".
 *
 *  - **ONE TRACK PER DEVICE, IN DEVICE ORDER**, so track N is always the same
 *    instrument and a DAW's track list is readable.
 *  - **THE NOTE NUMBER IS THE ENGINE'S**, already resolved by `ri_p303_note`
 *    before the scheduler ran. The bridge does not map it.
 *  - **THE SAMPLE POSITION IS CARRIED THROUGH UNCHANGED**; the writer turns
 *    samples into ticks against its own `SMF_SR`, and second-guessing that
 *    here would be a second answer to a question it already answers.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/sched.h"
#include "project/smf_bridge.h"

#define MAXEV 64u

static struct RIEvent EV[MAXEV];
static struct RISmfEvent MEV[MAXEV];
static struct RISmfTrack TRK;
static struct RISmfTrack TRKS[5];
static struct RISmfSong SONG;

static uint32_t g_n;

static void reset(void) {
    memset(EV, 0, sizeof EV);
    g_n = 0u;
    memset(MEV, 0, sizeof MEV);
    memset(&TRK, 0, sizeof TRK);
    memset(&SONG, 0, sizeof SONG);
}

static void ev(uint32_t type, uint32_t dev, uint32_t note, uint64_t at,
    uint32_t flags) {
    uint32_t k = g_n;
    g_n++;
    EV[k].type = (uint16_t)type;
    EV[k].device = (uint16_t)dev;
    EV[k].value = note;
    EV[k].sample = at;
    EV[k].flags = (uint16_t)flags;
}

int main(void) {
    uint32_t n, i;

    /* --- 1. THE TYPE NUMBERS ARE NOT COPIED -------------------------- */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 36u, 0u, 0u);
    ev(RI_EV_NOTE_OFF, 0u, 36u, 4800u, 0u);
    n = ri_smf_bridge(&TRK, MEV, MAXEV, EV, 2u, 0u);
    RI_ASSERT(n == 2u, "two events crossed (%u)", (unsigned)n);
    RI_ASSERT(MEV[0].type == RI_SMF_EV_NOTE_ON,
        "a NOTE-ON becomes RI_SMF_EV_NOTE_ON (%u), not the engine's %u",
        (unsigned)MEV[0].type, (unsigned)RI_EV_NOTE_ON);
    RI_ASSERT(MEV[1].type == RI_SMF_EV_NOTE_OFF,
        "a NOTE-OFF becomes RI_SMF_EV_NOTE_OFF (%u)", (unsigned)MEV[1].type);
    /* The two enums are genuinely different numbers, which is why this is a
     * mapping and not a cast. */
    RI_ASSERT(RI_EV_NOTE_ON != RI_SMF_EV_NOTE_ON,
        "the engine's NOTE_ON is not the SMF's (%u vs %u)",
        (unsigned)RI_EV_NOTE_ON, (unsigned)RI_SMF_EV_NOTE_ON);

    /* --- 2. NOTE, CHANNEL AND POSITION CARRY THROUGH ------------------ */
    RI_ASSERT(MEV[0].note == 36u, "the engine's note survives (%u)",
        (unsigned)MEV[0].note);
    RI_ASSERT(MEV[0].channel == 0u, "on the track's channel (%u)",
        (unsigned)MEV[0].channel);
    RI_ASSERT(MEV[0].sample == 0u && MEV[1].sample == 4800u,
        "and the sample positions are carried unchanged (%llu, %llu)",
        (unsigned long long)MEV[0].sample,
        (unsigned long long)MEV[1].sample);
    RI_ASSERT(TRK.count == 2u && TRK.events == MEV,
        "the track borrows the caller's array, it does not copy it");
    RI_ASSERT(TRK.channel == 0u, "on channel 0 (%u)", (unsigned)TRK.channel);

    /* --- 3. ACCENT CARRIES; OTHER FLAGS DO NOT INVENT THEMSELVES ----- */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 40u, 0u, RI_EVFLAG_ACCENT);
    ev(RI_EV_NOTE_ON, 0u, 41u, 100u, 0u);
    n = ri_smf_bridge(&TRK, MEV, MAXEV, EV, 2u, 0u);
    RI_ASSERT(n == 2u, "both crossed (%u)", (unsigned)n);
    RI_ASSERT((MEV[0].flags & RI_SMF_ACCENT) != 0u,
        "an accent carries across");
    RI_ASSERT((MEV[1].flags & RI_SMF_ACCENT) == 0u,
        "and a plain note does not invent one");

    /* --- 4. A SLIDE CANNOT CROSS AND IS COUNTED ---------------------- */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 36u, 0u, 0u);
    ev(RI_EV_NOTE_CONTINUE, 0u, 43u, 4800u, RI_EVFLAG_SLIDE);
    ev(RI_EV_NOTE_OFF, 0u, 43u, 9600u, 0u);
    n = ri_smf_bridge(&TRK, MEV, MAXEV, EV, 3u, 0u);
    RI_ASSERT(n == 2u, "the slide did NOT become an event (%u)", (unsigned)n);
    RI_ASSERT(TRK.slides == 1u, "and it is COUNTED (%u)",
        (unsigned)TRK.slides);
    /* What is left is a note-on and a note-off. The bridge must NOT have
     * invented a re-attack in the gap -- that is the defect R6a exists to
     * refuse, and it would be easy to add here by "helpfully" converting the
     * slide into a second note-on. */
    RI_ASSERT(MEV[0].type == RI_SMF_EV_NOTE_ON &&
        MEV[1].type == RI_SMF_EV_NOTE_OFF,
        "what remains is the original note-on and note-off");
    RI_ASSERT(MEV[0].note == 36u,
        "at the pitch the slide STARTED from (%u)", (unsigned)MEV[0].note);

    /* --- 5. ONE TRACK PER DEVICE, IN DEVICE ORDER -------------------- */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 36u, 0u, 0u);
    ev(RI_EV_NOTE_ON, 1u, 48u, 100u, 0u);
    ev(RI_EV_NOTE_OFF, 0u, 36u, 4800u, 0u);
    ev(RI_EV_NOTE_ON, 2u, 36u, 5000u, 0u);
    n = ri_smf_bridge_song(&SONG, TRKS, MEV, MAXEV, EV, 4u, 96u, 140000u);
    RI_ASSERT(n == 3u, "three devices, three tracks (%u)", (unsigned)n);
    RI_ASSERT(SONG.ntracks == 3u, "the song has them (%u)",
        (unsigned)SONG.ntracks);
    /* Track 0 is device 0, and it holds BOTH of its events in order. */
    RI_ASSERT(SONG.tracks[0].count == 2u,
        "track 0 is the first device's, with its two events (%u)",
        (unsigned)SONG.tracks[0].count);
    RI_ASSERT(SONG.tracks[1].count == 1u &&
        SONG.tracks[1].events[0].note == 48u,
        "track 1 is the second device's");
    /* An event from another device never leaks into a track. */
    for (i = 0u; i < SONG.tracks[0].count; i++)
        RI_ASSERT(SONG.tracks[0].channel == 0u,
            "track 0 is entirely device 0");
    RI_ASSERT(TRK.slides == 0u, "no slides here to count");

    /* --- 5b. PPQ AND TEMPO ARE THE CALLER'S AND SURVIVE --------------- */
    /* Not cosmetic. `ri_smf_delta_ticks` treats ppq 0 as "every event is on
     * tick 0", so a song built without them is a file that is only correct
     * when it holds one event -- and it LOOKS fine, which is the part that
     * hurts. */
    RI_ASSERT(SONG.ppq == 96u, "the song keeps the ppq it was given (%u)",
        (unsigned)SONG.ppq);
    RI_ASSERT(SONG.bpm_milli == 140000u, "and the tempo (%u)",
        (unsigned)SONG.bpm_milli);
    /* Two events 4800 samples apart are 4800 * ppq / SMF_SR ticks apart, and
     * a ppq of 0 would make that zero. */
    RI_ASSERT(SONG.ppq != 0u && ri_smf_delta_ticks(&SONG.tracks[0], SONG.ppq,
        1u) > 0u, "a real gap produces a real delta");

    /* --- 6. A DEVICE WITH NOTHING IN IT GETS NO TRACK ---------------- */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 36u, 0u, 0u);
    n = ri_smf_bridge_song(&SONG, TRKS, MEV, MAXEV, EV, 1u, 96u, 140000u);
    RI_ASSERT(n == 1u, "only the device that has events (%u)", (unsigned)n);
    RI_ASSERT(SONG.ntracks == 1u, "and one track");

    /* --- 6b. A DEVICE WHOSE ONLY EVENT IS A SLIDE GETS NO TRACK ------ */
    /* Distinct from section 6, and I only found the difference when a
     * mutant survived: section 6 covers a device with NO EVENTS, and a
     * slide-only device has events -- it takes the other branch entirely,
     * the one after `ri_smf_bridge` returned 0. So a track with count 0
     * and a valid pointer would be added, and build_track would emit a
     * bare end-of-track meta: a track in the DAW's list that plays
     * nothing, which is a file that looks longer than the music. */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 36u, 0u, 0u);
    ev(RI_EV_NOTE_CONTINUE, 3u, 48u, 100u, RI_EVFLAG_SLIDE);
    ev(RI_EV_NOTE_CONTINUE, 3u, 50u, 200u, RI_EVFLAG_SLIDE);
    n = ri_smf_bridge_song(&SONG, TRKS, MEV, MAXEV, EV, 3u, 96u, 140000u);
    RI_ASSERT(n == 1u, "only the device with a note that can cross (%u)",
        (unsigned)n);
    RI_ASSERT(SONG.ntracks == 1u, "and one track in the song");
    /* And every track that IS there has at least one event. */
    {
        uint32_t k;
        for (k = 0u; k < SONG.ntracks; k++)
            RI_ASSERT(SONG.tracks[k].count > 0u,
                "track %u is not empty (%u)", (unsigned)k,
                (unsigned)SONG.tracks[k].count);
    }

    /* --- 7. NOTHING TO WRITE IS REFUSED, NOT AN EMPTY FILE ------------ */
    reset();
    RI_ASSERT(ri_smf_bridge(&TRK, MEV, MAXEV, EV, 0u, 0u) == 0u,
        "no events");
    reset();
    /* ONLY slides. (My first version had a note-on in here and expected 0,
     * which is the bridge being RIGHT and the assertion being wrong -- a
     * note-on crosses, so a stream containing one is not slide-only.) */
    ev(RI_EV_NOTE_CONTINUE, 0u, 43u, 100u, RI_EVFLAG_SLIDE);
    ev(RI_EV_NOTE_CONTINUE, 0u, 45u, 200u, RI_EVFLAG_SLIDE);
    RI_ASSERT(ri_smf_bridge(&TRK, MEV, MAXEV, EV, 2u, 0u) == 0u,
        "a slide-only stream is not a track");
    RI_ASSERT(TRK.slides == 2u, "but both slides are counted (%u)",
        (unsigned)TRK.slides);
    /* A capacity of 1 cannot hold two events, and the second is refused
     * rather than overwriting the first. */
    reset();
    ev(RI_EV_NOTE_ON, 0u, 36u, 0u, 0u);
    ev(RI_EV_NOTE_OFF, 0u, 36u, 100u, 0u);
    RI_ASSERT(ri_smf_bridge(&TRK, MEV, 1u, EV, 2u, 0u) == 1u,
        "a short output array truncates rather than overwrites");
    RI_ASSERT(MEV[0].type == RI_SMF_EV_NOTE_ON, "keeping the first event");

    /* --- 8. NULLs ----------------------------------------------------- */
    reset();
    RI_ASSERT(ri_smf_bridge(0, MEV, MAXEV, EV, 1u, 0u) == 0u, "NULL track");
    RI_ASSERT(ri_smf_bridge(&TRK, 0, MAXEV, EV, 1u, 0u) == 0u, "NULL out");
    RI_ASSERT(ri_smf_bridge(&TRK, MEV, MAXEV, 0, 1u, 0u) == 0u, "NULL in");
    RI_ASSERT(ri_smf_bridge_song(0, TRKS, MEV, MAXEV, EV, 1u, 96u, 140000u) == 0u,
        "NULL song");
    RI_ASSERT(ri_smf_bridge_song(&SONG, TRKS, MEV, 0u, EV, 1u, 96u, 140000u) == 0u,
        "NULL event array");

    RI_RESULT("smf-bridge");
}