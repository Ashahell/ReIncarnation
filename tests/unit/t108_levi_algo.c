/* t108_levi_algo — 8-operator algorithm core (owner 2026-09-28, v2 slice 1).
 * Own topologies (functional shapes, not ASM's chart): presets render,
 * DUO default is the v1 2-op pair (t103 passes unmodified alongside),
 * custom routing validates acyclic, carriers sum, fail-closed edges.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"

static void render_sum(struct RILeviSet *s, float *out, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++) {
        float m = 0.0f;
        uint32_t v;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], 0, 48000.0f);
        out[i] = m;
    }
}

static uint32_t energy(const float *b, uint32_t n) {
    uint32_t i, k = 0u;
    for (i = 0u; i < n; i++)
        if (b[i] != 0.0f)
            k++;
    return k;
}

static int finite(const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (!(b[i] > -1e30f && b[i] < 1e30f))
            return 0;
    return 1;
}

int main(void) {
    static struct RILeviSet s;
    static float o[4800];
    uint32_t v;
    RI_ASSERT(RI_LEVI_NOPS == 8u, "8 ops");
    levi_init_set(&s);
    /* Default is DUO: one 2-op pair, the v1 sound. */
    RI_ASSERT(levi_algo_get(&s, 0u) == 0, "default duo");
    RI_ASSERT(levi_trigger(&s, 0u, 59u) == 0, "trig");
    render_sum(&s, o, 4800u);
    RI_ASSERT(energy(o, 4800u) > 1000u, "duo sounds %u", energy(o, 4800u));
    RI_ASSERT(finite(o, 4800u), "duo finite");
    /* Presets 0..7 select; out of range fails closed. */
    for (v = 0u; v < 8u; v++)
        RI_ASSERT(levi_set_algo(&s, 0u, v) == 0, "preset %u", v);
    RI_ASSERT(levi_algo_get(&s, 0u) == 7, "algo reads back");
    RI_ASSERT(levi_set_algo(&s, 0u, RI_LEVI_ALGO_CUSTOM) == 2, "custom not settable");
    RI_ASSERT(levi_set_algo(&s, 8u, 0u) == 2, "bad voice");
    RI_ASSERT(levi_set_algo(0, 0u, 0u) == 2, "null set");
    RI_ASSERT(levi_algo_get(&s, 8u) < 0, "bad voice reads negative");
    RI_ASSERT(levi_algo_get(0, 0u) < 0, "null reads negative");
    /* Every preset sounds finite with a triad held. */
    for (v = 0u; v < 8u; v++) {
        uint32_t k;
        levi_init_set(&s);
        RI_ASSERT(levi_set_algo(&s, 0u, v) == 0, "preset %u", v);
        RI_ASSERT(levi_trigger(&s, 0u, 59u) == 0, "trig");
        RI_ASSERT(levi_trigger(&s, 1u, 62u) == 0, "trig");
        render_sum(&s, o, 4800u);
        RI_ASSERT(finite(o, 4800u), "preset %u finite", v);
        for (k = 0u; k < 4800u; k++)
            o[k] = 0.0f;
        levi_release(&s, 0u);
        levi_release(&s, 1u);
        render_sum(&s, o, 4800u);
        render_sum(&s, o, 4800u); /* release tail is 150 ms = 7200 samples */
        render_sum(&s, o, 4800u);
        RI_ASSERT(energy(o, 4800u) == 0u, "preset %u rests", v);
    }
    /* Custom routing: op1 modulates op0; carriers sum. */
    levi_init_set(&s);
    RI_ASSERT(levi_set_route(&s, 0u, 0u, -1) == 0, "op0 carrier");
    RI_ASSERT(levi_set_route(&s, 0u, 1u, 0) == 0, "op1 -> op0");
    RI_ASSERT(levi_trigger(&s, 0u, 59u) == 0, "trig");
    render_sum(&s, o, 4800u);
    RI_ASSERT(energy(o, 4800u) > 1000u && finite(o, 4800u), "custom sounds");
    /* Cycle rejected; self-route rejected; bad op/voice refused. */
    RI_ASSERT(levi_set_route(&s, 0u, 0u, 1) == 2, "cycle 0->1->0 refused");
    RI_ASSERT(levi_set_route(&s, 0u, 3u, 3) == 2, "self refused");
    RI_ASSERT(levi_set_route(&s, 0u, 8u, -1) == 2, "bad op");
    RI_ASSERT(levi_set_route(&s, 0u, 2u, 8) == 2, "bad src");
    RI_ASSERT(levi_set_route(&s, 8u, 0u, -1) == 2, "bad voice");
    RI_ASSERT(levi_set_route(0, 0u, 0u, -1) == 2, "null set");
    RI_ASSERT(levi_route_get(&s, 0u, 1u) == 0, "route reads back");
    RI_ASSERT(levi_route_get(&s, 0u, 7u) == -1, "unset reads carrier");
    RI_ASSERT(levi_route_get(&s, 0u, 8u) == -2, "bad op reads -2");
    RI_ASSERT(levi_route_get(&s, 8u, 0u) == -2, "bad voice reads -2");
    RI_ASSERT(levi_algo_get(&s, 0u) == (int)RI_LEVI_ALGO_CUSTOM, "custom reads 64");
    RI_RESULT("levialgo");
}
