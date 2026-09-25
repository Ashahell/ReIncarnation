/* pcf.c — PCF 12 dB SVF engine (Task 10, gate G10).
 * The response table is NEVER embedded: this TU refuses to compile unless
 * the ledger has verified rows (the build defines PCF_TABLE_VERIFIED only
 * when reference/pcf-table.bin exists). Deleting the data file fails the
 * pcf build target with the #error below — the G10 negative gate.
 */
#ifndef PCF_TABLE_VERIFIED
#error "PCF table unverified: reference/pcf-table.bin absent, build the ledger rows first (spec §12)"
#endif

#include "engine/fx/pcf.h"
#include "engine/dsp/kernels.h"
#include <stdio.h>

#define RI_PCF_PI 3.14159265f

int pcf_table_load(const char *path, struct PCFTable *t) {
    unsigned char hdr[12];
    FILE *f;
    uint32_t ver, n, i;
    if (!path || !t)
        return -1;
    f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fread(hdr, 1, 12, f) != 12) {
        fclose(f);
        return -1;
    }
    if (hdr[0] != 'P' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'T') {
        fclose(f);
        return -2;
    }
    ver = (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
        ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    if (ver != RI_PCF_TABLE_VERSION) {
        fclose(f);
        return -3;
    }
    n = (uint32_t)hdr[8] | ((uint32_t)hdr[9] << 8) |
        ((uint32_t)hdr[10] << 16) | ((uint32_t)hdr[11] << 24);
    if (n == 0u || n > RI_PCF_TABLE_MAX) {
        fclose(f);
        return -4;
    }
    for (i = 0; i < n; i++) {
        unsigned char rb[12];
        uint32_t ub;
        float bf, ef;
        if (fread(rb, 1, 12, f) != 12) {
            fclose(f);
            return -1;
        }
        if (rb[2] != 0u || rb[3] != 0u) {
            fclose(f);
            return -5;
        }
        ub = (uint32_t)rb[0];
        {
            int8_t sa = (int8_t)rb[1];
            if (ub > 127u || sa < -4 || sa > 4) {
                fclose(f);
                return -5;
            }
            t->rows[i].v = rb[0];
            t->rows[i].amt = sa;
        }
        t->rows[i].pad[0] = 0;
        t->rows[i].pad[1] = 0;
        {
            union {
                uint32_t u;
                float f;
            } cu;
            cu.u = (uint32_t)rb[4] | ((uint32_t)rb[5] << 8) |
                ((uint32_t)rb[6] << 16) | ((uint32_t)rb[7] << 24);
            bf = cu.f;
            cu.u = (uint32_t)rb[8] | ((uint32_t)rb[9] << 8) |
                ((uint32_t)rb[10] << 16) | ((uint32_t)rb[11] << 24);
            ef = cu.f;
        }
        if (!(bf > 0.0f) || !(bf <= 24000.0f) || !(ef > 0.0f) ||
            !(ef <= 24000.0f)) {
            fclose(f);
            return -5;
        }
        t->rows[i].base_fc = bf;
        t->rows[i].expected_fc = ef;
    }
    fclose(f);
    t->n = n;
    return (int)n;
}

float pcf_cutoff_hz(float base_fc, int v, float amt_oct) {
    float vv, aa, e;
    if (!(base_fc > 0.0f))
        return 0.0f;
    vv = (float)v;
    if (vv < 0.0f)
        vv = 0.0f;
    if (vv > 127.0f)
        vv = 127.0f;
    aa = amt_oct;
    if (aa < -RI_PCF_AMT_MAX)
        aa = -RI_PCF_AMT_MAX;
    if (aa > RI_PCF_AMT_MAX)
        aa = RI_PCF_AMT_MAX;
    e = ((vv - 64.0f) / 64.0f) * aa;
    return base_fc * ri_pow2(e);
}

uint8_t pcf_pattern_step(uint8_t pattern, uint32_t step16) {
    (void)pattern;
    (void)step16;
    /* OPEN-04: pattern contents unverified — neutral, never a claim. */
    return (uint8_t)RI_PCF_STEP_NEUTRAL;
}

void pcf_init(struct PCF *p) {
    p->svf.low = 0.0f;
    p->svf.band = 0.0f;
    p->pattern = 0;
    p->mode = 0;
    p->pad[0] = 0;
    p->pad[1] = 0;
    p->base_fc = 1000.0f;
    p->q = 2.0f;
    p->amt_oct = 0.0f;
    p->bpm = 140.0f;
    p->pos_smp = 0ull;
    p->last_step = 0u;
    p->env = 64.0f; /* neutral velocity (matches old v=64 at t=0) */
    p->decay = 0.05f;
}

void pcf_set_tempo(struct PCF *p, float bpm) {
    if (bpm < 20.0f)
        bpm = 20.0f;
    if (bpm > 500.0f)
        bpm = 500.0f;
    p->bpm = bpm;
}

void pcf_set_decay(struct PCF *p, uint8_t v) {
    int vv = v > 127u ? 127 : (int)v;
    p->decay = 0.05f * ri_pow2(((float)vv - 64.0f) / 16.0f);
}

void pcf_restart(struct PCF *p) {
    p->svf.low = 0.0f;
    p->svf.band = 0.0f;
    p->pos_smp = 0ull;
    p->last_step = 0u;
    p->env = 64.0f;
}

uint32_t pcf_step_index(uint64_t pos_smp, float bpm, float sr) {
    double s;
    if (!(bpm > 0.0f) || !(sr > 0.0f))
        return 0u;
    /* 16th index = pos * bpm * 4 / (60 * sr); double holds integer
     * positions exactly to 2^53 samples (3400 years at 48 kHz). */
    s = (double)pos_smp * (double)bpm * 4.0 / (60.0 * (double)sr);
    if (s < 0.0)
        return 0u;
    return (uint32_t)s;
}

/* One Chamberlin SVF sample on the low/band/high taps (12 dB). */
static float pcf_svf_step(struct PCFSVF *s, float x, float f, float damp,
    uint32_t mode) {
    float low, high, band, out;
    low = s->low + f * s->band;
    high = x - low - damp * s->band;
    band = s->band + f * high;
    /* Denormal guard: exact-zero snap inside ±1e-30 (no FTZ flags). */
    if (low > -1e-30f && low < 1e-30f)
        low = 0.0f;
    if (band > -1e-30f && band < 1e-30f)
        band = 0.0f;
    s->low = low;
    s->band = band;
    if (mode == 1u)
        out = band;
    else if (mode == 2u)
        out = high;
    else
        out = low;
    return out;
}

void pcf_render(struct PCF *p, const float *in, float *out, uint32_t n,
    float sr) {
    float q, damp, fc, fmax, f, dec_a;
    uint32_t i, mode;
    if (!p || !in || !out || n == 0u || !(sr > 0.0f))
        return;
    q = p->q;
    if (q < RI_PCF_Q_MIN)
        q = RI_PCF_Q_MIN;
    if (q > RI_PCF_Q_MAX)
        q = RI_PCF_Q_MAX;
    damp = 1.0f / q;
    /* No HP mode in ReBirth (§12.8c1): 2 maps to band. */
    mode = p->mode > 1u ? 1u : (uint32_t)p->mode;
    fmax = sr / 6.0f;
    if (p->decay * sr > 1.0f)
        dec_a = ri_exp(-1.0f / (p->decay * sr));
    else
        dec_a = 0.0f;
    for (i = 0; i < n; i++) {
        /* Integer clock: step is a pure function of the absolute sample
         * (§12.8c1 — the float accumulator stalled near 7 min). A new
         * 16th retriggers the neutral hit (velocity 64, instant attack;
         * per-pattern velocity/attack arrive with the pattern rows). */
        uint32_t step16 = pcf_step_index(p->pos_smp, p->bpm, sr);
        if (step16 != p->last_step) {
            p->last_step = step16;
            p->env = 64.0f;
        }
        p->env *= dec_a;
        fc = p->base_fc * ri_pow2(p->amt_oct * p->env / 64.0f);
        if (fc < RI_PCF_FC_MIN_HZ)
            fc = RI_PCF_FC_MIN_HZ;
        if (fc > fmax)
            fc = fmax;
        /* f = 2*sin(pi*fc/sr): the argument is small (|a| <= pi/6 after
         * the fs/6 clamp), well inside the ri_sin cycle clamp. */
        f = 2.0f * ri_sin(RI_PCF_PI * fc / sr);
        out[i] = pcf_svf_step(&p->svf, in[i], f, damp, mode);
        p->pos_smp++;
    }
}
