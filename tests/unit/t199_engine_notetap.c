/* t199_engine_notetap — R6d: the engine's note tap.
 *
 * The tap exists so the render can put a note on the wire without the engine
 * knowing what a wire is. That is the whole design: `engine/` gains a ring of
 * records and nothing else, and `app/` drains it. The engine does not include
 * midi_io, does not know a channel exists, and still sends nothing itself.
 *
 * The tap records the RESOLVED sound, never the lane. This is the law the
 * whole of R6 rests on and it is the one a lazy implementation gets wrong:
 * the 808's lane->sound map is `e->s808.slot[lane]`, which the user can
 * remap, while the 909's is the static `RI_LANE_TO_RB909_VOICE`. A tap that
 * recorded the LANE would hand the app a number it cannot interpret without
 * re-implementing the engine's own resolution -- and the version of that
 * re-implementation in the app would be wrong the moment a lane moved.
 *
 * Two more laws, both of which cost something to get wrong:
 *
 *  - `ri_engine_load` RUNS EVERY RENDER BUFFER on the live path (the drum-ring
 *    A0 note in engine.c), so a tap cleared in `load` would never hold more
 *    than one block's notes. Pinned below, because the fix for that bug once
 *    was to add a reset to the song-load path and the bug was that a reset
 *    existed at all.
 *
 *  - THE LATE ACCENT IS RECORDED AS ITS OWN RECORD. A total accent
 *    (RI_EV_ACCENT to RI_VOICE_ALL) sorts AFTER the same-sample hits and is
 *    applied retroactively to voices stamped at this cursor. By the time it
 *    arrives the note-on has already been written to the tap, and MIDI has no
 *    way to make a note you already sent louder. Rather than pretend that
 *    does not happen, the tap carries the accent as a second record so the
 *    caller (and a test) can COUNT how often the wire loses an accent
 *    instead of arguing about whether it does.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "engine/seq/pattern.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"

static struct RIEngine E;
static struct RIEvent EV;
static struct RINoteTapRec R[8];

static void ev(uint32_t type, uint32_t dev, uint32_t voice, uint32_t value,
    uint32_t flags) {
    memset(&EV, 0, sizeof EV);
    EV.type = (uint16_t)type;
    EV.device = (uint16_t)dev;
    EV.voice = (uint16_t)voice;
    EV.value = value;
    EV.flags = (uint16_t)flags;
    ri_engine_apply_event(&E, &EV);
}

static uint32_t drain(void) { return ri_engine_note_read(&E, R, 8u); }

int main(void) {
    uint32_t n;

    ri_engine_init(&E);

    /* --- silence by default ------------------------------------------- */
    n = drain();
    RI_ASSERT(n == 0u, "a fresh engine has nothing to say (%u)", (unsigned)n);
    RI_ASSERT(ri_engine_note_pending(&E) == 0u, "and nothing pending");

    /* --- the 303s: the note the engine already has -------------------- */
    ev(RI_EV_NOTE_ON, 0u, 0u, 46u, 0u);
    n = drain();
    RI_ASSERT(n == 1u, "a 303A note-on is recorded (%u)", (unsigned)n);
    RI_ASSERT(R[0].kind == RI_NOTEK_NOTE, "as a note");
    RI_ASSERT(R[0].device == 0u, "on device 0 (%u)", (unsigned)R[0].device);
    RI_ASSERT(R[0].note == 46u, "carrying the ENGINE's note 46 (%u)",
        (unsigned)R[0].note);
    ev(RI_EV_NOTE_ON, 1u, 0u, 38u, 0u);
    n = drain();
    RI_ASSERT(n == 1u && R[0].device == 1u && R[0].note == 38u,
        "303B is device 1 and keeps its own note (%u, %u)",
        (unsigned)R[0].device, (unsigned)R[0].note);

    /* --- slide and note-off are RECORDED; the caller decides ---------- */
    /* The tap is not allowed to be clever here. A slide becomes a legato
     * and a note-off becomes a release in the BYTE producer, and a tap that
     * pre-decided that would be a second place to be wrong about it. */
    ev(RI_EV_NOTE_CONTINUE, 0u, 0u, 40u, RI_EVFLAG_SLIDE);
    ev(RI_EV_NOTE_OFF, 0u, 0u, 46u, 0u);
    n = drain();
    RI_ASSERT(n == 2u, "slide and release are both recorded (%u)", (unsigned)n);
    RI_ASSERT(R[0].note == 40u && (R[0].flags & RI_EVFLAG_SLIDE) != 0u,
        "the slide keeps its flag (%u)", (unsigned)R[0].flags);
    RI_ASSERT(R[1].kind == RI_NOTEK_NOTE && R[1].is_off == 1u,
        "and the release is marked as one");
    RI_ASSERT(R[0].is_off == 0u, "while the slide is not");

    /* --- the 808: THE LIVE SLOT MAP, not a table ---------------------- */
    /* Default slot 5 is the rim shot. Move it and the tap must follow,
     * because that is the whole reason the engine resolves here. */
    E.s808.slot[5] = RB808_BD;
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);
    n = drain();
    RI_ASSERT(n == 1u, "the 808 hit is recorded (%u)", (unsigned)n);
    RI_ASSERT(R[0].device == 2u, "on device 2 (%u)", (unsigned)R[0].device);
    RI_ASSERT(R[0].sound == RB808_BD,
        "as the RESOLVED sound the user's slot map now holds (got %u, want %u)",
        (unsigned)R[0].sound, (unsigned)RB808_BD);
    RI_ASSERT(R[0].sound != 5u,
        "and emphatically not as the lane number it arrived on");
    E.s808.slot[5] = RB808_RS;

    /* --- accent rides the record, not the sound ----------------------- */
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, RI_EVFLAG_ACCENT);
    n = drain();
    RI_ASSERT(n == 1u && (R[0].flags & RI_EVFLAG_ACCENT) != 0u,
        "an accented hit keeps its flag (%u)", (unsigned)R[0].flags);

    /* --- the 909: the static voice table ------------------------------ */
    ev(RI_EV_NOTE_ON, 3u, 3u, 3u, 0u);
    n = drain();
    RI_ASSERT(n == 1u && R[0].device == 3u, "the 909 hit is device 3");
    RI_ASSERT(R[0].sound == RI_LANE_TO_RB909_VOICE[3],
        "resolved through the static table (got %u, want %u)",
        (unsigned)R[0].sound, (unsigned)RI_LANE_TO_RB909_VOICE[3]);

    /* --- events that are NOT notes say nothing ------------------------ */
    ev(RI_EV_FLAM, 3u, 3u, 4u, 0u);
    ev(RI_EV_PARAM, 0u, 0u, 7u, 0u);
    ev(RI_EV_AUTOMATION, 0u, 0u, 3u, 0u);
    ev(RI_EV_METER, 0u, 0u, 1u, 0u);
    n = drain();
    RI_ASSERT(n == 0u, "flam, param, automation and meter are silent (%u)",
        (unsigned)n);

    /* --- a reserved lane says nothing --------------------------------- */
    ev(RI_EV_NOTE_ON, 2u, RI_DRUM_CLASSIC_LANES, 0u, 0u);
    ev(RI_EV_NOTE_ON, 3u, 40u, 0u, 0u);
    n = drain();
    RI_ASSERT(n == 0u, "a lane past the classic eleven is not a note (%u)",
        (unsigned)n);

    /* --- the LATE ACCENT, recorded as itself -------------------------- */
    /* It sorts after the same-sample hits and accents voices stamped at
     * this cursor. The note is already in the tap by now; MIDI has no way
     * to make it louder, so the only honest thing is to say so. */
    ev(RI_EV_ACCENT, 2u, RI_VOICE_ALL, 0u, 0u);
    n = drain();
    RI_ASSERT(n == 1u, "a total accent is recorded (%u)", (unsigned)n);
    RI_ASSERT(R[0].kind == RI_NOTEK_LATE_ACCENT,
        "and it says what it is (%u)", (unsigned)R[0].kind);
    RI_ASSERT(R[0].sample == 0u, "at the cursor it landed on");

    /* --- and it carries the SAMPLE, so the pairing is measurable ------- */
    E.cursor = 4096u;
    ev(RI_EV_NOTE_ON, 2u, 5u, 5u, 0u);
    ev(RI_EV_ACCENT, 2u, RI_VOICE_ALL, 0u, 0u);
    n = drain();
    RI_ASSERT(n == 2u, "a hit and its late accent are two records (%u)",
        (unsigned)n);
    RI_ASSERT(R[0].sample == 4096u && R[1].sample == 4096u,
        "sharing one sample position (%llu, %llu)",
        (unsigned long long)R[0].sample, (unsigned long long)R[1].sample);
    RI_ASSERT(R[0].kind == RI_NOTEK_NOTE && R[1].kind == RI_NOTEK_LATE_ACCENT,
        "which is exactly how a caller counts accents the wire cannot carry");

    /* --- LOAD MUST NOT CLEAR IT --------------------------------------- */
    /* ri_engine_load runs once per 256-frame render buffer on the live
     * path. A tap reset there would cap the ring at one block's notes. */
    E.cursor = 0u;
    ev(RI_EV_NOTE_ON, 0u, 0u, 60u, 0u);
    RI_ASSERT(ri_engine_note_pending(&E) == 1u, "one note is waiting");
    ri_engine_load(&E, 0, 0u, 0u, 0u);
    RI_ASSERT(ri_engine_note_pending(&E) == 1u,
        "ri_engine_load did NOT clear the tap (%u)",
        (unsigned)ri_engine_note_pending(&E));
    n = drain();
    RI_ASSERT(n == 1u && R[0].note == 60u, "and the note survived it (%u)",
        (unsigned)n);

    /* --- bounded: the OLDEST goes, and it is counted ------------------ */
    {
        uint32_t k;
        for (k = 0u; k < RI_NOTETAP_CAP; k++)
            ev(RI_EV_NOTE_ON, 0u, 0u, k & 0x7Fu, 0u);
        RI_ASSERT(ri_engine_note_pending(&E) == RI_NOTETAP_CAP,
            "the ring is exactly full (%u)",
            (unsigned)ri_engine_note_pending(&E));
        /* One more: the oldest note must go, not the newest. Dropping the
         * newest would silently eat the note that just played, which is
         * the one the user is looking at. */
        ev(RI_EV_NOTE_ON, 0u, 0u, 0x7Fu, 0u);
        RI_ASSERT(ri_engine_note_pending(&E) == RI_NOTETAP_CAP,
            "and still full (%u)", (unsigned)ri_engine_note_pending(&E));
        RI_ASSERT(ri_engine_note_dropped(&E) == 1u,
            "with the drop COUNTED (%u)",
            (unsigned)ri_engine_note_dropped(&E));
        {
            uint32_t total = 0u, first = 0u;
            static struct RINoteTapRec all[RI_NOTETAP_CAP];
            total = ri_engine_note_read(&E, all, RI_NOTETAP_CAP);
            first = all[0].note;
            RI_ASSERT(total == RI_NOTETAP_CAP, "drained the full ring (%u)",
                (unsigned)total);
            RI_ASSERT(first == 1u,
                "and the OLDEST note is the one that went (got %u, want 1)",
                (unsigned)first);
            RI_ASSERT(all[total - 1u].note == 0x7Fu,
                "while the newest is the one still there (%u)",
                (unsigned)all[total - 1u].note);
        }
    }

    /* --- a partial drain keeps the rest in order ---------------------- */
    {
        static struct RINoteTapRec two[2];
        uint32_t got;
        ri_notetap_reset(&E.notetap);
        ev(RI_EV_NOTE_ON, 0u, 0u, 10u, 0u);
        ev(RI_EV_NOTE_ON, 0u, 0u, 11u, 0u);
        ev(RI_EV_NOTE_ON, 0u, 0u, 12u, 0u);
        got = ri_engine_note_read(&E, two, 2u);
        RI_ASSERT(got == 2u && two[0].note == 10u && two[1].note == 11u,
            "two of three, in order (%u, %u, %u)", (unsigned)got,
            (unsigned)two[0].note, (unsigned)two[1].note);
        RI_ASSERT(ri_engine_note_pending(&E) == 1u, "one left (%u)",
            (unsigned)ri_engine_note_pending(&E));
        n = drain();
        RI_ASSERT(n == 1u && R[0].note == 12u, "and it is the third (%u, %u)",
            (unsigned)n, (unsigned)R[0].note);
    }

    /* --- nulls ---------------------------------------------------------- */
    RI_ASSERT(ri_engine_note_read(0, R, 8u) == 0u, "NULL engine");
    RI_ASSERT(ri_engine_note_read(&E, 0, 8u) == 0u, "NULL output");
    RI_ASSERT(ri_engine_note_pending(0) == 0u, "NULL pending");
    RI_ASSERT(ri_engine_note_dropped(0) == 0u, "NULL dropped");
    ri_notetap_reset(0);
    RI_ASSERT(1u, "NULL reset is a no-op");

    RI_RESULT("engine-notetap");
}