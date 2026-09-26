/* t79_auto_delivery — §12.9c Task 5b/5c: automation lane keys reach the
 * engine exactly like their knob paths.
 *   (a) 808 per-voice keys 0x0Cpv set ONE voice (rb808_set_param on that
 *       voice only); the section-wide accent key 0x0C50 sets every voice.
 *   (b) 909 per-voice keys 0x0Dpv; 0x0D1F = the shared CH/OH level.
 *   (c) strip keys 0x0Bsp: pan, delay send, dist/pcf/comp inserts (on =
 *       assign, off = release only when this strip owns the unit), master
 *       comp.
 *   (d) strip level: new ri_engine_set_level, E0 P-17 law (v/127)^2 with a
 *       64-sample zipless slew; 127 renders bit-identical with the
 *       pre-fader engine; the automation key renders identical to the
 *       setter; 0 silences the strip after the ramp.
 *   (e) unknown / legacy keys are ignored (0x0B00..0x0B0F panel ids,
 *       strip 6+, unknown blocks).
 * RED-first: no drum/strip key delivery and no level setter existed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "engine/seq/autolane.h"
#include "engine/dsp/rb303.h"

#define SR 48000.0f
#define N 9600u

static float LA[N], RA[N], LB[N], RB[N];
static struct RIEngine E, F;

static void solo(struct RIEngine *e) {
    static struct RIEvent ev[2];
    ev[0].sample = 0;    ev[0].type = RI_EV_NOTE_ON;  ev[0].device = 0;
    ev[0].voice = 0;     ev[0].value = 57;            ev[0].flags = 0; ev[0].seq = 0;
    ev[1].sample = 8000; ev[1].type = RI_EV_NOTE_OFF; ev[1].device = 0;
    ev[1].voice = 0;     ev[1].value = 0;             ev[1].flags = 0; ev[1].seq = 1;
    ri_engine_init(e);
    ri_engine_defaults(e);
    ri_engine_load(e, ev, 2, N, RI_ENGINE_S303A);
}

static void autoev(struct RIEngine *e, uint16_t key, uint8_t val, uint16_t device) {
    struct RIEvent a;
    memset(&a, 0, sizeof a);
    a.type = RI_EV_AUTOMATION;
    a.device = device;
    a.value = key;
    a.flags = val;
    ri_engine_apply_event(e, &a);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

int main(void) {
    uint32_t v;

    /* (a) 808 per voice */
    ri_engine_init(&E);
    ri_engine_init(&F);
    autoev(&E, RI_AUTO_ID_808(RI_CTL_808_LEVEL, RB808_SD), 37u, 2u);
    rb808_set_param(&F.s808.v[RB808_SD], RI_CTL_808_LEVEL, 37u);
    RI_ASSERT(!memcmp(&E.s808, &F.s808, sizeof E.s808), "808 SD level key == knob path on SD only");
    autoev(&E, RI_AUTO_ID_808(RI_CTL_808_TUNE, RB808_LT), 90u, 2u);
    rb808_set_param(&F.s808.v[RB808_LT], RI_CTL_808_TUNE, 90u);
    RI_ASSERT(!memcmp(&E.s808, &F.s808, sizeof E.s808), "808 LT tune key");
    autoev(&E, RI_AUTO_ID_808(RI_CTL_808_ACCENT, 0u), 99u, 2u);
    for (v = 0; v < RI_808_NSOUNDS; v++)
        rb808_set_param(&F.s808.v[v], RI_CTL_808_ACCENT, 99u);
    RI_ASSERT(!memcmp(&E.s808, &F.s808, sizeof E.s808), "808 accent key sets every voice");
    ri_engine_init(&F);
    autoev(&F, RI_AUTO_ID_808(RI_CTL_808_ACCENT, 3u), 99u, 2u); /* not a key */
    ri_engine_init(&E);
    RI_ASSERT(!memcmp(&E.s808, &F.s808, sizeof E.s808), "per-voice accent key ignored");

    /* (b) 909 per voice + hat pair */
    ri_engine_init(&E);
    ri_engine_init(&F);
    autoev(&E, RI_AUTO_ID_909(RI_CTL_909_LEVEL, RB909_CR), 20u, 3u);
    rb909_set_param(&F.s909.v[RB909_CR], RI_CTL_909_LEVEL, 20u);
    RI_ASSERT(!memcmp(&E.s909, &F.s909, sizeof E.s909), "909 CR level key");
    autoev(&E, RI_AUTO_ID_909(RI_CTL_909_DECAY, RB909_OH), 70u, 3u);
    rb909_set_param(&F.s909.v[RB909_OH], RI_CTL_909_DECAY, 70u);
    RI_ASSERT(!memcmp(&E.s909, &F.s909, sizeof E.s909), "909 OH decay key");
    autoev(&E, RI_AUTO_ID_909(RI_CTL_909_LEVEL, RI_AUTO_909_HATPAIR), 55u, 3u);
    rb909_set_hat_level(&F.s909, 55u);
    RI_ASSERT(!memcmp(&E.s909, &F.s909, sizeof E.s909), "909 hat pair key");

    /* (c) strips */
    ri_engine_init(&E);
    autoev(&E, RI_AUTO_ID_MIX(1u, RI_AUTO_MIX_PAN), 10u, 0u);
    RI_ASSERT(E.pan[1] == 10u, "303B pan key %u", E.pan[1]);
    autoev(&E, RI_AUTO_ID_MIX(3u, RI_AUTO_MIX_SEND), 77u, 0u);
    RI_ASSERT(E.send[3] == 77u, "909 send key %u", E.send[3]);
    autoev(&E, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_DIST + RI_ROUTE_PCF), 1u, 0u);
    RI_ASSERT(ri_route_owner(&E.route, RI_ROUTE_PCF) == 0, "303A pcf insert on");
    autoev(&E, RI_AUTO_ID_MIX(2u, RI_AUTO_MIX_DIST + RI_ROUTE_PCF), 0u, 0u);
    RI_ASSERT(ri_route_owner(&E.route, RI_ROUTE_PCF) == 0, "808 pcf off leaves 303A's insert");
    autoev(&E, RI_AUTO_ID_MIX(2u, RI_AUTO_MIX_DIST + RI_ROUTE_PCF), 1u, 0u);
    RI_ASSERT(ri_route_owner(&E.route, RI_ROUTE_PCF) == 2, "808 pcf on takes the radio");
    autoev(&E, RI_AUTO_ID_MIX(2u, RI_AUTO_MIX_DIST + RI_ROUTE_PCF), 0u, 0u);
    RI_ASSERT(ri_route_owner(&E.route, RI_ROUTE_PCF) == RI_ROUTE_NONE, "808 pcf off releases");
    autoev(&E, RI_AUTO_ID_MIX(RI_AUTO_STRIP_MASTER, RI_AUTO_MIX_DIST + RI_ROUTE_COMP), 127u, 0u);
    RI_ASSERT(ri_route_owner(&E.route, RI_ROUTE_COMP) == RI_ROUTE_MASTER, "master comp key");
    autoev(&E, RI_AUTO_ID_MIX(3u, RI_AUTO_MIX_LEVEL), 90u, 0u);
    RI_ASSERT(E.level[3] == 90u, "909 level key %u", E.level[3]);

    /* (d) level law + render identity */
    RI_ASSERT(ri_engine_set_level(&E, 4u, 1u) == 2 && ri_engine_set_level(0, 0u, 1u) == 2, "level bad args");
    solo(&E);
    RI_ASSERT(E.level[0] == 127u, "level default 127 (unity)");
    RI_ASSERT(ri_engine_render(&E, LA, RA, N, SR) == N, "neutral render");
    solo(&E);
    RI_ASSERT(ri_engine_set_level(&E, 0u, 127u) == 0, "set 127");
    RI_ASSERT(ri_engine_render(&E, LB, RB, N, SR) == N, "127 render");
    RI_ASSERT(!differ(LA, LB, N) && !differ(RA, RB, N), "level 127 is bit-identical to the pre-fader engine");
    solo(&E);
    RI_ASSERT(ri_engine_set_level(&E, 0u, 64u) == 0, "set 64");
    RI_ASSERT(ri_engine_render(&E, LB, RB, N, SR) == N, "64 render");
    solo(&F);
    autoev(&F, RI_AUTO_ID_MIX(0u, RI_AUTO_MIX_LEVEL), 64u, 0u);
    {
        static float LC[N], RC[N];
        uint32_t i, bad = 0u;
        float g = (64.0f / 127.0f) * (64.0f / 127.0f);
        RI_ASSERT(ri_engine_render(&F, LC, RC, N, SR) == N, "key render");
        RI_ASSERT(!differ(LB, LC, N) && !differ(RB, RC, N), "level key renders identical to the setter");
        RI_ASSERT(differ(LA, LB, N), "level 64 changes the output");
        for (i = RI_MIX_RAMP_SMP; i < 8000u; i++) /* after the slew: exact law */
            if (fabsf(LB[i] - LA[i] * g) > 1e-6f * (1.0f + fabsf(LA[i])))
                bad++;
        RI_ASSERT(bad == 0u, "steady state = (64/127)^2 x neutral (%u off)", bad);
        RI_ASSERT(fabsf(LB[0]) <= fabsf(LA[0]) + 1e-9f, "slew starts from unity");
    }
    solo(&E);
    RI_ASSERT(ri_engine_set_level(&E, 0u, 0u) == 0, "set 0");
    RI_ASSERT(ri_engine_render(&E, LB, RB, N, SR) == N, "0 render");
    {
        uint32_t i, loud = 0u;
        for (i = RI_MIX_RAMP_SMP; i < N; i++)
            if (LB[i] != 0.0f || RB[i] != 0.0f)
                loud++;
        RI_ASSERT(loud == 0u, "level 0 silences the strip after the ramp (%u)", loud);
        RI_ASSERT(LA[20] != 0.0f && LB[20] != 0.0f && fabsf(LB[20]) < fabsf(LA[20]),
                  "zipless: mid-ramp sample is attenuated, not cut");
    }

    /* (e) unknown / legacy keys ignored */
    ri_engine_init(&E);
    ri_engine_init(&F);
    autoev(&E, 0x0B00u, 5u, 0u); /* legacy Task-12 panel id */
    autoev(&E, 0x0B0Fu, 5u, 0u);
    autoev(&E, 0x0B60u, 5u, 0u); /* strip 5 (no such strip) */
    autoev(&E, 0x0B16u, 5u, 0u); /* param 6 */
    autoev(&E, 0x0E00u, 5u, 0u); /* unknown block */
    autoev(&E, RI_AUTO_ID_909(RI_CTL_909_TUNE, 12u), 5u, 3u); /* no voice 12 */
    RI_ASSERT(!memcmp(E.pan, F.pan, sizeof E.pan) && !memcmp(E.send, F.send, sizeof E.send) &&
              !memcmp(E.level, F.level, sizeof E.level) && !memcmp(&E.route, &F.route, sizeof E.route) &&
              !memcmp(&E.s808, &F.s808, sizeof E.s808) && !memcmp(&E.s909, &F.s909, sizeof E.s909),
              "unknown keys change nothing");

    RI_RESULT("auto_delivery");
}
