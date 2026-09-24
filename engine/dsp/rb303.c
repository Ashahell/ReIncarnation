/* rb303.c — 303 voice + Appendix B candidate filter (Task 4, gate G4).
 * Implements the ledger claim docs/evidence/303/filter-candidate.md (E0,
 * UNVALIDATED): core recurrence verbatim, DRIVE=1.0, 2x oversample with
 * linear-interp input, feedback-loop one-pole HP 150 Hz, post one-pole HPs
 * 44.486/24.167 Hz. Post allpass 14.008 + notch 7.5164/BW4.7 DEFERRED (M2.2).
 * Voice chain (VCO saw/square, exp envelopes, VCA clamp 1.2, no retrigger on
 * slide) follows spec §9 placeholder formulas (P-01..P-06 hypotheses).
 * Kernels only (ri_tanh/ri_exp/ri_pow2/ri_sin); no platform transcendentals.
 * Bounded loops only. No allocation. Denormal note: ladder states flush to
 * zero per-sample by comparison (skeleton simplification of the ledger's
 * per-block dither guard; M2.2 revisits).
 */
#include "engine/dsp/rb303.h"
#include "engine/dsp/kernels.h"

#define RI_303_DRIVE 1.0f      /* [HYPOTHESIS] ledger D-claim */
#define RI_303_FB_HP_HZ 150.0f /* feedback-loop HPF (spec §9 E2 default) */
#define RI_303_POST_HP1_HZ 44.486f
#define RI_303_POST_HP2_HZ 24.167f
#define RI_303_RESO_MAX 3.8f  /* P-04 */
#define RI_303_VCA_CLAMP 1.2f /* P-02 */
#define RI_303_PI 3.14159265f

/* tan(a) via allowlisted kernels: sin(a)/sin(a + pi/2). Caller keeps
 * a small (|a| <= pi/12 after the fc < fs/6 clamp), well inside ri_sin. */
static float ri_tan_small(float a) {
    float c = ri_sin(a + RI_303_PI * 0.5f);
    if (c == 0.0f)
        return 0.0f;
    return ri_sin(a) / c;
}

static float clampf(float x, float lo, float hi) {
    if (x < lo)
        return lo;
    if (x > hi)
        return hi;
    return x;
}

/* Accent-sweep charge: saturating buildup, so consecutive accents peak
 * higher (the "wow" on accent runs). Discharge is per-sample in render
 * with a reso-dependent lag. */
static void sweep_hit(struct RB303Voice *v) {
    v->sweep += v->accent_amt * (1.0f - v->sweep);
}

/* MIDI note -> Hz: 440 * 2^((m-69)/12). Exponent in [-5.75, 4.83]. */
static float midi_to_hz(uint8_t m) {
    return 440.0f * ri_pow2(((float)m - 69.0f) / 12.0f);
}

static float ftz(float x) {
    if (x < 1e-20f && x > -1e-20f)
        return 0.0f;
    return x;
}

void rb303_init(struct RB303Voice *v) {
    v->cutoff_hz = 1000.0f;
    v->reso_k = 0.0f;
    v->env_mod = 1.0f;
    v->decay_tau = 0.6f;
    v->accent_amt = 0.8f;
    v->slide_tc = 0.040f; /* P-03 candidate default; M2.2 A/B vs 60 ms */
    v->volume = 1.0f;
    v->tune_st = 0.0f;
    v->wave_square = 0;
    v->classic_click = 1;
    v->wave_rendered = 0;
    v->xfade_n = 0u;
    v->s0 = 0.0f;
    v->s1 = 0.0f;
    v->s2 = 0.0f;
    v->fb_lp = 0.0f;
    v->post_lp1 = 0.0f;
    v->post_lp2 = 0.0f;
    v->phase = 0.0f;
    v->freq = 55.0f;
    v->target_freq = 55.0f;
    v->meg = 0.0f;
    v->veg = 0.0f;
    v->sweep = 0.0f;
    v->accented = 0;
    v->accent_env = 0.0f;
    v->prev_in = 0.0f;
    v->gate = 0.0f;
}

void rb303_set_cutoff_hz(struct RB303Voice *v, float fc) {
    v->cutoff_hz = fc;
}

void rb303_set_reso(struct RB303Voice *v, float k) {
    v->reso_k = k;
}

/* One Appendix B core step, 2x oversampled: linear-interp input between
 * prev/current, two ladder substeps at rate 2*sr, emit the second. */
float rb303_filter_step(struct RB303Voice *v, float in, float sr) {
    float fc = clampf(v->cutoff_hz, 1.0f, sr / 6.0f);
    float k = clampf(v->reso_k, 0.0f, RI_303_RESO_MAX);
    /* Appendix B literal: g = tan(pi*fc/fs) at the BASE rate; the two
     * substeps run at 2x with linear-interp input. (An earlier reading used
     * the oversampled rate in g and drooped -0.57 dB at f/fc=1/8; the base-
     * rate form is flatter and keeps g <= tan(pi/6) under the fc < fs/6
     * clamp — see ledger D1.) */
    float g = ri_tan_small(RI_303_PI * fc / sr);
    float ghp = ri_tan_small(RI_303_PI * RI_303_FB_HP_HZ / sr);
    float mid = (v->prev_in + in) * 0.5f;
    float xc[2];
    int j;
    xc[0] = mid;
    xc[1] = in;
    for (j = 0; j < 2; j++) {
        float tap = k * v->s2;
        float fb;
        float x;
        v->fb_lp += ghp * (tap - v->fb_lp);
        fb = tap - v->fb_lp;
        x = ri_tanh(RI_303_DRIVE * (xc[j] - fb));
        v->s0 = ftz(v->s0 + g * (x - ri_tanh(v->s0)));
        v->s1 = ftz(v->s1 + g * (ri_tanh(v->s0) - ri_tanh(v->s1)));
        v->s2 = ftz(v->s2 + g * (ri_tanh(v->s1) - ri_tanh(v->s2)));
    }
    v->prev_in = in;
    return v->s2;
}

void rb303_note(struct RB303Voice *v, uint8_t midi, int slide, int accent) {
    v->target_freq = midi_to_hz(midi) * ri_pow2(v->tune_st / 12.0f);
    v->gate = 1.0f;
    if (!slide) {
        v->freq = v->target_freq; /* pitch jumps */
        v->meg = 1.0f;            /* envelopes restart */
        v->veg = 1.0f;
        v->phase = 0.0f;
    }
    /* slide: gate stays high, no env reset (§8 table); pitch slews in render */
    v->accented = accent ? 1 : 0;
    if (accent) {
        v->accent_env = 1.0f;
        sweep_hit(v);
    }
}

void rb303_slide_to(struct RB303Voice *v, uint8_t midi) {
    v->target_freq = midi_to_hz(midi) * ri_pow2(v->tune_st / 12.0f); /* CONTINUE: target only, nothing reset */
}

void rb303_accent(struct RB303Voice *v) {
    v->accent_env = 1.0f;
    sweep_hit(v);
}

void rb303_release(struct RB303Voice *v) {
    v->gate = 0.0f; /* gate low; pitch holds, envelope takes the release ramp */
}

/* Full voice render: VCO -> env -> VCA -> ladder -> post HPs -> volume. */
void rb303_render(struct RB303Voice *v, float *out, uint32_t n, float sr) {
    float slide_a, meg_a, veg_a, rel_a, acc_a, sw_a;
    float g1, g2;
    uint32_t i;
    uint32_t xf_total; /* 0.5 ms crossfade length, samples (TC-2.2.5) */
    float meg_tau = v->accented ? RI_303_MEG_ACC_TAU : v->decay_tau;
    float veg_tau = v->accented ? RI_303_VEG_ACC_TAU : RI_303_VEG_TAU;
    float rel_tau = v->accented ? RI_303_REL_ACC_TAU : RI_303_REL_TAU;
    float sw_tau = 0.05f + v->reso_k * 0.05f; /* accent-sweep lag follows
        resonance (E0); reso_k here is the base (per-sample mod copies it) */
    if (v->slide_tc * sr > 1.0f)
        slide_a = 1.0f - ri_exp(-1.0f / (v->slide_tc * sr));
    else
        slide_a = 1.0f;
    if (meg_tau * sr > 1.0f)
        meg_a = ri_exp(-1.0f / (meg_tau * sr));
    else
        meg_a = 0.0f;
    if (veg_tau * sr > 1.0f)
        veg_a = ri_exp(-1.0f / (veg_tau * sr));
    else
        veg_a = 0.0f;
    if (rel_tau * sr > 1.0f)
        rel_a = ri_exp(-1.0f / (rel_tau * sr));
    else
        rel_a = 0.0f;
    acc_a = ri_exp(-1.0f / (0.060f * sr)); /* P-02 accent 60 ms */
    if (sw_tau * sr > 1.0f)
        sw_a = ri_exp(-1.0f / (sw_tau * sr));
    else
        sw_a = 0.0f;
    xf_total = (uint32_t)(0.0005f * sr + 0.5f); /* 24 @48 kHz */
    if (xf_total < 1u)
        xf_total = 1u;
    g1 = ri_tan_small(RI_303_PI * RI_303_POST_HP1_HZ / sr);
    g2 = ri_tan_small(RI_303_PI * RI_303_POST_HP2_HZ / sr);
    for (i = 0; i < n; i++) {
        float osc, vca, y, fc_save;
        /* slide slew (fixed tau rate) */
        v->freq += (v->target_freq - v->freq) * slide_a;
        v->phase += v->freq / sr;
        if (v->phase >= 1.0f)
            v->phase -= 1.0f;
        /* TC-2.2.5 waveform-switch click flag: classic hard-switches
         * (deterministic click); otherwise 0.5 ms crossfade. */
        {
            int wnow = v->wave_square ? 1 : 0;
            float o_new = wnow ? ((v->phase < 0.5f) ? 0.5f : -0.5f)
                               : (2.0f * v->phase - 1.0f);
            if (wnow != v->wave_rendered && !v->classic_click) {
                float o_old = v->wave_rendered
                    ? ((v->phase < 0.5f) ? 0.5f : -0.5f)
                    : (2.0f * v->phase - 1.0f);
                v->xfade_n++;
                if (v->xfade_n >= xf_total) {
                    v->wave_rendered = wnow;
                    v->xfade_n = 0u;
                    osc = o_new;
                } else {
                    osc = o_old + (o_new - o_old) *
                        ((float)v->xfade_n / (float)xf_total);
                }
            } else {
                v->wave_rendered = wnow;
                v->xfade_n = 0u;
                osc = o_new;
            }
        }
        /* Dual envelopes (§12.4b): MEG (filter) follows the Decay knob
         * (minimum on accents); VEG (amp) is fixed and long; the gate
         * closes the VCA via the release ramp while MEG keeps its note
         * decay. Slide leaves both untouched per §8. */
        if (v->gate > 0.5f) {
            v->meg *= meg_a;
            v->veg *= veg_a;
        } else {
            v->veg *= rel_a;
        }
        v->sweep *= sw_a;
        v->accent_env *= acc_a;
        vca = v->veg * (1.0f + v->accent_amt * v->accent_env);
        if (vca > RI_303_VCA_CLAMP)
            vca = RI_303_VCA_CLAMP;
        /* cutoff mod: fc_base * 2^(meg*envmod + accent sweep); reso +15% */
        fc_save = v->cutoff_hz;
        v->cutoff_hz = fc_save * ri_pow2(v->meg * v->env_mod + v->sweep * 0.5f);
        {
            float k_save = v->reso_k;
            v->reso_k = k_save * (1.0f + 0.15f * v->accent_env);
            y = rb303_filter_step(v, osc * vca, sr);
            v->reso_k = k_save;
        }
        v->cutoff_hz = fc_save;
        /* post HP chain */
        v->post_lp1 += g1 * (y - v->post_lp1);
        y -= v->post_lp1;
        v->post_lp2 += g2 * (y - v->post_lp2);
        y -= v->post_lp2;
        out[i] = v->volume * y;
    }
}
