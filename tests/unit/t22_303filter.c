/* t22_303filter — TC-2.2.1 filter response (WBS 2.2 property pin).
 * Resonance knob min (reso_k must read 0.0), cutoff 1000 Hz direct:
 * 20 log-spaced sines C0..C6 at 0.1 peak through rb303_filter_step;
 * steady-state RMS (last 0.5 s of 2 s) vs an INDEPENDENT float64
 * reference implementing the documented Appendix B linear form
 * (tan g, feedback HPF, DRIVE, 2x interp substeps; tanh skipped as
 * identity — valid at 0.1 amplitude to 0.3%). Assert within +-1 dB
 * per the TC. Reference written from the code comments/ledger, never
 * by copying voice code; a shared misunderstanding would survive, but
 * any coding deviation (wrong g/topology/knob/oversampling) fails.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "engine/dsp/rb303.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define T22F_SR 48000.0
#define T22F_FC 1000.0
#define T22F_AMP 0.1
#define T22F_NFREQ 20
#define T22F_F0 16.351597831287414  /* C0 */
#define T22F_F1 1046.5022310668792  /* C6 */
#define T22F_RUN 96000u
#define T22F_MEAS 24000u

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

/* Independent linearized reference (float64, from documentation). */
static double ref_rms(double freq) {
    double g = tan(M_PI * T22F_FC / T22F_SR);
    double ghp = tan(M_PI * 150.0 / T22F_SR);
    double f0 = 0.0, f1 = 0.0, f2 = 0.0, fh = 0.0, prev = 0.0;
    double sum = 0.0;
    uint32_t i;
    for (i = 0u; i < T22F_RUN; i++) {
        double in = T22F_AMP * sin(2.0 * M_PI * freq * (double)i / T22F_SR);
        double mid = (prev + in) * 0.5;
        double xj[2] = { mid, in };
        int j;
        for (j = 0; j < 2; j++) {
            double tap = 0.0; /* k = 0 at reso min */
            double fb, x;
            fh += ghp * (tap - fh);
            fb = tap - fh;
            x = 1.0 * (xj[j] - fb); /* DRIVE = 1.0, tanh -> identity */
            f0 += g * (x - f0);
            f1 += g * (f0 - f1);
            f2 += g * (f1 - f2);
        }
        prev = in;
        if (i + T22F_MEAS >= T22F_RUN)
            sum += f2 * f2;
    }
    return sqrt(sum / (double)T22F_MEAS);
}

static double meas_rms(struct RB303Voice *v, double freq) {
    double sum = 0.0;
    uint32_t i;
    for (i = 0u; i < T22F_RUN; i++) {
        float in = (float)(T22F_AMP *
            sin(2.0 * M_PI * freq * (double)i / T22F_SR));
        float y = rb303_filter_step(v, in, (float)T22F_SR);
        if (i + T22F_MEAS >= T22F_RUN)
            sum += (double)y * (double)y;
    }
    return sqrt(sum / (double)T22F_MEAS);
}

int main(void) {
    struct RB303Voice v;
    int f;
    rb303_init(&v);
    rb303_set_param(&v, RI_CTL_303A_RESO, 0);
    CHECK(v.reso_k == 0.0f, "reso knob min k=%f", (double)v.reso_k);
    rb303_set_cutoff_hz(&v, (float)T22F_FC);
    for (f = 0; f < T22F_NFREQ; f++) {
        double freq = T22F_F0 *
            pow(2.0, (double)f / (double)(T22F_NFREQ - 1) * 6.0);
        double ref, got, db;
        rb303_init(&v);
        rb303_set_param(&v, RI_CTL_303A_RESO, 0);
        rb303_set_cutoff_hz(&v, (float)T22F_FC);
        ref = ref_rms(freq);
        got = meas_rms(&v, freq);
        if (!(ref > 0.0)) {
            printf("FAIL f=%d: ref degenerates\n", f);
            fails++;
            continue;
        }
        db = 20.0 * log10(got / ref);
        if (!(db >= -1.0 && db <= 1.0)) {
            printf("FAIL f=%d (%.2f Hz): %+0.3f dB (got %.6g ref %.6g)\n",
                   f, freq, db, got, ref);
            fails++;
        }
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t22_303filter\n");
    return fails != 0;
}
