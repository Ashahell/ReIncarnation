/* t50_fx_dist_comp.c — §12.8: dist 2x oversample + comp ratio/GR meter.
 * Dist (2x OS: linear-mid evaluate, box decimate, prev-sample state):
 *   (a) bypass exact (drive 0 + shape 0 copies input bit-exactly);
 *   (b) DC law preserved: full-scale DC 1.0 -> 1.0 (<=1e-6, 3x3 grid);
 *   (c) block-split identical (4096 vs 2x2048 bit-exact: state streams);
 *   (d) engaged path differs from the naive single-rate reference
 *       (proves the oversample path is live, not bypassed);
 *   (e) alias reduction: 5 kHz sine at full drive, Goertzel at 13 kHz
 *       (fold of the 35 kHz harmonic) reads lower than naive.
 * Comp (ratio knob 0..127 -> 1..20, GR meter = block min gain in dB):
 *   (f) ratio 1:1 passes a hot steady DC unchanged (within 1%);
 *   (g) max ratio compresses more than the default 4:1 (same signal);
 *   (h) GR meter: quiet -> 0 dB; hot default -> negative; ratio 1:1
 *       hot -> 0 dB; reset restores 0 dB.
 * Wrapper: RI_FXID_COMP_RATIO sets/clamps; pattern 54 now stored
 * (m59 left the wrapper clamping at 53).
 * RED-first: none of the new APIs exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/fx.h"

#define SR 48000.0f
#define N 48000u

static float IN[N];
static float OUT[N];
static float REF[N];

static void sine(float *b, uint32_t n, double f, double amp) {
    uint32_t i;
    for (i = 0; i < n; i++)
        b[i] = (float)(amp * sin(2.0 * 3.141592653589793 * f *
            (double)i / 48000.0));
}

static void dc(float *b, uint32_t n, float v) {
    uint32_t i;
    for (i = 0; i < n; i++)
        b[i] = v;
}

/* Naive single-rate reference: the pre-oversample transfer snapshot. */
static void naive_ref(const struct RiFXDist *d, const float *in, float *out,
    uint32_t n) {
    uint32_t i;
    double k = (double)d->drive / 127.0 * 8.0;
    double a = (double)d->shape / 127.0 * 0.5;
    double norm_in = (1.0 + k) + a * (1.0 + k) * (1.0 + k);
    double norm = tanh(norm_in);
    for (i = 0; i < n; i++) {
        double x1 = (double)in[i] * (1.0 + k);
        out[i] = (float)(tanh(x1 + a * x1 * x1) / norm);
    }
}

static double goertzel(const float *b, uint32_t n, double target) {
    /* Single-bin DFT magnitude at target Hz (sr 48 kHz). */
    double w = 2.0 * 3.141592653589793 * target / 48000.0;
    double cw = cos(w), sw = sin(w);
    double u0 = 0.0, u1 = 0.0;
    uint32_t i;
    for (i = 64; i < n; i++) {
        double u2 = (double)b[i] + 2.0 * cw * u0 - u1;
        u1 = u0;
        u0 = u2;
    }
    {
        double re = u0 * cw - u1;
        double im = u0 * sw;
        return sqrt(re * re + im * im) / (double)(n - 64u);
    }
}

int main(void) {
    struct RiFXDist d;
    struct RiFXComp c;
    uint32_t i, dv, sv;
    ri_fxdist_init(&d);

    /* (a) bypass exact. */
    sine(IN, N, 1000.0, 0.5);
    ri_fxdist_render(&d, IN, OUT, N);
    for (i = 0; i < N; i++)
        RI_ASSERT(OUT[i] == IN[i], "bypass differs at %u", i);

    /* (b) DC law preserved under oversampling. */
    {
        static const uint8_t drives[3] = { 1, 64, 127 };
        static const uint8_t shapes[3] = { 0, 64, 127 };
        for (dv = 0; dv < 3; dv++)
            for (sv = 0; sv < 3; sv++) {
                ri_fxdist_set(&d, drives[dv], shapes[sv]);
                dc(IN, 1024u, 1.0f);
                ri_fxdist_render(&d, IN, OUT, 1024u);
                RI_ASSERT(fabs((double)OUT[512] - 1.0) <= 1e-6,
                    "dc norm d=%u s=%u out=%.9g",
                    drives[dv], shapes[sv], (double)OUT[512]);
            }
    }

    /* (c) block-split identical. */
    {
        static float OA[4096u], OB[4096u];
        struct RiFXDist e;
        ri_fxdist_init(&e);
        ri_fxdist_set(&e, 127, 64);
        sine(IN, 4096u, 1000.0, 0.9);
        ri_fxdist_render(&e, IN, OA, 4096u);
        /* Same handle streams both halves (no re-init: state carries). */
        ri_fxdist_init(&e);
        ri_fxdist_set(&e, 127, 64);
        ri_fxdist_render(&e, IN, OB, 2048u);
        ri_fxdist_render(&e, IN + 2048u, OB + 2048u, 2048u);
        for (i = 0; i < 4096u; i++)
            RI_ASSERT(OA[i] == OB[i], "split at %u", i);
    }

    /* (d) engaged path differs from naive (oversample is live). */
    {
        int diff = 0;
        ri_fxdist_set(&d, 127, 0);
        sine(IN, N, 1000.0, 0.9);
        ri_fxdist_render(&d, IN, OUT, N);
        naive_ref(&d, IN, REF, N);
        for (i = 64; i < N; i++)
            if (OUT[i] != REF[i]) {
                diff = 1;
                break;
            }
        RI_ASSERT(diff, "oversampled == naive (OS dead?)");
    }

    /* (e) alias reduction at the 13 kHz fold. */
    {
        double g_os, g_naive;
        ri_fxdist_set(&d, 127, 0);
        sine(IN, N, 5000.0, 0.9);
        ri_fxdist_render(&d, IN, OUT, N);
        naive_ref(&d, IN, REF, N);
        g_os = goertzel(OUT, N, 13000.0);
        g_naive = goertzel(REF, N, 13000.0);
        printf("alias13k os=%.6g naive=%.6g\n", g_os, g_naive);
        RI_ASSERT(g_os < g_naive, "no alias reduction");
    }

    /* (f) ratio 1:1 passes hot DC through. */
    {
        ri_fxcomp_init(&c, SR);
        ri_fxcomp_set(&c, 64);
        ri_fxcomp_set_ratio(&c, 0);
        dc(IN, N, 0.9f);
        ri_fxcomp_render(&c, IN, OUT, N);
        RI_ASSERT(fabs((double)OUT[N - 1] - 0.9) <= 0.009,
            "ratio1 out=%.6g want 0.9", (double)OUT[N - 1]);
    }

    /* (g) max ratio compresses more than default 4:1. */
    {
        float def, max;
        ri_fxcomp_init(&c, SR);
        ri_fxcomp_set(&c, 64);
        dc(IN, N, 0.9f);
        ri_fxcomp_render(&c, IN, OUT, N);
        def = OUT[N - 1];
        ri_fxcomp_reset(&c);
        ri_fxcomp_set_ratio(&c, 127);
        ri_fxcomp_render(&c, IN, OUT, N);
        max = OUT[N - 1];
        printf("steady def=%.6g max=%.6g\n", (double)def, (double)max);
        RI_ASSERT(max < def, "ratio knob dead");
    }

    /* (h) GR meter. */
    {
        float gr;
        ri_fxcomp_init(&c, SR);
        ri_fxcomp_set(&c, 64);
        dc(IN, N, 0.01f);
        ri_fxcomp_render(&c, IN, OUT, N);
        gr = ri_fxcomp_gr_db(&c);
        RI_ASSERT(gr == 0.0f, "quiet GR=%.4g want 0", (double)gr);
        dc(IN, N, 0.9f);
        ri_fxcomp_render(&c, IN, OUT, N);
        gr = ri_fxcomp_gr_db(&c);
        printf("hot GR=%.4g dB\n", (double)gr);
        RI_ASSERT(gr < -0.5f, "hot GR=%.4g want < -0.5", (double)gr);
        ri_fxcomp_gr_reset(&c);
        RI_ASSERT(ri_fxcomp_gr_db(&c) == 0.0f, "GR reset dead");
        ri_fxcomp_reset(&c);
        ri_fxcomp_set_ratio(&c, 0);
        dc(IN, N, 0.9f);
        ri_fxcomp_render(&c, IN, OUT, N);
        RI_ASSERT(ri_fxcomp_gr_db(&c) == 0.0f, "ratio1 GR nonzero");
    }

    /* Wrapper: ratio ID + pattern 54. */
    {
        struct RIFX *cp = RiFXCreate(RI_FX_COMP);
        struct RIFX *pf = RiFXCreate(RI_FX_PCF);
        RI_ASSERT(cp && pf, "wrapper create");
        RiFXSetParam(cp, RI_FXID_COMP_RATIO, 200);
        RI_ASSERT(cp->comp.ratio_knob == 127u, "ratio clamp %u",
            cp->comp.ratio_knob);
        RiFXSetParam(pf, RI_FXID_PCF_PATTERN, 54);
        RI_ASSERT(pf->pcf_pattern == 54u, "pattern54 lost (%u)",
            pf->pcf_pattern);
        RiFXDestroy(cp);
        RiFXDestroy(pf);
    }

    RI_RESULT("fx_dist_comp");
}
