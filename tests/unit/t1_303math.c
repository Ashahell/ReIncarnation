/* t1_303math — Task 4 Step 2 (failing-first math tests for the ledger candidate).
 * Gates ONLY determinism + DC convergence (candidate UNVALIDATED — see
 * docs/evidence/303/filter-candidate.md).
 *
 * Measurement note (brief Step 2 confirmed, mechanism recorded): DC 0.5 in,
 * fc=1000 Hz, k=0 converges to 0.5 within 1e-4 — NOT to tanh(0.5). The
 * ladder inverts the input saturation at DC: each stage settles where
 * tanh(s) equals its drive, so s2 settles at atanh(tanh(0.5)) = 0.5.
 * Measured 2026-09-20: mean of last 1000 = 0.49999827.
 *
 * Sine amplitude 0.1 is an executor choice (brief fixes f/fc/k only):
 * small-signal passband passthrough; THD is printed as a lagging indicator,
 * NOT gated.
 */
#include <stdio.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f
#define N 4800

static float buf[N];

int main(void) {
    struct RB303Voice v;
    int i;
    double sum, mean, var, mn, mx, expected;
    /* --- DC convergence: 0.5 in, fc=1000 Hz, k=0 --- */
    rb303_init(&v);
    rb303_set_cutoff_hz(&v, 1000.0f);
    rb303_set_reso(&v, 0.0f);
    for (i = 0; i < N; i++)
        buf[i] = rb303_filter_step(&v, 0.5f, SR);
    sum = 0.0;
    mn = 1e30;
    mx = -1e30;
    for (i = N - 1000; i < N; i++) {
        sum += buf[i];
        if (buf[i] < mn)
            mn = buf[i];
        if (buf[i] > mx)
            mx = buf[i];
    }
    mean = sum / 1000.0;
    expected = 0.5; /* ladder inverts input tanh at DC: atanh(tanh(0.5)) */
    RI_ASSERT(fabs(mean - expected) <= 1e-4, "dc mean %.9g vs %.9g", mean, expected);
    RI_ASSERT((mx - mn) < 1e-6, "dc not converged: window spread %.9g", mx - mn);
    for (i = 0; i < N; i++)
        RI_ASSERT(buf[i] == buf[i] && buf[i] > -2.0f && buf[i] < 2.0f, "dc non-finite/range at %d", i);

    /* --- Sine passthrough: 1 kHz amp 0.1, fc=8 kHz, k=0 --- */
    {
        double rms_in = 0.0, rms_out = 0.0, a1 = 0.0, b1 = 0.0, fund, res, thd;
        rb303_init(&v);
        rb303_set_cutoff_hz(&v, 8000.0f);
        rb303_set_reso(&v, 0.0f);
        for (i = 0; i < N; i++) {
            float in = 0.1f * sinf(2.0f * 3.14159265f * 1000.0f * (float)i / SR);
            buf[i] = rb303_filter_step(&v, in, SR);
        }
        for (i = N - 2400; i < N; i++) {
            float in = 0.1f * sinf(2.0f * 3.14159265f * 1000.0f * (float)i / SR);
            float ph = 2.0f * 3.14159265f * 1000.0f * (float)i / SR;
            rms_in += (double)in * in;
            rms_out += (double)buf[i] * buf[i];
            a1 += (double)buf[i] * sinf(ph);
            b1 += (double)buf[i] * cosf(ph);
        }
        rms_in = sqrt(rms_in / 2400.0);
        rms_out = sqrt(rms_out / 2400.0);
        {
            double db = 20.0 * log10(rms_out / rms_in);
            RI_ASSERT(fabs(db) <= 0.5, "sine rms %.9g vs %.9g (%+.3f dB)", rms_out, rms_in, db);
        }
        /* THD via least-squares fundamental fit over integer cycles (2400 = 50 cycles): lagging indicator only. */
        a1 = a1 / 1200.0;
        b1 = b1 / 1200.0;
        fund = sqrt(a1 * a1 + b1 * b1) / 1.41421356;
        res = 0.0;
        for (i = N - 2400; i < N; i++) {
            float ph = 2.0f * 3.14159265f * 1000.0f * (float)i / SR;
            double f = a1 * sinf(ph) + b1 * cosf(ph);
            double d = (double)buf[i] - f;
            res += d * d;
        }
        res = sqrt(res / 2400.0);
        thd = (fund > 0.0) ? res / fund : 0.0;
        printf("INFO sine THD %.6g (recorded, not gated)\n", thd);
        for (i = 0; i < N; i++)
            RI_ASSERT(buf[i] == buf[i] && buf[i] > -2.0f && buf[i] < 2.0f, "sine non-finite/range at %d", i);
    }
    (void)var;
    RI_RESULT("303math");
}
