/* t78_engine_taps — per-section + per-FX peak taps (C2 engine prerequisite).
 * Peaks follow rendered energy: a sounding section reads > 0, silent ones
 * read 0; each FX meter follows its own unit's renders; bad indices and a
 * fresh engine read 0; identical renders give identical peaks.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"

#define SR 48000.0f
#define N 4800u

static float outL[N], outR[N];

static void load_note(struct RIEngine *e, uint32_t sections) {
    static struct RIEvent ev[2];
    ev[0].sample = 0;
    ev[0].type = RI_EV_NOTE_ON;
    ev[0].device = 0;
    ev[0].voice = 0;
    ev[0].value = 69;
    ev[0].flags = 0;
    ev[0].seq = 0;
    ev[1].sample = N;
    ev[1].type = RI_EV_NOTE_OFF;
    ev[1].device = 0;
    ev[1].voice = 0;
    ev[1].value = 0;
    ev[1].flags = 0;
    ev[1].seq = 1;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 2, N, sections);
}

int main(void) {
    struct RIEngine a, b;
    float p0;

    /* fresh engine: every tap reads 0 */
    ri_engine_init(&a);
    RI_ASSERT(ri_engine_section_peak(&a, 0u) == 0.0f, "fresh sec0");
    RI_ASSERT(ri_engine_section_peak(&a, 3u) == 0.0f, "fresh sec3");
    RI_ASSERT(ri_engine_section_peak(&a, 4u) == 0.0f, "bad section");
    RI_ASSERT(ri_engine_fx_peak(&a, 0u) == 0.0f, "fresh dist");
    RI_ASSERT(ri_engine_fx_peak(&a, 3u) == 0.0f, "fresh comp");
    RI_ASSERT(ri_engine_fx_peak(&a, 4u) == 0.0f, "bad unit");
    RI_ASSERT(ri_engine_section_peak(0, 0u) == 0.0f, "null engine");
    RI_ASSERT(ri_engine_fx_peak(0, 0u) == 0.0f, "null fx");

    /* 303A sounds: its tap reads energy, the other sections stay 0 */
    load_note(&a, RI_ENGINE_S303A);
    RI_ASSERT(ri_engine_render(&a, outL, outR, N, SR) == N, "rendered");
    p0 = ri_engine_section_peak(&a, 0u);
    RI_ASSERT(p0 > 0.0f, "sec0 energy %f", p0);
    RI_ASSERT(ri_engine_section_peak(&a, 1u) == 0.0f, "sec1 silent");
    RI_ASSERT(ri_engine_section_peak(&a, 2u) == 0.0f, "sec2 silent");
    RI_ASSERT(ri_engine_section_peak(&a, 3u) == 0.0f, "sec3 silent");

    /* determinism: same events, same peaks */
    load_note(&b, RI_ENGINE_S303A);
    ri_engine_render(&b, outL, outR, N, SR);
    RI_ASSERT(ri_engine_section_peak(&b, 0u) == p0, "deterministic");

    /* FX: DIST assigned to section 0 renders -> its meter reads energy */
    load_note(&a, RI_ENGINE_S303A);
    RI_ASSERT(ri_engine_assign_insert(&a, RI_ROUTE_DIST, 0) != -2, "assign dist");
    ri_engine_render(&a, outL, outR, N, SR);
    RI_ASSERT(ri_engine_fx_peak(&a, RI_ENGINE_FX_DIST) > 0.0f, "dist energy");
    RI_ASSERT(ri_engine_fx_peak(&a, RI_ENGINE_FX_PCF) == 0.0f, "pcf silent");
    RI_ASSERT(ri_engine_fx_peak(&a, RI_ENGINE_FX_DELAY) == 0.0f, "delay silent");
    RI_ASSERT(ri_engine_fx_peak(&a, RI_ENGINE_FX_COMP) == 0.0f, "comp silent");

    RI_RESULT("taps");
}
