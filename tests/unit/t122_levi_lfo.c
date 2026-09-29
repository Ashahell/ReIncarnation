/* t122_levi_lfo — LFO core + matrix feed (v2 feature 4d, owner order).
 * 5 LFOs per voice (own laws): rate 0.01..30 Hz exp-mapped, smooth
 * sine or quantized 3-step {-1,0,+1}, trigger-reset phases (no RNG,
 * deterministic). Matrix sources 8..12. Automation-only controls
 * (303-VOLUME precedent: engine params with no panel knob yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"

int main(void) {
    struct RILeviSet s;
    struct RILeviMatrix mx;
    float openv[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float lfo5[5] = { 0.0f, 0.0f, 0.5f, 0.0f, 0.0f };
    float dst[RI_LEVI_MD_N];
    struct RILeviLFO l;
    uint32_t i;
    levi_init_set(&s);
    /* Defaults: mid rate, smooth, parked at zero. */
    RI_ASSERT(s.v[0].lfo[0].phase == 0.0f && s.v[0].lfo[0].value == 0.0f, "parked");
    RI_ASSERT(s.v[0].lfo[0].shape == 0u, "smooth default");
    /* Fail-closed. */
    RI_ASSERT(ri_levi_lfo_step(0, 48000.0f) == 0.0f, "null lfo");
    RI_ASSERT(ri_levi_lfo_rate(127u) > ri_levi_lfo_rate(0u), "rate order");
    RI_ASSERT(ri_levi_lfo_rate(0u) == 0.01f, "rate floor");
    /* Smooth: starts at 0, rises through the first quarter. */
    RI_ASSERT(levi_set_param_ui(&s, 0u, RI_CTL_LEVI_LFO0RATE & 0xFFu, 127u) == 0, "rate ui");
    l = s.v[0].lfo[0];
    RI_ASSERT(ri_levi_lfo_step(&l, 48000.0f) > 0.0f, "rises");
    for (i = 0u; i < 400u; i++)
        ri_levi_lfo_step(&l, 48000.0f);
    RI_ASSERT(l.value > 0.9f && l.value <= 1.0f, "quarter peak %f", l.value);
    /* Quantized: three levels over the cycle. */
    RI_ASSERT(levi_set_param_ui(&s, 0u, RI_CTL_LEVI_LFO0SHAPE & 0xFFu, 1u) == 0, "shape ui");
    l = s.v[0].lfo[0];
    l.phase = 0.0f;
    RI_ASSERT(ri_levi_lfo_step(&l, 48000.0f) == -1.0f, "step lo");
    l.phase = 0.5f;
    RI_ASSERT(ri_levi_lfo_step(&l, 48000.0f) == 0.0f, "step mid");
    l.phase = 0.9f;
    RI_ASSERT(ri_levi_lfo_step(&l, 48000.0f) == 1.0f, "step hi");
    /* Trigger resets phases (deterministic songs). */
    RI_ASSERT(levi_trigger(&s, 0u, 60u) == 0, "trig");
    for (i = 0u; i < 5u; i++)
        RI_ASSERT(s.v[0].lfo[i].phase == 0.0f, "reset %u", i);
    /* Matrix feed: LFO2 (0.5) -> VLEVEL @100% = +0.5. */
    ri_levi_matrix_init(&mx);
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_LFO2, RI_LEVI_MD_VLEVEL, 100) == 0, "slot");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 0, 60u, dst) == 2, "lfo null");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, lfo5, 60u, dst) == 0, "feed");
    RI_ASSERT(dst[RI_LEVI_MD_VLEVEL] == 0.5f, "lfo offset %f", dst[RI_LEVI_MD_VLEVEL]);
    RI_RESULT("levilfo");
}
