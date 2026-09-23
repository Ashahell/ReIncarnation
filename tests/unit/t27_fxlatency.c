/* t27_fxlatency — M2.4 GUI tail, TC-2.11.1 (FX knob audible
 * within 1 buffer):
 *
 * RiFXSetParam stores immediately (no smoothing in the setters), so
 * a DIST_DRIVE 0 → 127 edit mid-stream must be audible in the very
 * next 64-sample buffer (≤2 ms @48 kHz/64): the post-edit buffer's
 * RMS differs from a continued drive-0 render of the same input by
 * a clearly audible margin, and differs sample-wise (not a
 * metadata-only write).
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "engine/fx/fx.h"

#define T27_SR 48000.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static float IN[64], PRE[64], POST[64], DRY[64];

static float rms(const float *b, uint32_t n) {
    double s = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)b[i] * b[i];
    return (float)sqrt(s / (double)n);
}

int main(void) {
    struct RIFX *x = RiFXCreate(RI_FX_DIST);
    uint32_t i, ndiff = 0;
    double move;
    for (i = 0; i < 64; i++)
        IN[i] = (float)(0.9 * sin(2.0 * 3.141592653589793 * 440.0 *
            (double)i / 48000.0));
    CHECK(x != 0, "dist create");
    if (!x) {
        printf("FAIL %d\n", fails);
        return 1;
    }
    RiFXSetParam(x, RI_FXID_DIST_DRIVE, 0);
    RiFXSetParam(x, RI_FXID_DIST_SHAPE, 0);
    RiFXRender(x, IN, PRE, 64u, T27_SR, 140.0f);
    /* the edit, mid-stream */
    RiFXSetParam(x, RI_FXID_DIST_DRIVE, 127);
    RiFXRender(x, IN, POST, 64u, T27_SR, 140.0f);
    /* continued drive-0 reference on the same input */
    RiFXSetParam(x, RI_FXID_DIST_DRIVE, 0);
    RiFXRender(x, IN, DRY, 64u, T27_SR, 140.0f);
    move = fabs((double)rms(POST, 64) - (double)rms(DRY, 64));
    /* 0.01 mirrors t1_fx's engaged-differs bound; the latency proof
     * is positional (buffer N+1 already differs), magnitude just
     * needs to be clearly real, not a corner. */
    CHECK(move > 0.01, "edit inaudible in 1 buffer (rms move %.5g)", move);
    for (i = 0; i < 64; i++) {
        if (POST[i] != DRY[i])
            ndiff++;
    }
    CHECK(ndiff > 32u, "only %u/64 samples differ", ndiff);
    printf("fxlatency: rms move %.5g, %u/64 samples differ\n", move, ndiff);

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t27_fxlatency\n");
    return fails != 0;
}
