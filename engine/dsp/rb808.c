/* rb808.c — 808 fifteen-voice set (Task 8, gate G8).
 * Implements the Appendix C skeleton per voice (E0 candidates, HYPOTHESIS
 * P-07..P-12); per-voice rows in docs/evidence/808/<voice>.md.
 * Accent = excitation pre-envelope: osc/noise sources are scaled by
 * EXCITE(accent) at the source; saturation drive is kept low (peak |x| <=
 * ~0.35) so pre-envelope scaling reads as x1.5 (+3.52 dB) at the output.
 * Determinism: fixed LFSR seeds at trigger (D1); kernels ri_* only.
 */
#include "engine/dsp/rb808.h"
#include "engine/dsp/kernels.h"

#define RI_808_PI 3.14159265f
#define RI_808_TWO_PI 6.2831853f

const float RI_808_METAL_RATIO[6] = { 1.0f, 1.30f, 1.62f, 1.93f, 2.27f, 2.63f };

static const char *const RI_808_NAMES[RI_808_NVOICES] = {
    "bd", "sd", "lt", "mt", "ht", "lc", "mc", "hc",
    "rs", "cl", "cp", "ch", "oh", "cy", "cb"
};

const char *rb808_name(uint32_t voice) {
    if (voice < RI_808_NVOICES)
        return RI_808_NAMES[voice];
    return "?";
}

static float clampf(float x, float lo, float hi) {
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

static float ftz(float x) {
    if (x < 1e-20f && x > -1e-20f)
        return 0.0f;
    return x;
}

/* Deterministic white noise in [-1,1): xorshift32, bounded (no loops). */
static float lfsr_next(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return (float)(x >> 8) * (1.0f / 8388608.0f) - 1.0f;
}

static float square(float s) {
    return (s >= 0.0f) ? 0.05f : -0.05f;
}

/* One-pole coefficients via the allowlisted kernel (spec §10: no libm). */
static float lp_a(float fc, float sr) {
    return 1.0f - ri_exp(-RI_808_TWO_PI * fc / sr);
}

float rb808_excite(uint32_t voice, uint32_t accent) {
    (void)voice; /* one excitation law for all 15 voices (ledger rows note
                  * per-voice source mapping; P-12 three-state OPEN) */
    if (accent == 0u)
        return 1.0f;
    return RI_808_EXCITE_ACC; /* accent 1, and reserved 2 (binary hold) */
}

float rb808_pitch_hz(uint32_t voice, float t, float tune_st) {
    float f = RI_808_FLOOR_HZ;
    float tune = clampf(tune_st, -7.0f, 7.0f);
    float fstart;
    if (voice == RB808_BD) {
        fstart = RI_808_BD_F_START * ri_pow2(tune / 12.0f);
        f = RI_808_BD_F_END + (fstart - RI_808_BD_F_END) * ri_exp(-t / RI_808_BD_TAU_PITCH);
    } else if (voice == RB808_LT) {
        f = RI_808_TOM_LT_F0 * (RI_808_TOM_F1_RATIO +
            (1.0f - RI_808_TOM_F1_RATIO) * ri_exp(-t / 0.030f));
    } else if (voice == RB808_MT) {
        f = RI_808_TOM_MT_F0 * (RI_808_TOM_F1_RATIO +
            (1.0f - RI_808_TOM_F1_RATIO) * ri_exp(-t / 0.030f));
    } else if (voice == RB808_HT) {
        f = RI_808_TOM_HT_F0 * (RI_808_TOM_F1_RATIO +
            (1.0f - RI_808_TOM_F1_RATIO) * ri_exp(-t / 0.030f));
    } else if (voice == RB808_SD) {
        f = RI_808_SD_F1;
    } else if (voice == RB808_LC) {
        f = RI_808_CONGA_LC_F;
    } else if (voice == RB808_MC) {
        f = RI_808_CONGA_MC_F;
    } else if (voice == RB808_HC) {
        f = RI_808_CONGA_HC_F;
    } else if (voice == RB808_RS) {
        f = 800.0f;
    } else if (voice == RB808_CL) {
        f = 1100.0f;
    } else if (voice == RB808_CP) {
        f = RI_808_CLAP_BP_F;
    } else if (voice == RB808_CB) {
        f = RI_808_CB_F2;
    } else { /* CH/OH/CY: metal cluster base (partials = base * ratio) */
        f = RI_808_METAL_BASE;
    }
    if (f < RI_808_FLOOR_HZ)
        f = RI_808_FLOOR_HZ;
    return f;
}

static float default_tau(uint32_t voice) {
    switch (voice) {
    case RB808_BD: return 0.5f;
    case RB808_SD: return 0.25f;
    case RB808_LT: case RB808_MT: case RB808_HT: return 0.4f;
    case RB808_LC: case RB808_MC: case RB808_HC: return 0.3f;
    case RB808_RS: return 0.03f;
    case RB808_CL: return 0.04f;
    case RB808_CP: return 1.0f; /* envelope inherent in burst/tail fn */
    case RB808_CH: return RI_808_CH_TAU;
    case RB808_OH: return 0.4f;
    case RB808_CY: return RI_808_CY_TAU;
    case RB808_CB: return 0.2f;
    default: return 0.3f;
    }
}

static float max_tau(uint32_t voice) {
    switch (voice) {
    case RB808_BD: return 2.8f; /* P-07 tau_amp max */
    case RB808_SD: return 0.5f;
    case RB808_LT: case RB808_MT: case RB808_HT: return 1.0f;
    case RB808_LC: case RB808_MC: case RB808_HC: return 0.8f;
    case RB808_RS: case RB808_CL: return 0.1f;
    case RB808_CP: return 1.0f;
    case RB808_CH: return 0.1f;
    case RB808_OH: return 1.2f; /* P-10 OH range 0.2..1.2 */
    case RB808_CY: return 2.5f;
    case RB808_CB: return 0.5f;
    default: return 0.5f;
    }
}

void rb808_init_set(struct RB808Set *s) {
    uint32_t i, k;
    s->triggered = 0u;
    for (i = 0; i < RI_808_NVOICES; i++) {
        s->v[i].id = (uint8_t)i;
        s->v[i].accent = 0u;
        s->v[i].active = 0u;
        s->v[i].pad = 0u;
        s->v[i].t = 0.0f;
        s->v[i].tune_st = 0.0f;
        s->v[i].tau_amp = default_tau(i);
        s->v[i].phase = 0.0f;
        s->v[i].phase2 = 0.0f;
        for (k = 0; k < 6; k++)
            s->v[i].mph[k] = 0.0f;
        s->v[i].rng = 0u;
        s->v[i].st_hp = 0.0f;
        s->v[i].st_lp = 0.0f;
        s->v[i].tail = 1.0f;
    }
}

void rb808_trigger(struct RB808Set *s, uint32_t voice, uint32_t accent, float tune_st) {
    struct RB808Voice *v;
    uint32_t k;
    if (voice >= RI_808_NVOICES)
        return;
    v = &s->v[voice];
    v->accent = (uint8_t)(accent & 3u);
    v->active = 1u;
    /* Appendix C TRIGGER: phase=0; env_pitch=1; env_amp=1 (implicit in t=0). */
    v->t = 0.0f;
    v->tune_st = clampf(tune_st, -7.0f, 7.0f);
    v->phase = 0.0f;
    v->phase2 = 0.0f;
    for (k = 0; k < 6; k++)
        v->mph[k] = 0.0f;
    v->rng = 0x12345678u + (uint32_t)voice * 0x9e3779b9u; /* fixed seed: D1 */
    v->st_hp = 0.0f;
    v->st_lp = 0.0f;
    s->triggered |= (uint16_t)(1u << voice);
}

void rb808_set_decay(struct RB808Set *s, uint32_t voice, float tau_amp) {
    if (voice >= RI_808_NVOICES)
        return;
    s->v[voice].tau_amp = clampf(tau_amp, 0.005f, 4.0f);
}

void rb808_max_decay(struct RB808Set *s) {
    uint32_t i;
    for (i = 0; i < RI_808_NVOICES; i++)
        s->v[i].tau_amp = max_tau(i);
}

/* Sine osc helper: phase 0..1 -> ri_sin radians. */
static float osc_sin(float phase) {
    return ri_sin(phase * RI_808_TWO_PI);
}

/* One sample per voice family. Each follows Appendix C:
 * f = PITCH (via rb808_pitch_hz), osc, nz -> FILTER, y = MIX -> SATURATE,
 * out = y * env_amp, with excitation pre-scaled at the sources. t advances
 * by 1/sr. Inactive voices return 0 without advancing. */
float rb808_voice_render(struct RB808Voice *v, float sr) {
    float exc, env, out = 0.0f;
    uint32_t id;
    if (!v->active)
        return 0.0f;
    id = v->id;
    exc = rb808_excite(id, v->accent);
    if (v->tau_amp * sr > 1.0f)
        env = ri_exp(-v->t / v->tau_amp);
    else
        env = 0.0f;

    if (id == RB808_BD) {
        /* P-07: decaying sine + 6 ms attack transient + HP click. */
        float f = rb808_pitch_hz(id, v->t, v->tune_st);
        float osc, click = 0.0f, y;
        v->phase += f / sr;
        if (v->phase >= 1.0f)
            v->phase -= 1.0f;
        osc = exc * 0.16f * osc_sin(v->phase);
        if (v->t < 0.006f)
            click = exc * 0.08f * ri_exp(-v->t / 0.0012f)
                * ri_sin(RI_808_TWO_PI * 2500.0f * v->t); /* radians form */
        y = ri_tanh(osc + click);
        out = y * env;
    } else if (id == RB808_SD) {
        /* P-08: two partials + BP noise (HP 1.4k + LP 2.3k cascade). */
        float p1, p2, nz, h, y;
        float a_hp = lp_a(1400.0f, sr), a_lp = lp_a(2300.0f, sr);
        v->phase += RI_808_SD_F1 / sr;
        if (v->phase >= 1.0f)
            v->phase -= 1.0f;
        v->phase2 += RI_808_SD_F2 / sr;
        if (v->phase2 >= 1.0f)
            v->phase2 -= 1.0f;
        p1 = exc * 0.13f * osc_sin(v->phase);
        p2 = exc * 0.09f * osc_sin(v->phase2);
        nz = lfsr_next(&v->rng);
        v->st_hp = ftz(v->st_hp + a_hp * (nz - v->st_hp));
        h = nz - v->st_hp;
        v->st_lp = ftz(v->st_lp + a_lp * (h - v->st_lp));
        y = ri_tanh(p1 + p2);
        out = y * env + exc * 0.11f * v->st_lp * ri_exp(-v->t / 0.09f);
    } else if (id == RB808_LT || id == RB808_MT || id == RB808_HT ||
               id == RB808_LC || id == RB808_MC || id == RB808_HC) {
        /* P-09 sweep family (toms) / fixed congas. */
        float f = rb808_pitch_hz(id, v->t, 0.0f);
        float osc, y;
        v->phase += f / sr;
        if (v->phase >= 1.0f)
            v->phase -= 1.0f;
        osc = exc * 0.24f * osc_sin(v->phase);
        y = ri_tanh(osc);
        out = y * env;
    } else if (id == RB808_RS || id == RB808_CL) {
        /* RS-family short BP pulse + click. */
        float f = rb808_pitch_hz(id, v->t, 0.0f);
        float a_hp = lp_a(500.0f, sr), a_lp = lp_a(2500.0f, sr);
        float osc, h, y;
        v->phase += f / sr;
        if (v->phase >= 1.0f)
            v->phase -= 1.0f;
        osc = exc * 0.20f * osc_sin(v->phase);
        if (v->t < 0.001f)
            osc += exc * 0.12f * ri_exp(-v->t / 0.0004f);
        v->st_hp = ftz(v->st_hp + a_hp * (osc - v->st_hp));
        h = osc - v->st_hp;
        v->st_lp = ftz(v->st_lp + a_lp * (h - v->st_lp));
        y = ri_tanh(v->st_lp); /* unity drive: accent ratio stays x1.5 */
        out = y * env;
    } else if (id == RB808_CP) {
        /* P-11: 3+1 bursts (2 ms gates at 0/9/18 ms, tail onset at 27 ms
         * = the 4th hump) + tail tau 120 ms.
         * Noise through HP 800 + LP 1600 (≈1.1 kHz Q2.5 region). */
        float a_hp = lp_a(800.0f, sr), a_lp = lp_a(1600.0f, sr);
        float nz, h, g = 0.0f, tail_e, drive, y;
        int k;
        for (k = 0; k < 3; k++) {
            float tk = (float)k * RI_808_CLAP_BURST_GAP;
            if (v->t >= tk && v->t < tk + RI_808_CLAP_BURST_W)
                g = 1.0f;
        }
        if (v->t >= 3.0f * RI_808_CLAP_BURST_GAP)
            tail_e = ri_exp(-(v->t - 3.0f * RI_808_CLAP_BURST_GAP) / RI_808_CLAP_TAIL_TAU);
        else
            tail_e = 0.0f;
        nz = lfsr_next(&v->rng);
        v->st_hp = ftz(v->st_hp + a_hp * (nz - v->st_hp));
        h = nz - v->st_hp;
        v->st_lp = ftz(v->st_lp + a_lp * (h - v->st_lp));
        drive = exc * 0.30f * (g + tail_e * v->tail);
        y = ri_tanh(v->st_lp * 2.0f * drive);
        out = y; /* envelope inherent in the burst/tail function */
    } else if (id == RB808_CH || id == RB808_OH || id == RB808_CY) {
        /* P-10: six-square cluster + HP (7 kHz hats; 5 kHz cymbal). */
        float base = (id == RB808_CY) ? 250.0f : RI_808_METAL_BASE;
        float hpf = (id == RB808_CY) ? 5000.0f : RI_808_HP_METAL;
        float a_hp = lp_a(hpf, sr);
        float sum = 0.0f, h, y;
        uint32_t k;
        for (k = 0; k < 6; k++) {
            v->mph[k] += base * RI_808_METAL_RATIO[k] / sr;
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            sum += square(osc_sin(v->mph[k]));
        }
        sum *= exc;
        v->st_hp = ftz(v->st_hp + a_hp * (sum - v->st_hp));
        h = sum - v->st_hp;
        y = ri_tanh(h * 1.2f);
        out = y * env;
        if (id == RB808_OH) {
            /* OH bleed: short HP-noise transient (E0, ledger row OH). */
            float nz = lfsr_next(&v->rng);
            v->st_lp = ftz(v->st_lp + a_hp * (nz - v->st_lp));
            out += exc * 0.04f * (nz - v->st_lp) * ri_exp(-v->t / 0.035f);
        }
    } else { /* RB808_CB: 540 + 800 Hz squares + 800 Hz BP. */
        float a_hp = lp_a(600.0f, sr), a_lp = lp_a(1100.0f, sr);
        float s1, s2, h, y;
        v->phase += RI_808_CB_F1 / sr;
        if (v->phase >= 1.0f)
            v->phase -= 1.0f;
        v->phase2 += RI_808_CB_F2 / sr;
        if (v->phase2 >= 1.0f)
            v->phase2 -= 1.0f;
        s1 = square(osc_sin(v->phase));
        s2 = square(osc_sin(v->phase2));
        v->st_hp = ftz(v->st_hp + a_hp * ((s1 + s2) - v->st_hp));
        h = (s1 + s2) - v->st_hp;
        v->st_lp = ftz(v->st_lp + a_lp * (h - v->st_lp));
        y = ri_tanh(exc * v->st_lp * 1.5f);
        out = y * env;
    }
    v->t += 1.0f / sr;
    return out;
}

void rb808_render_mix(struct RB808Set *s, float *out, uint32_t n, float sr) {
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        float m = 0.0f;
        for (k = 0; k < RI_808_NVOICES; k++)
            m += rb808_voice_render(&s->v[k], sr);
        /* Master safety: soft-clip the storm sum (linear under 1.0). */
        if (m > 1.0f || m < -1.0f)
            m = ri_tanh(m * 0.5f) * 1.4f;
        out[i] = m;
    }
}
