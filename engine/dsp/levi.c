/* levi.c — Levi FM voice bank bodies (owner 2026-09-28, v1 slice 3a).
 * 2-op FM/PM + per-operator DAHDSR + resonant lowpass. Render-contract
 * safe: bounded, no allocation, no IO; denormal-safe (ftz states, short
 * envelopes end exact).
 */
#include "engine/dsp/levi.h"
#include "engine/dsp/kernels.h"

/* E0 DAHDSR shape: fast pluck (delays/holds 0, attack 5 ms, decay
 * 300 ms to sustain 0.8, release 150 ms). Panel owns these later. */
#define LEVI_T_D 0.0f
#define LEVI_T_A 0.005f
#define LEVI_T_H 0.0f
#define LEVI_T_D2 0.300f
#define LEVI_T_R 0.150f
#define LEVI_SUS 0.8f
#define LEVI_REST_LEVEL 0.0001f /* -80 dBFS: inaudible, rests exact */

static float ftz(float x) {
    return (x > -1e-18f && x < 1e-18f) ? 0.0f : x;
}

static float note_hz(uint8_t note) {
    return 440.0f * ri_pow2(((float)note - 69.0f) / 12.0f);
}

static void env_reset(struct RILeviEnv *e) {
    e->times[0] = LEVI_T_D;
    e->times[1] = LEVI_T_A;
    e->times[2] = LEVI_T_H;
    e->times[3] = LEVI_T_D2;
    e->times[4] = 0.0f;
    e->times[5] = LEVI_T_R;
    e->sustain = LEVI_SUS;
    e->value = 0.0f;
    e->stage = RI_LEVI_SEG_D;
    e->stage_t = 0.0f;
}

/* One envelope sample; returns 1 while sounding, 0 at rest end. */
static int env_tick(struct RILeviEnv *e, float sr) {
    float dt, span, from, to;
    if (!e || sr <= 0.0f)
        return 0;
    if (e->stage == RI_LEVI_SEG_IDLE)
        return 0;
    if (e->stage == RI_LEVI_SEG_S)
        return 1;
    dt = 1.0f / sr;
    e->stage_t += dt;
    span = e->times[e->stage];
    from = e->value;
    switch (e->stage) {
    case RI_LEVI_SEG_D:
        to = 0.0f;
        break;
    case RI_LEVI_SEG_A:
        to = 1.0f;
        break;
    case RI_LEVI_SEG_H:
        to = 1.0f;
        break;
    case RI_LEVI_SEG_D2:
        to = e->sustain;
        break;
    default: /* RI_LEVI_SEG_R */
        to = 0.0f;
        break;
    }
    if (span <= 0.0f || e->stage_t >= span) {
        e->value = to;
        e->stage_t = 0.0f;
        if (e->stage == RI_LEVI_SEG_R) {
            e->stage = RI_LEVI_SEG_IDLE;
            e->value = 0.0f;
            return 0;
        }
        e->stage++;
        if (e->stage == RI_LEVI_SEG_S)
            e->value = e->sustain;
        return 1;
    }
    e->value = from + (to - from) * (e->stage_t / span);
    return 1;
}

static void op_reset(struct RILeviOp *o, float freq, float level) {
    o->phase = 0.0f;
    o->freq = freq;
    o->level = level;
    env_reset(&o->env);
}

/* Own preset topologies (functional shapes; mod_src per op, -1 = carrier).
 * 0 DUO: v1 pair; 1 ALLPAR: 8 carriers; 2 STACK8: one chain into op0;
 * 3 STACK44: chains into op0/op4; 4 STACK422: chain + two pairs;
 * 5 PAIRS4: four pairs; 6 STACK332: 3+3+2; 7 STACK62: 6-chain + pair. */
static const int8_t RI_LEVI_PRESET_SRC[RI_LEVI_ALGO_N][RI_LEVI_NOPS] = {
    { -1, 0, -1, -1, -1, -1, -1, -1 },
    { -1, -1, -1, -1, -1, -1, -1, -1 },
    { -1, 0, 1, 2, 3, 4, 5, 6 },
    { -1, 0, 1, 2, -1, 4, 5, 6 },
    { -1, 0, 1, 2, -1, 4, -1, 6 },
    { -1, 0, -1, 2, -1, 4, -1, 6 },
    { -1, 0, 1, -1, 3, 4, -1, 6 },
    { -1, 0, 1, 2, 3, 4, -1, 6 },
};

static const uint8_t RI_LEVI_PRESET_LIVE[RI_LEVI_ALGO_N][RI_LEVI_NOPS] = {
    { 1, 1, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
};

/* Render order: modulators before their carriers (depth-descending).
 * Graph is acyclic by construction (presets) or validation (custom). */
static void order_compute(struct RILeviVoice *v) {
    uint8_t depth[RI_LEVI_NOPS], done[RI_LEVI_NOPS];
    uint32_t i, k, placed = 0u;
    for (i = 0u; i < RI_LEVI_NOPS; i++) {
        int cur = (int)i, d = 0;
        while (cur >= 0 && d <= (int)RI_LEVI_NOPS) {
            cur = v->mod_src[cur];
            d++;
        }
        depth[i] = (uint8_t)(d - 1);
        done[i] = 0u;
        v->order[i] = (uint8_t)i;
    }
    for (k = 0u; k < RI_LEVI_NOPS; k++) {
        uint32_t best = RI_LEVI_NOPS, i;
        for (i = 0u; i < RI_LEVI_NOPS; i++)
            if (!done[i] && (best == RI_LEVI_NOPS || depth[i] > depth[best]))
                best = i;
        if (best == RI_LEVI_NOPS)
            break; /* unreachable: placed counts every op once */
        v->order[placed++] = (uint8_t)best;
        done[best] = 1u;
    }
    for (; placed < RI_LEVI_NOPS; placed++)
        v->order[placed] = 0u;
}

static void voice_preset(struct RILeviVoice *v, uint32_t algo) {
    uint32_t i;
    for (i = 0u; i < RI_LEVI_NOPS; i++) {
        v->mod_src[i] = RI_LEVI_PRESET_SRC[algo][i];
        v->live[i] = RI_LEVI_PRESET_LIVE[algo][i];
    }
    v->algo = (uint8_t)algo;
    order_compute(v);
}

void levi_init_set(struct RILeviSet *s) {
    uint32_t i, o;
    if (!s)
        return;
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        struct RILeviVoice *v = &s->v[i];
        v->active = 0u;
        v->note = 0u;
        for (o = 0u; o < RI_LEVI_NOPS; o++) {
            v->op[o].phase = 0.0f;
            v->op[o].freq = 440.0f;
            v->op[o].ratio = RI_LEVI_DEF_RATIO;
            v->op[o].level = 1.0f;
            v->op[o].mode = RI_LEVI_FM;
            env_reset(&v->op[o].env);
            v->op[o].env.stage = RI_LEVI_SEG_IDLE;
        }
        voice_preset(v, RI_LEVI_ALGO_DUO);
        v->cutoff = RI_LEVI_DEF_CUTOFF;
        v->reso = RI_LEVI_DEF_RESO;
        v->level = 1.0f;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
    }
}

int levi_trigger(struct RILeviSet *s, uint32_t voice, uint8_t note) {
    struct RILeviVoice *v;
    float f;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES || note > 127u)
        return 2;
    v = &s->v[voice];
    f = note_hz(note);
    v->active = 1u;
    v->note = note;
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        if (v->live[o])
            op_reset(&v->op[o], f * v->op[o].ratio, 1.0f);
    v->lp1 = 0.0f;
    v->lp2 = 0.0f;
    return 0;
}

void levi_release(struct RILeviSet *s, uint32_t voice) {
    struct RILeviVoice *v;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES)
        return;
    v = &s->v[voice];
    for (o = 0u; o < RI_LEVI_NOPS; o++) {
        if (v->live[o] && v->op[o].env.stage != RI_LEVI_SEG_IDLE) {
            v->op[o].env.stage = RI_LEVI_SEG_R;
            v->op[o].env.stage_t = 0.0f;
        }
    }
}

int levi_set_param(struct RILeviSet *s, uint32_t voice, uint32_t id,
    float value) {
    struct RILeviVoice *v;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES)
        return 2;
    v = &s->v[voice];
    switch (id) {
    case RI_LEVI_CUTOFF:
        if (!(value >= 40.0f && value <= 18000.0f))
            return 2;
        v->cutoff = value;
        return 0;
    case RI_LEVI_RESO:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        v->reso = value;
        return 0;
    case RI_LEVI_MODE:
        if (value != (float)RI_LEVI_FM && value != (float)RI_LEVI_PM)
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o] && v->mod_src[o] >= 0)
                v->op[o].mode = (uint8_t)value;
        return 0;
    case RI_LEVI_RATIO:
        if (!(value >= 0.25f && value <= 64.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o] && v->mod_src[o] >= 0)
                v->op[o].ratio = value;
        return 0;
    default:
        return 2;
    }
}

/* Resonant lowpass (clean-room 2-pole Chamberlin SVF): cutoff/reso
 * modulate per render; states flushed (denormal-safe). f is clamped to
 * 1.0 (Dell 2026-09-28: the 1.8 ceiling admitted tunings past the
 * stability limit — inf/NaN ~300 samples after trigger, latched). */
static float lp_step(struct RILeviVoice *v, float x, float sr) {
    float f, q, hp, bp, lp;
    if (!(sr > 0.0f))
        return 0.0f;
    f = 2.0f * ri_sin(3.14159265f * v->cutoff / sr);
    if (f > 1.0f)
        f = 1.0f;
    if (f < 0.02f)
        f = 0.02f;
    q = 1.0f - v->reso * 0.85f;
    if (q < 0.05f)
        q = 0.05f;
    hp = x - v->lp1 * q - v->lp2;
    bp = v->lp1 + f * hp;
    lp = v->lp2 + f * bp;
    v->lp1 = ftz(bp);
    v->lp2 = ftz(lp);
    return lp;
}

int levi_set_param_ui(struct RILeviSet *s, uint32_t voice, uint32_t id,
    uint8_t val) {
    float f;
    if (!s || voice >= RI_LEVI_NVOICES)
        return 2;
    switch (id) {
    case RI_LEVI_CUTOFF:
        f = 40.0f * ri_pow2(((float)val / 127.0f) * 8.5f);
        if (f > 18000.0f)
            f = 18000.0f;
        return levi_set_param(s, voice, id, f);
    case RI_LEVI_RESO:
        return levi_set_param(s, voice, id, (float)val / 127.0f);
    case RI_LEVI_MODE:
        return levi_set_param(s, voice, id, val != 0u ? 1.0f : 0.0f);
    case RI_LEVI_RATIO:
        return levi_set_param(s, voice, id,
            0.25f * ri_pow2(((float)val / 127.0f) * 8.0f));
    default:
        return 2;
    }
}

float levi_voice_render(struct RILeviVoice *v, float sr) {
    float opout[RI_LEVI_NOPS] = { 0.0f }, mix = 0.0f, out;
    uint32_t k;
    int any_on = 0;
    if (!v || !v->active || !(sr > 0.0f))
        return 0.0f;
    for (k = 0u; k < RI_LEVI_NOPS; k++) {
        uint32_t i = v->order[k];
        struct RILeviOp *o;
        float m, carph, osc;
        int on;
        if (i >= RI_LEVI_NOPS || !v->live[i]) {
            opout[k & (RI_LEVI_NOPS - 1u)] = 0.0f;
            continue;
        }
        o = &v->op[i];
        on = env_tick(&o->env, sr);
        any_on |= on;
        if (!on) {
            opout[i] = 0.0f;
            continue;
        }
        /* Modulator output carries its envelope (v1 modsig law). */
        m = v->mod_src[i] >= 0 ? opout[(uint32_t)v->mod_src[i]] : 0.0f;
        /* Modulator always runs at its ratio; envelope gates depth. */
        if (o->mode == RI_LEVI_FM)
            o->phase += (o->freq + o->freq * RI_LEVI_MOD_INDEX * m) / sr;
        else
            o->phase += o->freq / sr;
        if (o->phase >= 1.0f)
            o->phase -= 1.0f;
        if (o->mode == RI_LEVI_PM)
            carph = o->phase + RI_LEVI_MOD_INDEX * m;
        else
            carph = o->phase;
        if (o->phase < 0.0f)
            o->phase += 1.0f;
        osc = ri_sin(carph * 6.2831853f) * o->level;
        opout[i] = osc * o->env.value;
        if (v->mod_src[i] < 0)
            mix += opout[i];
    }
    if (!any_on) {
        v->active = 0u;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        return 0.0f;
    }
    out = lp_step(v, mix, sr);
    if (!(out > -1e20f && out < 1e20f)) {
        /* Non-finite latch guard (Dell 2026-09-28): a poisoned filter
         * state must self-heal to silence, never mute the mix. */
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        return 0.0f;
    }
    return out * v->level;
}

void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr) {
    uint32_t i, v;
    if (!s || !out)
        return;
    for (i = 0u; i < n; i++) {
        float m = 0.0f;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], sr);
        out[i] = m;
    }
}

int levi_set_algo(struct RILeviSet *s, uint32_t voice, uint32_t algo) {
    if (!s || voice >= RI_LEVI_NVOICES || algo >= RI_LEVI_ALGO_N)
        return 2;
    voice_preset(&s->v[voice], algo);
    return 0;
}

int levi_algo_get(const struct RILeviSet *s, uint32_t voice) {
    if (!s || voice >= RI_LEVI_NVOICES)
        return -1;
    return (int)s->v[voice].algo;
}

int levi_set_route(struct RILeviSet *s, uint32_t voice, uint32_t op,
    int src) {
    struct RILeviVoice *v;
    int cur;
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return 2;
    if (src < -1 || src >= (int)RI_LEVI_NOPS)
        return 2;
    v = &s->v[voice];
    /* Acyclic only: src's chain must not reach op (self hits at once). */
    for (cur = src; cur >= 0; cur = v->mod_src[cur])
        if (cur == (int)op)
            return 2;
    v->mod_src[op] = (int8_t)src;
    v->live[op] = 1u;
    v->algo = RI_LEVI_ALGO_CUSTOM;
    order_compute(v);
    return 0;
}

int levi_route_get(const struct RILeviSet *s, uint32_t voice,
    uint32_t op) {
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return -2;
    return (int)s->v[voice].mod_src[op];
}
