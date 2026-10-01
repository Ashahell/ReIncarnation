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

float ri_levi_delay_snap(float sec, float bpm) {
    /* Nearest straight 16th (P7d via the P8a clock). Idempotent:
     * snapped values re-snap to themselves. */
    float b = bpm, beats, q;
    if (!(b >= 20.0f && b <= 500.0f))
        b = 140.0f;
    if (!(sec > 0.0f))
        return 0.001f;
    beats = sec * b / 60.0f;
    q = (float)(int)(beats * 4.0f + 0.5f) / 4.0f;
    sec = q * 60.0f / b;
    if (sec < 0.001f)
        sec = 0.001f;
    if (sec > 2.0f)
        sec = 2.0f;
    return sec;
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
    for (i = 0u; i < RI_LEVI_MOD_MAX; i++)
        f->pre.mdl[i] = f->pre.mdr[i] = f->post.mdl[i] = f->post.mdr[i] = 0.0f;
    f->pre.mpos = f->post.mpos = 0u;
    f->pre.lfo = f->post.lfo = 0.0f;
    for (i = 0u; i < 4u; i++)
        f->pre.apd[0][i] = f->pre.apd[1][i] = f->post.apd[0][i] = f->post.apd[1][i] = 0.0f;
    f->pre.eql[0] = f->pre.eql[1] = f->pre.eqr[0] = f->pre.eqr[1] = 0.0f;
    f->post.eql[0] = f->post.eql[1] = f->post.eqr[0] = f->post.eqr[1] = 0.0f;
    f->pre.envl = f->pre.envr = f->pre.cgl = f->pre.cgr = 0.0f;
    f->post.envl = f->post.envr = f->post.cgl = f->post.cgr = 0.0f;
    f->pre.cgl = f->pre.cgr = f->post.cgl = f->post.cgr = 1.0f;
    f->pre.dtl = f->pre.dtr = f->post.dtl = f->post.dtr = 0.0f;
    f->pre.lh[0] = f->pre.lh[1] = f->post.lh[0] = f->post.lh[1] = 0.0f;
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

/* Own factory preset tuples (p1, p2, drywet as UI 0..127). */
static const uint8_t MOD_PRESETS[RI_LEVI_MT_N][RI_LEVI_MT_PRESETS][3] = {
    { { 16, 64, 48 }, { 32, 80, 64 }, { 8, 110, 80 }, { 64, 127, 96 } },     /* CHORUS */
    { { 24, 48, 48 }, { 48, 80, 64 }, { 12, 110, 80 }, { 96, 127, 96 } },    /* FLANGER */
    { { 32, 64, 64 }, { 64, 64, 80 }, { 16, 96, 64 }, { 110, 32, 96 } },     /* ROTARY */
    { { 16, 48, 48 }, { 32, 72, 64 }, { 8, 100, 80 }, { 90, 127, 96 } },     /* PHASER */
    { { 48, 24, 64 }, { 80, 64, 80 }, { 100, 100, 96 }, { 127, 127, 110 } }, /* LOFI */
    { { 32, 80, 80 }, { 64, 100, 96 }, { 16, 48, 64 }, { 100, 127, 110 } },  /* TREMOLO */
    { { 80, 64, 80 }, { 64, 90, 80 }, { 96, 96, 96 }, { 48, 80, 64 } },       /* EQ */
    { { 64, 48, 80 }, { 32, 80, 96 }, { 96, 32, 64 }, { 16, 110, 110 } },    /* COMP */
    { { 32, 80, 80 }, { 64, 90, 96 }, { 96, 64, 110 }, { 127, 100, 120 } },   /* DISTORT */
};

int ri_levi_mod_preset(uint32_t type, uint32_t preset, uint8_t *p1,
    uint8_t *p2, uint8_t *dw) {
    if (type >= RI_LEVI_MT_N || preset >= RI_LEVI_MT_PRESETS)
        return 2;
    if (!p1 || !p2 || !dw)
        return 2;
    *p1 = MOD_PRESETS[type][preset][0];
    *p2 = MOD_PRESETS[type][preset][1];
    *dw = MOD_PRESETS[type][preset][2];
    return 0;
}

/* Fractional modulated-delay read (linear interp, own). */
static float mod_dl_read(const float *line, uint32_t pos, float dsmp) {
    float p = (float)pos - dsmp;
    uint32_t i0, i1;
    float fr;
    while (p < 0.0f)
        p += (float)RI_LEVI_MOD_MAX;
    i0 = (uint32_t)p;
    fr = p - (float)i0;
    i1 = i0 + 1u >= RI_LEVI_MOD_MAX ? 0u : i0 + 1u;
    return line[i0] + (line[i1] - line[i0]) * fr;
}

/* One allpass stage (own): y = d + a*x; d' = x - a*y. */
static float allpass(float *d, float x, float a) {
    float y = *d + a * x;
    *d = flushf(x - a * y);
    return y;
}

void levi_fx_mod(struct RILeviMod *m, float sr, float in_l, float in_r,
    float *out_l, float *out_r) {
    uint32_t type;
    float p1, p2, wet, wl, wr;
    if (!m || !out_l || !out_r) {
        if (out_l)
            *out_l = in_l;
        if (out_r)
            *out_r = in_r;
        return;
    }
    if (m->bypass) {   /* exact dry */
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    if (!(sr > 0.0f)) {
        *out_l = in_l;
        *out_r = in_r;
        return;
    }
    type = m->type >= RI_LEVI_MT_N ? RI_LEVI_MT_N - 1u : m->type;
    p1 = m->p1;
    p2 = m->p2;
    wet = m->drywet;
    if (m->mxm_on) {
        p1 += m->mxm[RI_LEVI_DX_P1] * 0.5f;
        p2 += m->mxm[RI_LEVI_DX_P2] * 0.5f;
        wet += m->mxm[RI_LEVI_DX_DRYWET];
    }
    if (p1 < 0.0f)
        p1 = 0.0f;
    if (p1 > 1.0f)
        p1 = 1.0f;
    if (p2 < 0.0f)
        p2 = 0.0f;
    if (p2 > 1.0f)
        p2 = 1.0f;
    if (wet < 0.0f)
        wet = 0.0f;
    if (wet > 1.0f)
        wet = 1.0f;
    wl = in_l;
    wr = in_r;
    switch (type) {
    case RI_LEVI_MT_CHORUS: {
        /* Modulated 20 ms delay, ±8 ms sweep, R anti-phase (own). */
        float rate = 0.05f + p1 * 7.95f, dep = p2;
        float base = 0.020f * sr, swp = 0.008f * sr * dep;
        float s1 = ri_sin(m->lfo * 6.2831853f);
        float dl = base + swp * s1, dr = base - swp * s1;
        float rl, rr;
        if (dl > (float)(RI_LEVI_MOD_MAX - 2u))
            dl = (float)(RI_LEVI_MOD_MAX - 2u);
        if (dr > (float)(RI_LEVI_MOD_MAX - 2u))
            dr = (float)(RI_LEVI_MOD_MAX - 2u);
        if (dl < 1.0f)
            dl = 1.0f;
        if (dr < 1.0f)
            dr = 1.0f;
        rl = flushf(mod_dl_read(m->mdl, m->mpos, dl));
        rr = flushf(mod_dl_read(m->mdr, m->mpos, dr));
        m->mdl[m->mpos] = in_l;
        m->mdr[m->mpos] = in_r;
        m->mpos++;
        if (m->mpos >= RI_LEVI_MOD_MAX)
            m->mpos = 0u;
        m->lfo += rate / sr;
        if (m->lfo >= 1.0f)
            m->lfo -= 1.0f;
        wl = rl;
        wr = rr;
        break;
    }
    case RI_LEVI_MT_FLANGER: {
        /* Short 3 ms line with 0.55 loop (own); R anti-phase. */
        float rate = 0.05f + p1 * 7.95f, dep = p2;
        float base = 0.003f * sr, swp = 0.0025f * sr * dep;
        float s1 = ri_sin(m->lfo * 6.2831853f);
        float dl = base + swp * s1, dr = base - swp * s1;
        float rl, rr;
        if (dl > (float)(RI_LEVI_MOD_MAX - 2u))
            dl = (float)(RI_LEVI_MOD_MAX - 2u);
        if (dr > (float)(RI_LEVI_MOD_MAX - 2u))
            dr = (float)(RI_LEVI_MOD_MAX - 2u);
        if (dl < 1.0f)
            dl = 1.0f;
        if (dr < 1.0f)
            dr = 1.0f;
        rl = flushf(mod_dl_read(m->mdl, m->mpos, dl));
        rr = flushf(mod_dl_read(m->mdr, m->mpos, dr));
        m->mdl[m->mpos] = flushf(in_l + 0.55f * rl);
        m->mdr[m->mpos] = flushf(in_r + 0.55f * rr);
        m->mpos++;
        if (m->mpos >= RI_LEVI_MOD_MAX)
            m->mpos = 0u;
        m->lfo += rate / sr;
        if (m->lfo >= 1.0f)
            m->lfo -= 1.0f;
        wl = rl;
        wr = rr;
        break;
    }
    case RI_LEVI_MT_ROTARY: {
        /* Dual-rate tremolo with cross-pan (own rotary impression). */
        float rate = 0.5f + p1 * 7.5f, bal = p2;
        float ph = m->lfo * 6.2831853f;
        float fast = 0.5f + 0.5f * ri_sin(ph);
        float slow = 0.5f + 0.5f * ri_sin(ph * 0.5f + 1.0f);
        float pan = 0.5f + 0.5f * ri_sin(ph + 1.5707963f);
        float tl = bal * fast + (1.0f - bal) * slow;
        float tr = bal * slow + (1.0f - bal) * fast;
        m->lfo += rate / sr;
        if (m->lfo >= 1.0f)
            m->lfo -= 1.0f;
        wl = in_l * (1.0f - 0.7f * tl) * (0.7f + 0.6f * pan);
        wr = in_r * (1.0f - 0.7f * tr) * (1.3f - 0.6f * pan);
        break;
    }
    case RI_LEVI_MT_PHASER: {
        /* 4 allpass stages, swept coefficient (own). */
        float rate = 0.05f + p1 * 3.95f, dep = p2;
        float a = 0.3f + 0.6f * dep * (0.5f + 0.5f * ri_sin(m->lfo * 6.2831853f));
        uint32_t s;
        float xl = in_l, xr = in_r;
        m->lfo += rate / sr;
        if (m->lfo >= 1.0f)
            m->lfo -= 1.0f;
        for (s = 0u; s < 4u; s++) {
            xl = allpass(&m->apd[0][s], xl, a);
            xr = allpass(&m->apd[1][s], xr, a);
        }
        wl = flushf(xl);
        wr = flushf(xr);
        break;
    }
    case RI_LEVI_MT_LOFI: {
        /* Bitcrush + decim hold; lfo doubles as the decim counter. */
        uint32_t bits = 16u - (uint32_t)(p1 * 12.99f);
        uint32_t dec = 1u + (uint32_t)(p2 * 15.99f);
        float step, ql, qr;
        if (bits > 16u)
            bits = 16u;
        if (bits < 4u)
            bits = 4u;
        m->lfo += 1.0f;
        if (m->lfo >= (float)dec) {
            m->lfo = 0.0f;
            step = 8.0f / (float)(1u << (bits - 1u)) / 2.0f;
            ql = (float)(int)(in_l / step + (in_l < 0.0f ? -0.5f : 0.5f)) * step;
            qr = (float)(int)(in_r / step + (in_r < 0.0f ? -0.5f : 0.5f)) * step;
            m->lh[0] = ql;
            m->lh[1] = qr;
        }
        wl = m->lh[0];
        wr = m->lh[1];
        break;
    }
    case RI_LEVI_MT_TREMOLO: {
        /* Sine AM (own). */
        float rate = 0.5f + p1 * 14.5f, dep = p2;
        float g = 1.0f - dep * 0.5f + dep * 0.5f * ri_sin(m->lfo * 6.2831853f);
        m->lfo += rate / sr;
        if (m->lfo >= 1.0f)
            m->lfo -= 1.0f;
        wl = in_l * g;
        wr = in_r * g;
        break;
    }
    case RI_LEVI_MT_EQ: {
        /* One-pole bass/treble shelves ±12 dB (own). */
        float bl = ri_pow2((p1 * 2.0f - 1.0f) * 12.0f / 6.0f);
        float tr = ri_pow2((p2 * 2.0f - 1.0f) * 12.0f / 6.0f);
        float lo_l = lopass(&m->eql[0], in_l, 350.0f, sr);
        float lo_r = lopass(&m->eqr[0], in_r, 350.0f, sr);
        float hi_l = in_l - lopass(&m->eql[1], in_l, 3000.0f, sr);
        float hi_r = in_r - lopass(&m->eqr[1], in_r, 3000.0f, sr);
        wl = fx_sat(in_l + (bl - 1.0f) * lo_l + (tr - 1.0f) * hi_l);
        wr = fx_sat(in_r + (bl - 1.0f) * lo_r + (tr - 1.0f) * hi_r);
        break;
    }
    case RI_LEVI_MT_COMP: {
        /* Peak-follower compressor with set-time makeup (own). */
        float thr = ri_pow2((-40.0f + p1 * 40.0f) * 0.1660964f);
        float ratio = 1.0f + p2 * 19.0f;
        float expo = (ratio - 1.0f) / ratio;
        float mu = ri_pow2((-40.0f + p1 * 40.0f) * -expo * 0.1660964f);
        float at = 1.0f - ri_pow2(-1.0f / (0.010f * sr));
        float rl = 1.0f - ri_pow2(-1.0f / (0.100f * sr));
        float al = in_l < 0.0f ? -in_l : in_l;
        float ar = in_r < 0.0f ? -in_r : in_r;
        float gl, gr, tgt;
        m->envl += al > m->envl ? at * (al - m->envl) : rl * (al - m->envl);
        m->envr += ar > m->envr ? at * (ar - m->envr) : rl * (ar - m->envr);
        m->envl = flushf(m->envl);
        m->envr = flushf(m->envr);
        /* gain target: 1/(1+(env/thr-1)*expo) above threshold, unity
         * below (smooth curve, exact unity at threshold). */
        if (m->envl > thr && m->envl > 0.0f)
            tgt = 1.0f / (1.0f + (m->envl / thr - 1.0f) * expo);
        else
            tgt = 1.0f;
        gl = tgt;
        if (m->envr > thr && m->envr > 0.0f)
            tgt = 1.0f / (1.0f + (m->envr / thr - 1.0f) * expo);
        else
            tgt = 1.0f;
        gr = tgt;
        m->cgl += (gl > m->cgl ? at : rl) * (gl - m->cgl);
        m->cgr += (gr > m->cgr ? at : rl) * (gr - m->cgr);
        wl = fx_sat(in_l * flushf(m->cgl) * mu);
        wr = fx_sat(in_r * flushf(m->cgr) * mu);
        break;
    }
    default: { /* RI_LEVI_MT_DISTORT */
        /* Asymmetric soft-clip drive + tone (own). */
        float k = 1.0f + p1 * 24.0f;
        float tone = 500.0f + p2 * 17000.0f;
        float dl = in_l * k, dr = in_r * k;
        float yl = dl >= 0.0f ? fx_sat(dl) : fx_sat(dl * 0.8f) * 1.1f;
        float yr = dr >= 0.0f ? fx_sat(dr) : fx_sat(dr * 0.8f) * 1.1f;
        wl = lopass(&m->dtl, yl, tone, sr);
        wr = lopass(&m->dtr, yr, tone, sr);
        break;
    }
    }
    *out_l = flushf(in_l + (wl - in_l) * wet);
    *out_r = flushf(in_r + (wr - in_r) * wet);
}
