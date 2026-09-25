/* t57_engine_drums — §12.7a/m64: 808/909 hosting in the engine.
 *   (a) 808 section renders lane hits; section off = silence.
 *   (b) 909 renders; HIGH-flag hits louder than LOW (flag path live).
 *   (c) same-sample AC accents 808 hits (stamp path; ON sorts before AC).
 *   (d) 909 flam bit adds the second hit (differs from no-flam).
 *   (e) OH+CH same step on 808: both present, OH cut short by CH.
 *   (f) reserved lanes (>= 11) ignored: silent, no crash.
 *   (g) 303 behavior unchanged (t37 covers; asserted here as L==R).
 * RED-first: the engine hosts no drum sets yet.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/pattern.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

#define SR 48000.0f
#define N 19200u

static float LA[N], RA[N], LB[N], RB[N];
static const struct RISegment SEG0[] = { { 0, 428571428ULL } };
static const struct RITempoMap MAP = { SEG0, 1, 96, 48000u };
static const struct RISchedOpts OPTS = { 0, 0, RI_FLAM_MS_DEFAULT };

/* Minimal 909 layers (decaying sine; production binds the pack later). */
static float LAY[4800];
static const struct RISampleLayer LAY1[1] = { { LAY, 4800u, 48000u, 0,
    127, { 0, 0 } } };

static void bake_layers(void) {
    uint32_t i;
    for (i = 0; i < 4800u; i++) {
        double t = (double)i / 48000.0;
        LAY[i] = (float)(0.8 * sin(2.0 * 3.141592653589793 * 440.0 * t) *
            exp(-t / 0.05));
    }
}

static void bind909(struct RIEngine *e) {
    uint32_t v;
    for (v = 0; v < 11u; v++)
        RI_ASSERT(ri_engine_909_bind(e, v, LAY1, 1) == 0, "bind %u", v);
}

/* Emit a drum pattern into ev (returns count). */
static uint32_t emit_drum(struct RIPattern *p, uint16_t device,
    struct RIEvent *ev) {
    return ri_sched_emit_pattern(p, device, &MAP, 0, 96, &OPTS, 0, 0,
        ev, 256);
}

static float rms(const float *b, uint32_t s, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = s; i < s + n && i < N; i++)
        acc += (double)b[i] * (double)b[i];
    return (float)(acc / (double)n);
}

int main(void) {
    struct RIEngine e;
    static struct RIEvent ev[256];
    struct RIPattern p;
    uint32_t nev, got;

    bake_layers();

    /* (a) 808 BD hits render; section off = silence. (N covers ~3.7
     * sixteenths, so hits sit at steps 0 and 2.) */
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_pattern_set_length(&p, 4);
    ri_pdrum_set(&p, 0, RI_L808_BD, RI_HIT_LOW);
    ri_pdrum_set(&p, 2, RI_L808_BD, RI_HIT_LOW);
    nev = emit_drum(&p, 2, ev);
    RI_ASSERT(nev == 2u, "808 events %u", nev);
    ri_engine_init(&e);
    ri_engine_defaults(&e);
    ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808);
    got = ri_engine_render(&e, LA, RA, N, SR);
    RI_ASSERT(got == N, "808 short");
    RI_ASSERT(rms(LA, 0, 4800u) > 0.0f, "808 BD silent");
    RI_ASSERT(rms(LA, 9600u, 4800u) > 0.0f, "808 BD hit 2 silent");
    ri_engine_init(&e);
    ri_engine_defaults(&e);
    ri_engine_load(&e, ev, nev, N, 0);
    got = ri_engine_render(&e, LB, RB, N, SR);
    RI_ASSERT(got == N, "808off short");
    RI_ASSERT(rms(LB, 0, N) == 0.0f && rms(RB, 0, N) == 0.0f,
        "808 off audible");

    /* (b) 909 HIGH louder than LOW. */
    ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_pattern_set_length(&p, 4);
    ri_pdrum_set(&p, 0, RI_L909_SD, RI_HIT_HIGH);
    ri_pdrum_set(&p, 2, RI_L909_SD, RI_HIT_LOW);
    nev = emit_drum(&p, 3, ev);
    ri_engine_init(&e);
    ri_engine_defaults(&e);
    bind909(&e);
    ri_engine_load(&e, ev, nev, N, RI_ENGINE_S909);
    got = ri_engine_render(&e, LA, RA, N, SR);
    RI_ASSERT(got == N, "909 short");
    {
        float hi = rms(LA, 0, 4800u), lo = rms(LA, 9600u, 4800u);
        RI_ASSERT(hi > 0.0f && lo > 0.0f, "909 silent %g %g",
            (double)hi, (double)lo);
        RI_ASSERT(hi > lo * 1.05f, "909 high<=low %g %g", (double)hi,
            (double)lo);
    }

    /* (c) AC accents same-step 808 hits. */
    {
        float plain, accd;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 4);
        ri_pdrum_set(&p, 0, RI_L808_BD, RI_HIT_LOW);
        nev = emit_drum(&p, 2, ev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808);
        ri_engine_render(&e, LA, RA, N, SR);
        plain = rms(LA, 0, 4800u);
        ri_pdrum_set_ac(&p, 0, 1);
        nev = emit_drum(&p, 2, ev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808);
        ri_engine_render(&e, LB, RB, N, SR);
        accd = rms(LB, 0, 4800u);
        RI_ASSERT(plain > 0.0f, "ac base silent");
        RI_ASSERT(accd > plain * 1.05f, "AC inaudible %g vs %g",
            (double)accd, (double)plain);
    }

    /* (d) 909 flam adds the second hit. */
    {
        int diff = 0;
        uint32_t i;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
        ri_pattern_set_length(&p, 2);
        ri_pdrum_set(&p, 0, RI_L909_SD, RI_HIT_LOW);
        nev = emit_drum(&p, 3, ev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        bind909(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808 | RI_ENGINE_S909);
        ri_engine_render(&e, LA, RA, N, SR);
        ri_pdrum_set(&p, 0, RI_L909_SD, RI_HIT_FLAM);
        nev = emit_drum(&p, 3, ev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        bind909(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808 | RI_ENGINE_S909);
        ri_engine_render(&e, LB, RB, N, SR);
        for (i = 0; i < N; i++)
            if (LA[i] != LB[i]) {
                diff = 1;
                break;
            }
        RI_ASSERT(diff, "flam inaudible");
    }

    /* (e) 808 OH+CH same step: OH-solo audible (lane mapping works);
     * OH+CH differs from OH-solo (CH present); OH+CH == CH-solo
     * bit-exactly (voice law: same-step CH cuts OH hard, t42). */
    {
        int d2 = 0;
        uint32_t i;
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 2);
        ri_pdrum_set(&p, 0, RI_L808_OH, RI_HIT_LOW);
        ri_pdrum_set(&p, 0, RI_L808_CH, RI_HIT_LOW);
        nev = emit_drum(&p, 2, ev);
        RI_ASSERT(nev == 2u, "ohch events %u", nev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808);
        ri_engine_render(&e, LA, RA, N, SR);
        RI_ASSERT(rms(LA, 0, 1200u) > 0.0f, "ohch attack silent");
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 2);
        ri_pdrum_set(&p, 0, RI_L808_CH, RI_HIT_LOW);
        nev = emit_drum(&p, 2, ev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808);
        ri_engine_render(&e, LB, RB, N, SR);
        for (i = 0; i < N; i++)
            RI_ASSERT(LA[i] == LB[i], "same-step != CH-solo at %u", i);
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 2);
        ri_pdrum_set(&p, 0, RI_L808_OH, RI_HIT_LOW);
        nev = emit_drum(&p, 2, ev);
        {
            struct RIEngine f;
            static float LC[N], RC[N];
            ri_engine_init(&f);
            ri_engine_defaults(&f);
            ri_engine_load(&f, ev, nev, N, RI_ENGINE_S808);
            ri_engine_render(&f, LC, RC, N, SR);
            RI_ASSERT(rms(LC, 0, 1200u) > 0.0f, "OH-solo silent");
            for (i = 0; i < N; i++)
                if (LA[i] != LC[i]) {
                    d2 = 1;
                    break;
                }
        }
        RI_ASSERT(d2, "CH missing from mix");
    }

    /* (f) reserved lane ignored. */
    {
        ri_pattern_init(&p, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
        ri_pattern_set_length(&p, 2);
        ri_pdrum_set(&p, 0, 12, RI_HIT_LOW);
        nev = emit_drum(&p, 2, ev);
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, ev, nev, N, RI_ENGINE_S808);
        got = ri_engine_render(&e, LA, RA, N, SR);
        RI_ASSERT(got == N, "reserved short");
        RI_ASSERT(rms(LA, 0, N) == 0.0f, "reserved lane audible");
    }

    /* (g) 303 path unchanged: centre unity. */
    {
        static struct RIEvent ev2[4];
        uint32_t i;
        ev2[0].sample = 0; ev2[0].type = RI_EV_NOTE_ON; ev2[0].device = 0;
        ev2[0].voice = 0; ev2[0].value = 69; ev2[0].flags = 0; ev2[0].seq = 0;
        ev2[1].sample = 4800; ev2[1].type = RI_EV_NOTE_OFF; ev2[1].device = 0;
        ev2[1].voice = 0; ev2[1].value = 0; ev2[1].flags = 0; ev2[1].seq = 1;
        ev2[2].sample = 9600; ev2[2].type = RI_EV_NOTE_ON; ev2[2].device = 1;
        ev2[2].voice = 0; ev2[2].value = 81; ev2[2].flags = 0; ev2[2].seq = 2;
        ev2[3].sample = 14400; ev2[3].type = RI_EV_NOTE_OFF; ev2[3].device = 1;
        ev2[3].voice = 0; ev2[3].value = 0; ev2[3].flags = 0; ev2[3].seq = 3;
        ri_engine_init(&e);
        ri_engine_defaults(&e);
        ri_engine_load(&e, ev2, 4, N,
            RI_ENGINE_S303A | RI_ENGINE_S303B);
        ri_engine_render(&e, LA, RA, N, SR);
        for (i = 0; i < N; i++)
            RI_ASSERT(LA[i] == RA[i], "303 unity at %u", i);
    }

    RI_RESULT("engine_drums");
}
