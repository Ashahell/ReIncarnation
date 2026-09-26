/* t95_riapp_core — portability T8: portable application core.
 * Init/demo/transport/meters at the core API: bank defaults, demo content
 * (303 line + 808 hits), play renders audibly through the live session,
 * control-plane send drains, meters land after render, stop silences.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/riapp_core.h"
#include "engine/engine.h"
#include "engine/dsp/rb303.h"

#define N 512u

int main(void) {
    static struct RIAppCore c;
    struct RIPatternBank *b;
    uint32_t i, hits = 0u;
    int l303, l808;
    uint64_t six = 0u;
    static float fl[N], fr[N];
    struct RIEvent ev[64];
    uint32_t seq = 0u, nd;
    ri_core_init(&c, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808);
    ri_core_demo(&c);
    /* Bank defaults + demo content. */
    for (i = 0u; i < 32u; i++)
        RI_ASSERT(c.banks[0].pat[i].length == 16u, "len %u", i);
    b = ri_core_bank(&c, 0u);
    RI_ASSERT(b == &c.banks[0], "bank0");
    RI_ASSERT(ri_core_bank(&c, 9u) == &c.banks[0], "bank clamp");
    RI_ASSERT(ri_core_bank(0, 0u) == 0, "bank null");
    RI_ASSERT(ri_core_bank_ro(&c, 2u) == &c.banks[2], "bank ro");
    for (i = 0u; i < 16u; i++)
        if (c.banks[2].pat[0].row.drum[i].on)
            hits++;
    RI_ASSERT(hits >= 8u, "drum hits %u", hits);
    RI_ASSERT(c.banks[0].pat[0].row.r303[0].key == 0u, "step0 key");
    /* Transport + render through the owned session. */
    ri_core_play(&c);
    {
        uint32_t got = ri_live_render(&c.session, fl, fr, N);
        float peak = 0.0f;
        RI_ASSERT(got == N, "render n");
        for (i = 0u; i < N; i++) {
            float a = fl[i] < 0 ? -fl[i] : fl[i];
            float d = fr[i] < 0 ? -fr[i] : fr[i];
            if (a > peak)
                peak = a;
            if (d > peak)
                peak = d;
        }
        RI_ASSERT(peak > 0.001f, "audible %f", peak);
    }
    /* Control plane at the core level. */
    RI_ASSERT(ri_ctl_send(&c.ctl, RI_CTL_303A_CUTOFF, 20u) == 0, "ctl send");
    nd = ri_ctl_drain(&c.ctl, ev, 64u, 0u, &seq);
    RI_ASSERT(nd == 1u && ev[0].value == RI_CTL_303A_CUTOFF, "ctl drain");
    /* Meters land after render. */
    RI_ASSERT(ri_core_meters(&c, &l303, &l808, &six, 48000u, 1) == 1, "meters");
    RI_ASSERT(l303 >= 0 && l808 >= 0, "levels %d %d", l303, l808);
    RI_ASSERT(ri_core_meters(0, &l303, &l808, &six, 48000u, 1) == 0, "meters null");
    ri_core_stop(&c);
    {
        uint32_t k, silent = 1u;
        ri_live_render(&c.session, fl, fr, N);
        for (k = 0u; k < N; k++) {
            if (fl[k] != 0.0f || fr[k] != 0.0f) {
                silent = 0u;
                break;
            }
        }
        RI_ASSERT(silent, "stop silent");
    }
    RI_RESULT("riapp_core");
}
