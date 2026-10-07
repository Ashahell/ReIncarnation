/* drum_bench — HOST timing harness for the 909/808 section renders
 * (drum-tail A0).
 *
 * Every swept quantity carries a positive control: the vc_voice_active
 * counter (t175) proving the swept voices were actually sounding. A bench
 * that measures an early-out reports zero work and looks like a result —
 * unbound 909 voices render exact silence through an early return, so every
 * 909 row binds a real layer first and every row prints CONTROL-OK/FAIL.
 *
 * Sections:
 *   1. 909 held voices k = 0..11 (marginal step = one voice-sample).
 *   2. 909 each voice on its own (a single expensive voice shows by name).
 *   3. 909 flam on/off: explicit arm (pure second-playhead price) and the
 *      accent-2 compat path (flam + accent shelf) vs accent 1 (shelf only).
 *   4. 808 held voices k = 0..11 in distinct-slot order (slot walk priced).
 *   5. 808 each sound on its own.
 *
 * Timings are median-of-7 with min/max, all rows in ONE process: this host
 * is bimodal by ~25 %, so only within-process ratios are verdicts.
 *
 * Ungated by design (asserts no machine-dependent bound); the only assert
 * is the non-silence signal check.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/rb808.h"

#define BLK 64u
#define BLOCKS 400u
#define REPS 7u
#define SETTLE 32u

static double now_s(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static struct RB909Set P9;
static struct RB808Set P8;
static float OB[BLK];

/* One long shared 909 layer (sine; voices must outlive every rep). */
static float LAY[65536];
static const struct RISampleLayer LAY1[1] = { { LAY, 65536u, 48000u, 0,
    127, { 0, 0 } } };

static void bake(void) {
    uint32_t i;
    for (i = 0u; i < 65536u; i++)
        LAY[i] = 0.5f;
}

static void bind9(void) {
    uint32_t v;
    for (v = 0u; v < RI_909_NVOICES; v++)
        rb909_set_layers(&P9, v, LAY1, 1);
}

/* Distinct-slot order for the 808 held-rows (slots 0,1,2,3,4,5,6,8,7,10,9). */
static const uint32_t ORD808[11] = { 0u, 1u, 2u, 3u, 4u, 8u, 10u, 13u, 14u,
    11u, 12u };

static void decay8(void) {
    uint32_t v;
    for (v = 0u; v < RI_808_NSOUNDS; v++)
        rb808_set_decay(&P8, v, 4.0f);
}

typedef void (*setup_fn)(void);

/* One timed run: setup, settle, then time BLOCKS blocks. Re-triggering per
 * rep keeps every rep inside the layer/decay lifetime. */
static double run9ns(setup_fn setup) {
    double t0, t1;
    uint32_t i;
    rb909_init_set(&P9);
    bind9();
    setup();
    for (i = 0u; i < SETTLE; i++) {
        rb909_voice_counters_reset(&P9);
        rb909_render_mix(&P9, OB, BLK, 48000.0f);
    }
    t0 = now_s();
    for (i = 0u; i < BLOCKS; i++) {
        rb909_voice_counters_reset(&P9);
        rb909_render_mix(&P9, OB, BLK, 48000.0f);
    }
    t1 = now_s();
    return (t1 - t0) * 1e9 / (double)BLOCKS;
}

static double run8ns(setup_fn setup) {
    double t0, t1;
    uint32_t i;
    rb808_init_set(&P8);
    decay8();
    setup();
    for (i = 0u; i < SETTLE; i++) {
        rb808_voice_counters_reset(&P8);
        rb808_render_mix(&P8, OB, BLK, 48000.0f);
    }
    t0 = now_s();
    for (i = 0u; i < BLOCKS; i++) {
        rb808_voice_counters_reset(&P8);
        rb808_render_mix(&P8, OB, BLK, 48000.0f);
    }
    t1 = now_s();
    return (t1 - t0) * 1e9 / (double)BLOCKS;
}

static int cmpd(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : x > y;
}

static double med7_9(setup_fn setup, double *mn, double *mx) {
    double r[REPS];
    uint32_t i;
    for (i = 0u; i < REPS; i++)
        r[i] = run9ns(setup);
    qsort(r, REPS, sizeof r[0], cmpd);
    *mn = r[0];
    *mx = r[REPS - 1u];
    return r[REPS / 2u];
}

static double med7_8(setup_fn setup, double *mn, double *mx) {
    double r[REPS];
    uint32_t i;
    for (i = 0u; i < REPS; i++)
        r[i] = run8ns(setup);
    qsort(r, REPS, sizeof r[0], cmpd);
    *mn = r[0];
    *mx = r[REPS - 1u];
    return r[REPS / 2u];
}

/* Steady-state control: fresh setup, settle past any flam fire, one block. */
static uint32_t control9(void) {
    uint32_t i;
    for (i = 0u; i < SETTLE; i++) {
        rb909_voice_counters_reset(&P9);
        rb909_render_mix(&P9, OB, BLK, 48000.0f);
    }
    rb909_voice_counters_reset(&P9);
    rb909_render_mix(&P9, OB, BLK, 48000.0f);
    return P9.vc_voice_active;
}

static uint32_t control8(void) {
    uint32_t i;
    for (i = 0u; i < SETTLE; i++) {
        rb808_voice_counters_reset(&P8);
        rb808_render_mix(&P8, OB, BLK, 48000.0f);
    }
    rb808_voice_counters_reset(&P8);
    rb808_render_mix(&P8, OB, BLK, 48000.0f);
    return P8.vc_voice_active;
}

static void trow9(const char *label, setup_fn setup, uint32_t want) {
    double mn, mx, md = med7_9(setup, &mn, &mx);
    uint32_t got;
    rb909_init_set(&P9);
    bind9();
    setup();
    got = control9();
    printf("  %-30s %10.1f  (%8.1f..%8.1f)  %7.4f us/vs  active=%5u (want %5u) %s\n",
        label, md, mn, mx, md / 1000.0 / (double)BLK, got, want,
        got == want ? "CONTROL-OK" : "CONTROL-FAIL");
}

static void trow8(const char *label, setup_fn setup, uint32_t want) {
    double mn, mx, md = med7_8(setup, &mn, &mx);
    uint32_t got;
    rb808_init_set(&P8);
    decay8();
    setup();
    got = control8();
    printf("  %-30s %10.1f  (%8.1f..%8.1f)  %7.4f us/vs  active=%5u (want %5u) %s\n",
        label, md, mn, mx, md / 1000.0 / (double)BLK, got, want,
        got == want ? "CONTROL-OK" : "CONTROL-FAIL");
}

static uint32_t g_k;
static uint32_t g_v;
static uint32_t g_flam; /* 0 none, 1 explicit arm, 2 compat accent, 3 shelf */

/* 909 hold order: OH (3) before CH (2). The shared-hat-ROM steal rule
 * (t57) kills CH when OH triggers after it, and a fresh CH (age < 240)
 * leaves a ringing OH alone — so this order keeps all k sounding and the
 * control exact. Triggering 0..k-1 in id order would read k-1 for k >= 4,
 * which is the steal law working, not a counter defect. */
static const uint32_t ORD909[11] = { 0u, 1u, 3u, 2u, 4u, 5u, 6u, 7u, 8u, 9u,
    10u };

static void hold9k(void) {
    uint32_t k;
    for (k = 0u; k < g_k; k++)
        rb909_trigger(&P9, ORD909[k], 0u, 64u, 0);
}

static void hold9one(void) {
    rb909_trigger(&P9, g_v, 0u, 64u, 0);
}

static void hold9flam(void) {
    if (g_flam == 1u) {
        rb909_trigger(&P9, RB909_BD, 0u, 64u, 0);
        rb909_arm_flam(&P9, RB909_BD, 1680u);
    } else if (g_flam == 2u) {
        rb909_trigger(&P9, RB909_BD, 2u, 64u, 1680);
    } else if (g_flam == 3u) {
        rb909_trigger(&P9, RB909_BD, 1u, 64u, 0);
    } else {
        rb909_trigger(&P9, RB909_BD, 0u, 64u, 0);
    }
}

static void hold8k(void) {
    uint32_t k;
    for (k = 0u; k < g_k; k++)
        rb808_trigger(&P8, ORD808[k], 0u, 0.0f);
}

static void hold8one(void) {
    rb808_trigger(&P8, g_v, 0u, 0.0f);
}

static void hold8none(void) {
}

static void hold9none(void) {
}

int main(void) {
    uint32_t k;
    printf("drum section host bench (A0: controls + median-of-7)\n");
    printf("  build: %s\n",
#if defined(__OPTIMIZE__)
        "optimised"
#else
        "UNOPTIMISED"
#endif
    );
    printf("  block: %u samples, %u blocks x %u reps\n\n", (unsigned)BLK,
        (unsigned)BLOCKS, (unsigned)REPS);
    bake();

    printf("=== 1. 909 held voices (marginal step = one voice) ===\n");
    printf("  %-30s %10s  %17s  %11s  %22s\n", "row", "ns/block",
        "(min..max)", "us/vs", "control");
    for (k = 0u; k <= RI_909_NVOICES; k++) {
        char lb[40];
        g_k = k;
        snprintf(lb, sizeof lb, "909 hold %u", (unsigned)k);
        trow9(lb, k ? hold9k : hold9none, k * 64u);
    }

    printf("\n=== 2. 909 each voice alone ===\n");
    for (k = 0u; k < RI_909_NVOICES; k++) {
        char lb[40];
        g_v = k;
        snprintf(lb, sizeof lb, "909 solo %s", rb909_name(k));
        trow9(lb, hold9one, 64u);
    }

    printf("\n=== 3. 909 flam ===\n");
    g_flam = 0u;
    trow9("909 BD no flam", hold9flam, 64u);
    g_flam = 1u;
    trow9("909 BD explicit flam", hold9flam, 128u);
    g_flam = 3u;
    trow9("909 BD accent1 (shelf)", hold9flam, 64u);
    g_flam = 2u;
    trow9("909 BD accent2 (shelf+flam)", hold9flam, 128u);

    printf("\n=== 4. 808 held voices, distinct slots ===\n");
    for (k = 0u; k <= 11u; k++) {
        char lb[40];
        g_k = k;
        snprintf(lb, sizeof lb, "808 hold %u", (unsigned)k);
        trow8(lb, k ? hold8k : hold8none, k * 64u);
    }

    printf("\n=== 5. 808 each sound alone ===\n");
    for (k = 0u; k < RI_808_NSOUNDS; k++) {
        char lb[40];
        g_v = k;
        snprintf(lb, sizeof lb, "808 solo %s", rb808_name(k));
        trow8(lb, hold8one, 64u);
    }

    {
        double peak = 0.0;
        uint32_t i;
        rb909_init_set(&P9);
        bind9();
        g_k = 4u;
        hold9k();
        memset(OB, 0, sizeof OB);
        rb909_render_mix(&P9, OB, BLK, 48000.0f);
        for (i = 0u; i < BLK; i++) {
            double a = OB[i] < 0 ? -OB[i] : OB[i];
            if (a > peak)
                peak = a;
        }
        RI_ASSERT(peak > 1e-6, "the bench patch must produce signal (%g)",
            peak);
        printf("\n  signal check: 4 909 voices give peak %g\n", peak);
    }

    RI_RESULT("drum_bench");
    return 0;
}
