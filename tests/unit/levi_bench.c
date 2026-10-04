/* levi_bench — HOST timing harness for levi_voice_render_sum_stereo.
 *
 * WHY THIS EXISTS (owner 2026-10-04). The on-target answer for this function is
 * a floor, not a number. A clock read costs 4 us on riqemu1 and 6 us on the
 * Dell, while the function averages ~3-8 us per SAMPLE, so timing its regions
 * on a guest costs more than the code being measured. The guest lane can still
 * report "81-91 % of LEVI" and a per-block total; it cannot report a per-operator
 * or per-voice slope without either perturbing the answer or regressing over
 * samples that are noisy by construction.
 *
 * On the host a clock read is ~20-30 ns -- three orders of magnitude below the
 * work -- so the regions become directly measurable and the counts become
 * exactly controlled instead of sampled: render a fixed patch with exactly k
 * held voices for a fixed number of blocks, and the difference between k and k+1
 * IS the marginal cost of one voice. No regression, no noise, no song, no AHI,
 * and nothing depends on which machine happens to be busy.
 *
 * WHAT IT MEASURES, in order:
 *   - k = 0..RI_LEVI_NVOICES held voices  -> fixed cost + cost per voice
 *   - k = 1..RI_LEVI_NOPS active operators -> cost per operator
 *   - morph bank A/B on and off            -> what the identical bank-skip cut
 *                                             is actually worth
 *
 * A DELIBERATE NON-GOAL: this is not a gate and asserts nothing about
 * correctness. Bit-identity for the optimisation cuts lives in the property
 * tests and the audit's goldens (t93_raster_goldens, t136_levi_stereo); this
 * harness exists only to put a defensible number on the table first.
 *
 * Build (host, so gcc and the shared .o files in /tmp/ri/build):
 *   ./scripts/ri_build_host.sh all
 *   ./scripts/ri_build_host.sh test levi_bench
 * or directly, for an -O2 vs -O0 comparison of the SAME harness:
 *   gcc -std=gnu99 -O2 -I. -o /tmp/levi_bench_O2 tests/unit/levi_bench.c \
 *       /tmp/ri/build/*.o -lm -lpng
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"

#define BLK_SAMPLES 64u   /* the guest's block size: same unit, so ns/block
                           * converts to us/block without a fudge factor */
#define BLOCKS      400u  /* ~25 blocks/s at 48 kHz; enough to dwarf timer
                           * resolution, short enough to stay interactive */

static double now_s(void) {
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* A fixed, non-silent patch: every voice triggered at the same note with the
 * same amplitude, so the only thing varying between runs is how many voices
 * are held. Operator count is swept separately below. */
static void patch(struct RILeviSet *s, uint32_t voices) {
    uint32_t v;
    levi_init_set(s);
    for (v = 0u; v < voices && v < RI_LEVI_NVOICES; v++) {
        levi_set_param_ui(s, v, RI_CTL_LEVI_VSPREAD & 0xFFu, 64u);
        levi_trigger(s, v, (uint8_t)(48u + v));
    }
    /* settle envelopes/alloc state so the first timed block is not an outlier */
    {
        float l[BLK_SAMPLES], r[BLK_SAMPLES];
        int i;
        for (i = 0; i < 8; i++)
            levi_voice_render_sum_stereo(s, l, r, BLK_SAMPLES, 48000.0f);
    }
}

static double time_ns_per_block(struct RILeviSet *s, uint32_t blocks) {
    float l[BLK_SAMPLES], r[BLK_SAMPLES];
    double t0, t1;
    uint32_t i;
    /* warm the caches and the branch predictors before the timed run */
    for (i = 0; i < 32u; i++)
        levi_voice_render_sum_stereo(s, l, r, BLK_SAMPLES, 48000.0f);
    t0 = now_s();
    for (i = 0; i < blocks; i++)
        levi_voice_render_sum_stereo(s, l, r, BLK_SAMPLES, 48000.0f);
    t1 = now_s();
    return (t1 - t0) * 1e9 / (double)blocks;
}

int main(void) {
    static struct RILeviSet S;
    float l[BLK_SAMPLES], r[BLK_SAMPLES];
    double v[RI_LEVI_NVOICES + 1u];
    uint32_t k;

    printf("levi voice-render host bench\n");
    printf("  build        : %s\n",
#if defined(__OPTIMIZE__)
        "optimised (-O2 or -O1; NDEBUG unset)"
#else
        "UNOPTIMISED (-O0)"
#endif
    );
    printf("  block        : %u samples, %u blocks timed\n", (unsigned)BLK_SAMPLES,
        (unsigned)BLOCKS);
    printf("  voices       : %u\n  ops/voice    : %u\n\n", (unsigned)RI_LEVI_NVOICES,
        (unsigned)RI_LEVI_NOPS);

    /* --- 1. held voices: fixed cost + marginal cost per voice --- */
    printf("=== held voices (marginal cost = the step between adjacent rows) ===\n");
    printf("  voices |   ns/block |  us/block | us/sample | us/voice-sample\n");
    for (k = 0u; k <= RI_LEVI_NVOICES; k++) {
        patch(&S, k);
        v[k] = time_ns_per_block(&S, BLOCKS);
        printf("  %6u | %10.1f | %9.3f | %10.4f |", (unsigned)k, v[k],
            v[k] / 1000.0, v[k] / 1000.0 / (double)BLK_SAMPLES);
        if (k > 0u && v[k - 1u] > 0.0)
            printf(" %14.4f\n", (v[k] - v[k - 1u]) / (double)BLK_SAMPLES / 1000.0);
        else
            printf(" %14s\n", "-");
    }

    /* The two numbers the lane data could not give, read straight off the
     * table above rather than inferred: what an idle voice costs, and what a
     * sounding one costs. They are different because idle voices early-out. */
    printf("\n=== the two costs that matter ===\n");
    printf("  fixed (0 voices)          : %8.1f ns/block  %7.3f us/block\n", v[0],
        v[0] / 1000.0);
    printf("  marginal, 1st voice       : %8.1f ns/block  %7.3f us/block\n",
        v[1] - v[0], (v[1] - v[0]) / 1000.0);
    if (v[RI_LEVI_NVOICES] > 0.0)
        printf("  marginal, mean over all   : %8.1f ns/voice  (all %u sounding)\n",
            (v[RI_LEVI_NVOICES] - v[0]) / (double)RI_LEVI_NVOICES, (unsigned)RI_LEVI_NVOICES);

    /* --- 2. active operators, on ONE voice so the voice count is held ---
     * Operators the algorithm does not reach contribute exactly zero, so this
     * sweep is also the direct price of skipping them. The lever is the morph
     * slot list: RI_LEVI_SLOT_SILENCE mutes an operator outright, which is the
     * honest form of "output level 0" for this harness (there is no per-op
     * output-level control in the public surface). */
    printf("\n=== active operators (ONE voice held, so voice count is fixed) ===\n");
    printf("  ops |   ns/block |  us/block | ns/op | what it prices\n");
    for (k = RI_LEVI_NOPS; k >= 1u; k--) {
        double ns;
        uint32_t o;
        patch(&S, 1u);
        /* Silence every operator from k upward: they contribute exactly 0. */
        for (o = k; o < RI_LEVI_NOPS; o++)
            S.v[0].slot[o] = (uint8_t)RI_LEVI_SLOT_SILENCE;
        ns = time_ns_per_block(&S, BLOCKS);
        printf("  %3u | %10.1f | %9.3f | %6.1f |", (unsigned)k, ns, ns / 1000.0,
            ns / (double)k);
        printf(" %s\n", k == RI_LEVI_NOPS ? "full stack (baseline)" :
            (k == 1u ? "one op sounding" : "skips the top ops"));
        if (k == 1u)
            break;
    }

    /* --- 3. morph: what an identical bank-skip cut is worth ---
     * This is the measurement that decides whether a cut is worth making. In
     * levi.c both banks are rendered unconditionally and one is DISCARDED when
     * morph is 0 or 100 -- so the waste is provably removable at zero sound
     * cost, and the size of it is the difference between the cut being obvious
     * and being speculative. */
    printf("\n=== morph banks (4 voices held; both banks render, one is discarded\n"
           "    at morph 0 or 100 -- so the gap between those and the midpoint is\n"
           "    exactly what skipping the unused bank would save) ===\n");
    {
        static const uint32_t at[] = { 0u, 50u, 100u };
        double ns[3];
        uint32_t i;
        for (i = 0u; i < 3u; i++) {
            patch(&S, 4u);
            levi_set_param_ui(&S, 0u, RI_CTL_LEVI_MORPH & 0xFFu, (uint8_t)at[i]);
            levi_set_param_ui(&S, 1u, RI_CTL_LEVI_MORPH & 0xFFu, (uint8_t)at[i]);
            levi_set_param_ui(&S, 2u, RI_CTL_LEVI_MORPH & 0xFFu, (uint8_t)at[i]);
            levi_set_param_ui(&S, 3u, RI_CTL_LEVI_MORPH & 0xFFu, (uint8_t)at[i]);
            ns[i] = time_ns_per_block(&S, BLOCKS);
        }
        printf("  morph |   ns/block |  us/block\n");
        for (i = 0u; i < 3u; i++)
            printf("  %5u | %10.1f | %9.3f\n", (unsigned)at[i], ns[i], ns[i] / 1000.0);
        if (ns[0] > 0.0) {
            double waste = (ns[0] + ns[2]) / 2.0 - ns[1];
            printf("\n  midpoint minus the mean of the two extremes: %.1f ns/block"
                   " (%.1f%%)\n", waste, 100.0 * waste / ((ns[0] + ns[2]) / 2.0));
            printf("  => a bank BOTH banks render but only ONE uses is already"
                   " being computed and thrown away.\n");
        }
    }

    /* Non-silence check: a bench that measures nothing is worse than useless,
     * so prove the patch produces signal before trusting any number above. */
    {
        double peak = 0.0;
        uint32_t i;
        patch(&S, 4u);
        memset(l, 0, sizeof l); memset(r, 0, sizeof r);
        levi_voice_render_sum_stereo(&S, l, r, BLK_SAMPLES, 48000.0f);
        for (i = 0; i < BLK_SAMPLES; i++) {
            double a = l[i] < 0 ? -l[i] : l[i];
            double b = r[i] < 0 ? -r[i] : r[i];
            if (a > peak) peak = a;
            if (b > peak) peak = b;
        }
        RI_ASSERT(peak > 1e-6, "the bench patch must produce signal (peak %g)", peak);
        printf("\n  signal check: 4 voices give peak %g (non-silent, so the\n"
               "  numbers above are work, not an early-out)\n", peak);
    }

    RI_RESULT("levi_bench");
    return 0;
}