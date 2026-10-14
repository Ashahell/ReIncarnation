/* t222_fx_exclusive — the FX rack is single-active, and engaging it works.
 *
 * Owner ear proof 2026-10-14, after deploying the keybed build:
 *
 *   "Delay and DIST work, PCF and COMP don't. Nothing seems to be wired up
 *    for the latter two."
 *   "With PCF turned on, the meter there doesn't work. ... moving any of the
 *    sliders makes any audible difference. In Comp. the meter doesn't work.
 *    Turning the knobs makes no audible difference"
 *
 * One cause explains all of it. The render guard is
 *
 *   if ((mask & (1u << RI_ROUTE_PCF)) && e->fx_on[RI_ENGINE_FX_PCF])
 *
 * and `mask` comes from the routing table, whose only writer reachable from
 * the UI is the MIXER's per-strip insert switches. Turning on PCF from its
 * canvas set fx_on[PCF] and assigned no route, so the block never ran: no
 * sound, and -- because ri_meter_feed() sits INSIDE that same block -- no
 * meter either. PCF and Comp were dead in both respects while Dist was alive
 * in both, which is the proof: one gate, not three faults. The meters were
 * not broken; they were faithfully reporting an FX that was not running.
 *
 * Owner ruling 2026-10-14: "we need to make this as intuitive as possible.
 * If the user turns on an FX panel that conflicts with another, that other
 * one should be turned off and the newly activated FX panel should become
 * active."
 *
 * Two halves, and the second is the half that was missing:
 *   1. MUTUALLY EXCLUSIVE, last-on-wins. The lamp of the displaced module
 *      must go dark, or the panel lies -- the same class of defect as a
 *      switch that looks live and does nothing.
 *   2. "BECOME ACTIVE" means audible. Engaging must assign the insert, not
 *      merely un-bypass it.
 *
 * WHY THIS TEST CHECKS STATE AND NOT RENDERED AUDIO. t218 already pins
 * (insert owned AND fx_on) -> audible, bit-exactly, mono and stereo. t222
 * pins (enable) -> (insert owned AND fx_on). The owner's symptom is the
 * composition of the two, and both halves are pinned; re-deriving the render
 * fixture here would test the same thing a third time at three times the
 * cost. The one thing state CANNOT express -- that the engaged owner is a
 * strip that actually renders -- is covered by asserting the owner is
 * RI_FX_ENGAGE_OWNER and that it is in range.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectfx.h"
#include "gui/sectui.h"
#include "engine/engine.h"
#include "engine/fx/fx.h"
#include "engine/fx/route.h"

/* The owner's symptom, restated as the smallest falsifiable claim: after a
 * panel engage, the module the owner turned on has BOTH halves of the guard
 * satisfied. Anything less and it is silent and its meter is dark. */
static void assert_engaged(struct RIEngine *e, uint32_t mod, int unit,
    const char *what) {
    uint32_t owner;
    RI_ASSERT(e->fx_on[mod] == 1u, "%s: rack gate open", what);
    owner = (uint32_t)ri_route_owner(&e->route, (uint32_t)unit);
    RI_ASSERT(owner != (uint32_t)RI_ROUTE_NONE, "%s: insert routed", what);
    RI_ASSERT(owner < RI_ROUTE_NSECTIONS,
        "%s: owner %u is a rendering section", what, owner);
}

int main(void) {
    struct RIEngine e;
    struct RIFxRack rack;
    struct RISectUI pcf, dly, dst, cmp, unbound;
    int live, i;
    struct RISectUI *all[4];

    /* ---------- PANEL: mutual exclusion, last-on-wins ---------- */
    ri_sfx_rack_init(&rack);
    ri_sui_init(&pcf, RI_SEC_PCF);
    ri_sui_init(&dly, RI_SEC_DELAY);
    ri_sui_init(&dst, RI_SEC_DIST);
    ri_sui_init(&cmp, RI_SEC_COMP);
    all[0] = &pcf; all[1] = &dly; all[2] = &dst; all[3] = &cmp;
    RI_ASSERT(ri_sui_bind_fx_rack(&pcf, &rack) == 0, "bind PCF");
    RI_ASSERT(ri_sui_bind_fx_rack(&dly, &rack) == 0, "bind Delay");
    RI_ASSERT(ri_sui_bind_fx_rack(&dst, &rack) == 0, "bind Dist");
    RI_ASSERT(ri_sui_bind_fx_rack(&cmp, &rack) == 0, "bind Comp");
    /* Fail-closed on the wrong kinds of argument, like every other bind. */
    RI_ASSERT(ri_sui_bind_fx_rack(&pcf, 0) == 2, "null rack refused");
    RI_ASSERT(ri_sui_bind_fx_rack(0, &rack) == 2, "null section refused");

    RI_ASSERT(ri_sui_value(&pcf, RI_SFX_ONOFF) == 0
        && ri_sui_value(&dly, RI_SFX_ONOFF) == 0
        && ri_sui_value(&dst, RI_SFX_ONOFF) == 0
        && ri_sui_value(&cmp, RI_SFX_ONOFF) == 0,
        "nothing is live at rest");

    RI_ASSERT(ri_sui_press(&pcf, RI_SFX_ONOFF) == 1, "press PCF");
    RI_ASSERT(ri_sui_value(&pcf, RI_SFX_ONOFF) == 1, "PCF lit");
    live = 0;
    for (i = 0; i < 4; i++) live += ri_sui_value(all[i], RI_SFX_ONOFF);
    RI_ASSERT(live == 1, "exactly one module live after one press (%d)", live);

    /* The displacement: this is the half that keeps the panel honest. */
    RI_ASSERT(ri_sui_press(&dst, RI_SFX_ONOFF) == 1, "press Dist");
    RI_ASSERT(ri_sui_value(&pcf, RI_SFX_ONOFF) == 0,
        "PCF lamp went dark when Dist engaged");
    RI_ASSERT(ri_sui_value(&dst, RI_SFX_ONOFF) == 1, "Dist lit");
    live = 0;
    for (i = 0; i < 4; i++) live += ri_sui_value(all[i], RI_SFX_ONOFF);
    RI_ASSERT(live == 1, "still exactly one live (%d)", live);

    /* OFF is not symmetric with ON: it clears itself and disturbs nobody.
     * The asymmetry is what makes it feel right, so it is pinned. */
    RI_ASSERT(ri_sui_press(&dst, RI_SFX_ONOFF) == 1, "press Dist again");
    RI_ASSERT(ri_sui_value(&dst, RI_SFX_ONOFF) == 0, "Dist dark");
    RI_ASSERT(ri_sui_value(&pcf, RI_SFX_ONOFF) == 0,
        "switching off must NOT bring PCF back -- the displaced module stays "
        "displaced");
    RI_ASSERT(ri_sui_press(&cmp, RI_SFX_ONOFF) == 1, "press Comp");
    RI_ASSERT(ri_sui_value(&cmp, RI_SFX_ONOFF) == 1, "Comp lit");

    /* A canvas with no rack bound must not silently keep private state and
     * steal the selection. This is the negative control: without it the
     * cheapest implementation -- each canvas toggling its own val[0] and
     * "mostly" agreeing -- passes every assertion above. */
    ri_sui_init(&unbound, RI_SEC_PCF);
    RI_ASSERT(ri_sui_value(&unbound, RI_SFX_ONOFF) == 0, "unbound reads off");
    RI_ASSERT(ri_sui_press(&unbound, RI_SFX_ONOFF) == 1, "unbound press handled");
    RI_ASSERT(ri_sui_value(&unbound, RI_SFX_ONOFF) == 0,
        "an unbound canvas cannot light itself");
    RI_ASSERT(ri_sui_value(&cmp, RI_SFX_ONOFF) == 1,
        "an unbound canvas cannot steal the selection");
    /* The storage itself must stay clean, not merely read clean. This is not
     * a white-box assertion for its own sake: app/sectproof.c serialises
     * f->u.fx.val[0] DIRECTLY, bypassing ri_sfx_value, so a press that
     * quietly wrote a private 1 would be reported there as an enabled FX.
     * mutate_fx_exclusive m7 survives the reads above and dies on this one
     * -- the harness found it, not me. */
    RI_ASSERT(unbound.u.fx.val[RI_SFX_ONOFF] == 0,
        "an unbound canvas writes nothing (sectproof serialises this slot)");

    /* ---------- ENGINE: exclusivity + engage ---------- */
    ri_engine_init(&e);
    ri_engine_defaults(&e);

    /* The owner's case, verbatim: PCF on, nothing had ever owned the insert. */
    ri_engine_fx_set(&e, RI_FXID_PCF_ENABLE, 1u);
    assert_engaged(&e, RI_ENGINE_FX_PCF, (int)RI_ROUTE_PCF, "PCF engaged");
    RI_ASSERT(ri_route_owner(&e.route, RI_ROUTE_PCF) == RI_FX_ENGAGE_OWNER,
        "an unowned insert engages on the default strip (%d)",
        ri_route_owner(&e.route, RI_ROUTE_PCF));

    /* The same for Comp, the owner's other dead panel. */
    ri_engine_fx_set(&e, RI_FXID_COMP_ENABLE, 1u);
    assert_engaged(&e, RI_ENGINE_FX_COMP, (int)RI_ROUTE_COMP, "Comp engaged");
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_PCF] == 0u, "PCF displaced by Comp");
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_COMP] == 1u, "Comp live");

    /* Engage must NOT steal an insert somebody routed deliberately. This is
     * the correction to the design I first proposed and got wrong: I said
     * "engage on every strip", which route.h cannot represent -- owner[] is
     * one int8_t per unit, so a unit has at most ONE owner. */
    ri_engine_assign_insert(&e, RI_ROUTE_PCF, 1);
    ri_engine_fx_set(&e, RI_FXID_PCF_ENABLE, 1u);
    RI_ASSERT(ri_route_owner(&e.route, RI_ROUTE_PCF) == 1,
        "an insert already owned by another strip is left alone");
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_PCF] == 1u && e.fx_on[RI_ENGINE_FX_COMP] == 0u,
        "PCF takes the rack from Comp");

    /* OFF clears itself and nobody else. */
    ri_engine_fx_set(&e, RI_FXID_PCF_ENABLE, 0u);
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_PCF] == 0u, "PCF off");
    RI_ASSERT(ri_route_owner(&e.route, RI_ROUTE_PCF) == 1,
        "switching off must not unroute the insert either");
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_COMP] == 0u,
        "switching off must not resurrect the displaced module");

    /* Delay is NOT an insert. It is gated by e->dline alone (engine.c), so
     * engaging it must not invent a route -- there is no Delay unit in
     * RI_ROUTE_NUNITS. If a mutant made Delay claim an insert it would be
     * stealing a unit that belongs to Dist/PCF/Comp. */
    ri_engine_fx_set(&e, RI_FXID_DELAY_ENABLE, 1u);
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_DELAY] == 1u, "Delay live");
    RI_ASSERT(e.fx_on[RI_ENGINE_FX_PCF] == 0u, "Delay takes the rack");
    RI_ASSERT(ri_route_owner(&e.route, RI_ROUTE_DIST) == RI_ROUTE_NONE,
        "Delay must not assign any insert route");

    /* Whatever the rack ends on, exactly one member may be live. Asserted as
     * a sum over the whole array so a mutant that zeroes everything (a real
     * possibility when "exclusive" is implemented as "clear the array") is
     * caught -- clearing all four satisfies "no two are live". */
    live = 0;
    for (i = 0; i < (int)RI_ENGINE_FX_COUNT; i++) live += (int)e.fx_on[i];
    RI_ASSERT(live == 1, "engine rack holds exactly one live module (%d)", live);

    RI_RESULT("fx_exclusive");
    return 0;
}
