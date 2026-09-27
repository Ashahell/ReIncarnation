/* t99_pack_bind_lifetime — 909 layer descriptors are engine-owned copy.
 * Dell 2026-09-27 guru (render task, ri_layer_mix +0x226): pack_909.c
 * bound a stack descriptor array; set_layers stored the pointer; the
 * frame recycled and the render task faulted minutes later. The header
 * always documented copy semantics ("copies the descriptors, NOT the
 * sample data") — this test pins it: pointer identity differs, fields
 * match, and an out-of-scope bind still renders bit-identical audio.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/rb909.h"

static const struct RISampleLayer *s_passed;
static float s_smp[128];

static void bind_scoped(struct RB909Set *s) {
    struct RISampleLayer lay[1];
    uint32_t i;
    for (i = 0u; i < 128u; i++)
        s_smp[i] = (float)i / 128.0f;
    lay[0].data = s_smp;
    lay[0].frames = 128u;
    lay[0].rate = 48000u;
    lay[0].lo = 0u;
    lay[0].hi = 127u;
    s_passed = lay;
    RI_ASSERT(rb909_set_layers(s, RB909_BD, lay, 1u) == 0, "bind rc");
}

static void render_voice(struct RB909Set *s, float *out, uint32_t n) {
    uint32_t i;
    rb909_trigger(s, RB909_BD, 0u, 64u, RI_909_FLAM_DEFAULT_SMP);
    for (i = 0u; i < n; i++)
        out[i] = rb909_voice_render(&s->v[RB909_BD], 48000.0f);
}

int main(void) {
    static struct RB909Set a, b;
    static float oa[256], ob[256];
    struct RISampleLayer lay[1];
    uint32_t i, nonzero = 0u;
    rb909_init_set(&a);
    rb909_init_set(&b);
    bind_scoped(&a);
    RI_ASSERT(a.v[RB909_BD].n_layers == 1u, "n layers");
    RI_ASSERT(a.v[RB909_BD].layers != s_passed, "owned copy, not caller ptr");
    RI_ASSERT(a.v[RB909_BD].layers[0].data == s_smp, "data kept");
    RI_ASSERT(a.v[RB909_BD].layers[0].frames == 128u, "frames kept");
    /* Synchronous bind on b (always-safe shape) must render identically. */
    lay[0].data = s_smp;
    lay[0].frames = 128u;
    lay[0].rate = 48000u;
    lay[0].lo = 0u;
    lay[0].hi = 127u;
    RI_ASSERT(rb909_set_layers(&b, RB909_BD, lay, 1u) == 0, "bind b rc");
    render_voice(&a, oa, 256u);
    render_voice(&b, ob, 256u);
    for (i = 0u; i < 256u; i++) {
        RI_ASSERT(oa[i] == ob[i], "identical render");
        if (oa[i] != 0.0f)
            nonzero++;
    }
    RI_ASSERT(nonzero > 0u, "audible");
    RI_RESULT("packbind");
}
