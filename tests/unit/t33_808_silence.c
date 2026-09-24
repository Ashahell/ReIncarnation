/* t33_808_silence.c — long-silence regression (§2.1 fix proof).
 * Trigger each 808 voice ONCE, render 30 s, assert the per-100 ms window
 * RMS decays monotonically (past transients) and ends below -120 dBFS.
 * RED-first: the ri_exp overflow ramps RS/CL/CH/OH to full-scale noise.
 * Rate 8 kHz keeps the run cheap; decay arguments are t/tau (rate-free).
 */
#include <stdio.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb808.h"

#define SR 8000.0f
#define SECS 3
#define WIN ((uint32_t)(SR * 0.1f)) /* 100 ms windows */
#define NWIN (SECS * 10)

int main(void) {
    uint32_t v, w, i;
    for (v = 0; v < RI_808_NSOUNDS; v++) {
        struct RB808Set s;
        float rms[NWIN];
        rb808_init_set(&s);
        /* Short test tau: the defect argument is t/tau (blowup past -25,
         * silence past -13.8), so 3 s at tau 0.05 covers the same ground
         * as 30 s at default taus, 10x cheaper. CP ignores tau (own tail). */
        rb808_set_decay(&s, v, 0.05f);
        rb808_trigger(&s, v, 0u, 0.0f);
        for (w = 0; w < NWIN; w++) {
            double acc = 0.0;
            for (i = 0; i < WIN; i++) {
                float y = rb808_voice_render(&s.v[v], SR);
                RI_ASSERT(y == y, "NaN voice %u win %u", v, w);
                acc += (double)y * (double)y;
            }
            rms[w] = (float)sqrt(acc / (double)WIN);
        }
        /* monotone decay past transients (bursts/clicks done by 200 ms). */
        for (w = 2; w + 1 < NWIN; w++)
            RI_ASSERT(rms[w + 1] <= rms[w] * 1.05f, "voice %s (%u) rises win %u: %g -> %g",
                rb808_name(v), v, w, rms[w], rms[w + 1]);
        /* silent floor: -120 dBFS = 1e-6 peak-ish; RMS bound is stricter. */
        RI_ASSERT(rms[NWIN - 1] <= 1e-6f, "voice %s (%u) tail %g", rb808_name(v), v, rms[NWIN - 1]);
    }
    RI_RESULT("808_silence");
}
