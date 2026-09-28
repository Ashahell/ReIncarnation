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

void levi_init_set(struct RILeviSet *s) {
    uint32_t i;
    if (!s)
        return;
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        struct RILeviVoice *v = &s->v[i];
        v->active = 0u;
        v->note = 0u;
        v->car.phase = 0.0f;
        v->car.freq = 440.0f;
        v->car.ratio = 1.0f;
        v->car.level = 1.0f;
        v->car.mode = RI_LEVI_FM;
        env_reset(&v->car.env);
        v->car.env.stage = RI_LEVI_SEG_IDLE;
        v->mod.phase = 0.0f;
        v->mod.freq = 440.0f;
        v->mod.ratio = RI_LEVI_DEF_RATIO;
        v->mod.level = 1.0f;
        v->mod.mode = RI_LEVI_FM;
        env_reset(&v->mod.env);
        v->mod.env.stage = RI_LEVI_SEG_IDLE;
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
    if (!s || voice >= RI_LEVI_NVOICES || note > 127u)
        return 2;
    v = &s->v[voice];
    f = note_hz(note);
    v->active = 1u;
    v->note = note;
    op_reset(&v->car, f, 1.0f);
    op_reset(&v->mod, f * v->mod.ratio, 1.0f);
    v->lp1 = 0.0f;
    v->lp2 = 0.0f;
    return 0;
}

void levi_release(struct RILeviSet *s, uint32_t voice) {
    struct RILeviVoice *v;
    if (!s || voice >= RI_LEVI_NVOICES)
        return;
    v = &s->v[voice];
    if (v->car.env.stage != RI_LEVI_SEG_IDLE) {
        v->car.env.stage = RI_LEVI_SEG_R;
        v->car.env.stage_t = 0.0f;
    }
    if (v->mod.env.stage != RI_LEVI_SEG_IDLE) {
        v->mod.env.stage = RI_LEVI_SEG_R;
        v->mod.env.stage_t = 0.0f;
    }
}

int levi_set_param(struct RILeviSet *s, uint32_t voice, uint32_t id,
    float value) {
    struct RILeviVoice *v;
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
        v->mod.mode = (uint8_t)value;
        return 0;
    case RI_LEVI_RATIO:
        if (!(value >= 0.25f && value <= 64.0f))
            return 2;
        v->mod.ratio = value;
        return 0;
    default:
        return 2;
    }
}

/* Resonant lowpass (clean-room 2-pole SVF): cutoff/reso modulate per
 * render; states flushed (denormal-safe). */
static float lp_step(struct RILeviVoice *v, float x, float sr) {
    float f, q, hp, bp, lp;
    if (!(sr > 0.0f))
        return 0.0f;
    f = 2.0f * ri_sin(3.14159265f * v->cutoff / sr);
    if (f > 1.8f)
        f = 1.8f;
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
        return levi_set_param(s, voice, id, val >= 64u ? 1.0f : 0.0f);
    case RI_LEVI_RATIO:
        return levi_set_param(s, voice, id,
            0.25f * ri_pow2(((float)val / 127.0f) * 8.0f));
    default:
        return 2;
    }
}

float levi_voice_render(struct RILeviVoice *v, float sr) {    float modsig, carph, osc, out;
    int car_on, mod_on;
    if (!v || !v->active || !(sr > 0.0f))
        return 0.0f;
    mod_on = env_tick(&v->mod.env, sr);
    car_on = env_tick(&v->car.env, sr);
    if (!car_on && !mod_on) {
        v->active = 0u;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        return 0.0f;
    }
    /* Modulator always runs at its ratio; envelope gates its depth. */
    v->mod.phase += v->mod.freq / sr;
    if (v->mod.phase >= 1.0f)
        v->mod.phase -= 1.0f;
    modsig = ri_sin(v->mod.phase * 6.2831853f) * v->mod.env.value * v->mod.level;
    if (v->mod.mode == RI_LEVI_PM)
        carph = v->car.phase + RI_LEVI_MOD_INDEX * modsig;
    else
        carph = v->car.phase;
    if (v->mod.mode == RI_LEVI_FM)
        v->car.phase += (v->car.freq + v->car.freq * RI_LEVI_MOD_INDEX * modsig) / sr;
    else
        v->car.phase += v->car.freq / sr;
    if (v->car.phase >= 1.0f)
        v->car.phase -= 1.0f;
    if (v->car.phase < 0.0f)
        v->car.phase += 1.0f;
    osc = ri_sin(carph * 6.2831853f) * v->car.env.value * v->car.level;
    out = lp_step(v, osc, sr);
    if (!car_on) {
        /* Carrier resting while modulator still rings: output is exact
         * 0 (filter states already flushed on full rest above). */
        out = 0.0f;
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
