/* t219_fx_meters — the four FX module meters had no wire at all.
 *
 * Owner ear check 2026-10-13: "the leds top right of the panel don't work" —
 * confirmed as the METERS, not the on/off lamps. The engine has always
 * measured them: `ri_meter_feed(&e->fx_meter[RI_ENGINE_FX_*], ...)` sits
 * inside both the mono and stereo insert guards. `struct RILiveMeters` has
 * carried `float fx_peak[4]` into the app every frame.
 *
 * Nothing ever read it. `ri_sfx_meter_set` had exactly two callers in the
 * whole tree — t67_sectfx and app/sectproof.c, the self-test harness — and
 * none in riapp.c, while the mixer meters next door got exactly the
 * delivery the FX meters lacked (riapp.c:1670, :1699). The level arrived at
 * the app and was discarded; RI_SFX_METER was a slot nothing wrote.
 *
 * WHAT THIS PINS is the part that is portable and therefore host-testable:
 * the canvas -> engine-unit transpose. Canvas order is C_FX0..C_FX3 =
 * PCF, DELAY, DIST, COMP. The engine's meter units are DIST=0, PCF=1,
 * DELAY=2, COMP=3. That is a genuine permutation [1,2,0,3] — not an
 * off-by-one — and getting it wrong puts each module's level on its
 * NEIGHBOUR's meter, which looks entirely plausible on screen.
 *
 * The behavioural check below is the anti-tautology guard: DIST is engaged
 * and rendered, so exactly one meter must be lit, and it must be the DIST
 * slot. A test that only compared the map against itself would pass on a
 * transpose that had been "fixed" to identity.
 *
 * NOT pinned here: the shell half (riapp.c calling ri_sfx_meter_set and
 * repainting that box). riapp.c is an AROS shell and cannot be linked on the
 * host. That half is gated by source inspection in ri_audit.sh and, in the
 * end, by the meter actually moving on the lane.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/riapp_core.h"
#include "engine/engine.h"
#include "gui/ctlreg.h"

#define N 48000u

/* Canvas order, spelled out here rather than shared with the shell, so the
 * test fails if the shell and the core ever disagree about it. */
static const uint32_t FX_ORDER[4] = { RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP };
#define FX_PCF  0
#define FX_DLY  1
#define FX_DST  2
#define FX_CMP  3

int main(void) {
    static struct RIAppCore c;
    static float sl[N], sr[N];
    int lvl[4];
    float gr;
    uint32_t i;

    /* --- the transpose, unit by unit --- */
    RI_ASSERT(ri_core_fx_meter_unit(RI_SEC_PCF) == RI_ENGINE_FX_PCF, "PCF unit");
    RI_ASSERT(ri_core_fx_meter_unit(RI_SEC_DELAY) == RI_ENGINE_FX_DELAY, "delay unit");
    RI_ASSERT(ri_core_fx_meter_unit(RI_SEC_DIST) == RI_ENGINE_FX_DIST, "dist unit");
    RI_ASSERT(ri_core_fx_meter_unit(RI_SEC_COMP) == RI_ENGINE_FX_COMP, "comp unit");
    /* Non-FX sections have no meter: the map must refuse rather than hand
     * back unit 0, which is DIST and would light the wrong box. */
    RI_ASSERT(ri_core_fx_meter_unit(RI_SEC_LEVI) == RI_ENGINE_FX_COUNT,
        "Levi has no FX meter");
    RI_ASSERT(ri_core_fx_meter_unit(RI_SEC_MASTER) == RI_ENGINE_FX_COUNT,
        "master has no FX meter");

    /* The permutation is real. If these two ever both hold, someone has
     * replaced the map with identity and this test must notice. */
    RI_ASSERT(FX_ORDER[FX_DST] == RI_SEC_DIST, "canvas order: DIST is third");
    RI_ASSERT(ri_core_fx_meter_unit(FX_ORDER[FX_DST]) == 0u,
        "DIST is engine unit 0 while sitting third in canvas order");
    RI_ASSERT(ri_core_fx_meter_unit(FX_ORDER[FX_PCF]) != 0u,
        "PCF is NOT unit 0 -- identity would pass a self-comparison");

    /* --- behavioural: exactly the engaged module's meter lights --- */
    RI_ASSERT(ri_core_fx_meters(0, lvl, &gr) == 0, "null core refused");
    RI_ASSERT(ri_core_fx_meters(&c, 0, &gr) == 0, "null out refused");

    ri_core_init(&c, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A);
    ri_core_demo(&c);
    ri_core_play(&c);
    /* PCF owns strip 0. PCF is the discriminator, not DIST: with DIST the
     * fixture measured sec_peak[0] == fx_peak[0] == 0.32142, so reading the
     * wrong array produced byte-identical output and a mutant survived it.
     * PCF is engine unit 1, where sec_peak[1] is 0 while fx_peak[1] is
     * 0.674 -- the two arrays can only be told apart here. The meter feeds
     * inside the insert guard, so it fills even in unity bypass: no drive
     * is needed to prove the wire. */
    ri_engine_assign_insert(&c.session.eng, RI_ROUTE_PCF, 0);
    for (i = 0; i < 8u; i++)
        ri_live_render(&c.session, sl, sr, N);

    RI_ASSERT(ri_core_fx_meters(&c, lvl, &gr) == 1, "fx meters read");
    RI_ASSERT(lvl[FX_PCF] > 0, "PCF slot must carry the level (%d)", lvl[FX_PCF]);
    RI_ASSERT(lvl[FX_DST] == 0, "DIST not engaged, slot must stay dark (%d)", lvl[FX_DST]);
    RI_ASSERT(lvl[FX_DLY] == 0, "delay slot must stay dark (%d)", lvl[FX_DLY]);
    RI_ASSERT(lvl[FX_CMP] == 0, "comp slot must stay dark (%d)", lvl[FX_CMP]);

    /* Levels are the 0..127 projection the mixer meters already use, so a
     * raw peak would read as a plausible-looking number and be wrong. */
    RI_ASSERT(lvl[FX_DST] <= 127, "level in range (%d)", lvl[FX_DST]);
    RI_ASSERT(gr <= 0.0f, "comp gain reduction is <= 0 dB (%f)", gr);

    /* --- displacement must darken the displaced meter (owner 2026-10-15:
     * "PCF and Delay leds remain active/on when COMP is activated") ---
     *
     * ri_meter_feed() is measure AND decay in one call, and it sits inside
     * the fx_on render guard -- so a unit that stops being live is never fed
     * again and its peak freezes at its last level. The panel then keeps
     * publishing that frozen level and the meter stays lit on a module that
     * is audibly off. Engaging COMP here must therefore darken PCF, not by
     * waiting out a decay that will never run, but at the transition. */
    ri_engine_fx_set(&c.session.eng, RI_FXID_COMP_ENABLE, 1u);
    for (i = 0; i < 8u; i++)
        ri_live_render(&c.session, sl, sr, N);
    RI_ASSERT(ri_core_fx_meters(&c, lvl, &gr) == 1, "fx meters re-read");
    RI_ASSERT(lvl[FX_PCF] == 0, "displaced PCF slot must go dark (%d)",
        lvl[FX_PCF]);
    RI_ASSERT(ri_engine_fx_peak(&c.session.eng, RI_ENGINE_FX_PCF) == 0.0f,
        "displaced PCF engine peak must be zero, not held (%f)",
        (double)ri_engine_fx_peak(&c.session.eng, RI_ENGINE_FX_PCF));

    /* --- OFF must darken its own meter for the same reason --- */
    ri_engine_fx_set(&c.session.eng, RI_FXID_PCF_ENABLE, 1u);
    for (i = 0; i < 8u; i++)
        ri_live_render(&c.session, sl, sr, N);
    RI_ASSERT(ri_core_fx_meters(&c, lvl, &gr) == 1, "fx meters re-read");
    RI_ASSERT(lvl[FX_PCF] > 0, "re-engaged PCF lights again (%d)", lvl[FX_PCF]);
    ri_engine_fx_set(&c.session.eng, RI_FXID_PCF_ENABLE, 0u);
    for (i = 0; i < 8u; i++)
        ri_live_render(&c.session, sl, sr, N);
    RI_ASSERT(ri_core_fx_meters(&c, lvl, &gr) == 1, "fx meters re-read");
    RI_ASSERT(lvl[FX_PCF] == 0, "switched-off PCF slot must go dark (%d)",
        lvl[FX_PCF]);

    ri_core_stop(&c);
    RI_RESULT("fxmeters");
}