/* t201_devout_conventions — R6e: the five owner decisions, pinned.
 *
 * These are CONVENTIONS, not mechanics, and every one of them is a place
 * where a plausible-looking change is silently audible somewhere else. So
 * each is pinned here with the reason it was decided, and each has a mutant
 * that undoes it.
 *
 *  1. THE 303 GOES OUT AS-IS, NEVER TRANSPOSED. `RI_303_BASE_NOTE` is 36 --
 *     key 0 is C2 -- and that is a settled machine decision with its own
 *     evidence file. A GM plugin puts key 0 near middle C, so the same
 *     pattern arrives in a DAW two octaves lower than a GM user expects.
 *     We keep it: the wire agreeing with the synth is worth more than the
 *     wire agreeing with a convention, and the cost is one session-level
 *     transpose in the DAW, done once.
 *
 *  2. DRUMS GO TO CHANNEL 10 AND IT IS NOT A CHOICE. GM defines percussion
 *     on channel 10 and nowhere else. A drum note on channel 3 selects a
 *     melodic instrument and plays a wrong pitched tone, or nothing. So
 *     `ri_devout_emit` OVERRIDES the channel for the 808 and the 909 and
 *     counts the override. Two consequences follow and both are pinned: a
 *     caller with no melodic channel assigned still gets drums out, and a
 *     caller that asks for drums on its own channel does not get them there.
 *
 *  3. VELOCITY IS 112 ACCENTED / 64 PLAIN, LIVE AND EXPORTED ALIKE. Already
 *     pinned by t198 against `ri_smf_velocity()`; restated here because it
 *     is now an owner decision rather than my placeholder, and because the
 *     303 row carries no level field -- two levels is the WHOLE dynamic
 *     range of this wire, not a stand-in for a bigger one.
 *
 *  4. A SLIDE IS SILENT. Legato, no re-attack, no bytes. The consequence --
 *     a DAW recording a slide holds the STARTING pitch and never learns the
 *     destination, so a recorded slide plays back at the wrong pitch -- is
 *     a real fidelity limit and is ledgered rather than hidden. Emitting
 *     pitch bend would fix it and make the wire stateful; the owner took
 *     "accept it".
 *
 *  5. ONE PROGRAM CHANGE ON ENABLE, ON THE MELODIC CHANNEL ONLY. Not on
 *     every note (that is a stream of noise) and not while disabled (E0).
 *     Drums get none, because on channel 10 the kit IS the program. The
 *     number is GM Electric Bass (pick) as the closest analogue to a 303
 *     line: GM has no 303 program, so any value is a convention, and this
 *     one is a named constant precisely so it is a one-line change.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"
#include "engine/seq/sched.h"
#include "midi_io/midi_devout.h"

#define CH 5u

static struct RIDevOut D;
static uint8_t B[8];

int main(void) {
    uint32_t w;

    ri_devout_init(&D, 1);
    ri_devout_assign(&D, 0u, CH);

    /* --- 1. THE 303 IS NOT TRANSPOSED ------------------------------- */
    {
        static struct RI303Row row[3];
        struct RIDevEvent x;
        struct RIEvent ev;
        memset(row, 0, sizeof row);
        /* key 0, then an octave up, then two keys up. */
        row[0].key = 0u;
        row[1].key = 0u;  row[1].flags = RI_STEP_UP;
        row[2].key = 2u;
        {
            uint32_t k;
            static const uint8_t want[3] = { 36u, 48u, 38u };
            for (k = 0u; k < 3u; k++) {
                memset(&ev, 0, sizeof ev);
                ev.type = RI_EV_NOTE_ON;
                ev.value = ri_p303_note(&row[k]);
                RI_ASSERT(ri_devout_translate(&ev, &x) == 1u, "translated");
                w = ri_devout_emit(&D, B, sizeof B, CH, RI_DEVOUT_303A, &x, 0u);
                RI_ASSERT(w == 3u, "emitted (%u)", (unsigned)w);
                RI_ASSERT(B[1] == want[k],
                    "303 key %u out as %u, NOT transposed to %u",
                    (unsigned)row[k].key, (unsigned)B[1],
                    (unsigned)(want[k] + 24u));
            }
        }
        RI_ASSERT(RI_303_BASE_NOTE == 36u,
            "the base itself is unchanged (%u)", (unsigned)RI_303_BASE_NOTE);
    }

    /* --- 2a. DRUMS GO TO CHANNEL 10 WHATEVER WAS ASKED FOR ----------- */
    {
        struct RIDevEvent x;
        memset(&x, 0, sizeof x);
        x.note = 60u;
        x.vel = ri_devout_velocity(0u);
        ri_devout_init(&D, 1);
        ri_devout_assign(&D, 0u, CH);
        /* The caller insists on channel 5 for a drum. */
        w = ri_devout_emit(&D, B, sizeof B, CH, RI_DEVOUT_808, &x, 0u);
        RI_ASSERT(w == 3u, "the 808 hit emitted (%u)", (unsigned)w);
        RI_ASSERT(B[0] == (uint8_t)(0x90u | RI_DEVOUT_GM_PERCUSSION),
            "on GM channel 10, not on the caller's %u (%02X)",
            (unsigned)CH, (unsigned)B[0]);
        RI_ASSERT(B[1] == 36u, "still the bass drum (%u)", (unsigned)B[1]);
        RI_ASSERT(ri_devout_override(&D) == 1u,
            "and the override is COUNTED (%u)", (unsigned)ri_devout_override(&D));
        /* And on a channel the caller owns, to prove it is the OVERRIDE and
         * not an accident of channel 5 being 10. */
        ri_devout_init(&D, 1);
        ri_devout_assign(&D, 0u, 3u);
        w = ri_devout_emit(&D, B, sizeof B, 3u, RI_DEVOUT_909, &x, 0u);
        RI_ASSERT(w == 3u && B[0] == (uint8_t)(0x90u | RI_DEVOUT_GM_PERCUSSION),
            "and the 909 too (%02X)", (unsigned)B[0]);
    }

    /* --- 2b. A MELODIC CHANNEL IS STILL REQUIRED FOR A MELODIC NOTE -- */
    /* Forcing drums to 10 must NOT quietly make an unassigned melodic
     * channel legal. An unclaimed channel is refused, never defaulted --
     * that is R6a's law and channel 0 is the G7 remote. */
    {
        struct RIDevEvent x;
        memset(&x, 0, sizeof x);
        x.note = 46u;
        x.vel = ri_devout_velocity(0u);
        ri_devout_init(&D, 1);       /* nothing assigned at all */
        RI_ASSERT(ri_devout_emit(&D, B, sizeof B, CH, RI_DEVOUT_303A, &x, 0u)
            == 0u, "an unclaimed melodic channel still refuses");
        /* But the drums need no melodic channel, and that is the point of
         * the decision: a user who only wants the 808 out does not have to
         * configure a melodic channel first. */
        RI_ASSERT(ri_devout_emit(&D, B, sizeof B, CH, RI_DEVOUT_808, &x, 0u)
            == 3u, "drums need no melodic channel assigned (%u)",
            (unsigned)w);
        RI_ASSERT(B[0] == (uint8_t)(0x90u | RI_DEVOUT_GM_PERCUSSION),
            "and land on channel 10 (%02X)", (unsigned)B[0]);
    }

    /* --- 3. TWO VELOCITIES, AND THEY MATCH THE EXPORT ---------------- */
    /* The 303 row carries no level field, so this is the whole range. */
    RI_ASSERT(ri_devout_velocity(0u) == 64u, "plain is 64 (%u)",
        (unsigned)ri_devout_velocity(0u));
    RI_ASSERT(ri_devout_velocity(RI_EVFLAG_ACCENT) == 112u, "accent is 112 (%u)",
        (unsigned)ri_devout_velocity(RI_EVFLAG_ACCENT));
    RI_ASSERT(ri_devout_velocity(0u) < ri_devout_velocity(RI_EVFLAG_ACCENT),
        "and accent is louder");

    /* --- 4. A SLIDE IS SILENT ---------------------------------------- */
    {
        struct RIDevEvent x;
        uint32_t legato_before, emitted_before;
        /* Section 2b re-inited with NOTHING assigned, and an unclaimed
         * channel is refused before the legato branch is even reached --
         * so a slide on it counts as a refusal, not a legato. Reassign, or
         * this asserts the wrong thing. */
        ri_devout_init(&D, 1);
        ri_devout_assign(&D, 0u, CH);
        memset(&x, 0, sizeof x);
        x.note = 42u;
        x.vel = ri_devout_velocity(0u);
        x.is_legato = 1u;
        legato_before = ri_devout_legato(&D);
        emitted_before = ri_devout_emitted(&D);
        w = ri_devout_emit(&D, B, sizeof B, CH, RI_DEVOUT_303A, &x, 0u);
        RI_ASSERT(w == 0u, "a slide emits nothing (%u)", (unsigned)w);
        RI_ASSERT(ri_devout_legato(&D) == legato_before + 1u,
            "and is counted as a legato (%u -> %u)",
            (unsigned)legato_before, (unsigned)ri_devout_legato(&D));
        RI_ASSERT(ri_devout_emitted(&D) == emitted_before,
            "and nothing was emitted (%u -> %u)", (unsigned)emitted_before,
            (unsigned)ri_devout_emitted(&D));
    }

    /* --- 5. ONE PROGRAM CHANGE, MELODIC CHANNEL ONLY, AND ON ENABLE -- */
    {
        uint8_t prog;
        uint32_t sent;
        ri_devout_init(&D, 1);
        ri_devout_assign(&D, 0u, CH);
        prog = ri_devout_program();
        /* Sent by the caller ONCE, on enable. While disabled it is nothing:
         * E0 means not one byte, and a program change is a byte. */
        ri_devout_init(&D, 0);
        ri_devout_assign(&D, 0u, CH);
        RI_ASSERT(ri_devout_program_change(&D, B, sizeof B, CH) == 0u,
            "a disabled producer sends no program change");
        ri_devout_enable(&D, 1);
        sent = ri_devout_program_change(&D, B, sizeof B, CH);
        RI_ASSERT(sent == 2u, "a program change is two bytes (%u)",
            (unsigned)sent);
        RI_ASSERT(B[0] == (uint8_t)(0xC0u | CH),
            "on the melodic channel %u (%02X)", (unsigned)CH,
            (unsigned)B[0]);
        RI_ASSERT(B[1] == prog, "carrying the program %u (%u vs %u)",
            (unsigned)prog, (unsigned)B[1], (unsigned)prog);
        RI_ASSERT(prog < 128u, "the program is in 0..127 (%u)", (unsigned)prog);
        RI_ASSERT(ri_devout_emitted(&D) == 1u,
            "and it counts as an emitted message (%u)",
            (unsigned)ri_devout_emitted(&D));
        /* A ONE-BYTE BUFFER IS NOT ENOUGH. Truncating 0xCn,pp to 0xCn
         * selects whatever program the receiving device last had, which is
         * the opposite of self-describing. */
        RI_ASSERT(ri_devout_program_change(&D, B, 1u, CH) == 0u,
            "a one-byte buffer is refused, not truncated");
        /* An unclaimed melodic channel refuses it too -- a program change
         * on the G7 remote would select an instrument behind the user's
         * back. */
        ri_devout_init(&D, 1);
        RI_ASSERT(ri_devout_program_change(&D, B, sizeof B, CH) == 0u,
            "an unclaimed channel refuses the program change");
        /* And an out-of-range channel is refused, never clamped.
         *
         * CHANNEL 8 IS CLAIMED FIRST, and that detail is the whole test:
         * my first version ran this against a producer with nothing
         * assigned, where the clamp lands on an unclaimed channel and the
         * claim check refuses it for a completely different reason. The
         * test could not tell clamping from refusing. */
        ri_devout_assign(&D, 0u, 8u);
        RI_ASSERT(ri_devout_program_change(&D, B, sizeof B, 200u) == 0u,
            "a channel above 15 is refused, not clamped");
        RI_ASSERT(ri_devout_program_change(&D, B, sizeof B, 8u) == 2u,
            "while channel 8 itself works, so the two are distinguishable");
        /* Nulls. */
        RI_ASSERT(ri_devout_program_change(0, B, sizeof B, CH) == 0u, "NULL d");
        RI_ASSERT(ri_devout_program_change(&D, 0, 8u, CH) == 0u, "NULL buf");
    }

    /* --- nulls on the new paths --------------------------------------- */
    RI_ASSERT(ri_devout_emit(&D, B, sizeof B, CH, RI_DEVOUT_808, 0, 0u) == 0u,
        "NULL event still refused");
    RI_RESULT("devout-conventions");
}