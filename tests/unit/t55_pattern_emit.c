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

    /* ---- Task 7: drum emit path ---- */

    /* 808 BD on 0/4/8/12 + AC on 4: 4 NOTE_ONs + 1 total ACCENT,
     * ACCENT sorted after the step-4 NOTE_ON (type 3 < 5). */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, k, ons = 0, acs = 0, ac_pos = 0, on4_pos = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 16);
        ri_pdrum_set(&p, 0, RI_L808_BD, RI_HIT_LOW);
        ri_pdrum_set(&p, 4, RI_L808_BD, RI_HIT_LOW);
        ri_pdrum_set(&p, 8, RI_L808_BD, RI_HIT_LOW);
        ri_pdrum_set(&p, 12, RI_L808_BD, RI_HIT_LOW);
        ri_pdrum_set_ac(&p, 4, 1);
        n = ri_sched_emit_pattern(&p, 2, &MAP, 0, 96, 0, 0, 0, e, 64);
        RI_ASSERT(n == 5u, "d808 count %u", n);
        for (k = 0; k < n; k++) {
            if (e[k].type == RI_EV_NOTE_ON) {
                ons++;
                RI_ASSERT(e[k].voice == 0u && e[k].value == 0u,
                    "d808 on voice");
                RI_ASSERT(e[k].flags == 0u, "d808 on flags");
                if (e[k].sample == e[1].sample)
                    on4_pos = k;
            }
            if (e[k].type == RI_EV_ACCENT) {
                acs++;
                ac_pos = k;
                RI_ASSERT(e[k].voice == RI_VOICE_ALL, "d808 ac voice");
                RI_ASSERT(e[k].value == 1u, "d808 ac value");
            }
        }
        RI_ASSERT(ons == 4u && acs == 1u, "d808 kinds");
        RI_ASSERT(ac_pos > on4_pos, "d808 ac order");
    }

    /* 909 levels + flam: HIGH -> ACCENT flag; LOW -> none;
     * FLAM -> NOTE_ON (no ACCENT) + FLAM at +1680. */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        struct RISchedOpts fo = { 0, 0, RI_FLAM_MS_DEFAULT };
        uint32_t n, k, hi_acc = 99, lo_acc = 99, fl_on = 0, fl_ev = 0;
        uint64_t fl_s = 0, on_s = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
        ri_pattern_set_length(&p, 2);
        ri_pdrum_set(&p, 0, RI_L909_CH, RI_HIT_HIGH);
        ri_pdrum_set(&p, 1, RI_L909_CH, RI_HIT_LOW);
        ri_pdrum_set(&p, 1, RI_L909_SD, RI_HIT_FLAM);
        n = ri_sched_emit_pattern(&p, 3, &MAP, 0, 96, &fo, 0, 0, e, 64);
        for (k = 0; k < n; k++) {
            if (e[k].type == RI_EV_NOTE_ON && e[k].voice == 7u &&
                e[k].sample == 0u)
                hi_acc = e[k].flags;
            if (e[k].type == RI_EV_NOTE_ON && e[k].voice == 7u &&
                e[k].sample != 0u)
                lo_acc = e[k].flags;
            if (e[k].type == RI_EV_NOTE_ON && e[k].voice == 1u) {
                fl_on = 1;
                on_s = e[k].sample;
                RI_ASSERT((e[k].flags & RI_EVFLAG_ACCENT) == 0u,
                    "d909 flam accented");
            }
            if (e[k].type == RI_EV_FLAM && e[k].voice == 1u) {
                fl_ev = 1;
                fl_s = e[k].sample;
                RI_ASSERT(e[k].flags == RI_EVFLAG_FLAM2, "d909 flam2");
                RI_ASSERT(e[k].value == 1680u, "d909 flam val %u",
                    e[k].value);
            }
        }
        RI_ASSERT(hi_acc == RI_EVFLAG_ACCENT, "d909 high flags %u",
            hi_acc);
        RI_ASSERT(lo_acc == 0u, "d909 low flags %u", lo_acc);
        RI_ASSERT(fl_on && fl_ev, "d909 flam pair");
        RI_ASSERT(fl_s == on_s + 1680u, "d909 flam time");
    }

    /* OH + CH same step: both NOTE_ONs present (voice rules later). */
    {
        struct RIPattern p;
        struct RIEvent e[64];
        uint32_t n, k, oh = 0, ch = 0;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
        ri_pattern_set_length(&p, 1);
        ri_pdrum_set(&p, 0, RI_L909_OH, RI_HIT_LOW);
        ri_pdrum_set(&p, 0, RI_L909_CH, RI_HIT_LOW);
        n = ri_sched_emit_pattern(&p, 3, &MAP, 0, 96, 0, 0, 0, e, 64);
        RI_ASSERT(n == 2u, "d909 ohch count %u", n);
        for (k = 0; k < n; k++) {
            if (e[k].type == RI_EV_NOTE_ON && e[k].voice == 8u)
                oh = 1;
            if (e[k].type == RI_EV_NOTE_ON && e[k].voice == 7u)
                ch = 1;
        }
        RI_ASSERT(oh && ch, "d909 ohch voices");
    }

    /* Shuffle parity with the 303 walker (same length, shuffle 50). */
    {
        struct RIPattern dp;
        struct RIStep s[4];
        struct RIEvent e1[64], e2[64];
        struct RISchedOpts o = { 50, 0, RI_FLAM_MS_DEFAULT };
        uint32_t n1, n2;
        ri_pattern_init(&dp, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&dp, 4);
        ri_pdrum_set(&dp, 1, RI_L808_BD, RI_HIT_LOW);
        n1 = ri_sched_emit_pattern(&dp, 2, &MAP, 0, 96, &o, 0, 0, e1,
            64);
        s[0].note = 60; s[0].flags = RI_STEP_REST;
        s[1].note = 60; s[1].flags = 0;
        s[2].note = 60; s[2].flags = RI_STEP_REST;
        s[3].note = 60; s[3].flags = RI_STEP_REST;
        n2 = ri_sched_emit_timed(&MAP, 0, 96, s, 4, 0, &o, e2, 64);
        RI_ASSERT(n1 == 1u && n2 >= 1u, "dshuffle counts");
        {
            uint32_t k2, found = 0;
            for (k2 = 0; k2 < n2; k2++)
                if (e2[k2].type == RI_EV_NOTE_ON) {
                    found = 1;
                    RI_ASSERT(e1[0].sample == e2[k2].sample,
                        "dshuffle %llu vs %llu",
                        (unsigned long long)e1[0].sample,
                        (unsigned long long)e2[k2].sample);
                }
            RI_ASSERT(found, "dshuffle no 303 ON");
        }
    }

    /* Length gate + cap determinism + corrupt fail-closed. */
    {
        struct RIPattern p;
        struct RIEvent e1[64], e2[64];
        uint32_t n1, n2, k;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 7);
        for (k = 0; k < 16u; k++)
            ri_pdrum_set(&p, k, RI_L808_BD, RI_HIT_LOW);
        n1 = ri_sched_emit_pattern(&p, 2, &MAP, 0, 96, 0, 0, 0, e1,
            64);
        RI_ASSERT(n1 == 7u, "dlen count %u", n1);
        n1 = ri_sched_emit_pattern(&p, 2, &MAP, 0, 96, 0, 0, 0, e1, 5);
        n2 = ri_sched_emit_pattern(&p, 2, &MAP, 0, 96, 0, 0, 0, e2, 5);
        RI_ASSERT(n1 == n2, "dcap counts");
        RI_ASSERT(memcmp(e1, e2, n1 * sizeof(struct RIEvent)) == 0,
            "dcap determinism");
        p.row.drum[0].high = 0x0001u; /* hand-corrupted 808 */
        n1 = ri_sched_emit_pattern(&p, 2, &MAP, 0, 96, 0, 0, 0, e1,
            64);
        RI_ASSERT(n1 == 0u, "dcorrupt events %u", n1);
    }

    RI_RESULT("pattern_emit");
}
