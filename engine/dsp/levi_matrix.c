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
        m->slot[i].dmod = RI_LEVI_DM_DFILT;           /* the v1 default: cutoff */
        m->slot[i].dpar = 0u;
        m->slot[i].pad[0] = m->slot[i].pad[1] = 0u;
    }
    for (i = 0u; i < RI_LEVI_NMACRO; i++) {
        uint32_t r;
        m->mknob[i] = 0u;
        m->mbtn[i] = 0u;
        for (r = 0u; r < RI_LEVI_MACRO_NR; r++) {
            m->mroute[i][r].dmod = RI_LEVI_DM_NONE;
            m->mroute[i][r].dpar = 0u;
            m->mroute[i][r].depth = 0;
            m->mroute[i][r].bval = 0u;
        }
    }
}

/* v1 destination ids onto module + parameter. */
static const uint8_t V1_DMOD[RI_LEVI_MD_N] = { RI_LEVI_DM_DFILT, RI_LEVI_DM_DFILT, RI_LEVI_DM_AFILT, RI_LEVI_DM_VCA,
    RI_LEVI_DM_VCA, RI_LEVI_DM_ALGO };
static const uint8_t V1_DPAR[RI_LEVI_MD_N] = { 0u, 1u, 2u, 2u, 0u, 0u };

static int src_ok(uint32_t src) {
    return src < RI_LEVI_MS_N && (src < 13u || src > 15u);
}

int ri_levi_matrix_set(struct RILeviMatrix *m, uint32_t slot, uint32_t src,
    uint32_t dst, int depth) {
    if (!m || slot >= RI_LEVI_MX_NSLOTS || depth < -100 || depth > 100)
        return 2;
    if (!src_ok(src) || dst >= RI_LEVI_MD_N)
        return 2;
    m->slot[slot].src = (uint8_t)src;
    m->slot[slot].dst = (uint8_t)dst;
    m->slot[slot].dmod = V1_DMOD[dst];
    m->slot[slot].dpar = V1_DPAR[dst];
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
    const float *lfo5, uint32_t note, float *dst_out) {
    uint32_t s, d;
    if (!m || !openv || !lfo5 || !dst_out)
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
        else if (sl->src >= RI_LEVI_MS_LFO0 && sl->src <= RI_LEVI_MS_LFO4)
            src = lfo5[sl->src - RI_LEVI_MS_LFO0];
        else if (sl->src == RI_LEVI_MS_NOTE)
            src = ((float)(note > 127u ? 127u : note) - 60.0f) / 60.0f;
        else
            continue; /* reserved ids stay inert until their slice */
        if (sl->dst < RI_LEVI_MD_N)
            dst_out[sl->dst] += ((float)sl->depth / 100.0f) * src;
    }
    return 0;
}

/* ---- Fidelity P5b: module/param routes, macros, names ---- */
static const uint8_t DM_NPARAM[RI_LEVI_DM_N] = {
    0, 14, 14, 14, 14, 14, 14, 14, 14, 14, 2, 2, 7, 5, 3, 9, 9, 9, 9, 9, 4, 4, 4, 4, 4, 32, 8, 1, 10, 5, 5, 3, 3
};

uint32_t ri_levi_dm_nparam(uint32_t dmod) {
    return dmod < RI_LEVI_DM_N ? DM_NPARAM[dmod] : 0u;
}

int ri_levi_matrix_route(struct RILeviMatrix *m, uint32_t slot, uint32_t src, uint32_t dmod, uint32_t dpar, int depth) {
    uint32_t d;
    if (!m || slot >= RI_LEVI_MX_NSLOTS || depth < -100 || depth > 100 || !src_ok(src) || dmod >= RI_LEVI_DM_N ||
        (dmod != RI_LEVI_DM_NONE && dpar >= DM_NPARAM[dmod]))
        return 2;
    m->slot[slot].src = (uint8_t)src;
    m->slot[slot].dmod = (uint8_t)dmod;
    m->slot[slot].dpar = (uint8_t)dpar;
    m->slot[slot].depth = (int8_t)depth;
    m->slot[slot].on = dmod != RI_LEVI_DM_NONE ? 1u : 0u;
    m->slot[slot].dst = 0xFFu;                        /* no v1 twin unless it matches one */
    for (d = 0u; d < RI_LEVI_MD_N; d++)
        if (V1_DMOD[d] == dmod && V1_DPAR[d] == dpar)
            m->slot[slot].dst = (uint8_t)d;
    return 0;
}

uint32_t ri_levi_matrix_eval2(const struct RILeviMatrix *m, const float *src, struct RILeviModOut *out) {
    float dd[RI_LEVI_MX_NSLOTS], mm[RI_LEVI_NMACRO];
    uint32_t s, i, r, n = 0u;
    if (!m || !src || !out)
        return 0u;
    for (s = 0u; s < RI_LEVI_MX_NSLOTS; s++)
        dd[s] = 0.0f;
    for (i = 0u; i < RI_LEVI_NMACRO; i++)
        mm[i] = 0.0f;
    /* Pass 1: routes that modulate route depths or macro knobs. */
    for (s = 0u; s < RI_LEVI_MX_NSLOTS; s++) {
        const struct RILeviMxSlot *sl = &m->slot[s];
        if (!sl->on || !sl->depth || sl->src >= RI_LEVI_MS_N)
            continue;
        if (sl->dmod == RI_LEVI_DM_MTRX && sl->dpar < RI_LEVI_MX_NSLOTS)
            dd[sl->dpar] += ((float)sl->depth / 100.0f) * src[sl->src];
        else if (sl->dmod == RI_LEVI_DM_MACRO && sl->dpar < RI_LEVI_NMACRO)
            mm[sl->dpar] += ((float)sl->depth / 100.0f) * src[sl->src];
    }
    /* Macros: knob (or button value) plus its modulation, 0..1. */
    for (i = 0u; i < RI_LEVI_NMACRO; i++) {
        float mv = 0.0f;
        int any = 0;
        for (r = 0u; r < RI_LEVI_MACRO_NR; r++)
            any |= m->mroute[i][r].dmod != RI_LEVI_DM_NONE && m->mroute[i][r].depth != 0;
        if (!any)
            continue;
        for (r = 0u; r < RI_LEVI_MACRO_NR; r++) {
            const struct RILeviMacroRoute *mr = &m->mroute[i][r];
            if (mr->dmod == RI_LEVI_DM_NONE || !mr->depth || mr->dmod == RI_LEVI_DM_MACRO)
                continue;
            mv = m->mbtn[i] ? (float)mr->bval / 127.0f : (float)m->mknob[i] / 127.0f + mm[i];
            mv = mv < 0.0f ? 0.0f : mv > 1.0f ? 1.0f : mv;
            if (mr->dmod == RI_LEVI_DM_MTRX) {
                if (mr->dpar < RI_LEVI_MX_NSLOTS)
                    dd[mr->dpar] += ((float)mr->depth / 100.0f) * mv;
                continue;
            }
            out[n].dmod = mr->dmod;
            out[n].dpar = mr->dpar;
            out[n].pad[0] = out[n].pad[1] = 0u;
            out[n].x = ((float)mr->depth / 100.0f) * mv;
            n++;
        }
    }
    /* Pass 2: the routes proper, depth + its modulation (clamped). */
    for (s = 0u; s < RI_LEVI_MX_NSLOTS; s++) {
        const struct RILeviMxSlot *sl = &m->slot[s];
        float d;
        if (!sl->on || sl->src >= RI_LEVI_MS_N || sl->dmod == RI_LEVI_DM_NONE || sl->dmod == RI_LEVI_DM_MTRX ||
            sl->dmod == RI_LEVI_DM_MACRO)
            continue;
        d = (float)sl->depth / 100.0f + dd[s];
        if (d == 0.0f)
            continue;
        d = d < -1.0f ? -1.0f : d > 1.0f ? 1.0f : d;
        out[n].dmod = sl->dmod;
        out[n].dpar = sl->dpar;
        out[n].pad[0] = out[n].pad[1] = 0u;
        out[n].x = d * src[sl->src];
        n++;
    }
    return n;
}

static const char *const MS_NAME[RI_LEVI_MS_N] = {
    "OSC 1 ENV", "OSC 2 ENV", "OSC 3 ENV", "OSC 4 ENV", "OSC 5 ENV", "OSC 6 ENV", "OSC 7 ENV", "OSC 8 ENV",
    "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "", "", "", "KEYTRACK",
    "ENV 1", "ENV 2", "ENV 3", "ENV 4", "ENV 5", "LFO 1+", "LFO 2+", "LFO 3+", "LFO 4+", "LFO 5+",
    "POLYAT", "MONOAT", "VEL ON", "VEL OFF", "VOICE MOD", "VOICE MOD+", "MOD WHEEL", "PITCH BEND",
    "RBN ABS", "RBN ABS+", "RBN REL", "MPE-X", "MPE-Y REL", "MPE-Y ABS", "EXP PEDAL", "SUS PEDAL"
};
static const char *const DM_NAME[RI_LEVI_DM_N] = {
    "---", "OSC 1", "OSC 2", "OSC 3", "OSC 4", "OSC 5", "OSC 6", "OSC 7", "OSC 8", "ALL OSC", "CARRIERS",
    "MODULATORS", "D.FILTER", "A.FILTER", "VCA", "ENV 1", "ENV 2", "ENV 3", "ENV 4", "ENV 5", "LFO 1", "LFO 2",
    "LFO 3", "LFO 4", "LFO 5", "MOD MTRX", "MACRO", "ALGO", "VOICE", "DELAY", "REVERB", "PRE-FX", "POST-FX"
};
static const char *const DP_OSC[RI_LEVI_DO_N] = { "INIT LVL", "ENV LVL", "PITCH", "RATIO", "FINE", "FEEDBACK",
    "PHASE", "PAN", "WAVE", "ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE" };
static const char *const DP_DF[7] = { "CUTOFF", "RESO", "MORPH", "DRIVE", "ENV1 AMT", "LFO1 AMT", "LEVEL" };
static const char *const DP_AF[5] = { "CUTOFF", "RESO", "PRE-DRIVE", "ENV2 AMT", "LFO2 AMT" };
static const char *const DP_VCA[3] = { "LEVEL", "LFO3 AMT", "OSCS LVL" };
static const char *const DP_ENV[9] = { "ATTACK", "HOLD", "DECAY", "SUSTAIN", "RELEASE", "LEVEL", "ATK CRV",
    "DEC CRV", "REL CRV" };
static const char *const DP_LFO[4] = { "RATE", "LEVEL", "SMOOTH", "STEPS" };
static const char *const DP_DEPTH[32] = { "DEPTH 1", "DEPTH 2", "DEPTH 3", "DEPTH 4", "DEPTH 5", "DEPTH 6",
    "DEPTH 7", "DEPTH 8", "DEPTH 9", "DEPTH 10", "DEPTH 11", "DEPTH 12", "DEPTH 13", "DEPTH 14", "DEPTH 15",
    "DEPTH 16", "DEPTH 17", "DEPTH 18", "DEPTH 19", "DEPTH 20", "DEPTH 21", "DEPTH 22", "DEPTH 23", "DEPTH 24",
    "DEPTH 25", "DEPTH 26", "DEPTH 27", "DEPTH 28", "DEPTH 29", "DEPTH 30", "DEPTH 31", "DEPTH 32" };
static const char *const DP_MACRO[8] = { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4", "MACRO 5", "MACRO 6",
    "MACRO 7", "MACRO 8" };
static const char *const DP_VOICE[RI_LEVI_DVO_N] = { "DETUNE", "PAN", "ANALOG FL", "BEND RNG", "VIB AMT",
    "VIB RATE", "GLIDE TGL", "GLIDE TIME", "GLIDE CRV", "PAN WIDTH" };
static const char *const DP_DLY[RI_LEVI_DD_N] = { "TIME", "FEEDBACK", "WET TONE", "FB TONE", "DRY/WET" };
static const char *const DP_REV[RI_LEVI_DR_N] = { "TIME", "TONE", "HI DAMP", "LO DAMP", "DRY/WET" };
static const char *const DP_MDX[RI_LEVI_DX_N] = { "PARAM 1", "PARAM 2", "DRY/WET" };

static const uint8_t MS_UI[RI_LEVI_MS_UI_N] = {
    RI_LEVI_MS_N, 17, 18, 19, 20, 21, 8, 9, 10, 11, 12, 22, 23, 24, 25, 26, 0, 1, 2, 3, 4, 5, 6, 7,
    RI_LEVI_MS_NOTE, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42
};

uint32_t ri_levi_ms_by_ui(uint32_t ui) {
    return ui < RI_LEVI_MS_UI_N ? MS_UI[ui] : RI_LEVI_MS_N;
}

uint32_t ri_levi_ms_to_ui(uint32_t src) {
    uint32_t i;
    for (i = 1u; i < RI_LEVI_MS_UI_N; i++)
        if (MS_UI[i] == src)
            return i;
    return 0u;
}

const char *ri_levi_ms_name(uint32_t src) {
    return src < RI_LEVI_MS_N ? MS_NAME[src] : "";
}

const char *ri_levi_dm_name(uint32_t dmod) {
    return dmod < RI_LEVI_DM_N ? DM_NAME[dmod] : "";
}

const char *ri_levi_dp_name(uint32_t dmod, uint32_t dpar) {
    if (dmod >= RI_LEVI_DM_N || dpar >= DM_NPARAM[dmod])
        return "";
    if (dmod >= RI_LEVI_DM_OSC1 && dmod <= RI_LEVI_DM_MODS)
        return DP_OSC[dpar];
    if (dmod == RI_LEVI_DM_DFILT)
        return DP_DF[dpar];
    if (dmod == RI_LEVI_DM_AFILT)
        return DP_AF[dpar];
    if (dmod == RI_LEVI_DM_VCA)
        return DP_VCA[dpar];
    if (dmod >= RI_LEVI_DM_ENV1 && dmod < RI_LEVI_DM_ENV1 + 5u)
        return DP_ENV[dpar];
    if (dmod >= RI_LEVI_DM_LFO1 && dmod < RI_LEVI_DM_LFO1 + 5u)
        return DP_LFO[dpar];
    if (dmod == RI_LEVI_DM_MTRX)
        return DP_DEPTH[dpar];
    if (dmod == RI_LEVI_DM_MACRO)
        return DP_MACRO[dpar];
    if (dmod == RI_LEVI_DM_VOICE)
        return DP_VOICE[dpar];
    if (dmod == RI_LEVI_DM_DELAY)
        return DP_DLY[dpar];
    if (dmod == RI_LEVI_DM_REVERB)
        return DP_REV[dpar];
    if (dmod == RI_LEVI_DM_PREFX || dmod == RI_LEVI_DM_POSTFX)
        return DP_MDX[dpar];
    return "MORPH";
}
