/* t123_reverb — reverb DSP core (v2 feature 5a, owner order).
 * Own comb+allpass network (own lengths/gains, clean-room): wet-only
 * output; decay 0 is exact silence; impulse tails decay; two instances
 * run bit-identically; 10 s of noise stays finite; fail-closed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/reverb.h"

#define SR 48000u
/* Worst-case line need: longest comb @48 kHz + margin. */
#define CAP 2048u
static float L1[CAP], L2[CAP], L3[CAP], L4[CAP], A1[CAP], A2[CAP];

static void lines(struct RIReverb *r) {
    ri_reverb_lines(r, L1, L2, L3, L4, A1, A2, CAP);
}

static float energy_tail(struct RIReverb *r, uint32_t n) {
    uint32_t i;
    float e = 0.0f;
    for (i = 0u; i < n; i++) {
        float o = ri_reverb_render(r, 0.0f);
        e += o < 0.0f ? -o : o;
    }
    return e;
}

int main(void) {
    struct RIReverb a, b;
    float o1, o2;
    uint32_t i;
    /* Fail-closed. */
    RI_ASSERT(ri_reverb_init(0, SR) == 2, "null init");
    RI_ASSERT(ri_reverb_init(&a, 0u) == 2, "bad sr");
    ri_reverb_init(&a, SR);
    RI_ASSERT(ri_reverb_lines(0, L1, L2, L3, L4, A1, A2, CAP) == 2, "lines null");
    RI_ASSERT(ri_reverb_lines(&a, 0, L2, L3, L4, A1, A2, CAP) == 2, "lines null buf");
    RI_ASSERT(ri_reverb_lines(&a, L1, L2, L3, L4, A1, A2, 16u) == 2, "lines small cap");
    RI_ASSERT(ri_reverb_set_decay(0, 64u) == 2, "decay null");
    lines(&a);
    /* Decay 0 = exact silence (bypass law), even past the longest
     * tap (cold lines alone would pass delayed copies after ~1400
     * samples — the gate, not empty buffers, guarantees this). */
    RI_ASSERT(ri_reverb_set_decay(&a, 0u) == 0, "decay 0");
    for (i = 0u; i < 1600u; i++)
        RI_ASSERT(ri_reverb_render(&a, 1.0f) == 0.0f, "silent %u", i);
    /* Impulse rings then dies (decay 100). */
    RI_ASSERT(ri_reverb_set_decay(&a, 100u) == 0, "decay 100");
    o1 = ri_reverb_render(&a, 1.0f);
    (void)o1;
    {
        float e1 = energy_tail(&a, 4000u);
        float e2 = energy_tail(&a, 4000u);
        float e3 = energy_tail(&a, 4000u);
        RI_ASSERT(e1 > e2 && e2 > e3, "tail decays %f %f %f", e1, e2, e3);
    }
    /* Determinism: twin instance, same knobs, same stream. */
    ri_reverb_init(&b, SR);
    lines(&b);
    RI_ASSERT(ri_reverb_set_decay(&b, 100u) == 0, "decay b");
    ri_reverb_init(&a, SR);
    lines(&a);
    RI_ASSERT(ri_reverb_set_decay(&a, 100u) == 0, "decay a2");
    for (i = 0u; i < 512u; i++) {
        float in = (i & 63u) == 0u ? 0.5f : 0.0f;
        o1 = ri_reverb_render(&a, in);
        o2 = ri_reverb_render(&b, in);
        RI_ASSERT(o1 == o2, "twin %u", i);
    }
    /* 10 s hotspot noise stays finite (latch discipline). */
    {
        uint32_t s = 0x12345678u;
        for (i = 0u; i < 480000u; i++) {
            float in;
            s = s * 1664525u + 1013904223u;
            in = (float)((int32_t)(s >> 8)) / 8388608.0f;
            o1 = ri_reverb_render(&a, in);
            RI_ASSERT(o1 > -1e20f && o1 < 1e20f, "finite %u", i);
        }
    }
    /* Reset returns to silence. */
    ri_reverb_reset(&a);
    for (i = 0u; i < 64u; i++)
        RI_ASSERT(ri_reverb_render(&a, 0.0f) == 0.0f, "reset %u", i);
    ri_reverb_reset(0);
    RI_RESULT("reverb");
}
