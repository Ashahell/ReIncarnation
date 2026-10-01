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

/* Own reverb tap sets (never the core's 1123/1201/1291/1361/401/271):
 * spread-out odd lengths per type. */
static const uint32_t REV_TAPS[RI_LEVI_RT_N][6] = {
    { 307u, 353u, 401u, 449u, 151u, 167u },   /* ROOM */
    { 1491u, 1613u, 1741u, 1867u, 457u, 479u }, /* HALL */
    { 809u, 877u, 947u, 1013u, 277u, 311u },  /* PLATE */
    { 613u, 673u, 733u, 797u, 211u, 233u },   /* CHAMBER */
};

static void reverb_taps(struct RIReverb *r, uint32_t type, float sr) {
    /* Lengths retune live (cheap); positions are NEVER touched here
     * (bind zeroes them once — resetting per sample freezes the lines
     * and the output sits at exact 0). Type changes may click faintly. */
    uint32_t i;
    for (i = 0u; i < 6u; i++) {
        uint64_t len = (uint64_t)REV_TAPS[type < RI_LEVI_RT_N ? type : 0u][i] *
            (uint64_t)(sr > 0.0f ? sr : 48000.0f) / 48000u;
        uint32_t max = RI_LEVI_REV_CAP - 1u;
        r->len[i] = len < 1u ? 1u : len > max ? max : (uint32_t)len;
    }
}

/* Highpass helper via the shared lowpass (own). */
static float hipass(float *st, float x, float fc, float sr) {
    return x - lopass(st, x, fc, sr);
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

/* Bind the two core instances to the static backing (once, at set
 * init — never per sample, or tails would clear). Taps retune per
 * sample from the live type (cheap, pos-preserving). */
void levi_fx_reverb_bind(struct RILeviFx *f) {
    uint32_t i;
    if (!f)
        return;
    ri_reverb_init(&f->rvl, 48000u);
    ri_reverb_init(&f->rvr, 48000u);
    ri_reverb_lines(&f->rvl, f->rlc[0][0], f->rlc[0][1], f->rlc[0][2],
        f->rlc[0][3], f->rla[0][0], f->rla[0][1], RI_LEVI_REV_CAP);
    ri_reverb_lines(&f->rvr, f->rlc[1][0], f->rlc[1][1], f->rlc[1][2],
        f->rlc[1][3], f->rla[1][0], f->rla[1][1], RI_LEVI_REV_CAP);
    for (i = 0u; i < RI_LEVI_PREDLY_MAX; i++)
        f->pdl[0][i] = f->pdl[1][i] = 0.0f;
    f->pdpos = 0u;
    f->twl = f->twr = 0.0f;
    f->hdwl = f->hdwr = 0.0f;
    f->ldl = f->ldr = 0.0f;
    f->frz[0] = f->frz[1] = 0.0f;
}

void levi_fx_reverb(struct RILeviFx *f, float sr, float in_l, float in_r,
    float *out_l, float *out_r) {
    float fb, tone, hid, lod, wet, pdd;
    uint32_t pdl;
    float pl, pr, wl, wr;
    if (!f || !out_l || !out_r) {
        if (out_l)
            *out_l = in_l;
        if (out_r)
            *out_r = in_r;
        return;
    }
    if (f->rbypass) {   /* exact dry */
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    if (!(sr > 0.0f)) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    reverb_taps(&f->rvl, f->rtype, sr);
    reverb_taps(&f->rvr, f->rtype, sr);
    fb = f->rtime;
    tone = f->rtone;
    hid = f->rhidamp;
    lod = f->rlodamp;
    wet = f->rdrywet;
    if (f->rfxm_on) {
        fb += f->rfxm[RI_LEVI_DR_TIME];
        tone *= ri_pow2(2.0f * f->rfxm[RI_LEVI_DR_TONE]);
        hid *= ri_pow2(2.0f * f->rfxm[RI_LEVI_DR_HIDAMP]);
        lod *= ri_pow2(2.0f * f->rfxm[RI_LEVI_DR_LODAMP]);
        wet += f->rfxm[RI_LEVI_DR_DRYWET];
    }
    if (f->rfreeze) {
        /* Frozen drone: static held wet mix over live dry (playable
         * over); core + predelay untouched, so tails resume on release. */
        *out_l = flushf(in_l + (f->frz[0] - in_l) * wet);
        *out_r = flushf(in_r + (f->frz[1] - in_r) * wet);
        return;
    }
    if (fb < 0.0f)
        fb = 0.0f;
    if (fb > 0.95f)
        fb = 0.95f;
    if (wet < 0.0f)
        wet = 0.0f;
    if (wet > 1.0f)
        wet = 1.0f;
    f->rvl.fb = fb;
    f->rvr.fb = fb;
    /* Predelay (keeps shifting under freeze; 0 = dry straight in). */
    pdd = f->rpredly * sr;
    pdl = pdd < 0.0f ? 0u : pdd > (float)(RI_LEVI_PREDLY_MAX - 1u) ?
        RI_LEVI_PREDLY_MAX - 1u : (uint32_t)pdd;
    if (pdl == 0u) {
        pl = in_l;
        pr = in_r;
    } else {
        uint32_t p = (f->pdpos + RI_LEVI_PREDLY_MAX - pdl) % RI_LEVI_PREDLY_MAX;
        pl = f->pdl[0][p];
        pr = f->pdl[1][p];
        f->pdl[0][f->pdpos] = in_l;
        f->pdl[1][f->pdpos] = in_r;
        f->pdpos++;
        if (f->pdpos >= RI_LEVI_PREDLY_MAX)
            f->pdpos = 0u;
    }
    pl = flushf(pl);
    pr = flushf(pr);
    wl = ri_reverb_render(&f->rvl, pl);
    wr = ri_reverb_render(&f->rvr, pr);
    /* Wet EQ (own interpretation of tone + damps): lowpass, second
     * lowpass, and highpass via a shared lowpass helper. */
    wl = lopass(&f->twl, wl, tone, sr);
    wr = lopass(&f->twr, wr, tone, sr);
    wl = lopass(&f->hdwl, wl, hid, sr);
    wr = lopass(&f->hdwr, wr, hid, sr);
    wl = hipass(&f->ldl, wl, lod, sr);
    wr = hipass(&f->ldr, wr, lod, sr);
    f->frz[0] = wl;
    f->frz[1] = wr;
    *out_l = flushf(in_l + (wl - in_l) * wet);
    *out_r = flushf(in_r + (wr - in_r) * wet);
}
