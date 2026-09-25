/* t54_pattern_edit — §12.7a Task 4: ReBirth Edit menu ops (p. 51-54).
 * RED-first: none of the edit APIs exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"

static void fill303(struct RIPattern *p) {
    uint32_t i;
    ri_pattern_init(p, RI_PATTERN_KIND_303, 0);
    for (i = 0; i < 16u; i++)
        ri_p303_set(p, i, (uint8_t)(i % 13u),
            (uint8_t)(i & 1u ? RI_STEP_ACCENT : RI_STEP_SLIDE));
}

static void sort_rows(struct RI303Row *r, uint32_t n) {
    uint32_t i, j;
    for (i = 0; i < n; i++)
        for (j = i + 1u; j < n; j++)
            if (r[j].key < r[i].key ||
                (r[j].key == r[i].key && r[j].flags < r[i].flags)) {
                struct RI303Row t = r[i];
                r[i] = r[j];
                r[j] = t;
            }
}

int main(void) {
    struct RIPattern p, snap;
    struct RIPatternBank b;
    uint32_t i, nf = 0;

    /* Clear: rows to rest/empty, length kept. */
    fill303(&p);
    ri_pattern_set_length(&p, 11);
    RI_ASSERT(ri_pattern_clear(&p) == 0, "clear rc");
    RI_ASSERT(p.length == 11u, "clear length");
    for (i = 0; i < 16u; i++)
        RI_ASSERT(p.row.r303[i].key == 0u &&
            p.row.r303[i].flags == RI_STEP_REST, "clear row %u", i);
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_pdrum_set(&p, 2, 4, RI_HIT_HIGH);
    ri_pdrum_set_ac(&p, 2, 1);
    ri_pattern_set_length(&p, 7);
    RI_ASSERT(ri_pattern_clear(&p) == 0, "drum clear rc");
    RI_ASSERT(p.length == 7u, "drum clear length");
    for (i = 0; i < 16u; i++)
        RI_ASSERT(p.row.drum[i].on == 0u && p.row.drum[i].high == 0u &&
            p.row.drum[i].flam == 0u && p.row.drum[i].flags == 0u,
            "drum clear row %u", i);
    RI_ASSERT(ri_pattern_clear(0) == 2, "clear null");

    /* Copy/Cut/Paste + refusals (byte-identical state). */
    ri_bank_init(&b, 0, RI_PATTERN_KIND_303, 0);
    fill303(&b.pat[3]);
    memcpy(&snap, &b.pat[3], sizeof snap);
    RI_ASSERT(ri_bank_copy(&b, 3, &b, 9) == 0, "copy rc");
    RI_ASSERT(memcmp(&snap, &b.pat[9], sizeof snap) == 0, "copy payload");
    RI_ASSERT(ri_bank_copy(&b, 3, &b, 32) == 2, "copy slot 32");
    RI_ASSERT(ri_bank_copy(&b, 32, &b, 3) == 2, "copy from 32");
    RI_ASSERT(ri_bank_cut(&b, 3, &p) == 0, "cut rc");
    RI_ASSERT(memcmp(&snap, &p, sizeof snap) == 0, "cut clip");
    for (i = 0; i < 16u; i++)
        RI_ASSERT(b.pat[3].row.r303[i].key == 0u &&
            b.pat[3].row.r303[i].flags == RI_STEP_REST, "cut clear %u", i);
    RI_ASSERT(ri_bank_paste(&b, 5, &p) == 0, "paste rc");
    RI_ASSERT(memcmp(&snap, &b.pat[5], sizeof snap) == 0, "paste payload");
    {
        struct RIPatternBank d9;
        struct RIPatternBank snapb;
        ri_bank_init(&d9, 2, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
        memcpy(&snapb, &b, sizeof snapb);
        RI_ASSERT(ri_bank_copy(&b, 5, &d9, 5) == 2, "cross-kind copy");
        RI_ASSERT(memcmp(&snapb, &b, sizeof snapb) == 0, "cross-kind b");
        RI_ASSERT(ri_bank_paste(&d9, 5, &p) == 2, "cross-kind paste");
        RI_ASSERT(ri_bank_cut(0, 0, &p) == 2, "cut null");
        RI_ASSERT(ri_bank_copy(0, 0, &b, 0) == 2, "copy null");
        RI_ASSERT(ri_bank_paste(&b, 0, 0) == 2, "paste null");
    }

    /* Shift ignores length; 16x = identity; L o R = identity. */
    ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
    ri_pattern_set_length(&p, 5);
    ri_p303_set(&p, 10, 9, RI_STEP_ACCENT);
    ri_p303_set(&p, 15, 4, RI_STEP_SLIDE);
    RI_ASSERT(ri_pattern_shift(&p, 1) == 0, "shift R");
    RI_ASSERT(p.row.r303[11].key == 9u &&
        p.row.r303[11].flags == RI_STEP_ACCENT, "row10 -> 11");
    RI_ASSERT(p.row.r303[0].key == 4u &&
        p.row.r303[0].flags == RI_STEP_SLIDE, "slide 15 -> 0");
    RI_ASSERT(ri_pattern_shift(&p, -1) == 0, "shift L");
    RI_ASSERT(p.row.r303[10].key == 9u, "back to 10");
    RI_ASSERT(p.row.r303[15].key == 4u, "slide back to 15");
    memcpy(&snap, &p, sizeof snap);
    for (i = 0; i < 16u; i++)
        RI_ASSERT(ri_pattern_shift(&p, 1) == 0, "shift16 %u", i);
    RI_ASSERT(memcmp(&snap, &p, sizeof snap) == 0, "16x != identity");
    RI_ASSERT(ri_pattern_shift(&p, 0) == 2, "shift dir 0");
    RI_ASSERT(ri_pattern_shift(&p, 2) == 2, "shift dir 2");
    RI_ASSERT(ri_pattern_shift(0, 1) == 2, "shift null");

    /* Shift Drum: one lane only. */
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_pdrum_set(&p, 3, 2, RI_HIT_HIGH);
    ri_pdrum_set(&p, 3, 5, RI_HIT_LOW);
    ri_pdrum_set_ac(&p, 3, 1);
    RI_ASSERT(ri_pdrum_shift_lane(&p, 2, 1) == 0, "lane shift");
    RI_ASSERT(ri_pdrum_get(&p, 4, 2) == RI_HIT_HIGH, "lane moved");
    RI_ASSERT(ri_pdrum_get(&p, 3, 2) == RI_HIT_OFF, "lane vacated");
    RI_ASSERT(ri_pdrum_get(&p, 3, 5) == RI_HIT_LOW, "other lane moved!");
    RI_ASSERT((p.row.drum[3].flags & RI_DRUM_AC) != 0u, "AC moved!");
    RI_ASSERT((p.row.drum[4].flags & RI_DRUM_AC) == 0u, "AC copied!");
    RI_ASSERT(ri_pdrum_shift_lane(&p, 11, 1) == 2, "lane 11");
    RI_ASSERT(ri_pdrum_shift_lane(&p, 2, 0) == 2, "lane dir 0");
    fill303(&snap);
    memcpy(&p, &snap, sizeof snap);
    RI_ASSERT(ri_pdrum_shift_lane(&p, 0, 1) == 2, "lane on 303");
    RI_ASSERT(memcmp(&snap, &p, sizeof snap) == 0, "303 mutated");

    /* Transpose. */
    ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
    ri_p303_set(&p, 0, 12, RI_STEP_UP); /* semi 24 */
    ri_p303_set(&p, 1, 11, 0); /* semi 11 */
    ri_p303_set(&p, 2, 0, 0); /* semi 0 */
    ri_p303_set(&p, 3, 5, RI_STEP_REST); /* rest: silent */
    nf = 0;
    RI_ASSERT(ri_p303_transpose(&p, 12, &nf) == 0, "tr rc");
    RI_ASSERT(nf == 1u, "nfolded %u", nf);
    RI_ASSERT(p.row.r303[0].key == 12u &&
        p.row.r303[0].flags == RI_STEP_UP, "24+12 folds to 24");
    RI_ASSERT(p.row.r303[1].key == 11u &&
        p.row.r303[1].flags == RI_STEP_UP, "11+12=23 up");
    RI_ASSERT(p.row.r303[2].key == 12u && p.row.r303[2].flags == 0u,
        "0+12 plain");
    RI_ASSERT(p.row.r303[3].key == 5u, "rest transposed!");
    nf = 0;
    RI_ASSERT(ri_p303_transpose(&p, -1, &nf) == 0, "tr -1");
    RI_ASSERT(p.row.r303[2].key == 11u && p.row.r303[2].flags == 0u,
        "-1 from 12");
    RI_ASSERT(ri_p303_transpose(&p, 13, &nf) == 2, "tr |13|");
    RI_ASSERT(ri_p303_transpose(&p, -13, &nf) == 2, "tr |-13|");
    RI_ASSERT(ri_p303_transpose(0, 1, &nf) == 2, "tr null");
    {
        struct RIPattern dp;
        ri_pattern_init(&dp, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
        RI_ASSERT(ri_p303_transpose(&dp, 1, &nf) == 2, "tr on drum");
    }
    /* Result never has Up+Down. */
    for (i = 0; i < 16u; i++)
        RI_ASSERT((p.row.r303[i].flags &
            (uint8_t)(RI_STEP_UP | RI_STEP_DOWN)) !=
            (uint8_t)(RI_STEP_UP | RI_STEP_DOWN), "both at %u", i);

    /* Random: determinism + scope. */
    {
        struct RIPattern a, b;
        fill303(&a);
        memcpy(&b, &a, sizeof b);
        RI_ASSERT(ri_p303_random(&a, RI_RND_PITCHES, 1) == 0, "rnd rc");
        memcpy(&snap, &a, sizeof snap);
        RI_ASSERT(ri_p303_random(&b, RI_RND_PITCHES, 1) == 0, "rnd rc2");
        RI_ASSERT(memcmp(&snap, &b, sizeof snap) == 0, "seed != det");
        fill303(&b);
        RI_ASSERT(ri_p303_random(&b, RI_RND_PITCHES, 2) == 0, "rnd seed2");
        RI_ASSERT(memcmp(&snap, &b, sizeof snap) != 0, "seed2 == seed1?");
        /* PITCHES leaves every flags byte identical to the fill. */
        fill303(&b);
        memcpy(&snap, &b, sizeof snap);
        ri_p303_random(&b, RI_RND_PITCHES, 7);
        for (i = 0; i < 16u; i++)
            RI_ASSERT(b.row.r303[i].flags == snap.row.r303[i].flags,
                "pitch rnd flags %u", i);
        /* ACCENTS leaves every key identical. */
        fill303(&b);
        memcpy(&snap, &b, sizeof snap);
        ri_p303_random(&b, RI_RND_ACCENTS, 7);
        for (i = 0; i < 16u; i++)
            RI_ASSERT(b.row.r303[i].key == snap.row.r303[i].key,
                "accent rnd key %u", i);
        RI_ASSERT(ri_p303_random(&b, 9, 7) == 2, "rnd bad what");
        RI_ASSERT(ri_p303_random(0, RI_RND_PITCHES, 7) == 2, "rnd null");
        /* Frozen seed-1 key stream (index 0 verified by hand against
         * the LCG: s1=1015568748, >>16=15496, 15496%13=0). */
        {
            static const uint8_t k1[16] = { 0, 7, 0, 6, 10, 10, 9, 11,
                1, 7, 7, 3, 10, 2, 0, 8 };
            ri_pattern_init(&b, RI_PATTERN_KIND_303, 0);
            ri_p303_random(&b, RI_RND_PITCHES, 1);
            for (i = 0; i < 16u; i++)
                RI_ASSERT(b.row.r303[i].key == k1[i], "k1[%u]=%u want %u",
                    i, b.row.r303[i].key, k1[i]);
        }
    }
    /* Drum random: one lane, valid states only. */
    {
        struct RIPattern dp;
        ri_pattern_init(&dp, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
        RI_ASSERT(ri_pdrum_random_lane(&dp, 4, 3) == 0, "drnd rc");
        RI_ASSERT(ri_pattern_valid(&dp) == 0, "drnd invalid");
        for (i = 0; i < 16u; i++) {
            uint32_t k;
            for (k = 0; k < 16u; k++) {
                if (k == 4u)
                    continue;
                RI_ASSERT(ri_pdrum_get(&dp, i, k) == RI_HIT_OFF,
                    "drnd lane %u", k);
            }
        }
        RI_ASSERT(ri_pdrum_random_lane(&dp, 0, 3) == 0, "lane 0 rc");
        /* Whole-pattern random on a drum kind is refused (OPEN row). */
        RI_ASSERT(ri_p303_random(&dp, RI_RND_PATTERN, 3) == 2,
            "drum whole-pattern random");
        RI_ASSERT(ri_pdrum_random_lane(&dp, 16, 3) == 2, "drnd lane 16");
    }
    /* Alter: empty stays empty; multiset preserved; column variants. */
    {
        struct RIPattern ap, bp;
        struct RI303Row sa[16], sb[16];
        uint32_t cnt_a[4], cnt_b[4], k;
        ri_pattern_init(&ap, RI_PATTERN_KIND_303, 0);
        memcpy(&bp, &ap, sizeof bp);
        RI_ASSERT(ri_p303_alter(&ap, RI_RND_PATTERN, 5) == 0, "alter rc");
        RI_ASSERT(memcmp(&ap, &bp, sizeof ap) == 0, "empty altered!");
        fill303(&ap);
        memcpy(sb, ap.row.r303, sizeof sb);
        sort_rows(sb, 16);
        RI_ASSERT(ri_p303_alter(&ap, RI_RND_PATTERN, 5) == 0, "alter2");
        memcpy(sa, ap.row.r303, sizeof sa);
        sort_rows(sa, 16);
        RI_ASSERT(memcmp(sa, sb, sizeof sa) == 0, "multiset changed");
        /* Pitches-only alter preserves the flags column exactly. */
        fill303(&ap);
        memcpy(&bp, &ap, sizeof bp);
        RI_ASSERT(ri_p303_alter(&ap, RI_RND_PITCHES, 5) == 0, "alter p");
        for (i = 0; i < 16u; i++)
            RI_ASSERT(ap.row.r303[i].flags == bp.row.r303[i].flags,
                "alter p flags %u", i);
        RI_ASSERT(ri_p303_alter(&ap, 9, 5) == 2, "alter bad what");
        RI_ASSERT(ri_p303_alter(0, RI_RND_PATTERN, 5) == 2, "alter null");
        /* Drum lane alter preserves per-state hit counts. */
        {
            struct RIPattern dp;
            ri_pattern_init(&dp, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
            ri_pdrum_random_lane(&dp, 6, 11);
            for (k = 0; k < 4u; k++) {
                cnt_a[k] = 0;
                for (i = 0; i < 16u; i++)
                    if (ri_pdrum_get(&dp, i, 6) == k)
                        cnt_a[k]++;
            }
            RI_ASSERT(ri_pdrum_alter_lane(&dp, 6, 11) == 0, "d alter");
            for (k = 0; k < 4u; k++) {
                cnt_b[k] = 0;
                for (i = 0; i < 16u; i++)
                    if (ri_pdrum_get(&dp, i, 6) == k)
                        cnt_b[k]++;
                RI_ASSERT(cnt_a[k] == cnt_b[k], "d counts %u", k);
            }
            RI_ASSERT(ri_pdrum_alter_lane(&dp, 16, 11) == 2, "d lane 16");
        }
    }

    RI_RESULT("pattern_edit");
}
