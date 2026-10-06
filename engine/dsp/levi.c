/* levi.c — Levi FM voice bank bodies (owner 2026-09-28, v1 slice 3a).
 * 2-op FM/PM + per-operator DAHDSR + resonant lowpass. Render-contract
 * safe: bounded, no allocation, no IO; denormal-safe (ftz states, short
 * envelopes end exact).
 */
#include "engine/dsp/levi.h"

#include <string.h>
#include "engine/dsp/levi_matrix.h"
#include "engine/dsp/kernels.h"
#ifdef RI_LEVI_OPT_FLOATK
/* P3 O3 (default off): levi render calls use the single-precision kernels.
 * Never combined with RI_LEVI_PROFILE (both rename the same symbols). */
#ifdef RI_LEVI_PROFILE
#error "RI_LEVI_OPT_FLOATK and RI_LEVI_PROFILE are exclusive"
#endif
#define ri_sin ri_sin_f
#define ri_pow2 ri_pow2_f
#endif

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
    float dt, span, from, to, seff;
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
            if (e->susmod != 0.0f) {          /* matrix-moved sustain level (P5b) */
                seff = e->sustain + e->susmod;
                e->value = seff < 0.0f ? 0.0f : seff > 1.0f ? 1.0f : seff;
            }
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
        else if (e->stage == RI_LEVI_SEG_H)
            span *= tscale[3];
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
        seff = e->sustain + e->susmod;
        to = seff < 0.0f ? 0.0f : seff > 1.0f ? 1.0f : seff;
        c = e->curve[1];
        break;
    default: /* RI_LEVI_SEG_R */
        to = 0.0f;
        c = e->curve[2];
        break;
    }
    if ((e->cmod[0] | e->cmod[1] | e->cmod[2]) &&        /* matrix-moved curves (P5b) */
        (e->stage == RI_LEVI_SEG_A || e->stage == RI_LEVI_SEG_D2 || e->stage == RI_LEVI_SEG_R)) {
        int k = e->stage == RI_LEVI_SEG_A ? 0 : e->stage == RI_LEVI_SEG_D2 ? 1 : 2;
        c += e->cmod[k];
        c = c < -64 ? -64 : c > 63 ? 63 : c;
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
    float s;
    if (w == 0u)
        return ri_sin(ph * TAU);
    switch (f) {
    case 0u:
        switch (k) {
        case 1u: return w_tri(ph);
        case 2u: return 0.5f * (w_tri(ph) + w_saw(ph, dt));
        case 3u: return w_saw(ph, dt);
        case 4u: return w_pulse(ph, dt, 0.5f);
        case 5u: return ph < 0.5f ? 2.0f * ri_sin(ph * TAU) - 1.0f : -1.0f;
        case 6u: s = ri_sin(ph * TAU); return 2.0f * (s < 0.0f ? -s : s) - 1.0f;
        case 7u:
            if (ph < 0.25f || (ph >= 0.5f && ph < 0.75f)) {
                float q = ri_sin(ph * TAU);
                return (q < 0.0f ? -q : q) * 2.0f - 1.0f;
            }
            return -1.0f;
        case 8u: s = ri_sin(ph * TAU); return s * s * s;
        case 9u: s = ri_sin(ph * TAU);
            return 0.6f * s + 0.4f * ri_sin(2.0f * ph * TAU);
        case 10u: s = ri_sin(ph * TAU);
            return 0.5f * s + 0.3f * ri_sin(2.0f * ph * TAU) + 0.2f * ri_sin(3.0f * ph * TAU);
        case 11u: s = ri_sin(ph * TAU); return ri_tanh(3.0f * s) / ri_tanh(3.0f);
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
        s = ri_sin(ph * TAU);
        return (s + a * ri_sin(h * ph * TAU)) / (1.0f + a);
    }
    case 3u: { /* FOLD: triangle-folded sine, gain 1.2..4.2 */
        float y;
        s = ri_sin(ph * TAU);
        y = frac1(0.25f * s * (1.2f + 0.2f * (float)k) + 0.25f);
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
        s = ri_sin(ph * TAU);
        return s * ri_sin((float)(k + 2u) * ph * TAU);
    default: { /* CHEBY: T_n(sin) mixed with sine, n = 2..17 */
        float t0 = 1.0f, t1, tn;
        s = ri_sin(ph * TAU);
        t1 = tn = s;
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

/* Musical time (fidelity P8a): beats(ui) = ui/127*4 over 0..4 beats. */
static float beats_of(uint8_t ui, float bpm) {
    float b = bpm;
    if (!(b >= 20.0f && b <= 500.0f))
        b = 140.0f;
    return (float)(ui > 127u ? 127u : ui) / 127.0f * 4.0f * 60.0f / b;
}

float ri_levi_beats_time(uint8_t ui, float bpm) {
    return beats_of(ui, bpm);
}

float ri_levi_lfo_sync_hz(uint8_t ui, float bpm) {
    /* One cycle per 4*2^(-ui/127*7) beats (4 beats..1/32). */
    float b = bpm, bpc;
    uint8_t u = ui > 127u ? 127u : ui;
    if (!(b >= 20.0f && b <= 500.0f))
        b = 140.0f;
    bpc = 4.0f / ri_pow2((float)u / 127.0f * 7.0f);
    return b / 60.0f / bpc;
}

int levi_set_tempo(struct RILeviSet *s, float bpm) {
    if (!s)
        return 2;
    if (!(bpm > 0.0f))
        s->tempo_bpm = 140.0f;
    else if (bpm < 20.0f)
        s->tempo_bpm = 20.0f;
    else if (bpm > 500.0f)
        s->tempo_bpm = 500.0f;
    else
        s->tempo_bpm = bpm;
    return 0;
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

/* One envelope UI param (oscillator and mod envelopes share the law). */
static void env_ui_apply(struct RILeviEnv *e, const uint8_t *u, uint32_t p) {
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

/* ---- Modulation envelopes ENV 1-5 (fidelity P5, pp. 71-75) ---- */
int ri_levi_menv_range(uint32_t param, int *lo, int *hi) {
    if (!lo || !hi)
        return 2;
    *lo = 0;
    if (param <= 3u)
        *hi = (int)RI_LEVI_TS_N - 1;
    else if (param == RI_LEVI_ME_LEVEL || param == RI_LEVI_ME_VELCRV)
        *hi = 127;
    else if (param >= RI_LEVI_OP_DELAY && param <= RI_LEVI_OP_VELENV)
        return ri_levi_op_range(param, lo, hi);
    else
        return 2;
    return 0;
}

int ri_levi_menv_default(uint32_t env, uint32_t param) {
    switch (param) {
    case RI_LEVI_ME_TRIG1: return RI_LEVI_TS_NOTE;
    case RI_LEVI_ME_LEVEL: return 127;
    case RI_LEVI_ME_VELCRV: return 64;
    /* ENV 3 is the VCA contour: instant, full sustain, a release a
     * little longer than the oscillator default so the oscillators
     * still shape the tail (E0). */
    case RI_LEVI_OP_ATTACK: return env == 2u ? 0 : ri_levi_op_default(0u, param);
    case RI_LEVI_OP_SUSTAIN: return env == 2u ? 127 : ri_levi_op_default(0u, param);
    case RI_LEVI_OP_RELEASE: return env == 2u ? 34 : ri_levi_op_default(0u, param);
    default:
        return param >= RI_LEVI_OP_DELAY && param <= RI_LEVI_OP_VELENV ? ri_levi_op_default(0u, param) : 0;
    }
}

static void menv_start(struct RILeviEnv *e) {
    e->relpend = 0u;
    e->loopleft = e->loopn == 255u ? 0u : (uint8_t)(e->loopn > 1u ? e->loopn - 1u : 0u);
    e->seg_from = e->value;          /* a retrigger ramps from where it is */
    e->stage = RI_LEVI_SEG_D;
    e->stage_t = 0.0f;
}

static void menv_release(struct RILeviEnv *e) {
    if (e->stage == RI_LEVI_SEG_IDLE)
        return;
    if (e->freerun && e->stage < RI_LEVI_SEG_S) {
        e->relpend = 1u;
        return;
    }
    e->stage = RI_LEVI_SEG_R;
    e->stage_t = 0.0f;
    e->seg_from = e->value;
}

static int menv_has(const struct RILeviVoice *v, uint32_t e, uint32_t src) {
    uint32_t k;
    for (k = 0u; k < 4u; k++)
        if (v->meui[e][RI_LEVI_ME_TRIG1 + k] == src)
            return 1;
    return 0;
}

int levi_set_mx_ui(struct RILeviSet *s, uint32_t slot, uint32_t field, uint8_t val) {
    struct RILeviMxSlot *sl;
    uint32_t src;
    int depth;
    if (!s || slot >= RI_LEVI_MX_NSLOTS || field > 3u)
        return 2;
    sl = &s->mx.slot[slot];
    switch (field) {
    case 0u:
        src = ri_levi_ms_by_ui(val);
        sl->pad[0] = (uint8_t)(src < RI_LEVI_MS_N ? val : 0u);  /* UI truth: 0 = no source */
        if (src < RI_LEVI_MS_N)
            sl->src = (uint8_t)src;
        break;
    case 1u:
        sl->dmod = (uint8_t)(val < RI_LEVI_DM_N ? val : RI_LEVI_DM_NONE);
        if (sl->dpar >= ri_levi_dm_nparam(sl->dmod))
            sl->dpar = 0u;
        break;
    case 2u: {
        uint32_t np = ri_levi_dm_nparam(sl->dmod);
        sl->dpar = (uint8_t)(np && val < np ? val : 0u);
        break;
    }
    default:
        depth = ((int)(val > 127u ? 127u : val) - 64) * 100 / 63;
        sl->depth = (int8_t)(depth < -100 ? -100 : depth);
        break;
    }
    sl->on = sl->pad[0] != 0u && sl->dmod != RI_LEVI_DM_NONE;
    sl->dst = 0xFFu;                            /* module routes leave the v1 table */
    ri_levi_matrix_refresh_empty(&s->mx);
    return 0;
}

int levi_set_mr_ui(struct RILeviSet *s, uint32_t macro, uint32_t route, uint32_t field, uint8_t val) {
    struct RILeviMacroRoute *r;
    int depth;
    if (!s || macro >= RI_LEVI_NMACRO || route >= RI_LEVI_MACRO_NR || field > 3u)
        return 2;
    r = &s->mx.mroute[macro][route];
    switch (field) {
    case 0u:
        r->dmod = (uint8_t)(val < RI_LEVI_DM_N ? val : RI_LEVI_DM_NONE);
        if (r->dpar >= ri_levi_dm_nparam(r->dmod))
            r->dpar = 0u;
        break;
    case 1u: {
        uint32_t np = ri_levi_dm_nparam(r->dmod);
        r->dpar = (uint8_t)(np && val < np ? val : 0u);
        break;
    }
    case 2u:
        depth = ((int)(val > 127u ? 127u : val) - 64) * 100 / 63;
        r->depth = (int8_t)(depth < -100 ? -100 : depth);
        break;
    default:
        r->bval = val > 127u ? 127u : val;
        break;
    }
    ri_levi_matrix_refresh_empty(&s->mx);
    return 0;
}

int levi_set_menv_ui(struct RILeviSet *s, uint32_t voice, uint32_t env, uint32_t param, uint8_t val) {
    struct RILeviVoice *v;
    int lo, hi;
    if (!s || voice >= RI_LEVI_NVOICES || env >= RI_LEVI_NMENV || ri_levi_menv_range(param, &lo, &hi))
        return 2;
    v = &s->v[voice];
    v->meui[env][param] = (uint8_t)(val < lo ? lo : val > hi ? hi : val);
    if (param <= 3u) {
        uint32_t e, k;
        v->melfo = 0u;
        for (e = 0u; e < RI_LEVI_NMENV; e++)
            for (k = 0u; k < 4u; k++)
                if (v->meui[e][k] >= RI_LEVI_TS_LFO1 && v->meui[e][k] < RI_LEVI_TS_LFO1 + RI_LEVI_NLFO)
                    v->melfo = 1u;
    }
    if (param == RI_LEVI_ME_LEVEL)
        v->melevel[env] = (float)v->meui[env][param] / 127.0f;
    else if (param == RI_LEVI_ME_VELCRV) {         /* P9b: bipolar curve */
        v->mvelcrv[env] = ((float)v->meui[env][param] - 64.0f) / 63.0f;
        if (v->mvelcrv[env] < -1.0f)
            v->mvelcrv[env] = -1.0f;
        else if (v->mvelcrv[env] > 1.0f)
            v->mvelcrv[env] = 1.0f;
    } else if (param >= RI_LEVI_OP_DELAY && param <= RI_LEVI_OP_FREERUN)
        env_ui_apply(&v->menv[env], v->meui[env], param);
    return 0;
}

float levi_menv_value(const struct RILeviVoice *v, uint32_t env) {
    if (!v || env >= RI_LEVI_NMENV)
        return 0.0f;
    {
        float out;
        if (v->melmod[env] != 0.0f) {
            float l = v->melevel[env] + v->melmod[env];
            out = env_out(&v->menv[env]) * (l < 0.0f ? 0.0f : l > 1.0f ? 1.0f : l);
        } else
            out = env_out(&v->menv[env]) * v->melevel[env];
        /* VEL CRV (P9b): a velocity curve on the envelope value, read the
         * same way by the ENV matrix source (which calls this). */
        if (v->mvelcrv[env] != 0.0f) {
            float g = 1.0f + v->mvelcrv[env] * (2.0f * v->vel01 - 1.0f);
            out *= g < 0.0f ? 0.0f : g;
        }
        return out;
    }
}

/* Recompute an op's derived values after a UI change; env params go
 * into both bank states of the voice. */
static int reaches(const uint8_t *feeds, uint32_t from, uint32_t to);
static void bank_load(struct RILeviVoice *v, uint32_t bank, uint32_t algo);
static void filt_clear(struct RILeviVoice *v);
static void lfo_init(struct RILeviLFO *l, uint32_t seed);
static void kt_update(struct RILeviVoice *v);

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
    case RI_LEVI_OP_VELENV: op->venv = (float)u[p] / 127.0f; break;   /* P9b */
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
        for (b = 0u; b < 2u; b++)
            env_ui_apply(&v->st[b][o].env, u, p);
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
/* Analog ladder feedback at full resonance (tuned: self-oscillation
 * sets in near 110/128, manual p. 66). */
#define RI_LEVI_AF_KMAX 4.65f

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
    if (bank) {
        /* P2 C6: a bank with no live operators renders nothing (voice_pass
         * only zeroes its local opout), so the render may skip the call. */
        uint32_t e = 0u;
        for (i = 0u; i < RI_LEVI_NOPS; i++)
            e |= live[i];
        v->liveB_empty = e == 0u ? 1u : 0u;
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
    memset(s, 0, sizeof *s);   /* padding included: whole-struct memcmp stays clean */
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        struct RILeviVoice *v = &s->v[i];
        uint32_t b;
        v->active = 0u;
        v->note = 0u;
        v->nvel = 127u;     /* performance signals (P9a): full velocity */
        v->nveloff = 127u;
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
            op->venv = 0.0f;
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
                v->st[b][o].env.cmod[0] = v->st[b][o].env.cmod[1] = v->st[b][o].env.cmod[2] = 0;
                v->st[b][o].env.susmod = 0.0f;
                v->st[b][o].env.seg_from = 0.0f;
                v->st[b][o].last = 0.0f;
                v->st[b][o].amp = 0.0f;
            }
        }
        voice_preset(v, RI_LEVI_ALGO_DUO);
        v->bias_envl = 0.0f;
        v->bias_t[0] = v->bias_t[1] = v->bias_t[2] = v->bias_t[3] = 1.0f;
        memset(v->opm, 0, sizeof v->opm);
        memset(v->mem, 0, sizeof v->mem);
        v->opm_on = v->mem_on = 0u;
        memset(v->melmod, 0, sizeof v->melmod);
        v->padm2[0] = v->padm2[1] = 0u;
        v->cutoff = RI_LEVI_DEF_CUTOFF;
        v->reso = RI_LEVI_DEF_RESO;
        v->zgain = 1.0f;                    /* zone layer gain (P9c) */
        v->level = 1.0f;
        v->drive = 0.0f;
        v->cutoff2 = RI_LEVI_DEF_CUTOFF;
        v->reso2 = RI_LEVI_DEF_RESO;
        v->dtype = RI_LEVI_DF_LP_12;
        v->dmorph = 0u;
        v->dpost = 0u;
        v->vorder = 0u;
        v->dkt = 0.0f;                  /* digital keytrack 0 %, analog 100 % (pp. 64, 67) */
        v->akt = 1.0f;
        v->dlfo = v->alfo = v->vlfo = 0.0f;
        v->dlevel = v->osclvl = v->vcalvl = v->patchlvl = 1.0f;
        kt_update(v);
        filt_clear(v);
        for (o = 0u; o < RI_LEVI_NLFO; o++)
            lfo_init(&v->lfo[o], i * RI_LEVI_NLFO + o + 1u);
        for (o = 0u; o < RI_LEVI_NMENV; o++) {
            uint32_t pp;
            memset(&v->menv[o], 0, sizeof v->menv[o]);
            env_reset(&v->menv[o]);
            v->menv[o].stage = RI_LEVI_SEG_IDLE;
            v->menv[o].loop = 0u;
            v->menv[o].loopend = RI_LEVI_SEG_D2;
            for (pp = 0u; pp < RI_LEVI_OP_NPARAM; pp++)
                v->meui[o][pp] = (uint8_t)ri_levi_menv_default(o, pp);
            for (pp = RI_LEVI_OP_DELAY; pp <= RI_LEVI_OP_FREERUN; pp++)
                env_ui_apply(&v->menv[o], v->meui[o], pp);
            v->melevel[o] = 1.0f;
            v->mvelcrv[o] = 0.0f;
        }
        v->melfo = 0u;
        v->mepad[0] = v->mepad[1] = 0u;
        v->denv = v->aenv = v->vinit = 0.0f;
        v->dvel = v->dpat = v->avel = v->apat = v->vvel = v->vpat = 0.0f;
        v->vdetune = v->vafeel = v->vrndph = 0.0f;
        v->vpan = 0.0f;
        v->vwidth = 0.5f;
        v->vpanmode = 0u;
        v->vbendrng = 2.0f;
        v->vvibrate = 5.0f;
        v->vvibamt = v->vvibdly = 0.0f;
        v->vglide = 0u;
        v->gforce = 0u;   /* glide button (P9d); refreshed per block */
        v->vgltime = 1.0f;
        v->vglcurve = 1.0f;
        v->vibphase = v->vibtime = 0.0f;
        v->glsemi = v->glt = 0.0f;
        v->wtime = (float)i * 1.7f;
        v->vidx = (uint8_t)i;
        v->vpad[0] = v->vpad[1] = v->vpad[2] = 0u;
        memset(v->vom, 0, sizeof v->vom);
        v->vom_on = 0u;
        memset(v->dfxm, 0, sizeof v->dfxm);
        v->dfxm_on = 0u;
        memset(v->rfxm, 0, sizeof v->rfxm);
        v->rfxm_on = 0u;
        memset(v->pfxm, 0, sizeof v->pfxm);
        v->pfxm_on = 0u;
        memset(v->ofxm, 0, sizeof v->ofxm);
        v->ofxm_on = 0u;
        memset(v->axm, 0, sizeof v->axm);
        v->axm_on = 0u;
        memset(v->sxm, 0, sizeof v->sxm);
        v->sxm_on = 0u;
        v->vspread = 0.0f;
        {
            uint32_t q;
            for (q = 0u; q < RI_LEVI_NOPS; q++)
                v->oppan[q] = 0.0f;
        }
        v->vscale = 0u;
        v->vmicro = 0u;
        v->vkeylock = 0u;
        v->vint_bits = 16u;
        v->vint_dec = 1u;
        v->vpad2[0] = v->vpad2[1] = v->vpad2[2] = 0u;
        v->vhold[0] = v->vhold[1] = 0.0f;
        v->vcount = 0u;
    }
    for (o = 0u; o < RI_LEVI_NLFO; o++)
        lfo_init(&s->glfo[o], 97u + o);
    s->bias[0] = s->bias[1] = s->bias[2] = s->bias[3] = 64u;
    s->arpon = 0u;
    s->arprate = 64u;
    /* Device arp params (fidelity P8b): defaults = v2-identical sound. */
    s->arpoctmode = 0u;
    s->arpoctrange = 0u;
    s->arpgate = 127u;
    s->arpmode = RI_LEVI_ARP_UP;
    s->arplen = 127u;
    s->arpphrase = 0u;
    s->arpentropy = 0u;
    s->arpswing = 0u;
    s->arpratchet = 0u;
    s->arpchance = 0u;
    s->arplatch = 0u;
    s->arpclock = 1u;
    s->arpstepoff = 0u;
    s->arppad[0] = s->arppad[1] = s->arppad[2] = 0u;
    s->arp_samp = 0u;
    s->arp_t0 = 0u;
    s->arp_k = 0u;
    s->arp_pos = 0u;
    s->arp_oct = 0u;
    s->arp_lcg = 0x9E3779B9u;
    s->arp_nstr = 0u;
    {
        uint32_t g;
        for (g = 0u; g < RI_LEVI_NVOICES; g++)
            s->arp_gate[g] = -1;
    }
    s->arp_nchord = 0u;
    s->arp_nlatch = 0u;
    s->arp_npend = 0u;
    ri_levi_arp_init(&s->darp);
    /* Device sequencer (fidelity P8c): silent until recorded. */
    s->seqrate = 64u;
    s->seqmode = 1u;   /* parallel: empty tracks are rests until recorded */
    s->seqswing = 0u;
    s->seqgate = 127u;
    s->seqprob = 127u;
    s->seqdrift = 64u;
    s->seqtransp = 64u;
    s->seqtrklen = 127u;
    s->seqrec = 0u;
    s->seqstep = 0u;
    s->seqstrig = 0u;
    s->seqsprob = 127u;
    s->seqsdrift = 64u;
    s->seqsentr = 0u;
    s->seqpad[0] = s->seqpad[1] = 0u;
    {
        uint32_t st, nn;
        for (st = 0u; st < RI_LEVI_SEQ_STEPS; st++) {
            for (nn = 0u; nn < RI_LEVI_SEQ_NOTES; nn++)
                s->seq_t1[st].note[nn] = s->seq_t2[st].note[nn] = 255u;
            s->seq_t1[st].vel = s->seq_t2[st].vel = 100u;
            s->seq_t1[st].gate = s->seq_t2[st].gate = 100u;
            s->seq_t1[st].trig = s->seq_t2[st].trig = 1u;
            s->seq_t1[st].prob = s->seq_t2[st].prob = 127u;
            s->seq_t1[st].drift = s->seq_t2[st].drift = 0;
            s->seq_t1[st].entropy = s->seq_t2[st].entropy = 0u;
            s->seq_t1[st].spad = s->seq_t2[st].spad = 0u;
        }
    }
    s->seq_lastk = -1;
    s->seq_lastrec = -1;
    s->seq_tick = 0u;
    s->seq_lcg = 0x9E3779B9u;
    s->seq_nstr = 0u;
    {
        uint32_t g;
        for (g = 0u; g < RI_LEVI_NVOICES; g++) {
            s->seq_gate[g] = -1;
            s->seq_gnote[g] = 0u;
        }
    }
    s->seq_npend = 0u;
    s->seq_samp = 0u;
    /* Ribbon (fidelity P8d): centered, untouched, off. */
    s->rbn_pos = 64u;
    s->rbn_touch = 0u;
    s->rbn_mode = 0u;
    s->rbn_last = 64u;
    s->rbn_pad[0] = s->rbn_pad[1] = s->rbn_pad[2] = s->rbn_pad[3] = 0u;
    /* Performance signals (fidelity P9a): full velocity, no pressure,
     * no wheel, bend centred. */
    s->pvel = 127u;
    s->rvel = 127u;
    s->press = 0u;
    s->wheel = 0u;
    memset(s->pat, 0, sizeof s->pat);
    s->psigpad = 0u;
    s->bend = 0.0f;
    /* Keyboard zones (fidelity P9c): Single at the centre octave, the
     * balance even, the split key at middle C, no pending layer gain. */
    s->p_mode = RI_LEVI_PF_SINGLE;
    s->p_sel = RI_LEVI_PF_BOTH;
    s->p_split = RI_LEVI_PF_DUAL;
    s->p_oct = 0u;
    s->p_bal = 64u;
    s->p_splitkey = 60u;
    s->p_zgain = 1.0f;
    s->p_glidehold = 0u;
    s->p_chord = 0u;
    s->p_chord_on = 0u;
    s->p_chord_n = 0u;
    for (i = 0u; i < RI_LEVI_NCHORD; i++)
        s->p_chord_note[i] = 0u;
    s->seqon = 0u;
    s->seqlen = 16u;
    ri_levi_matrix_init(&s->mx);
    s->fx.dtype = RI_LEVI_DT_CLEAN;
    s->fx.dbypass = 1u;   /* bypassed by default: songs bit-identical */
    s->fx.dbpm = 0u;
    s->fx.dtime = ri_levi_delay_time(64u);
    s->fx.dfb = 64.0f / 127.0f * 0.95f;
    s->fx.dwtone = 18000.0f;
    s->fx.dfbtone = 8000.0f;
    s->fx.ddrywet = 32.0f / 127.0f;
    levi_fx_clear(&s->fx);
    s->fx.rtype = RI_LEVI_RT_ROOM;
    s->fx.rfreeze = 0u;
    s->fx.rbypass = 1u;   /* bypassed by default: songs bit-identical */
    s->fx.rpredly = 0.0f;
    s->fx.rtime = 64.0f / 127.0f * 0.95f;
    s->fx.rtone = 18000.0f;
    s->fx.rhidamp = 18000.0f;
    s->fx.rlodamp = 20.0f;
    s->fx.rdrywet = 32.0f / 127.0f;
    memset(s->fx.rfxm, 0, sizeof s->fx.rfxm);
    s->fx.rfxm_on = 0u;
    s->fx.rpad = 0u;
    s->fx.fxpad[0] = s->fx.fxpad[1] = s->fx.fxpad[2] = 0u;
    s->fx.rfxpad[0] = s->fx.rfxpad[1] = s->fx.rfxpad[2] = 0u;
    levi_fx_reverb_bind(&s->fx);
    s->tempo_bpm = 140.0f;   /* device tempo cache (P8a; engine pushes per block) */
    /* Mod slots (fidelity P7c): chorus by default, bypassed (songs
     * bit-identical until switched on). */
    s->fx.pre.type = s->fx.post.type = RI_LEVI_MT_CHORUS;
    s->fx.pre.preset = s->fx.post.preset = 0u;
    s->fx.pre.bypass = s->fx.post.bypass = 1u;
    s->fx.pre.p1 = s->fx.post.p1 = 32.0f / 127.0f;
    s->fx.pre.p2 = s->fx.post.p2 = 64.0f / 127.0f;
    s->fx.pre.drywet = s->fx.post.drywet = 32.0f / 127.0f;
    memset(s->fx.pre.mxm, 0, sizeof s->fx.pre.mxm);
    memset(s->fx.post.mxm, 0, sizeof s->fx.post.mxm);
    s->fx.pre.mxm_on = s->fx.post.mxm_on = 0u;
    s->polymode = RI_LEVI_POLY_ROTATE;
    s->udensity = 8u;
    s->ulimit = RI_LEVI_NVOICES;
    s->arot = 0u;
    s->an = 0u;
    s->apad[0] = s->apad[1] = 0u;
    memset(s->anotes, 0, sizeof s->anotes);
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

/* ---- Full LFOs (fidelity P5, manual pp. 76-82; own laws) ---- */
static const char *const LFO_WAVE_NAME[RI_LEVI_NLW] = {
    "SINE", "TRIANGLE", "SAW UP", "SAW DOWN", "SQUARE", "PULSE 27", "PULSE 13", "S&H", "NOISE", "RANDOM", "STEP"
};
static const uint8_t LFO_HI[RI_LEVI_LP_N] = { 10, 127, 1, 2, 127, 127, 15, 127, 64, 127, 1, 2, 127, 127, 1 };

const char *ri_levi_lfo_wave_name(uint32_t w) {
    return w < RI_LEVI_NLW ? LFO_WAVE_NAME[w] : "";
}

int ri_levi_lfo_range(uint32_t param, int *lo, int *hi) {
    if (param >= RI_LEVI_LP_N || !lo || !hi)
        return 2;
    *lo = param == RI_LEVI_LP_STEPS ? 2 : 0;
    *hi = LFO_HI[param];
    return 0;
}

int ri_levi_lfo_default(uint32_t param) {
    switch (param) {
    case RI_LEVI_LP_RATE: return 36;          /* ~0.57 Hz, the v1 default rate */
    case RI_LEVI_LP_LEVEL: return 127;
    case RI_LEVI_LP_STEPS: return 3;          /* the v1 3-step shape */
    default: return 0;
    }
}

float ri_levi_lfo_hz(uint32_t fast, uint8_t ui) {
    float u = (float)(ui > 127u ? 127u : ui) / 127.0f;
    if (fast)
        return 5.0f * ri_pow2(u * 4.9068906f);   /* 5 .. 150 Hz (x30) */
    return 25.0f * u * u * u;                   /* 0 .. 25 Hz */
}

static uint32_t xrng(uint32_t *r) {
    uint32_t x = *r ? *r : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *r = x;
    return x;
}

static float xrnd(uint32_t *r) {                /* -1 .. 1 */
    return (float)(xrng(r) >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

/* Steps in use: the Steps param plus its matrix offset (P5b), 2..64. */
static uint32_t lfo_nsteps(const struct RILeviLFO *l) {
    int n = (int)l->steps + (l->stmod != 0.0f ? (int)(l->stmod * 62.0f) : 0);
    return (uint32_t)(n < 2 ? 2 : n > 64 ? 64 : n);
}

static float lfo_step_value(uint32_t n, uint32_t k) {
    return -1.0f + 2.0f * (float)(k % n) / (float)(n - 1u);
}

/* Step table value law (fidelity P8e): the panel is bipolar 7-bit with
 * 64 at the centre, the table keeps the value minus 64, and the wave is
 * that value over 63 (so -64 and -63 both read -1). */
static float step_f(int8_t v) {
    float x = (float)v * (1.0f / 63.0f);
    return x < -1.0f ? -1.0f : x;
}

static int8_t step_q(float x) {
    int v;
    if (x < -1.0f)
        x = -1.0f;
    if (x > 1.0f)
        x = 1.0f;
    v = (int)(x * 63.0f + (x < 0.0f ? -0.5f : 0.5f));
    return (int8_t)v;
}

/* SEMI LOCK: a written value lands on the 1/12 grid of the bipolar range
 * (one semitone over the two octaves the wave spans, 25 levels). */
static uint8_t step_snap(uint8_t val) {
    float x = (float)((int)val - 64) / 63.0f;
    float g;
    int v;
    if (x < -1.0f)
        x = -1.0f;
    if (x > 1.0f)
        x = 1.0f;
    g = (float)(int)(x * 12.0f + (x < 0.0f ? -0.5f : 0.5f)) / 12.0f;
    v = (int)(g * 63.0f + (g < 0.0f ? -0.5f : 0.5f)) + 64;
    return (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
}

/* First write lays the current analytic ramp into all 64 entries, so
 * editing one step leaves the others where they were. */
static void lfo_ladder(struct RILeviLFO *l) {
    uint32_t k, n = lfo_nsteps(l);
    for (k = 0u; k < RI_LEVI_MAXSTEPS; k++)
        l->sval[k] = step_q(lfo_step_value(n, k >= n ? n - 1u : k));
    l->sown = 1u;
}

/* Raw wave at phase p (0..1). */
static float lfo_wave(struct RILeviLFO *l, float p) {
    switch (l->wave) {
    case RI_LEVI_LW_TRI: return p < 0.25f ? 4.0f * p : p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f;
    case RI_LEVI_LW_SAWUP: return 2.0f * p - 1.0f;
    case RI_LEVI_LW_SAWDN: return 1.0f - 2.0f * p;
    case RI_LEVI_LW_SQUARE: return p < 0.5f ? 1.0f : -1.0f;
    case RI_LEVI_LW_PULSE27: return p < 0.27f ? 1.0f : -1.0f;
    case RI_LEVI_LW_PULSE13: return p < 0.13f ? 1.0f : -1.0f;
    case RI_LEVI_LW_SH: return l->held;
    case RI_LEVI_LW_NOISE: return xrnd(&l->rng);
    case RI_LEVI_LW_RANDOM: {
        float x = p * p * (3.0f - 2.0f * p);
        return l->from + (l->held - l->from) * x;
    }
    case RI_LEVI_LW_STEP: {
        uint32_t n = lfo_nsteps(l);
        uint32_t k = l->oneshot == 2u ? (l->stepk == 0xFFu ? 0u : l->stepk) : (uint32_t)(p * (float)n);
        if (k >= n)
            k = n - 1u;
        return l->sown ? step_f(l->sval[k]) : lfo_step_value(n, k);
    }
    default: return ri_sin(2.0f * 3.14159265f * p);
    }
}

/* Level, delay/fade, quantize, smooth (all bypassed at their defaults:
 * the v1 value exactly). sy: smoother state. */
static float lfo_post(const struct RILeviLFO *l, float v, float *sy) {
    static const uint8_t Q[16] = { 0, 2, 3, 4, 5, 6, 8, 10, 12, 16, 24, 32, 48, 64, 96, 128 };
    {
        float lv = l->level + l->lmod;                  /* level + its matrix offset */
        if (lv != 1.0f)
            v *= lv < 0.0f ? 0.0f : lv > 1.0f ? 1.0f : lv;
    }
    if (l->delay > 0.0f || l->fade > 0.0f) {
        float g = l->t < l->delay ? 0.0f : l->fade > 0.0f ? (l->t - l->delay) / l->fade : 1.0f;
        v *= g > 1.0f ? 1.0f : g;
    }
    if (l->quant) {
        float q = (float)Q[l->quant & 15u] * 0.5f;
        v = (float)(int)(v * q + (v < 0.0f ? -0.5f : 0.5f)) / q;
    }
    {
        float sm = l->smooth;
        if (l->smod != 0.0f) {                          /* matrix-moved smooth (P5b) */
            float u = (float)l->ui[RI_LEVI_LP_SMOOTH] / 127.0f + l->smod;
            sm = u <= 0.0f ? 0.0f : 1.0f - ri_pow2(-12.0f + 11.0f * (1.0f - (u > 1.0f ? 1.0f : u)));
        }
        if (sm > 0.0f && l->oneshot != 2u) {
            *sy += (v - *sy) * (1.0f - sm);
            v = *sy;
        }
    }
    return v;
}

/* Advance phase / time one sample; sets wrapped on a new cycle. */
static void lfo_advance(struct RILeviLFO *l, float sr) {
    l->t += 1.0f / sr;
    l->wrapped = 0u;
    if (l->oneshot == 2u || l->done)
        return;
    l->phase += l->rate * l->rmul / sr;
    if (l->phase >= 1.0f) {
        l->phase -= (float)(int)l->phase;
        l->wrapped = 1u;
        l->from = l->held;
        l->held = xrnd(&l->rng);
        if (l->oneshot == 1u) {
            l->done = 1u;                         /* one cycle, then hold its end */
            l->phase = 0.9999999f;
        }
    }
}

float ri_levi_lfo_step(struct RILeviLFO *l, float sr) {
    if (!l || !(sr > 0.0f))
        return 0.0f;
    if (l->shared)
        return l->value;
    if (!(l->rate > 0.0f) && l->oneshot != 2u && l->wave != RI_LEVI_LW_NOISE)
        return l->value;
    lfo_advance(l, sr);
    l->value = lfo_post(l, lfo_wave(l, l->phase), &l->sy);
    return l->value;
}

/* Note-on for one LFO (poly: every voice; single: the shared one). */
static void lfo_trigger(struct RILeviLFO *l) {
    l->phase = l->phase0;
    l->t = 0.0f;
    l->done = 0u;
    l->value = 0.0f;
    l->sy = 0.0f;
    l->from = l->held;
    l->held = xrnd(&l->rng);
    if (l->oneshot == 2u)                         /* the first note plays step 1 */
        l->stepk = l->stepk == 0xFFu ? 0u : (uint8_t)((l->stepk + 1u) % lfo_nsteps(l));
}

static void lfo_apply(struct RILeviLFO *l, uint32_t p) {
    const uint8_t *u = l->ui;
    uint32_t slow = u[RI_LEVI_LP_SPEED] ? 0u : 1u;   /* env-time speed flag: 1 = the long range */
    switch (p) {
    case RI_LEVI_LP_WAVE: l->wave = u[p]; l->shape = u[p] == RI_LEVI_LW_STEP ? RI_LEVI_LFO_STEPS : RI_LEVI_LFO_SMOOTH; break;
    case RI_LEVI_LP_RATE: case RI_LEVI_LP_SPEED: case RI_LEVI_LP_DELAY: case RI_LEVI_LP_FADE:
        /* the speed range sets the rate range and the delay/fade ranges */
        if (p == RI_LEVI_LP_RATE || p == RI_LEVI_LP_SPEED)
            l->rate = ri_levi_lfo_hz(u[RI_LEVI_LP_SPEED], u[RI_LEVI_LP_RATE]);
        l->delay = ri_levi_env_time(RI_LEVI_OP_DELAY, u[RI_LEVI_LP_DELAY], slow);
        l->fade = ri_levi_env_time(RI_LEVI_OP_ATTACK, u[RI_LEVI_LP_FADE], slow);
        break;
    case RI_LEVI_LP_TRIG: l->trig = u[p]; l->shared = u[p] ? 1u : 0u; break;
    case RI_LEVI_LP_QUANT: l->quant = u[p]; break;
    case RI_LEVI_LP_LEVEL: l->level = (float)u[p] / 127.0f; break;
    case RI_LEVI_LP_STEPS: l->steps = u[p]; break;
    case RI_LEVI_LP_SEMI: l->semi = u[p] ? 1u : 0u; break;
    case RI_LEVI_LP_SMOOTH: l->smooth = u[p] ? 1.0f - ri_pow2(-12.0f + 11.0f * (1.0f - (float)u[p] / 127.0f)) : 0.0f; break;
    case RI_LEVI_LP_ONESHOT: l->oneshot = u[p]; l->done = 0u; l->stepk = 0xFFu; break;
    case RI_LEVI_LP_PHASE: l->phase0 = (float)u[p] / 128.0f; break;
    default: break;                               /* BPM, stagger: read where used */
    }
}

/* Tempo refresh (fidelity P8a): recompute flagged musical times from
 * retained UI values + the cached tempo. Runs at the top of every
 * sum_stereo call; flag branches only when everything is off, so the
 * steady-state cost is ~600 branches per block against an 8-voice
 * render. No dirty flag (knob edits on flagged units apply instantly). */
static void lfo_sync_apply(struct RILeviLFO *l, float bpm) {
    const uint8_t *u;
    if (!l)
        return;
    u = l->ui;
    if (!u[RI_LEVI_LP_BPM])
        return;
    l->rate = ri_levi_lfo_sync_hz(u[RI_LEVI_LP_RATE], bpm);
    l->delay = beats_of(u[RI_LEVI_LP_DELAY], bpm);
    l->fade = beats_of(u[RI_LEVI_LP_FADE], bpm);
}

static void env_sync_apply(struct RILeviEnv *e, const uint8_t *u, float bpm) {
    if (!e || !u)
        return;
    e->times[RI_LEVI_SEG_D] = beats_of(u[RI_LEVI_OP_DELAY], bpm);
    e->times[RI_LEVI_SEG_A] = beats_of(u[RI_LEVI_OP_ATTACK], bpm);
    e->times[RI_LEVI_SEG_H] = beats_of(u[RI_LEVI_OP_HOLD], bpm);
    e->times[RI_LEVI_SEG_D2] = beats_of(u[RI_LEVI_OP_DECAY], bpm);
    e->times[RI_LEVI_SEG_R] = beats_of(u[RI_LEVI_OP_RELEASE], bpm);
}

static void levi_tempo_refresh(struct RILeviSet *s) {
    uint32_t v, o, e;
    float bpm;
    if (!s)
        return;
    bpm = s->tempo_bpm;
    for (v = 0u; v < RI_LEVI_NVOICES; v++) {
        struct RILeviVoice *vv = &s->v[v];
        for (o = 0u; o < RI_LEVI_NOPS; o++) {
            if (vv->opbpm[o]) {
                uint32_t b;
                for (b = 0u; b < 2u; b++)
                    env_sync_apply(&vv->st[b][o].env, vv->op[o].ui, bpm);
            }
        }
        for (e = 0u; e < RI_LEVI_NMENV; e++) {
            if (vv->mebpm[e])
                env_sync_apply(&vv->menv[e], vv->meui[e], bpm);
        }
        for (o = 0u; o < RI_LEVI_NLFO; o++)
            lfo_sync_apply(&vv->lfo[o], bpm);
    }
    for (o = 0u; o < RI_LEVI_NLFO; o++)
        lfo_sync_apply(&s->glfo[o], bpm);
    if (s->fx.dbpm)
        s->fx.dtime = ri_levi_delay_snap(s->fx.dtime, bpm);
}

static void lfo_init(struct RILeviLFO *l, uint32_t seed) {
    uint32_t p;
    memset(l, 0, sizeof *l);
    for (p = 0u; p < RI_LEVI_LP_N; p++)
        l->ui[p] = (uint8_t)ri_levi_lfo_default(p);
    for (p = 0u; p < RI_LEVI_LP_N; p++)
        lfo_apply(l, p);
    l->rate = ri_levi_lfo_rate(64u);              /* the v1 rate exactly */
    l->rmul = 1.0f;
    l->rng = 0x9E3779B9u ^ (seed * 2654435761u);
}

int levi_set_lfo_ui(struct RILeviSet *s, uint32_t voice, uint32_t lfo, uint32_t param, uint8_t val) {
    struct RILeviLFO *l;
    uint32_t k;
    int lo, hi;
    if (!s || voice >= RI_LEVI_NVOICES || lfo >= RI_LEVI_NLFO || ri_levi_lfo_range(param, &lo, &hi))
        return 2;
    val = (uint8_t)(val < lo ? lo : val > hi ? hi : val);
    for (k = 0u; k < 2u; k++) {                   /* the voice's own and the shared copy */
        l = k ? &s->glfo[lfo] : &s->v[voice].lfo[lfo];
        l->ui[param] = val;
        lfo_apply(l, param);
    }
    s->glfo[lfo].shared = 0u;                     /* the set steps the shared copy itself */
    return 0;
}

/* Step editor (fidelity P8e): one write reaches every voice's copy and
 * the shared one (the table is device state), the cursor is per LFO. */
int levi_set_lfo_stepctl(struct RILeviSet *s, uint32_t lfo, uint32_t field, uint8_t val) {
    struct RILeviLFO *l;
    uint32_t k;
    if (!s || lfo >= RI_LEVI_NLFO || field > RI_LEVI_LS_RAMP)
        return 2;
    if (field == RI_LEVI_LS_STEP) {
        s->lsc[lfo] = val >= RI_LEVI_MAXSTEPS ? (uint8_t)(RI_LEVI_MAXSTEPS - 1u) : val;
        return 0;
    }
    if (field == RI_LEVI_LS_RAMP) {
        if (val)                                /* back to the analytic ramp */
            for (k = 0u; k <= RI_LEVI_NVOICES; k++)
                (k < RI_LEVI_NVOICES ? &s->v[k].lfo[lfo] : &s->glfo[lfo])->sown = 0u;
        return 0;
    }
    if (s->glfo[lfo].semi)
        val = step_snap(val);
    for (k = 0u; k <= RI_LEVI_NVOICES; k++) {
        l = k < RI_LEVI_NVOICES ? &s->v[k].lfo[lfo] : &s->glfo[lfo];
        if (!l->sown)
            lfo_ladder(l);
        l->sval[s->lsc[lfo]] = (int8_t)((int)val - 64);
    }
    return 0;
}

/* ---- Scales + microtuning (fidelity P6c, manual pp. 87-96).
 * Own sets: scale degrees as pitch-class masks (standard theory names
 * are fine by name), microtuning as authored cent tables. ---- */
static const uint16_t RI_LEVI_SCALES[RI_LEVI_NSCALES] = {
    0x0fff, /* CHROMATIC */
    0x0ab5, /* MAJOR */
    0x05ad, /* MINOR */
    0x06ad, /* DORIAN */
    0x05ab, /* PHRYGIAN */
    0x0ad5, /* LYDIAN */
    0x06b5, /* MIXOLYDIAN */
    0x056b, /* LOCRIAN */
    0x09ad, /* HARM MINOR */
    0x0aad, /* MELODIC MINOR */
    0x0295, /* MAJ PENT */
    0x04a9, /* MIN PENT */
    0x04e9, /* BLUES */
    0x0555, /* WHOLE TONE */
    0x06db, /* DIMINISHED */
    0x09b3, /* ARABIAN */
};

static const char *const SCALE_NAME[RI_LEVI_NSCALES] = {
    "CHROMATIC", "MAJOR", "MINOR", "DORIAN", "PHRYGIAN", "LYDIAN",
    "MIXOLYDIAN", "LOCRIAN", "HARM MINOR", "MELODIC MINOR", "MAJ PENT",
    "MIN PENT", "BLUES", "WHOLE TONE", "DIMINISHED", "ARABIAN"
};

static const int16_t RI_LEVI_MICROS[RI_LEVI_NMICRO][12] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },                     /* EQUAL */
    { 0, 114, 204, 294, 408, 498, 612, 702, 792, 906, 996, 1110 }, /* PYTHAG */
    { 0, 76, 193, 310, 386, 503, 579, 697, 814, 890, 1007, 1083 }, /* MEANTONE */
    { 0, 71, 204, 316, 386, 498, 568, 702, 773, 884, 1018, 1088 }, /* JUST */
    { 0, 90, 204, 250, 386, 498, 602, 702, 750, 906, 1000, 1050 }, /* ARABIC */
    { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88 },           /* STRETCH */
    { 0, -8, -16, -24, -32, -40, -48, -56, -64, -72, -80, -88 }, /* SQUEEZE */
    { 0, 90, 192, 294, 390, 498, 588, 696, 792, 888, 996, 1092 }, /* WERCK */
};

static const char *const MICRO_NAME[RI_LEVI_NMICRO] = {
    "EQUAL", "PYTHAG", "MEANTONE", "JUST", "ARABIC", "STRETCH", "SQUEEZE", "WERCK"
};

const char *ri_levi_scale_name(uint32_t w) {
    return w < RI_LEVI_NSCALES ? SCALE_NAME[w] : "";
}

const char *ri_levi_micro_name(uint32_t w) {
    return w < RI_LEVI_NMICRO ? MICRO_NAME[w] : "";
}

/* Scale quantize (key lock): nearest degree of the voice's map, ties
 * to the lower degree; chromatic map (or lock off) is identity. Pure. */
static uint8_t voice_quantize(const struct RILeviVoice *v, uint8_t note) {
    uint16_t mask;
    uint32_t pc, d;
    if (!v || !v->vkeylock || v->vscale >= RI_LEVI_NSCALES)
        return note;
    mask = RI_LEVI_SCALES[v->vscale];
    if (mask == 0x0FFFu)
        return note;
    pc = (uint32_t)note % 12u;
    if ((mask >> pc) & 1u)
        return note;
    for (d = 1u; d < 12u; d++) {
        uint32_t lo = (pc + 12u - d) % 12u, hi = (pc + d) % 12u;
        int lo_in = (mask >> lo) & 1u, hi_in = (mask >> hi) & 1u;
        if (lo_in || hi_in) {
            int dn = (int)note - (int)d, up = (int)note + (int)d;
            if (lo_in && dn >= 0)
                return (uint8_t)dn;
            if (hi_in && up <= 127)
                return (uint8_t)up;
            if (lo_in)
                return (uint8_t)(dn < 0 ? 0 : dn);
            return (uint8_t)(up > 127 ? 127 : up);
        }
    }
    return note;
}

/* Microtuning multiplier for a note (own tables; equal map is exact 1). */
static float voice_micro_mult(const struct RILeviVoice *v, uint8_t note) {
    int16_t c;
    if (!v || v->vmicro >= RI_LEVI_NMICRO)
        return 1.0f;
    c = RI_LEVI_MICROS[v->vmicro][note % 12u];
    if (!c)
        return 1.0f;
    return ri_pow2((float)c / 1200.0f);
}

/* Per-voice/per-op trigger hash 0..1 (fidelity P6b): deterministic,
 * no RNG; static per voice+op so retriggers are identical. */
static float voice_phase_hash(uint32_t voice, uint32_t op) {
    uint32_t h = (voice * 8u + op + 1u) * 0x9E3779B1u;
    h ^= h >> 13;
    h *= 0x85EBCA6Bu;
    h ^= h >> 16;
    return (float)h / 4294967296.0f;
}

int levi_trigger(struct RILeviSet *s, uint32_t voice, uint8_t note) {
    struct RILeviVoice *v;
    float f;
    uint32_t o, b;
    if (!s || voice >= RI_LEVI_NVOICES || note > 127u)
        return 2;
    v = &s->v[voice];
    /* Performance signals (P9a): the pending velocity is the note-on
     * velocity and the starting release velocity; a retriggered voice
     * drops the old key's pressure (its slot is the new key's). */
    v->nvel = s->pvel;
    v->nveloff = s->pvel;
    v->zgain = s->p_zgain;      /* zone layer gain (P9c); 1.0f on the song path */
    s->pat[voice] = 0u;
    note = voice_quantize(v, note);   /* key lock (P6c; identity off/chromatic) */
    f = note_hz(note) * voice_micro_mult(v, note);
    {
        /* Glide (P6b): a new note on a sounding voice slides from the
         * old pitch; fresh voices start on pitch. Legato retunes set
         * this in alloc_retune instead (same law). The glide button
         * (P9d) slides too, read live from the device flag because a
         * trigger runs between blocks. */
        uint8_t old = v->note;
        int was = v->active;
        if ((v->vglide || s->p_glidehold) && was && old != note) {
            v->glsemi = (float)old - (float)note;
            v->glt = 0.0f;
        } else {
            v->glsemi = 0.0f;
            v->glt = 1.0f;
        }
    }
    v->vibphase = 0.0f;
    v->vibtime = 0.0f;
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
                v->st[b][o].phase = frac1(op->phase0 + v->vrndph * voice_phase_hash(voice, o));
            }
    }
    filt_clear(v);
    kt_update(v);
    for (o = 0u; o < RI_LEVI_NLFO; o++) {
        if (v->lfo[o].trig == 0u)
            lfo_trigger(&v->lfo[o]);          /* poly: every note restarts its LFO */
        else if (v->lfo[o].trig == 1u)
            lfo_trigger(&s->glfo[o]);         /* single: every note restarts the shared one */
    }
    for (o = 0u; o < RI_LEVI_NMENV; o++)
        if (menv_has(v, o, RI_LEVI_TS_NOTE))
            menv_start(&v->menv[o]);
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
    for (o = 0u; o < RI_LEVI_NMENV; o++)
        menv_release(&v->menv[o]);
}

/* ---- Voice allocator (fidelity P6a, manual pp. 87-96; own laws) ---- */
static int alloc_has_legato(const struct RILeviVoice *v) {
    uint32_t o;
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        if (v->op[o].ui[RI_LEVI_OP_LEGATO])
            return 1;
    return 0;
}

static int alloc_has_reset(const struct RILeviVoice *v) {
    uint32_t o;
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        if (v->op[o].ui[RI_LEVI_OP_RESET])
            return 1;
    return 0;
}

/* Legato retune: new pitch without restarting envelopes, LFOs, mod envs
 * or filters (E0: retune-only; reset wins over legato). Glide restarts
 * from the old note like a trigger (P6b), and the glide button (P9d)
 * starts one too -- ghold is the device's momentary button, read live
 * here because a retune runs between blocks. */
static void alloc_retune(struct RILeviVoice *v, uint8_t note, int ghold) {
    float f;
    uint32_t o, b;
    uint8_t old = v->note;
    note = voice_quantize(v, note);
    f = note_hz(note) * voice_micro_mult(v, note);
    if ((v->vglide || ghold) && old != note) {
        v->glsemi = (float)old - (float)note;
        v->glt = 0.0f;
    } else {
        v->glsemi = 0.0f;
        v->glt = 1.0f;
    }
    v->note = note;
    for (b = 0u; b < 2u; b++) {
        uint8_t *live = b ? v->liveB : v->live;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (live[o]) {
                const struct RILeviOp *op = &v->op[o];
                float base = op->kt == 1.0f ? f
                    : note_hz(60u) * ri_pow2((((float)note - 60.0f) / 12.0f) * op->kt);
                v->st[b][o].freq = op->pmode == 2u ? op->hz
                    : op->pmode == 0u ? base * op->pitchmul : base * op->ratio;
            }
    }
    v->active = 1u;
    kt_update(v);
}

/* Is this note one of the held keys? Chord mode (P9d) works on the hold
 * list, because a chord's voices hold the pushed lanes, not the key the
 * player pressed. */
static int alloc_holds(const struct RILeviSet *s, uint8_t note) {
    uint32_t i;
    for (i = 0u; i < s->an; i++)
        if (s->anotes[i] == note)
            return 1;
    return 0;
}

static void alloc_hold_add(struct RILeviSet *s, uint8_t note) {
    uint32_t i;
    for (i = 0u; i < s->an; i++)
        if (s->anotes[i] == note)
            return;
    if (s->an < 16u)
        s->anotes[s->an++] = note;
}

static void alloc_hold_del(struct RILeviSet *s, uint8_t note) {
    uint32_t i, j;
    for (i = 0u; i < s->an; i++)
        if (s->anotes[i] == note) {
            for (j = i; j + 1u < s->an; j++)
                s->anotes[j] = s->anotes[j + 1u];
            s->an--;
            return;
        }
}

/* A voice is reusable when silent or when its note is no longer held
 * (released, ringing out). */
static int alloc_reuse(const struct RILeviSet *s, uint32_t v) {
    uint32_t i;
    if (!s->v[v].active)
        return 1;
    for (i = 0u; i < s->an; i++)
        if (s->anotes[i] == s->v[v].note)
            return 0;
    return 1;
}

static uint8_t alloc_pick_mono(const struct RILeviSet *s, uint32_t mode) {
    uint32_t i;
    uint8_t n;
    if (!s->an)
        return 0u;
    n = s->anotes[0];
    for (i = 1u; i < s->an; i++)
        if ((mode == RI_LEVI_POLY_MONOLO || mode == RI_LEVI_POLY_UNISONLO) ? s->anotes[i] < n : s->anotes[i] > n)
            n = s->anotes[i];
    return n;
}

static void alloc_fire(struct RILeviSet *s, uint32_t voice, uint8_t note, int legato_ok) {
    struct RILeviVoice *v = &s->v[voice];
    if (legato_ok && v->active && alloc_has_legato(v) && !alloc_has_reset(v))
        alloc_retune(v, note, (int)s->p_glidehold);
    else
        levi_trigger(s, voice, note);
}

int levi_set_alloc_ui(struct RILeviSet *s, uint32_t mode) {
    if (!s || mode >= RI_LEVI_POLY_N)
        return 2;
    s->polymode = (uint8_t)mode;
    return 0;
}

uint32_t levi_alloc_mode(const struct RILeviSet *s) {
    return s ? s->polymode : 0u;
}

/* Allocator body (fidelity P6a): the poly-mode policy, no hold list, no
 * zone routing. note_on_core below wraps it (P9c). */
static int note_on_body(struct RILeviSet *s, uint8_t note) {
    uint32_t v, n = 0u;
    uint32_t mode, dens, lim;
    if (!s || note > 127u)
        return -1;
    mode = s->polymode;
    dens = s->udensity < 1u ? 1u : s->udensity > 8u ? 8u : s->udensity;
    lim = s->ulimit < 1u ? 1u : s->ulimit > RI_LEVI_NVOICES ? RI_LEVI_NVOICES : s->ulimit;
    switch (mode) {
    case RI_LEVI_POLY_MONO:
    case RI_LEVI_POLY_MONOLO:
    case RI_LEVI_POLY_MONOHI:
        alloc_fire(s, 0u, mode == RI_LEVI_POLY_MONO ? note : alloc_pick_mono(s, mode), 1);
        return 1;
    case RI_LEVI_POLY_UNISON:
    case RI_LEVI_POLY_UNISONLO:
    case RI_LEVI_POLY_UNISONHI: {
        uint8_t nn = mode == RI_LEVI_POLY_UNISON ? note : alloc_pick_mono(s, mode);
        for (v = 0u; v < lim; v++) {
            alloc_fire(s, v, nn, v == 0u);
            n++;
        }
        return (int)n;
    }
    case RI_LEVI_POLY_UNISONPOLY: {
        uint32_t want = dens > lim ? lim : dens;
        uint32_t freev[RI_LEVI_NVOICES], nf = 0u;
        for (v = 0u; v < lim; v++)
            if (alloc_reuse(s, v))
                freev[nf++] = v;
        for (v = 0u; v < want; v++) {
            uint32_t t = v < nf ? freev[v] : (uint32_t)(s->arot++ % lim);
            alloc_fire(s, t, note, 0);
            n++;
        }
        return (int)n;
    }
    case RI_LEVI_POLY_REASSIGN: {
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (alloc_reuse(s, v)) {
                alloc_fire(s, v, note, 0);
                return 1;
            }
        /* Steal oldest: lowest voice index sounding the longest-held note. */
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (s->v[v].note == s->anotes[0]) {
                alloc_fire(s, v, note, 0);
                return 1;
            }
        alloc_fire(s, 0u, note, 0);
        return 1;
    }
    default: { /* RI_LEVI_POLY_ROTATE */
        for (v = 0u; v < RI_LEVI_NVOICES; v++) {
            uint32_t t = (s->arot + v) % RI_LEVI_NVOICES;
            if (alloc_reuse(s, t)) {
                s->arot = (uint8_t)((t + 1u) % RI_LEVI_NVOICES);
                alloc_fire(s, t, note, 0);
                return 1;
            }
        }
        {
            uint32_t t = s->arot % RI_LEVI_NVOICES;
            s->arot = (uint8_t)((s->arot + 1u) % RI_LEVI_NVOICES);
            alloc_fire(s, t, note, 0);
            return 1;
        }
    }
    }
}

/* ---- Keyboard zones (fidelity P9c; own laws) ----
 *
 * The octave bias moves the note the zone plays; the layer gains sum to 2
 * so a two-layer note is one layer louder than a single note at any
 * balance. A layer at gain 0 is muted but still takes its voice. */
static uint8_t perf_note(const struct RILeviSet *s, uint8_t note) {
    int n = (int)note + 12 * (int)s->p_oct;
    return (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
}

/* A release and per-key pressure arrive on the played key: the voice may
 * hold the biased note, and the bias may have moved since (a knob turn
 * while a key is held must not leave a stuck voice). */
static int note_held_by(const struct RILeviSet *s, uint32_t vnote, uint8_t played) {
    return vnote == played || vnote == perf_note(s, played);
}

int levi_perf_set(struct RILeviSet *s, uint32_t field, int val) {
    if (!s || field >= RI_LEVI_PF_NFIELDS)
        return 2;
    switch (field) {
    case RI_LEVI_PF_OCT:
        s->p_oct = (int8_t)(val < 0 ? -2 : val > 4 ? 2 : val - 2);
        return 0;
    case RI_LEVI_PF_MODE:
        s->p_mode = (uint8_t)(val != 0 ? RI_LEVI_PF_MULTI : RI_LEVI_PF_SINGLE);
        return 0;
    case RI_LEVI_PF_SELECT:
        s->p_sel = (uint8_t)(val < 0 ? RI_LEVI_PF_LOWER : val > 2 ? RI_LEVI_PF_BOTH : (uint32_t)val);
        return 0;
    case RI_LEVI_PF_SPLITM:
        s->p_split = (uint8_t)(val != 0 ? RI_LEVI_PF_KEYSPLIT : RI_LEVI_PF_DUAL);
        return 0;
    case RI_LEVI_PF_BALANCE:
        s->p_bal = (uint8_t)(val < 0 ? 0 : val > 127 ? 127 : val);
        return 0;
    default: /* RI_LEVI_PF_SPLITKEY */
        s->p_splitkey = (uint8_t)(val < 0 ? 0 : val > 127 ? 127 : val);
        return 0;
    }
}

/* Fire one layer: the pending gain rides into the voice at the trigger and
 * never outlives the fire, so a song lane triggered later is at unity. */
static int zone_fire(struct RILeviSet *s, uint8_t note, float gain) {
    int n;
    s->p_zgain = gain;
    n = note_on_body(s, note);
    s->p_zgain = 1.0f;
    return n;
}

/* Allocator core: the hold list keeps the *played* note (the arp chord is
 * the played chord and the bias is applied once, at the strike), the zone
 * routing then offers the shifted note to the layers. */
static int note_on_core(struct RILeviSet *s, uint8_t note, int hold) {
    float gu, gl;
    int n, nl;
    if (!s || note > 127u)
        return -1;
    if (hold)
        alloc_hold_add(s, note);
    note = perf_note(s, note);
#ifdef RI_LEVI_OPT_VOICECAP
    /* P3 O4 (default off): refuse new live notes once N voices sound.
     * Songs bypass the allocator (direct triggers), so corpus and song
     * output are unchanged by construction; live storms drop notes. */
    {
        uint32_t av = 0u, vv;
#ifndef RI_LEVI_OPT_VOICECAP_N
#define RI_LEVI_OPT_VOICECAP_N 6u
#endif
        for (vv = 0u; vv < RI_LEVI_NVOICES; vv++)
            av += s->v[vv].active ? 1u : 0u;
        if (av >= RI_LEVI_OPT_VOICECAP_N)
            return 0;
    }
#endif
    if (s->p_mode == RI_LEVI_PF_SINGLE)            /* one layer, unity */
        return zone_fire(s, note, 1.0f);
    gu = (float)s->p_bal / 64.0f;
    gl = 2.0f - gu;
    if (s->p_split == RI_LEVI_PF_DUAL) {           /* both layers, always */
        n = zone_fire(s, note, gl);              /* lower first, then upper */
        nl = zone_fire(s, note, gu);
        return n + nl;
    }
    /* Key split: SELECT decides, BOTH lets the split key decide. */
    if (s->p_sel == RI_LEVI_PF_BOTH)
        n = zone_fire(s, note, note < s->p_splitkey ? gl : gu);
    else
        n = zone_fire(s, note, s->p_sel == RI_LEVI_PF_UPPER ? gu : gl);
    return n;
}

/* ---- Performance buttons (fidelity P9d; own laws) ---- */

/* Glide hold: a momentary override of the voice glide mode. Device-wide,
 * copied into every voice each block (the render has no set pointer), so
 * releasing it hands the voices back at the next block; a trigger or
 * legato retune reads the flag directly, so a press slides the very next
 * note without waiting for a render. */
int levi_glide_hold(struct RILeviSet *s, int on) {
    if (!s)
        return 2;
    s->p_glidehold = on ? 1u : 0u;
    return 0;
}

int levi_chord_mode(struct RILeviSet *s, int on) {
    if (!s)
        return 2;
    s->p_chord = on ? 1u : 0u;
    return 0;
}

/* Push the chord row. on_flags marks the lanes that sound (bit i = lane
 * i); n is how many lanes are in use, and the lanes past it are cleared
 * so a shorter push really is shorter. */
int levi_chord_set(struct RILeviSet *s, uint32_t on_flags, const uint8_t *notes, int n) {
    int i;
    if (!s || !notes || n < 0 || (uint32_t)n > RI_LEVI_NCHORD)
        return 2;
    for (i = 0; i < (int)RI_LEVI_NCHORD; i++) {
        if (i < n) {
            uint8_t k = notes[i];
            s->p_chord_note[i] = k > 127u ? 127u : k;
        } else {
            s->p_chord_note[i] = 0u;
        }
    }
    s->p_chord_on = (uint8_t)(n > 0 ? on_flags & ((1u << (uint32_t)n) - 1u) : 0u);
    s->p_chord_n = (uint8_t)n;
    return 0;
}

/* Chord fire (P9d): the played key transposes the pushed chord, so the
 * keyboard still plays chords in key. The root is the lowest *on* lane,
 * not lane 0 -- the mask is what the player holds. An empty chord is
 * silent by design (no fallback to the single note). Each lane then
 * enters the allocator core, so a chord also passes the octave bias and
 * the zone routing: a keyboard feature on the same routing, not a second
 * allocator. Returns the voices fired across the lanes. */
static int chord_fire(struct RILeviSet *s, uint8_t note, uint8_t vel) {
    uint32_t i, mask = s->p_chord_on;
    int root = -1, n = 0;
    for (i = 0u; i < RI_LEVI_NCHORD; i++)
        if (mask & (1u << i)) {
            root = (int)s->p_chord_note[i];
            break;
        }
    if (root < 0)
        return 0;
    s->pvel = vel > 127u ? 127u : vel;
    for (i = 0u; i < RI_LEVI_NCHORD; i++) {
        int k;
        if (!(mask & (1u << i)))
            continue;
        k = (int)s->p_chord_note[i] + (int)note - root;
        n += note_on_core(s, (uint8_t)(k < 0 ? 0 : k > 127 ? 127 : k), 1);
    }
    return n;
}

int levi_note_on(struct RILeviSet *s, uint8_t note) {
    if (!s || note > 127u)
        return -1;
    if (s->p_chord)
        return chord_fire(s, note, s->pvel);
    return note_on_core(s, note, 1);
}

/* ---- Performance signals (fidelity P9a; own laws) ---- */
int levi_note_vel(struct RILeviSet *s, uint8_t note, uint8_t vel) {
    if (!s || note > 127u)
        return -1;
    s->pvel = vel > 127u ? 127u : vel;
    if (s->p_chord)
        return chord_fire(s, note, s->pvel);
    return note_on_core(s, note, 1);
}

int levi_note_rel_vel(struct RILeviSet *s, uint8_t note, uint8_t vel) {
    if (!s || note > 127u)
        return -1;
    s->rvel = vel > 127u ? 127u : vel;
    return levi_note_off(s, note);
}

int levi_press(struct RILeviSet *s, uint8_t press) {
    if (!s)
        return 2;
    s->press = press > 127u ? 127u : press;
    return 0;
}

int levi_polyat(struct RILeviSet *s, uint8_t note, uint8_t press) {
    uint32_t v;
    if (!s)
        return 2;
    if (press > 127u)
        press = 127u;
    if (s->p_chord) {
        /* Chord mode (P9d): the pressed key is the *played* key, which is
         * in no voice -- so pressure means the chord, like a release. */
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (s->v[v].active && alloc_holds(s, s->v[v].note))
                s->pat[v] = press;
        return 0;
    }
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        if (s->v[v].active && note_held_by(s, s->v[v].note, note))
            s->pat[v] = press;   /* the played key, bias included (P9c) */
    return 0;
}

int levi_wheel(struct RILeviSet *s, uint8_t val) {
    if (!s)
        return 2;
    s->wheel = val > 127u ? 127u : val;
    return 0;
}

int levi_bend(struct RILeviSet *s, float semis) {
    if (!s)
        return 2;
    if (semis < -24.0f)
        semis = -24.0f;
    else if (semis > 24.0f)
        semis = 24.0f;
    s->bend = semis;
    return 0;
}

int levi_note_strike(struct RILeviSet *s, uint8_t note) {
    if (!s || note > 127u)
        return -1;
    return note_on_core(s, note, 0);
}

int levi_arp_release_note(struct RILeviSet *s, uint8_t note) {
    uint32_t v;
    if (!s || note > 127u)
        return -1;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        if (s->v[v].active && note_held_by(s, s->v[v].note, note))
            levi_release(s, v);
    return 1;
}

int levi_note_off(struct RILeviSet *s, uint8_t note) {
    uint32_t v;
    uint32_t mode;
    if (!s || note > 127u)
        return -1;
    if (s->p_chord) {
        /* Chord mode (P9d): any key releases the whole chord. The hold
         * list is the chord, so every held voice goes with the pending
         * release velocity and the list empties; the poly-mode
         * single-voice cases are bypassed, because one voice cannot own
         * a chord. */
        uint32_t n = 0u;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            if (s->v[v].active && alloc_holds(s, s->v[v].note)) {
                s->v[v].nveloff = s->rvel;   /* release velocity (P9a) */
                levi_release(s, v);
                n++;
            }
        s->an = 0u;
        return (int)n;
    }
    alloc_hold_del(s, note);
    mode = s->polymode;
    if (mode == RI_LEVI_POLY_MONO || mode == RI_LEVI_POLY_MONOLO || mode == RI_LEVI_POLY_MONOHI) {
        if (s->an)
            alloc_fire(s, 0u, alloc_pick_mono(s, mode == RI_LEVI_POLY_MONO ? RI_LEVI_POLY_MONO : mode), 1);
        else {
            s->v[0].nveloff = s->rvel;   /* release velocity (P9a) */
            levi_release(s, 0u);
        }
        return 1;
    }
    if (mode >= RI_LEVI_POLY_UNISON && mode <= RI_LEVI_POLY_UNISONHI) {
        if (s->an) {
            uint8_t nn = mode == RI_LEVI_POLY_UNISON ? s->anotes[s->an - 1u] : alloc_pick_mono(s, mode);
            for (v = 0u; v < s->ulimit && v < RI_LEVI_NVOICES; v++)
                alloc_fire(s, v, nn, v == 0u);
        } else {
            for (v = 0u; v < s->ulimit && v < RI_LEVI_NVOICES; v++) {
                s->v[v].nveloff = s->rvel;   /* release velocity (P9a) */
                levi_release(s, v);
            }
        }
        return 1;
    }
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        if (s->v[v].active && note_held_by(s, s->v[v].note, note)) {
            s->v[v].nveloff = s->rvel;   /* release velocity (P9a) */
            levi_release(s, v);
        }
    return 1;
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
    case RI_LEVI_FTYPE: {
        /* v1 type (legacy key) onto the P4 models: LP/HP/BP 12 dB, notch
         * = the LP-notch-HP SVF at its middle. */
        static const uint8_t MAP[RI_LEVI_NFTYPES] = { RI_LEVI_DF_LP_12, RI_LEVI_DF_HP_12, RI_LEVI_DF_BP_12,
            RI_LEVI_DF_SVF_LNH };
        if (value != (float)RI_LEVI_FTYPE_LP && value != (float)RI_LEVI_FTYPE_HP &&
            value != (float)RI_LEVI_FTYPE_BP && value != (float)RI_LEVI_FTYPE_NOTCH)
            return 2;
        if (v->dtype != MAP[(uint32_t)value])
            filt_clear(v);
        v->dtype = MAP[(uint32_t)value];
        if (v->dtype == RI_LEVI_DF_SVF_LNH)
            v->dmorph = 64u;
        return 0;
    }
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

/* ---- Filters (fidelity P4, manual pp. 62-67; own designs) ----
 * Digital: 18 models from three own primitives: a TPT state-variable
 * core, TPT one-pole ladders with a soft-clipped delayed feedback, and
 * a three-formant band bank. Analog: a 4-pole ladder with a soft-clipped
 * feedback sum (self-oscillates near 110/128) and gain-compensated
 * pre-drive. Every feedback path is bounded (soft clip), coefficients
 * stay below Nyquist, states are flushed (denormal-safe): no inf/NaN
 * (Dell 2026-09-28 record). */
static float sclip(float x) {                /* rational soft clip, |y| <= 1 */
    if (x > 3.0f)
        return 1.0f;
    if (x < -3.0f)
        return -1.0f;
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static float tpt_g(float fc, float sr) {
#ifdef RI_LEVI_PROFILE
    ri_prof_tptg++;
#endif
    float w = 3.14159265f * fc / sr;
    if (!(w > 1e-5f))
        w = 1e-5f;
    if (w > 1.45f)
        w = 1.45f;
    return ri_sin(w) / ri_sin(w + 1.5707963f);
}

/* Filter-coefficient memo (P2 C1): tpt_g is a pure function of (fc, sr),
 * so a per-instance last-key cache returns bit-identical g on repeats.
 * Zero-init is safe: fc = 0 can never match (fc > 0 always, dc >= 20). */
static float tpt_g_memo(struct RILeviVoice *v, uint32_t slot, float fc,
    float sr) {
    uint32_t fb, sb;
    memcpy(&fb, &fc, 4);
    memcpy(&sb, &sr, 4);
    if (v->memo_fc[slot] == fb && v->memo_sr[slot] == sb)
        return v->memo_g[slot];
    {
        float g = tpt_g(fc, sr);
        v->memo_fc[slot] = fb;
        v->memo_sr[slot] = sb;
        v->memo_g[slot] = g;
        return g;
    }
}

static float op1(float *st, float x, float G) {   /* TPT one-pole LP */
    float v = (x - *st) * G, y = v + *st;
    *st = ftz(y + v);
    return y;
}

/* TPT SVF; k = damping (2 = flat, -> 0 resonant); sat bounds the states. */
static void svf(float *st, float x, float g, float k, int sat, float *lp, float *bp, float *hp) {
    float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    float v3 = x - st[1], v1 = a1 * st[0] + a2 * v3, v2 = st[1] + a2 * st[0] + a3 * v3;
    st[0] = ftz(2.0f * v1 - st[0]);
    st[1] = ftz(2.0f * v2 - st[1]);
    if (sat) {
        st[0] = 2.0f * sclip(st[0] * 0.5f);
        st[1] = 2.0f * sclip(st[1] * 0.5f);
    }
    *lp = v2;
    *bp = v1;
    *hp = x - k * v1 - v2;
}

/* n one-pole stages (LP, or HP when hp) with negative feedback k from
 * the last output, solved without the unit delay (each stage is
 * y = a x + c, so the loop is linear in u): the oscillation threshold is
 * then the same at every cutoff (k = 4 for 4 stages). The loop input is
 * soft-clipped, which bounds a self-oscillating ladder; comp restores
 * the passband (compensated); sat soft-clips every stage. */
static float ladder(float *st, uint32_t n, float x, float G, float k, int comp, int hp, int sat) {
    uint32_t i;
    float a = hp ? 1.0f - G : G, S = 0.0f, an = 1.0f, u;
    for (i = 0u; i < n; i++) {
        float c = (hp ? -(1.0f - G) : (1.0f - G)) * st[i];
        S = S * a + c;
        an *= a;
    }
    u = (x * (comp ? 1.0f + k : 1.0f) - k * S) / (1.0f + k * an);
    u = 4.0f * sclip(u * 0.25f);
    for (i = 0u; i < n; i++) {
        float in = sat ? 1.5f * sclip(u * 0.6667f) : u, l = op1(&st[i], in, G);
        u = hp ? in - l : l;
    }
    return u;
}

/* Vowel formants F1-F3 (Hz, generic phonetics averages) for A E I O U,
 * and eight own vowel orders. */
static const float VOWEL_F[5][3] = {
    { 800.0f, 1150.0f, 2800.0f }, { 400.0f, 2000.0f, 2550.0f }, { 300.0f, 2300.0f, 3000.0f },
    { 450.0f, 800.0f, 2830.0f }, { 325.0f, 700.0f, 2530.0f }
};
static const uint8_t VOWEL_ORD[8][5] = {
    { 0, 1, 2, 3, 4 }, { 4, 3, 2, 1, 0 }, { 0, 3, 4, 1, 2 }, { 2, 1, 0, 3, 4 },
    { 1, 2, 0, 4, 3 }, { 3, 0, 1, 4, 2 }, { 4, 2, 3, 0, 1 }, { 1, 4, 0, 2, 3 }
};

static float vowel(struct RILeviVoice *v, float *df, float x, float sr, float pos, float reso) {
    static const float GAIN[3] = { 1.0f, 0.6f, 0.3f };
    const uint8_t *ord = VOWEL_ORD[v->vorder & 7u];
    float p = pos * 4.0f, fr, size = ri_pow2(((float)v->dmorph - 64.0f) / 128.0f), k, y = 0.0f;
    uint32_t a, j;
    if (p < 0.0f)
        p = 0.0f;
    if (p > 4.0f)
        p = 4.0f;
    a = (uint32_t)p;
    if (a > 3u)
        a = 3u;
    fr = p - (float)a;
    k = 0.5f - 0.45f * reso;
    {
        uint32_t base = (df == v->dfR ? 7u : 4u);
        for (j = 0u; j < 3u; j++) {
            float f = (VOWEL_F[ord[a]][j] + (VOWEL_F[ord[a + 1u]][j] - VOWEL_F[ord[a]][j]) * fr) * size, lp, bp, hp;
            svf(&df[2u * j], x, tpt_g_memo(v, base + j, f, sr), k, 0, &lp, &bp, &hp);
            y += GAIN[j] * k * bp * 2.0f;
        }
    }
    return y;
}

static float drive_sat(float x, uint8_t d) {       /* 0 = bypass (exact) */
    float g;
    if (!d)
        return x;
    g = 1.0f + (float)d * (7.0f / 127.0f);
    return sclip(x * g) * (1.0f + (float)d / 254.0f);
}

/* Digital filter: model t at cutoff fc (vowel: fc places the vowel). */
static float dfilt_step(struct RILeviVoice *v, float *df, float x, float sr, float fc, float reso) {
    uint32_t t = v->dtype < RI_LEVI_NDF ? v->dtype : RI_LEVI_DF_LP_12;
    int morph = t == RI_LEVI_DF_SVF_LBH || t == RI_LEVI_DF_SVF_LNH || t == RI_LEVI_DF_VOWEL;
    float g, G, k, lp, bp, hp, y, m = (float)v->dmorph / 127.0f, r = reso < 0.0f ? 0.0f : reso > 1.0f ? 1.0f : reso;
    if (!morph && !v->dpost)
        x = drive_sat(x, v->dmorph);
    if (t == RI_LEVI_DF_VOWEL) {
        float pos = ri_log2(fc / 40.0f) / 8.5f;
        y = vowel(v, df, x, sr, pos, r);
    } else {
        g = tpt_g_memo(v, df == v->dfR ? 1u : 0u, fc, sr);
        G = g / (1.0f + g);
        k = 2.0f - 1.98f * r;
        switch (t) {
        case RI_LEVI_DF_SVF_LBH: case RI_LEVI_DF_SVF_LNH:
            svf(df, x, g, k, 0, &lp, &bp, &hp);
            if (t == RI_LEVI_DF_SVF_LNH)
                bp = lp + hp;                   /* notch in the middle */
            y = m < 0.5f ? lp + (bp - lp) * (2.0f * m) : bp + (hp - bp) * (2.0f * m - 1.0f);
            break;
        case RI_LEVI_DF_HP_GRIT:
            svf(df, 1.5f * x, g, k - 0.03f, 1, &lp, &bp, &hp);
            y = hp * 0.6667f;
            break;
        case RI_LEVI_DF_HP_MOD: y = ladder(df, 4u, x, G, 4.1f * r, 0, 1, 1); break;
        case RI_LEVI_DF_HP_12: svf(df, x, g, k, 0, &lp, &bp, &hp); y = hp; break;
        case RI_LEVI_DF_BP_MOD: svf(df, x, g, k, 1, &lp, &bp, &hp); y = k * bp; break;
        case RI_LEVI_DF_BP_12: {                /* 6 dB HP into 6 dB LP, positive feedback (ZDF) */
            float kb = 2.05f * r, a = G * (1.0f - G);
            float S = G * (-(1.0f - G) * df[0]) + (1.0f - G) * df[1];
            float u = (x + kb * S) / (1.0f - kb * a);
            u = 4.0f * sclip(u * 0.25f);
            u = u - op1(&df[0], u, G);
            y = 2.0f * op1(&df[1], u, G);
            break;
        }
        /* 12 dB "ladders": resonant 2-pole cores; uncompensated loses
         * bass as resonance rises, compensated keeps it (p. 63). */
        case RI_LEVI_DF_LP_L12: svf(df, x, g, k, 0, &lp, &bp, &hp); y = lp / (1.0f + 2.0f * r); break;
        case RI_LEVI_DF_LP_L24: y = ladder(df, 4u, x, G, 4.1f * r, 0, 0, 0); break;
        case RI_LEVI_DF_LP_F12: svf(df, x, g, k, 1, &lp, &bp, &hp); y = lp; break;
        case RI_LEVI_DF_LP_F24: y = ladder(df, 4u, x, G, 4.1f * r, 1, 0, 0); break;
        case RI_LEVI_DF_LP_GATE: {              /* amplitude follows the cutoff */
            float a = ri_log2(fc / 40.0f) / 8.5f;
            a = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
            svf(df, x, g, k, 0, &lp, &bp, &hp);
            y = lp * a * a;
            break;
        }
        case RI_LEVI_DF_LP_GRIT:                /* hard-clipped states, hot input */
            svf(df, 1.5f * x, g, k - 0.03f, 1, &lp, &bp, &hp);
            y = lp * 0.6667f;
            break;
        case RI_LEVI_DF_LP_MOD: y = ladder(df, 4u, x, G, 4.1f * r, 0, 0, 1); break;
        case RI_LEVI_DF_LP_6: y = op1(&df[0], x, G); break;
        case RI_LEVI_DF_LP_48: y = ladder(df, 8u, x, G, 1.95f * r, 0, 0, 0); break;
        default: svf(df, x, g, k, 0, &lp, &bp, &hp); y = lp; break;   /* LP 12 */
        }
    }
    if (!morph && v->dpost)
        y = drive_sat(y, v->dmorph);
    return y;
}

/* Analog 4-pole: pre-drive (gain-compensated, small-signal unity) into
 * the ladder; the soft-clipped feedback self-oscillates near 110/128. */
static float afilt_step(struct RILeviVoice *v, uint32_t slot, float *af, float x, float sr, float fc, float reso,
    float drive) {
    float g = tpt_g_memo(v, slot, fc, sr), G = g / (1.0f + g), r = reso < 0.0f ? 0.0f : reso > 1.0f ? 1.0f : reso;
    if (drive > 0.0f) {
        float pg = 1.0f + 6.0f * drive;
        x = 3.0f * sclip(x * pg / 3.0f) / pg * (1.0f + 0.5f * drive);
    }
    return ladder(af, 4u, x, G, RI_LEVI_AF_KMAX * r, 0, 0, 0);
}

static void filt_clear(struct RILeviVoice *v) {
    uint32_t i;
    for (i = 0u; i < 12u; i++)
        v->df[i] = v->dfR[i] = 0.0f;
    for (i = 0u; i < 5u; i++)
        v->af[i] = v->afR[i] = 0.0f;
    /* Control-rate + bank-skip reset (owner 2026-10-06): the first sample
     * of a note recomputes fresh targets, so a retrigger never smears
     * pitch/filter from the previous note. Legato retune (alloc_retune)
     * deliberately skips this: no envelope restart, pitch keeps sliding. */
    v->ctl_init = 0u;
    v->ctl_k = 0u;
    v->morph_hold = 0u;
    v->bank_skipped = 0u;
}

static void kt_update(struct RILeviVoice *v) {
    float oct = ((float)v->note - 36.0f) / 12.0f;   /* C2 = centre */
    v->dktm = v->dkt == 0.0f ? 1.0f : ri_pow2(oct * v->dkt);
    v->aktm = v->akt == 0.0f ? 1.0f : ri_pow2(oct * v->akt);
}

static const char *const DF_NAME[RI_LEVI_NDF] = {
    "SVF LP-BP-HP", "SVF LP-NO-HP", "HP GRIT", "HP MOD", "HP 12", "BP MOD", "BP 12", "LP L12", "LP L24",
    "LP F12", "LP F24", "LP GATE", "LP GRIT", "LP MOD", "LP 12", "LP 6", "LP 48", "VOWEL"
};

const char *ri_levi_df_name(uint32_t t) {
    return t < RI_LEVI_NDF ? DF_NAME[t] : "";
}

/* Ribbon touch/move/release (fidelity P8d, manual pp. 97-98). Touch
 * starts trig-7 menvs, retunes theremin voices and jumps the seq grid;
 * move retunes; release ends trig-8 menvs. */
static void ribbon_theremin(struct RILeviSet *s) {
    uint32_t v;
    uint8_t note;
    if (!s->rbn_touch || s->rbn_mode != 3u)
        return;
    note = s->rbn_pos > 127u ? 127u : s->rbn_pos;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        if (s->v[v].active)
            alloc_retune(&s->v[v], note, (int)s->p_glidehold);
}

int levi_ribbon_touch(struct RILeviSet *s, uint8_t pos) {
    uint32_t v, e;
    uint32_t trklen;
    if (!s)
        return 2;
    if (pos > 127u)
        pos = 127u;
    s->rbn_touch = 1u;
    s->rbn_pos = pos;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        for (e = 0u; e < RI_LEVI_NMENV; e++)
            if (menv_has(&s->v[v], e, 7u))
                menv_start(&s->v[v].menv[e]);
    ribbon_theremin(s);
    /* Seq step selector: jump the grid to the touched position. */
    trklen = 1u + s->seqtrklen;
    if (trklen > RI_LEVI_SEQ_STEPS)
        trklen = RI_LEVI_SEQ_STEPS;
    s->seq_lastk = (int64_t)((uint64_t)pos * trklen / 128u) - 1;
    return 0;
}

int levi_ribbon_move(struct RILeviSet *s, uint8_t pos) {
    if (!s)
        return 2;
    s->rbn_pos = pos > 127u ? 127u : pos;
    ribbon_theremin(s);
    return 0;
}

int levi_ribbon_release(struct RILeviSet *s) {
    uint32_t v, e;
    if (!s)
        return 2;
    s->rbn_touch = 0u;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        for (e = 0u; e < RI_LEVI_NMENV; e++)
            if (menv_has(&s->v[v], e, 8u))
                menv_release(&s->v[v].menv[e]);
    return 0;
}

int levi_set_param_ui(struct RILeviSet *s, uint32_t voice, uint32_t id,
    uint8_t val) {
    float f;
    if (!s || voice >= RI_LEVI_NVOICES)
        return 2;
    /* ENV BPM flags (fidelity P8a): per-op 0x93..9A, per-menv 0x9B..9F,
     * section-wide apply to every voice (all voices share the flags). */
    if (id >= (RI_CTL_LEVI_OPBPM0 & 0xFFu) && id <= (RI_CTL_LEVI_OPBPM7 & 0xFFu)) {
        uint32_t w;
        for (w = 0u; w < RI_LEVI_NVOICES; w++)
            s->v[w].opbpm[id - (RI_CTL_LEVI_OPBPM0 & 0xFFu)] = val ? 1u : 0u;
        return 0;
    }
    if (id >= (RI_CTL_LEVI_MEBPM0 & 0xFFu) && id <= (RI_CTL_LEVI_MEBPM4 & 0xFFu)) {
        uint32_t w;
        for (w = 0u; w < RI_LEVI_NVOICES; w++)
            s->v[w].mebpm[id - (RI_CTL_LEVI_MEBPM0 & 0xFFu)] = val ? 1u : 0u;
        return 0;
    }
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
    case (RI_CTL_LEVI_ARPOCTMODE & 0xFFu):
        s->arpoctmode = val > 2u ? 2u : val;
        return 0;
    case (RI_CTL_LEVI_ARPOCTRANGE & 0xFFu):
        s->arpoctrange = val;
        return 0;
    case (RI_CTL_LEVI_ARPGATE & 0xFFu):
        s->arpgate = val;
        return 0;
    case (RI_CTL_LEVI_ARPMODE & 0xFFu):
        s->arpmode = val > RI_LEVI_ARP_NMODES - 1u ? RI_LEVI_ARP_NMODES - 1u : val;
        return 0;
    case (RI_CTL_LEVI_ARPLEN & 0xFFu):
        s->arplen = val;
        return 0;
    case (RI_CTL_LEVI_ARPPHRASE & 0xFFu):
        s->arpphrase = val;
        return 0;
    case (RI_CTL_LEVI_ARPENTROPY & 0xFFu):
        s->arpentropy = val;
        return 0;
    case (RI_CTL_LEVI_ARPSWING & 0xFFu):
        s->arpswing = val;
        return 0;
    case (RI_CTL_LEVI_ARPRATCHET & 0xFFu):
        s->arpratchet = val;
        return 0;
    case (RI_CTL_LEVI_ARPCHANCE & 0xFFu):
        s->arpchance = val;
        return 0;
    case (RI_CTL_LEVI_ARPLATCH & 0xFFu):
        s->arplatch = val ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_ARPCLOCK & 0xFFu):
        s->arpclock = val ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_ARPSTEPPOFF & 0xFFu):
        s->arpstepoff = val;
        return 0;
    case (RI_CTL_LEVI_SEQRATE & 0xFFu):
        s->seqrate = val;
        return 0;
    case (RI_CTL_LEVI_SEQMODE & 0xFFu):
        s->seqmode = val > 2u ? 2u : val;
        return 0;
    case (RI_CTL_LEVI_SEQSWING & 0xFFu):
        s->seqswing = val;
        return 0;
    case (RI_CTL_LEVI_SEQGATE & 0xFFu):
        s->seqgate = val;
        return 0;
    case (RI_CTL_LEVI_SEQPROB & 0xFFu):
        s->seqprob = val;
        return 0;
    case (RI_CTL_LEVI_SEQDRIFT & 0xFFu):
        s->seqdrift = val;
        return 0;
    case (RI_CTL_LEVI_SEQTRANSP & 0xFFu):
        s->seqtransp = val;
        return 0;
    case (RI_CTL_LEVI_SEQTRKLEN & 0xFFu):
        s->seqtrklen = val;
        return 0;
    case (RI_CTL_LEVI_SEQREC & 0xFFu):
        s->seqrec = val ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_SEQSTEP & 0xFFu):
        s->seqstep = val > RI_LEVI_SEQ_STEPS - 1u ? RI_LEVI_SEQ_STEPS - 1u : val;
        return 0;
    case (RI_CTL_LEVI_SEQCLEAR & 0xFFu):
        if (val) {
            uint32_t st, nn, m, g;
            for (st = 0u; st < RI_LEVI_SEQ_STEPS; st++) {
                for (nn = 0u; nn < RI_LEVI_SEQ_NOTES; nn++)
                    s->seq_t1[st].note[nn] = s->seq_t2[st].note[nn] = 255u;
                for (m = 0u; m < 8u; m++)
                    s->seq_macro[m][st] = 0u;
            }
            s->seq_npend = 0u;
            for (g = 0u; g < RI_LEVI_NVOICES; g++)
                s->seq_gate[g] = -1;
        }
        return 0;
    case (RI_CTL_LEVI_SEQSTRIG & 0xFFu):
        s->seqstrig = val;
        return 0;
    case (RI_CTL_LEVI_SEQSPROB & 0xFFu):
        s->seqsprob = val;
        return 0;
    case (RI_CTL_LEVI_SEQSDRIFT & 0xFFu):
        s->seqsdrift = val;
        return 0;
    case (RI_CTL_LEVI_SEQSENTR & 0xFFu):
        s->seqsentr = val;
        return 0;
    case (RI_CTL_LEVI_RBNMODE & 0xFFu):
        s->rbn_mode = val > 3u ? 3u : val;
        return 0;
    case (RI_CTL_LEVI_RBNPOS & 0xFFu):
        s->rbn_pos = val > 127u ? 127u : val;
        return 0;
    case (RI_CTL_LEVI_RBNTOUCH & 0xFFu):
        if (val)
            return levi_ribbon_touch(s, s->rbn_pos);
        return levi_ribbon_release(s);
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
        ri_levi_matrix_refresh_empty(&s->mx);
        return 0;
    case (RI_CTL_LEVI_LFO0RATE & 0xFFu): case (RI_CTL_LEVI_LFO1RATE & 0xFFu):
    case (RI_CTL_LEVI_LFO2RATE & 0xFFu): case (RI_CTL_LEVI_LFO3RATE & 0xFFu):
    case (RI_CTL_LEVI_LFO4RATE & 0xFFu):
        /* LFO rate (v2 feature 4d, automation-only: no panel knob yet,
         * 303-VOLUME precedent). */
        s->v[voice].lfo[id - (RI_CTL_LEVI_LFO0RATE & 0xFFu)].rate =
            ri_levi_lfo_rate(val > 127u ? 127u : val);
        return 0;
    case (RI_CTL_LEVI_DTYPE & 0xFFu): {
        struct RILeviVoice *v = &s->v[voice];
        uint8_t t = val < RI_LEVI_NDF ? val : RI_LEVI_NDF - 1u;
        if (t != v->dtype)
            filt_clear(v);                    /* new topology starts clean */
        v->dtype = t;
        return 0;
    }
    case (RI_CTL_LEVI_DMORPH & 0xFFu): s->v[voice].dmorph = val > 127u ? 127u : val; return 0;
    case (RI_CTL_LEVI_DPOST & 0xFFu): s->v[voice].dpost = val ? 1u : 0u; return 0;
    case (RI_CTL_LEVI_VORDER & 0xFFu): s->v[voice].vorder = val & 7u; return 0;
    case (RI_CTL_LEVI_DKEYTRK & 0xFFu): case (RI_CTL_LEVI_AKEYTRK & 0xFFu): {
        /* 64 = 0 %, 32 steps per 100 %, -200..+196 % (pp. 64, 67). */
        float kt = ((float)(val > 127u ? 127u : val) - 64.0f) / 32.0f;
        if (id == (RI_CTL_LEVI_DKEYTRK & 0xFFu))
            s->v[voice].dkt = kt;
        else
            s->v[voice].akt = kt;
        kt_update(&s->v[voice]);
        return 0;
    }
    case (RI_CTL_LEVI_DLFO1 & 0xFFu): case (RI_CTL_LEVI_ALFO2 & 0xFFu): case (RI_CTL_LEVI_VLFO3 & 0xFFu): {
        float a = ((float)(val > 127u ? 127u : val) - 64.0f) / 63.0f;
        if (a < -1.0f)
            a = -1.0f;
        if (id == (RI_CTL_LEVI_DLFO1 & 0xFFu))
            s->v[voice].dlfo = a;
        else if (id == (RI_CTL_LEVI_ALFO2 & 0xFFu))
            s->v[voice].alfo = a;
        else
            s->v[voice].vlfo = a;
        return 0;
    }
    case (RI_CTL_LEVI_DLEVEL & 0xFFu): case (RI_CTL_LEVI_OSCLVL & 0xFFu):
    case (RI_CTL_LEVI_VCALVL & 0xFFu): case (RI_CTL_LEVI_PATCHLVL & 0xFFu): {
        /* 64 = unity (p. 68), 127 ~ +6 dB, 0 = silent. */
        float g = (float)(val > 127u ? 127u : val) / 64.0f;
        struct RILeviVoice *v = &s->v[voice];
        if (id == (RI_CTL_LEVI_DLEVEL & 0xFFu))
            v->dlevel = g;
        else if (id == (RI_CTL_LEVI_OSCLVL & 0xFFu))
            v->osclvl = g;
        else if (id == (RI_CTL_LEVI_VCALVL & 0xFFu))
            v->vcalvl = g;
        else
            v->patchlvl = g;
        return 0;
    }
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
    {
        /* v1 shape key: 0 sine, 1 = the 3-step wave (P5: STEP, 3 steps). */
        uint32_t l = id - (RI_CTL_LEVI_LFO0SHAPE & 0xFFu);
        if (val != 0u)
            levi_set_lfo_ui(s, voice, l, RI_LEVI_LP_STEPS, 3u);
        levi_set_lfo_ui(s, voice, l, RI_LEVI_LP_WAVE, val != 0u ? RI_LEVI_LW_STEP : RI_LEVI_LW_SINE);
        return 0;
    }
    case (RI_CTL_LEVI_DENV1 & 0xFFu): case (RI_CTL_LEVI_AENV2 & 0xFFu): {
        float a = ((float)(val > 127u ? 127u : val) - 64.0f) / 63.0f;
        if (a < -1.0f)
            a = -1.0f;
        if (id == (RI_CTL_LEVI_DENV1 & 0xFFu))
            s->v[voice].denv = a;
        else
            s->v[voice].aenv = a;
        return 0;
    }
    case (RI_CTL_LEVI_DVEL & 0xFFu): case (RI_CTL_LEVI_DPAT & 0xFFu):
    case (RI_CTL_LEVI_AVEL & 0xFFu): case (RI_CTL_LEVI_APAT & 0xFFu):
    case (RI_CTL_LEVI_VVEL & 0xFFu): case (RI_CTL_LEVI_VPAT & 0xFFu): {
        /* Performance amounts (fidelity P9b): the P5 ENV-amount scale,
         * UI 64 = none. Velocity reads bipolar about mid, pressure
         * unipolar (the caller returns it to 0). */
        float a = ((float)(val > 127u ? 127u : val) - 64.0f) / 63.0f;
        if (a < -1.0f)
            a = -1.0f;
        if (id == (RI_CTL_LEVI_DVEL & 0xFFu))
            s->v[voice].dvel = a;
        else if (id == (RI_CTL_LEVI_DPAT & 0xFFu))
            s->v[voice].dpat = a;
        else if (id == (RI_CTL_LEVI_AVEL & 0xFFu))
            s->v[voice].avel = a;
        else if (id == (RI_CTL_LEVI_APAT & 0xFFu))
            s->v[voice].apat = a;
        else if (id == (RI_CTL_LEVI_VVEL & 0xFFu))
            s->v[voice].vvel = a;
        else
            s->v[voice].vpat = a;
        return 0;
    }
    case (RI_CTL_LEVI_PFOCT & 0xFFu): case (RI_CTL_LEVI_PFMODE & 0xFFu):
    case (RI_CTL_LEVI_PFSEL & 0xFFu): case (RI_CTL_LEVI_PFSPLIT & 0xFFu):
    case (RI_CTL_LEVI_PFBAL & 0xFFu): {
        /* Keyboard zones (fidelity P9c): device-wide fields, so the
         * voice index is ignored and the section-wide 0x0E loop is
         * idempotent (the same value eight times). */
        static const uint32_t FIELD[5] = { RI_LEVI_PF_OCT, RI_LEVI_PF_MODE,
            RI_LEVI_PF_SELECT, RI_LEVI_PF_SPLITM, RI_LEVI_PF_BALANCE };
        uint32_t i;
        uint32_t base = RI_CTL_LEVI_PFOCT & 0xFFu;
        for (i = 0u; i < 5u; i++)
            if (id == base + i)
                return levi_perf_set(s, FIELD[i], (int)val);
        return 2;
    }
    case (RI_CTL_LEVI_GLIDE & 0xFFu):
        /* Performance buttons (fidelity P9d): device-wide like the zone
         * rows, so the voice index is ignored and the section-wide 0x0E
         * loop is idempotent. */
        return levi_glide_hold(s, val ? 1 : 0);
    case (RI_CTL_LEVI_CHORD & 0xFFu):
        return levi_chord_mode(s, val ? 1 : 0);
    case (RI_CTL_LEVI_MKNOB0 & 0xFFu): case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 1u:
    case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 2u: case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 3u:
    case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 4u: case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 5u:
    case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 6u: case (RI_CTL_LEVI_MKNOB0 & 0xFFu) + 7u:
        s->mx.mknob[id - (RI_CTL_LEVI_MKNOB0 & 0xFFu)] = val > 127u ? 127u : val;
        return 0;
    case (RI_CTL_LEVI_MBTN0 & 0xFFu): case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 1u:
    case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 2u: case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 3u:
    case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 4u: case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 5u:
    case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 6u: case (RI_CTL_LEVI_MBTN0 & 0xFFu) + 7u:
        s->mx.mbtn[id - (RI_CTL_LEVI_MBTN0 & 0xFFu)] = val ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_VINIT & 0xFFu):
        s->v[voice].vinit = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_POLYMODE & 0xFFu):
        return levi_set_alloc_ui(s, val > RI_LEVI_POLY_N - 1u ? RI_LEVI_POLY_N - 1u : val);
    case (RI_CTL_LEVI_UDENSITY & 0xFFu): {
        uint32_t d = (uint32_t)val * 8u / 127u + 1u;
        s->udensity = (uint8_t)(d > 8u ? 8u : d);
        return 0;
    }
    case (RI_CTL_LEVI_ULIMIT & 0xFFu): {
        uint32_t l = (uint32_t)val * (uint32_t)RI_LEVI_NVOICES / 127u + 1u;
        s->ulimit = (uint8_t)(l > RI_LEVI_NVOICES ? RI_LEVI_NVOICES : l);
        return 0;
    }
    case (RI_CTL_LEVI_VDETUNE & 0xFFu):
        s->v[voice].vdetune = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VAFEEL & 0xFFu):
        s->v[voice].vafeel = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VRNDPH & 0xFFu):
        s->v[voice].vrndph = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VPAN & 0xFFu): {
        float p = ((float)(val > 127u ? 127u : val) - 64.0f) / 63.0f;
        s->v[voice].vpan = p < -1.0f ? -1.0f : p > 1.0f ? 1.0f : p;
        return 0;
    }
    case (RI_CTL_LEVI_VWIDTH & 0xFFu):
        s->v[voice].vwidth = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VPANMODE & 0xFFu):
        s->v[voice].vpanmode = val > 2u ? 2u : val;
        return 0;
    case (RI_CTL_LEVI_VBENDRNG & 0xFFu):
        s->v[voice].vbendrng = (float)(val > 127u ? 127u : val) * 24.0f / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VVIBRATE & 0xFFu):
        s->v[voice].vvibrate = 0.1f * ri_pow2((float)(val > 127u ? 127u : val) / 127.0f * 7.6439f);
        return 0;
    case (RI_CTL_LEVI_VVIBAMT & 0xFFu):
        s->v[voice].vvibamt = (float)(val > 127u ? 127u : val) * 4.0f / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VVIBDLY & 0xFFu):
        s->v[voice].vvibdly = (float)(val > 127u ? 127u : val) * 5.0f / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VGLIDE & 0xFFu):
        s->v[voice].vglide = val == 0u ? 0u : val < 64u ? 1u : 2u;
        return 0;
    case (RI_CTL_LEVI_VGLTIME & 0xFFu): {
        float u = (float)(val > 127u ? 127u : val) / 127.0f;
        s->v[voice].vgltime = 5.0f * u * u;
        return 0;
    }
    case (RI_CTL_LEVI_VGLCURVE & 0xFFu):
        s->v[voice].vglcurve = ri_pow2(((float)(val > 127u ? 127u : val) - 64.0f) / 32.0f);
        return 0;
    case (RI_CTL_LEVI_VINTAGE & 0xFFu): {
        /* Own law: bits 16..1 linear, decimation 1..32 quadratic. */
        uint32_t b = val > 127u ? 127u : val;
        struct RILeviVoice *v = &s->v[voice];
        v->vint_bits = (uint8_t)(16u - b * 15u / 127u);
        v->vint_dec = (uint8_t)(1u + b * b * 31u / (127u * 127u));
        return 0;
    }
    case (RI_CTL_LEVI_VSCALE & 0xFFu):
        s->v[voice].vscale = val > RI_LEVI_NSCALES - 1u ? RI_LEVI_NSCALES - 1u : val;
        return 0;
    case (RI_CTL_LEVI_VMICRO & 0xFFu):
        s->v[voice].vmicro = val > RI_LEVI_NMICRO - 1u ? RI_LEVI_NMICRO - 1u : val;
        return 0;
    case (RI_CTL_LEVI_VKEYLOCK & 0xFFu):
        s->v[voice].vkeylock = val ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_VSPREAD & 0xFFu):
        s->v[voice].vspread = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu): case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 1u:
    case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 2u: case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 3u:
    case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 4u: case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 5u:
    case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 6u: case (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 7u: {
        float p = ((float)(val > 127u ? 127u : val) - 64.0f) / 63.0f;
        s->v[voice].oppan[id - (RI_CTL_LEVI_VOSCPAN1 & 0xFFu)] =
            p < -1.0f ? -1.0f : p > 1.0f ? 1.0f : p;
        return 0;
    }
    case (RI_CTL_LEVI_DLYTYPE & 0xFFu):
        s->fx.dtype = val > RI_LEVI_DT_N - 1u ? RI_LEVI_DT_N - 1u : val;
        return 0;
    case (RI_CTL_LEVI_DLYTIME & 0xFFu):
        s->fx.dtime = ri_levi_delay_time(val);
        return 0;
    case (RI_CTL_LEVI_DLYFB & 0xFFu):
        s->fx.dfb = (float)(val > 127u ? 127u : val) / 127.0f * 0.95f;
        return 0;
    case (RI_CTL_LEVI_DLYWTONE & 0xFFu):
        s->fx.dwtone = 200.0f * ri_pow2((float)(val > 127u ? 127u : val) / 127.0f * 6.4919f);
        return 0;
    case (RI_CTL_LEVI_DLYFBTONE & 0xFFu):
        s->fx.dfbtone = 100.0f * ri_pow2((float)(val > 127u ? 127u : val) / 127.0f * 6.3219f);
        return 0;
    case (RI_CTL_LEVI_DLYDRYWET & 0xFFu):
        s->fx.ddrywet = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_DLYBPM & 0xFFu):
        s->fx.dbpm = val ? 1u : 0u;   /* stored; sync goes live with the P8 clock */
        return 0;
    case (RI_CTL_LEVI_DBYPASS & 0xFFu):
        s->fx.dbypass = val ? 0u : 1u;  /* panel ON (FXDLY) vs engine bypass */
        return 0;
    case (RI_CTL_LEVI_RTYPE & 0xFFu):
        s->fx.rtype = val > RI_LEVI_RT_N - 1u ? RI_LEVI_RT_N - 1u : val;
        return 0;
    case (RI_CTL_LEVI_RPREDLY & 0xFFu):
        s->fx.rpredly = (float)(val > 127u ? 127u : val) / 127.0f * 0.25f;
        return 0;
    case (RI_CTL_LEVI_RTIME & 0xFFu):
        s->fx.rtime = (float)(val > 127u ? 127u : val) / 127.0f * 0.95f;
        return 0;
    case (RI_CTL_LEVI_RTONE & 0xFFu):
        s->fx.rtone = 200.0f * ri_pow2((float)(val > 127u ? 127u : val) / 127.0f * 6.4919f);
        return 0;
    case (RI_CTL_LEVI_RHIDAMP & 0xFFu):
        s->fx.rhidamp = 200.0f * ri_pow2((float)(val > 127u ? 127u : val) / 127.0f * 6.4919f);
        return 0;
    case (RI_CTL_LEVI_RLODAMP & 0xFFu):
        s->fx.rlodamp = 20.0f * ri_pow2((float)(val > 127u ? 127u : val) / 127.0f * 4.6439f);
        return 0;
    case (RI_CTL_LEVI_RDRYWET & 0xFFu):
        s->fx.rdrywet = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_RFREEZE & 0xFFu):
        s->fx.rfreeze = val ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_RBYPASS & 0xFFu):
        s->fx.rbypass = val ? 0u : 1u;  /* panel ON (FXREV) vs engine bypass */
        return 0;
    case (RI_CTL_LEVI_PTYPE & 0xFFu):
        s->fx.pre.type = val > RI_LEVI_MT_N - 1u ? RI_LEVI_MT_N - 1u : val;
        return 0;
    case (RI_CTL_LEVI_PPRESET & 0xFFu): {
        uint8_t q1 = 0u, q2 = 0u, qw = 0u;
        s->fx.pre.preset = val > 3u ? 3u : val;
        ri_levi_mod_preset(s->fx.pre.type, s->fx.pre.preset, &q1, &q2, &qw);
        s->fx.pre.p1 = (float)q1 / 127.0f;
        s->fx.pre.p2 = (float)q2 / 127.0f;
        s->fx.pre.drywet = (float)qw / 127.0f;
        return 0;
    }
    case (RI_CTL_LEVI_PP1 & 0xFFu):
        s->fx.pre.p1 = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_PP2 & 0xFFu):
        s->fx.pre.p2 = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_PDRYWET & 0xFFu):
        s->fx.pre.drywet = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_PREBYPASS & 0xFFu):
        s->fx.pre.bypass = val ? 0u : 1u;  /* panel ON (FXPRE) vs engine bypass */
        return 0;
    case (RI_CTL_LEVI_OTYPE & 0xFFu):
        s->fx.post.type = val > RI_LEVI_MT_N - 1u ? RI_LEVI_MT_N - 1u : val;
        return 0;
    case (RI_CTL_LEVI_OPRESET & 0xFFu): {
        uint8_t q1 = 0u, q2 = 0u, qw = 0u;
        s->fx.post.preset = val > 3u ? 3u : val;
        ri_levi_mod_preset(s->fx.post.type, s->fx.post.preset, &q1, &q2, &qw);
        s->fx.post.p1 = (float)q1 / 127.0f;
        s->fx.post.p2 = (float)q2 / 127.0f;
        s->fx.post.drywet = (float)qw / 127.0f;
        return 0;
    }
    case (RI_CTL_LEVI_OP1 & 0xFFu):
        s->fx.post.p1 = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_OP2 & 0xFFu):
        s->fx.post.p2 = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_ODRYWET & 0xFFu):
        s->fx.post.drywet = (float)(val > 127u ? 127u : val) / 127.0f;
        return 0;
    case (RI_CTL_LEVI_POSTBYPASS & 0xFFu):
        s->fx.post.bypass = val ? 0u : 1u;  /* panel ON (FXPOST) vs engine bypass */
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
/* Stereo pan gains (fidelity P6c, own laws, p in -1..1). BALANCE is
 * linear with exact 1 at center (dual-mono bit-identity); POWER is
 * equal-power via ri_sin; WIDE a steeper balance. */
#ifdef RI_LEVI_PROFILE
static void pan_gains(float p, uint32_t mode, float *lg, float *rg,
    struct RILeviVoice *v) {
    uint32_t ppb;
    memcpy(&ppb, &p, 4);
    v->prof_pan++;
    if (v->prof_panhave && v->prof_panpp == ppb && v->prof_panmode == mode)
        v->prof_panhit++;
    v->prof_panpp = ppb;
    v->prof_panmode = mode;
    v->prof_panhave = 1u;
    if (mode == 1u) {
#else
static void pan_gains(float p, uint32_t mode, float *lg, float *rg) {
    if (mode == 1u) {
#endif
        float phi = (p + 1.0f) * 0.7853982f;
        *lg = ri_sin(1.5707963f - phi);
        *rg = ri_sin(phi);
    } else if (mode == 2u) {
        float l = 1.0f - 1.5f * (p > 0.0f ? p : 0.0f);
        float r = 1.0f - 1.5f * (p < 0.0f ? -p : 0.0f);
        *lg = l < 0.0f ? 0.0f : l;
        *rg = r < 0.0f ? 0.0f : r;
    } else {
        *lg = 1.0f - (p > 0.0f ? p : 0.0f);
        *rg = 1.0f - (p < 0.0f ? -p : 0.0f);
    }
}

static float voice_pass(struct RILeviVoice *v, uint32_t bank, float sr,
    float vpitch, float vpan, float vwidth, float panoff, uint32_t pmode,
    float *mixr, int *any_on) {
    float opout[RI_LEVI_NOPS] = { 0.0f }, mix = 0.0f;
    float mixR = 0.0f;
    const uint8_t *fd = bank ? v->feedsB : v->feeds;
    const uint8_t *ord = bank ? v->orderB : v->order;
    const uint8_t *live = bank ? v->liveB : v->live;
    uint32_t k, j;
#ifdef RI_LEVI_PROFILE
    if (bank)
        v->prof_passB++;
    else
        v->prof_passA++;
#endif
    for (k = 0u; k < RI_LEVI_NOPS; k++) {
        uint32_t i = ord[k];
        struct RILeviOp *p = &v->op[i];
        struct RILeviOpState *o = &v->st[bank][i];
        float fm, pm, ph, osc, amp, fq, fbe;
        const float *m;
        uint32_t wave;
        int on, warped = 0;
        if (i >= RI_LEVI_NOPS || !live[i]) {
            opout[k & (RI_LEVI_NOPS - 1u)] = 0.0f;
            continue;
        }
#ifdef RI_LEVI_PROFILE
        v->prof_envop++;
#endif
        m = v->opm_on ? v->opm[i] : 0;                 /* matrix / macro offsets (P5b) */
        if (m) {
            float tsc[4];
            tsc[0] = v->bias_t[0] * (m[RI_LEVI_DO_ATTACK] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DO_ATTACK]) : 1.0f);
            tsc[1] = v->bias_t[1] * (m[RI_LEVI_DO_DECAY] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DO_DECAY]) : 1.0f);
            tsc[2] = v->bias_t[2] * (m[RI_LEVI_DO_RELEASE] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DO_RELEASE]) : 1.0f);
            tsc[3] = v->bias_t[3] * (m[RI_LEVI_DO_HOLD] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DO_HOLD]) : 1.0f);
            o->env.susmod = m[RI_LEVI_DO_SUSTAIN];
            on = env_tick_b(&o->env, sr, tsc);
        } else {
            on = env_tick_b(&o->env, sr, v->bias_t);
        }
        *any_on |= on;
        if (!on) {
            opout[i] = 0.0f;
            o->amp = 0.0f;
            continue;
        }
#ifdef RI_LEVI_PROFILE
        v->prof_ops++;
#endif
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
        fq = o->freq * vpitch;
        if (m) {
            /* Pitch +/-24 semitones full scale; ratio (+/-8 octaves) and
             * fine (+/-50 cents) act in ratio mode only (p. 127 note 1). */
            float semi = 24.0f * m[RI_LEVI_DO_PITCH];
            if (p->pmode == 1u)
                semi += 96.0f * m[RI_LEVI_DO_RATIO] + 0.5f * m[RI_LEVI_DO_FINE];
            if (semi != 0.0f)
                fq *= ri_pow2(semi / 12.0f);
        }
        o->phase += (fq + fq * (RI_LEVI_MOD_DEPTH * fm)) / sr;
        if (o->phase >= 1.0f)
            o->phase -= 1.0f;
        if (o->phase < 0.0f)
            o->phase += 1.0f;
        ph = o->phase;
        if (pm != 0.0f)
            ph = frac1(ph + RI_LEVI_MOD_DEPTH * pm * 0.1591549f);
        if (m && m[RI_LEVI_DO_PHASE] != 0.0f)
            ph = frac1(ph + m[RI_LEVI_DO_PHASE] + 1.0f);
        fbe = p->fb;
        if (m && m[RI_LEVI_DO_FB] != 0.0f) {
            fbe += m[RI_LEVI_DO_FB];
            fbe = fbe < 0.0f ? 0.0f : fbe > 1.0f ? 1.0f : fbe;
        }
        if (fbe > 0.0f && (p->mode == RI_LEVI_FM || p->mode == RI_LEVI_PM))
            ph = frac1(ph + fbe * 1.2f * o->last);     /* self feedback (FM/PM only) */
        if (warped)
            for (j = 0u; j < RI_LEVI_NOPS; j++)
                if (((fd[j] >> i) & 1u) && v->op[j].mode > RI_LEVI_PM && o->freq > 0.0f) {
                    float n = v->st[bank][j].freq / o->freq;
                    n = n < 0.125f ? 0.125f : n > 64.0f ? 64.0f : n;
                    ph = mod_warp(v->op[j].mode, ph, n, v->st[bank][j].amp);
                }
        wave = p->wave;
        if (m && m[RI_LEVI_DO_WAVE] != 0.0f) {
            int w = (int)p->wave + (int)(m[RI_LEVI_DO_WAVE] * 127.0f);
            wave = (uint32_t)(w < 0 ? 0 : w > 127 ? 127 : w);
        }
        osc = ri_levi_wave(wave, ph, fq / sr);
        if (p->invert)
            osc = -osc;
        {
            /* VEL > ENV (P9b): the depth scales the envelope *level* term
             * only, about mid velocity (depth 0..1); never INIT or BIAS. */
            float envl = p->venv != 0.0f ?
                p->envl * (1.0f + p->venv * (2.0f * v->vel01 - 1.0f)) : p->envl;
            if (m)
                amp = p->init + m[RI_LEVI_DO_INIT] +
                    (envl + 2.0f * m[RI_LEVI_DO_ENVL] + v->bias_envl) * env_out(&o->env);
            else
                amp = p->init + (envl + v->bias_envl) * env_out(&o->env);
        }
        if (amp < 0.0f)
            amp = 0.0f;
        if (amp > 1.0f)
            amp = 1.0f;
        o->ps = fm;
        o->last = osc;
        o->amp = amp;
        opout[i] = osc * p->level * amp;
        /* Carriers (feed nothing) and Direct Out reach the mix, Mute drops
         * an op; Solo auditions one op alone, modulator or not (p. 59).
         * Stereo (P6c): per-op pan through the mode law; NULL mixr is
         * the legacy mono path, bit for bit. */
        if (v->solo ? v->solo == i + 1u : (fd[i] == 0u || p->direct) && !((v->mute >> i) & 1u)) {
            if (!mixr) {
                mix += opout[i];
            } else {
                float mp = m ? m[RI_LEVI_DO_PAN] : 0.0f;
                float pp = vpan + (v->oppan[i] + mp) * vwidth + panoff;
                float lg, rg;
                pp = pp < -1.0f ? -1.0f : pp > 1.0f ? 1.0f : pp;
#ifdef RI_LEVI_PROFILE
                pan_gains(pp, pmode, &lg, &rg, v);
#else
                pan_gains(pp, pmode, &lg, &rg);
#endif
                mix += opout[i] * lg;
                mixR += opout[i] * rg;
            }
        }
    }
    if (mixr)
        *mixr = mixR;
    return mix;
}

/* Matrix + macro evaluation for one voice sample (fidelity P5b): builds
 * the source vector (previous-sample oscillator contours, this sample's
 * LFOs and envelopes, keytrack; performance sources read 0 until their
 * data arrives), evaluates, and folds each contribution into the
 * effective values. Own laws: a contribution x (-1..1 full scale) moves
 * a parameter by x times its span (cutoffs 8.5 octaves, levels as a
 * 1 + x factor, times 2^(8x)). */
static float clampf(float x, float lo, float hi) {
    return x < lo ? lo : x > hi ? hi : x;
}

#ifdef RI_LEVI_PROFILE
/* Host-only work counters (levi-perf P1). All no-ops when undefined. */
uint64_t ri_prof_tptg = 0u;
void ri_prof_tptg_reset(void) {
    ri_prof_tptg = 0u;
}
void levi_profile_reset(struct RILeviSet *s) {
    uint32_t i;
    ri_prof_tptg = 0u;
    if (!s)
        return;
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        struct RILeviVoice *v = &s->v[i];
        v->prof_ops = 0u; v->prof_passA = 0u; v->prof_passB = 0u;
        v->prof_tptg = 0u; v->prof_tptghit = 0u; v->prof_modapply = 0u;
        v->prof_mrows = 0u; v->prof_lfo = 0u; v->prof_envop = 0u;
        v->prof_envmod = 0u; v->prof_chain = 0u; v->prof_dual = 0u;
        v->prof_pan = 0u; v->prof_panhit = 0u;
        v->prof_have = 0u; v->prof_panhave = 0u;
    }
}
/* Filter-coefficient memo check: dc/ac bit repeat per voice (H2 prize). */
static void prof_memo_check(struct RILeviVoice *v, float dc, float ac) {
    uint32_t db, ab;
    memcpy(&db, &dc, 4);
    memcpy(&ab, &ac, 4);
    if (v->prof_have && v->prof_dc == db && v->prof_ac == ab)
        v->prof_tptghit++;
    v->prof_dc = db;
    v->prof_ac = ab;
    v->prof_have = 1u;
}
#endif

static void levi_mod_apply(struct RILeviVoice *v, const struct RILeviMatrix *mx, const float *lfo5, float *ecut,
    float *ereso, float *edm, float *edenv, float *edlfo, float *edlevel, float *eacut, float *eareso, float *edrive,
    float *eaenv, float *ealfo, float *evlevel, float *evlfo, float *eoplevel, float *emorph) {
    float src[RI_LEVI_MS_N];
    struct RILeviModOut out[RI_LEVI_MODOUT_MAX];
    uint32_t n, i, o, touch_op = 0u, touch_me = 0u, touch_vo = 0u, touch_fx = 0u, touch_rv = 0u, touch_px = 0u,
        touch_ox = 0u, touch_ax = 0u, touch_sx = 0u;
    if (mx->mx_empty) {
        /* P2 C3: no slot or macro route can emit, so eval2 yields no rows.
         * Reproduce the full path's unconditional effects bit for bit:
         * LFO rate/level/smooth/step resets, stale-route clears, melmod
         * zeroing and the e* clamps (which move out-of-range bases). */
        for (o = 0u; o < RI_LEVI_NLFO; o++) {
            v->lfo[o].rmul = 1.0f;
            v->lfo[o].lmod = v->lfo[o].smod = v->lfo[o].stmod = 0.0f;
        }
        if (v->opm_on) {
            memset(v->opm, 0, sizeof v->opm);
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                v->st[0][o].env.susmod = v->st[1][o].env.susmod = 0.0f;
        }
        if (v->mem_on) {
            memset(v->mem, 0, sizeof v->mem);
            for (o = 0u; o < RI_LEVI_NMENV; o++) {
                v->menv[o].susmod = 0.0f;
                v->menv[o].cmod[0] = v->menv[o].cmod[1] = v->menv[o].cmod[2] = 0;
            }
        }
        if (v->vom_on)
            memset(v->vom, 0, sizeof v->vom);
        if (v->dfxm_on)
            memset(v->dfxm, 0, sizeof v->dfxm);
        if (v->rfxm_on)
            memset(v->rfxm, 0, sizeof v->rfxm);
        if (v->pfxm_on)
            memset(v->pfxm, 0, sizeof v->pfxm);
        if (v->ofxm_on)
            memset(v->ofxm, 0, sizeof v->ofxm);
        if (v->axm_on)
            memset(v->axm, 0, sizeof v->axm);
        if (v->sxm_on)
            memset(v->sxm, 0, sizeof v->sxm);
        v->opm_on = v->mem_on = v->vom_on = v->dfxm_on = v->rfxm_on = 0u;
        v->pfxm_on = v->ofxm_on = v->axm_on = v->sxm_on = 0u;
        for (o = 0u; o < RI_LEVI_NMENV; o++)
            v->melmod[o] = 0.0f;
        *ecut = clampf(*ecut, 40.0f, 18000.0f);
        *ereso = clampf(*ereso, 0.0f, 1.0f);
        *eareso = clampf(*eareso, 0.0f, 1.0f);
        *edrive = clampf(*edrive, 0.0f, 1.0f);
        *edenv = clampf(*edenv, -1.0f, 1.0f);
        *eaenv = clampf(*eaenv, -1.0f, 1.0f);
        *edlfo = clampf(*edlfo, -1.0f, 1.0f);
        *ealfo = clampf(*ealfo, -1.0f, 1.0f);
        *evlfo = clampf(*evlfo, -1.0f, 1.0f);
        *edlevel = clampf(*edlevel, 0.0f, 4.0f);
        *emorph = clampf(*emorph, 0.0f, 100.0f);
        *eoplevel = clampf(*eoplevel, 0.0f, 2.0f);
        *evlevel = clampf(*evlevel, 0.0f, 2.0f);
        return;
    }
    for (i = 0u; i < RI_LEVI_MS_N; i++)
        src[i] = 0.0f;
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        src[RI_LEVI_MS_OPENV0 + o] = v->st[0][o].env.value;
    for (o = 0u; o < RI_LEVI_NLFO; o++) {
        src[RI_LEVI_MS_LFO0 + o] = lfo5[o];
        src[RI_LEVI_MS_LFOP0 + o] = 0.5f * (lfo5[o] + 1.0f);
        v->lfo[o].rmul = 1.0f;
        v->lfo[o].lmod = v->lfo[o].smod = v->lfo[o].stmod = 0.0f;
    }
    for (o = 0u; o < RI_LEVI_NMENV; o++)
        src[RI_LEVI_MS_ENV0 + o] = levi_menv_value(v, o);
    src[RI_LEVI_MS_NOTE] = ((float)(v->note > 127u ? 127u : v->note) - 60.0f) / 60.0f;
    /* Ribbon (fidelity P8d): per-voice copies refreshed per block. */
    src[RI_LEVI_MS_RBNABS] = v->rbn_abs;
    src[RI_LEVI_MS_RBNABSP] = v->rbn_absp;
    src[RI_LEVI_MS_RBNREL] = v->rbn_rel;
    /* Performance signals (fidelity P9a): velocity on/off, per-key and
     * channel aftertouch, mod wheel, pitch bend (bend already carries
     * the voice's bend range). */
    src[RI_LEVI_MS_VELON] = v->vel01;
    src[RI_LEVI_MS_VELOFF] = v->veloff01;
    src[RI_LEVI_MS_POLYAT] = v->pat01;
    src[RI_LEVI_MS_MONOAT] = v->mpat01;
    src[RI_LEVI_MS_WHEEL] = v->wheel01;
    src[RI_LEVI_MS_BEND] = v->bsrc;
    {
        /* VoiceMod (fidelity P6b, manual p. 127): per-voice static
         * values, own law — VMOD bipolar hash, VMOD+ ordinal unipolar.
         * Deterministic, no RNG. */
        uint32_t h = ((uint32_t)v->vidx + 1u) * 0x9E3779B1u;
        h ^= h >> 13;
        h *= 0x85EBCA6Bu;
        h ^= h >> 16;
        src[RI_LEVI_MS_VMOD] = (float)h / 2147483648.0f - 1.0f;
        src[RI_LEVI_MS_VMODP] = (float)v->vidx / ((float)RI_LEVI_NVOICES - 1.0f);
    }
    n = ri_levi_matrix_eval2(mx, src, out);
#ifdef RI_LEVI_PROFILE
    v->prof_mrows += n;
#endif
    if (v->opm_on) {
        memset(v->opm, 0, sizeof v->opm);
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            v->st[0][o].env.susmod = v->st[1][o].env.susmod = 0.0f;
    }
    if (v->mem_on) {
        memset(v->mem, 0, sizeof v->mem);
        for (o = 0u; o < RI_LEVI_NMENV; o++) {
            v->menv[o].susmod = 0.0f;
            v->menv[o].cmod[0] = v->menv[o].cmod[1] = v->menv[o].cmod[2] = 0;
        }
    }
    if (v->vom_on)
        memset(v->vom, 0, sizeof v->vom);
    if (v->dfxm_on)
        memset(v->dfxm, 0, sizeof v->dfxm);
    if (v->rfxm_on)
        memset(v->rfxm, 0, sizeof v->rfxm);
    if (v->pfxm_on)
        memset(v->pfxm, 0, sizeof v->pfxm);
    if (v->ofxm_on)
        memset(v->ofxm, 0, sizeof v->ofxm);
    if (v->axm_on)
        memset(v->axm, 0, sizeof v->axm);
    if (v->sxm_on)
        memset(v->sxm, 0, sizeof v->sxm);
    for (i = 0u; i < n; i++) {
        uint32_t dm = out[i].dmod, dp = out[i].dpar;
        float x = out[i].x;
        if (dm >= RI_LEVI_DM_OSC1 && dm <= RI_LEVI_DM_MODS) {
            for (o = 0u; o < RI_LEVI_NOPS; o++) {
                int carrier = v->feeds[o] == 0u;
                if ((dm <= RI_LEVI_DM_OSC1 + 7u && o != dm - RI_LEVI_DM_OSC1) ||
                    (dm == RI_LEVI_DM_CARR && !carrier) || (dm == RI_LEVI_DM_MODS && carrier))
                    continue;
                v->opm[o][dp] += x;
            }
            touch_op = 1u;
        } else if (dm >= RI_LEVI_DM_ENV1 && dm < RI_LEVI_DM_ENV1 + RI_LEVI_NMENV) {
            v->mem[dm - RI_LEVI_DM_ENV1][dp] += x;
            touch_me = 1u;
        } else if (dm >= RI_LEVI_DM_LFO1 && dm < RI_LEVI_DM_LFO1 + RI_LEVI_NLFO) {
            struct RILeviLFO *l = &v->lfo[dm - RI_LEVI_DM_LFO1];
            if (dp == 0u)
                l->rmul *= ri_pow2(4.0f * x);            /* rate +/-4 octaves */
            else if (dp == 1u)
                l->lmod += x;
            else if (dp == 2u)
                l->smod += x;
            else
                l->stmod += x;
        } else if (dm == RI_LEVI_DM_DFILT) {
            switch (dp) {
            case 0u: *ecut *= ri_pow2(8.5f * x); break;
            case 1u: *ereso += x; break;
            case 2u: case 3u: *edm += 127.0f * x; break;   /* morph / drive share the knob */
            case 4u: *edenv += 2.0f * x; break;
            case 5u: *edlfo += 2.0f * x; break;
            default: *edlevel *= 1.0f + x; break;
            }
        } else if (dm == RI_LEVI_DM_AFILT) {
            switch (dp) {
            case 0u: *eacut *= ri_pow2(8.5f * x); break;
            case 1u: *eareso += x; break;
            case 2u: *edrive += x; break;
            case 3u: *eaenv += 2.0f * x; break;
            default: *ealfo += 2.0f * x; break;
            }
        } else if (dm == RI_LEVI_DM_VCA) {
            if (dp == 0u)
                *evlevel += x;
            else if (dp == 1u)
                *evlfo += 2.0f * x;
            else
                *eoplevel += x;
        } else if (dm == RI_LEVI_DM_ALGO) {
            *emorph += 100.0f * x;
        } else if (dm == RI_LEVI_DM_VOICE) {
            if (dp < RI_LEVI_DVO_N) {
                v->vom[dp] += x;
                touch_vo = 1u;
            }
        } else if (dm == RI_LEVI_DM_DELAY) {
            if (dp < RI_LEVI_DD_N) {
                v->dfxm[dp] += x;
                touch_fx = 1u;
            }
        } else if (dm == RI_LEVI_DM_REVERB) {
            if (dp < RI_LEVI_DR_N) {
                v->rfxm[dp] += x;
                touch_rv = 1u;
            }
        } else if (dm == RI_LEVI_DM_PREFX) {
            if (dp < RI_LEVI_DX_N) {
                v->pfxm[dp] += x;
                touch_px = 1u;
            }
        } else if (dm == RI_LEVI_DM_POSTFX) {
            if (dp < RI_LEVI_DX_N) {
                v->ofxm[dp] += x;
                touch_ox = 1u;
            }
        } else if (dm == RI_LEVI_DM_ARP) {
            if (dp < RI_LEVI_DA_N) {
                v->axm[dp] += x;
                touch_ax = 1u;
            }
        } else if (dm == RI_LEVI_DM_SEQ) {
            if (dp < RI_LEVI_DS_N) {
                v->sxm[dp] += x;
                touch_sx = 1u;
            }
        }
    }
    v->opm_on = (uint8_t)touch_op;
    v->mem_on = (uint8_t)touch_me;
    v->vom_on = (uint8_t)touch_vo;
    v->dfxm_on = (uint8_t)touch_fx;
    v->rfxm_on = (uint8_t)touch_rv;
    v->pfxm_on = (uint8_t)touch_px;
    v->ofxm_on = (uint8_t)touch_ox;
    v->axm_on = (uint8_t)touch_ax;
    v->sxm_on = (uint8_t)touch_sx;
    for (o = 0u; o < RI_LEVI_NMENV && touch_me; o++)
        v->melmod[o] = v->mem[o][RI_LEVI_DE_LEVEL];
    if (!touch_me)
        for (o = 0u; o < RI_LEVI_NMENV; o++)
            v->melmod[o] = 0.0f;
    *ecut = clampf(*ecut, 40.0f, 18000.0f);
    *ereso = clampf(*ereso, 0.0f, 1.0f);
    *eareso = clampf(*eareso, 0.0f, 1.0f);
    *edrive = clampf(*edrive, 0.0f, 1.0f);
    *edenv = clampf(*edenv, -1.0f, 1.0f);
    *eaenv = clampf(*eaenv, -1.0f, 1.0f);
    *edlfo = clampf(*edlfo, -1.0f, 1.0f);
    *ealfo = clampf(*ealfo, -1.0f, 1.0f);
    *evlfo = clampf(*evlfo, -1.0f, 1.0f);
    *edlevel = clampf(*edlevel, 0.0f, 4.0f);
    *emorph = clampf(*emorph, 0.0f, 100.0f);
    *eoplevel = clampf(*eoplevel, 0.0f, 2.0f);
    *evlevel = clampf(*evlevel, 0.0f, 2.0f);
}

/* Voice pitch (P6b laws + P6c spreads, owner 2026-10-06 always on):
 * control-rate update every RI_LEVI_CTRL_N samples. Time integrals run
 * per sample (trajectory exact); evaluation at control points. */
static void voice_pitch_advance(struct RILeviVoice *v, float sr) {
    float va = v->vvibamt + (v->vom_on ? 4.0f * v->vom[RI_LEVI_DVO_VIBAMT] : 0.0f);
    float vr = v->vvibrate * (v->vom_on ? ri_pow2(2.0f * v->vom[RI_LEVI_DVO_VIBRATE]) : 1.0f);
    uint32_t gm = v->vglide ? v->vglide : (v->gforce ? 1u : 0u);
    if (v->vom_on && v->vom[RI_LEVI_DVO_GLIDETGL] != 0.0f)
        gm = v->vom[RI_LEVI_DVO_GLIDETGL] > 0.0f ? 1u : 0u;
    if (va != 0.0f)
        v->vibtime += 1.0f / sr;
    v->vibphase += vr / sr;
    v->vibphase -= (float)(int)v->vibphase;
    if (gm && v->glt < 1.0f) {
        float gt = v->vgltime * (v->vom_on ? ri_pow2(4.0f * v->vom[RI_LEVI_DVO_GLTIME]) : 1.0f);
        float e = gt <= 0.0f ? 1.0f : v->glt + 1.0f / (gt * sr);
        e = e > 1.0f ? 1.0f : e;
        v->glt = e;
    }
    v->wtime += 1.0f / sr;
}
/* Evaluation half (no time updates; runs at control points). Mirrors the
 * computation below operation for operation. */
static float voice_pitch_eval(struct RILeviVoice *v, float sr, float *afwob) {
    float det = v->vdetune + (v->vom_on ? v->vom[RI_LEVI_DVO_DETUNE] : 0.0f);
    float feel = v->vafeel + (v->vom_on ? v->vom[RI_LEVI_DVO_AFEEL] : 0.0f);
    float va = v->vvibamt + (v->vom_on ? 4.0f * v->vom[RI_LEVI_DVO_VIBAMT] : 0.0f);
    float hc = ((float)RI_LEVI_NVOICES - 1.0f) * 0.5f;
    float vsemi = 0.0f;
    float vpitch = 1.0f;
    *afwob = 0.0f;
    (void)sr;
    det = det < -1.0f ? -1.0f : det > 1.0f ? 1.0f : det;
    feel = feel < 0.0f ? 0.0f : feel > 1.0f ? 1.0f : feel;
    if (det != 0.0f)
        vsemi += det * (((float)v->vidx - hc) / hc) * 50.0f;
    if (va != 0.0f) {
        float dg = v->vvibdly <= 0.0f ? 1.0f
            : v->vibtime >= v->vvibdly ? 1.0f : v->vibtime / v->vvibdly;
        vsemi += va * ri_sin(6.2831853f * v->vibphase) * dg;
    }
    if (v->bsrc != 0.0f)
        vsemi += v->bsrc * v->vbendrng;
    {
        uint32_t gm = v->vglide ? v->vglide : (v->gforce ? 1u : 0u);
        if (v->vom_on && v->vom[RI_LEVI_DVO_GLIDETGL] != 0.0f)
            gm = v->vom[RI_LEVI_DVO_GLIDETGL] > 0.0f ? 1u : 0u;
        if (gm && v->glt < 1.0f) {
            float gt = v->vgltime * (v->vom_on ? ri_pow2(4.0f * v->vom[RI_LEVI_DVO_GLTIME]) : 1.0f);
            float gc = v->vglcurve + (v->vom_on ? v->vom[RI_LEVI_DVO_GLCURVE] : 0.0f);
            float e, ee;
            /* Instant glide (time 0) lands at once, exactly like the
             * per-sample path (which updates glt before evaluating): e
             * reads 1.0 here so the offset is exactly 0. Finite-time
             * glide keeps its control-rate sampling (re-pinned in t172). */
            if (gt <= 0.0f)
                e = 1.0f;
            else
                e = v->glt;
            ee = e + (gc - 1.0f) * e * (1.0f - e);
            ee = ee < 0.0f ? 0.0f : ee > 1.0f ? 1.0f : ee;
            if (gm == 2u) {
                float ns = v->glsemi < 0.0f ? -v->glsemi : v->glsemi;
                uint32_t nst = (uint32_t)(ns + 0.5f);
                if (nst < 1u)
                    nst = 1u;
                ee = ((float)(int)(ee * (float)nst + 0.5f)) / (float)nst;
            }
            vsemi += v->glsemi * (1.0f - ee);
        }
    }
    if (feel != 0.0f) {
        float w1 = ri_sin(6.2831853f * (v->wtime / 7.3f + (float)v->vidx * 0.13f));
        float w2 = ri_sin(6.2831853f * (v->wtime / 11.7f + (float)v->vidx * 0.29f));
        vsemi += feel * (3.6f * w1 + 2.4f * w2);
        *afwob = feel * (0.3f * w1 + 0.2f * w2);
    }
    if (vsemi != 0.0f)
        vpitch = ri_pow2(vsemi / 12.0f);
    return vpitch;
}


/* One filtered channel: digital (+ D.Filt level) + analog (P6c stereo
 * shares it per channel; the dmorph save/restore stays with the
 * caller, shared). */
static float voice_chain(struct RILeviVoice *v, float *df, float *af,
    float mix, float sr, float dc, float ac, float ereso, float eareso,
    float edrive, float edlevel) {
#ifdef RI_LEVI_PROFILE
    v->prof_chain++;
#endif
    float out = dfilt_step(v, df, mix, sr, dc, ereso) * edlevel;
    out = afilt_step(v, df == v->dfR ? 3u : 2u, af, out, sr, ac, eareso, edrive);
    return out;
}

/* Vintage digital degradation (P6c, own law): bit-depth quantization
 * plus sample-rate hold, per stereo pair with one shared counter.
 * Exact bypass at 16 bits / 1x (holds track the signal, so engaging
 * the knob is click-free). */
static void vintage_pair(struct RILeviVoice *v, float *l, float *r) {
    if (v->vint_bits >= 16u && v->vint_dec <= 1u) {
        v->vhold[0] = *l;
        v->vhold[1] = *r;
        v->vcount = 0u;
        return;
    }
    if (++v->vcount < v->vint_dec) {
        *l = v->vhold[0];
        *r = v->vhold[1];
        return;
    }
    v->vcount = 0u;
    {
        uint32_t bits = v->vint_bits > 16u ? 16u : v->vint_bits;
        float step = 8.0f / (float)(1u << (bits > 0u ? bits - 1u : 0u)) / 2.0f;
        float ql = (float)(int)(*l / step + (*l < 0.0f ? -0.5f : 0.5f)) * step;
        float qr = (float)(int)(*r / step + (*r < 0.0f ? -0.5f : 0.5f)) * step;
        v->vhold[0] = ql;
        v->vhold[1] = qr;
        *l = ql;
        *r = qr;
    }
}

/* Stereo render (fidelity P6c): the mono render's twin with per-op pans
 * through dual-mono filters. Center pans render dual-mono, each channel
 * bit-identical to the mono sum. Idle voices write exact 0. */
void levi_voice_render_stereo(struct RILeviVoice *v, const struct RILeviMatrix *mx,
    float sr, float *l, float *r) {
    float mixAL, mixAR, mixBL, mixBR, mixL, mixR, outL, outR;
    float ecut, ereso, edrive, emorph, eoplevel, evlevel, amp, lfo5[RI_LEVI_NLFO];
    float vpitch;
    float edm, edenv, eaenv, edlfo, ealfo, evlfo, edlevel, eacut = 1.0f, eareso;
    float vpan, vwidth, panoff, hc2;
    uint32_t pmode;
    int any_on = 0, lfo_on;
    uint32_t o;
    if (!v || !l || !r) {
        if (l)
            *l = 0.0f;
        if (r)
            *r = 0.0f;
        return;
    }
    if (!v->active || !(sr > 0.0f)) {
        *l = *r = 0.0f;
        return;
    }
    lfo_on = mx || v->dlfo != 0.0f || v->alfo != 0.0f || v->vlfo != 0.0f || v->melfo;
    for (o = 0u; o < RI_LEVI_NLFO; o++)
        lfo5[o] = lfo_on ? ri_levi_lfo_step(&v->lfo[o], sr) : 0.0f;
#ifdef RI_LEVI_PROFILE
    if (lfo_on)
        v->prof_lfo += RI_LEVI_NLFO;
#endif
    {
        uint32_t e, k;
        for (e = 0u; e < RI_LEVI_NMENV; e++) {
            if (lfo_on)
                for (k = 0u; k < 4u; k++) {
                    uint32_t src = v->meui[e][RI_LEVI_ME_TRIG1 + k];
                    if (src >= RI_LEVI_TS_LFO1 && src < RI_LEVI_TS_LFO1 + RI_LEVI_NLFO &&
                        v->lfo[src - RI_LEVI_TS_LFO1].wrapped) {
                        menv_start(&v->menv[e]);
                        break;
                    }
                }
            if (v->mem_on) {
                const float *m = v->mem[e];
                float tsc[4];
                tsc[0] = m[RI_LEVI_DE_ATTACK] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_ATTACK]) : 1.0f;
                tsc[1] = m[RI_LEVI_DE_DECAY] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_DECAY]) : 1.0f;
                tsc[2] = m[RI_LEVI_DE_RELEASE] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_RELEASE]) : 1.0f;
                tsc[3] = m[RI_LEVI_DE_HOLD] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_HOLD]) : 1.0f;
                v->menv[e].susmod = m[RI_LEVI_DE_SUSTAIN];
                for (k = 0u; k < 3u; k++) {
                    float c = 64.0f * m[RI_LEVI_DE_ACURVE + k];
                    v->menv[e].cmod[k] = (int8_t)(c < -127.0f ? -127.0f : c > 127.0f ? 127.0f : c);
                }
#ifdef RI_LEVI_PROFILE
                v->prof_envmod++;
#endif
                (void)env_tick_b(&v->menv[e], sr, tsc);
            } else {
#ifdef RI_LEVI_PROFILE
                v->prof_envmod++;
#endif
                (void)env_tick_b(&v->menv[e], sr, 0);
            }
        }
    }
    ecut = v->cutoff;
    ereso = v->reso;
    edrive = v->drive;
    emorph = (float)v->morph;
    eoplevel = 1.0f;
    evlevel = 1.0f;
    edm = (float)v->dmorph;
    edenv = v->denv;
    eaenv = v->aenv;
    edlfo = v->dlfo;
    ealfo = v->alfo;
    evlfo = v->vlfo;
    edlevel = v->dlevel;
    eareso = v->reso2;
    {
        /* Control rate (owner 2026-10-06, N = RI_LEVI_CTRL_N): matrix fold,
         * pitch evaluate and dc/ac/gain targets run at control points.
         * dc/ac/vpitch/evlevel/eoplevel/edlevel interpolate linearly;
         * ereso/edrive/edenv/eaenv/edlfo/ealfo/evlfo/emorph hold.
         * emorph stays held so the O2 endpoint test sees exact values. */
        uint32_t ph = v->ctl_k % RI_LEVI_CTRL_N;
        if (!v->ctl_init || ph == 0u) {
            float tdc, tac, tvp, tafw;
            if (mx)
                levi_mod_apply(v, mx, lfo5, &ecut, &ereso, &edm, &edenv,
                    &edlfo, &edlevel, &eacut, &eareso, &edrive, &eaenv,
                    &ealfo, &evlevel, &evlfo, &eoplevel, &emorph);
            tvp = voice_pitch_eval(v, sr, &tafw);
            tdc = ecut * v->dktm;
            tac = v->cutoff2 * v->aktm * eacut;
            if (tafw != 0.0f) {
                tdc *= ri_pow2(tafw);
                tac *= ri_pow2(tafw);
            }
            if (lfo_on && edlfo != 0.0f)
                tdc *= ri_pow2(4.0f * edlfo * lfo5[0]);
            if (lfo_on && ealfo != 0.0f)
                tac *= ri_pow2(4.0f * ealfo * lfo5[1]);
            if (edenv != 0.0f)
                tdc *= ri_pow2(8.0f * edenv * levi_menv_value(v, 0u));
            if (eaenv != 0.0f)
                tac *= ri_pow2(8.0f * eaenv * levi_menv_value(v, 1u));
            {
                float velb = 2.0f * v->vel01 - 1.0f;
                if (v->dvel != 0.0f || v->dpat != 0.0f)
                    tdc *= ri_pow2(8.0f * (v->dvel * velb + v->dpat * v->pat01));
                if (v->avel != 0.0f || v->apat != 0.0f)
                    tac *= ri_pow2(8.0f * (v->avel * velb + v->apat * v->pat01));
            }
            tdc = tdc < 20.0f ? 20.0f : tdc > 20000.0f ? 20000.0f : tdc;
            tac = tac < 20.0f ? 20.0f : tac > 20000.0f ? 20000.0f : tac;
            v->ctl_e[0] = ecut; v->ctl_e[1] = ereso; v->ctl_e[2] = edm;
            v->ctl_e[3] = edenv; v->ctl_e[4] = edlfo; v->ctl_e[5] = edlevel;
            v->ctl_e[6] = eacut; v->ctl_e[7] = eareso; v->ctl_e[8] = edrive;
            v->ctl_e[9] = eaenv; v->ctl_e[10] = ealfo; v->ctl_e[11] = evlevel;
            v->ctl_e[12] = evlfo; v->ctl_e[13] = eoplevel; v->ctl_e[14] = emorph;
            if (!v->ctl_init) {
                v->ctl_dc0 = v->ctl_dc1 = tdc;
                v->ctl_ac0 = v->ctl_ac1 = tac;
                v->ctl_vp0 = v->ctl_vp1 = tvp;
                v->ctl_g0[0] = v->ctl_g1[0] = evlevel;
                v->ctl_g0[1] = v->ctl_g1[1] = eoplevel;
                v->ctl_g0[2] = v->ctl_g1[2] = edlevel;
                v->ctl_init = 1u;
            } else {
                v->ctl_dc0 = v->ctl_dc1; v->ctl_dc1 = tdc;
                v->ctl_ac0 = v->ctl_ac1; v->ctl_ac1 = tac;
                v->ctl_vp0 = v->ctl_vp1; v->ctl_vp1 = tvp;
                v->ctl_g0[0] = v->ctl_g1[0]; v->ctl_g1[0] = evlevel;
                v->ctl_g0[1] = v->ctl_g1[1]; v->ctl_g1[1] = eoplevel;
                v->ctl_g0[2] = v->ctl_g1[2]; v->ctl_g1[2] = edlevel;
            }
        }
        ecut = v->ctl_e[0]; ereso = v->ctl_e[1]; edm = v->ctl_e[2];
        edenv = v->ctl_e[3]; edlfo = v->ctl_e[4]; edlevel = v->ctl_e[5];
        eacut = v->ctl_e[6]; eareso = v->ctl_e[7]; edrive = v->ctl_e[8];
        eaenv = v->ctl_e[9]; ealfo = v->ctl_e[10]; evlevel = v->ctl_e[11];
        evlfo = v->ctl_e[12]; eoplevel = v->ctl_e[13]; emorph = v->ctl_e[14];
        {
            float t = (float)(v->ctl_k % RI_LEVI_CTRL_N) /
                (float)RI_LEVI_CTRL_N;
            vpitch = v->ctl_vp0 + (v->ctl_vp1 - v->ctl_vp0) * t;
            evlevel = v->ctl_g0[0] + (v->ctl_g1[0] - v->ctl_g0[0]) * t;
            eoplevel = v->ctl_g0[1] + (v->ctl_g1[1] - v->ctl_g0[1]) * t;
            edlevel = v->ctl_g0[2] + (v->ctl_g1[2] - v->ctl_g0[2]) * t;
        }
    }
    voice_pitch_advance(v, sr);
    vpan = v->vpan + (v->vom_on ? v->vom[RI_LEVI_DVO_PAN] : 0.0f);
    vwidth = v->vwidth + (v->vom_on ? v->vom[RI_LEVI_DVO_PANWIDTH] : 0.0f);
    hc2 = ((float)RI_LEVI_NVOICES - 1.0f) * 0.5f;
    panoff = v->vspread * (((float)v->vidx - hc2) / hc2);
    pmode = v->vpanmode;
    vpan = vpan < -1.0f ? -1.0f : vpan > 1.0f ? 1.0f : vpan;
    vwidth = vwidth < 0.0f ? 0.0f : vwidth > 1.0f ? 1.0f : vwidth;
    if (v->liveB_empty) {
        /* P2 C6: no live operator in bank B; the call would only zero its
         * local opout (no state, no any_on, mix stays 0). */
        mixBL = 0.0f;
        mixBR = 0.0f;
        mixAL = voice_pass(v, 0u, sr, vpitch, vpan, vwidth, panoff, pmode, &mixAR, &any_on);
        v->bank_skipped = 0u;
    }
    else {
        /* Static-morph gate (owner 2026-10-06): skip only with no ALGO
         * route and morph exactly at the endpoint for >= 64 samples.
         * LFO-driven morph never skips (bit-exact); knob moves resync
         * once per leave. Composes with liveB_empty above (empty skips,
         * no resync). */
        int has_algo = mx ? mx->mx_has_algo : 0;
        if (!has_algo && (emorph == 0.0f || emorph == 100.0f)) {
            if (v->morph_hold < 64u)
                v->morph_hold++;
        } else {
            v->morph_hold = 0u;
        }
        if (!has_algo && v->morph_hold >= 64u && emorph == 0.0f) {
            mixAL = voice_pass(v, 0u, sr, vpitch, vpan, vwidth, panoff, pmode, &mixAR, &any_on);
            mixBL = 0.0f;
            mixBR = 0.0f;
            v->bank_skipped = 1u;
        } else if (!has_algo && v->morph_hold >= 64u && emorph == 100.0f) {
            mixAL = 0.0f;
            mixAR = 0.0f;
            mixBL = voice_pass(v, 1u, sr, vpitch, vpan, vwidth, panoff, pmode, &mixBR, &any_on);
            v->bank_skipped = 2u;
        } else {
            uint32_t bko;
            if (v->bank_skipped == 1u)
                for (bko = 0u; bko < RI_LEVI_NOPS; bko++)
                    v->st[1][bko] = v->st[0][bko];
            else if (v->bank_skipped == 2u)
                for (bko = 0u; bko < RI_LEVI_NOPS; bko++)
                    v->st[0][bko] = v->st[1][bko];
            v->bank_skipped = 0u;
            mixAL = voice_pass(v, 0u, sr, vpitch, vpan, vwidth, panoff, pmode, &mixAR, &any_on);
            mixBL = voice_pass(v, 1u, sr, vpitch, vpan, vwidth, panoff, pmode, &mixBR, &any_on);
        }
    }
    if (emorph <= 0.0f) {
        mixL = mixAL;
        mixR = mixAR;
    } else if (emorph >= 100.0f) {
        mixL = mixBL;
        mixR = mixBR;
    } else {
        mixL = mixAL + (mixBL - mixAL) * (emorph / 100.0f);
        mixR = mixAR + (mixBR - mixAR) * (emorph / 100.0f);
    }
    mixL *= eoplevel * v->osclvl;
    mixR *= eoplevel * v->osclvl;
    if (v->menv[2].stage == RI_LEVI_SEG_IDLE && v->vinit == 0.0f && v->menv[2].value == 0.0f)
        any_on = 0;                                    /* the VCA has closed */
    if (!any_on) {
        v->active = 0u;
        filt_clear(v);
        *l = *r = 0.0f;
        return;
    }
    {
        float t = (float)(v->ctl_k % RI_LEVI_CTRL_N) /
            (float)RI_LEVI_CTRL_N;
        float dc = v->ctl_dc0 + (v->ctl_dc1 - v->ctl_dc0) * t;
        float ac = v->ctl_ac0 + (v->ctl_ac1 - v->ctl_ac0) * t;
        uint8_t dmsave = v->dmorph;
        dc = dc < 20.0f ? 20.0f : dc > 20000.0f ? 20000.0f : dc;
        ac = ac < 20.0f ? 20.0f : ac > 20000.0f ? 20000.0f : ac;
#ifdef RI_LEVI_PROFILE
        prof_memo_check(v, dc, ac);
#endif
        v->dmorph = (uint8_t)(edm < 0.0f ? 0.0f : edm > 127.0f ? 127.0f : edm + 0.5f);
#ifdef RI_LEVI_PROFILE
        {
            uint32_t la, ra;
            memcpy(&la, &mixL, 4);
            memcpy(&ra, &mixR, 4);
            if (la == ra && !memcmp(v->df, v->dfR, sizeof v->df) &&
                !memcmp(v->af, v->afR, sizeof v->af))
                v->prof_dual++;
        }
#endif
        /* P2 C5: dual-mono collapse. Spread voices (panoff != 0 for every
         * voice) can never be dual-mono, so they skip the check entirely
         * and take the old path. Otherwise mixL == mixR bitwise is checked
         * every sample; the lock (states equal at last sample end) lets a
         * locked voice skip the state compare. */
        if (v->vspread != 0.0f) {
            outL = voice_chain(v, v->df, v->af, mixL, sr, dc, ac, ereso,
                eareso, edrive, edlevel);
            outR = voice_chain(v, v->dfR, v->afR, mixR, sr, dc, ac, ereso,
                eareso, edrive, edlevel);
            v->dual_lock = 0u;
        } else {
            uint32_t la, ra;
            uint8_t same_st = 0u;
            memcpy(&la, &mixL, 4);
            memcpy(&ra, &mixR, 4);
            if (la == ra) {
                if (v->dual_lock)
                    same_st = 1u;
                else if (!memcmp(v->df, v->dfR, sizeof v->df) &&
                    !memcmp(v->af, v->afR, sizeof v->af))
                    same_st = 1u;
            }
            if (same_st) {
                outL = voice_chain(v, v->df, v->af, mixL, sr, dc, ac, ereso,
                    eareso, edrive, edlevel);
                outR = outL;
                memcpy(v->dfR, v->df, sizeof v->df);
                memcpy(v->afR, v->af, sizeof v->af);
                v->dual_lock = 1u;
            } else {
                outL = voice_chain(v, v->df, v->af, mixL, sr, dc, ac, ereso,
                    eareso, edrive, edlevel);
                outR = voice_chain(v, v->dfR, v->afR, mixR, sr, dc, ac, ereso,
                    eareso, edrive, edlevel);
                v->dual_lock = 0u;
            }
        }
        v->dmorph = dmsave;
        amp = v->vcalvl * (v->vinit + (1.0f - v->vinit) * levi_menv_value(v, 2u));
        if (v->vvel != 0.0f || v->vpat != 0.0f) {  /* VCA > velocity / polyat */
            float g = 1.0f + v->vvel * (2.0f * v->vel01 - 1.0f) + v->vpat * v->pat01;
            amp *= g < 0.0f ? 0.0f : g;
        }
        if (lfo_on && evlfo != 0.0f) {
            amp *= 1.0f + evlfo * lfo5[2];
            if (amp < 0.0f)
                amp = 0.0f;
        }
        if (v->zgain != 1.0f)                      /* zone layer gain (P9c) */
            amp *= v->zgain;
    }
    if (!(outL > -1e20f && outL < 1e20f) || !(outR > -1e20f && outR < 1e20f)) {
        filt_clear(v);
        *l = *r = 0.0f;
        return;
    }
    outL *= amp * v->patchlvl * v->level * evlevel;
    outR *= amp * v->patchlvl * v->level * evlevel;
    vintage_pair(v, &outL, &outR);
    *l = outL;
    *r = outR;
    v->ctl_k++;
}

float levi_voice_render(struct RILeviVoice *v, const struct RILeviMatrix *mx,
    float sr) {
    float mixA, mixB, mix, out;
    /* Effective params: base copies when the matrix is silent, so the
     * plain path stays bit-identical (x*1.0 and x+0.0 are exact; morph
     * keeps its integer branches through emorph). */
    float ecut, ereso, edrive, emorph, eoplevel, evlevel, amp, lfo5[RI_LEVI_NLFO];
    float vpitch;
    float edm, edenv, eaenv, edlfo, ealfo, evlfo, edlevel, eacut = 1.0f, eareso;
    /* LFOs step once per sample when the matrix or a pre-wired amount
     * (LFO 1 > digital, 2 > analog, 3 > VCA; P4) listens. */
    int any_on = 0, lfo_on;
    uint32_t o;
    if (!v || !v->active || !(sr > 0.0f))
        return 0.0f;
    v->dual_lock = 0u;   /* mono advances only the L filters (P2 C5 lock) */
    lfo_on = mx || v->dlfo != 0.0f || v->alfo != 0.0f || v->vlfo != 0.0f || v->melfo;
    for (o = 0u; o < RI_LEVI_NLFO; o++)
        lfo5[o] = lfo_on ? ri_levi_lfo_step(&v->lfo[o], sr) : 0.0f;
#ifdef RI_LEVI_PROFILE
    if (lfo_on)
        v->prof_lfo += RI_LEVI_NLFO;
#endif
    {
        /* Mod envelopes (P5): LFO cycle starts retrigger where chosen;
         * matrix offsets move their times, sustain and curves (P5b). */
        uint32_t e, k;
        for (e = 0u; e < RI_LEVI_NMENV; e++) {
            if (lfo_on)
                for (k = 0u; k < 4u; k++) {
                    uint32_t src = v->meui[e][RI_LEVI_ME_TRIG1 + k];
                    if (src >= RI_LEVI_TS_LFO1 && src < RI_LEVI_TS_LFO1 + RI_LEVI_NLFO &&
                        v->lfo[src - RI_LEVI_TS_LFO1].wrapped) {
                        menv_start(&v->menv[e]);
                        break;
                    }
                }
            if (v->mem_on) {
                const float *m = v->mem[e];
                float tsc[4];
                tsc[0] = m[RI_LEVI_DE_ATTACK] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_ATTACK]) : 1.0f;
                tsc[1] = m[RI_LEVI_DE_DECAY] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_DECAY]) : 1.0f;
                tsc[2] = m[RI_LEVI_DE_RELEASE] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_RELEASE]) : 1.0f;
                tsc[3] = m[RI_LEVI_DE_HOLD] != 0.0f ? ri_pow2(8.0f * m[RI_LEVI_DE_HOLD]) : 1.0f;
                v->menv[e].susmod = m[RI_LEVI_DE_SUSTAIN];
                for (k = 0u; k < 3u; k++) {
                    float c = 64.0f * m[RI_LEVI_DE_ACURVE + k];
                    v->menv[e].cmod[k] = (int8_t)(c < -127.0f ? -127.0f : c > 127.0f ? 127.0f : c);
                }
#ifdef RI_LEVI_PROFILE
                v->prof_envmod++;
#endif
                (void)env_tick_b(&v->menv[e], sr, tsc);
            } else {
#ifdef RI_LEVI_PROFILE
                v->prof_envmod++;
#endif
                (void)env_tick_b(&v->menv[e], sr, 0);
            }
        }
    }
    ecut = v->cutoff;
    ereso = v->reso;
    edrive = v->drive;
    emorph = (float)v->morph;
    eoplevel = 1.0f;
    evlevel = 1.0f;
    edm = (float)v->dmorph;
    edenv = v->denv;
    eaenv = v->aenv;
    edlfo = v->dlfo;
    ealfo = v->alfo;
    evlfo = v->vlfo;
    edlevel = v->dlevel;
    eareso = v->reso2;
    {
        uint32_t ph = v->ctl_k % RI_LEVI_CTRL_N;
        if (!v->ctl_init || ph == 0u) {
            float tdc, tac, tvp, tafw;
            if (mx)
                levi_mod_apply(v, mx, lfo5, &ecut, &ereso, &edm, &edenv,
                    &edlfo, &edlevel, &eacut, &eareso, &edrive, &eaenv,
                    &ealfo, &evlevel, &evlfo, &eoplevel, &emorph);
            tvp = voice_pitch_eval(v, sr, &tafw);
            tdc = ecut * v->dktm;
            tac = v->cutoff2 * v->aktm * eacut;
            if (tafw != 0.0f) {
                tdc *= ri_pow2(tafw);
                tac *= ri_pow2(tafw);
            }
            if (lfo_on && edlfo != 0.0f)
                tdc *= ri_pow2(4.0f * edlfo * lfo5[0]);
            if (lfo_on && ealfo != 0.0f)
                tac *= ri_pow2(4.0f * ealfo * lfo5[1]);
            if (edenv != 0.0f)
                tdc *= ri_pow2(8.0f * edenv * levi_menv_value(v, 0u));
            if (eaenv != 0.0f)
                tac *= ri_pow2(8.0f * eaenv * levi_menv_value(v, 1u));
            {
                float velb = 2.0f * v->vel01 - 1.0f;
                if (v->dvel != 0.0f || v->dpat != 0.0f)
                    tdc *= ri_pow2(8.0f * (v->dvel * velb + v->dpat * v->pat01));
                if (v->avel != 0.0f || v->apat != 0.0f)
                    tac *= ri_pow2(8.0f * (v->avel * velb + v->apat * v->pat01));
            }
            tdc = tdc < 20.0f ? 20.0f : tdc > 20000.0f ? 20000.0f : tdc;
            tac = tac < 20.0f ? 20.0f : tac > 20000.0f ? 20000.0f : tac;
            v->ctl_e[0] = ecut; v->ctl_e[1] = ereso; v->ctl_e[2] = edm;
            v->ctl_e[3] = edenv; v->ctl_e[4] = edlfo; v->ctl_e[5] = edlevel;
            v->ctl_e[6] = eacut; v->ctl_e[7] = eareso; v->ctl_e[8] = edrive;
            v->ctl_e[9] = eaenv; v->ctl_e[10] = ealfo; v->ctl_e[11] = evlevel;
            v->ctl_e[12] = evlfo; v->ctl_e[13] = eoplevel; v->ctl_e[14] = emorph;
            if (!v->ctl_init) {
                v->ctl_dc0 = v->ctl_dc1 = tdc;
                v->ctl_ac0 = v->ctl_ac1 = tac;
                v->ctl_vp0 = v->ctl_vp1 = tvp;
                v->ctl_g0[0] = v->ctl_g1[0] = evlevel;
                v->ctl_g0[1] = v->ctl_g1[1] = eoplevel;
                v->ctl_g0[2] = v->ctl_g1[2] = edlevel;
                v->ctl_init = 1u;
            } else {
                v->ctl_dc0 = v->ctl_dc1; v->ctl_dc1 = tdc;
                v->ctl_ac0 = v->ctl_ac1; v->ctl_ac1 = tac;
                v->ctl_vp0 = v->ctl_vp1; v->ctl_vp1 = tvp;
                v->ctl_g0[0] = v->ctl_g1[0]; v->ctl_g1[0] = evlevel;
                v->ctl_g0[1] = v->ctl_g1[1]; v->ctl_g1[1] = eoplevel;
                v->ctl_g0[2] = v->ctl_g1[2]; v->ctl_g1[2] = edlevel;
            }
        }
        ecut = v->ctl_e[0]; ereso = v->ctl_e[1]; edm = v->ctl_e[2];
        edenv = v->ctl_e[3]; edlfo = v->ctl_e[4]; edlevel = v->ctl_e[5];
        eacut = v->ctl_e[6]; eareso = v->ctl_e[7]; edrive = v->ctl_e[8];
        eaenv = v->ctl_e[9]; ealfo = v->ctl_e[10]; evlevel = v->ctl_e[11];
        evlfo = v->ctl_e[12]; eoplevel = v->ctl_e[13]; emorph = v->ctl_e[14];
        {
            float t = (float)(v->ctl_k % RI_LEVI_CTRL_N) /
                (float)RI_LEVI_CTRL_N;
            vpitch = v->ctl_vp0 + (v->ctl_vp1 - v->ctl_vp0) * t;
            evlevel = v->ctl_g0[0] + (v->ctl_g1[0] - v->ctl_g0[0]) * t;
            eoplevel = v->ctl_g0[1] + (v->ctl_g1[1] - v->ctl_g0[1]) * t;
            edlevel = v->ctl_g0[2] + (v->ctl_g1[2] - v->ctl_g0[2]) * t;
        }
    }
    voice_pitch_advance(v, sr);
    if (v->liveB_empty) {
        mixB = 0.0f;
        mixA = voice_pass(v, 0u, sr, vpitch, 0.0f, 0.0f, 0.0f, 0u, 0, &any_on);
        v->bank_skipped = 0u;
    }
    else {
        int has_algo = mx ? mx->mx_has_algo : 0;
        if (!has_algo && (emorph == 0.0f || emorph == 100.0f)) {
            if (v->morph_hold < 64u)
                v->morph_hold++;
        } else {
            v->morph_hold = 0u;
        }
        if (!has_algo && v->morph_hold >= 64u && emorph == 0.0f) {
            mixA = voice_pass(v, 0u, sr, vpitch, 0.0f, 0.0f, 0.0f, 0u, 0, &any_on);
            mixB = 0.0f;
            v->bank_skipped = 1u;
        } else if (!has_algo && v->morph_hold >= 64u && emorph == 100.0f) {
            mixA = 0.0f;
            mixB = voice_pass(v, 1u, sr, vpitch, 0.0f, 0.0f, 0.0f, 0u, 0, &any_on);
            v->bank_skipped = 2u;
        } else {
            uint32_t bko;
            if (v->bank_skipped == 1u)
                for (bko = 0u; bko < RI_LEVI_NOPS; bko++)
                    v->st[1][bko] = v->st[0][bko];
            else if (v->bank_skipped == 2u)
                for (bko = 0u; bko < RI_LEVI_NOPS; bko++)
                    v->st[0][bko] = v->st[1][bko];
            v->bank_skipped = 0u;
            mixA = voice_pass(v, 0u, sr, vpitch, 0.0f, 0.0f, 0.0f, 0u, 0, &any_on);
            mixB = voice_pass(v, 1u, sr, vpitch, 0.0f, 0.0f, 0.0f, 0u, 0, &any_on);
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
    mix *= eoplevel * v->osclvl;
    if (v->menv[2].stage == RI_LEVI_SEG_IDLE && v->vinit == 0.0f && v->menv[2].value == 0.0f)
        any_on = 0;                                    /* the VCA has closed */
    if (!any_on) {
        v->active = 0u;
        filt_clear(v);
        return 0.0f;
    }
    {
        /* Signal flow (p. 67): OSCs level > digital filter > D.Filt
         * level > analog pre-drive + filter > VCA level > patch level. */
        float t = (float)(v->ctl_k % RI_LEVI_CTRL_N) /
            (float)RI_LEVI_CTRL_N;
        float dc = v->ctl_dc0 + (v->ctl_dc1 - v->ctl_dc0) * t;
        float ac = v->ctl_ac0 + (v->ctl_ac1 - v->ctl_ac0) * t;
        uint8_t dmsave = v->dmorph;
        dc = dc < 20.0f ? 20.0f : dc > 20000.0f ? 20000.0f : dc;
        ac = ac < 20.0f ? 20.0f : ac > 20000.0f ? 20000.0f : ac;
#ifdef RI_LEVI_PROFILE
        prof_memo_check(v, dc, ac);
#endif
        v->dmorph = (uint8_t)(edm < 0.0f ? 0.0f : edm > 127.0f ? 127.0f : edm + 0.5f);
        out = voice_chain(v, v->df, v->af, mix, sr, dc, ac, ereso, eareso, edrive, edlevel);
        v->dmorph = dmsave;
        /* ENV 3 > VCA, opened to Initial Level at rest (p. 69). */
        amp = v->vcalvl * (v->vinit + (1.0f - v->vinit) * levi_menv_value(v, 2u));
        if (v->vvel != 0.0f || v->vpat != 0.0f) {  /* VCA > velocity / polyat */
            float g = 1.0f + v->vvel * (2.0f * v->vel01 - 1.0f) + v->vpat * v->pat01;
            amp *= g < 0.0f ? 0.0f : g;
        }
        if (lfo_on && evlfo != 0.0f) {
            amp *= 1.0f + evlfo * lfo5[2];
            if (amp < 0.0f)
                amp = 0.0f;
        }
        if (v->zgain != 1.0f)                      /* zone layer gain (P9c) */
            amp *= v->zgain;
    }
    if (!(out > -1e20f && out < 1e20f)) {
        /* Non-finite latch guard (Dell 2026-09-28): a poisoned filter
         * state must self-heal to silence, never mute the mix. */
        filt_clear(v);
        return 0.0f;
    }
    v->ctl_k++;
    return out * amp * v->patchlvl * v->level * evlevel;
}

/* Performance signals (fidelity P9a): per-voice copies, one block behind
 * the setter (the same shape as the ribbon). Bend normalises against each
 * voice's P6b bend range, so range 0 reads 0. Both sums refresh: the amount
 * laws (P9b) read vel01 / pat01 from the voice, so a sum that skipped this
 * would hear a dead signal. */
static void levi_sig_refresh(struct RILeviSet *s) {
    uint32_t v;
    for (v = 0u; v < RI_LEVI_NVOICES; v++) {
        struct RILeviVoice *pv = &s->v[v];
        pv->vel01 = (float)pv->nvel / 127.0f;
        pv->veloff01 = (float)pv->nveloff / 127.0f;
        pv->pat01 = (float)s->pat[v] / 127.0f;
        pv->mpat01 = (float)s->press / 127.0f;
        pv->wheel01 = (float)s->wheel / 127.0f;
        pv->bsrc = pv->vbendrng > 0.0f ? s->bend / pv->vbendrng : 0.0f;
        if (pv->bsrc < -1.0f)
            pv->bsrc = -1.0f;
        else if (pv->bsrc > 1.0f)
            pv->bsrc = 1.0f;
        /* Glide button (fidelity P9d): the render has no set pointer, so
         * the momentary hold arrives here, per block -- releasing it
         * hands every voice back to its own mode at the next block. */
        pv->gforce = s->p_glidehold ? 1u : 0u;
    }
}

void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr) {
    uint32_t i, v;
    if (!s || !out)
        return;
    /* Performance signals (fidelity P9a, P9b): the mono sum refreshes the
     * same per-voice copies the stereo sum does -- otherwise the amount
     * laws (which read vel01 / pat01) would see a dead signal here. */
    levi_sig_refresh(s);
    for (i = 0u; i < n; i++) {
        float m = 0.0f;
        uint32_t o;
        /* Shared LFOs (trig sync single / off, P5): stepped once, read by
         * every voice; Off staggers each voice's phase. */
        for (o = 0u; o < RI_LEVI_NLFO; o++) {
            struct RILeviLFO *g = &s->glfo[o];
            if (!g->trig)
                continue;
            lfo_advance(g, sr);
            for (v = 0u; v < RI_LEVI_NVOICES; v++) {
                struct RILeviLFO *l = &s->v[v].lfo[o];
                float ph = g->phase;
                if (!l->shared)
                    continue;
                if (g->trig == 2u && g->ui[RI_LEVI_LP_STAGGER]) {
                    ph += (float)v * (float)g->ui[RI_LEVI_LP_STAGGER] / 128.0f;
                    ph -= (float)(int)ph;
                }
                l->value = lfo_post(g, lfo_wave(g, ph), &l->sy);
                l->wrapped = g->wrapped;
            }
        }
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], &s->mx, sr);
        out[i] = m;
    }
}

/* Stereo sum (fidelity P6c): per-voice stereo into out_l/out_r. */
void levi_voice_counters_reset(struct RILeviSet *s) {
    if (!s)
        return;
    s->vc_samples = 0u;
    s->vc_lfo_samples = 0u;
    s->vc_lfo_iters = 0u;
    s->vc_voice_calls = 0u;
    s->vc_voice_active = 0u;
    s->vc_fx_samples = 0u;
}

#ifdef RI_LEVI_OPT_SUBNORM_PROBE
uint64_t ri_levi_subnorm_count = 0u;
static int is_subnorm(float x) {
    uint32_t u;
    memcpy(&u, &x, 4);
    return (u & 0x7f800000u) == 0u && (u & 0x007fffffu) != 0u;
}
#endif
void levi_voice_render_sum_stereo(struct RILeviSet *s, float *out_l,
    float *out_r, uint32_t n, float sr) {
    uint32_t i, v;
    if (!s || !out_l || !out_r)
        return;
    levi_tempo_refresh(s);   /* flagged musical times follow the cached tempo */
    /* Ribbon sources (fidelity P8d): computed once per block, voices
     * read their copies (render has no set pointer). REL consumes. */
    {
        float pos = (float)(s->rbn_pos > 127u ? 127u : s->rbn_pos);
        float d = s->rbn_mode ? pos - (float)s->rbn_last : 0.0f;
        float rel = d / 64.0f;
        float ab, ap;
        if (s->rbn_mode == 0u) {
            ab = 0.0f;
            ap = 0.0f;
            rel = 0.0f;
        } else {
            ab = pos / 127.0f * 2.0f - 1.0f;
            ap = pos / 127.0f;
        }
        if (rel < -1.0f)
            rel = -1.0f;
        if (rel > 1.0f)
            rel = 1.0f;
        for (v = 0u; v < RI_LEVI_NVOICES; v++) {
            s->v[v].rbn_abs = ab;
            s->v[v].rbn_absp = ap;
            s->v[v].rbn_rel = rel;
        }
        s->rbn_last = s->rbn_pos;
    }
    /* Performance signals (fidelity P9a): per-voice copies, one block
     * behind the setter (the same shape as the ribbon). Bend normalises
     * against each voice's P6b bend range, so range 0 reads 0. */
    levi_sig_refresh(s);
    for (i = 0u; i < n; i++) {
        float ml = 0.0f, mr = 0.0f, vl, vr;
        uint32_t o;
        uint32_t lfo_hit = 0u;   /* did any LFO fire this sample? */
        s->vc_samples++;
        for (o = 0u; o < RI_LEVI_NLFO; o++) {
            struct RILeviLFO *g = &s->glfo[o];
            if (!g->trig)
                continue;
            lfo_hit = 1u;
            lfo_advance(g, sr);
            /* Every voice is walked whether or not it reads this LFO, so the
             * iteration count is the whole voice span per trigged LFO. That is
             * the cost this counter exists to make visible. */
            s->vc_lfo_iters += RI_LEVI_NVOICES;
            for (v = 0u; v < RI_LEVI_NVOICES; v++) {
                struct RILeviLFO *l = &s->v[v].lfo[o];
                float ph = g->phase;
                if (!l->shared)
                    continue;
                if (g->trig == 2u && g->ui[RI_LEVI_LP_STAGGER]) {
                    ph += (float)v * (float)g->ui[RI_LEVI_LP_STAGGER] / 128.0f;
                    ph -= (float)(int)ph;
                }
                l->value = lfo_post(g, lfo_wave(g, ph), &l->sy);
                l->wrapped = g->wrapped;
            }
        }
        for (v = 0u; v < RI_LEVI_NVOICES; v++) {
            s->vc_voice_calls++;
            if (s->v[v].active)
                s->vc_voice_active++;
            levi_voice_render_stereo(&s->v[v], &s->mx, sr, &vl, &vr);
            ml += vl;
            mr += vr;
        }
        /* Per-device FX chain (fidelity P7): pre -> delay -> reverb ->
         * post on the voice sum. Bypassed == bit-identical. */
        if (!s->fx.pre.bypass) {
            /* Matrix pre-FX modulation follows the lead voice (P7a pattern). */
            uint32_t lv, q;
            for (lv = 0u; lv < RI_LEVI_NVOICES; lv++)
                if (s->v[lv].active)
                    break;
            if (lv >= RI_LEVI_NVOICES)
                lv = 0u;
            if (s->v[lv].pfxm_on) {
                for (q = 0u; q < RI_LEVI_DX_N; q++)
                    s->fx.pre.mxm[q] = s->v[lv].pfxm[q];
                s->fx.pre.mxm_on = 1u;
            } else {
                s->fx.pre.mxm_on = 0u;
            }
            levi_fx_mod(&s->fx.pre, sr, ml, mr, &ml, &mr);
        }
        if (!s->fx.dbypass) {
            /* Matrix delay modulation follows the lead (first active) voice. */
            uint32_t lv, q;
            for (lv = 0u; lv < RI_LEVI_NVOICES; lv++)
                if (s->v[lv].active)
                    break;
            if (lv >= RI_LEVI_NVOICES)
                lv = 0u;
            if (s->v[lv].dfxm_on) {
                for (q = 0u; q < RI_LEVI_DD_N; q++)
                    s->fx.dfxm[q] = s->v[lv].dfxm[q];
                s->fx.dfxm_on = 1u;
            } else {
                s->fx.dfxm_on = 0u;
            }
            levi_fx_delay(&s->fx, sr, ml, mr, &ml, &mr);
        }
        /* Reverb (fidelity P7b): lead voice drives the matrix offsets. */
        if (!s->fx.rbypass) {
            uint32_t lv, q;
            for (lv = 0u; lv < RI_LEVI_NVOICES; lv++)
                if (s->v[lv].active)
                    break;
            if (lv >= RI_LEVI_NVOICES)
                lv = 0u;
            if (s->v[lv].rfxm_on) {
                for (q = 0u; q < RI_LEVI_DR_N; q++)
                    s->fx.rfxm[q] = s->v[lv].rfxm[q];
                s->fx.rfxm_on = 1u;
            } else {
                s->fx.rfxm_on = 0u;
            }
            levi_fx_reverb(&s->fx, sr, ml, mr, &ml, &mr);
        }
        if (!s->fx.post.bypass) {
            uint32_t lv, q;
            for (lv = 0u; lv < RI_LEVI_NVOICES; lv++)
                if (s->v[lv].active)
                    break;
            if (lv >= RI_LEVI_NVOICES)
                lv = 0u;
            if (s->v[lv].ofxm_on) {
                for (q = 0u; q < RI_LEVI_DX_N; q++)
                    s->fx.post.mxm[q] = s->v[lv].ofxm[q];
                s->fx.post.mxm_on = 1u;
            } else {
                s->fx.post.mxm_on = 0u;
            }
            levi_fx_mod(&s->fx.post, sr, ml, mr, &ml, &mr);
            s->vc_fx_samples++;
        }
        out_l[i] = ml;
        out_r[i] = mr;
#ifdef RI_LEVI_OPT_SUBNORM_PROBE
        ri_levi_subnorm_count += (uint64_t)(is_subnorm(ml) + is_subnorm(mr));
#endif
        s->vc_lfo_samples += lfo_hit;
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

/* Effective arp UI (knob + DM_ARP lead-voice offset, ±64 UI span). */
static uint8_t arp_eff(struct RILeviSet *s, uint32_t dp, uint8_t knob) {
    uint32_t lv;
    int x = (int)knob;
    for (lv = 0u; lv < RI_LEVI_NVOICES; lv++)
        if (s->v[lv].active)
            break;
    if (lv >= RI_LEVI_NVOICES)
        lv = 0u;
    if (dp < RI_LEVI_DA_N && s->v[lv].axm_on)
        x += (int)(s->v[lv].axm[dp] * 64.0f);
    if (x < 0)
        x = 0;
    if (x > 127)
        x = 127;
    return (uint8_t)x;
}

static uint32_t arp_lcg_next(struct RILeviSet *s) {
    s->arp_lcg = s->arp_lcg * 1664525u + 1013904223u;
    return s->arp_lcg >> 16;
}

/* Snapshot the held chord low -> high (insertion sort, n <= 6). */
static uint32_t arp_chord(struct RILeviSet *s, uint8_t *chord) {
    uint32_t i, j, n = 0u;
    for (i = 0u; i < s->an && n < RI_LEVI_ARP_MAXNOTES; i++)
        chord[n++] = s->anotes[i];
    for (i = 1u; i < n; i++) {
        uint8_t v = chord[i];
        j = i;
        while (j > 0u && chord[j - 1u] > v) {
            chord[j] = chord[j - 1u];
            j--;
        }
        chord[j] = v;
    }
    return n;
}

/* Arm gate countdowns on voices sounding the struck note. */
static void arp_arm(struct RILeviSet *s, uint8_t note, int32_t len) {
    uint32_t v;
    if (len < 0)
        return;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        if (s->v[v].active && s->v[v].note == note) {
            s->arp_gate[v] = len;
            s->arp_gnote[v] = note;
        }
}

/* (Re)start the pattern on a chord: stepper + stepoff rotation. */
static void arp_start_chord(struct RILeviSet *s, const uint8_t *chord,
    uint32_t nch, uint8_t mode, uint8_t phrase, uint8_t stepoff, uint8_t len) {
    uint8_t tmp;
    uint32_t k, off;
    s->darp.phrase = phrase;
    s->darp.uphr = &s->arp_uphr[0][0];
    s->darp.on = 1u;
    if (ri_levi_arp_start(&s->darp, chord, nch, mode, (uint32_t)s->arp_samp) != 0) {
        s->darp.on = 0u;
        return;
    }
    off = stepoff >= 16u ? 15u : stepoff;
    for (k = 0u; k < off; k++)
        ri_levi_arp_step(&s->darp, &tmp);
    s->arp_pos = len ? off % len : 0u;
    s->arp_oct = 0u;
}

void levi_arp_block(struct RILeviSet *s, float sr, uint32_t n) {
    uint32_t i, stepsq, len, rats;
    float step_dur;
    uint8_t chord[RI_LEVI_ARP_MAXNOTES], nch = 0u;
    uint8_t mode, octmode, phrase, eff_sw, eff_ch, eff_en;
    uint32_t octrange, gatepct;
    if (!s || !(sr > 0.0f) || n == 0u)
        return;
    /* Gate countdowns tick even with the arp off (releases land). */
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        if (s->arp_gate[i] > 0) {
            s->arp_gate[i] -= (int32_t)n;
            if (s->arp_gate[i] <= 0) {
                s->arp_gate[i] = -1;
                if (s->v[i].active && s->v[i].note == s->arp_gnote[i])
                    levi_release(s, i);
            }
        }
    }
    /* Pending ratchet sub-hits fire on schedule. */
    for (i = 0u; i < s->arp_npend;) {
        if (s->arp_pend_at[i] < s->arp_samp + n) {
            uint8_t nn = s->arp_pend_note[i];
            int32_t off = s->arp_pend_off[i];
            uint32_t j;
            levi_note_strike(s, nn);
            arp_arm(s, nn, off);
            s->arp_npend--;
            for (j = i; j < s->arp_npend; j++) {
                s->arp_pend_at[j] = s->arp_pend_at[j + 1u];
                s->arp_pend_note[j] = s->arp_pend_note[j + 1u];
                s->arp_pend_off[j] = s->arp_pend_off[j + 1u];
            }
        } else {
            i++;
        }
    }
    if (!s->arpon) {
        s->arp_samp += n;
        return;
    }
    /* Effective params (knob + matrix). */
    mode = arp_eff(s, RI_LEVI_DA_MODE, s->arpmode);
    if (mode > RI_LEVI_ARP_NMODES - 1u)
        mode = RI_LEVI_ARP_NMODES - 1u;
    octmode = arp_eff(s, RI_LEVI_DA_OCTMODE, s->arpoctmode);
    if (octmode > 2u)
        octmode = 2u;
    octrange = 1u + arp_eff(s, RI_LEVI_DA_OCTAVE, s->arpoctrange) * 3u / 127u;
    gatepct = 5u + arp_eff(s, RI_LEVI_DA_GATE, s->arpgate) * 145u / 127u;
    len = 1u + arp_eff(s, RI_LEVI_DA_LENGTH, s->arplen) * 15u / 127u;
    phrase = arp_eff(s, RI_LEVI_DA_PHRASE, s->arpphrase);
    eff_sw = arp_eff(s, RI_LEVI_DA_SWING, s->arpswing);
    eff_ch = arp_eff(s, RI_LEVI_DA_CHANCE, s->arpchance);
    eff_en = arp_eff(s, RI_LEVI_DA_ENTROPY, s->arpentropy);
    /* Chord snapshot: held, else latch, else empty. */
    nch = arp_chord(s, chord);
    if (nch > 0u) {
        for (i = 0u; i < nch; i++)
            s->arp_latch[i] = chord[i];
        s->arp_nlatch = (uint8_t)nch;
    } else if (s->arplatch && s->arp_nlatch) {
        for (i = 0u; i < s->arp_nlatch && i < RI_LEVI_ARP_MAXNOTES; i++)
            chord[i] = s->arp_latch[i];
        nch = s->arp_nlatch;
    }
    if (nch == 0u) {
        /* Nothing to play: release last-chord voices, park the clock. */
        for (i = 0u; i < RI_LEVI_NVOICES; i++) {
            uint32_t k, hit = 0u;
            for (k = 0u; k < s->arp_nchord; k++)
                if (s->v[i].active && s->v[i].note == s->arp_chord[k])
                    hit = 1u;
            if (hit)
                levi_release(s, i);
        }
        s->arp_nchord = 0u;
        s->arp_npend = 0u;
        s->arp_samp += n;
        return;
    }
    /* Chord change: restart the pattern (and the grid when locked). */
    {
        uint32_t same = nch == s->arp_nchord;
        for (i = 0u; same && i < nch; i++)
            if (chord[i] != s->arp_chord[i])
                same = 0u;
        if (!same) {
            uint8_t off = (uint8_t)(s->arpstepoff * 15u / 127u);
            uint8_t save_pos = s->darp.pos;
            uint32_t save_oct = s->arp_oct, save_cyc = s->arp_pos;
            for (i = 0u; i < nch; i++)
                s->arp_chord[i] = chord[i];
            s->arp_nchord = (uint8_t)nch;
            s->arp_lcg = (uint32_t)s->arp_samp ^ 0x9E3779B9u;
            arp_start_chord(s, chord, nch, mode, phrase,
                s->arpclock ? off : 0u, (uint8_t)len);
            s->darp.mode = mode;
            if (s->arpclock) {
                s->arp_t0 = s->arp_samp;
                s->arp_k = 0u;
            } else {
                /* Free-run: pattern position rides through the change. */
                s->darp.pos = save_pos;
                s->arp_oct = save_oct;
                s->arp_pos = save_cyc;
            }
        }
    }
    s->darp.mode = mode;
    s->darp.phrase = phrase;
    s->darp.uphr = &s->arp_uphr[0][0];
    /* Division to step duration (sub-audible rate steps nothing). */
    stepsq = RI_LEVI_ARP_STEPSQ(arp_eff(s, RI_LEVI_DA_DIVISION, s->arprate));
    if (stepsq == 0u) {
        s->arp_samp += n;
        return;
    }
    step_dur = sr * 60.0f / (s->tempo_bpm * (float)stepsq);
    if (!(step_dur >= 1.0f)) {
        s->arp_samp += n;
        return;
    }
    rats = 1u + (arp_eff(s, RI_LEVI_DA_RATCHET, s->arpratchet) * 3u + 63u) / 127u;
    /* Fire due strikes (block-granular note-ons; swung strikes wait). */
    for (;;) {
        uint64_t at = s->arp_t0 + (uint64_t)((float)s->arp_k * step_dur);
        uint8_t note = 0u;
        int rc, sub;
        uint32_t draw;
        /* Swing shifts odd strikes by up to half a step. */
        if ((s->arp_nstr & 1u) && eff_sw)
            at += (uint64_t)((float)eff_sw / 127.0f * 0.5f * step_dur);
        if (at >= s->arp_samp + n)
            break;
        /* Length wrap restarts the pattern. */
        if (s->arp_pos >= len) {
            arp_start_chord(s, chord, nch, mode, phrase, 0u, (uint8_t)len);
            s->darp.mode = mode;
        }
        /* Chance skips the strike (pattern still advances). */
        draw = arp_lcg_next(s);
        if ((draw & 127u) < eff_ch) {
            uint8_t tmp;
            ri_levi_arp_step(&s->darp, &tmp);
            s->arp_pos++;
            s->arp_oct++;
            s->arp_k++;
            continue;
        }
        rc = ri_levi_arp_step(&s->darp, &note);
        if (rc != 0) {   /* rest or empty: advance, no strike */
            s->arp_pos++;
            s->arp_oct++;
            s->arp_k++;
            continue;
        }
        /* Octave transpose + entropy leap. */
        if (octmode == 1u) {
            uint32_t tr = 12u * (s->arp_oct % octrange);
            note = tr > 127u - note ? 127u : (uint8_t)(note + tr);
        } else if (octmode == 2u) {
            uint32_t tr = 12u * (s->arp_oct % octrange);
            note = tr > note ? 0u : (uint8_t)(note - tr);
        }
        draw = arp_lcg_next(s);
        if (eff_en && (draw & 255u) < eff_en) {
            int nn = (int)note + ((draw & 256u) ? 12 : -12);
            note = (uint8_t)(nn < 0 ? 0 : nn > 127 ? 127 : nn);
        }
        /* Strike + ratchet subs (later subs queue for future blocks). */
        levi_note_strike(s, note);
        {
            int32_t sub_per = rats > 1u ? (int32_t)(step_dur / (float)rats) : (int32_t)step_dur;
            int32_t glen = gatepct >= 100u ? -1 : (int32_t)(gatepct * (uint32_t)(sub_per > 0 ? sub_per : 1) / 100u);
            arp_arm(s, note, glen);
            for (sub = 1; sub < (int)rats; sub++) {
                uint64_t sat = at + (uint64_t)((float)sub * step_dur / (float)rats);
                if (s->arp_npend < 16u) {
                    s->arp_pend_at[s->arp_npend] = sat;
                    s->arp_pend_note[s->arp_npend] = note;
                    s->arp_pend_off[s->arp_npend] = glen;
                    s->arp_npend++;
                }
            }
        }
        s->arp_pos++;
        s->arp_oct++;
        s->arp_nstr++;
        s->arp_k++;
    }
    s->arp_samp += n;
}

/* Effective seq UI (knob + DM_SEQ lead-voice offset, ±64 UI span). */
static uint8_t seq_eff(struct RILeviSet *s, uint32_t dp, uint8_t knob) {
    uint32_t lv;
    int x = (int)knob;
    for (lv = 0u; lv < RI_LEVI_NVOICES; lv++)
        if (s->v[lv].active)
            break;
    if (lv >= RI_LEVI_NVOICES)
        lv = 0u;
    if (dp < RI_LEVI_DS_N && s->v[lv].sxm_on)
        x += (int)(s->v[lv].sxm[dp] * 64.0f);
    if (x < 0)
        x = 0;
    if (x > 127)
        x = 127;
    return (uint8_t)x;
}

static uint32_t seq_lcg_next(struct RILeviSet *s) {
    s->seq_lcg = s->seq_lcg * 1664525u + 1013904223u;
    return s->seq_lcg >> 16;
}

/* Arm seq gate countdowns on voices sounding the struck note. */
static void seq_arm(struct RILeviSet *s, uint8_t note, int32_t len) {
    uint32_t v;
    if (len < 1)
        len = 1;
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        if (s->v[v].active && s->v[v].note == note) {
            s->seq_gate[v] = len;
            s->seq_gnote[v] = note;
        }
}

/* Record the held chord into a step (round-robin split over tracks). */
static void seq_record_step(struct RILeviSet *s, uint32_t idx) {
    uint8_t chord[RI_LEVI_ARP_MAXNOTES], nch = 0u;
    uint32_t i, j, n1 = 0u, n2 = 0u, m;
    struct RILeviSeqStep *t1, *t2;
    uint8_t trig;
    if (idx >= RI_LEVI_SEQ_STEPS)
        return;
    t1 = &s->seq_t1[idx];
    t2 = &s->seq_t2[idx];
    for (i = 0u; i < RI_LEVI_SEQ_NOTES; i++)
        t1->note[i] = t2->note[i] = 255u;
    nch = arp_chord(s, chord);
    for (i = 0u; i < nch; i++) {
        if ((i & 1u) == 0u) {
            if (n1 < RI_LEVI_SEQ_NOTES)
                t1->note[n1++] = chord[i];
        } else {
            if (n2 < RI_LEVI_SEQ_NOTES)
                t2->note[n2++] = chord[i];
        }
    }
    trig = (uint8_t)(1u + s->seqstrig * 3u / 127u);
    t1->vel = t2->vel = 100u;
    t1->gate = t2->gate = 100u;
    t1->trig = t2->trig = trig;
    t1->prob = t2->prob = s->seqsprob;
    t1->drift = t2->drift = (int8_t)((int)s->seqsdrift - 64);
    t1->entropy = t2->entropy = s->seqsentr;
    for (m = 0u; m < 8u; m++)
        s->seq_macro[m][idx] = s->mx.mknob[m];
    (void)j;
}

void levi_seq_block(struct RILeviSet *s, float sr, uint32_t n,
    uint32_t playing, uint64_t tick, uint32_t ppq) {
    uint32_t i, div, trklen, mode;
    uint64_t step_ticks;
    uint64_t tick_span = 0u;
    float spt;
    if (!s || !(sr > 0.0f) || n == 0u)
        return;
    if (ppq < 4u)
        ppq = 96u;
    /* Gate countdowns + pending subs run regardless (releases land). */
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        if (s->seq_gate[i] > 0) {
            s->seq_gate[i] -= (int32_t)n;
            if (s->seq_gate[i] <= 0) {
                s->seq_gate[i] = -1;
                if (s->v[i].active && s->v[i].note == s->seq_gnote[i])
                    levi_release(s, i);
            }
        }
    }
    for (i = 0u; i < s->seq_npend;) {
        if (s->seq_pend_tick[i] <= tick) {
            uint8_t nn = s->seq_pend_note[i];
            int32_t off = s->seq_pend_off[i];
            uint32_t j;
            levi_note_strike(s, nn);
            seq_arm(s, nn, off);
            s->seq_nstr++;
            s->seq_npend--;
            for (j = i; j < s->seq_npend; j++) {
                s->seq_pend_tick[j] = s->seq_pend_tick[j + 1u];
                s->seq_pend_note[j] = s->seq_pend_note[j + 1u];
                s->seq_pend_off[j] = s->seq_pend_off[j + 1u];
            }
        } else {
            i++;
        }
    }
    if (!s->seqon) {
        s->seq_samp += n;
        return;
    }
    div = seq_eff(s, RI_LEVI_DS_RATE, s->seqrate);
    div = div <= 42u ? 1u : div <= 85u ? 2u : 4u;
    trklen = 1u + seq_eff(s, RI_LEVI_DS_TRKLEN, s->seqtrklen);
    if (trklen > RI_LEVI_SEQ_STEPS)
        trklen = RI_LEVI_SEQ_STEPS;
    step_ticks = (uint64_t)ppq / div;
    if (step_ticks == 0u)
        step_ticks = 1u;
    spt = 60.0f * sr / (s->tempo_bpm * (float)ppq);
    if (!(spt > 0.0f))
        spt = 1.0f;
    /* Record: realtime captures crossed steps, stopped writes cursor. */
    if (s->seqrec) {
        if (playing) {
            int64_t idx = (int64_t)((tick / step_ticks) % trklen);
            int64_t k = s->seq_lastrec;
            uint32_t c = 0u;
            if (k < idx - (int64_t)RI_LEVI_SEQ_STEPS || k > idx)
                k = idx - 1;   /* jump/seek: record current only */
            while (k < idx && c < 16u) {
                k++;
                seq_record_step(s, (uint32_t)((uint64_t)k % trklen));
                c++;
            }
            s->seq_lastrec = idx;
        } else {
            uint32_t cur = s->seqstep;
            if (cur >= RI_LEVI_SEQ_STEPS)
                cur = RI_LEVI_SEQ_STEPS - 1u;
            seq_record_step(s, cur);
        }
    }
    if (!playing) {
        s->seq_samp += n;
        s->seq_tick = tick;
        s->seq_npend = 0u;   /* stopped: drop futures (gates still land above) */
        return;
    }
    /* Transport rewind/seek: catch up, never retrofire; drain stale
     * futures (queued strikes, armed gates) so a restart plays clean.
     * Already-ringing voices are left alone (steal reclaims them). */
    if (tick < s->seq_tick) {
        uint32_t g;
        s->seq_lastk = (int64_t)(tick / step_ticks) - 1;
        s->seq_npend = 0u;
        for (g = 0u; g < RI_LEVI_NVOICES; g++)
            s->seq_gate[g] = -1;
        tick_span = 0u;
    } else {
        tick_span = tick - s->seq_tick;
    }
    s->seq_tick = tick;
    mode = seq_eff(s, RI_LEVI_DS_MODE, s->seqmode);
    if (mode < 1u || mode > 2u) {
        s->seq_samp += n;
        return;
    }
    /* Schedule steps starting within reach of this block. Reach derives
     * from the actual tick advance (any host granularity): steps the
     * block overlaps, plus one tick of slack. Each step schedules once
     * (lastk); a forward jump wider than 16 steps drops the stale span
     * (hitch recovery) instead of piling late strikes. */
    {
        int64_t k_hi = (int64_t)((tick + tick_span + 1u) / step_ticks);
        int64_t k = s->seq_lastk + 1;
        if (k < 0)
            k = 0;
        if (k_hi - k > 16)
            k = k_hi;
        for (; k <= k_hi; k++) {
            uint64_t idx;
            float at;
            uint32_t sw, mgate, mprob, mdrift, mtransp, loop;
            int fire_t1, fire_t2;
            s->seq_lastk = k;
            idx = ((uint64_t)k % trklen + trklen) % trklen;
            loop = ((uint64_t)k / trklen) & 1u;
            fire_t1 = mode == 1u || (mode == 2u && loop == 0u);
            fire_t2 = mode == 1u || (mode == 2u && loop != 0u);
            if (!fire_t1 && !fire_t2)
                continue;
            sw = seq_eff(s, RI_LEVI_DS_SWING, s->seqswing);
            mgate = seq_eff(s, RI_LEVI_DS_GATE, s->seqgate);
            mprob = seq_eff(s, RI_LEVI_DS_PROB, s->seqprob);
            mdrift = seq_eff(s, RI_LEVI_DS_DRIFT, s->seqdrift);
            mtransp = seq_eff(s, RI_LEVI_DS_TRANSP, s->seqtransp);
            at = (float)((uint64_t)k * step_ticks);
            if ((idx & 1u) && sw)
                at += (float)sw / 127.0f * 0.5f * (float)step_ticks;
            /* Fire each active track; the macro lane follows only steps
             * that strike (rests must not stomp the live knobs, or a
             * later record would capture the stomp). */
            {
                uint32_t t, fired = 0u;
                for (t = 0u; t < 2u; t++) {
                    struct RILeviSeqStep *st = t ? &s->seq_t2[idx] : &s->seq_t1[idx];
                    uint32_t nn, trig;
                    int32_t drift_t = (int32_t)st->drift + (int32_t)mdrift - 64;
                    float nat = at + (float)drift_t;
                    uint32_t thr;
                    if ((t == 0u && !fire_t1) || (t == 1u && !fire_t2))
                        continue;
                    thr = (uint32_t)st->prob * mprob / 127u;
                    if (thr < 127u && (seq_lcg_next(s) & 127u) >= thr)
                        continue;
                    trig = st->trig;
                    if (trig < 1u)
                        trig = 1u;
                    if (trig > 4u)
                        trig = 4u;
                    for (nn = 0u; nn < RI_LEVI_SEQ_NOTES; nn++) {
                        uint8_t on = st->note[nn];
                        int tr = (int)on + ((int)mtransp - 64) * 48 / 127;
                        int sub;
                        if (on >= 250u)
                            continue;
                        if (st->entropy) {
                            uint32_t er = seq_lcg_next(s);
                            tr += (int)((er % 25u) - 12u) * (int)st->entropy / 127;
                        }
                        if (tr < 0)
                            tr = 0;
                        if (tr > 127)
                            tr = 127;
                        for (sub = 0; sub < (int)trig; sub++) {
                            float sat = nat + (trig > 1u ? (float)sub * (float)step_ticks / (float)trig : 0.0f);
                            int32_t glen;
                            float step_samp = (float)step_ticks * spt / (float)trig;
                            glen = (int32_t)((float)st->gate / 127.0f * (float)mgate / 127.0f * step_samp);
                            if (glen < 1)
                                glen = 1;
                            if (sat > (float)tick) {
                                /* Beyond now: queue for a later block. */
                                if (s->seq_npend < 64u) {
                                    s->seq_pend_tick[s->seq_npend] = (uint64_t)(sat + 0.5f);
                                    s->seq_pend_note[s->seq_npend] = (uint8_t)tr;
                                    s->seq_pend_off[s->seq_npend] = glen;
                                    s->seq_npend++;
                                    fired = 1u;
                                }
                                continue;
                            }
                            levi_note_strike(s, (uint8_t)tr);
                            seq_arm(s, (uint8_t)tr, glen);
                            s->seq_nstr++;
                            fired = 1u;
                        }
                    }
                }
                if (fired) {
                    uint32_t m;
                    for (m = 0u; m < 8u; m++)
                        s->mx.mknob[m] = s->seq_macro[m][idx];
                }
            }
        }
    }
    s->seq_samp += n;
}
