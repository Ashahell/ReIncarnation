/* levi_fx.c — Levi per-device FX chain DSP (fidelity P7, manual pp.
 * 83-86). Own algorithms (clean-room). P7a: delay only; pre/post and
 * reverb engines land in P7b/c on this framework.
 */
#include "engine/dsp/levi_fx.h"
#include "engine/dsp/kernels.h"

float ri_levi_delay_time(uint8_t ui) {
    /* 1 ms .. 2 s over 7 bits (2000x range). */
    float t = 0.001f * ri_pow2((float)(ui > 127u ? 127u : ui) / 127.0f * 10.9658f);
    if (t < 0.001f)
        t = 0.001f;
    if (t > 2.0f)
        t = 2.0f;
    return t;
}

/* Flush subnormals (no denormals in any feedback loop). */
static float flushf(float x) {
    return (x > -1e-18f && x < 1e-18f) ? 0.0f : x;
}

/* One-pole lowpass step (own); fc clamped inside. */
static float lopass(float *st, float x, float fc, float sr) {
    float a;
    if (fc < 20.0f)
        fc = 20.0f;
    if (fc > 20000.0f)
        fc = 20000.0f;
    if (!(sr > 0.0f))
        return x;
    a = 1.0f - ri_exp(-6.2831853f * fc / sr);
    if (a < 0.0f)
        a = 0.0f;
    if (a > 1.0f)
        a = 1.0f;
    *st += a * (x - *st);
    return *st;
}

/* Gentle saturator for the analog loop (own law). */
static float fx_sat(float x) {
    float a = x < 0.0f ? -x : x;
    return x / (1.0f + a * 0.5f);
}

void levi_fx_clear(struct RILeviFx *f) {
    uint32_t i;
    if (!f)
        return;
    for (i = 0u; i < RI_LEVI_DLY_MAX; i++)
        f->dl[i] = f->dr[i] = 0.0f;
    f->dpos = 0u;
    f->wl = f->wr = f->fl = f->fr = 0.0f;
    f->wphase = 0.0f;
}

void levi_fx_delay(struct RILeviFx *f, float sr, float in_l, float in_r,
    float *out_l, float *out_r) {
    float t, fb, wt, ft, wet;
    uint32_t dl;
    float rl, rr, wl, wr, yL, yR;
    if (!f || !out_l || !out_r) {
        if (out_l)
            *out_l = in_l;
        if (out_r)
            *out_r = in_r;
        return;
    }
    if (f->dbypass) {   /* exact dry */
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    if (!(sr > 0.0f)) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    /* Effective params (matrix offsets ride the lead voice, P7a E0). */
    t = f->dtime;
    fb = f->dfb;
    wt = f->dwtone;
    ft = f->dfbtone;
    wet = f->ddrywet;
    if (f->dfxm_on) {
        t *= ri_pow2(4.0f * f->dfxm[RI_LEVI_DD_TIME]);
        fb += f->dfxm[RI_LEVI_DD_FEEDBACK];
        wt *= ri_pow2(2.0f * f->dfxm[RI_LEVI_DD_WETTONE]);
        ft *= ri_pow2(2.0f * f->dfxm[RI_LEVI_DD_FBTONE]);
        wet += f->dfxm[RI_LEVI_DD_DRYWET];
    }
    if (t < 0.001f)
        t = 0.001f;
    if (t > 2.0f)
        t = 2.0f;
    if (fb < 0.0f)
        fb = 0.0f;
    if (fb > 0.95f)
        fb = 0.95f;    /* bounded: no runaway at any corner */
    if (wet < 0.0f)
        wet = 0.0f;
    if (wet > 1.0f)
        wet = 1.0f;
    dl = (uint32_t)(t * sr);
    if (dl < 1u)
        dl = 1u;
    if (dl > RI_LEVI_DLY_MAX - 1u)
        dl = RI_LEVI_DLY_MAX - 1u;
    if (f->dtype == RI_LEVI_DT_TAPE) {
        /* Wow: slow sine wanders the tap by +/-0.3 % (own). */
        float wob = 1.0f + 0.003f * ri_sin(6.2831853f * f->wphase);
        uint32_t dlw = (uint32_t)((float)dl * wob);
        float fr2;
        uint32_t p0, p1;
        f->wphase += 0.55f / sr;
        if (f->wphase >= 1.0f)
            f->wphase -= 1.0f;
        if (dlw < 1u)
            dlw = 1u;
        if (dlw > RI_LEVI_DLY_MAX - 1u)
            dlw = RI_LEVI_DLY_MAX - 1u;
        p0 = (f->dpos + RI_LEVI_DLY_MAX - dlw) % RI_LEVI_DLY_MAX;
        p1 = (p0 + 1u) % RI_LEVI_DLY_MAX;
        fr2 = (float)dl * wob - (float)dlw;
        rl = f->dl[p0] + (f->dl[p1] - f->dl[p0]) * fr2;
        rr = f->dr[p0] + (f->dr[p1] - f->dr[p0]) * fr2;
    } else if (f->dtype == RI_LEVI_DT_PINGPONG) {
        uint32_t p = (f->dpos + RI_LEVI_DLY_MAX - dl) % RI_LEVI_DLY_MAX;
        rl = f->dl[p];
        rr = f->dr[p];
    } else {
        uint32_t p = (f->dpos + RI_LEVI_DLY_MAX - dl) % RI_LEVI_DLY_MAX;
        rl = f->dl[p];
        rr = f->dr[p];
    }
    rl = flushf(rl);
    rr = flushf(rr);
    /* Loop tone first (shared shape), then per-type loop color. */
    rl = lopass(&f->fl, rl, ft, sr);
    rr = lopass(&f->fr, rr, ft, sr);
    if (f->dtype == RI_LEVI_DT_ANALOG) {
        rl = lopass(&f->fl, fx_sat(rl), 1800.0f, sr);
        rr = lopass(&f->fr, fx_sat(rr), 1800.0f, sr);
        fb *= 0.98f;
    } else if (f->dtype == RI_LEVI_DT_TAPE) {
        rl = lopass(&f->fl, rl, ft * 0.35f > 20.0f ? ft * 0.35f : 20.0f, sr);
        rr = lopass(&f->fr, rr, ft * 0.35f > 20.0f ? ft * 0.35f : 20.0f, sr);
        fb *= 0.99f;
    }
    if (f->dtype == RI_LEVI_DT_PINGPONG) {
        f->dl[f->dpos] = in_l + fb * rr;
        f->dr[f->dpos] = in_r + fb * rl;
    } else {
        f->dl[f->dpos] = in_l + fb * rl;
        f->dr[f->dpos] = in_r + fb * rr;
    }
    f->dpos++;
    if (f->dpos >= RI_LEVI_DLY_MAX)
        f->dpos = 0u;
    /* Wet tone + dry/wet. */
    wl = lopass(&f->wl, rl, wt, sr);
    wr = lopass(&f->wr, rr, wt, sr);
    yL = in_l + (wl - in_l) * wet;
    yR = in_r + (wr - in_r) * wet;
    *out_l = flushf(yL);
    *out_r = flushf(yR);
}
