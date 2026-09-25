/* t55_pattern_emit — §12.7a Tasks 5-7: walker octave/carry + 303/drum emit.
 * Task 5 first (RED: carry API + octave emission do not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };
static struct RIEvent OA[256], OB[256];

static void steps4(struct RIStep *s) {
    s[0].note = 60; s[0].flags = 0;
    s[1].note = 62; s[1].flags = RI_STEP_SLIDE;
    s[2].note = 64; s[2].flags = RI_STEP_ACCENT;
    s[3].note = 65; s[3].flags = RI_STEP_REST;
}

int main(void) {
    struct RIStep s[4];
    uint32_t na, nb, i, ppq;

    /* Octave flag: Up/Down/none/both on a single note step. */
    {
        struct RIStep o[1];
        o[0].note = 60;
        o[0].flags = 0;
        na = ri_sched_emit_timed(&MAP, 0, 96, o, 1, 0, 0, OA, 256);
        RI_ASSERT(na > 0, "no events");
        RI_ASSERT((OA[0].flags & RI_EVFLAG_OCTAVE) == 0u, "oct bare");
        o[0].flags = RI_STEP_UP;
        ri_sched_emit_timed(&MAP, 0, 96, o, 1, 0, 0, OA, 256);
        RI_ASSERT((OA[0].flags & RI_EVFLAG_OCTAVE) != 0u, "oct up");
        o[0].flags = RI_STEP_DOWN;
        ri_sched_emit_timed(&MAP, 0, 96, o, 1, 0, 0, OA, 256);
        RI_ASSERT((OA[0].flags & RI_EVFLAG_OCTAVE) != 0u, "oct down");
        o[0].flags = (uint8_t)(RI_STEP_UP | RI_STEP_DOWN);
        ri_sched_emit_timed(&MAP, 0, 96, o, 1, 0, 0, OA, 256);
        RI_ASSERT((OA[0].flags & RI_EVFLAG_OCTAVE) == 0u, "oct both");
    }

    /* Gate-constant equivalence sweep (NUM/DEN form == /2 form). */
    for (ppq = 24u; ppq <= 960u; ppq++) {
        uint32_t st = ppq / 4u;
        if (st == 0u)
            st = 24u;
        RI_ASSERT(st * RI_SCHED_GATE_NUM / RI_SCHED_GATE_DEN == st / 2u,
            "gate ppq %u", ppq);
    }

    /* NULL carries reproduce timed byte-for-byte. */
    steps4(s);
    na = ri_sched_emit_timed(&MAP, 0, 96, s, 4, 1, 0, OA, 256);
    nb = ri_sched_emit_timed_carry(&MAP, 0, 96, s, 4, 1, 0, 0, 0, OB,
        256);
    RI_ASSERT(na == nb, "carry count %u vs %u", na, nb);
    RI_ASSERT(memcmp(OA, OB, na * sizeof(struct RIEvent)) == 0,
        "carry != timed");

    /* Carry out: last step TIE_OUT suppresses the final OFF. */
    {
        struct RISchedCarry co = { 0, 0, { 0, 0 } };
        struct RIStep t[2];
        t[0].note = 60; t[0].flags = 0;
        t[1].note = 62; t[1].flags = RI_STEP_TIE_OUT;
        na = ri_sched_emit_timed_carry(&MAP, 0, 96, t, 2, 0, 0, 0, &co,
            OA, 256);
        RI_ASSERT(co.valid == 1u, "carry invalid");
        RI_ASSERT(co.held_note == 62u, "carry held %u", co.held_note);
        /* No NOTE_OFF with value 62 anywhere after the last NOTE_ON. */
        {
            uint32_t last_on = 0;
            for (i = 0; i < na; i++)
                if (OA[i].type == RI_EV_NOTE_ON && OA[i].value == 62u)
                    last_on = i;
            for (i = last_on + 1u; i < na; i++)
                RI_ASSERT(!(OA[i].type == RI_EV_NOTE_OFF &&
                    OA[i].value == 62u), "held OFF at %u", i);
        }
    }

    /* Carry in: step-0 note ties (SLIDE, no OFF before it). */
    {
        struct RISchedCarry ci = { 1, 62, { 0, 0 } };
        struct RISchedCarry co = { 0, 0, { 0, 0 } };
        struct RIStep t[2];
        t[0].note = 64; t[0].flags = 0;
        t[1].note = 64; t[1].flags = RI_STEP_REST;
        na = ri_sched_emit_timed_carry(&MAP, 0, 96, t, 2, 0, 0, &ci,
            &co, OA, 256);
        RI_ASSERT(OA[0].type == RI_EV_NOTE_ON, "first not ON");
        RI_ASSERT((OA[0].flags & RI_EVFLAG_SLIDE) != 0u, "no carry SLIDE");
        RI_ASSERT(co.valid == 0u, "carry out set (gate fell)");
        /* Rest step ends the gate: exactly one OFF for the held note. */
        {
            uint32_t offs = 0;
            for (i = 0; i < na; i++)
                if (OA[i].type == RI_EV_NOTE_OFF)
                    offs++;
            RI_ASSERT(offs == 1u, "offs %u", offs);
        }
    }

    RI_RESULT("pattern_emit");
}
