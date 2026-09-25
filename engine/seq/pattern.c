/* pattern.c — per-instance pattern model (spec 2026-09-25 §1).
 * Pure functions; no allocation, no IO, no RNG state, no time.
 */
#include <string.h>
#include "engine/seq/pattern.h"

const uint8_t RI_LANE_TO_RB808_SLOT[RI_DRUM_CLASSIC_LANES] = {
    0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u
};

const uint8_t RI_LANE_TO_RB909_VOICE[RI_DRUM_CLASSIC_LANES] = {
    RB909_BD, RB909_SD, RB909_LT, RB909_MT, RB909_HT, RB909_RS, RB909_CP,
    RB909_CH, RB909_OH, RB909_CR, RB909_RD
};

void ri_pattern_init(struct RIPattern *p, uint8_t kind, uint8_t drum_class) {
    uint32_t i;
    if (!p || kind > RI_PATTERN_KIND_DRUM)
        return;
    /* Zero first: the union's inactive overlay has no meaning, and
     * cleared-state memcmp (sparse BANK writing, round-trip tests)
     * needs deterministic bytes. */
    memset(p, 0, sizeof *p);
    p->kind = kind;
    p->length = RI_PATTERN_STEPS;
    p->payload_ver = RI_PATTERN_PAYLOAD_VERSION;
    p->drum_class = (kind == RI_PATTERN_KIND_DRUM) ? drum_class : 0u;
    for (i = 0; i < RI_PATTERN_STEPS; i++) {
        if (kind == RI_PATTERN_KIND_303) {
            p->row.r303[i].key = 0u;
            p->row.r303[i].flags = RI_STEP_REST;
        } else {
            p->row.drum[i].on = 0u;
            p->row.drum[i].high = 0u;
            p->row.drum[i].flam = 0u;
            p->row.drum[i].flags = 0u;
            p->row.drum[i].pad = 0u;
        }
    }
}

void ri_bank_init(struct RIPatternBank *b, uint8_t instance, uint8_t kind,
    uint8_t drum_class) {
    uint32_t i;
    if (!b || kind > RI_PATTERN_KIND_DRUM)
        return;
    b->instance = instance;
    b->kind = kind;
    b->drum_class = (kind == RI_PATTERN_KIND_DRUM) ? drum_class : 0u;
    b->pad = 0u;
    for (i = 0; i < RI_PATTERN_BANK_PATTERNS; i++)
        ri_pattern_init(&b->pat[i], kind,
            (kind == RI_PATTERN_KIND_DRUM) ? drum_class : 0u);
}

int ri_pattern_set_length(struct RIPattern *p, uint32_t len) {
    if (!p)
        return 2;
    if (len < 1u)
        len = 1u;
    if (len > RI_PATTERN_STEPS)
        len = RI_PATTERN_STEPS;
    p->length = (uint8_t)len;
    return 0;
}

#define RI_STEP_KNOWN_MASK 0x3Fu /* REST|ACCENT|SLIDE|UP|DOWN */

int ri_pattern_valid(const struct RIPattern *p) {
    uint32_t i;
    if (!p)
        return 2;
    if (p->kind > RI_PATTERN_KIND_DRUM)
        return 2;
    if (p->length < 1u || p->length > RI_PATTERN_STEPS)
        return 2;
    if (p->payload_ver != RI_PATTERN_PAYLOAD_VERSION)
        return 2;
    if (p->kind == RI_PATTERN_KIND_303) {
        for (i = 0; i < RI_PATTERN_STEPS; i++) {
            if (p->row.r303[i].key > 12u)
                return 2;
            if (p->row.r303[i].flags & (uint8_t)~RI_STEP_KNOWN_MASK)
                return 2;
        }
        return 0;
    }
    for (i = 0; i < RI_PATTERN_STEPS; i++) {
        uint16_t on = p->row.drum[i].on;
        uint16_t hi = p->row.drum[i].high;
        uint16_t fl = p->row.drum[i].flam;
        if ((on | hi | fl) & (uint16_t)~RI_DRUM_LANE_MASK_CLASSIC)
            return 2;
        if (hi & (uint16_t)~on)
            return 2;
        if (fl & (uint16_t)~on)
            return 2;
        if (hi & fl)
            return 2;
        if (p->drum_class == RI_DRUM_CLASS_808 && (hi | fl))
            return 2;
        if (p->row.drum[i].flags & (uint8_t)~RI_DRUM_AC)
            return 2;
    }
    return 0;
}

int ri_p303_set(struct RIPattern *p, uint32_t step, uint8_t key,
    uint8_t flags) {
    if (!p || p->kind != RI_PATTERN_KIND_303)
        return 2;
    if (step >= RI_PATTERN_STEPS || key > 12u)
        return 2;
    p->row.r303[step].key = key;
    p->row.r303[step].flags = flags;
    return 0;
}

int ri_pdrum_set(struct RIPattern *p, uint32_t step, uint32_t lane,
    uint32_t state) {
    uint16_t bit;
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return 2;
    if (step >= RI_PATTERN_STEPS || lane >= RI_DRUM_LANES)
        return 2;
    if (state > RI_HIT_FLAM)
        return 2;
    if (p->drum_class == RI_DRUM_CLASS_808 &&
        (state == RI_HIT_HIGH || state == RI_HIT_FLAM))
        return 2;
    bit = (uint16_t)(1u << lane);
    p->row.drum[step].on &= (uint16_t)~bit;
    p->row.drum[step].high &= (uint16_t)~bit;
    p->row.drum[step].flam &= (uint16_t)~bit;
    if (state == RI_HIT_LOW || state == RI_HIT_HIGH || state == RI_HIT_FLAM)
        p->row.drum[step].on |= bit;
    if (state == RI_HIT_HIGH)
        p->row.drum[step].high |= bit;
    if (state == RI_HIT_FLAM)
        p->row.drum[step].flam |= bit;
    return 0;
}

uint32_t ri_pdrum_get(const struct RIPattern *p, uint32_t step,
    uint32_t lane) {
    uint16_t bit;
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return RI_HIT_OFF;
    if (step >= RI_PATTERN_STEPS || lane >= RI_DRUM_LANES)
        return RI_HIT_OFF;
    bit = (uint16_t)(1u << lane);
    if (!(p->row.drum[step].on & bit))
        return RI_HIT_OFF;
    if (p->row.drum[step].high & bit)
        return RI_HIT_HIGH;
    if (p->row.drum[step].flam & bit)
        return RI_HIT_FLAM;
    return RI_HIT_LOW;
}

int ri_pdrum_set_ac(struct RIPattern *p, uint32_t step, int on) {
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return 2;
    if (step >= RI_PATTERN_STEPS)
        return 2;
    if (on)
        p->row.drum[step].flags |= RI_DRUM_AC;
    else
        p->row.drum[step].flags &= (uint8_t)~RI_DRUM_AC;
    return 0;
}

uint32_t ri_pdrum_click(struct RIPattern *p, uint32_t step, uint32_t lane,
    int flam_mode) {
    uint32_t cur;
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return RI_HIT_OFF;
    if (step >= RI_PATTERN_STEPS || lane >= RI_DRUM_LANES)
        return RI_HIT_OFF;
    cur = ri_pdrum_get(p, step, lane);
    if (p->drum_class == RI_DRUM_CLASS_808) {
        /* OFF<->LOW regardless of flam mode. */
        ri_pdrum_set(p, step, lane,
            cur == RI_HIT_OFF ? RI_HIT_LOW : RI_HIT_OFF);
        return cur == RI_HIT_OFF ? RI_HIT_LOW : RI_HIT_OFF;
    }
    if (flam_mode) {
        /* OFF->FLAM, any other->OFF (p. 30-31). */
        ri_pdrum_set(p, step, lane,
            cur == RI_HIT_OFF ? RI_HIT_FLAM : RI_HIT_OFF);
        return cur == RI_HIT_OFF ? RI_HIT_FLAM : RI_HIT_OFF;
    }
    if (cur == RI_HIT_OFF)
        cur = RI_HIT_LOW;
    else if (cur == RI_HIT_LOW)
        cur = RI_HIT_HIGH;
    else
        cur = RI_HIT_OFF; /* HIGH or FLAM -> OFF */
    ri_pdrum_set(p, step, lane, cur);
    return cur;
}

/* Task 3: key <-> note. */

int ri_p303_octave(uint8_t flags) {
    int up = (flags & RI_STEP_UP) != 0;
    int dn = (flags & RI_STEP_DOWN) != 0;
    if (up && !dn)
        return 1;
    if (dn && !up)
        return -1;
    return 0;
}

int ri_p303_semi(const struct RI303Row *r) {
    if (!r)
        return 0;
    return (int)r->key + 12 * ri_p303_octave(r->flags);
}

uint8_t ri_p303_note(const struct RI303Row *r) {
    int n;
    if (!r)
        return RI_303_BASE_NOTE;
    n = (int)RI_303_BASE_NOTE + ri_p303_semi(r);
    if (n < 0)
        n = 0;
    if (n > 127)
        n = 127;
    return (uint8_t)n;
}

int ri_p303_fold(int semi, int *folded) {
    int f = 0;
    while (semi < RI_303_SEMI_MIN) {
        semi += 12;
        f = 1;
    }
    while (semi > RI_303_SEMI_MAX) {
        semi -= 12;
        f = 1;
    }
    if (folded)
        *folded = f;
    return semi;
}

void ri_p303_encode(int semi, uint8_t *key, uint8_t *octflags) {
    uint8_t k = 0u, o = 0u;
    if (semi < 0) {
        o = RI_STEP_DOWN;
        k = (uint8_t)(semi + 12);
    } else if (semi <= 12) {
        k = (uint8_t)semi;
    } else {
        o = RI_STEP_UP;
        k = (uint8_t)(semi - 12);
    }
    if (key)
        *key = k;
    if (octflags)
        *octflags = o;
}

/* Task 4: ReBirth Edit menu (p. 51-54). */

int ri_pattern_clear(struct RIPattern *p) {
    uint8_t len;
    if (!p)
        return 2;
    if (p->kind > RI_PATTERN_KIND_DRUM)
        return 2;
    len = p->length;
    ri_pattern_init(p, p->kind,
        p->kind == RI_PATTERN_KIND_DRUM ? p->drum_class : 0u);
    p->length = len; /* length unchanged (p. 52) */
    return 0;
}

int ri_bank_copy(const struct RIPatternBank *src, uint32_t sslot,
    struct RIPatternBank *dst, uint32_t dslot) {
    uint32_t i;
    if (!src || !dst)
        return 2;
    if (sslot >= RI_PATTERN_BANK_PATTERNS ||
        dslot >= RI_PATTERN_BANK_PATTERNS)
        return 2;
    if (src->kind != dst->kind || src->drum_class != dst->drum_class)
        return 2;
    for (i = 0; i < sizeof(struct RIPattern); i++)
        ((unsigned char *)&dst->pat[dslot])[i] =
            ((const unsigned char *)&src->pat[sslot])[i];
    return 0;
}

int ri_bank_cut(struct RIPatternBank *b, uint32_t slot,
    struct RIPattern *clip) {
    uint32_t i;
    if (!b || !clip || slot >= RI_PATTERN_BANK_PATTERNS)
        return 2;
    for (i = 0; i < sizeof(struct RIPattern); i++)
        ((unsigned char *)clip)[i] =
            ((const unsigned char *)&b->pat[slot])[i];
    ri_pattern_clear(&b->pat[slot]); /* clear, not remove (p. 51) */
    return 0;
}

int ri_bank_paste(struct RIPatternBank *b, uint32_t slot,
    const struct RIPattern *clip) {
    uint32_t i;
    if (!b || !clip || slot >= RI_PATTERN_BANK_PATTERNS)
        return 2;
    if (clip->kind != b->kind || clip->drum_class != b->drum_class)
        return 2;
    for (i = 0; i < sizeof(struct RIPattern); i++)
        ((unsigned char *)&b->pat[slot])[i] =
            ((const unsigned char *)clip)[i];
    return 0;
}

int ri_pattern_shift(struct RIPattern *p, int dir) {
    uint32_t i;
    if (!p)
        return 2;
    if (p->kind > RI_PATTERN_KIND_DRUM)
        return 2;
    if (dir != -1 && dir != 1)
        return 2;
    if (p->kind == RI_PATTERN_KIND_303) {
        struct RI303Row tmp;
        if (dir == 1) {
            tmp = p->row.r303[RI_PATTERN_STEPS - 1u];
            for (i = RI_PATTERN_STEPS - 1u; i > 0u; i--)
                p->row.r303[i] = p->row.r303[i - 1u];
            p->row.r303[0] = tmp;
        } else {
            tmp = p->row.r303[0];
            for (i = 0; i < RI_PATTERN_STEPS - 1u; i++)
                p->row.r303[i] = p->row.r303[i + 1u];
            p->row.r303[RI_PATTERN_STEPS - 1u] = tmp;
        }
    } else {
        struct RIDrumRow tmp;
        if (dir == 1) {
            tmp = p->row.drum[RI_PATTERN_STEPS - 1u];
            for (i = RI_PATTERN_STEPS - 1u; i > 0u; i--)
                p->row.drum[i] = p->row.drum[i - 1u];
            p->row.drum[0] = tmp;
        } else {
            tmp = p->row.drum[0];
            for (i = 0; i < RI_PATTERN_STEPS - 1u; i++)
                p->row.drum[i] = p->row.drum[i + 1u];
            p->row.drum[RI_PATTERN_STEPS - 1u] = tmp;
        }
    }
    return 0;
}

int ri_pdrum_shift_lane(struct RIPattern *p, uint32_t lane, int dir) {
    uint32_t i;
    uint16_t bit;
    uint8_t h0, f0;
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return 2;
    if (lane >= RI_DRUM_CLASSIC_LANES)
        return 2;
    if (dir != -1 && dir != 1)
        return 2;
    bit = (uint16_t)(1u << lane);
    if (dir == 1) {
        /* Save row-15 lane state, shift down, restore at row 0. */
        h0 = (p->row.drum[RI_PATTERN_STEPS - 1u].high & bit) != 0u;
        f0 = (p->row.drum[RI_PATTERN_STEPS - 1u].flam & bit) != 0u;
        for (i = RI_PATTERN_STEPS - 1u; i > 0u; i--) {
            uint8_t h = (p->row.drum[i - 1u].high & bit) != 0u;
            uint8_t f = (p->row.drum[i - 1u].flam & bit) != 0u;
            uint8_t o = (p->row.drum[i - 1u].on & bit) != 0u;
            p->row.drum[i].on =
                (uint16_t)((p->row.drum[i].on & (uint16_t)~bit) |
                (o ? bit : 0u));
            p->row.drum[i].high =
                (uint16_t)((p->row.drum[i].high & (uint16_t)~bit) |
                (h ? bit : 0u));
            p->row.drum[i].flam =
                (uint16_t)((p->row.drum[i].flam & (uint16_t)~bit) |
                (f ? bit : 0u));
        }
        {
            uint8_t o =
                (p->row.drum[RI_PATTERN_STEPS - 1u].on & bit) != 0u;
            p->row.drum[0].on =
                (uint16_t)((p->row.drum[0].on & (uint16_t)~bit) |
                (o ? bit : 0u));
            p->row.drum[0].high =
                (uint16_t)((p->row.drum[0].high & (uint16_t)~bit) |
                (h0 ? bit : 0u));
            p->row.drum[0].flam =
                (uint16_t)((p->row.drum[0].flam & (uint16_t)~bit) |
                (f0 ? bit : 0u));
        }
    } else {
        uint8_t h15, f15, o15;
        h15 = (p->row.drum[0].high & bit) != 0u;
        f15 = (p->row.drum[0].flam & bit) != 0u;
        o15 = (p->row.drum[0].on & bit) != 0u;
        for (i = 0; i < RI_PATTERN_STEPS - 1u; i++) {
            uint8_t h = (p->row.drum[i + 1u].high & bit) != 0u;
            uint8_t f = (p->row.drum[i + 1u].flam & bit) != 0u;
            uint8_t o = (p->row.drum[i + 1u].on & bit) != 0u;
            p->row.drum[i].on =
                (uint16_t)((p->row.drum[i].on & (uint16_t)~bit) |
                (o ? bit : 0u));
            p->row.drum[i].high =
                (uint16_t)((p->row.drum[i].high & (uint16_t)~bit) |
                (h ? bit : 0u));
            p->row.drum[i].flam =
                (uint16_t)((p->row.drum[i].flam & (uint16_t)~bit) |
                (f ? bit : 0u));
        }
        p->row.drum[RI_PATTERN_STEPS - 1u].on =
            (uint16_t)((p->row.drum[RI_PATTERN_STEPS - 1u].on &
                (uint16_t)~bit) | (o15 ? bit : 0u));
        p->row.drum[RI_PATTERN_STEPS - 1u].high =
            (uint16_t)((p->row.drum[RI_PATTERN_STEPS - 1u].high &
                (uint16_t)~bit) | (h15 ? bit : 0u));
        p->row.drum[RI_PATTERN_STEPS - 1u].flam =
            (uint16_t)((p->row.drum[RI_PATTERN_STEPS - 1u].flam &
                (uint16_t)~bit) | (f15 ? bit : 0u));
    }
    return 0;
}

int ri_p303_transpose(struct RIPattern *p, int semis, uint32_t *nfolded) {
    uint32_t i, nf = 0u;
    if (!p || p->kind != RI_PATTERN_KIND_303)
        return 2;
    if (semis > 12 || semis < -12)
        return 2;
    for (i = 0; i < RI_PATTERN_STEPS; i++) {
        int s, folded = 0;
        uint8_t k, o;
        if (p->row.r303[i].flags & RI_STEP_REST)
            continue; /* silent rows keep their key (p. 54: "notes") */
        s = ri_p303_fold(ri_p303_semi(&p->row.r303[i]) + semis, &folded);
        nf += (uint32_t)folded;
        ri_p303_encode(s, &k, &o);
        p->row.r303[i].key = k;
        p->row.r303[i].flags = (uint8_t)((p->row.r303[i].flags &
            (uint8_t)~(RI_STEP_UP | RI_STEP_DOWN)) | o);
    }
    if (nfolded)
        *nfolded = nf;
    return 0;
}

/* Numerical Recipes LCG; top 16 bits used. Never global. */
static uint32_t ri_lcg(uint32_t *s) {
    *s = *s * 1664525u + 1013904223u;
    return *s >> 16;
}

static uint32_t ri_pick(uint32_t *s, uint32_t n) {
    return ri_lcg(s) % n; /* n <= 65536 */
}

int ri_p303_random(struct RIPattern *p, uint32_t what, uint32_t seed) {
    uint32_t i;
    uint32_t s = seed;
    if (!p || p->kind != RI_PATTERN_KIND_303)
        return 2;
    if (what != RI_RND_PATTERN && what != RI_RND_PITCHES &&
        what != RI_RND_ACCENTS)
        return 2;
    if (what == RI_RND_PATTERN || what == RI_RND_PITCHES) {
        for (i = 0; i < RI_PATTERN_STEPS; i++)
            p->row.r303[i].key = (uint8_t)ri_pick(&s, 13u);
    }
    if (what == RI_RND_PATTERN || what == RI_RND_ACCENTS) {
        for (i = 0; i < RI_PATTERN_STEPS; i++) {
            uint8_t f = 0u, r;
            if (ri_pick(&s, 16u) < 4u)
                f |= RI_STEP_REST;
            if (ri_pick(&s, 16u) < 4u)
                f |= RI_STEP_ACCENT;
            if (ri_pick(&s, 16u) < 3u)
                f |= RI_STEP_SLIDE;
            r = (uint8_t)ri_pick(&s, 16u);
            if (r < 2u)
                f |= RI_STEP_DOWN;
            else if (r < 4u)
                f |= RI_STEP_UP;
            p->row.r303[i].flags = f;
        }
    }
    return 0;
}

int ri_pdrum_random_lane(struct RIPattern *p, uint32_t lane, uint32_t seed) {
    uint32_t i;
    uint32_t s = seed;
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return 2;
    if (lane >= RI_DRUM_CLASSIC_LANES)
        return 2;
    for (i = 0; i < RI_PATTERN_STEPS; i++) {
        uint32_t r = ri_pick(&s, 16u);
        uint32_t st;
        if (p->drum_class == RI_DRUM_CLASS_808) {
            st = r < 11u ? RI_HIT_OFF : RI_HIT_LOW;
        } else {
            if (r < 8u)
                st = RI_HIT_OFF;
            else if (r < 12u)
                st = RI_HIT_LOW;
            else if (r < 15u)
                st = RI_HIT_HIGH;
            else
                st = RI_HIT_FLAM;
        }
        ri_pdrum_set(p, i, lane, st);
    }
    return 0;
}

int ri_p303_alter(struct RIPattern *p, uint32_t what, uint32_t seed) {
    uint32_t i, s = seed;
    if (!p || p->kind != RI_PATTERN_KIND_303)
        return 2;
    if (what != RI_RND_PATTERN && what != RI_RND_PITCHES &&
        what != RI_RND_ACCENTS)
        return 2;
    /* Seeded Fisher-Yates over the 16 rows/columns. */
    for (i = RI_PATTERN_STEPS - 1u; i > 0u; i--) {
        uint32_t j = ri_pick(&s, i + 1u);
        if (what == RI_RND_PATTERN) {
            struct RI303Row t = p->row.r303[i];
            p->row.r303[i] = p->row.r303[j];
            p->row.r303[j] = t;
        } else if (what == RI_RND_PITCHES) {
            uint8_t t = p->row.r303[i].key;
            p->row.r303[i].key = p->row.r303[j].key;
            p->row.r303[j].key = t;
        } else {
            uint8_t t = p->row.r303[i].flags;
            p->row.r303[i].flags = p->row.r303[j].flags;
            p->row.r303[j].flags = t;
        }
    }
    return 0;
}

int ri_pdrum_alter_lane(struct RIPattern *p, uint32_t lane, uint32_t seed) {
    uint32_t i, s = seed;
    uint32_t st[RI_PATTERN_STEPS];
    if (!p || p->kind != RI_PATTERN_KIND_DRUM)
        return 2;
    if (lane >= RI_DRUM_CLASSIC_LANES)
        return 2;
    for (i = 0; i < RI_PATTERN_STEPS; i++)
        st[i] = ri_pdrum_get(p, i, lane);
    for (i = RI_PATTERN_STEPS - 1u; i > 0u; i--) {
        uint32_t j = ri_pick(&s, i + 1u);
        uint32_t t = st[i];
        st[i] = st[j];
        st[j] = t;
    }
    for (i = 0; i < RI_PATTERN_STEPS; i++)
        ri_pdrum_set(p, i, lane, st[i]);
    return 0;
}
