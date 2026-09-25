/* t55_pattern_emit — §12.7a Tasks 5-7: walker octave/carry + 303/drum emit.
 * Task 5 first (RED: carry API + octave emission do not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"
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

    /* ---- Task 6: 303 converter + emit (cases 1-7; case 8 in Task 8) ---- */

    /* Case 1: row 0 C + SLIDE, row 1 E -> tied (no OFF between). */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, offs_between = 0, k;
        uint64_t s0 = 0, s1 = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&p, 2);
        ri_p303_set(&p, 0, 0, RI_STEP_SLIDE);
        ri_p303_set(&p, 1, 4, 0);
        n = ri_sched_emit_pattern(&p, 0, &MAP, 0, 96, 0, 0, 0, e, 64);
        for (k = 0; k < n; k++) {
            if (e[k].type == RI_EV_NOTE_ON && e[k].value == 36u)
                s0 = e[k].sample;
            if (e[k].type == RI_EV_NOTE_ON && e[k].value == 40u) {
                s1 = e[k].sample;
                RI_ASSERT((e[k].flags & RI_EVFLAG_SLIDE) != 0u,
                    "case1 no SLIDE");
            }
            if (e[k].type == RI_EV_NOTE_OFF && e[k].sample > s0 &&
                e[k].sample < s1)
                offs_between++;
        }
        RI_ASSERT(s0 != s1 && s1 > s0, "case1 order");
        RI_ASSERT(offs_between == 0u, "case1 OFF between");
    }

    /* Case 2: slide flag on row 1 instead -> break + plain attack. */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, k, plain_on = 0, frac_off = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&p, 2);
        ri_p303_set(&p, 0, 0, 0);
        ri_p303_set(&p, 1, 4, RI_STEP_SLIDE);
        n = ri_sched_emit_pattern(&p, 0, &MAP, 0, 96, 0, 0, 0, e, 64);
        for (k = 0; k < n; k++) {
            if (e[k].type == RI_EV_NOTE_ON && e[k].value == 40u &&
                (e[k].flags & RI_EVFLAG_SLIDE) == 0u)
                plain_on = 1;
            if (e[k].type == RI_EV_NOTE_OFF && e[k].value == 36u)
                frac_off = 1;
        }
        RI_ASSERT(plain_on, "case2 no plain ON");
        RI_ASSERT(frac_off, "case2 no fractional OFF");
    }

    /* Case 3: SLIDE on a Pause row is inert (identical to plain Pause). */
    {
        struct RIPattern a, b;
        struct RIEvent ea[64], eb[64];
        uint32_t na, nb;
        ri_pattern_init(&a, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&a, 2);
        ri_p303_set(&a, 0, 0, 0);
        ri_p303_set(&a, 1, 0, RI_STEP_REST);
        memcpy(&b, &a, sizeof b);
        ri_p303_set(&b, 1, 0,
            (uint8_t)(RI_STEP_REST | RI_STEP_SLIDE));
        na = ri_sched_emit_pattern(&a, 0, &MAP, 0, 96, 0, 0, 0, ea, 64);
        nb = ri_sched_emit_pattern(&b, 0, &MAP, 0, 96, 0, 0, 0, eb, 64);
        RI_ASSERT(na == nb, "case3 count %u vs %u", na, nb);
        RI_ASSERT(memcmp(ea, eb, na * sizeof(struct RIEvent)) == 0,
            "case3 differs");
    }

    /* Case 4: ACCENT on a Pause row emits no ACCENT. */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, k, accents = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&p, 2);
        ri_p303_set(&p, 0, 0, 0);
        ri_p303_set(&p, 1, 0,
            (uint8_t)(RI_STEP_REST | RI_STEP_ACCENT));
        n = ri_sched_emit_pattern(&p, 0, &MAP, 0, 96, 0, 0, 0, e, 64);
        for (k = 0; k < n; k++)
            if (e[k].type == RI_EV_ACCENT)
                accents++;
        RI_ASSERT(accents == 0u, "case4 accents %u", accents);
    }

    /* Case 5: octave rows (Up key0 -> 48+OCT; Down key12 -> 36+OCT;
     * both -> 36 no OCT). */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, k, f48 = 99, f36d = 99, f36b = 99, v;
        ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&p, 4);
        ri_p303_set(&p, 0, 0, RI_STEP_UP);
        ri_p303_set(&p, 1, 12, RI_STEP_DOWN);
        ri_p303_set(&p, 2, 5, (uint8_t)(RI_STEP_UP | RI_STEP_DOWN));
        ri_p303_set(&p, 3, 0, RI_STEP_REST);
        n = ri_sched_emit_pattern(&p, 0, &MAP, 0, 96, 0, 0, 0, e, 64);
        for (k = 0; k < n; k++) {
            if (e[k].type != RI_EV_NOTE_ON)
                continue;
            v = e[k].value;
            if (v == 48u)
                f48 = e[k].flags;
            else if (v == 36u && e[k].sample > 0u &&
                f36d == 99u)
                f36d = e[k].flags;
        }
        RI_ASSERT(f48 != 99u && (f48 & RI_EVFLAG_OCTAVE) != 0u,
            "case5 up");
        RI_ASSERT(f36d != 99u && (f36d & RI_EVFLAG_OCTAVE) != 0u,
            "case5 down");
        /* Both-set row emits 36 without OCTAVE (third NOTE_ON). */
        {
            uint32_t ons = 0;
            for (k = 0; k < n; k++)
                if (e[k].type == RI_EV_NOTE_ON && e[k].value == 41u)
                    ons++;
            RI_ASSERT(ons == 1u, "case5 both note");
            for (k = 0; k < n; k++)
                if (e[k].type == RI_EV_NOTE_ON && e[k].value == 41u)
                    f36b = e[k].flags;
            RI_ASSERT((f36b & RI_EVFLAG_OCTAVE) == 0u, "case5 both oct");
        }
    }

    /* Case 6: length 5, data beyond -> only steps 0-4 emit. */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, k;
        ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&p, 5);
        ri_p303_set(&p, 0, 7, 0);
        for (k = 5u; k < 16u; k++)
            ri_p303_set(&p, k, 3, RI_STEP_ACCENT);
        n = ri_sched_emit_pattern(&p, 0, &MAP, 0, 96, 0, 0, 0, e, 64);
        RI_ASSERT(n == 2u, "case6 count %u", n); /* ON + frac OFF */
        RI_ASSERT(e[0].type == RI_EV_NOTE_ON && e[0].value == 43u,
            "case6 on");
        RI_ASSERT(e[1].type == RI_EV_NOTE_OFF && e[1].value == 43u,
            "case6 off");
    }

    /* Case 7: cyclic seam across two iterations via carry. */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        struct RISchedCarry co = { 0, 0, { 0, 0 } };
        uint32_t n, k, seam_on = 0, offs_after = 0;
        uint64_t last_s = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
        ri_pattern_set_length(&p, 2);
        ri_p303_set(&p, 0, 0, 0);
        ri_p303_set(&p, 1, 4, RI_STEP_SLIDE);
        n = ri_sched_emit_pattern(&p, 0, &MAP, 0, 96, 0, 0, &co, e,
            64);
        RI_ASSERT(co.valid == 1u && co.held_note == 40u, "case7 carry");
        /* Iteration B starts at the pattern tick length (2 steps). */
        n = ri_sched_emit_pattern(&p, 0, &MAP, 48, 96, 0, &co, 0, e,
            64);
        for (k = 0; k < n; k++) {
            if (e[k].type == RI_EV_NOTE_ON && e[k].value == 36u) {
                seam_on = 1;
                RI_ASSERT((e[k].flags & RI_EVFLAG_SLIDE) != 0u,
                    "case7 no seam SLIDE");
                last_s = e[k].sample;
            }
            if (e[k].type == RI_EV_NOTE_OFF && e[k].sample > last_s &&
                last_s != 0u)
                offs_after++;
        }
        RI_ASSERT(seam_on, "case7 no seam ON");
        /* Exactly one OFF after the seam ON: step 1 is a new attack. */
        RI_ASSERT(offs_after == 1u, "case7 offs %u", offs_after);
    }

    RI_RESULT("pattern_emit");
}
