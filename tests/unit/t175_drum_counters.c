/* t175_drum_counters — per-block active voice-sample counters for the drum
 * sections (drum-tail A0).
 *
 * WHY. A drum machine's block cost scales with how many voices sound in that
 * block: a block where kick, snare, hats and a cymbal overlap costs several
 * times a block with one hat. The "tail" (max/avg up to 4x on the 808) is
 * therefore suspected to be polyphony, not a spike — but suspicion is not a
 * number. These counters turn it into a regression: per 64-sample block, how
 * many voice-samples were active, paired with that block's stage time.
 *
 * Counting rules (pinned here, used by the bench and the Dell fit):
 *   909: per sample, +1 per voice with active set, +1 more when that voice's
 *        flam second playhead is live (pos2 >= 0). A flamming voice counts 2.
 *        The single sample on which the flam FIRES counts 1 (pos2 is armed
 *        inside the render): steady-state blocks are exact, the fire block
 *        reads strictly between 1x and 2x. That boundary is documented, not
 *        hidden — the bench control uses steady-state blocks only.
 *   808: per sample, +1 per SLOT whose selected voice is active. A voice that
 *        is active but not selected (switch-pair partner after last-wins)
 *        does no work and counts 0.
 *   Both sets also count vc_samples (samples rendered since reset). The
 *   engine resets both sets once per block before the section render (the
 *   levi vc_* discipline), so the figures describe the last block alone.
 *
 * The Dell pairing needs per-block STAGE times with no extra clock reads
 * (a read costs 4-6 us against a 22-54 us stage). The S808/S909 stages use
 * RI_ESTAGE_E_STORE into drum_last808_us/drum_last909_us — the same pair
 * the stage already pays — and ri_engine_drum_sample stores (us, active)
 * every 256th full 64-sample block into a 512-entry ring, then stops.
 * Partial (event-split) blocks are not blocks and never tick the stride.
 * The ring only records when a clock is injected: no clock, no record.
 *
 * Laws below; RED-first: none of the symbols exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/dsp/rb909.h"
#include "engine/dsp/rb808.h"

#define SR 48000.0f
#define BLK 64u

static float OUT[BLK];
static float OUT2[128];
static float OUTL[128], OUTR[128];

/* One long shared layer: voices must stay active for whole runs. */
static float LAY[65536];
static const struct RISampleLayer LAY1[1] = { { LAY, 65536u, 48000u, 0,
    127, { 0, 0 } } };

static void bake(void) {
    uint32_t i;
    for (i = 0u; i < 65536u; i++)
        LAY[i] = 0.5f;
}

static void bind909(struct RB909Set *s) {
    uint32_t v;
    for (v = 0u; v < RI_909_NVOICES; v++)
        RI_ASSERT(rb909_set_layers(s, v, LAY1, 1) == 0, "bind %u", v);
}

static void block909(struct RB909Set *s) {
    rb909_voice_counters_reset(s);
    rb909_render_mix(s, OUT, BLK, SR);
}

static void block808(struct RB808Set *s) {
    rb808_voice_counters_reset(s);
    rb808_render_mix(s, OUT, BLK, SR);
}

/* Fake clock: +1000 per read, so one stage pair costs exactly 1000. */
static uint64_t g_t;
static uint64_t tick_clock(void) {
    g_t += 1000u;
    return g_t;
}

int main(void) {
    bake();

    /* 1. 909 silence: samples counted, nobody active. */
    {
        struct RB909Set s;
        rb909_init_set(&s);
        bind909(&s);
        block909(&s);
        RI_ASSERT(s.vc_samples == 64u, "909 silent samples %u", s.vc_samples);
        RI_ASSERT(s.vc_voice_active == 0u, "909 silent active %u",
            s.vc_voice_active);
    }

    /* 2. 909 positive control: k voices -> k x 64 per block. */
    {
        struct RB909Set s;
        rb909_init_set(&s);
        bind909(&s);
        rb909_trigger(&s, RB909_BD, 0u, 64u, 0);
        rb909_trigger(&s, RB909_CH, 0u, 64u, 0);
        rb909_trigger(&s, RB909_CR, 0u, 64u, 0);
        block909(&s);
        RI_ASSERT(s.vc_samples == 64u, "909 k samples %u", s.vc_samples);
        RI_ASSERT(s.vc_voice_active == 3u * 64u, "909 k=3 active %u",
            s.vc_voice_active);
    }

    /* 3. 909 flam: the second playhead counts separately. BD accent 2 arms
     * the compat flam at 1680 samples; blocks fully before read 64, the
     * fire block reads strictly between, blocks fully after read 128. */
    {
        struct RB909Set s;
        uint32_t b;
        rb909_init_set(&s);
        bind909(&s);
        rb909_trigger(&s, RB909_BD, 2u, 64u, (int32_t)RI_909_FLAM_DEFAULT_SMP);
        for (b = 0u; b < 26u; b++) {
            block909(&s);
            RI_ASSERT(s.vc_voice_active == 64u, "pre-flam b%u %u", b,
                s.vc_voice_active);
        }
        block909(&s); /* block 26: fires mid-block at sample 1680 */
        RI_ASSERT(s.vc_voice_active > 64u && s.vc_voice_active <= 128u,
            "fire block %u", s.vc_voice_active);
        block909(&s);
        RI_ASSERT(s.vc_voice_active == 128u, "post-flam %u",
            s.vc_voice_active);
    }

    /* 4. 808 positive control: distinct slots -> k x 64. */
    {
        struct RB808Set s;
        rb808_init_set(&s);
        rb808_trigger(&s, RB808_BD, 0u, 0.0f);
        rb808_trigger(&s, RB808_SD, 0u, 0.0f);
        rb808_trigger(&s, RB808_LT, 0u, 0.0f);
        rb808_trigger(&s, RB808_MT, 0u, 0.0f);
        block808(&s);
        RI_ASSERT(s.vc_samples == 64u, "808 k samples %u", s.vc_samples);
        RI_ASSERT(s.vc_voice_active == 4u * 64u, "808 k=4 active %u",
            s.vc_voice_active);
    }

    /* 5. 808 shared slot: RS then CL, CL wins the slot; RS stays active
     * but does no work and counts 0. */
    {
        struct RB808Set s;
        rb808_init_set(&s);
        rb808_trigger(&s, RB808_RS, 0u, 0.0f);
        rb808_trigger(&s, RB808_CL, 0u, 0.0f);
        RI_ASSERT(s.v[RB808_RS].active && s.v[RB808_CL].active,
            "both must stay active");
        RI_ASSERT(s.slot[rb808_slot_of(RB808_RS)] == RB808_CL,
            "last wins the slot");
        block808(&s);
        RI_ASSERT(s.vc_voice_active == 64u, "shared slot %u",
            s.vc_voice_active);
    }

    /* 6. 808 silence. */
    {
        struct RB808Set s;
        rb808_init_set(&s);
        block808(&s);
        RI_ASSERT(s.vc_voice_active == 0u, "808 silent %u",
            s.vc_voice_active);
    }

    /* 7. Engine reset discipline: counters describe the last block alone.
     * No clock: the vc counters still count (the bench has no clock). */
    {
        struct RIEngine e;
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        RI_ASSERT(ri_engine_909_bind(&e, 0u, LAY1, 1) == 0, "eng bind");
        rb909_trigger(&e.s909, 0u, 0u, 64u, 0);
        ri_engine_load(&e, 0, 0u, 128u, RI_ENGINE_S909);
        RI_ASSERT(ri_engine_render(&e, OUTL, OUTR, 128u, SR) == 128u,
            "eng short");
        RI_ASSERT(e.s909.vc_samples == 64u, "eng last-block samples %u",
            e.s909.vc_samples);
        RI_ASSERT(e.s909.vc_voice_active == 64u, "eng last-block active %u",
            e.s909.vc_voice_active);
        RI_ASSERT(e.drum_n == 0u, "no clock, no ring record");
    }

    /* 8. Engine drum-sample mechanics with a fake clock: stride, spans,
     * partial skip. 300 full blocks -> samples at stride ticks 0 and 256. */
    {
        struct RIEngine e;
        uint32_t b;
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        RI_ASSERT(ri_engine_909_bind(&e, 0u, LAY1, 1) == 0, "eng bind 2");
        rb909_trigger(&e.s909, 0u, 0u, 64u, 0);
        rb808_trigger(&e.s808, RB808_BD, 0u, 0.0f);
        rb808_set_decay(&e.s808, RB808_BD, 4.0f);
        ri_engine_load(&e, 0, 0u, 400u * 64u, RI_ENGINE_S808 | RI_ENGINE_S909);
        ri_engine_set_clock(&e, tick_clock);
        g_t = 0u;
        for (b = 0u; b < 300u; b++)
            RI_ASSERT(ri_engine_render(&e, OUTL, OUTR, 64u, SR) == 64u,
                "step %u", b);
        RI_ASSERT(e.drum_tick == 300u, "tick %u", e.drum_tick);
        RI_ASSERT(e.drum_n == 2u, "ring n %u", e.drum_n);
        RI_ASSERT(e.drum_ring[0].us808 == 1000u, "us808 %u",
            e.drum_ring[0].us808);
        RI_ASSERT(e.drum_ring[0].us909 == 1000u, "us909 %u",
            e.drum_ring[0].us909);
        RI_ASSERT(e.drum_ring[0].a808 == 64u, "a808 %u",
            e.drum_ring[0].a808);
        RI_ASSERT(e.drum_ring[0].a909 == 64u, "a909 %u",
            e.drum_ring[0].a909);
        /* A partial (event-split-shaped) render never ticks the stride. */
        RI_ASSERT(ri_engine_render(&e, OUT2, OUT, 36u, SR) == 36u,
            "partial short");
        RI_ASSERT(e.drum_tick == 300u, "partial tick %u", e.drum_tick);
        RI_ASSERT(e.drum_n == 2u, "partial must not sample (n %u)",
            e.drum_n);
    }

    /* 9. Stop-when-full, driven directly (131072 renders would be silly):
     * stride sampling caps at RI_DRUMDIAG_N and never writes past it. */
    {
        struct RIEngine e;
        uint32_t i, n;
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_set_clock(&e, tick_clock);
        e.sections = RI_ENGINE_S808 | RI_ENGINE_S909;
        e.drum_last808_us = 11u;
        e.drum_last909_us = 22u;
        e.s808.vc_voice_active = 33u;
        e.s909.vc_voice_active = 44u;
        n = (RI_DRUMDIAG_N + 4u) * RI_DRUMDIAG_STRIDE;
        for (i = 0u; i < n; i++)
            ri_engine_drum_sample(&e, 64u);
        RI_ASSERT(e.drum_n == RI_DRUMDIAG_N, "cap %u", e.drum_n);
        RI_ASSERT(e.drum_ring[RI_DRUMDIAG_N - 1u].us808 == 11u, "last kept");
        RI_ASSERT(e.drum_ring[RI_DRUMDIAG_N - 1u].a909 == 44u, "last kept 2");
    }

    RI_RESULT("drum_counters");
    return 0;
}
