/* t196_devout_map — R6b: the 808/909 note maps.
 *
 * KEYED BY THE ENGINE'S SOUND ID, NOT BY PANEL LANE — and that is the whole
 * design decision.
 *
 * The obvious keying is the panel lane number, and it is the wrong one. The
 * two machines' lane enums DISAGREE: lane 7 is `RI_L808_CB` on the 808 and
 * `RI_L909_CH` on the 909, lane 9 is `OH` on one and `CC` on the other, and
 * lane 10 is `CH` on one and `RC` on the other. A single eleven-entry table
 * indexed by lane would therefore play a completely different instrument on
 * the two machines for four of eleven indices, and nothing would report it.
 *
 * The LANE->SOUND step is not this module's and already exists in the engine:
 * `RI_LANE_TO_RB808_SLOT[i] == i` is pinned by t53 alongside
 * `rb808_slot_of(RI_808_SLOT_DEFAULT[i]) == i`, and the live 808 mapping is
 * `e->s808.slot[lane]`. So R6b's whole job is to key the NOTES by sound id
 * and let the engine's own step do the rest.
 *
 * I first wrote here that the 808 table was an unfinished identity table
 * mapping rim shot onto low conga, and used it to call R6c blocked. Both were
 * wrong: I read the `RB808_*` slot macros as sound ids, and they are not.
 *
 * So the map is keyed by each machine's own sound id -- `RB808_*` slot or
 * `RB909_*` voice -- which is a sound identity and does not move when the
 * panel does. R6 therefore contains NO lane translation at all, and cannot
 * inherit a wrong one.
 *
 * THE NOTES THEMSELVES ARE A CONVENTION, not a fact. These are the General
 * MIDI percussion assignments, which is the conventional answer and the one
 * that makes a DAW show the right name in its drum editor. They are an
 * OWNER REVIEW ITEM, isolated in two functions so changing them is one edit
 * and one test rather than a search. t196 pins that they are in range,
 * distinct where the sounds are distinct, and refused -- never guessed --
 * for an id that does not exist.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_devout.h"

int main(void) {
    static struct RIDevOut d;
    uint8_t buf[8];
    uint32_t n;

    /* --- the 303 needs NO map: the engine already has one ------------- */
    /* `RI303Row.key` is a 0..12 SCALE DEGREE, and `ri_p303_note()` converts
     * it against RI_303_BASE_NOTE with an octave flag. Writing a second
     * 303 map in R6 would be a second answer to a question the engine has
     * already answered, and the two would drift. */
    RI_ASSERT(ri_devout_needs_303_map() == 0,
        "the 303 note comes from the engine's own ri_p303_note(), not from here");

    /* --- the 808 map, keyed by RB808_* slot --------------------------- */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 0u, 9u);              /* the 808 voice channel */
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_BD);
    RI_ASSERT(n == 3u && buf[1] == 36u, "808 bass drum is GM 36 (%u)",
        (unsigned)buf[1]);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_SD);
    RI_ASSERT(n == 3u && buf[1] == 38u, "808 snare is GM 38 (%u)", (unsigned)buf[1]);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_CL);
    RI_ASSERT(n == 3u && buf[1] == 56u, "808 cowbell is GM 56 (%u)", (unsigned)buf[1]);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_CH);
    RI_ASSERT(n == 3u && buf[1] == 42u, "808 closed hat is GM 42 (%u)", (unsigned)buf[1]);

    /* --- the 909 map, keyed by RB909_* voice -------------------------- */
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_909, RB909_OH);
    RI_ASSERT(n == 3u && buf[1] == 46u, "909 open hat is GM 46 (%u)", (unsigned)buf[1]);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_909, RB909_CR);
    RI_ASSERT(n == 3u && buf[1] == 49u, "909 crash is GM 49 (%u)", (unsigned)buf[1]);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_909, RB909_RD);
    RI_ASSERT(n == 3u && buf[1] == 51u, "909 ride is GM 51 (%u)", (unsigned)buf[1]);

    /* --- the two maps are SEPARATE, not one table by index ------------ */
    /* This is the law the whole design exists to satisfy. If the two were
     * one table, the 808's cowbell and the 909's closed hat -- both at
     * "index 7" in their own enums -- would have to share a note. */
    {
        uint8_t a = 0u, b = 0u;
        n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_CL);
        a = buf[1];
        n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_909, RB909_CH);
        b = buf[1];
        RI_ASSERT(a == 56u && b == 42u,
            "808 cowbell and 909 closed hat are DIFFERENT notes (%u/%u)",
            (unsigned)a, (unsigned)b);
    }

    /* --- an unknown sound id is REFUSED, never guessed ---------------- */
    /* Defaulting an unknown id to the bass drum would put a hi-hat on the
     * kick: audible, and invisible in the log unless something counts it. */
    ri_devout_init(&d, 1u);
    ri_devout_assign(&d, 0u, 9u);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, 200u);
    RI_ASSERT(n == 0u, "an unknown 808 slot emits nothing (%u)", (unsigned)n);
    RI_ASSERT(ri_devout_refused(&d) == 1u, "and the refusal is counted (%u)",
        (unsigned)ri_devout_refused(&d));
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, 7u, RB808_BD);
    RI_ASSERT(n == 0u, "an unknown drum CLASS emits nothing (%u)", (unsigned)n);
    RI_ASSERT(ri_devout_refused(&d) == 2u, "and is counted too (%u)",
        (unsigned)ri_devout_refused(&d));

    /* --- the channel rules from R6a still apply ----------------------- */
    ri_devout_init(&d, 1u);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_BD);
    RI_ASSERT(n == 0u, "an unclaimed channel still refuses a drum hit (%u)",
        (unsigned)n);
    ri_devout_init(&d, 0u);
    ri_devout_assign(&d, 0u, 9u);
    n = ri_devout_drum(&d, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_BD);
    RI_ASSERT(n == 0u, "and disabled still refuses it (%u)", (unsigned)n);

    /* --- every defined sound has a note, and every note is in range --- */
    {
        uint8_t slot, seen[128];
        for (slot = 0u; slot < 128u; slot++)
            seen[slot] = 0u;
        for (slot = RB808_BD; slot <= RB808_CH; slot++) {
            uint8_t note = ri_devout_drum808_note(slot);
            RI_ASSERT(note <= 127u && note > 0u, "808 slot %u has a note (%u)",
                (unsigned)slot, (unsigned)note);
            seen[note]++;
        }
        for (slot = RB909_BD; slot <= RB909_CP; slot++) {
            uint8_t note = ri_devout_drum909_note(slot);
            RI_ASSERT(note <= 127u && note > 0u, "909 voice %u has a note (%u)",
                (unsigned)slot, (unsigned)note);
            seen[note]++;
        }
        /* Distinct sounds must not collide on one note. WITHIN a machine
         * they may not: two sounds on one GM note means a drum editor shows
         * one name and the wrong one plays. ACROSS machines sharing is fine
         * and expected -- the same GM note legitimately means the same drum
         * on both -- so only the within-machine case is pinned. */
        {
            uint8_t x, y;
            for (x = RB808_BD; x <= RB808_CH; x++) {
                for (y = (uint8_t)(x + 1u); y <= RB808_CH; y++) {
                    RI_ASSERT(ri_devout_drum808_note(x) != ri_devout_drum808_note(y),
                        "808 slots %u and %u share note %u", (unsigned)x, (unsigned)y,
                        (unsigned)ri_devout_drum808_note(x));
                }
            }
            for (x = RB909_BD; x <= RB909_CP; x++) {
                for (y = (uint8_t)(x + 1u); y <= RB909_CP; y++) {
                    RI_ASSERT(ri_devout_drum909_note(x) != ri_devout_drum909_note(y),
                        "909 voices %u and %u share note %u", (unsigned)x, (unsigned)y,
                        (unsigned)ri_devout_drum909_note(x));
                }
            }
        }
    }

    /* --- nulls are inert ----------------------------------------------- */
    RI_ASSERT(ri_devout_drum(0, buf, sizeof buf, 9u, RI_DRUM_CLASS_808, RB808_BD) == 0u,
        "NULL sink");
    RI_ASSERT(ri_devout_drum(&d, 0, 8u, 9u, RI_DRUM_CLASS_808, RB808_BD) == 0u, "NULL buffer");

    RI_RESULT("devout-map");
}