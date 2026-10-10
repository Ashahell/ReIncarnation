/* t194_smf_import — SMF type 1 import (R7b).
 *
 * An importer's job is not to read a file; it is to read a file that was
 * written by someone else, and most of what arrives is not what the writer
 * intended. Every law here is a case where being wrong is INVISIBLE:
 *
 *  - a note-on with velocity 0 is a note-off. Treating it as a note-on
 *    leaves the note sounding forever, and the pattern looks fine until you
 *    count how many are stuck.
 *  - a note still held at end-of-track has to be closed and REPORTED. A DAW
 *    that saved mid-note is routine, and silently dropping it loses a hit.
 *  - a note-off with no matching note-on must be ignored, not matched to
 *    whatever happens to be nearest.
 *  - a note number above 127 does not exist in MIDI. Clamping it is a
 *    choice and has to be counted so the caller can be told.
 *  - events past the end of the pattern must be dropped AND counted, or the
 *    pattern quietly loses its ending.
 *  - a truncated or malformed file must be REFUSED. Parsing past the end of
 *    a buffer is how an importer turns one bad file into a hang.
 *
 * The strongest law is the ROUND TRIP: what the exporter writes, the
 * importer must read back as the same steps. Everything else is special
 * cases on top of that.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/sched.h"
#include "project/smf_export.h"

int main(void) {
    static struct RIStep steps[64];
    static struct RISmfImport rep;
    struct RISmfTrack trk[2];
    struct RISmfSong song;
    struct RISmfEvent evs[4];
    uint8_t buf[512];
    uint32_t n, i;

    /* --- the round trip, which subsumes most of the rest -------------- */
    evs[0].sample = 0u;    evs[0].type = RI_SMF_EV_NOTE_ON;  evs[0].channel = 0u;
    evs[0].note = 36u;     evs[0].flags = RI_SMF_ACCENT;
    evs[1].sample = 24000u; evs[1].type = RI_SMF_EV_NOTE_OFF; evs[1].channel = 0u;
    evs[1].note = 36u;     evs[1].flags = 0u;
    evs[2].sample = 48000u; evs[2].type = RI_SMF_EV_NOTE_ON;  evs[2].channel = 0u;
    evs[2].note = 42u;     evs[2].flags = 0u;
    evs[3].sample = 72000u; evs[3].type = RI_SMF_EV_NOTE_OFF; evs[3].channel = 0u;
    evs[3].note = 42u;     evs[3].flags = 0u;
    trk[0].events = evs; trk[0].count = 4u; trk[0].channel = 0u; trk[0].name = "303A";
    ri_smf_song_init(&song, 480u, 120000u, trk, 2u);
    RI_ASSERT(ri_smf_song_add(&song, &trk[0]) == 0, "track added");
    n = ri_smf_write(&song, buf, sizeof buf);
    RI_ASSERT(n > 0u, "the exporter produced a file (%u)", (unsigned)n);

    /* 480 ppq is 120 ticks to a 16th. At 48 kHz a 16th is 12000 samples, so
     * the second note at 48000 samples is FOUR 16ths in, not two. My first
     * expectation said two, which is what you get by forgetting the sample
     * rate. */
    i = ri_smf_read(buf, n, steps, 64u, 480u, &rep);
    RI_ASSERT(i == 5u, "the pattern is five steps long (%u)", (unsigned)i);
    RI_ASSERT(steps[0].note == 36u, "step 0 is the first note (%u)", (unsigned)steps[0].note);
    RI_ASSERT(steps[4].note == 42u, "the second note is four 16ths in (%u)",
        (unsigned)steps[4].note);
    RI_ASSERT(steps[1].note == 0u && (steps[1].flags & RI_STEP_REST) != 0u,
        "step 1 is a rest, not a hole");
    RI_ASSERT(rep.steps == 5u, "the reported length is the LAST note, not the "
        "first rest (%u)", (unsigned)rep.steps);
    /* The accent must survive the trip or the export is lossy in a way
     * nobody asked for. */
    RI_ASSERT((steps[0].flags & RI_STEP_ACCENT) != 0u, "accent survived the round trip");
    RI_ASSERT(rep.stuck_notes == 0u && rep.orphan_off == 0u &&
        rep.bad_note == 0u && rep.past_end == 0u,
        "a well-formed file reports nothing odd (%u/%u/%u/%u)",
        (unsigned)rep.stuck_notes, (unsigned)rep.orphan_off,
        (unsigned)rep.bad_note, (unsigned)rep.past_end);

    /* --- velocity 0 note-on is a note-off ----------------------------- */
    {
        static const uint8_t f[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0x01,0xE0,
            'M','T','r','k', 0,0,0,9,
            0x00, 0x90, 40, 100,          /* on  at tick 0   */
            0x81, 0x40,                    /* delta 192       */
            0x90, 40, 0                    /* on vel 0 = OFF  */
        };
        i = ri_smf_read(f, (uint32_t)sizeof f, steps, 64u, 480u, &rep);
        RI_ASSERT(i > 0u, "a velocity-0 note-on still parses (%u)", (unsigned)i);
        RI_ASSERT(rep.stuck_notes == 0u,
            "velocity 0 is a note-off, so nothing is left hanging (%u)",
            (unsigned)rep.stuck_notes);
    }

    /* --- a note still held at end-of-track is CLOSED and REPORTED ----- */
    {
        static const uint8_t f[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0x01,0xE0,
            'M','T','r','k', 0,0,0,9,
            0x00, 0x90, 44, 90,
            0x81, 0x40,
            0xFF, 0x2F, 0x00
        };
        i = ri_smf_read(f, (uint32_t)sizeof f, steps, 64u, 480u, &rep);
        RI_ASSERT(i > 0u, "a held note still parses (%u)", (unsigned)i);
        RI_ASSERT(rep.stuck_notes == 1u,
            "the held note is counted, not silently dropped (%u)",
            (unsigned)rep.stuck_notes);
        RI_ASSERT(steps[0].note == 44u, "and the note is not lost (%u)",
            (unsigned)steps[0].note);
    }

    /* --- a note-off with nothing playing is ignored, and counted ------ */
    {
        static const uint8_t f[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0x01,0xE0,
            'M','T','r','k', 0,0,0,16,
            0x00, 0x80, 36, 0x40,
            0x00, 0x90, 38, 90,
            0x81, 0x40,
            0x80, 38, 0x40,
            0xFF, 0x2F, 0x00
        };
        i = ri_smf_read(f, (uint32_t)sizeof f, steps, 64u, 480u, &rep);
        RI_ASSERT(i > 0u, "an orphan note-off does not stop the parse (%u)", (unsigned)i);
        RI_ASSERT(rep.orphan_off == 1u, "the orphan is counted (%u)",
            (unsigned)rep.orphan_off);
        RI_ASSERT(steps[0].note == 38u,
            "the orphan did not overwrite the real note (%u)", (unsigned)steps[0].note);
    }

    /* --- a note number above 127 does not exist in MIDI --------------- */
    /* A well-formed writer masks it, so this only arrives from a file
     * someone else's tool produced. Clamping is a CHOICE, and an uncounted
     * choice is a silent one. */
    {
        static const uint8_t f[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0x01,0xE0,
            'M','T','r','k', 0,0,0,9,
            0x00, 0x90, 200, 100,         /* note byte 200 -- out of range */
            0x81, 0x40,
            0x80, 200, 0x40,
            0xFF, 0x2F, 0x00
        };
        i = ri_smf_read(f, (uint32_t)sizeof f, steps, 64u, 480u, &rep);
        RI_ASSERT(i > 0u, "an out-of-range note still parses (%u)", (unsigned)i);
        /* Counted PER EVENT, so the pair of note-on/note-off on note 200
         * counts twice. That is the honest reading -- the file contains two
         * bytes that are not MIDI -- and it is why this is a count the
         * caller can show rather than a flag it can only guess about. */
        RI_ASSERT(rep.bad_note == 2u,
            "the clamp is COUNTED per event, not silent (%u)", (unsigned)rep.bad_note);
        RI_ASSERT(steps[0].note == 127u, "and it lands on 127 (%u)",
            (unsigned)steps[0].note);
    }

    /* --- events past the end of the pattern are dropped AND counted -- */
    /* Dropping is correct -- the caller's array is the pattern's length --
     * but dropping WITHOUT counting loses the ending of every long pattern
     * with no sign anywhere. */
    {
        static const uint8_t f[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0x01,0xE0,
            'M','T','r','k', 0,0,0,17,
            0x00, 0x90, 36, 100,
            0x83, 0x60,                   /* delta 480 ticks = four 16ths */
            0x90, 40, 100,
            0x81, 0x40,
            0x80, 40, 0x40,
            0xFF, 0x2F, 0x00
        };
        /* a four-step pattern: the second note is far past its end */
        i = ri_smf_read(f, (uint32_t)sizeof f, steps, 4u, 480u, &rep);
        RI_ASSERT(i > 0u, "a long pattern still parses (%u)", (unsigned)i);
        RI_ASSERT(rep.past_end == 1u,
            "the note past the end is COUNTED as dropped (%u)", (unsigned)rep.past_end);
        RI_ASSERT(steps[0].note == 36u, "and the note inside the pattern survives (%u)",
            (unsigned)steps[0].note);
    }

    /* --- malformed input is REFUSED, never parsed past the buffer ----- */
    RI_ASSERT(ri_smf_read(0, 10u, steps, 64u, 480u, &rep) == 0u, "NULL is refused");
    RI_ASSERT(ri_smf_read(buf, 3u, steps, 64u, 480u, &rep) == 0u,
        "three bytes is refused, not parsed");
    /* ppq 0 is only a refusal when NOTHING supplies one: a zero in the
     * header falls back to the caller's, and only two zeros refuse. */
    {
        static const uint8_t z2[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0,0,
            'M','T','r','k', 0,0,0,7,
            0x00, 0x90, 40, 100, 0xFF, 0x2F, 0x00
        };
        RI_ASSERT(ri_smf_read(z2, (uint32_t)sizeof z2, steps, 64u, 480u, &rep) > 0u,
            "a zero header ppq falls back to the caller's");
        RI_ASSERT(ri_smf_read(z2, (uint32_t)sizeof z2, steps, 64u, 0u, &rep) == 0u,
            "ppq 0 with no fallback is refused rather than dividing by it");
    }
    {
        uint8_t bad[16];
        for (i = 0u; i < 16u; i++)
            bad[i] = 0xA5u;
        RI_ASSERT(ri_smf_read(bad, 16u, steps, 64u, 480u, &rep) == 0u,
            "junk is refused");
    }
    /* A header that claims a track length past the end of the buffer is the
     * classic way an importer walks off the end of its own allocation. */
    {
        static const uint8_t f[] = {
            'M','T','h','d', 0,0,0,6, 0,1, 0,1, 0x01,0xE0,
            'M','T','r','k', 0x7F,0xFF,0xFF,0xFF
        };
        RI_ASSERT(ri_smf_read(f, (uint32_t)sizeof f, steps, 64u, 480u, &rep) == 0u,
            "a track length past the end of the buffer is refused");
    }

    RI_RESULT("smf-import");
}