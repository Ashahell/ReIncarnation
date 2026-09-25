/* t1_fx — Task 10 (gate G10): PCF SVF + FX trio tests.
 * Gates (brief order): P-15 cutoff law ±2 cents at the 10 ledger rows in
 * reference/pcf-table.bin (SAME file the engine loads — single source);
 * determinism across restarts; delay sync error <0.1% at 120/140/174 BPM;
 * distortion unity ±0.2 dB at drive 0 + full-grid no-NaN fuzz; order-swap
 * <= 1 buffer zipper (edge energy bound); compressor make-up + no-NaN;
 * generic RIFX wrapper; table validator mutants.
 * CWD convention: repo root (same as render --909pack default pack path).
 * Analysis may use libm (tests/ only; engine/ stays kernels-only).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/pcf.h"
#include "engine/fx/fx.h"

#define SR 48000.0f
#define SRU 48000u
#define T10D "/tmp/ri/run/t10/"

static float IN[SRU], OA[SRU], OB[SRU];
static float DLBUF[96000];
static float DLBUF2[96000];

static void sine(float *b, uint32_t n, double f, double amp) {
    uint32_t i;
    for (i = 0; i < n; i++)
        b[i] = (float)(amp * sin(2.0 * 3.141592653589793 * f *
            (double)i / 48000.0));
}

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

static int finite_buf(const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        if (!(b[i] == b[i]) || b[i] > 1e30f || b[i] < -1e30f)
            return 0;
    return 1;
}

int main(void) {
    struct PCFTable t;
    int nr;
    uint32_t i;

    /* --- 0. table loads from the ledger data file (single source) --- */
    nr = pcf_table_load("reference/pcf-table.bin", &t);
    RI_ASSERT(nr == 10, "table rows %d want 10", nr);
    if (nr == 10) {
        RI_ASSERT(t.n == 10u, "table count");
        RI_ASSERT(t.rows[4].v == 64 && t.rows[4].amt == 4, "row4 id");
        RI_ASSERT(t.rows[4].base_fc == 1000.0f, "row4 base");
        RI_ASSERT(t.rows[4].expected_fc == 1000.0f, "row4 unity");
    }

    /* --- 1. P-15 cutoff law ±2 cents at all 10 rows --- */
    if (nr == 10) {
        for (i = 0; i < t.n; i++) {
            float got = pcf_cutoff_hz(t.rows[i].base_fc,
                (int)t.rows[i].v, (float)t.rows[i].amt);
            double cents = 1200.0 * log2((double)got /
                (double)t.rows[i].expected_fc);
            RI_ASSERT(fabs(cents) <= 2.0,
                "row %u v=%u amt=%d cents=%.4g", i, t.rows[i].v,
                t.rows[i].amt, cents);
            if (i == 0 || i == 8)
                printf("row %u: v=%u amt=%d got=%.3f want=%.3f (%.4g cents)\n",
                    i, t.rows[i].v, t.rows[i].amt, (double)got,
                    (double)t.rows[i].expected_fc, cents);
        }
        /* clamps, not law: out-of-range never escapes */
        RI_ASSERT(pcf_cutoff_hz(1000.0f, -5, 4.0f) ==
            pcf_cutoff_hz(1000.0f, 0, 4.0f), "v low clamp");
        RI_ASSERT(pcf_cutoff_hz(1000.0f, 200, 4.0f) ==
            pcf_cutoff_hz(1000.0f, 127, 4.0f), "v high clamp");
        RI_ASSERT(pcf_cutoff_hz(1000.0f, 0, 99.0f) ==
            pcf_cutoff_hz(1000.0f, 0, 4.0f), "amt clamp");
        RI_ASSERT(pcf_cutoff_hz(0.0f, 64, 4.0f) == 0.0f, "base guard");
    }

    /* --- 2. PCF determinism + SVF sanity (DC unity, HF cut) --- */
    {
        struct PCF a, b;
        float r_dc, r_hf;
        pcf_init(&a);
        pcf_init(&b);
        RI_ASSERT(a.svf.low == 0.0f && a.svf.band == 0.0f, "init state");
        RI_ASSERT(a.base_fc == 1000.0f && a.q == 2.0f, "init params");
        for (i = 0; i < SRU; i++)
            IN[i] = 0.5f;
        pcf_render(&a, IN, OA, SRU, SR);
        pcf_render(&b, IN, OB, SRU, SR);
        for (i = 0; i < SRU; i++)
            RI_ASSERT(OA[i] == OB[i], "pcf nondet at %u", i);
        r_dc = rms(OA + SRU - 4096, 4096);
        RI_ASSERT(fabs(r_dc - 0.5f) <= 0.005f, "dc gain %.5g", (double)r_dc);
        printf("pcf dc settled rms: %.6g\n", (double)r_dc);
        sine(IN, SRU, 10000.0, 0.5);
        pcf_init(&a);
        pcf_render(&a, IN, OA, SRU, SR);
        r_hf = rms(OA + SRU - 4096, 4096);
        RI_ASSERT(r_hf < 0.25f, "hf not cut (%.5g)", (double)r_hf);
        RI_ASSERT(finite_buf(OA, SRU), "pcf non-finite");
        RI_ASSERT(a.pos_smp == SRU && a.last_step > 0u, "clock not advancing");
    }

    /* --- 3. delay sync error <0.1% at 120/140/174 BPM --- */
    {
        static const float bpms[3] = { 120.0f, 140.0f, 174.0f };
        for (i = 0; i < 3; i++) {
            struct RiFXDelay d;
            float want, err;
            uint32_t s, k, echo = 0;
            RI_ASSERT(ri_fxdelay_init(&d, DLBUF, 96000u) == 0, "dl init");
            ri_fxdelay_set(&d, 0, 127);
            s = ri_fxdelay_sync(&d, bpms[i], 0.75f, SR);
            want = 0.75f * 60.0f * SR / bpms[i];
            err = (float)fabs((double)s - (double)want) / want * 100.0f;
            RI_ASSERT(err < 0.1f, "bpm %.0f sync err %.5g%%",
                (double)bpms[i], (double)err);
            for (k = 0; k < 40000u; k++)
                IN[k] = (k == 0) ? 1.0f : 0.0f;
            ri_fxdelay_render(&d, IN, OA, 40000u);
            for (k = 1; k < 40000u; k++) {
                if (OA[k] > 0.5f || OA[k] < -0.5f) {
                    echo = k;
                    break;
                }
            }
            RI_ASSERT(echo == s, "bpm %.0f echo %u want %u",
                (double)bpms[i], echo, s);
            printf("bpm %.0f: delay %u smp, sync err %.5g%%\n",
                (double)bpms[i], s, (double)err);
        }
        RI_ASSERT(ri_fxdelay_init(0, DLBUF, 96000u) == 2, "dl null");
        RI_ASSERT(ri_fxdelay_init((struct RiFXDelay *)OA, DLBUF, 8u) == 2,
            "dl small");
    }

    /* --- 4. distortion unity ±0.2 dB at drive 0 + grid no-NaN fuzz --- */
    {
        struct RiFXDist d;
        float r0, r1, ratio_db;
        uint32_t dv, sv, k;
        static const float probe[8] = { -2.0f, -1.0f, -0.5f, 0.0f, 0.25f,
            0.5f, 1.0f, 2.0f };
        ri_fxdist_init(&d);
        sine(IN, SRU, 1000.0, 0.5);
        r0 = rms(IN, SRU);
        ri_fxdist_render(&d, IN, OA, SRU);
        r1 = rms(OA, SRU);
        ratio_db = (float)fabs(20.0 * log10((double)r1 / (double)r0));
        RI_ASSERT(ratio_db <= 0.2f, "drive0 unity %.4g dB", (double)ratio_db);
        printf("dist drive0 unity: %.4g dB\n", (double)ratio_db);
        /* 16x16 grid (fix round 1: 0..120 step 8 per axis — 127 is
         * clamp-covered, not gridded): engaged cells finite + bounded. */
        for (dv = 0; dv <= 127u; dv += 8u) {
            for (sv = 0; sv <= 127u; sv += 8u) {
                float mx = 0.0f;
                ri_fxdist_set(&d, (uint8_t)dv, (uint8_t)sv);
                for (k = 0; k < 8u; k++)
                    IN[k] = probe[k];
                sine(IN + 8, 1016, 1000.0, 0.9);
                ri_fxdist_render(&d, IN, OA, 1024u);
                RI_ASSERT(finite_buf(OA, 1024u), "nan d=%u s=%u", dv, sv);
                if (dv == 0u && sv == 0u) {
                    /* bypass cell: bit-exact passthrough, extremes
                     * included (linear path clips nowhere) */
                    for (k = 0; k < 1024u; k++)
                        RI_ASSERT(OA[k] == IN[k], "bypass inexact");
                    continue;
                }
                for (k = 0; k < 1024u; k++) {
                    float a = OA[k] < 0.0f ? -OA[k] : OA[k];
                    if (a > mx)
                        mx = a;
                }
                RI_ASSERT(mx <= 1.5f, "drive bound d=%u s=%u mx=%.4g",
                    dv, sv, (double)mx);
            }
        }
        /* engaged drive is a real curve, not a bypass rename */
        ri_fxdist_set(&d, 127, 0);
        sine(IN, SRU, 1000.0, 0.9);
        ri_fxdist_render(&d, IN, OA, SRU);
        RI_ASSERT(fabs((double)rms(OA, SRU) - (double)rms(IN, SRU)) > 0.01,
            "full drive changes nothing");
    }

    /* --- 5. compressor: make-up honored, hot crest cut, no-NaN --- */
    {
        struct RiFXComp c;
        float r0, r1, mu_db;
        uint32_t tv, k;
        RI_ASSERT(ri_fxcomp_init(&c, SR) == 0, "comp init");
        RI_ASSERT(ri_fxcomp_init(0, SR) == 2, "comp null");
        sine(IN, SRU, 440.0, 0.05);
        r0 = rms(IN, SRU);
        ri_fxcomp_render(&c, IN, OA, SRU);
        r1 = rms(OA, SRU);
        mu_db = (float)(20.0 * log10((double)r1 / (double)r0));
        /* thresh 64 = -19.84 dBFS, MU = +14.88 dB by the premult */
        RI_ASSERT(fabs(mu_db - 14.88) <= 0.5,
            "makeup %.3g dB want 14.88", (double)mu_db);
        printf("comp low-level gain (make-up): %.3g dB\n", (double)mu_db);
        sine(IN, SRU, 440.0, 0.9);
        ri_fxcomp_reset(&c);
        ri_fxcomp_render(&c, IN, OA, SRU);
        {
            float pk_in = 0.0f, pk_out = 0.0f;
            for (k = SRU - 4096; k < SRU; k++) {
                float a = IN[k] < 0.0f ? -IN[k] : IN[k];
                float b2 = OA[k] < 0.0f ? -OA[k] : OA[k];
                if (a > pk_in)
                    pk_in = a;
                if (b2 > pk_out)
                    pk_out = b2;
            }
            /* compressed peak must sit below the linear+MU projection */
            RI_ASSERT(pk_out < pk_in * c.mu_lin, "no compression");
            RI_ASSERT(finite_buf(OA, SRU), "comp hot non-finite");
            printf("comp hot: pk_in=%.4g pk_out=%.4g mu=%.4g\n",
                (double)pk_in, (double)pk_out, (double)c.mu_lin);
        }
        for (tv = 0; tv <= 127u; tv += 43u) {
            ri_fxcomp_set(&c, (uint8_t)tv);
            for (k = 0; k < 8u; k++)
                IN[k] = (k & 1) ? 2.0f : -2.0f;
            sine(IN + 8, 1016, 1000.0, 0.9);
            ri_fxcomp_render(&c, IN, OA, 1024u);
            RI_ASSERT(finite_buf(OA, 1024u), "comp nan t=%u", tv);
        }
        /* determinism */
        ri_fxcomp_reset(&c);
        sine(IN, 8192, 440.0, 0.4);
        ri_fxcomp_render(&c, IN, OA, 8192u);
        ri_fxcomp_reset(&c);
        ri_fxcomp_render(&c, IN, OB, 8192u);
        for (k = 0; k < 8192u; k++)
            RI_ASSERT(OA[k] == OB[k], "comp nondet at %u", k);
    }

    /* --- 6. order-swap <= 1 buffer zipper (dist<->pcf edge energy) ---
     * Mid-stream reorder on shared instances vs the ideal chain that ran
     * the new order all along: the edge transient energy over the
     * post-swap segment is bounded by 1 full-scale 64-frame buffer. */
    {
        struct PCF p, q;
        struct RiFXDist d;
        double energy = 0.0;
        uint32_t k;
        sine(IN, 8192, 440.0, 0.5);
        /* swap path: A = dist->pcf for seg1, then B = pcf->dist, shared */
        pcf_init(&p);
        p.base_fc = 2000.0f;
        p.q = 2.0f;
        ri_fxdist_init(&d);
        ri_fxdist_set(&d, 64, 32);
        ri_fxdist_render(&d, IN, OA, 4096u);
        pcf_render(&p, OA, OA, 4096u, SR);
        pcf_render(&p, IN + 4096u, OA + 4096u, 4096u, SR);
        ri_fxdist_render(&d, OA + 4096u, OA + 4096u, 4096u);
        /* ideal path: fresh B for both segments (seg1 discarded) */
        pcf_init(&q);
        q.base_fc = 2000.0f;
        q.q = 2.0f;
        pcf_render(&q, IN, OB, 4096u, SR);
        pcf_render(&q, IN + 4096u, OB + 4096u, 4096u, SR);
        ri_fxdist_render(&d, OB + 4096u, OB + 4096u, 4096u);
        for (k = 4096u; k < 8192u; k++) {
            double dd = (double)OA[k] - (double)OB[k];
            energy += dd * dd;
        }
        printf("order swap: edge energy %.4g (bound 64)\n", energy);
        RI_ASSERT(energy <= 64.0, "zipper energy %.4g > 64", energy);
    }

    /* --- 7. generic wrapper: create/set/render finite --- */
    {
        static float dlline[262144];
        struct RIFX *dl = RiFXCreateDelay(dlline, 262144u);
        struct RIFX *ds = RiFXCreate(RI_FX_DIST);
        struct RIFX *cp = RiFXCreate(RI_FX_COMP);
        struct RIFX *pf = RiFXCreate(RI_FX_PCF);
        RI_ASSERT(dl && ds && cp && pf, "wrapper create");
        RI_ASSERT(RiFXCreate(99) == 0, "wrapper bad type");
        RI_ASSERT(RiFXCreate(RI_FX_DELAY) == 0, "Create(DELAY) not retired");
        RiFXSetParam(ds, RI_FXID_DIST_DRIVE, 64);
        RiFXSetParam(ds, RI_FXID_DIST_SHAPE, 32);
        RiFXSetParam(cp, RI_FXID_COMP_THRESH, 64);
        RiFXSetParam(pf, RI_FXID_PCF_BASE, 80);
        RiFXSetParam(pf, RI_FXID_PCF_Q, 64);
        RiFXSetParam(pf, RI_FXID_PCF_AMT, 64);
        RiFXSetParam(pf, RI_FXID_PCF_MODE, 0);
        RiFXSetParam(0, RI_FXID_DIST_DRIVE, 64); /* null-safe */
        RiFXRender(0, IN, OA, 64u, SR, 140.0f); /* null-safe */
        sine(IN, 4096, 440.0, 0.4);
        RiFXRender(dl, IN, OA, 4096u, SR, 140.0f);
        RI_ASSERT(finite_buf(OA, 4096u), "wrap delay non-finite");
        RiFXRender(ds, IN, OA, 4096u, SR, 140.0f);
        RI_ASSERT(finite_buf(OA, 4096u), "wrap dist non-finite");
        RiFXRender(cp, IN, OA, 4096u, SR, 140.0f);
        RI_ASSERT(finite_buf(OA, 4096u), "wrap comp non-finite");
        RiFXRender(pf, IN, OA, 4096u, SR, 140.0f);
        RI_ASSERT(finite_buf(OA, 4096u), "wrap pcf non-finite");
        RI_ASSERT(rms(OA, 4096u) > 0.01f, "wrap pcf silent");
        RiFXReset(dl);
        RiFXReset(cp);
        /* wrapper PCF streams block-boundary-clean (fix round 1): one
         * 4096 call vs two 2048 halves on the same handle, bit-identical.
         * The old per-call re-init failed this (fresh SVF + beat_pos in
         * the second half). */
        RiFXReset(pf);
        RiFXRender(pf, IN, OA, 4096u, SR, 140.0f);
        RiFXReset(pf);
        RiFXRender(pf, IN, OB, 2048u, SR, 140.0f);
        RiFXRender(pf, IN + 2048u, OB + 2048u, 2048u, SR, 140.0f);
        for (i = 0; i < 4096u; i++)
            RI_ASSERT(OA[i] == OB[i], "wrap pcf split at %u", i);
        /* destroy/reuse (§12.8a): freed slots recycle, pool stays full. */
        RI_ASSERT(RiFXValid(dl) == 0, "dl not valid");
        RI_ASSERT(RiFXValid(pf) == 0, "pf not valid");
        RI_ASSERT(RiFXValid(0) == 2, "null valid");
        RiFXDestroy(dl);
        RiFXDestroy(ds);
        {
            struct RIFX *d2 = RiFXCreateDelay(dlline, 262144u);
            struct RIFX *c2 = RiFXCreate(RI_FX_COMP);
            RI_ASSERT(d2 != 0 && c2 != 0, "slots not reused");
            RiFXDestroy(d2);
            RiFXDestroy(c2);
        }
        RiFXDestroy(cp);
        RiFXDestroy(pf);
    }

    /* --- 8. validator mutants (wrong magic, truncated) --- */
    {
        struct PCFTable m;
        FILE *f;
        static unsigned char img[256];
        FILE *g = fopen("reference/pcf-table.bin", "rb");
        uint32_t got = 0;
        if (g) {
            got = (uint32_t)fread(img, 1, sizeof img, g);
            fclose(g);
        }
        RI_ASSERT(got == 132u, "table image %u bytes", got);
        img[0] = 'X';
        f = fopen(T10D "badmagic.bin", "wb");
        RI_ASSERT(f != 0, "write badmagic");
        if (f) {
            fwrite(img, 1, got, f);
            fclose(f);
        }
        RI_ASSERT(pcf_table_load(T10D "badmagic.bin", &m) == -2,
            "bad magic accepted");
        f = fopen(T10D "trunc.bin", "wb");
        RI_ASSERT(f != 0, "write trunc");
        if (f) {
            fwrite(img, 1, 20, f);
            fclose(f);
        }
        RI_ASSERT(pcf_table_load(T10D "trunc.bin", &m) < 0,
            "truncated accepted");
        RI_ASSERT(pcf_table_load(T10D "nope.bin", &m) == -1,
            "missing accepted");
        RI_ASSERT(pcf_table_load("reference/pcf-table.bin", 0) == -1,
            "null table accepted");
    }

    /* --- 9. chain determinism (D1): dist->pcf->delay->comp identical --- */
    {
        struct PCF p;
        struct RiFXDelay d;
        struct RiFXDist ds;
        struct RiFXComp c;
        uint32_t k;
        sine(IN, 8192, 440.0, 0.4);
        pcf_init(&p);
        p.base_fc = 3000.0f;
        RI_ASSERT(ri_fxdelay_init(&d, DLBUF2, 96000u) == 0, "chain dl");
        ri_fxdelay_sync(&d, 140.0f, 0.75f, SR);
        ri_fxdelay_set(&d, 40, 64);
        ri_fxdist_init(&ds);
        ri_fxdist_set(&ds, 32, 16);
        RI_ASSERT(ri_fxcomp_init(&c, SR) == 0, "chain comp");
        ri_fxdist_render(&ds, IN, OA, 8192u);
        pcf_render(&p, OA, OA, 8192u, SR);
        ri_fxdelay_render(&d, OA, OA, 8192u);
        ri_fxcomp_render(&c, OA, OA, 8192u);
        for (k = 0; k < 8192u; k++)
            OB[k] = OA[k];
        /* restart every instance, replay */
        pcf_init(&p);
        p.base_fc = 3000.0f;
        ri_fxdelay_reset(&d);
        ri_fxcomp_reset(&c);
        ri_fxdist_render(&ds, IN, OA, 8192u);
        pcf_render(&p, OA, OA, 8192u, SR);
        ri_fxdelay_render(&d, OA, OA, 8192u);
        ri_fxcomp_render(&c, OA, OA, 8192u);
        for (k = 0; k < 8192u; k++)
            RI_ASSERT(OA[k] == OB[k], "chain nondet at %u", k);
    }

    RI_RESULT("fx");
}
