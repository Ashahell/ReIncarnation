/* t53_pattern_model — §12.7a Task 2+3: types, validation, click cycle,
 * lane tables, key<->note helpers.
 * RED-first: pattern.h does not exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"

int main(void) {
    struct RIPattern p, q;
    struct RIDrumRow before;
    struct RI303Row rbefore;
    uint32_t i;

    /* Flags namespace: UP/DOWN must not collide with the walker bits. */
    RI_ASSERT((RI_STEP_UP & 0x0Fu) == 0u, "UP collides");
    RI_ASSERT((RI_STEP_DOWN & 0x0Fu) == 0u, "DOWN collides");
    RI_ASSERT(RI_STEP_UP != RI_STEP_DOWN, "UP == DOWN");

    /* Layout budget (static, caller-owned). */
    RI_ASSERT(sizeof(struct RIPattern) == 132u, "pattern size %u",
        (unsigned)sizeof(struct RIPattern));
    RI_ASSERT(sizeof(struct RIPatternBank) == 4228u, "bank size %u",
        (unsigned)sizeof(struct RIPatternBank));

    /* Init defaults. */
    ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
    RI_ASSERT(p.length == 16u && p.payload_ver == 1u, "303 init header");
    for (i = 0; i < 16u; i++)
        RI_ASSERT(p.row.r303[i].key == 0u &&
            p.row.r303[i].flags == RI_STEP_REST, "303 init row %u", i);
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (i = 0; i < 16u; i++)
        RI_ASSERT(p.row.drum[i].on == 0u && p.row.drum[i].high == 0u &&
            p.row.drum[i].flam == 0u && p.row.drum[i].flags == 0u,
            "drum init row %u", i);
    RI_ASSERT(ri_pattern_valid(&p) == 0, "init drum invalid");

    /* Length clamps. */
    RI_ASSERT(ri_pattern_set_length(&p, 0) == 0 && p.length == 1u,
        "len 0");
    RI_ASSERT(ri_pattern_set_length(&p, 17) == 0 && p.length == 16u,
        "len 17");
    RI_ASSERT(ri_pattern_set_length(&p, 255) == 0 && p.length == 16u,
        "len 255");
    RI_ASSERT(ri_pattern_set_length(0, 8) == 2, "len null");

    /* Validation rules, one per rejection. */
    ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
    RI_ASSERT(ri_pattern_valid(&p) == 0, "init 303 invalid");
    RI_ASSERT(ri_pattern_valid(0) == 2, "null valid");
    p.kind = 2u;
    RI_ASSERT(ri_pattern_valid(&p) == 2, "kind 2");
    p.kind = RI_PATTERN_KIND_303;
    p.length = 0u;
    RI_ASSERT(ri_pattern_valid(&p) == 2, "len 0");
    p.length = 17u;
    RI_ASSERT(ri_pattern_valid(&p) == 2, "len 17");
    p.length = 16u;
    p.payload_ver = 2u;
    RI_ASSERT(ri_pattern_valid(&p) == 2, "ver 2");
    p.payload_ver = 1u;
    p.row.r303[3].key = 13u;
    RI_ASSERT(ri_pattern_valid(&p) == 2, "key 13");
    p.row.r303[3].key = 0u;
    p.row.r303[3].flags = 0x80u;
    RI_ASSERT(ri_pattern_valid(&p) == 2, "flag 0x80");
    p.row.r303[3].flags = RI_STEP_REST;

    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    p.row.drum[0].on = 0x0800u; /* lane 11 on a Classic class */
    RI_ASSERT(ri_pattern_valid(&p) == 2, "lane 11");
    p.row.drum[0].on = 0u;
    p.row.drum[0].high = 0x0001u; /* high without on */
    RI_ASSERT(ri_pattern_valid(&p) == 2, "high w/o on");
    p.row.drum[0].high = 0u;
    p.row.drum[0].flam = 0x0001u; /* flam without on */
    RI_ASSERT(ri_pattern_valid(&p) == 2, "flam w/o on");
    p.row.drum[0].flam = 0u;
    p.row.drum[0].on = 0x0001u;
    p.row.drum[0].high = 0x0001u;
    p.row.drum[0].flam = 0x0001u; /* high + flam */
    RI_ASSERT(ri_pattern_valid(&p) == 2, "high+flam");
    p.row.drum[0].on = 0x0001u;
    p.row.drum[0].high = 0x0001u;
    p.row.drum[0].flam = 0u; /* 808 with high */
    RI_ASSERT(ri_pattern_valid(&p) == 2, "808 high");
    p.row.drum[0].high = 0u;
    p.row.drum[0].flags = 0x02u; /* non-AC flag */
    RI_ASSERT(ri_pattern_valid(&p) == 2, "drum flag 0x02");
    p.row.drum[0].flags = 0u;
    RI_ASSERT(ri_pattern_valid(&p) == 0, "808 clean invalid");

    /* 303 setter refusals leave the row unchanged. */
    ri_pattern_init(&p, RI_PATTERN_KIND_303, 0);
    memcpy(&rbefore, &p.row.r303[2], 2);
    RI_ASSERT(ri_p303_set(&p, 2, 13, 0) == 2, "key 13 set");
    RI_ASSERT(memcmp(&rbefore, &p.row.r303[2], 2) == 0, "key13 mutated");
    RI_ASSERT(ri_p303_set(&p, 16, 5, 0) == 2, "step 16 set");
    RI_ASSERT(ri_p303_set(0, 0, 5, 0) == 2, "null set");
    ri_pattern_init(&q, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    RI_ASSERT(ri_p303_set(&q, 0, 5, 0) == 2, "303 set on drum");
    RI_ASSERT(ri_p303_set(&p, 2, 7, RI_STEP_ACCENT) == 0, "good set");
    RI_ASSERT(p.row.r303[2].key == 7u &&
        p.row.r303[2].flags == RI_STEP_ACCENT, "set stored");

    /* Drum setter encodings + 808 refusal. */
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    RI_ASSERT(ri_pdrum_set(&p, 0, 3, RI_HIT_HIGH) == 0, "high set");
    RI_ASSERT(p.row.drum[0].on == 0x0008u &&
        p.row.drum[0].high == 0x0008u &&
        p.row.drum[0].flam == 0u, "high encoding");
    RI_ASSERT(ri_pdrum_set(&p, 0, 3, RI_HIT_FLAM) == 0, "flam set");
    RI_ASSERT(p.row.drum[0].on == 0x0008u &&
        p.row.drum[0].high == 0u &&
        p.row.drum[0].flam == 0x0008u, "flam encoding");
    RI_ASSERT(ri_pdrum_set(&p, 0, 3, RI_HIT_OFF) == 0, "off set");
    RI_ASSERT(p.row.drum[0].on == 0u && p.row.drum[0].high == 0u &&
        p.row.drum[0].flam == 0u, "off encoding");
    RI_ASSERT(ri_pdrum_get(&p, 0, 3) == RI_HIT_OFF, "get off");
    ri_pdrum_set(&p, 0, 3, RI_HIT_LOW);
    RI_ASSERT(ri_pdrum_get(&p, 0, 3) == RI_HIT_LOW, "get low");
    memcpy(&before, &p.row.drum[0], sizeof before);
    RI_ASSERT(ri_pdrum_set(&p, 0, 16, RI_HIT_LOW) == 2, "lane 16");
    RI_ASSERT(ri_pdrum_set(&p, 16, 0, RI_HIT_LOW) == 2, "step 16");
    RI_ASSERT(memcmp(&before, &p.row.drum[0], sizeof before) == 0,
        "bad lane mutated");
    ri_pattern_init(&q, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    (void)q;
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    memcpy(&before, &p.row.drum[1], sizeof before);
    RI_ASSERT(ri_pdrum_set(&p, 1, 2, RI_HIT_HIGH) == 2, "808 high");
    RI_ASSERT(ri_pdrum_set(&p, 1, 2, RI_HIT_FLAM) == 2, "808 flam");
    RI_ASSERT(memcmp(&before, &p.row.drum[1], sizeof before) == 0,
        "808 mutated");
    RI_ASSERT(ri_pdrum_set_ac(&p, 4, 1) == 0, "ac on");
    RI_ASSERT((p.row.drum[4].flags & RI_DRUM_AC) != 0u, "ac stored");
    RI_ASSERT(ri_pdrum_set_ac(&p, 4, 0) == 0, "ac off");
    RI_ASSERT((p.row.drum[4].flags & RI_DRUM_AC) == 0u, "ac cleared");

    /* Click cycles (p. 30-31). */
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 0) == RI_HIT_LOW, "click1");
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 0) == RI_HIT_HIGH, "click2");
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 0) == RI_HIT_OFF, "click3");
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 1) == RI_HIT_FLAM, "fclick1");
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 1) == RI_HIT_OFF, "fclick2");
    ri_pdrum_set(&p, 0, 5, RI_HIT_HIGH);
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 1) == RI_HIT_OFF, "fclick from high");
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 0) == RI_HIT_LOW, "808 click1");
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 0) == RI_HIT_OFF, "808 click2");
    RI_ASSERT(ri_pdrum_click(&p, 0, 5, 1) == RI_HIT_LOW, "808 fclick");

    /* Lane tables. */
    for (i = 0; i < 11u; i++) {
        RI_ASSERT(RI_LANE_TO_RB808_SLOT[i] == (uint8_t)i, "808 lane %u", i);
        RI_ASSERT(rb808_slot_of(RI_808_SLOT_DEFAULT[i]) == i,
            "808 default %u", i);
    }
    {
        static const uint8_t want[11] = { 0, 1, 6, 7, 8, 9, 10, 2, 3, 4, 5 };
        for (i = 0; i < 11u; i++)
            RI_ASSERT(RI_LANE_TO_RB909_VOICE[i] == want[i], "909 lane %u",
                i);
    }

    /* Task 3: key <-> note helpers. */
    {
        static const uint8_t keys[13] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            11, 12 };
        uint8_t k, oct;
        for (i = 0; i < 13u; i++) {
            struct RI303Row r;
            r.key = keys[i];
            r.flags = 0;
            RI_ASSERT(ri_p303_octave(r.flags) == 0, "oct none");
            RI_ASSERT(ri_p303_semi(&r) == (int)keys[i], "semi none");
            RI_ASSERT(ri_p303_note(&r) == 36u + keys[i], "note none");
            r.flags = RI_STEP_UP;
            RI_ASSERT(ri_p303_octave(r.flags) == 1, "oct up");
            RI_ASSERT(ri_p303_note(&r) == 36u + keys[i] + 12u, "note up");
            r.flags = RI_STEP_DOWN;
            RI_ASSERT(ri_p303_octave(r.flags) == -1, "oct down");
            RI_ASSERT(ri_p303_note(&r) == 36u + keys[i] - 12u, "note dn");
            r.flags = (uint8_t)(RI_STEP_UP | RI_STEP_DOWN);
            RI_ASSERT(ri_p303_octave(r.flags) == 0, "oct both");
            RI_ASSERT(ri_p303_note(&r) == 36u + keys[i], "note both");
        }
        {
            int folded = 0;
            RI_ASSERT(ri_p303_fold(-13, &folded) == -1 && folded == 1,
                "fold -13");
            folded = 0;
            RI_ASSERT(ri_p303_fold(25, &folded) == 13 && folded == 1,
                "fold 25");
            RI_ASSERT(ri_p303_fold(-12, &folded) == -12 && folded == 0,
                "fold -12");
            RI_ASSERT(ri_p303_fold(24, &folded) == 24 && folded == 0,
                "fold 24");
        }
        for (i = 0; i <= 36u; i++) {
            int semi = (int)i - 12;
            uint8_t ek, eo;
            struct RI303Row r;
            ri_p303_encode(semi, &ek, &eo);
            r.key = ek;
            r.flags = eo;
            RI_ASSERT(ri_p303_semi(&r) == semi, "roundtrip %d", semi);
            RI_ASSERT((eo & (uint8_t)(RI_STEP_UP | RI_STEP_DOWN)) !=
                (uint8_t)(RI_STEP_UP | RI_STEP_DOWN), "both encoded");
        }
        ri_p303_encode(12, &k, &oct);
        RI_ASSERT(k == 12u && oct == 0u, "high C canonical");
    }

    RI_RESULT("pattern_model");
}
