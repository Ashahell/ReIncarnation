/* rb909.c — 909 layered sampler (Task 9, gate G9).
 * Implements the Appendix C skeleton per voice (E0 candidate, HYPOTHESIS
 * P-13..P-15); per-voice rows in docs/evidence/909/<voice>.md.
 * Tune/morph split: rb909_pitch_mult (sample clock), rb909_layer_weight
 * (triangular blend, P-13) and accent/flam (P-14/P-05) are separate
 * definitions sharing no state. Determinism: baked layers + fixed
 * playheads (D1); no RNG, no globals. Kernels ri_* only.
 */
#include "engine/dsp/rb909.h"
#include "engine/dsp/kernels.h"

#define RI_909_PI 3.14159265f
#define RI_909_REF_RATE 48000.0f

static const char *const RI_909_NAMES[RI_909_NVOICES] = {
    "bd", "sd", "ch", "oh", "cr", "rd"
};

const char *rb909_name(uint32_t voice) {
    if (voice < RI_909_NVOICES)
        return RI_909_NAMES[voice];
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

float rb909_pitch_mult(uint8_t tune) {
    /* Sample clock: one octave per 48 knob steps through tune 64. */
    return ri_pow2(((float)tune - 64.0f) / 48.0f);
}

float rb909_layer_weight(uint8_t tune, uint8_t lo, uint8_t hi) {
    /* Triangular feather: full 1.0 inside [lo+HALF, hi-HALF], linear
     * ramps of total width 2*HALF = 8 knob positions (P-13) across
     * each boundary; un-normalized (ri_layer_mix normalizes). */
    float t = (float)tune;
    float wlo, whi;
    float edge = (float)RI_909_XFADE_HALF * 2.0f; /* 8 positions */
    if (hi < lo)
        return 0.0f;
    wlo = (t - ((float)lo - (float)RI_909_XFADE_HALF)) / edge;
    whi = (((float)hi + (float)RI_909_XFADE_HALF) - t) / edge;
    wlo = clampf(wlo, 0.0f, 1.0f);
    whi = clampf(whi, 0.0f, 1.0f);
    return (wlo < whi) ? wlo : whi;
}

float rb909_accent_gain(const struct RB909Voice *v) {
    if (v->accent_noop)
        return 1.0f; /* CR/RD quirk: accent is a no-op (P-14) */
    if (v->accent == 0u)
        return 1.0f;
    return RI_909_ACC1_GAIN; /* acc1, and acc2-on-non-flam (= acc1) */
}

float rb909_decay_scale(uint8_t voice, uint8_t tune) {
    /* Crash/ride decay shortens with Tune (extra envelope tau scale);
     * all other voices 1.0 (sample-clock shortening is automatic). */
    if (voice == RB909_CR || voice == RB909_RD)
        return ri_pow2(-(((float)tune - 64.0f) / 48.0f));
    return 1.0f;
}

/* Bounded Newton-sqrt on plain arithmetic (10 fixed iterations, no
 * libm, no branches on the value): the layer mixer needs an equal-power
 * normalization and the allowlisted kernel set has no sqrt. Bit-exact
 * for the same binary (D1); ~1e-7 relative for inputs in (0, 4]. */
static float sqrt_nr(float a) {
    float x = 0.5f * (a + 1.0f);
    int i;
    if (a <= 0.0f)
        return 0.0f;
    for (i = 0; i < 10; i++)
        x = 0.5f * (x + a / x);
    return x;
}

float ri_resample_linear(const float *d, uint32_t n, float pos) {
    uint32_t i;
    float frac;
    if (!d || n == 0u)
        return 0.0f;
    if (pos <= 0.0f)
        return d[0];
    if (pos >= (float)(n - 1u))
        return (pos < (float)n) ? d[n - 1u] : 0.0f;
    i = (uint32_t)pos;
    frac = pos - (float)i;
    return d[i] + frac * (d[i + 1u] - d[i]);
}

float ri_layer_mix(const struct RISampleLayer *L, uint32_t n,
    uint8_t tune, float pos) {
    /* Equal-power normalization (P-13 RMS continuity): triangular
     * weights (spec-exact) with u_i = sqrt(w_i / wsum), so an equal
     * mix of uncorrelated layers holds RMS constant across the morph.
     * Single-layer path yields u = 1 (bit-identical to sum norm).
     * Two passes over <= 4 layers (bounded): collect, then blend. */
    float xs[RI_909_MAX_LAYERS];
    float ws[RI_909_MAX_LAYERS];
    float acc = 0.0f, wsum = 0.0f;
    uint32_t k, m = 0;
    if (!L || n == 0u)
        return 0.0f;
    if (n > RI_909_MAX_LAYERS)
        n = RI_909_MAX_LAYERS;
    for (k = 0; k < n; k++) {
        float p, w;
        if (!L[k].data || L[k].frames == 0u || L[k].rate == 0u)
            continue;
        w = rb909_layer_weight(tune, L[k].lo, L[k].hi);
        if (w <= 0.0f)
            continue;
        /* pos is 48 kHz-ref layer frames; scale into the layer rate. */
        p = pos * (float)L[k].rate / RI_909_REF_RATE;
        ws[m] = w;
        xs[m] = ri_resample_linear(L[k].data, L[k].frames, p);
        wsum += w;
        m++;
    }
    if (m == 0u || wsum <= 0.0f)
        return 0.0f;
    for (k = 0; k < m; k++)
        acc += sqrt_nr(ws[k] / wsum) * xs[k];
    return acc;
}

void rb909_init_set(struct RB909Set *s) {
    uint32_t i;
    for (i = 0; i < RI_909_NVOICES; i++) {
        s->v[i].id = (uint8_t)i;
        s->v[i].tune = 64;
        s->v[i].accent = 0;
        s->v[i].active = 0;
        s->v[i].pos = 0.0f;
        s->v[i].pos2 = -1.0f;
        s->v[i].flam_delay = (float)RI_909_FLAM_DEFAULT_SMP;
        s->v[i].shelf_lp = 0.0f;
        s->v[i].age = 0;
        s->v[i].layers = 0;
        s->v[i].n_layers = 0;
        s->v[i].flam_capable = (i == RB909_BD || i == RB909_SD) ? 1u : 0u;
        s->v[i].accent_noop = (i == RB909_CR || i == RB909_RD) ? 1u : 0u;
        s->v[i].pad = 0;
    }
}

int rb909_set_layers(struct RB909Set *s, uint32_t voice,
    const struct RISampleLayer *layers, uint32_t n) {
    uint32_t k;
    if (voice >= RI_909_NVOICES)
        return RI_909_BADARG;
    if (s->v[voice].active)
        return RI_909_BUSY; /* idle-only mod swap */
    if (!layers || n == 0u || n > RI_909_MAX_LAYERS)
        return RI_909_BADARG;
    for (k = 0; k < n; k++) {
        if (!layers[k].data || layers[k].frames < 64u || layers[k].rate == 0u)
            return RI_909_BADARG;
        if (layers[k].lo > layers[k].hi)
            return RI_909_BADARG;
    }
    /* Descriptor install (sample data stays with the caller/mod). */
    s->v[voice].layers = layers;
    s->v[voice].n_layers = (uint8_t)n;
    return 0;
}

void rb909_trigger(struct RB909Set *s, uint32_t voice, uint32_t accent,
    uint8_t tune, int32_t flam_delay_smp) {
    struct RB909Voice *v;
    if (voice >= RI_909_NVOICES)
        return;
    v = &s->v[voice];
    /* Shared-hat-ROM steal rule (register entry 14, E0+E2): CH and OH
     * cannot sound together — triggering one kills the other. */
    if (voice == RB909_CH) {
        s->v[RB909_OH].active = 0;
        s->v[RB909_OH].pos2 = -1.0f;
    } else if (voice == RB909_OH) {
        s->v[RB909_CH].active = 0;
        s->v[RB909_CH].pos2 = -1.0f;
    }
    /* Monophonic retrigger: both playheads reset, previous cut. */
    v->tune = tune;
    v->accent = (accent > 2u) ? 2u : (uint8_t)accent;
    v->active = 1;
    v->pos = 0.0f;
    v->pos2 = -1.0f;
    v->shelf_lp = 0.0f;
    v->age = 0;
    if (flam_delay_smp < 0)
        flam_delay_smp = 0;
    if (flam_delay_smp > 65535)
        flam_delay_smp = 65535;
    v->flam_delay = (float)flam_delay_smp;
}

/* Extra crash/ride decay envelope (tau 1.2 s at tune 64, scaled). */
static float decay_env(const struct RB909Voice *v, float pos, float sr) {
    float tau, t;
    if (!v->accent_noop)
        return 1.0f;
    tau = 1.2f * rb909_decay_scale(v->id, v->tune);
    t = pos / sr;
    return ri_exp(-t / tau);
}

float rb909_voice_render(struct RB909Voice *v, float sr) {
    float step, y, y2 = 0.0f, g, a, out, ramp;
    int has2;
    if (!v->active || !v->layers || v->n_layers == 0u || sr <= 0.0f)
        return 0.0f;
    step = rb909_pitch_mult(v->tune);
    y = ri_layer_mix(v->layers, v->n_layers, v->tune, v->pos);
    /* Flam second hit (acc2 on capable voices): fires once when the
     * main playhead crosses flam_delay, at RI_909_FLAM_GAIN. */
    has2 = 0;
    if (v->accent == 2u && v->flam_capable) {
        /* flam_delay arrives in output samples (RI_EV_FLAM domain);
         * the playhead runs at step, so the crossing level scales. */
        float fire = v->flam_delay * step;
        if (v->pos2 >= 0.0f) {
            y2 = ri_layer_mix(v->layers, v->n_layers, v->tune, v->pos2);
            has2 = 1;
        } else if (v->pos >= fire) {
            v->pos2 = v->pos - fire;
            y2 = ri_layer_mix(v->layers, v->n_layers, v->tune, v->pos2);
            has2 = 1;
        }
    }
    g = rb909_accent_gain(v);
    out = y * g * decay_env(v, v->pos, sr);
    if (has2)
        out += y2 * RI_909_FLAM_GAIN * g * decay_env(v, v->pos2, sr);
    /* Accent shelf: small HF lift on accented non-quirk voices. */
    if (v->accent != 0u && !v->accent_noop) {
        a = 1.0f - ri_exp(-RI_909_PI * 2.0f * 6000.0f / sr);
        v->shelf_lp += a * (out - v->shelf_lp);
        out += 0.03f * (out - v->shelf_lp);
    }
    /* Trigger fade-in: click-free idle-swap/start (32 samples). */
    ramp = (v->age < RI_909_TRIG_RAMP) ? (float)(v->age + 1u) / 32.0f : 1.0f;
    out *= ramp;
    /* Advance + end-of-sample -> idle (swap allowed again). */
    v->pos += step;
    if (v->pos2 >= 0.0f)
        v->pos2 += step;
    v->age++;
    {
        uint32_t k, maxf = 0;
        for (k = 0; k < v->n_layers; k++)
            if (v->layers[k].frames > maxf)
                maxf = v->layers[k].frames;
        /* End when the 48 kHz-ref playhead passes the longest layer
         * (rate scaling only shortens real time, never extends it). */
        if (v->pos >= (float)maxf &&
            (v->pos2 < 0.0f || v->pos2 >= (float)maxf)) {
            v->active = 0;
            v->pos2 = -1.0f;
        }
    }
    return ftz(out);
}

void rb909_render_mix(struct RB909Set *s, float *out, uint32_t n, float sr) {
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        float acc = 0.0f;
        for (k = 0; k < RI_909_NVOICES; k++)
            if (s->v[k].active)
                acc += rb909_voice_render(&s->v[k], sr);
        /* Soft-clip only above 1.0 (808 convention). */
        if (acc > 1.0f || acc < -1.0f)
            acc = ri_tanh(acc);
        out[i] = acc;
    }
}
