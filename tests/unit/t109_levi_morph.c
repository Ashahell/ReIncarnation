/* t109_levi_morph — algorithm morph (owner 2026-09-28, v2 slice 1c).
 * Dual-bank crossfade (own design): bank A = current routing, bank B =
 * morph target; pos 0..100 blends carrier mixes, filter once. Pos 0 is
 * bit-identical to no morph; pos 100 is bit-identical to a pure preset-B
 * voice; mid differs from both; everything finite; fail-closed edges.
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
            m += levi_voice_render(&s->v[v], 48000.0f);
        out[i] = m;
    }
}

static int finite(const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (!(b[i] > -1e30f && b[i] < 1e30f))
            return 0;
    return 1;
}

int main(void) {
    static struct RILeviSet s, t;
    static float oa[2048], ob[2048];
    levi_init_set(&s);
    /* Default: pos 0, target DUO. */
    RI_ASSERT(levi_morph_get(&s, 0u) == 0, "pos default");
    RI_ASSERT(levi_morph_get(&s, 6u) < 0, "bad voice neg");
    RI_ASSERT(levi_morph_get(0, 0u) < 0, "null neg");
    /* Render reference, then an identically-reset voice morphed at
     * sample 0: pos 0 must be bit-identical (B contributes x0). */
    RI_ASSERT(levi_trigger(&s, 0u, 59u) == 0, "trig");
    render_sum(&s, oa, 2048u);
    levi_init_set(&s);
    RI_ASSERT(levi_trigger(&s, 0u, 59u) == 0, "trig");
    RI_ASSERT(levi_set_morph(&s, 0u, RI_LEVI_ALGO_ALLPAR, 0u) == 0, "morph set");
    RI_ASSERT(levi_morph_get(&s, 0u) == 0, "pos reads");
    render_sum(&s, ob, 2048u);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "pos 0 identical");
    /* Mid morph differs from both ends, stays finite. */
    RI_ASSERT(levi_set_morph(&s, 0u, RI_LEVI_ALGO_ALLPAR, 50u) == 0, "mid");
    render_sum(&s, ob, 2048u);
    RI_ASSERT(finite(ob, 2048u), "mid finite");
    RI_ASSERT(memcmp(ob, oa, sizeof oa) != 0, "mid differs from A");
    /* Pos 100 == pure preset-B voice, bit-exact (trigger resets both
     * banks, so the morph-time state copy is overwritten identically). */
    levi_init_set(&s);
    levi_init_set(&t);
    RI_ASSERT(levi_set_morph(&s, 0u, RI_LEVI_ALGO_ALLPAR, 100u) == 0, "full");
    RI_ASSERT(levi_trigger(&s, 0u, 59u) == 0, "trig");
    RI_ASSERT(levi_set_algo(&t, 0u, RI_LEVI_ALGO_ALLPAR) == 0, "pure B");
    RI_ASSERT(levi_trigger(&t, 0u, 59u) == 0, "trig");
    render_sum(&s, oa, 2048u);
    render_sum(&t, ob, 2048u);
    RI_ASSERT(!memcmp(oa, ob, sizeof oa), "pos 100 == pure B");
    /* Fail-closed edges. */
    RI_ASSERT(levi_set_morph(&s, 0u, RI_LEVI_ALGO_ALLPAR, 101u) == 2, "pos range");
    RI_ASSERT(levi_set_morph(&s, 0u, 8u, 50u) == 2, "bad target");
    RI_ASSERT(levi_set_morph(&s, 6u, RI_LEVI_ALGO_ALLPAR, 50u) == 2, "bad voice");
    RI_ASSERT(levi_set_morph(0, 0u, RI_LEVI_ALGO_ALLPAR, 50u) == 2, "null set");
    RI_RESULT("levimorph");
}
