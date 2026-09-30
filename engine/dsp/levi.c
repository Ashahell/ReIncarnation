/* levi.c — Levi FM voice bank bodies (owner 2026-09-28, v1 slice 3a).
 * 2-op FM/PM + per-operator DAHDSR + resonant lowpass. Render-contract
 * safe: bounded, no allocation, no IO; denormal-safe (ftz states, short
 * envelopes end exact).
 */
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
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

/* Segment shape (curve c -64..+63; 0 = the v1 linear ramp exactly).
 * Rising segments: exp (c < 0) starts slow, log (c > 0) starts fast.
 * Falling segments: exp (c > 0) drops fast first, log (c < 0) slow. */
static float seg_shape(float x, int c, int rising) {
    float p;
    if (c == 0)
        return x;
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    p = ri_pow2((float)(rising ? -c : c) / 32.0f);
    if (rising)
        return ri_pow2(p * ri_log2(x));
    return 1.0f - ri_pow2(p * ri_log2(1.0f - x));
}

/* One envelope sample; returns 1 while sounding, 0 at rest end.
 * tscale: device time bias for this segment's family (1 = none). */
static int env_tick_b(struct RILeviEnv *e, float sr, const float *tscale) {
    float dt, span, from, to;
    int c = 0, rising = 0;
    if (!e || sr <= 0.0f)
        return 0;
    if (e->stage == RI_LEVI_SEG_IDLE)
        return 0;
    if (e->stage == RI_LEVI_SEG_S) {
        if (e->relpend) {                    /* freerun: held release now */
            e->relpend = 0u;
            e->stage = RI_LEVI_SEG_R;
            e->stage_t = 0.0f;
            e->seg_from = e->value;
        } else if (e->loop && !e->loopn) {
            /* Contour loop: sustain falls back to attack (own loop
             * law; release still rests via R). */
            e->stage = RI_LEVI_SEG_A;
            e->stage_t = 0.0f;
            e->seg_from = e->value;
        } else {
            return 1;
        }
    }
    dt = 1.0f / sr;
    e->stage_t += dt;
    span = e->times[e->stage];
    if (tscale) {
        if (e->stage == RI_LEVI_SEG_A)
            span *= tscale[0];
        else if (e->stage == RI_LEVI_SEG_D2)
            span *= tscale[1];
        else if (e->stage == RI_LEVI_SEG_R)
            span *= tscale[2];
    }
    from = e->value;
    switch (e->stage) {
    case RI_LEVI_SEG_D:
        to = 0.0f;
        break;
    case RI_LEVI_SEG_A:
        to = 1.0f;
        c = e->curve[0];
        rising = 1;
        break;
    case RI_LEVI_SEG_H:
        to = 1.0f;
        break;
    case RI_LEVI_SEG_D2:
        to = e->sustain;
        c = e->curve[1];
        break;
    default: /* RI_LEVI_SEG_R */
        to = 0.0f;
        c = e->curve[2];
        break;
    }
    if (span <= 0.0f || e->stage_t >= span) {
        uint8_t done = e->stage;
        e->value = to;
        e->stage_t = 0.0f;
        if (e->stage == RI_LEVI_SEG_R) {
            e->stage = RI_LEVI_SEG_IDLE;
            e->value = 0.0f;
            return 0;
        }
        if (e->loopn && done == e->loopend && !e->relpend &&
            (e->loopn == 255u || e->loopleft > 0u)) {
            /* Counted/infinite loop over the stage range (manual p. 74):
             * back to Delay (or Attack when Delay is 0). */
            if (e->loopn != 255u)
                e->loopleft--;
            e->stage = e->times[RI_LEVI_SEG_D] > 0.0f ? RI_LEVI_SEG_D : RI_LEVI_SEG_A;
            e->value = 0.0f;                  /* the loop restarts the contour */
            e->seg_from = 0.0f;
            return 1;
        }
        e->stage++;
        e->seg_from = e->value;
        if (e->stage == RI_LEVI_SEG_S)
            e->value = e->sustain;
        return 1;
    }
    /* True segment ramp from the segment's start value (fidelity P2: the
     * v1 law re-read the running value every sample, so segments
     * collapsed long before their set time). */
    (void)from;
    {
        float x = e->stage_t / span, y = c == 0 ? x : seg_shape(x, c, rising);
        e->value = e->seg_from + (to - e->seg_from) * y;
    }
    return 1;
}

/* Quantized envelope output (Quantize, manual p. 72): value snapped to
 * q steps; 0 = continuous (the v1 value). */
static float env_out(const struct RILeviEnv *e) {
    if (!e->quant)
        return e->value;
    return (float)(int)(e->value * (float)e->quant + 0.5f) / (float)e->quant;
}

static void op_state_reset(struct RILeviOpState *st, float freq) {
    st->phase = 0.0f;
    st->freq = freq;
    st->ps = 0.0f;
    st->last = 0.0f;
    st->amp = 0.0f;
    st->env.relpend = 0u;
    st->env.loopleft = st->env.loopn == 255u ? 0u : (uint8_t)(st->env.loopn > 1u ? st->env.loopn - 1u : 0u);
    st->env.seg_from = 0.0f;
    /* Runtime only: times/sustain/loop are voice params (set_param),
     * preserved across triggers (unlike v1 constants). */
    st->env.value = 0.0f;
    st->env.stage = RI_LEVI_SEG_D;
    st->env.stage_t = 0.0f;
}


/* ---- Own wave set (fidelity P2; clean-room: authored formulas, own
 * names, none of the hardware's tables). 8 families x 16. Discontinuous
 * shapes carry a PolyBLEP correction (dt = cycles per sample). ---- */
static const char *const WAVE_FAMILY[8] = {
    "CLASSIC", "PULSE", "HARM", "FOLD", "WARP", "SYNC", "RING", "CHEBY"
};
static const char *const WAVE_CLASSIC[16] = {
    "SINE", "TRIANGLE", "TRISAW", "SAW", "SQUARE", "HALF SINE", "ABS SINE", "QTR SINE",
    "SINE CUBE", "OCTAVE", "ORGAN", "SOFT SQUARE", "SOFT SAW", "PULSE 25", "PULSE 12", "TRAPEZOID"
};

static float blep(float t, float dt) {
    if (dt <= 0.0f)
        return 0.0f;
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

static float frac1(float x) {
    x -= (float)(int)x;
    return x < 0.0f ? x + 1.0f : x;
}

static float w_saw(float ph, float dt) {
    return (2.0f * ph - 1.0f) - blep(ph, dt);
}

static float w_pulse(float ph, float dt, float w) {
    float v = ph < w ? 1.0f : -1.0f;
    return v + blep(ph, dt) - blep(frac1(ph + 1.0f - w), dt);
}

static float w_tri(float ph) {
    float t = 4.0f * ph;
    return ph < 0.25f ? t : ph < 0.75f ? 2.0f - t : t - 4.0f;
}

const char *ri_levi_wave_name(uint32_t w) {
    if (w >= RI_LEVI_NWAVES)
        return "";
    if (w < 16u)
        return WAVE_CLASSIC[w];
    return WAVE_FAMILY[w >> 4];   /* the UI appends the number (w & 15) + 1 */
}

float ri_levi_wave(uint32_t w, float ph, float dt) {
    const float TAU = 6.2831853f;
    uint32_t f = w >> 4, k = w & 15u;
    float s = ri_sin(ph * TAU);
    if (w == 0u)
        return s;
    switch (f) {
    case 0u:
        switch (k) {
        case 1u: return w_tri(ph);
        case 2u: return 0.5f * (w_tri(ph) + w_saw(ph, dt));
        case 3u: return w_saw(ph, dt);
        case 4u: return w_pulse(ph, dt, 0.5f);
        case 5u: return ph < 0.5f ? 2.0f * s - 1.0f : -1.0f;
        case 6u: return 2.0f * (s < 0.0f ? -s : s) - 1.0f;
        case 7u: return ph < 0.25f || (ph >= 0.5f && ph < 0.75f) ? (s < 0.0f ? -s : s) * 2.0f - 1.0f : -1.0f;
        case 8u: return s * s * s;
        case 9u: return 0.6f * s + 0.4f * ri_sin(2.0f * ph * TAU);
        case 10u: return 0.5f * s + 0.3f * ri_sin(2.0f * ph * TAU) + 0.2f * ri_sin(3.0f * ph * TAU);
        case 11u: return ri_tanh(3.0f * s) / ri_tanh(3.0f);
        case 12u: return ri_tanh(2.0f * w_saw(ph, dt)) / ri_tanh(2.0f);
        case 13u: return w_pulse(ph, dt, 0.25f);
        case 14u: return w_pulse(ph, dt, 0.125f);
        default: {
            float t = 3.0f * w_tri(ph);
            return t > 1.0f ? 1.0f : t < -1.0f ? -1.0f : t;
        }
        }
    case 1u: /* PULSE: widths 4..49 % */
        return w_pulse(ph, dt, 0.04f + 0.03f * (float)k);
    case 2u: { /* HARM: fundamental + one partial (2..9), two weights */
        float h = (float)(2u + (k >> 1)), a = (k & 1u) ? 0.7f : 0.35f;
        return (s + a * ri_sin(h * ph * TAU)) / (1.0f + a);
    }
    case 3u: { /* FOLD: triangle-folded sine, gain 1.2..4.2 */
        float y = frac1(0.25f * s * (1.2f + 0.2f * (float)k) + 0.25f);
        return 4.0f * (y < 0.5f ? 0.5f - y : y - 0.5f) - 1.0f;
    }
    case 4u: { /* WARP: phase knee 0.05..0.95 (phase-distorted sine) */
        float kn = 0.05f + 0.06f * (float)k;
        float q = ph < kn ? 0.5f * ph / kn : 0.5f + 0.5f * (ph - kn) / (1.0f - kn);
        return ri_sin(q * TAU);
    }
    case 5u: { /* SYNC: slave saw at 1.25..5x, reset each cycle */
        float r = 1.25f + 0.25f * (float)k;
        return 2.0f * frac1(ph * r) - 1.0f;
    }
    case 6u: /* RING: sine x sine(k+2) */
        return s * ri_sin((float)(k + 2u) * ph * TAU);
    default: { /* CHEBY: T_n(sin) mixed with sine, n = 2..17 */
        float t0 = 1.0f, t1 = s, tn = s;
        uint32_t n;
        for (n = 2u; n <= k + 2u; n++) {
            tn = 2.0f * s * t1 - t0;
            t0 = t1;
            t1 = tn;
        }
        return 0.5f * (s + tn);
    }
    }
}

/* ---- Per-op parameter laws (UI 0..127 -> values; own E0 maps) ---- */
static const uint8_t OP_LO[RI_LEVI_OP_NPARAM] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const uint8_t OP_HI[RI_LEVI_OP_NPARAM] = {
    127, 1, 2, 127, 127, 127, 127, 127, 127, 127, 1, 6,
    127, 127, 127, 127, 127, 127, 1, 127, 127, 127, 15, 50, 2, 1, 1, 1, 127, 8, 8, 8
};

int ri_levi_op_range(uint32_t param, int *lo, int *hi) {
    if (param >= RI_LEVI_OP_NPARAM || !lo || !hi)
        return 2;
    *lo = OP_LO[param];
    *hi = OP_HI[param];
    return 0;
}

float ri_levi_ratio(uint8_t idx) {
    if (idx == 0u)
        return 0.25f;
    if (idx == 1u)
        return 0.5f;
    return (float)(idx > 65u ? 64u : idx - 1u);
}

int ri_levi_op_default(uint32_t op, uint32_t param) {
    switch (param) {
    case RI_LEVI_OP_PMODE: return 1;
    case RI_LEVI_OP_COARSE: return 2;             /* ratio 1.00 */
    case RI_LEVI_OP_FINE: return 64;
    case RI_LEVI_OP_ENVL: return op == 0u ? 127 : 72; /* +128 / +16 */
    case RI_LEVI_OP_KEYTRK: return 96;            /* 100 % */
    case RI_LEVI_OP_ATTACK: return 14;            /* ~5 ms, the v1 attack */
    case RI_LEVI_OP_DECAY: return 34;             /* ~0.3 s */
    case RI_LEVI_OP_SUSTAIN: return 102;
    case RI_LEVI_OP_RELEASE: return 28;           /* ~0.15 s */
    case RI_LEVI_OP_ACURVE: case RI_LEVI_OP_DCURVE: case RI_LEVI_OP_RCURVE: return 64;
    case RI_LEVI_OP_STAGELOOP: return 2;
    default: return 0;
    }
}

float ri_levi_env_time(uint32_t param, uint8_t val, uint32_t speed) {
    /* Manual ranges (p. 38): Fast D 32 s, A/H 36 s, D/R 60 s;
     * Slow D 60 s, A/H 600 s, D/R 900 s. Own quartic map (fine short
     * times). */
    float mx, u = (float)(val > 127u ? 127u : val) / 127.0f;
    switch (param) {
    case RI_LEVI_OP_DELAY: mx = speed ? 60.0f : 32.0f; break;
    case RI_LEVI_OP_ATTACK: case RI_LEVI_OP_HOLD: mx = speed ? 600.0f : 36.0f; break;
    default: mx = speed ? 900.0f : 60.0f; break;
    }
    return mx * u * u * u * u;
}

static const uint8_t QUANT_STEPS[16] = { 0, 2, 3, 4, 5, 6, 8, 10, 12, 16, 24, 32, 48, 64, 96, 128 };

/* Recompute an op's derived values after a UI change; env params go
 * into both bank states of the voice. */
static int reaches(const uint8_t *feeds, uint32_t from, uint32_t to);
static void bank_load(struct RILeviVoice *v, uint32_t bank, uint32_t algo);

static void op_apply(struct RILeviVoice *v, uint32_t o, uint32_t p) {
    struct RILeviOp *op = &v->op[o];
    const uint8_t *u = op->ui;
    uint32_t b;
    switch (p) {
    case RI_LEVI_OP_WAVE: op->wave = u[p]; break;
    case RI_LEVI_OP_INVERT: op->invert = u[p] ? 1u : 0u; break;
    case RI_LEVI_OP_PMODE: case RI_LEVI_OP_COARSE: case RI_LEVI_OP_FINE: {
        int fine = (int)u[RI_LEVI_OP_FINE] - 64;
        op->pmode = u[RI_LEVI_OP_PMODE] > 2u ? 2u : u[RI_LEVI_OP_PMODE];
        if (op->pmode == 0u) {                    /* semitone + cent */
            int semi = (int)u[RI_LEVI_OP_COARSE] - 64;
            if (semi < -36) semi = -36;
            if (semi > 36) semi = 36;
            if (fine < -50) fine = -50;
            if (fine > 50) fine = 50;
            op->pitchmul = ri_pow2(((float)semi + (float)fine / 100.0f) / 12.0f);
        } else if (op->pmode == 1u) {             /* ratio x (1 + fine %) */
            float r = ri_levi_ratio(u[RI_LEVI_OP_COARSE]);
            float fp = fine < 0 ? (float)fine * 50.0f / 64.0f : (float)fine * 100.0f / 63.0f;
            op->ratio = fine == 0 ? r : r * (1.0f + fp / 100.0f);
        } else {                                  /* fixed Hz + fraction */
            float c = (float)u[RI_LEVI_OP_COARSE] / 127.0f;
            op->hz = 10000.0f * c * c * c + (float)u[RI_LEVI_OP_FINE] * 0.99f / 127.0f;
        }
        break;
    }
    case RI_LEVI_OP_INIT: op->init = (float)u[p] / 127.0f; break;
    case RI_LEVI_OP_ENVL: op->envl = u[p] >= 127u ? 1.0f : ((float)u[p] - 64.0f) * 2.0f / 128.0f; break;
    case RI_LEVI_OP_FEEDBACK: op->fb = (float)u[p] / 127.0f; break;
    case RI_LEVI_OP_KEYTRK: op->kt = ((float)u[p] - 64.0f) / 32.0f; break;
    case RI_LEVI_OP_PHASE: op->phase0 = (float)u[p] / 128.0f; break;
    case RI_LEVI_OP_DIRECT: op->direct = u[p] ? 1u : 0u; break;
    case RI_LEVI_OP_MODE: op->mode = u[p] > 6u ? 6u : u[p]; break;
    case RI_LEVI_OP_TGT1: case RI_LEVI_OP_TGT2: case RI_LEVI_OP_TGT3: {
        /* Custom grid row (manual p. 60): up to 3 targets; a target that
         * would close a loop (or is the op itself) is dropped. */
        uint8_t mask = 0u, save = v->cfeeds[o];
        uint32_t t;
        for (t = RI_LEVI_OP_TGT1; t <= RI_LEVI_OP_TGT3; t++)
            if (u[t] >= 1u && u[t] <= 8u && (uint32_t)(u[t] - 1u) != o)
                mask |= (uint8_t)(1u << (u[t] - 1u));
        v->cfeeds[o] = 0u;
        for (t = 0u; t < RI_LEVI_NOPS; t++)
            if (((mask >> t) & 1u) && !reaches(v->cfeeds, t, o))
                v->cfeeds[o] |= (uint8_t)(1u << t);
        (void)save;
        if (v->amode == RI_LEVI_AMODE_CUSTOM) {
            bank_load(v, 0u, RI_LEVI_ALGO_CUSTOM);
            v->algo = RI_LEVI_ALGO_CUSTOM;
        }
        break;
    }
    default:
        if (p == RI_LEVI_OP_SPEED && (u[p] ? 1u : 0u) == op->speed)
            break;                                /* same range: times stand */
        for (b = 0u; b < 2u; b++) {
            struct RILeviEnv *e = &v->st[b][o].env;
            uint32_t spd = u[RI_LEVI_OP_SPEED] ? 1u : 0u;
            if (p == RI_LEVI_OP_DELAY || p == RI_LEVI_OP_SPEED)
                e->times[RI_LEVI_SEG_D] = ri_levi_env_time(RI_LEVI_OP_DELAY, u[RI_LEVI_OP_DELAY], spd);
            if (p == RI_LEVI_OP_ATTACK || p == RI_LEVI_OP_SPEED)
                e->times[RI_LEVI_SEG_A] = ri_levi_env_time(RI_LEVI_OP_ATTACK, u[RI_LEVI_OP_ATTACK], spd);
            if (p == RI_LEVI_OP_HOLD || p == RI_LEVI_OP_SPEED)
                e->times[RI_LEVI_SEG_H] = ri_levi_env_time(RI_LEVI_OP_HOLD, u[RI_LEVI_OP_HOLD], spd);
            if (p == RI_LEVI_OP_DECAY || p == RI_LEVI_OP_SPEED)
                e->times[RI_LEVI_SEG_D2] = ri_levi_env_time(RI_LEVI_OP_DECAY, u[RI_LEVI_OP_DECAY], spd);
            if (p == RI_LEVI_OP_RELEASE || p == RI_LEVI_OP_SPEED)
                e->times[RI_LEVI_SEG_R] = ri_levi_env_time(RI_LEVI_OP_RELEASE, u[RI_LEVI_OP_RELEASE], spd);
            if (p == RI_LEVI_OP_SUSTAIN)
                e->sustain = (float)u[p] / 127.0f;
            e->curve[0] = (int8_t)((int)u[RI_LEVI_OP_ACURVE] - 64);
            e->curve[1] = (int8_t)((int)u[RI_LEVI_OP_DCURVE] - 64);
            e->curve[2] = (int8_t)((int)u[RI_LEVI_OP_RCURVE] - 64);
            e->quant = QUANT_STEPS[u[RI_LEVI_OP_QUANT] & 15u];
            e->loopn = u[RI_LEVI_OP_LOOP] == 0u ? 0u : u[RI_LEVI_OP_LOOP] >= 50u ? 255u
                : (uint8_t)(u[RI_LEVI_OP_LOOP] + 1u);
            e->loopend = u[RI_LEVI_OP_STAGELOOP] == 0u ? RI_LEVI_SEG_A
                : u[RI_LEVI_OP_STAGELOOP] == 1u ? RI_LEVI_SEG_H : RI_LEVI_SEG_D2;
            e->freerun = u[RI_LEVI_OP_FREERUN] ? 1u : 0u;
        }
        op->speed = u[RI_LEVI_OP_SPEED] ? 1u : 0u;
        break;
    }
}

int levi_set_op_ui(struct RILeviSet *s, uint32_t voice, uint32_t op, uint32_t param, uint8_t val) {
    struct RILeviVoice *v;
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS || param >= RI_LEVI_OP_NPARAM)
        return 2;
    v = &s->v[voice];
    if (val > OP_HI[param])
        val = OP_HI[param];
    v->op[op].ui[param] = val;
    op_apply(v, op, param);
    return 0;
}

/* Own algorithm bank (fidelity P3; clean-room, authored by rule in the
 * scratch generator, none of the hardware's 140+): feeds[i] = the ops
 * op i modulates, bit t = op t; every edge points to a lower op, so the
 * graph is acyclic; op 0 (OSC 1) is always a carrier. 0..7 are the v1
 * presets. */
static const uint8_t RI_LEVI_PRESET_FEEDS[RI_LEVI_ALGO_N][RI_LEVI_NOPS] = {
    { 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40 },
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x10, 0x20, 0x40 },
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x10, 0x00, 0x40 },
    { 0x00, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00, 0x40 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x10, 0x00, 0x40 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x40 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x00, 0x20, 0x40 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x00, 0x20, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x10, 0x20, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x10, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x10, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x00, 0x20, 0x00 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00, 0x00 },
    { 0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x01, 0x01, 0x04, 0x08, 0x10, 0x20, 0x40 },
    { 0x00, 0x01, 0x01, 0x01, 0x08, 0x10, 0x20, 0x40 },
    { 0x00, 0x01, 0x01, 0x01, 0x01, 0x10, 0x20, 0x40 },
    { 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x20, 0x40 },
    { 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x40 },
    { 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F },
    { 0x00, 0x01, 0x01, 0x02, 0x02, 0x04, 0x04, 0x02 },
    { 0x00, 0x01, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10 },
    { 0x00, 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x01 },
    { 0x00, 0x00, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02 },
    { 0x00, 0x00, 0x01, 0x02, 0x01, 0x02, 0x01, 0x02 },
    { 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x03, 0x03 },
    { 0x00, 0x01, 0x00, 0x04, 0x0A, 0x10, 0x20, 0x40 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x12, 0x20, 0x40 },
    { 0x00, 0x01, 0x02, 0x00, 0x08, 0x10, 0x00, 0x29 },
    { 0x00, 0x01, 0x00, 0x04, 0x00, 0x10, 0x00, 0x15 },
    { 0x00, 0x01, 0x01, 0x06, 0x0C, 0x18, 0x30, 0x60 },
    { 0x00, 0x01, 0x03, 0x06, 0x0C, 0x18, 0x30, 0x60 },
    { 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02 },
    { 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10 },
    { 0x00, 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x00, 0x00, 0x20 },
    { 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 0x10, 0x40 },
    { 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x20, 0x40 },
    { 0x00, 0x01, 0x01, 0x02, 0x02, 0x00, 0x00, 0x40 },
    { 0x00, 0x01, 0x03, 0x04, 0x08, 0x10, 0x00, 0x00 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x01 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x21, 0x40 },
    { 0x00, 0x01, 0x02, 0x01, 0x08, 0x01, 0x20, 0x01 },
    { 0x00, 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x04 },
    { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x08 },
    { 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x10 },
    { 0x00, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02, 0x20 },
    { 0x00, 0x01, 0x01, 0x04, 0x04, 0x04, 0x04, 0x40 },
    { 0x00, 0x01, 0x02, 0x01, 0x08, 0x08, 0x08, 0x01 },
};
static const uint8_t RI_LEVI_PRESET_LIVE[RI_LEVI_ALGO_N] = {
    0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

uint8_t ri_levi_preset_feeds(uint32_t algo, uint32_t op) {
    return (algo < RI_LEVI_ALGO_N && op < RI_LEVI_NOPS) ? RI_LEVI_PRESET_FEEDS[algo][op] : 0u;
}

/* Render order: modulators before the ops they modulate (Kahn over the
 * feeds masks; any leftover (a cycle) keeps index order, never loops). */
static void order_compute(const uint8_t *feeds, uint8_t *order) {
    uint8_t done = 0u;
    uint32_t placed = 0u, i, j, guard;
    for (guard = 0u; guard < RI_LEVI_NOPS && placed < RI_LEVI_NOPS; guard++) {
        uint32_t best = RI_LEVI_NOPS;
        /* highest ready op first (feeders are higher indexed in presets) */
        for (i = RI_LEVI_NOPS; i-- > 0u;) {
            int ready = !((done >> i) & 1u);
            for (j = 0u; j < RI_LEVI_NOPS && ready; j++)
                if (!((done >> j) & 1u) && j != i && ((feeds[j] >> i) & 1u))
                    ready = 0;                       /* an unplaced feeder of i */
            if (ready) {
                best = i;
                break;
            }
        }
        if (best == RI_LEVI_NOPS)
            break;
        order[placed++] = (uint8_t)best;
        done |= (uint8_t)(1u << best);
    }
    for (i = 0u; i < RI_LEVI_NOPS && placed < RI_LEVI_NOPS; i++)
        if (!((done >> i) & 1u)) {
            order[placed++] = (uint8_t)i;
            done |= (uint8_t)(1u << i);
        }
}

/* 1 when op reaches `to` through the feeds graph (for cycle refusal). */
static int reaches(const uint8_t *feeds, uint32_t from, uint32_t to) {
    uint8_t seen = 0u, front = (uint8_t)(1u << from);
    uint32_t it, i;
    for (it = 0u; it < RI_LEVI_NOPS && front; it++) {
        uint8_t next = 0u;
        for (i = 0u; i < RI_LEVI_NOPS; i++)
            if ((front >> i) & 1u)
                next |= feeds[i];
        if ((next >> to) & 1u)
            return 1;
        seen |= front;
        front = (uint8_t)(next & (uint8_t)~seen);
    }
    return 0;
}

/* Load a routing into bank A/B: a preset (0..63), SILENCE, or the
 * custom grid (RI_LEVI_ALGO_CUSTOM). */
/* Bank ids: presets 0..63, RI_LEVI_ALGO_CUSTOM (the grid), BANK_SILENCE.
 * The slot value SILENCE (64) equals the CUSTOM id, so morph maps it. */
#define BANK_SILENCE 0xFFu

static void bank_load(struct RILeviVoice *v, uint32_t bank, uint32_t algo) {
    uint32_t i;
    uint8_t *f = bank ? v->feedsB : v->feeds;
    uint8_t *ord = bank ? v->orderB : v->order;
    uint8_t *live = bank ? v->liveB : v->live;
    for (i = 0u; i < RI_LEVI_NOPS; i++) {
        if (algo == RI_LEVI_ALGO_CUSTOM) {
            f[i] = v->cfeeds[i];
            live[i] = 1u;
        } else if (algo < RI_LEVI_ALGO_N) {
            f[i] = RI_LEVI_PRESET_FEEDS[algo][i];
            live[i] = (uint8_t)((RI_LEVI_PRESET_LIVE[algo] >> i) & 1u);
        } else {                                      /* BANK_SILENCE */
            f[i] = 0u;
            live[i] = 0u;
        }
    }
    order_compute(f, ord);
}

static void bank_preset(struct RILeviVoice *v, uint32_t bank, uint32_t algo) {
    bank_load(v, bank, algo);
}

static void voice_preset(struct RILeviVoice *v, uint32_t algo) {
    bank_preset(v, 0u, algo);
    v->algo = (uint8_t)algo;
}

/* Active morph list (slot 0 always; OFF slots are skipped, so a later
 * slot moves up, manual p. 59). Returns the count, list in out[]. */
static uint32_t morph_list(const struct RILeviVoice *v, uint8_t *out) {
    uint32_t i, n = 0u;
    for (i = 0u; i < RI_LEVI_NSLOTS; i++)
        if (i == 0u || v->slot[i] != RI_LEVI_SLOT_OFF)
            out[n++] = i == 0u && v->slot[0] >= RI_LEVI_ALGO_N ? 0u : v->slot[i];
    return n;
}

/* Morph mode: banks A/B = the list entries around the position; the
 * blend is the position within the step (0..100). Crossing a step loads
 * the new pair; bank states carry over (B -> A going up, A -> B going
 * down) so voices glide. */
static void morph_apply(struct RILeviVoice *v) {
    uint8_t list[RI_LEVI_NSLOTS];
    uint32_t n = morph_list(v, list), maxp, a, o;
    maxp = (n - 1u) * 100u;
    if (v->mpos > maxp)
        v->mpos = (uint16_t)maxp;
    a = v->mpos / 100u;
    if (a >= n - 1u && n > 1u)
        a = n - 2u;
    if (n == 1u)
        a = 0u;
    if (a != v->mslotA) {
        if (a == (uint32_t)v->mslotA + 1u)
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                v->st[0][o] = v->st[1][o];
        else if (a + 1u == v->mslotA)
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                v->st[1][o] = v->st[0][o];
        v->mslotA = (uint8_t)a;
    }
    bank_load(v, 0u, list[a] >= RI_LEVI_ALGO_N ? BANK_SILENCE : list[a]);
    bank_load(v, 1u, n > 1u ? (list[a + 1u] >= RI_LEVI_ALGO_N ? BANK_SILENCE : list[a + 1u])
        : (list[a] >= RI_LEVI_ALGO_N ? BANK_SILENCE : list[a]));
    v->algo = list[0] < RI_LEVI_ALGO_N ? list[0] : 0u;
    v->algoB = n > 1u ? list[a + 1u] : list[a];
    v->morph = (uint8_t)(n > 1u ? v->mpos - a * 100u : 0u);
}

void levi_init_set(struct RILeviSet *s) {
    uint32_t i, o;
    if (!s)
        return;
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        struct RILeviVoice *v = &s->v[i];
        uint32_t b;
        v->active = 0u;
        v->note = 0u;
        v->algoB = RI_LEVI_ALGO_DUO;
        v->morph = 0u;
        v->amode = RI_LEVI_AMODE_SINGLE;
        for (o = 0u; o < RI_LEVI_NSLOTS; o++)
            v->slot[o] = o == 0u ? RI_LEVI_ALGO_DUO : RI_LEVI_SLOT_OFF;
        v->mslotA = 0u;
        v->mute = 0u;
        v->solo = 0u;
        v->mpos = 0u;
        v->padm[0] = v->padm[1] = 0u;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            v->cfeeds[o] = RI_LEVI_PRESET_FEEDS[RI_LEVI_ALGO_DUO][o];
        for (o = 0u; o < RI_LEVI_NOPS; o++) {
            uint32_t pp;
            struct RILeviOp *op = &v->op[o];
            op->ratio = RI_LEVI_DEF_RATIO;
            op->level = 1.0f;
            op->mode = RI_LEVI_FM;
            op->wave = 0u;
            op->invert = 0u;
            op->pmode = 1u;
            op->pitchmul = 1.0f;
            op->hz = 0.0f;
            op->init = 0.0f;
            op->envl = o == 0u ? 1.0f : 0.125f;   /* v1: carrier full, modulator index 0.5 */
            op->fb = 0.0f;
            op->kt = 1.0f;
            op->phase0 = 0.0f;
            op->direct = 0u;
            op->speed = 0u;
            op->pad2[0] = op->pad2[1] = 0u;
            for (pp = 0u; pp < RI_LEVI_OP_NPARAM; pp++)
                op->ui[pp] = (uint8_t)ri_levi_op_default(o, pp);
        }
        for (b = 0u; b < 2u; b++) {
            bank_preset(v, b, RI_LEVI_ALGO_DUO);
            for (o = 0u; o < RI_LEVI_NOPS; o++) {
                v->st[b][o].phase = 0.0f;
                v->st[b][o].freq = 440.0f;
                v->st[b][o].ps = 0.0f;
                env_reset(&v->st[b][o].env);
                v->st[b][o].env.stage = RI_LEVI_SEG_IDLE;
                v->st[b][o].env.loop = 0u;
                v->st[b][o].env.curve[0] = v->st[b][o].env.curve[1] = v->st[b][o].env.curve[2] = 0;
                v->st[b][o].env.quant = 0u;
                v->st[b][o].env.loopn = 0u;
                v->st[b][o].env.loopleft = 0u;
                v->st[b][o].env.loopend = RI_LEVI_SEG_D2;
                v->st[b][o].env.freerun = 0u;
                v->st[b][o].env.relpend = 0u;
                v->st[b][o].env.pad2[0] = v->st[b][o].env.pad2[1] = v->st[b][o].env.pad2[2] = 0u;
                v->st[b][o].env.seg_from = 0.0f;
                v->st[b][o].last = 0.0f;
                v->st[b][o].amp = 0.0f;
            }
        }
        voice_preset(v, RI_LEVI_ALGO_DUO);
        v->bias_envl = 0.0f;
        v->bias_t[0] = v->bias_t[1] = v->bias_t[2] = 1.0f;
        v->cutoff = RI_LEVI_DEF_CUTOFF;
        v->reso = RI_LEVI_DEF_RESO;
        v->level = 1.0f;
        v->drive = 0.0f;
        v->ftype = RI_LEVI_FTYPE_LP;
        v->cutoff2 = RI_LEVI_DEF_CUTOFF;
        v->reso2 = RI_LEVI_DEF_RESO;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        v->lp3 = 0.0f;
        v->lp4 = 0.0f;
        for (o = 0u; o < RI_LEVI_NLFO; o++) {
            v->lfo[o].rate = ri_levi_lfo_rate(64u);
            v->lfo[o].shape = RI_LEVI_LFO_SMOOTH;
            v->lfo[o].pad[0] = v->lfo[o].pad[1] = v->lfo[o].pad[2] = 0u;
            v->lfo[o].phase = 0.0f;
            v->lfo[o].value = 0.0f;
        }
    }
    s->bias[0] = s->bias[1] = s->bias[2] = s->bias[3] = 64u;
    s->arpon = 0u;
    s->arprate = 64u;
    s->seqon = 0u;
    s->seqlen = 16u;
    ri_levi_matrix_init(&s->mx);
}

float ri_levi_lfo_rate(uint8_t ui) {
    /* 0.01 Hz floor (100 s/cycle, spec) to 30 Hz ceiling, exp-mapped. */
    float f = 0.01f * ri_pow2(((float)ui / 127.0f) * 11.5500f);
    if (f < 0.01f)
        f = 0.01f;
    if (f > 30.0f)
        f = 30.0f;
    return f;
}

float ri_levi_lfo_step(struct RILeviLFO *l, float sr) {
    float v;
    if (!l || !(sr > 0.0f) || !(l->rate > 0.0f))
        return 0.0f;
    l->phase += l->rate / sr;
    if (l->phase >= 1.0f)
        l->phase -= 1.0f;
    if (l->shape == RI_LEVI_LFO_STEPS)
        v = l->phase < 1.0f / 3.0f ? -1.0f : l->phase < 2.0f / 3.0f ? 0.0f : 1.0f;
    else
        v = ri_sin(2.0f * 3.14159265f * l->phase);
    l->value = v;
    return v;
}

int levi_trigger(struct RILeviSet *s, uint32_t voice, uint8_t note) {
    struct RILeviVoice *v;
    float f;
    uint32_t o, b;
    if (!s || voice >= RI_LEVI_NVOICES || note > 127u)
        return 2;
    v = &s->v[voice];
    f = note_hz(note);
    v->active = 1u;
    v->note = note;
    for (b = 0u; b < 2u; b++) {
        uint8_t *live = b ? v->liveB : v->live;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (live[o]) {
                const struct RILeviOp *op = &v->op[o];
                /* Keytrack 100 % keeps the v1 note law exactly; others
                 * scale the distance from C4 (manual p. 38). */
                float base = op->kt == 1.0f ? f
                    : note_hz(60u) * ri_pow2((((float)note - 60.0f) / 12.0f) * op->kt);
                float fo = op->pmode == 2u ? op->hz
                    : op->pmode == 0u ? base * op->pitchmul : base * op->ratio;
                op_state_reset(&v->st[b][o], fo);
                v->st[b][o].phase = op->phase0;
            }
    }
    v->lp1 = 0.0f;
    v->lp2 = 0.0f;
    v->lp3 = 0.0f;
    v->lp4 = 0.0f;
    for (o = 0u; o < RI_LEVI_NLFO; o++) {
        v->lfo[o].phase = 0.0f;
        v->lfo[o].value = 0.0f;
    }
    return 0;
}

void levi_release(struct RILeviSet *s, uint32_t voice) {
    struct RILeviVoice *v;
    uint32_t o, b;
    if (!s || voice >= RI_LEVI_NVOICES)
        return;
    v = &s->v[voice];
    for (b = 0u; b < 2u; b++) {
        uint8_t *live = b ? v->liveB : v->live;
        for (o = 0u; o < RI_LEVI_NOPS; o++) {
            if (live[o] && v->st[b][o].env.stage != RI_LEVI_SEG_IDLE) {
                struct RILeviEnv *e = &v->st[b][o].env;
                if (e->freerun && e->stage < RI_LEVI_SEG_S) {
                    e->relpend = 1u;              /* runs to sustain first */
                    continue;
                }
                e->stage = RI_LEVI_SEG_R;
                e->stage_t = 0.0f;
                e->seg_from = e->value;
            }
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
        if (!(value >= 0.0f && value <= (float)(RI_LEVI_NMODES - 1u)))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->op[o].mode = (uint8_t)value;
        return 0;
    case RI_LEVI_RATIO:
        if (!(value >= 0.25f && value <= 64.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o] && v->feeds[o] != 0u)
                v->op[o].ratio = value;
        return 0;
    case RI_LEVI_FTYPE:
        if (value != (float)RI_LEVI_FTYPE_LP && value != (float)RI_LEVI_FTYPE_HP &&
            value != (float)RI_LEVI_FTYPE_BP && value != (float)RI_LEVI_FTYPE_NOTCH)
            return 2;
        v->ftype = (uint8_t)value;
        return 0;
    case RI_LEVI_DRIVE:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        v->drive = value;
        return 0;
    case RI_LEVI_CUTOFF2:
        if (!(value >= 40.0f && value <= 18000.0f))
            return 2;
        v->cutoff2 = value;
        return 0;
    case RI_LEVI_RESO2:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        v->reso2 = value;
        return 0;
    case RI_LEVI_ATTACK:
    case RI_LEVI_DECAY:
    case RI_LEVI_RELEASE: {
        uint32_t tt = id == RI_LEVI_ATTACK ? RI_LEVI_SEG_A
            : id == RI_LEVI_DECAY ? RI_LEVI_SEG_D2 : RI_LEVI_SEG_R;
        if (!(value >= 0.001f && value <= 2.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->st[0][o].env.times[tt] = v->st[1][o].env.times[tt] = value;
        return 0;
    }
    case RI_LEVI_SUSTAIN:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->st[0][o].env.sustain = v->st[1][o].env.sustain = value;
        return 0;
    case RI_LEVI_LOOP:
        if (value != 0.0f && value != 1.0f)
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->st[0][o].env.loop = v->st[1][o].env.loop = (uint8_t)value;
        return 0;
    default:
        return 2;
    }
}

/* Resonant multimode (clean-room 2-pole Chamberlin SVF + driven
 * 24 dB LP cascade): stage 1 selects the LP/HP/BP/notch tap, drive
 * saturates pre-filter, stage 2 re-filters the tap at the same tuning
 * (hardware digital-into-analog path, own topology). Cutoff/reso/drive
 * modulate per render; states flushed (denormal-safe). f is clamped to
 * 1.0 (Dell 2026-09-28: the 1.8 ceiling admitted tunings past the
 * stability limit — inf/NaN ~300 samples after trigger, latched). */
static float lp_step(struct RILeviVoice *v, float x, float sr,
    float cutoff, float reso, float drive) {
    float f, q, hp, bp, lp, tap, k, xd, hp2, bp2, lp2;
    if (!(sr > 0.0f))
        return 0.0f;
    f = 2.0f * ri_sin(3.14159265f * cutoff / sr);
    if (f > 1.0f)
        f = 1.0f;
    if (f < 0.02f)
        f = 0.02f;
    q = 1.0f - reso * 0.85f;
    if (q < 0.05f)
        q = 0.05f;
    k = drive < 0.0f ? 0.0f : drive > 1.0f ? 1.0f : drive;
    xd = x * (1.0f + 4.0f * k) / (1.0f + 4.0f * k * (x < 0.0f ? -x : x));
    hp = xd - v->lp1 * q - v->lp2;
    bp = v->lp1 + f * hp;
    lp = v->lp2 + f * bp;
    v->lp1 = ftz(bp);
    v->lp2 = ftz(lp);
    if (v->ftype == RI_LEVI_FTYPE_HP)
        tap = hp;
    else if (v->ftype == RI_LEVI_FTYPE_BP)
        tap = bp;
    else if (v->ftype == RI_LEVI_FTYPE_NOTCH)
        tap = lp + hp;
    else
        tap = lp;
    {
        float f2 = 2.0f * ri_sin(3.14159265f * v->cutoff2 / sr);
        float q2 = 1.0f - v->reso2 * 0.85f;
        if (f2 > 1.0f)
            f2 = 1.0f;
        if (f2 < 0.02f)
            f2 = 0.02f;
        if (q2 < 0.05f)
            q2 = 0.05f;
        hp2 = tap - v->lp3 * q2 - v->lp4;
        bp2 = v->lp3 + f2 * hp2;
    }
    lp2 = v->lp4 + f * bp2;
    v->lp3 = ftz(bp2);
    v->lp4 = ftz(lp2);
    return lp2;
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
    case (RI_CTL_LEVI_ALGO & 0xFFu): /* ALGO: preset 0..63 */
        return levi_set_algo(s, voice, val < RI_LEVI_ALGO_N ? val : RI_LEVI_ALGO_N - 1u);
    case (RI_CTL_LEVI_ALGOB & 0xFFu): /* ALGOB */ {
        uint32_t a = val < RI_LEVI_ALGO_N ? val : RI_LEVI_ALGO_N - 1u;
        struct RILeviVoice *v;
        if (a >= RI_LEVI_ALGO_N)
            return 2;
        v = &s->v[voice];
        bank_preset(v, 1u, a);
        v->algoB = (uint8_t)a;
        return 0;
    }
    case (RI_CTL_LEVI_MORPH & 0xFFu): /* MORPH */
        return levi_set_morph(s, voice, s->v[voice].algoB, val > 100u ? 100u : val);
    case (RI_CTL_LEVI_OPMODE & 0xFFu): /* OPMODE */ {
        uint32_t op = (uint32_t)val >> 4u, mode = (uint32_t)val & 0xFu;
        if (op >= RI_LEVI_NOPS || mode >= RI_LEVI_NMODES)
            return 2;
        return levi_set_op_mode(s, voice, op, mode);
    }
    case (RI_CTL_LEVI_FTYPE & 0xFFu): /* FTYPE */
        return levi_set_param(s, voice, RI_LEVI_FTYPE,
            val <= 3u ? (float)val : (float)(val >> 5));
    case (RI_CTL_LEVI_DRIVE & 0xFFu): /* DRIVE */
        return levi_set_param(s, voice, RI_LEVI_DRIVE, (float)val / 127.0f);
    case (RI_CTL_LEVI_CUTOFF2 & 0xFFu): /* CUTOFF2 */
        f = 40.0f * ri_pow2(((float)val / 127.0f) * 8.5f);
        if (f > 18000.0f)
            f = 18000.0f;
        return levi_set_param(s, voice, RI_LEVI_CUTOFF2, f);
    case (RI_CTL_LEVI_RESO2 & 0xFFu): /* RESO2 */
        return levi_set_param(s, voice, RI_LEVI_RESO2, (float)val / 127.0f);
    case (RI_CTL_LEVI_ATTACK & 0xFFu): /* ATTACK */
    case (RI_CTL_LEVI_DECAY & 0xFFu): /* DECAY */
    case (RI_CTL_LEVI_RELEASE & 0xFFu): { /* RELEASE */
        float t = 0.001f * ri_pow2(((float)val / 127.0f) * 11.0f);
        uint32_t pid = id == (RI_CTL_LEVI_ATTACK & 0xFFu) ? RI_LEVI_ATTACK
            : id == (RI_CTL_LEVI_DECAY & 0xFFu) ? RI_LEVI_DECAY : RI_LEVI_RELEASE;
        if (t > 2.0f)
            t = 2.0f;
        return levi_set_param(s, voice, pid, t);
    }
    case (RI_CTL_LEVI_SUSTAIN & 0xFFu): /* SUSTAIN */
        return levi_set_param(s, voice, RI_LEVI_SUSTAIN, (float)val / 127.0f);
    case (RI_CTL_LEVI_LOOP & 0xFFu): /* LOOP */
        return levi_set_param(s, voice, RI_LEVI_LOOP, val != 0u ? 1.0f : 0.0f);
    case (RI_CTL_LEVI_ARPON & 0xFFu): /* ARPON (device arp gate) */
        s->arpon = val != 0u ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_ARPRATE & 0xFFu): /* ARPRATE (device arp rate) */
        s->arprate = val;
        return 0;
    case (RI_CTL_LEVI_SEQON & 0xFFu): /* SEQON (device seq gate) */
        s->seqon = val != 0u ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_SEQLEN & 0xFFu): /* SEQLEN (device seq length) */
        s->seqlen = val < 1u ? 1u : val > 16u ? 16u : val;
        return 0;
    case (RI_CTL_LEVI_ROUTE0 & 0xFFu): case (RI_CTL_LEVI_ROUTE1 & 0xFFu):
    case (RI_CTL_LEVI_ROUTE2 & 0xFFu): case (RI_CTL_LEVI_ROUTE3 & 0xFFu):
    case (RI_CTL_LEVI_ROUTE4 & 0xFFu): case (RI_CTL_LEVI_ROUTE5 & 0xFFu):
    case (RI_CTL_LEVI_ROUTE6 & 0xFFu): case (RI_CTL_LEVI_ROUTE7 & 0xFFu):
        /* Matrix slot gates (v2 feature 4c): program stays put. */
        s->mx.slot[id - (RI_CTL_LEVI_ROUTE0 & 0xFFu)].on = val != 0u ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_LFO0RATE & 0xFFu): case (RI_CTL_LEVI_LFO1RATE & 0xFFu):
    case (RI_CTL_LEVI_LFO2RATE & 0xFFu): case (RI_CTL_LEVI_LFO3RATE & 0xFFu):
    case (RI_CTL_LEVI_LFO4RATE & 0xFFu):
        /* LFO rate (v2 feature 4d, automation-only: no panel knob yet,
         * 303-VOLUME precedent). */
        s->v[voice].lfo[id - (RI_CTL_LEVI_LFO0RATE & 0xFFu)].rate =
            ri_levi_lfo_rate(val > 127u ? 127u : val);
        return 0;
    case (RI_CTL_LEVI_AMODE & 0xFFu):
        return levi_set_amode(s, voice, val > 2u ? 2u : val);
    case (RI_CTL_LEVI_SLOT0 & 0xFFu): case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 1u:
    case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 2u: case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 3u:
    case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 4u: case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 5u:
    case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 6u: case (RI_CTL_LEVI_SLOT0 & 0xFFu) + 7u:
        return levi_set_slot(s, voice, id - (RI_CTL_LEVI_SLOT0 & 0xFFu),
            val > RI_LEVI_SLOT_OFF ? RI_LEVI_SLOT_OFF : val);
    case (RI_CTL_LEVI_MPOS & 0xFFu): {
        /* 0..127 spans the active slots (7-bit key; manual: 100 steps per
         * slot pair, E0 resolution). */
        uint32_t n = levi_morph_slots(s, voice);
        return levi_set_mpos(s, voice, n > 1u ? ((uint32_t)val * (n - 1u) * 100u + 63u) / 127u : 0u);
    }
    case (RI_CTL_LEVI_SOLO & 0xFFu):
        s->v[voice].solo = val > 8u ? 0u : val;
        return 0;
    case (RI_CTL_LEVI_MUTELO & 0xFFu):
        s->v[voice].mute = (uint8_t)((s->v[voice].mute & 0x80u) | (val & 0x7Fu));
        return 0;
    case (RI_CTL_LEVI_MUTEHI & 0xFFu):
        s->v[voice].mute = (uint8_t)((s->v[voice].mute & 0x7Fu) | (val ? 0x80u : 0u));
        return 0;
    case (RI_CTL_LEVI_BIAS_ENVL & 0xFFu): case (RI_CTL_LEVI_BIAS_ATK & 0xFFu):
    case (RI_CTL_LEVI_BIAS_DEC & 0xFFu): case (RI_CTL_LEVI_BIAS_REL & 0xFFu): {
        /* Osc Env Level & Bias (manual p. 54): 64 = none. Level bias adds
         * up to +/-1 to every env level; time bias scales +/-4 octaves. */
        uint32_t bi = id - (RI_CTL_LEVI_BIAS_ENVL & 0xFFu);
        struct RILeviVoice *v = &s->v[voice];
        uint8_t b = val > 127u ? 127u : val;
        s->bias[bi] = b;
        if (bi == 0u)
            v->bias_envl = b == 64u ? 0.0f : ((float)b - 64.0f) / 64.0f;
        else
            v->bias_t[bi - 1u] = b == 64u ? 1.0f : ri_pow2(((float)b - 64.0f) / 16.0f);
        return 0;
    }
    case (RI_CTL_LEVI_LFO0SHAPE & 0xFFu): case (RI_CTL_LEVI_LFO1SHAPE & 0xFFu):
    case (RI_CTL_LEVI_LFO2SHAPE & 0xFFu): case (RI_CTL_LEVI_LFO3SHAPE & 0xFFu):
    case (RI_CTL_LEVI_LFO4SHAPE & 0xFFu):
        s->v[voice].lfo[id - (RI_CTL_LEVI_LFO0SHAPE & 0xFFu)].shape =
            val != 0u ? RI_LEVI_LFO_STEPS : RI_LEVI_LFO_SMOOTH;
        return 0;
    default:
        return 2;
    }
}

/* Phase warp of a carrier by one non-FM/PM modulator (manual pp. 45-48,
 * own definitions): the modulator's pitch sets how many processes per
 * carrier cycle (n), its level the amount (a 0..1). Pure. */
static float mod_warp(uint32_t mode, float ph, float n, float a) {
    const float K = 0.1591549f;               /* 1 / (2 pi) */
    float x = ph * n, cyc = (float)(int)x, s = x - cyc, sw;
    switch (mode) {
    case RI_LEVI_PWM: {                       /* pulse width: narrow the first half */
        float w = 0.5f - 0.49f * a;
        sw = s < w ? 0.5f * s / w : 0.5f + 0.5f * (s - w) / (1.0f - w);
        break;
    }
    case RI_LEVI_SYNC:                        /* hard sync: slave restarts every process */
        sw = frac1(s * (1.0f + 7.0f * a));
        break;
    case RI_LEVI_PDSAW:                       /* saw trajectory */
        sw = s - a * K * ri_sin(6.2831853f * s);
        break;
    case RI_LEVI_PDSQ:                        /* binary (two-rate) trajectory */
        sw = s - a * K * ri_sin(12.5663706f * s);
        break;
    default: {                                /* PD saw pulse: saw, then binary */
        float t = s - a * K * ri_sin(6.2831853f * s);
        sw = t - a * K * ri_sin(12.5663706f * t);
        break;
    }
    }
    return frac1((cyc + sw) / (n > 0.0f ? n : 1.0f));
}

/* One morph-bank pass: carriers under this bank's routing into mix.
 * Returns the carrier mix; ORs envelope activity into *any_on.
 * Modes belong to the MODULATOR (manual p. 35: a mode changes how an
 * oscillator affects the ones it modulates): Freq Mod feeders move the
 * carrier's frequency, Phase Mod feeders its phase, PW/Sync/PD feeders
 * warp its phase trajectory. With every mode Freq Mod and the default
 * levels this is the v1 render bit for bit. */
static float voice_pass(struct RILeviVoice *v, uint32_t bank, float sr,
    int *any_on) {
    float opout[RI_LEVI_NOPS] = { 0.0f }, mix = 0.0f;
    const uint8_t *fd = bank ? v->feedsB : v->feeds;
    const uint8_t *ord = bank ? v->orderB : v->order;
    const uint8_t *live = bank ? v->liveB : v->live;
    uint32_t k, j;
    for (k = 0u; k < RI_LEVI_NOPS; k++) {
        uint32_t i = ord[k];
        struct RILeviOp *p = &v->op[i];
        struct RILeviOpState *o = &v->st[bank][i];
        float fm, pm, ph, osc, amp;
        int on, warped = 0;
        if (i >= RI_LEVI_NOPS || !live[i]) {
            opout[k & (RI_LEVI_NOPS - 1u)] = 0.0f;
            continue;
        }
        on = env_tick_b(&o->env, sr, v->bias_t);
        *any_on |= on;
        if (!on) {
            opout[i] = 0.0f;
            o->amp = 0.0f;
            continue;
        }
        /* Feeders render first by topological order, so their slots
         * are filled (zero-init covers custom edits mid-flight). */
        fm = 0.0f;
        pm = 0.0f;
        for (j = 0u; j < RI_LEVI_NOPS; j++)
            if ((fd[j] >> i) & 1u) {
                uint32_t md = v->op[j].mode;
                if (md == RI_LEVI_FM)
                    fm += opout[j];
                else if (md == RI_LEVI_PM)
                    pm += opout[j];
                else
                    warped = 1;
            }
        o->phase += (o->freq + o->freq * (RI_LEVI_MOD_DEPTH * fm)) / sr;
        if (o->phase >= 1.0f)
            o->phase -= 1.0f;
        if (o->phase < 0.0f)
            o->phase += 1.0f;
        ph = o->phase;
        if (pm != 0.0f)
            ph = frac1(ph + RI_LEVI_MOD_DEPTH * pm * 0.1591549f);
        if (p->fb > 0.0f && (p->mode == RI_LEVI_FM || p->mode == RI_LEVI_PM))
            ph = frac1(ph + p->fb * 1.2f * o->last);   /* self feedback (FM/PM only) */
        if (warped)
            for (j = 0u; j < RI_LEVI_NOPS; j++)
                if (((fd[j] >> i) & 1u) && v->op[j].mode > RI_LEVI_PM && o->freq > 0.0f) {
                    float n = v->st[bank][j].freq / o->freq;
                    n = n < 0.125f ? 0.125f : n > 64.0f ? 64.0f : n;
                    ph = mod_warp(v->op[j].mode, ph, n, v->st[bank][j].amp);
                }
        osc = ri_levi_wave(p->wave, ph, o->freq / sr);
        if (p->invert)
            osc = -osc;
        amp = p->init + (p->envl + v->bias_envl) * env_out(&o->env);
        if (amp < 0.0f)
            amp = 0.0f;
        if (amp > 1.0f)
            amp = 1.0f;
        o->ps = fm;
        o->last = osc;
        o->amp = amp;
        opout[i] = osc * p->level * amp;
        /* Carriers (feed nothing) and Direct Out reach the mix, Mute drops
         * an op; Solo auditions one op alone, modulator or not (p. 59). */
        if (v->solo ? v->solo == i + 1u : (fd[i] == 0u || p->direct) && !((v->mute >> i) & 1u))
            mix += opout[i];
    }
    return mix;
}

float levi_voice_render(struct RILeviVoice *v, const struct RILeviMatrix *mx,
    float sr) {
    float mixA, mixB, mix, out;
    /* Effective params: base copies when the matrix is absent, so the
     * legacy path below stays bit-identical (x*1.0 and x+0.0 are exact;
     * morph keeps its integer branches through emorph). */
    float ecut, ereso, edrive, emorph, eoplevel, evlevel;
    int any_on = 0;
    if (!v || !v->active || !(sr > 0.0f))
        return 0.0f;
    mixA = voice_pass(v, 0u, sr, &any_on);
    mixB = voice_pass(v, 1u, sr, &any_on);
    ecut = v->cutoff;
    ereso = v->reso;
    edrive = v->drive;
    emorph = (float)v->morph;
    eoplevel = 1.0f;
    evlevel = 1.0f;
    if (mx) {
        float openv[RI_LEVI_NOPS], dst[RI_LEVI_MD_N], lfo5[RI_LEVI_NLFO];
        uint32_t o;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            openv[o] = v->st[0][o].env.value;
        for (o = 0u; o < RI_LEVI_NLFO; o++)
            lfo5[o] = ri_levi_lfo_step(&v->lfo[o], sr);
        if (ri_levi_matrix_eval(mx, openv, lfo5, v->note, dst) == 0) {
            /* Own scaling laws (clean-room): cutoff ±2 octaves
             * full-scale, reso/drive linear, morph in blend units,
             * oplevel pre-filter (drives the timbre), vlevel post. */
            ecut = v->cutoff * ri_pow2(dst[RI_LEVI_MD_CUTOFF] * 2.0f);
            if (ecut < 40.0f)
                ecut = 40.0f;
            if (ecut > 18000.0f)
                ecut = 18000.0f;
            ereso = v->reso + dst[RI_LEVI_MD_RESO] * 0.5f;
            if (ereso < 0.0f)
                ereso = 0.0f;
            if (ereso > 1.0f)
                ereso = 1.0f;
            edrive = v->drive + dst[RI_LEVI_MD_DRIVE];
            if (edrive < 0.0f)
                edrive = 0.0f;
            if (edrive > 1.0f)
                edrive = 1.0f;
            emorph = (float)v->morph + dst[RI_LEVI_MD_MORPH] * 100.0f;
            if (emorph < 0.0f)
                emorph = 0.0f;
            if (emorph > 100.0f)
                emorph = 100.0f;
            eoplevel = 1.0f + dst[RI_LEVI_MD_OPLEVEL];
            if (eoplevel < 0.0f)
                eoplevel = 0.0f;
            if (eoplevel > 2.0f)
                eoplevel = 2.0f;
            evlevel = 1.0f + dst[RI_LEVI_MD_VLEVEL];
            if (evlevel < 0.0f)
                evlevel = 0.0f;
            if (evlevel > 2.0f)
                evlevel = 2.0f;
        }
    }
    /* Exact endpoints (bit-identity with no-morph / pure-B voices);
     * the slide blends between them. */
    if (emorph <= 0.0f)
        mix = mixA;
    else if (emorph >= 100.0f)
        mix = mixB;
    else
        mix = mixA + (mixB - mixA) * (emorph / 100.0f);
    mix *= eoplevel;
    if (!any_on) {
        v->active = 0u;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        v->lp3 = 0.0f;
        v->lp4 = 0.0f;
        return 0.0f;
    }
    out = lp_step(v, mix, sr, ecut, ereso, edrive);
    if (!(out > -1e20f && out < 1e20f)) {
        /* Non-finite latch guard (Dell 2026-09-28): a poisoned filter
         * state must self-heal to silence, never mute the mix. */
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        v->lp3 = 0.0f;
        v->lp4 = 0.0f;
        return 0.0f;
    }
    return out * v->level * evlevel;
}

void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr) {
    uint32_t i, v;
    if (!s || !out)
        return;
    for (i = 0u; i < n; i++) {
        float m = 0.0f;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], &s->mx, sr);
        out[i] = m;
    }
}

int levi_set_algo(struct RILeviSet *s, uint32_t voice, uint32_t algo) {
    if (!s || voice >= RI_LEVI_NVOICES || algo >= RI_LEVI_ALGO_N)
        return 2;
    voice_preset(&s->v[voice], algo);
    s->v[voice].slot[0] = (uint8_t)algo;
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
    uint8_t save;
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return 2;
    if (src < -1 || src >= (int)RI_LEVI_NOPS || src == (int)op)
        return 2;
    v = &s->v[voice];
    save = v->feeds[op];
    v->feeds[op] = src < 0 ? 0u : (uint8_t)(1u << src);
    /* Acyclic only: the target must not reach op back. */
    if (src >= 0 && reaches(v->feeds, (uint32_t)src, op)) {
        v->feeds[op] = save;
        return 2;
    }
    v->live[op] = 1u;
    v->algo = RI_LEVI_ALGO_CUSTOM;
    order_compute(v->feeds, v->order);
    return 0;
}

int levi_route_get(const struct RILeviSet *s, uint32_t voice,
    uint32_t op) {
    uint32_t t;
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return -2;
    for (t = 0u; t < RI_LEVI_NOPS; t++)
        if ((s->v[voice].feeds[op] >> t) & 1u)
            return (int)t;
    return -1;
}

int levi_set_feeds(struct RILeviSet *s, uint32_t voice, uint32_t op, uint32_t mask) {
    struct RILeviVoice *v;
    uint8_t save;
    uint32_t t;
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS || mask > 0xFFu || ((mask >> op) & 1u))
        return 2;
    v = &s->v[voice];
    save = v->cfeeds[op];
    v->cfeeds[op] = (uint8_t)mask;
    for (t = 0u; t < RI_LEVI_NOPS; t++)
        if (((mask >> t) & 1u) && reaches(v->cfeeds, t, op)) {
            v->cfeeds[op] = save;
            return 2;
        }
    v->amode = RI_LEVI_AMODE_CUSTOM;
    bank_load(v, 0u, RI_LEVI_ALGO_CUSTOM);
    v->algo = RI_LEVI_ALGO_CUSTOM;
    v->morph = 0u;
    return 0;
}

int levi_set_amode(struct RILeviSet *s, uint32_t voice, uint32_t mode) {
    struct RILeviVoice *v;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES || mode > RI_LEVI_AMODE_CUSTOM)
        return 2;
    v = &s->v[voice];
    if (mode == RI_LEVI_AMODE_CUSTOM && v->amode != RI_LEVI_AMODE_CUSTOM)
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            v->cfeeds[o] = v->feeds[o];         /* start from what sounds now */
    v->amode = (uint8_t)mode;
    if (mode == RI_LEVI_AMODE_CUSTOM) {
        bank_load(v, 0u, RI_LEVI_ALGO_CUSTOM);
        v->algo = RI_LEVI_ALGO_CUSTOM;
        v->morph = 0u;
    } else if (mode == RI_LEVI_AMODE_MORPH) {
        v->mslotA = 0u;
        morph_apply(v);
    } else {
        voice_preset(v, v->slot[0] < RI_LEVI_ALGO_N ? v->slot[0] : 0u);
        v->morph = 0u;
    }
    return 0;
}

int levi_set_slot(struct RILeviSet *s, uint32_t voice, uint32_t slot, uint32_t val) {
    struct RILeviVoice *v;
    if (!s || voice >= RI_LEVI_NVOICES || slot >= RI_LEVI_NSLOTS || val > RI_LEVI_SLOT_OFF)
        return 2;
    if (slot == 0u && val >= RI_LEVI_ALGO_N)
        return 2;                                /* Algo 1 is never OFF or Silence */
    v = &s->v[voice];
    v->slot[slot] = (uint8_t)val;
    if (v->amode == RI_LEVI_AMODE_MORPH)
        morph_apply(v);
    else if (slot == 0u && v->amode == RI_LEVI_AMODE_SINGLE)
        voice_preset(v, val);
    return 0;
}

int levi_set_mpos(struct RILeviSet *s, uint32_t voice, uint32_t pos) {
    struct RILeviVoice *v;
    if (!s || voice >= RI_LEVI_NVOICES || pos > 700u)
        return 2;
    v = &s->v[voice];
    v->mpos = (uint16_t)pos;
    if (v->amode == RI_LEVI_AMODE_MORPH)
        morph_apply(v);
    return 0;
}

uint32_t levi_morph_slots(const struct RILeviSet *s, uint32_t voice) {
    uint8_t list[RI_LEVI_NSLOTS];
    if (!s || voice >= RI_LEVI_NVOICES)
        return 0u;
    return morph_list(&s->v[voice], list);
}

int levi_set_op_mode(struct RILeviSet *s, uint32_t voice, uint32_t op,
    uint32_t mode) {
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return 2;
    if (mode >= RI_LEVI_NMODES)
        return 2;
    s->v[voice].op[op].mode = (uint8_t)mode;
    return 0;
}

int levi_set_morph(struct RILeviSet *s, uint32_t voice, uint32_t algoB,
    uint32_t pos) {
    struct RILeviVoice *v;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES || algoB >= RI_LEVI_ALGO_N)
        return 2;
    if (pos > 100u)
        return 2;
    v = &s->v[voice];
    bank_preset(v, 1u, algoB);
    v->algoB = (uint8_t)algoB;
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        v->st[1][o] = v->st[0][o];
    v->morph = (uint8_t)pos;
    return 0;
}

int levi_morph_get(const struct RILeviSet *s, uint32_t voice) {
    if (!s || voice >= RI_LEVI_NVOICES)
        return -1;
    return (int)s->v[voice].morph;
}
