/* fx.c — Delay / Distortion / Compressor + generic RIFX wrapper
 * (Task 10, gate G10). No allocation; no IO; bounded loops; ri_* only.
 */
#include "engine/fx/fx.h"
#include "engine/fx/pcf.h"
#include "engine/dsp/kernels.h"

static const float RI_FX_BEATS[4] = { 0.5f, 0.75f, 1.0f, 1.5f };

int ri_fxdelay_init(struct RiFXDelay *d, float *buf, uint32_t cap) {
    uint32_t i;
    if (!d || !buf || cap < 64u)
        return 2;
    d->buf = buf;
    d->cap = cap;
    d->pos = 0;
    d->delay_smp = cap - 1u;
    d->target_smp = cap - 1u;
    d->beats = 0.75f;
    d->steps = 3u;
    d->triplet = 0u;
    d->fb = 0.0f;
    d->mix = 0.0f;
    for (i = 0; i < cap; i++)
        buf[i] = 0.0f;
    return 0;
}

/* Steps model (§12.8b1): steps straight 16ths (1/4 beat each) or 8th
 * triplets (1/3 beat each). All knob paths funnel through these. */
static float delay_beats(const struct RiFXDelay *d) {
    float per = d->triplet ? (1.0f / 3.0f) : 0.25f;
    return (float)d->steps * per;
}

static uint8_t delay_steps_for(float beats) {
    /* Snap to the 16th grid (straight): the direct-sync entry point for
     * arbitrary floats; all in-tree callers pass table values (exact). */
    int s = (int)(beats * 4.0f + 0.5f);
    if (s < 1)
        s = 1;
    if (s > 32)
        s = 32;
    return (uint8_t)s;
}

/* Resolve beats/bpm/sr to whole samples (shared by sync/retarget). */
static uint32_t delay_samples(float bpm, float beats, float sr, uint32_t cap) {
    float want;
    uint32_t s;
    if (!(sr > 0.0f) || cap < 2u)
        return 1u;
    if (bpm < 20.0f)
        bpm = 20.0f;
    if (bpm > 500.0f)
        bpm = 500.0f;
    if (beats < 0.0625f)
        beats = 0.0625f;
    if (beats > 32.0f)
        beats = 32.0f;
    want = beats * 60.0f * sr / bpm;
    /* Round-half-away without libm (want >= 0 by construction). */
    s = (uint32_t)(want + 0.5f);
    if (s < 1u)
        s = 1u;
    if (s > cap - 1u)
        s = cap - 1u;
    return s;
}

uint32_t ri_fxdelay_sync(struct RiFXDelay *d, float bpm, float beats,
    float sr) {
    uint32_t s;
    if (!d)
        return 0;
    if (beats < 0.0625f)
        beats = 0.0625f;
    if (beats > 32.0f)
        beats = 32.0f;
    d->beats = beats;
    d->steps = delay_steps_for(beats);
    d->triplet = 0u;
    d->beats = delay_beats(d); /* field mirrors behavior (snapped grid) */
    s = delay_samples(bpm, d->beats, sr, d->cap);
    d->delay_smp = s;
    d->target_smp = s;
    return s;
}

/* Retarget without jumping: the render slews delay_smp to the resolved
 * target (no zipper clicks on tempo/beat changes). Returns the target. */
uint32_t ri_fxdelay_retarget(struct RiFXDelay *d, float bpm, float beats,
    float sr) {
    uint32_t s;
    if (!d)
        return 0;
    if (beats < 0.0625f)
        beats = 0.0625f;
    if (beats > 32.0f)
        beats = 32.0f;
    d->beats = beats;
    s = delay_samples(bpm, beats, sr, d->cap);
    d->target_smp = s;
    return s;
}

void ri_fxdelay_set(struct RiFXDelay *d, uint8_t fb128, uint8_t mix128) {
    if (!d)
        return;
    /* Feedback 0..1.0 (knob/127; 127 = infinite sustain — §12.8b1 parity,
     * was capped 0.8). Exact 1.0 recirculates bit-identically. */
    d->fb = (float)(fb128 > 127u ? 127u : fb128) / 127.0f;
    d->mix = (float)(mix128 > 127u ? 127u : mix128) / 127.0f;
}

void ri_fxdelay_reset(struct RiFXDelay *d) {
    uint32_t i;
    if (!d)
        return;
    for (i = 0; i < d->cap; i++)
        d->buf[i] = 0.0f;
    d->pos = 0;
}

void ri_fxdelay_render(struct RiFXDelay *d, const float *in, float *out,
    uint32_t n) {
    uint32_t i;
    if (!d || !in || !out)
        return;
    for (i = 0; i < n; i++) {
        /* Slew the live tap toward a retarget (at most 1/64th of the
         * remaining distance per sample: full traverse in <= 64 samples
         * ≈ 1.3 ms — no zipper click, fast lock. Chunk-agnostic: the
         * trajectory depends only on sample count, not chunking). */
        if (d->delay_smp != d->target_smp) {
            uint32_t diff = d->delay_smp > d->target_smp ?
                d->delay_smp - d->target_smp : d->target_smp - d->delay_smp;
            uint32_t step = diff / 64u + 1u;
            if (d->delay_smp > d->target_smp) {
                d->delay_smp -= step;
                if (d->delay_smp < d->target_smp)
                    d->delay_smp = d->target_smp;
            } else {
                d->delay_smp += step;
                if (d->delay_smp > d->target_smp)
                    d->delay_smp = d->target_smp;
            }
        }
        {
            uint32_t rp = (d->pos + d->cap - d->delay_smp) % d->cap;
            float dl = d->buf[rp];
            d->buf[d->pos] = in[i] + dl * d->fb;
            out[i] = in[i] * (1.0f - d->mix) + dl * d->mix;
        }
        d->pos++;
        if (d->pos >= d->cap)
            d->pos = 0;
    }
}

void ri_fxdist_init(struct RiFXDist *d) {
    if (!d)
        return;
    d->drive = 0;
    d->shape = 0;
}

void ri_fxdist_set(struct RiFXDist *d, uint8_t drive, uint8_t shape) {
    if (!d)
        return;
    d->drive = drive > 127u ? 127u : drive;
    d->shape = shape > 127u ? 127u : shape;
}

void ri_fxdist_render(struct RiFXDist *d, const float *in, float *out,
    uint32_t n) {
    uint32_t i;
    float k, a, norm_in, norm;
    if (!d || !in || !out)
        return;
    if (d->drive == 0u && d->shape == 0u) {
        /* Unity bypass by construction (drive 0 = linear path). */
        for (i = 0; i < n; i++)
            out[i] = in[i];
        return;
    }
    k = (float)d->drive / 127.0f * 8.0f;
    a = (float)d->shape / 127.0f * 0.5f;
    /* Normalize so a full-scale DC 1.0 maps to 1.0 (no level jump when
     * engaging drive; the curve, not the level, is the effect). */
    norm_in = (1.0f + k) + a * (1.0f + k) * (1.0f + k);
    norm = ri_tanh(norm_in);
    if (!(norm > 1e-6f))
        norm = 1.0f;
    for (i = 0; i < n; i++) {
        float x1 = in[i] * (1.0f + k);
        float y = ri_tanh(x1 + a * x1 * x1);
        out[i] = y / norm;
    }
}

/* thresh knob 0..127 -> -40..0 dB. */
static float ri_fxcomp_thresh_db(uint8_t t) {
    return -40.0f + (float)(t > 127u ? 127u : t) * (40.0f / 127.0f);
}

/* One-pole smoothers from the kernel (no libm): a = exp(-1/(tau*sr)).
 * Re-derived per render call (cheap: 2 exps) so the follower tracks the
 * live rate instead of the init-time 48 kHz. */
static void ri_fxcomp_derive(struct RiFXComp *c, float sr) {
    if (!(sr > 0.0f))
        sr = 48000.0f;
    c->atk_a = ri_exp(-1.0f / (RI_FXCOMP_ATTACK_S * sr));
    c->rel_a = ri_exp(-1.0f / (RI_FXCOMP_RELEASE_S * sr));
}

int ri_fxcomp_init(struct RiFXComp *c, float sr) {
    if (!c || !(sr > 0.0f))
        return 2;
    c->env = 0.0f;
    ri_fxcomp_derive(c, sr);
    c->thresh = 64;
    ri_fxcomp_set(c, (uint8_t)64);
    return 0;
}

void ri_fxcomp_set(struct RiFXComp *c, uint8_t thresh) {
    float tdb, mu_db;
    if (!c)
        return;
    if (thresh > 127u)
        thresh = 127u;
    c->thresh = thresh;
    tdb = ri_fxcomp_thresh_db(thresh);
    /* Linear threshold via the kernel: 10^(tdb/20) = 2^(tdb*log2(10)/20). */
    c->thresh_lin = ri_pow2(tdb * 0.1660964f);
    /* Auto make-up: MU_dB = -tdb*(1-1/R), premultiplied once here. */
    mu_db = -tdb * (1.0f - 1.0f / RI_FXCOMP_RATIO);
    c->mu_lin = ri_pow2(mu_db * 0.1660964f);
}

void ri_fxcomp_reset(struct RiFXComp *c) {
    if (!c)
        return;
    c->env = 0.0f;
}

void ri_fxcomp_render(struct RiFXComp *c, const float *in, float *out,
    uint32_t n) {
    uint32_t i;
    if (!c || !in || !out)
        return;
    for (i = 0; i < n; i++) {
        float ax = in[i] < 0.0f ? -in[i] : in[i];
        float a = ax > c->env ? c->atk_a : c->rel_a;
        float g;
        c->env = a * c->env + (1.0f - a) * ax;
        if (c->env > c->thresh_lin && c->env > 1e-9f) {
            float revised = c->thresh_lin +
                (c->env - c->thresh_lin) / RI_FXCOMP_RATIO;
            g = revised / c->env;
        } else {
            g = 1.0f;
        }
        out[i] = in[i] * g * c->mu_lin;
        if (!(out[i] == out[i]))
            out[i] = 0.0f; /* fail-closed: never emit NaN */
    }
}

/* ---- generic wrapper (Appendix D names) ---- */

#define RI_FX_POOL 8u
static struct RIFX RI_FX_INST[RI_FX_POOL];
static uint8_t RI_FX_BUSY[RI_FX_POOL];

static struct RIFX *fx_alloc(void) {
    uint32_t i;
    for (i = 0; i < RI_FX_POOL; i++)
        if (!RI_FX_BUSY[i]) {
            RI_FX_BUSY[i] = 1u;
            return &RI_FX_INST[i];
        }
    return 0;
}

static void fx_release(struct RIFX *x) {
    uint32_t i;
    if (!x)
        return;
    for (i = 0; i < RI_FX_POOL; i++)
        if (x == &RI_FX_INST[i])
            RI_FX_BUSY[i] = 0u;
}

/* Map wrapper PCF knob echoes onto the owned voice fields. Touches
 * parameters only — SVF state (low/band) and beat_pos are never reset
 * here, so block streaming stays continuous. */
static void ri_fx_pcf_apply(struct RIFX *x) {
    x->pcf.base_fc =
        100.0f * ri_pow2(((float)x->pcf_base / 127.0f) * 6.321928f);
    x->pcf.q = RI_PCF_Q_MIN +
        ((float)x->pcf_q / 127.0f) * (RI_PCF_Q_MAX - RI_PCF_Q_MIN);
    x->pcf.amt_oct = ((float)x->pcf_amt / 127.0f) * 8.0f - 4.0f;
    x->pcf.mode = x->pcf_mode;
    x->pcf.pattern = x->pcf_pattern;
}

struct RIFX *RiFXCreate(uint32_t fx_type) {
    struct RIFX *x;
    if (fx_type > RI_FX_PCF)
        return 0;
    if (fx_type == RI_FX_DELAY)
        return 0; /* no caller-owned line: use RiFXCreateDelay (§12.8a) */
    x = fx_alloc();
    if (!x)
        return 0;
    x->type = fx_type;
    x->busy = 1;
    x->pad[0] = 0;
    x->pad[1] = 0;
    x->pad[2] = 0;
    ri_fxdist_init(&x->dist);
    ri_fxcomp_init(&x->comp, 48000.0f);
    x->pcf_pattern = 0;
    x->pcf_mode = 0;
    x->pcf_base = 64;
    x->pcf_q = 64;
    x->pcf_amt = 64;
    x->pad2[0] = 0;
    x->pad2[1] = 0;
    x->pad2[2] = 0;
    x->delay.buf = 0;
    x->delay.cap = 0;
    x->delay.pos = 0;
    x->delay.delay_smp = 0;
    x->delay.target_smp = 0;
    x->delay.beats = 0.75f;
    x->delay.fb = 0.0f;
    x->delay.mix = 0.0f;
    /* Owned PCF voice: init once here, reused across renders (never
     * re-inited on the render path — block clicks otherwise). */
    pcf_init(&x->pcf);
    ri_fx_pcf_apply(x);
    pcf_set_tempo(&x->pcf, 140.0f);
    return x;
}

/* Delay with a caller-owned line (the ONLY delay constructor now).
 * The caller sizes cap for the worst case and keeps the buffer alive
 * for the handle's lifetime. */
struct RIFX *RiFXCreateDelay(float *buf, uint32_t cap) {
    struct RIFX *x;
    if (!buf || cap < 64u)
        return 0;
    x = fx_alloc();
    if (!x)
        return 0;
    x->type = RI_FX_DELAY;
    x->busy = 1;
    x->pad[0] = 0;
    x->pad[1] = 0;
    x->pad[2] = 0;
    ri_fxdist_init(&x->dist);
    ri_fxcomp_init(&x->comp, 48000.0f);
    x->pcf_pattern = 0;
    x->pcf_mode = 0;
    x->pcf_base = 64;
    x->pcf_q = 64;
    x->pcf_amt = 64;
    x->pad2[0] = 0;
    x->pad2[1] = 0;
    x->pad2[2] = 0;
    ri_fxdelay_init(&x->delay, buf, cap);
    pcf_init(&x->pcf);
    ri_fx_pcf_apply(x);
    pcf_set_tempo(&x->pcf, 140.0f);
    return x;
}

void RiFXDestroy(struct RIFX *x) {
    fx_release(x); /* NULL + double-destroy safe (no match = no-op) */
}

/* Render-ready check: 0 ok, 2 = DELAY handle without a line buffer
 * (unreachable via Create, which fails closed; defensive only). */
int RiFXValid(const struct RIFX *x) {
    if (!x)
        return 2;
    if (x->type == RI_FX_DELAY && !x->delay.buf)
        return 2;
    return 0;
}

void RiFXSetParam(struct RIFX *x, uint32_t id, uint8_t value) {
    uint32_t b;
    if (!x)
        return;
    if (value > 127u)
        value = 127u;
    switch (id) {
    case RI_FXID_DELAY_BEATS:
        /* Store the musical length; resolved against the live tempo/rate
         * at render (RiFXRender retargets — no zipper jumps). */
        b = value > 3u ? 3u : (uint32_t)value;
        x->delay.beats = RI_FX_BEATS[b];
        x->delay.steps = delay_steps_for(RI_FX_BEATS[b]);
        x->delay.triplet = 0u;
        break;
    case RI_FXID_DELAY_STEPS:
        /* 1..32 steps; triplet flag untouched (independent knob). */
        x->delay.steps = value < 1u ? 1u : (value > 32u ? 32u : value);
        x->delay.beats = delay_beats(&x->delay);
        break;
    case RI_FXID_DELAY_TRIPLET:
        x->delay.triplet = value ? 1u : 0u;
        x->delay.beats = delay_beats(&x->delay);
        break;
    case RI_FXID_DELAY_FB:
        if (x->type == RI_FX_DELAY && x->delay.buf)
            ri_fxdelay_set(&x->delay, value,
                (uint8_t)(x->delay.mix * 127.0f));
        break;
    case RI_FXID_DELAY_MIX:
        if (x->type == RI_FX_DELAY && x->delay.buf)
            ri_fxdelay_set(&x->delay,
                (uint8_t)(x->delay.fb * (127.0f / 0.8f)), value);
        break;
    case RI_FXID_DIST_DRIVE:
        x->dist.drive = value;
        break;
    case RI_FXID_DIST_SHAPE:
        x->dist.shape = value;
        break;
    case RI_FXID_COMP_THRESH:
        ri_fxcomp_set(&x->comp, value);
        break;
    case RI_FXID_PCF_BASE:
        x->pcf_base = value;
        break;
    case RI_FXID_PCF_Q:
        x->pcf_q = value;
        break;
    case RI_FXID_PCF_AMT:
        x->pcf_amt = value;
        break;
    case RI_FXID_PCF_MODE:
        x->pcf_mode = value > 2u ? 2u : value;
        break;
    case RI_FXID_PCF_PATTERN:
        x->pcf_pattern = value > 53u ? 53u : value;
        break;
    default:
        break;
    }
}

void RiFXRender(struct RIFX *x, float *in, float *out, uint32_t frames,
    float sr, float bpm) {
    uint32_t i;
    if (!x || !in || !out)
        return;
    if (!(sr > 0.0f))
        sr = 48000.0f;
    if (x->type == RI_FX_DELAY) {
        if (!x->delay.buf) {
            /* Fail-closed: bufferless delay renders silence, never a
             * different effect (CreateDelay always installs a line, so
             * this is defensive; see RiFXValid). */
            for (i = 0; i < frames; i++)
                out[i] = 0.0f;
            return;
        }
        if (bpm >= 20.0f && bpm <= 500.0f) {
            /* Re-resolve the STORED musical delay against the live tempo
             * (retarget slews — the old code overwrote beats with 0.75). */
            ri_fxdelay_retarget(&x->delay, bpm, delay_beats(&x->delay), sr);
        }
        ri_fxdelay_render(&x->delay, in, out, frames);
        return;
    }
    if (x->type == RI_FX_DIST) {
        ri_fxdist_render(&x->dist, in, out, frames);
        return;
    }
    if (x->type == RI_FX_COMP) {
        /* Follower coeffs track the live rate (not the init-time 48 k). */
        ri_fxcomp_derive(&x->comp, sr);
        ri_fxcomp_render(&x->comp, in, out, frames);
        return;
    }
    if (x->type != RI_FX_PCF) {
        for (i = 0; i < frames; i++)
            out[i] = 0.0f;
        return;
    }
    /* PCF via the OWNED voice: parameters re-applied, state preserved. */
    ri_fx_pcf_apply(x);
    pcf_set_tempo(&x->pcf, bpm >= 30.0f && bpm <= 300.0f ? bpm : 140.0f);
    pcf_render(&x->pcf, in, out, frames, sr);
}

void RiFXReset(struct RIFX *x) {
    if (!x)
        return;
    if (x->type == RI_FX_DELAY && x->delay.buf)
        ri_fxdelay_reset(&x->delay);
    if (x->type == RI_FX_COMP)
        ri_fxcomp_reset(&x->comp);
    if (x->type == RI_FX_PCF) {
        pcf_init(&x->pcf);
        ri_fx_pcf_apply(x);
    }
}
