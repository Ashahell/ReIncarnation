/* reverb.c — algorithmic reverb core bodies (v2 feature 5a).
 * Own network; kernels ri_* only (float plumbing is plain C).
 * No allocation, no IO. Decay 0 short-circuits to exact 0.0f
 * (bypass law — combs with zero feedback would still pass
 * delayed copies, so the gate sits at the output).
 */
#include "engine/fx/reverb.h"

static const uint32_t RI_REV_BASE[6] = {
    RI_REV_C0, RI_REV_C1, RI_REV_C2, RI_REV_C3, RI_REV_A0, RI_REV_A1
};

int ri_reverb_init(struct RIReverb *r, uint32_t sr) {
    uint32_t i;
    if (!r || sr == 0u)
        return 2;
    for (i = 0u; i < 4u; i++)
        r->lc[i] = 0;
    for (i = 0u; i < 2u; i++)
        r->la[i] = 0;
    r->cap = 0u;
    for (i = 0u; i < 6u; i++) {
        uint64_t len = (uint64_t)RI_REV_BASE[i] * (uint64_t)sr / 48000u;
        r->len[i] = len < 1u ? 1u : (uint32_t)len;
        r->pos[i] = 0u;
    }
    r->fb = 0.0f;
    r->sr = sr;
    return 0;
}

int ri_reverb_lines(struct RIReverb *r, float *c0, float *c1, float *c2,
    float *c3, float *a0, float *a1, uint32_t cap) {
    uint32_t i, need = 0u;
    float *all[6];
    if (!r || r->sr == 0u || !c0 || !c1 || !c2 || !c3 || !a0 || !a1)
        return 2;
    for (i = 0u; i < 6u; i++)
        if (r->len[i] + 1u > need)
            need = r->len[i] + 1u;
    if (cap < need || cap < RI_REV_MIN_CAP)
        return 2;
    all[0] = c0;
    all[1] = c1;
    all[2] = c2;
    all[3] = c3;
    all[4] = a0;
    all[5] = a1;
    for (i = 0u; i < 6u; i++) {
        uint32_t k;
        if (i < 4u)
            r->lc[i] = all[i];
        else
            r->la[i - 4u] = all[i];
        for (k = 0u; k < cap; k++)
            all[i][k] = 0.0f;
        r->pos[i] = 0u;
    }
    r->cap = cap;
    return 0;
}

int ri_reverb_set_decay(struct RIReverb *r, uint8_t decay) {
    if (!r)
        return 2;
    r->fb = (float)decay / 127.0f * 0.85f;
    return 0;
}

static float comb_step(float *line, uint32_t cap, uint32_t len,
    uint32_t *pos, float in, float fb) {
    uint32_t w = *pos % cap;
    uint32_t rd = (w + cap - (len % cap)) % cap;
    float d = line[rd];
    line[w] = in + d * fb;
    *pos = w + 1u;
    return d;
}

static float ap_step(float *line, uint32_t cap, uint32_t len,
    uint32_t *pos, float in) {
    uint32_t w = *pos % cap;
    uint32_t rd = (w + cap - (len % cap)) % cap;
    float d = line[rd];
    float y = -0.5f * in + d;
    line[w] = in + 0.5f * y;
    *pos = w + 1u;
    return y;
}

float ri_reverb_render(struct RIReverb *r, float in) {
    float acc, y;
    uint32_t i;
    if (!r || !r->lc[0] || r->cap == 0u)
        return 0.0f;
    if (r->fb <= 0.0f)
        return 0.0f; /* bypass law: exact silence */
    acc = 0.0f;
    for (i = 0u; i < 4u; i++)
        acc += comb_step(r->lc[i], r->cap, r->len[i], &r->pos[i], in, r->fb);
    acc *= 0.25f;
    y = ap_step(r->la[0], r->cap, r->len[4], &r->pos[4], acc);
    y = ap_step(r->la[1], r->cap, r->len[5], &r->pos[5], y);
    return y;
}

void ri_reverb_reset(struct RIReverb *r) {
    uint32_t i, k;
    if (!r || r->cap == 0u)
        return;
    for (i = 0u; i < 4u; i++) {
        if (!r->lc[i])
            continue;
        for (k = 0u; k < r->cap; k++)
            r->lc[i][k] = 0.0f;
        r->pos[i] = 0u;
    }
    for (i = 0u; i < 2u; i++) {
        if (!r->la[i])
            continue;
        for (k = 0u; k < r->cap; k++)
            r->la[i][k] = 0.0f;
        r->pos[i + 4u] = 0u;
    }
}
