/* levi_bench — HOST timing + profile harness for levi_voice_render_sum_stereo.
 *
 * P1 rewrite (levi-perf): every sweep row carries a positive control (a
 * RI_LEVI_PROFILE counter proving the swept quantity actually changed), and
 * timings are median-of-7 with min/max. Two build modes:
 *
 *   timing only : gcc -std=gnu99 -O2 -I. -o /tmp/levi_bench
 *                     tests/unit/levi_bench.c /tmp/ri/build/*.o -lm -lpng
 *   with profile: same but -DRI_LEVI_PROFILE and /tmp/ri/prof/*.o
 *                 (profile objects live in a separate directory; the
 *                 shipping objects are untouched — see G5 evidence).
 *
 * Sections:
 *   1. held voices 0..8 (default DUO patch; comparable to 2026-10-04).
 *   2. rendered operators per voice-sample. Rows change live[] through the
 *      PUBLIC path (algo/morph/slot setters); a counter asserts the rendered
 *      count. Includes the LEGACY row (the 2026-10-04 slot poke without
 *      morph_apply) to confirm the defect with a counter (G3).
 *   3. morph 0/50/100 with per-bank voice_pass counters (G4), plus a
 *      SILENCE-bank price row for bank B (price only, not exact).
 *   4. matrix empty / 1 route / 8 routes + macros.
 *   5. digital filter types, analog drive, pan modes, LFO consumer, menv
 *      curve, and the worst-case patch.
 *
 * Ungated by design (asserts no machine-dependent bound); the only assert is
 * the non-silence signal check.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#ifdef RI_LEVI_PROFILE
#include "engine/dsp/kernels.h"
#endif

#define BLK 64u
#define BLOCKS 400u
#define REPS 7u

static double now_s(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static struct RILeviSet S;
static float L[BLK], R[BLK];

static void settle(uint32_t blocks) {
    uint32_t i;
    for (i = 0u; i < blocks; i++)
        levi_voice_render_sum_stereo(&S, L, R, BLK, 48000.0f);
}

/* One timed run: warm caches, then time BLOCKS blocks. */
static double run_ns(void) {
    double t0, t1;
    uint32_t i;
    settle(32u);
    t0 = now_s();
    for (i = 0u; i < BLOCKS; i++)
        levi_voice_render_sum_stereo(&S, L, R, BLK, 48000.0f);
    t1 = now_s();
    return (t1 - t0) * 1e9 / (double)BLOCKS;
}

static int cmpd(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

/* Median of REPS runs; min/max through the pointers. */
static double med7(double *mn, double *mx) {
    double r[REPS];
    uint32_t i;
    for (i = 0u; i < REPS; i++)
        r[i] = run_ns();
    qsort(r, REPS, sizeof r[0], cmpd);
    *mn = r[0];
    *mx = r[REPS - 1u];
    return r[REPS / 2u];
}

static void default_patch(uint32_t voices) {
    uint32_t v;
    levi_init_set(&S);
    for (v = 0u; v < voices && v < RI_LEVI_NVOICES; v++) {
        levi_set_param_ui(&S, v, RI_CTL_LEVI_VSPREAD & 0xFFu, 64u);
        levi_trigger(&S, v, (uint8_t)(48u + v));
    }
    settle(8u);
}

#ifdef RI_LEVI_PROFILE
static void prof_reset_all(void) {
    levi_profile_reset(&S);
    ri_prof_reset();
    ri_prof_tptg_reset();
}

/* Render N steady samples and print the counter table. Returns rendered
 * operator count (for the row's positive control). */
static uint64_t prof_dump(uint32_t n, uint32_t voices) {
    uint64_t ops = 0u, pa = 0u, pb = 0u, ma = 0u, mr = 0u, lfo = 0u;
    uint64_t eo = 0u, em = 0u, ch = 0u, du = 0u, pn = 0u, ph = 0u, th = 0u;
    uint32_t v, i, blk = n;
    prof_reset_all();
    for (i = 0u; i < n; i += BLK) {
        uint32_t w = n - i > BLK ? BLK : n - i;
        (void)blk;
        levi_voice_render_sum_stereo(&S, L, R, w, 48000.0f);
    }
    for (v = 0u; v < RI_LEVI_NVOICES; v++) {
        ops += S.v[v].prof_ops; pa += S.v[v].prof_passA; pb += S.v[v].prof_passB;
        ma += S.v[v].prof_modapply; mr += S.v[v].prof_mrows;
        lfo += S.v[v].prof_lfo; eo += S.v[v].prof_envop; em += S.v[v].prof_envmod;
        ch += S.v[v].prof_chain; du += S.v[v].prof_dual;
        pn += S.v[v].prof_pan; ph += S.v[v].prof_panhit;
        th += S.v[v].prof_tptghit;
    }
    {
        double vs = (double)n * (double)voices;
        printf("    counters/voice-sample: ops=%.2f passA=%.2f passB=%.2f",
            (double)ops / vs, (double)pa / vs, (double)pb / vs);
        printf(" sin=%.1f pow2=%.1f log2=%.2f tanh=%.2f exp=%.2f",
            (double)ri_prof_sin / vs, (double)ri_prof_pow2 / vs,
            (double)ri_prof_log2 / vs, (double)ri_prof_tanh / vs,
            (double)ri_prof_exp / vs);
        printf(" tptg=%.1f tptghit=%.2f modapply=%.2f mrows=%.2f lfo=%.1f",
            (double)ri_prof_tptg / vs, (double)th / vs, (double)ma / vs,
            (double)mr / vs, (double)lfo / vs);
        printf(" envop=%.1f envmod=%.1f chain=%.1f dual=%.3f pan=%.1f panhit=%.2f\n",
            (double)eo / vs, (double)em / vs, (double)ch / vs,
            (double)du / vs, (double)pn / vs, (double)ph / vs);
    }
    return ops;
}
#else
static uint64_t prof_dump(uint32_t n, uint32_t voices) {
    (void)n;
    (void)voices;
    return 0u;
}
#endif

static void trow(const char *label, uint32_t voices_for_norm) {
    double mn, mx, md = med7(&mn, &mx);
    printf("  %-34s %10.1f  (%8.1f..%8.1f)  %7.4f us/vs\n", label, md, mn, mx,
        md / 1000.0 / (double)BLK /
        (voices_for_norm ? (double)voices_for_norm : 1.0));
}

int main(void) {
    double mn, mx;
    uint32_t k;
    printf("levi voice-render host bench (P1: controls + median-of-7)\n");
    printf("  build: %s%s\n",
#if defined(__OPTIMIZE__)
        "optimised",
#else
        "UNOPTIMISED",
#endif
#ifdef RI_LEVI_PROFILE
        " + RI_LEVI_PROFILE"
#else
        ""
#endif
    );
    printf("  block: %u samples, %u blocks x %u reps\n\n", (unsigned)BLK,
        (unsigned)BLOCKS, (unsigned)REPS);

    /* --- 1. held voices, default DUO patch --- */
    printf("=== 1. held voices (default patch; marginal step = one voice) ===\n");
    {
        double v[RI_LEVI_NVOICES + 1u];
        printf("  voices | ns/block (median, min..max) | us/voice-sample\n");
        for (k = 0u; k <= RI_LEVI_NVOICES; k++) {
            double md;
            default_patch(k);
            md = med7(&mn, &mx);
            v[k] = md;
            printf("  %6u | %10.1f  (%8.1f..%8.1f) |", (unsigned)k, md, mn, mx);
            if (k > 0u)
                printf(" %14.4f\n", (md - v[k - 1u]) / (double)BLK / 1000.0);
            else
                printf(" %14s\n", "-");
        }
        printf("  fixed (0v): %.3f us/block; 8v marginal mean: %.4f us/vs\n",
            v[0] / 1000.0, (v[RI_LEVI_NVOICES] - v[0]) / 8.0 / (double)BLK / 1000.0);
    }

    /* --- 2. rendered operators (public-path rows + legacy defect row) --- */
    printf("\n=== 2. rendered operators, ONE voice (control = ops counter) ===\n");
    {
        struct {
            const char *label;
            uint32_t a, b; /* bank presets (custom flag for SILENCE bank) */
            uint32_t amode, s1, mpos;
            uint32_t want_ops;
        } rows[] = {
            { "A8+B8 STACK8/m50", RI_LEVI_ALGO_STACK8, RI_LEVI_ALGO_STACK8, 0u, 0u, 50u, 16u },
            { "A8+B2 STACK8/DUO", RI_LEVI_ALGO_STACK8, RI_LEVI_ALGO_DUO, 0u, 0u, 50u, 10u },
            { "A8+B0 slot SILENCE", RI_LEVI_ALGO_STACK8, 0u, 1u, 1u, 0u, 8u },
            { "A2+B2 DUO/m50", RI_LEVI_ALGO_DUO, RI_LEVI_ALGO_DUO, 0u, 0u, 50u, 4u },
            { "A2+B0 slot SILENCE", RI_LEVI_ALGO_DUO, 0u, 1u, 1u, 0u, 2u },
        };
        uint32_t r;
        for (r = 0u; r < sizeof rows / sizeof rows[0]; r++) {
            uint64_t ops;
            levi_init_set(&S);
            if (rows[r].amode) {
                levi_set_amode(&S, 0u, RI_LEVI_AMODE_MORPH);
                levi_set_slot(&S, 0u, 0u, rows[r].a);
                levi_set_slot(&S, 0u, 1u, RI_LEVI_SLOT_SILENCE);
                levi_set_mpos(&S, 0u, rows[r].mpos);
            } else {
                levi_set_algo(&S, 0u, rows[r].a);
                levi_set_morph(&S, 0u, rows[r].b, rows[r].mpos);
            }
            levi_trigger(&S, 0u, 60u);
            settle(8u);
            ops = prof_dump(4800u, 1u);
#ifdef RI_LEVI_PROFILE
            printf("  %-22s ops/voice-sample=%llu (want %u) %s\n",
                rows[r].label, (unsigned long long)(ops / 4800u),
                (unsigned)rows[r].want_ops,
                ops / 4800u == rows[r].want_ops ? "CONTROL-OK" : "CONTROL-FAIL");
#endif
            trow(rows[r].label, 1u);
        }
        /* LEGACY defect row (G3): the 2026-10-04 poke, slot without apply. */
        {
            uint64_t ops;
            uint32_t o;
            levi_init_set(&S);
            levi_trigger(&S, 0u, 60u);
            for (o = 2u; o < RI_LEVI_NOPS; o++)
                S.v[0].slot[o] = (uint8_t)RI_LEVI_SLOT_SILENCE;
            settle(8u);
            ops = prof_dump(4800u, 1u);
#ifdef RI_LEVI_PROFILE
            printf("  LEGACY poke (no apply): ops/voice-sample=%llu (live[] untouched: defect confirmed)\n",
                (unsigned long long)(ops / 4800u));
#endif
            trow("LEGACY poke no-apply", 1u);
        }
    }

    /* --- 3. morph with per-bank counters --- */
    printf("\n=== 3. morph banks, 4 voices (control = passA/passB) ===\n");
    {
        static const uint32_t at[] = { 0u, 50u, 100u };
        uint32_t i;
        for (i = 0u; i < 3u; i++) {
            uint32_t vv;
            levi_init_set(&S);
            for (vv = 0u; vv < 4u; vv++) {
                levi_set_algo(&S, vv, RI_LEVI_ALGO_STACK44);
                levi_set_morph(&S, vv, RI_LEVI_ALGO_PAIRS4, at[i]);
                levi_trigger(&S, vv, (uint8_t)(48u + vv));
            }
            settle(8u);
#ifdef RI_LEVI_PROFILE
            {
                uint64_t pa = 0u, pb = 0u;
                prof_dump(4800u, 4u);
                for (vv = 0u; vv < 4u; vv++) {
                    pa += S.v[vv].prof_passA;
                    pb += S.v[vv].prof_passB;
                }
                printf("  morph %3u: bankA voicesamples=%llu bankB=%llu\n",
                    (unsigned)at[i], (unsigned long long)(pa / 4800u),
                    (unsigned long long)(pb / 4800u));
            }
#endif
            {
                char lb[48];
                snprintf(lb, sizeof lb, "morph %u", (unsigned)at[i]);
                trow(lb, 4u);
            }
        }
        /* Price-only row (NOT exact): bank B renders SILENCE, so the gap to
         * morph 0 prices the second voice_pass without changing bank A. */
        {
            uint32_t vv;
            levi_init_set(&S);
            for (vv = 0u; vv < 4u; vv++) {
                levi_set_amode(&S, vv, RI_LEVI_AMODE_MORPH);
                levi_set_slot(&S, vv, 0u, RI_LEVI_ALGO_STACK44);
                levi_set_slot(&S, vv, 1u, RI_LEVI_SLOT_SILENCE);
                levi_set_mpos(&S, vv, 0u);
                levi_trigger(&S, vv, (uint8_t)(48u + vv));
            }
            settle(8u);
            (void)prof_dump(4800u, 4u);
            trow("morph0 SILENCE-B (price only)", 4u);
        }
    }

    /* --- 4. matrix load --- */
    printf("\n=== 4. matrix load, 2 voices STACK8/m50 ===\n");
    {
        uint32_t vv;
        levi_init_set(&S);
        for (vv = 0u; vv < 2u; vv++) {
            levi_set_algo(&S, vv, RI_LEVI_ALGO_STACK8);
            levi_set_morph(&S, vv, RI_LEVI_ALGO_STACK8, 50u);
            levi_trigger(&S, vv, (uint8_t)(60u + vv * 4u));
        }
        settle(8u);
        (void)prof_dump(4800u, 2u);
        trow("matrix empty", 2u);
        ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_LFO0, RI_LEVI_DM_DFILT, 0u, 12);
        settle(8u);
        (void)prof_dump(4800u, 2u);
        trow("matrix 1 route", 2u);
        ri_levi_matrix_route(&S.mx, 1u, RI_LEVI_MS_ENV0, RI_LEVI_DM_VCA, 0u, 70);
        ri_levi_matrix_route(&S.mx, 2u, RI_LEVI_MS_NOTE, RI_LEVI_DM_AFILT, 0u, 50);
        ri_levi_matrix_route(&S.mx, 3u, RI_LEVI_MS_VELON, RI_LEVI_DM_ENV1, 0u, 30);
        ri_levi_matrix_route(&S.mx, 4u, RI_LEVI_MS_WHEEL, RI_LEVI_DM_VOICE, RI_LEVI_DVO_PAN, 80);
        ri_levi_matrix_route(&S.mx, 5u, RI_LEVI_MS_LFO1, RI_LEVI_DM_LFO1, 1u, 50);
        ri_levi_matrix_route(&S.mx, 6u, RI_LEVI_MS_BEND, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 100);
        ri_levi_matrix_route(&S.mx, 7u, RI_LEVI_MS_OPENV0, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 40);
        S.mx.mknob[0] = 96u;
        settle(8u);
        (void)prof_dump(4800u, 2u);
        trow("matrix 8 routes+macro", 2u);
    }

    /* --- 5. filters, pans, LFO, menv, worst case --- */
    printf("\n=== 5. filters / pans / LFO / menv / worst ===\n");
    {
        /* Non-sine waves (C2 prize rows): all 8 ALLPAR carriers on SAW,
         * then on narrow PULSE. */
        uint32_t o;
        levi_init_set(&S);
        levi_set_algo(&S, 0u, RI_LEVI_ALGO_ALLPAR);
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_WAVE, 3u);
        levi_trigger(&S, 0u, 60u);
        settle(8u);
        (void)prof_dump(4800u, 1u);
        trow("waves saw x8", 1u);
        levi_init_set(&S);
        levi_set_algo(&S, 0u, RI_LEVI_ALGO_ALLPAR);
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_WAVE, 14u);
        levi_trigger(&S, 0u, 60u);
        settle(8u);
        (void)prof_dump(4800u, 1u);
        trow("waves pulse x8", 1u);
    }
    {
        uint32_t t;
        for (t = 0u; t < RI_LEVI_NDF; t++) {
            char lb[48];
            levi_init_set(&S);
            levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)t);
            levi_trigger(&S, 0u, 57u);
            settle(8u);
            (void)prof_dump(2400u, 1u);
            snprintf(lb, sizeof lb, "dfilt %u", (unsigned)t);
            trow(lb, 1u);
        }
        levi_init_set(&S);
        levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DRIVE & 0xFFu, 0u);
        levi_trigger(&S, 0u, 55u);
        settle(8u);
        (void)prof_dump(2400u, 1u);
        trow("analog drive 0", 1u);
        levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DRIVE & 0xFFu, 127u);
        settle(8u);
        (void)prof_dump(2400u, 1u);
        trow("analog drive max", 1u);
        {
            uint32_t m;
            for (m = 0u; m < 3u; m++) {
                char lb[48];
                levi_init_set(&S);
                levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, (uint8_t)m);
                levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 96u);
                levi_trigger(&S, 0u, 60u);
                settle(8u);
                (void)prof_dump(2400u, 1u);
                snprintf(lb, sizeof lb, "panmode %u spread", (unsigned)m);
                trow(lb, 1u);
            }
            levi_init_set(&S);
            levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 64u);
            levi_trigger(&S, 0u, 60u);
            settle(8u);
            (void)prof_dump(2400u, 1u);
            trow("pan centred", 1u);
        }
        levi_init_set(&S);
        levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_OP_ACURVE, 64u);
        levi_trigger(&S, 0u, 60u);
        settle(8u);
        (void)prof_dump(2400u, 1u);
        trow("menv curve 0", 1u);
        levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_OP_ACURVE, 96u);
        settle(8u);
        (void)prof_dump(2400u, 1u);
        trow("menv curve nonzero", 1u);
        {
            uint32_t vv;
            levi_init_set(&S);
            for (vv = 0u; vv < RI_LEVI_NVOICES; vv++) {
                levi_set_algo(&S, vv, RI_LEVI_ALGO_STACK8);
                levi_set_morph(&S, vv, RI_LEVI_ALGO_STACK44, 50u);
                levi_trigger(&S, vv, (uint8_t)(48u + vv));
            }
            ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_OPENV0, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 40);
            ri_levi_matrix_route(&S.mx, 1u, RI_LEVI_MS_LFO0, RI_LEVI_DM_DFILT, 0u, -60);
            ri_levi_matrix_route(&S.mx, 2u, RI_LEVI_MS_ENV0, RI_LEVI_DM_VCA, 0u, 70);
            ri_levi_matrix_route(&S.mx, 3u, RI_LEVI_MS_NOTE, RI_LEVI_DM_AFILT, 0u, 50);
            ri_levi_matrix_route(&S.mx, 4u, RI_LEVI_MS_VELON, RI_LEVI_DM_ENV1, 0u, 30);
            ri_levi_matrix_route(&S.mx, 5u, RI_LEVI_MS_WHEEL, RI_LEVI_DM_VOICE, RI_LEVI_DVO_PAN, 80);
            ri_levi_matrix_route(&S.mx, 6u, RI_LEVI_MS_LFO1, RI_LEVI_DM_LFO1, 1u, 50);
            ri_levi_matrix_route(&S.mx, 7u, RI_LEVI_MS_BEND, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 100);
            settle(8u);
            (void)prof_dump(4800u, 8u);
            trow("worst: 8v STACK8 m50 mx8", 8u);
        }
        {
            /* Ablation A: worst setup, matrix emptied (matrix share on the
             * worst base). */
            uint32_t vv;
            levi_init_set(&S);
            for (vv = 0u; vv < RI_LEVI_NVOICES; vv++) {
                levi_set_algo(&S, vv, RI_LEVI_ALGO_STACK8);
                levi_set_morph(&S, vv, RI_LEVI_ALGO_STACK44, 50u);
                levi_trigger(&S, vv, (uint8_t)(48u + vv));
            }
            settle(8u);
            (void)prof_dump(4800u, 8u);
            trow("worst-nomx: 8v STACK8 m50", 8u);
        }
        {
            /* Ablation B: worst setup, bank B SILENCE (bank-B share on the
             * worst base; price only, not exact). */
            uint32_t vv;
            levi_init_set(&S);
            for (vv = 0u; vv < RI_LEVI_NVOICES; vv++) {
                levi_set_amode(&S, vv, RI_LEVI_AMODE_MORPH);
                levi_set_slot(&S, vv, 0u, RI_LEVI_ALGO_STACK8);
                levi_set_slot(&S, vv, 1u, RI_LEVI_SLOT_SILENCE);
                levi_set_mpos(&S, vv, 0u);
                levi_trigger(&S, vv, (uint8_t)(48u + vv));
            }
            settle(8u);
            (void)prof_dump(4800u, 8u);
            trow("worst-silB: 8v STACK8 B0", 8u);
        }
    }

    {
        double peak = 0.0;
        uint32_t i;
        default_patch(4u);
        memset(L, 0, sizeof L);
        memset(R, 0, sizeof R);
        levi_voice_render_sum_stereo(&S, L, R, BLK, 48000.0f);
        for (i = 0u; i < BLK; i++) {
            double a = L[i] < 0 ? -L[i] : L[i];
            double b = R[i] < 0 ? -R[i] : R[i];
            if (a > peak)
                peak = a;
            if (b > peak)
                peak = b;
        }
        RI_ASSERT(peak > 1e-6, "the bench patch must produce signal (peak %g)", peak);
        printf("\n  signal check: 4 voices give peak %g\n", peak);
    }

    RI_RESULT("levi_bench");
    return 0;
}
