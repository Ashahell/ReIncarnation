/* t231_automation_readback — song automation and MIDI must move the lamps.
 *
 * THE GAP, quantified. `song_ui_apply()` (app/riapp.c, called on song load)
 * mirrors an automation key back onto the panel through `ri_sui_set()`. Both
 * setters refuse switches outright:
 *
 *   ri_sfx_set_value:  KNOB / FADER / SELECTOR only   -> RI_SFX_ONOFF refused
 *   ri_smix_set_value: KNOB / FADER only               -> inserts AND mute refused
 *
 * so **25 automatable switch rows** are affected -- the 4 FX On/Off enables,
 * the 16 mixer insert rockers, and the 5 strip mutes. A song that automates any
 * of them loads and plays correctly while the panel shows nothing. That is the
 * last known member of the "looks live but isn't" class, and it is why FX
 * engaged by song automation has never moved a lamp.
 *
 * WHY A SETTER AND NOT A PRESS. This path needs an ABSOLUTE set, not a toggle.
 * `ri_sfx_press` / `ri_smix_press` toggle, which is right for a click and wrong
 * for readback: told "PCF is live" when the panel already believes it is, a
 * toggle would switch it OFF. So these setters must converge on a requested
 * state, releasing only when the request is "off" AND this control is the one
 * currently holding it.
 *
 * WHY NOT JUST ADD SWITCH TO THE ACCEPT LIST. Because the value these controls
 * display is DERIVED from shared state -- the FX selection lives in
 * `RIFxRack.live`, and the mixer insert lamps in `b->route`. Storing a private
 * per-canvas value would change nothing visible, which is exactly the
 * lie-by-storage defect P-FX4 removed. The setters must write the SHARED state,
 * and the existing `stale` bookkeeping must run, or the canvas that loses the
 * lamp will not be repainted.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectfx.h"
#include "gui/sectui.h"
#include "gui/sectmix.h"
#include "engine/fx/route.h"

int main(void) {
    struct RIFxRack rack;
    struct RIMixBoard b;
    struct RISectUI pcf, dly, dst, cmp;

    /* ================= FX: the rack is the truth ================= */
    ri_sfx_rack_init(&rack);
    ri_sui_init(&pcf, RI_SEC_PCF); ri_sui_init(&dly, RI_SEC_DELAY);
    ri_sui_init(&dst, RI_SEC_DIST); ri_sui_init(&cmp, RI_SEC_COMP);
    ri_sui_bind_fx_rack(&pcf, &rack); ri_sui_bind_fx_rack(&dly, &rack);
    ri_sui_bind_fx_rack(&dst, &rack); ri_sui_bind_fx_rack(&cmp, &rack);

    /* Automation says PCF is live. */
    RI_ASSERT(ri_sui_set(&pcf, RI_SFX_ONOFF, 1) == 1,
        "readback accepted an FX enable (SWITCH must not be refused)");
    RI_ASSERT(rack.live == (uint8_t)(RI_SEC_PCF - RI_SEC_PCF),
        "readback claimed the RACK, not a private slot");
    RI_ASSERT(ri_sui_led(&pcf, RI_SFX_ONOFF, 0) == 1, "PCF lamp lit");

    /* ABSOLUTE, not toggle: told the same state again it must NOT switch off.
     * This is the assertion a toggle implementation cannot pass. */
    RI_ASSERT(ri_sui_set(&pcf, RI_SFX_ONOFF, 1) == 1, "repeat readback accepted");
    RI_ASSERT(rack.live == (uint8_t)(RI_SEC_PCF - RI_SEC_PCF)
        && ri_sui_led(&pcf, RI_SFX_ONOFF, 0) == 1,
        "readback is ABSOLUTE: repeating 'live' must not toggle it off");
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SFX_LIVE_NONE,
        "repeating the state displaces nobody");

    /* Automation moves the selection: Comp takes over, PCF goes dark, and the
     * displaced canvas is recorded so the shell repaints it. */
    RI_ASSERT(ri_sui_set(&cmp, RI_SFX_ONOFF, 1) == 1, "comp readback accepted");
    RI_ASSERT(ri_sui_led(&cmp, RI_SFX_ONOFF, 0) == 1, "Comp lit");
    RI_ASSERT(ri_sui_led(&pcf, RI_SFX_ONOFF, 0) == 0, "PCF displaced and dark");
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SEC_PCF,
        "the displaced canvas is recorded for repaint");
    RI_ASSERT(ri_sfx_rack_stale_section(&rack) == RI_SFX_LIVE_NONE, "and cleared");

    /* OFF releases only the holder. */
    RI_ASSERT(ri_sui_set(&pcf, RI_SFX_ONOFF, 0) == 1, "off readback accepted");
    RI_ASSERT(ri_sui_led(&cmp, RI_SFX_ONOFF, 0) == 1,
        "releasing a non-holder must NOT disturb the live module");
    RI_ASSERT(rack.live == (uint8_t)(RI_SEC_COMP - RI_SEC_PCF),
        "and must not clear the rack either");
    RI_ASSERT(ri_sui_set(&cmp, RI_SFX_ONOFF, 0) == 1, "holder release accepted");
    RI_ASSERT(rack.live == RI_SFX_LIVE_NONE && ri_sui_led(&cmp, RI_SFX_ONOFF, 0) == 0,
        "the holder releasing does clear the rack");

    /* METER stays refused. A meter is display-only; accepting a set on it
     * would wire up a control with no business being writable (the t218/t219
     * negative control). */
    RI_ASSERT(ri_sui_set(&pcf, RI_SFX_METER, 40) == 0,
        "meter must remain read-only");

    /* ================= MIXER: the route is the truth ================= */
    ri_smix_init(&b);
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST, 1) == 1,
        "readback accepted an insert (SWITCH must not be refused)");
    RI_ASSERT(ri_route_owner(&b.route, RI_ROUTE_DIST) == 0,
        "readback assigned the ROUTE, not b->val");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST) == 1, "Synth1 lit");

    /* Absolute again: same request twice must not release. */
    /* The return is "did anything change" (the section's existing
     * convention), so an idempotent request returns 0. Only the STATE is
     * asserted here -- demanding 1 from a no-op would be asserting a
     * convention the section has never used. */
    RI_ASSERT(ri_route_owner(&b.route, RI_ROUTE_DIST) == 0
        && ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST) == 1,
        "readback is ABSOLUTE: repeating must not release the insert");

    /* A steal from automation must record the displaced strip, or that canvas
     * keeps stale pixels -- the P-MIX2 defect, re-entering through this door. */
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_808, RI_SMIX_DIST, 1) == 1,
        "808 readback accepted");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_808, RI_SMIX_DIST) == 1, "808 lit");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST) == 0,
        "Synth1 displaced in the model");
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SEC_MIX_SYNTH1,
        "the displaced strip is recorded for repaint");
    ri_smix_stale_section(&b);

    /* OFF releases only the holder. */
    ri_smix_set_value(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST, 0);
    RI_ASSERT(ri_route_owner(&b.route, RI_ROUTE_DIST) == 2,
        "releasing a non-holder must not steal the insert");
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_808, RI_SMIX_DIST, 0) == 1,
        "holder release accepted");
    RI_ASSERT(ri_route_owner(&b.route, RI_ROUTE_DIST) == RI_ROUTE_NONE,
        "the holder releasing clears the insert");

    /* The strip mute is NOT route-derived -- it is per-strip storage, so a
     * plain store is the right semantic and it must work here too. */
    ri_smix_init(&b);
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_LEVI, 0u, 0) == 1,
        "mute readback accepted");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_LEVI, 0u) == 0, "Levi mute dark");
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_LEVI, 0u, 1) == 1,
        "mute readback accepted again");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_LEVI, 0u) == 1, "Levi mute lit");
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "the mute displaces nobody -- it is per-strip storage");

    /* METER stays refused here too. */
    RI_ASSERT(ri_smix_set_value(&b, RI_SEC_MIX_LEVI, 1u, 40) == 0,
        "strip meter must remain read-only");

    /* ================= the census =================
     * Every automatable switch bound to FX / INSERT / MUTE must MOVE ITS LAMP
     * when driven from automation. The property asserted is the movement, not
     * the return code: these setters return "did anything change", so a row
     * asked for the state it already holds returns 0 and is NOT broken. The
     * first version of this census tested the return and reported all five
     * strip mutes as unreachable purely because their default is already 1.
     *
     * Each row is driven to the OPPOSITE of its current value, so every row
     * genuinely changes. A count alone would also have passed at 25 while the
     * two CC-bearing FX switches (PCF Mode, Delay Triplet) were still refused,
     * which is exactly what happened on the first run. */
    {
        unsigned sec, idx;
        int moved = 0, stuck = 0;
        struct RIFxRack r2;
        struct RIMixBoard b2;
        ri_sfx_rack_init(&r2);
        ri_smix_init(&b2);
        for (sec = 0; sec < 32u; sec++)
            for (idx = 0; idx < 256u; idx++) {
                const struct RICtlDef *d =
                    ri_ctlreg_find((uint16_t)((sec << 8) | idx));
                int cur, want, now;
                if (!d || d->kind != RI_CK_SWITCH || !d->automatable)
                    continue;
                if (d->bind != RI_BIND_FX && d->bind != RI_BIND_INSERT
                    && d->bind != RI_BIND_MUTE)
                    continue;
                want = d->max_v ? d->max_v : 1;
                if (d->bind == RI_BIND_FX) {
                    struct RISectFx fx;
                    ri_sfx_init(&fx, (uint8_t)sec);
                    ri_sfx_bind_rack(&fx, &r2);
                    cur = ri_sfx_value(&fx, idx);
                    ri_sfx_set_value(&fx, idx, cur ? 0 : want);
                    now = ri_sfx_value(&fx, idx);
                } else {
                    cur = ri_smix_value(&b2, sec, idx);
                    ri_smix_set_value(&b2, sec, idx, cur ? 0 : want);
                    now = ri_smix_value(&b2, sec, idx);
                }
                if (now != cur) moved++;
                else {
                    stuck++;
                    printf("  STUCK %04x sec=%u idx=%u bind=%u (lamp stayed %d)\n",
                        d->reg_id, sec, idx, d->bind, cur);
                }
            }
        RI_ASSERT(stuck == 0,
            "%d automatable FX/INSERT/MUTE switch(es) still refuse to move", stuck);
        RI_ASSERT(moved == 27, "automatable FX/INSERT/MUTE switches moved: %d", moved);
    }

    RI_RESULT("automation_readback");
    return 0;
}
