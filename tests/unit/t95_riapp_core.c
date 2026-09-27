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
    /* Sticky selection: a captured slot persists across bars (live pattern
     * mode). One-shot grid writes can't — the playhead leaves the bar. */
    RI_ASSERT(ri_track_capture(&c.track, 0u, 0u, 5u) == 0, "seed");
    RI_ASSERT(ri_core_capture_sel(&c, 0u, 0u, 5u) == 0, "already stored");
    RI_ASSERT(ri_core_capture_sel(&c, 1u, 0u, 5u) == 1, "re-capture next bar");
    RI_ASSERT(ri_track_selected(&c.track, 1u, 0u) == 5u, "sticky");
    RI_ASSERT(ri_core_capture_sel(0, 0u, 0u, 5u) == 0, "sel null");
    RI_ASSERT(ri_core_capture_sel(&c, 0u, 9u, 5u) == 0, "sel bad inst");
    /* Delay line wired (owner 2026-09-27: sends were dry everywhere). */
    RI_ASSERT(c.session.eng.dline != 0, "dline attached");
    /* Delay clock ownership (owner 2026-09-27: echoes ran at the 140 BPM
     * default against a 120 groove): session tempo reaches the engine. */
    {
        static float ql[256], qr[256];
        ri_live_render(&c.session, ql, qr, 256u);
        RI_ASSERT(c.session.eng.tempo == c.session.bpm, "tempo owned %f",
            c.session.eng.tempo);
    }
    {
        /* Send at full on 808 vs dry: the echo must be audible. Prime
         * 2 s first so the line holds signal, then compare 1 s. */
        static struct RIAppCore d, e;
        static float dl[144256], dr[144256], el[144256], er[144256];
        uint32_t n = 0u;
        ri_core_init(&d, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808);
        ri_core_demo(&d);
        ri_core_init(&e, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A | RI_ENGINE_S808);
        ri_core_demo(&e);
        ri_engine_set_send(&e.session.eng, 2u, 127u);
        ri_core_play(&d);
        ri_core_play(&e);
        while (n < 144000u) {
            uint32_t want = 144000u - n;
            uint32_t g, h;
            if (want > 256u)
                want = 256u;
            g = ri_live_render(&d.session, dl + n, dr + n, want);
            h = ri_live_render(&e.session, el + n, er + n, want);
            if (!g || !h || g != h)
                break;
            n += g;
        }
        RI_ASSERT(n == 144000u, "delay render %u", n);
        RI_ASSERT(!memcmp(dl, el, 9600u * sizeof(float)), "pre-echo same");
        RI_ASSERT(memcmp(dl + 48000u, el + 48000u, 48000u * sizeof(float)) != 0,
            "delay return audible");
    }
    RI_RESULT("riapp_core");
}
