/* levi_matrix.c — Levi mod-matrix slot core bodies (v2 feature 4a).
 * Own model; evaluation is pure arithmetic over caller-provided
 * source values (no DSP state touched here — the render hook in 4b
 * feeds openv/note and applies the offsets). */
#include "engine/dsp/levi_matrix.h"

void ri_levi_matrix_init(struct RILeviMatrix *m) {
    uint32_t i;
    if (!m)
        return;
    for (i = 0u; i < RI_LEVI_MX_NSLOTS; i++) {
        m->slot[i].src = RI_LEVI_MS_OPENV0;
        m->slot[i].dst = RI_LEVI_MD_CUTOFF;
        m->slot[i].depth = 0;
        m->slot[i].on = 0u;
    }
}

int ri_levi_matrix_set(struct RILeviMatrix *m, uint32_t slot, uint32_t src,
    uint32_t dst, int depth) {
    if (!m || slot >= RI_LEVI_MX_NSLOTS || depth < -100 || depth > 100)
        return 2;
    if ((src > RI_LEVI_MS_OPENV7 && src != RI_LEVI_MS_NOTE) || dst >= RI_LEVI_MD_N)
        return 2;
    m->slot[slot].src = (uint8_t)src;
    m->slot[slot].dst = (uint8_t)dst;
    m->slot[slot].depth = (int8_t)depth;
    m->slot[slot].on = 1u;
    return 0;
}

int ri_levi_matrix_enable(struct RILeviMatrix *m, uint32_t slot, uint32_t on) {
    if (!m || slot >= RI_LEVI_MX_NSLOTS)
        return 2;
    m->slot[slot].on = on != 0u ? 1u : 0u;
    return 0;
}

int ri_levi_matrix_eval(const struct RILeviMatrix *m, const float *openv,
    uint32_t note, float *dst_out) {
    uint32_t s, d;
    if (!m || !openv || !dst_out)
        return 2;
    for (d = 0u; d < RI_LEVI_MD_N; d++)
        dst_out[d] = 0.0f;
    for (s = 0u; s < RI_LEVI_MX_NSLOTS; s++) {
        const struct RILeviMxSlot *sl = &m->slot[s];
        float src;
        if (!sl->on || sl->depth == 0)
            continue;
        if (sl->src <= RI_LEVI_MS_OPENV7)
            src = openv[sl->src];
        else if (sl->src == RI_LEVI_MS_NOTE)
            src = ((float)(note > 127u ? 127u : note) - 60.0f) / 60.0f;
        else
            continue; /* reserved ids stay inert until their slice */
        dst_out[sl->dst] += ((float)sl->depth / 100.0f) * src;
    }
    return 0;
}
