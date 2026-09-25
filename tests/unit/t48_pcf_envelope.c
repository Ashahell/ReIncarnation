/* t48_pcf_envelope.c — PCF attack/decay envelope + integer clock (§12.8c1).
 * (a) Decay knob: fast decay collapses env between hits, slow sustains
 *     (white-box env state + output audibility).
 * (b) Integer clock: step index is a pure function of samples rendered
 *     (exact at 1e9 samples ≈ 5.8 h — the old float accumulator stalls
 *     near 7 min); pcf_restart resets to step 0 (new API).
 * (c) HP mode dropped (maps to band — not in ReBirth).
 * (d) Retrigger: neutral hits every step (env re-arms at each boundary).
 * RED-first: stepped cutoff, float beat_pos, no restart, HP mode.
 */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/pcf.h"

#define SR 48000.0f

static float IN[48000u];
static float OUT[48000u];

static void noise_in(void) {
    uint32_t i, s = 0x12345678u;
    for (i = 0; i < 48000u; i++) {
        s = s * 1664525u + 1013904223u;
        IN[i] = (float)(s >> 8) / 16777216.0f - 0.5f;
    }
}

static int rms_gt(const float *b, uint32_t n, float floor) {
    double acc = 0.0;
    uint32_t i;
    for (i = 0; i < n; i++)
        acc += (double)b[i] * (double)b[i];
    return sqrt(acc / (double)(n ? n : 1)) > (double)floor;
}

int main(void) {
    struct PCF p;
    uint32_t i;
    noise_in();
    /* (a) decay audibility (Amt max so the envelope swings the cutoff). */
    pcf_init(&p);
    p.base_fc = 800.0f;
    p.q = 2.0f;
    p.amt_oct = 4.0f;
    p.mode = 0;
    pcf_set_decay(&p, 0);
    pcf_set_tempo(&p, 140.0f);
    pcf_render(&p, IN, OUT, 48000u, SR);
    RI_ASSERT(p.env < 5.0f, "fast decay never collapses: %g", p.env);
    RI_ASSERT(rms_gt(OUT, 48000u, 0.001f), "render silent");
    pcf_init(&p);
    p.base_fc = 800.0f;
    p.q = 2.0f;
    p.amt_oct = 4.0f;
    p.mode = 0;
    pcf_set_decay(&p, 127);
    pcf_set_tempo(&p, 140.0f);
    pcf_render(&p, IN, OUT, 48000u, SR);
    RI_ASSERT(p.env > 40.0f, "slow decay never sustains: %g", p.env);
    /* (b) integer clock: exact at long horizon + restart determinism. */
    RI_ASSERT(pcf_step_index(1000000000ull, 140.0f, SR) == 194444u,
        "long-horizon step wrong");
    RI_ASSERT(pcf_step_index(0ull, 140.0f, SR) == 0u, "step 0 wrong");
    {
        static float r1[4800u], r2[4800u];
        struct PCF a, b;
        pcf_init(&a);
        pcf_set_tempo(&a, 140.0f);
        pcf_render(&a, IN, r1, 4800u, SR);
        pcf_restart(&a);
        pcf_render(&a, IN, r2, 4800u, SR);
        pcf_init(&b);
        pcf_set_tempo(&b, 140.0f);
        for (i = 0; i < 4800u; i++)
            RI_ASSERT(r1[i] == r2[i], "restart not deterministic at %u", i);
    }
    /* (c) HP mode == band mode now. */
    {
        static float ob[4800u], oh[4800u];
        struct PCF a, b;
        pcf_init(&a);
        a.mode = 2;
        pcf_render(&a, IN, ob, 4800u, SR);
        pcf_init(&b);
        b.mode = 1;
        pcf_render(&b, IN, oh, 4800u, SR);
        for (i = 0; i < 4800u; i++)
            RI_ASSERT(ob[i] == oh[i], "HP != band at %u", i);
    }
    /* (d) neutral retrigger: env re-arms at every 16th boundary. */
    {
        struct PCF r;
        float e0;
        pcf_init(&r);
        pcf_set_decay(&r, 0);
        pcf_set_tempo(&r, 140.0f);
        pcf_render(&r, IN, OUT, 4800u, SR);
        e0 = r.env;
        pcf_render(&r, IN, OUT, 4800u, SR);
        RI_ASSERT(r.env >= e0, "no retrigger across renders: %g -> %g", e0, r.env);
    }
    RI_RESULT("pcf_envelope");
}
