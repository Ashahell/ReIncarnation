/* t25_swap — Module 2.5, TC-2.5.5 (FX order swap without zipper):
 *
 * Mid-stream dist<->pcf reorder on shared instances vs the ideal
 * chain that ran the new order all along: transient energy over the
 * post-swap segment bounded by one full-scale 64-frame buffer
 * (energy <= 64.0). (Mirrors t1_fx §6 value: measured 0.81 there;
 * this pin re-establishes the bound as the TC-2.5.5 contract.)
 *
 * Green pin on frozen code expected.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/fx/pcf.h"
#include "engine/fx/fx.h"

#define T25_SR 48000.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float IN[8192], OA[8192], OB[8192];

int main(void) {
    struct PCF p, q;
    struct RiFXDist d;
    double energy = 0.0;
    uint32_t i, k;
    for (i = 0; i < 8192u; i++)
        IN[i] = (float)(0.5 * sin(2.0 * 3.141592653589793 * 440.0 *
            (double)i / 48000.0));

    /* swap path: A = dist->pcf for seg1, then B = pcf->dist, shared */
    pcf_init(&p);
    p.base_fc = 2000.0f;
    p.q = 2.0f;
    ri_fxdist_init(&d);
    ri_fxdist_set(&d, 64, 32);
    ri_fxdist_render(&d, IN, OA, 4096u);
    pcf_render(&p, OA, OA, 4096u, T25_SR);
    pcf_render(&p, IN + 4096u, OA + 4096u, 4096u, T25_SR);
    ri_fxdist_render(&d, OA + 4096u, OA + 4096u, 4096u);
    /* ideal path: fresh B for both segments (seg1 discarded) */
    pcf_init(&q);
    q.base_fc = 2000.0f;
    q.q = 2.0f;
    pcf_render(&q, IN, OB, 4096u, T25_SR);
    pcf_render(&q, IN + 4096u, OB + 4096u, 4096u, T25_SR);
    ri_fxdist_render(&d, OB + 4096u, OB + 4096u, 4096u);
    for (k = 4096u; k < 8192u; k++) {
        double dd = (double)OA[k] - (double)OB[k];
        energy += dd * dd;
    }
    printf("order swap: edge energy %.4g (bound 64)\n", energy);
    CHECK(energy <= 64.0, "zipper energy %.4g > 64", energy);

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t25_swap\n");
    return fails != 0;
}
