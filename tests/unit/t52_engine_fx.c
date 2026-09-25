/* t52_engine_fx — §12.8: inserts + pan + send/stereo-return in the engine.
 * Duet fixture (A plays [0,4800), B plays [9600,14400), N=19200):
 *   (a) neutral: L == R sample-exact, whole == two-slice render.
 *   (b) dist on section 0 changes the A region, leaves B bit-identical.
 *   (c) pan hard-left on 0: R silent in A region, L carries; hard-right
 *       on 1 mirrors in the B region.
 *   (d) send 0 + delay line installed == neutral (no-send bypass);
 *       send up + delay line -> render differs (echo present).
 *   (e) comp on master changes hot output; GR meter reads <= 0.
 *   (f) FX engaged stays chunk-agnostic (whole vs halves identical).
 * RED-first: none of the engine FX APIs exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f
#define N 19200u

static float LA[N], RA[N], LB[N], RB[N];
static float DLINE[96000u];

static void duet(struct RIEngine *e, uint32_t sections) {
    static struct RIEvent ev[4];
    ev[0].sample = 0;     ev[0].type = RI_EV_NOTE_ON;  ev[0].device = 0;
    ev[0].voice = 0;      ev[0].value = 69;            ev[0].flags = 0; ev[0].seq = 0;
    ev[1].sample = 4800;  ev[1].type = RI_EV_NOTE_OFF; ev[1].device = 0;
    ev[1].voice = 0;      ev[1].value = 0;             ev[1].flags = 0; ev[1].seq = 1;
    ev[2].sample = 9600;  ev[2].type = RI_EV_NOTE_ON;  ev[2].device = 1;
    ev[2].voice = 0;      ev[2].value = 81;            ev[2].flags = 0; ev[2].seq = 2;
    ev[3].sample = 14400; ev[3].type = RI_EV_NOTE_OFF; ev[3].device = 1;
    ev[3].voice = 0;      ev[3].value = 0;             ev[3].flags = 0; ev[3].seq = 3;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 4, N, sections);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

static float region_rms(const float *b, uint32_t s, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = s; i < s + n; i++)
        acc += (double)b[i] * (double)b[i];
    return (float)(acc / (double)n);
}

int main(void) {
    struct RIEngine e, f;
    uint32_t secs = RI_ENGINE_S303A | RI_ENGINE_S303B;
    uint32_t i;

    /* (a) neutral unity + determinism. */
    duet(&e, secs);
    RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "neutral short");
    for (i = 0; i < N; i++)
        RI_ASSERT(LA[i] == RA[i], "neutral L!=R at %u", i);
    duet(&f, secs);
    RI_ASSERT(ri_engine_render(&f, LB, RB, N, SR) == N, "re-render short");
    RI_ASSERT(!differ(LA, LB, N), "neutral nondet");

    /* (b) dist on section 0: A-solo moves, B-solo bit-identical
     * (an engaged insert transforms its own section bus; isolation
     * means the other voice never passes through it). */
    duet(&e, RI_ENGINE_S303A);
    RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "Asolo short");
    duet(&e, RI_ENGINE_S303A);
    RI_ASSERT(ri_engine_assign_insert(&e, RI_ROUTE_DIST, 0) == -1,
        "dist assign prev");
    ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "dist short");
    RI_ASSERT(differ(LA, LB, 4800u), "dist inaudible on A");
    duet(&e, RI_ENGINE_S303B);
    RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "Bsolo short");
    duet(&e, RI_ENGINE_S303B);
    ri_engine_assign_insert(&e, RI_ROUTE_DIST, 0);
    ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 100);
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "Bsolo treated");
    RI_ASSERT(!differ(LA, LB, N), "dist leaked into B");

    /* (c) pan on solos: hard-left A -> R exactly zero; hard-right
     * B -> L exactly zero (x * 0.0 == 0.0 for finite x). */
    duet(&e, RI_ENGINE_S303A);
    RI_ASSERT(ri_engine_set_pan(&e, 0, 0) == 0, "pan0");
    RI_ASSERT(ri_engine_set_pan(&e, 9, 64) == 2, "pan bad bus");
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "pan short");
    for (i = 0; i < N; i++)
        RI_ASSERT(RB[i] == 0.0f, "A leaks to R at %u", i);
    RI_ASSERT(region_rms(LB, 0, 4800u) > 0.0f, "A silent on L");
    duet(&e, RI_ENGINE_S303B);
    RI_ASSERT(ri_engine_set_pan(&e, 1, 127) == 0, "pan1");
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "panB short");
    for (i = 0; i < N; i++)
        RI_ASSERT(LB[i] == 0.0f, "B leaks to L at %u", i);
    RI_ASSERT(region_rms(RB, 9600u, 4800u) > 0.0f, "B silent on R");

    /* (d) delay send: line + send 0 == neutral (bypass with line live);
     * send up differs; NULL line detaches legally (dry default). */
    duet(&e, secs);
    RI_ASSERT(ri_engine_render(&e, LA, RA, N, SR) == N, "neutral duet");
    duet(&e, secs);
    RI_ASSERT(ri_engine_set_delay(&e, DLINE, 96000u) == 0, "delay line");
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "send0 short");
    RI_ASSERT(!differ(LA, LB, N), "send0 != neutral");
    duet(&e, secs);
    ri_engine_set_delay(&e, DLINE, 96000u);
    ri_engine_set_tempo(&e, 140.0f);
    /* 1 straight 16th @140 BPM = 5143 samples: the A-region send
     * echoes inside the window (steps 4 = 1 beat = 20571 > N). */
    ri_engine_fx_set(&e, RI_FXID_DELAY_STEPS, 1);
    ri_engine_fx_set(&e, RI_FXID_DELAY_FB, 64);
    RI_ASSERT(ri_engine_set_send(&e, 0, 100) == 0, "send0 up");
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "send short");
    RI_ASSERT(differ(LA, LB, N), "send inaudible");
    RI_ASSERT(ri_engine_set_delay(&e, 0, 0) == 0, "detach illegal");

    /* (e) master comp: audible + GR meter. */
    duet(&e, secs);
    RI_ASSERT(ri_engine_assign_insert(&e, RI_ROUTE_COMP,
        RI_ROUTE_MASTER) == -1, "comp master prev");
    ri_engine_fx_set(&e, RI_FXID_COMP_THRESH, 8); /* low threshold: hot */
    RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "comp short");
    RI_ASSERT(differ(LA, LB, N), "master comp inaudible");
    RI_ASSERT(ri_engine_comp_gr(&e) <= 0.0f, "GR positive");

    /* (f) chunk-agnostic with FX engaged. */
    {
        struct RIEngine g;
        uint32_t got;
        duet(&e, secs);
        ri_engine_assign_insert(&e, RI_ROUTE_DIST, 1);
        ri_engine_fx_set(&e, RI_FXID_DIST_DRIVE, 80);
        ri_engine_set_pan(&e, 1, 100);
        RI_ASSERT(ri_engine_render(&e, LB, RB, N, SR) == N, "whole short");
        duet(&g, secs);
        ri_engine_assign_insert(&g, RI_ROUTE_DIST, 1);
        ri_engine_fx_set(&g, RI_FXID_DIST_DRIVE, 80);
        ri_engine_set_pan(&g, 1, 100);
        got = ri_engine_render(&g, LA, RA, N / 2u, SR);
        RI_ASSERT(got == N / 2u, "half1 short");
        got += ri_engine_render(&g, LA + N / 2u, RA + N / 2u, N / 2u, SR);
        RI_ASSERT(got == N, "half2 short");
        RI_ASSERT(!differ(LA, LB, N), "chunk L differs");
        RI_ASSERT(!differ(RA, RB, N), "chunk R differs");
    }

    RI_RESULT("engine_fx");
}
