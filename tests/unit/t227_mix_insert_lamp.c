/* t227_mix_insert_lamp — the mixer insert rockers need to be told to redraw.
 *
 * Owner ear proof, mixer, 2026-10-14:
 *
 *   "various buttons like comp, PCF and dist light up when pressed once, but
 *    do not turn off when pressed a second time"
 *
 * THE MODEL IS CORRECT, and the lane log proves it rather than assuming it.
 * Reconstructing the value sequences from the Dell session:
 *
 *   0605: 1 0    808 Dist     toggles OFF correctly
 *   1405: 1 0    Levi Dist    toggles OFF correctly
 *   0405: 1 1    Synth1 Dist  1 then 1 again -- a radio STEAL, not a stuck
 *                          switch: Synth1 owned it, 808 took it, Synth1 took
 *                          it back. ri_smix_led derives the lamp from
 *                          b->route, and t73 already pins "808 PCF steals it".
 *
 * So every value is right and the LAMP is the thing that is wrong.
 *
 * WHY. Two facts, both verified in code this session:
 *
 *  1. `rsection.mcc.c` contains no Invalidate and no self-redraw. Every
 *     repaint is an explicit `ri_rsection_refresh` from the shell.
 *  2. `sync_values` -- the per-frame loop that runs on every panel change --
 *     PUSHES values but repaints nothing except (P-FX4) the displaced FX
 *     lamp. There is no mixer equivalent.
 *
 * And on AROS Zune, requests and draws are 1:1 (rsection.mcc.c records this
 * explicitly: mui_redraw.c calls MUIM_Draw inline, no coalescing). So a control
 * that nobody refreshes is not "eventually right", it stays visually frozen
 * until some UNRELATED code path happens to refresh the whole canvas -- which
 * is exactly why the rockers appear to light up: the mixer meter loop
 * redraws the canvas whenever a strip level changes, and the rocker picks up
 * its current value as a side effect. Press again, and nothing refreshes, so
 * the lamp freezes.
 *
 * THIS IS THE P-FX4 SHAPE AGAIN, on the other shared-state mechanism. The
 * decision must be portable: app/riapp.c cannot link on the host, which is
 * how mutate_fx_lamp m6 survived its first run.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectmix.h"
#include "engine/fx/route.h"

int main(void) {
    struct RIMixBoard b;
    uint32_t s;
    uint16_t rD, rP, rC;

    /* ---- the existing behaviour this test must not break ---- */
    ri_smix_init(&b);
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF) == 0,
        "nothing lit at rest");
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF) == 1, "press");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF) == 1, "lit");
    /* Second press on the SAME strip releases: this is the owner's exact
     * complaint, and it is the case that must repaint the pressed canvas. */
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF) == 1, "press again");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF) == 0,
        "a second press on the same strip turns it off");

    /* ---- WHO ELSE OWNS THE BOARD ----
     * The displaced strip is on a DIFFERENT canvas, which is the half that
     * needs telling: it gets no event of its own, exactly like P-FX4. */
    ri_smix_init(&b);
    ri_smix_press(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST);
    ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_DIST);
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_808, RI_SMIX_DIST) == 1, "808 took it");
    RI_ASSERT(ri_smix_led(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST) == 0,
        "Synth1 lost it in the model");

    ri_smix_init(&b);
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "nothing to repaint before any press");
    ri_smix_press(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST);
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "a FIRST claim displaces nobody: the pressed canvas is the only one "
        "that needs repainting, and the shell already knows which it pressed");
    RI_ASSERT(ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_DIST) == 1, "808 steals");
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SEC_MIX_SYNTH1,
        "the STEAL must name the strip that lost the lamp");
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "reading clears it, so the repaint happens once and not every frame");

    /* Releasing a strip you own displaces nobody: it is the pressed canvas
     * that changes, and no other lamp moved. */
    ri_smix_init(&b);
    ri_smix_press(&b, RI_SEC_MIX_LEVI, RI_SMIX_COMP);
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE, "claim: nobody");
    ri_smix_press(&b, RI_SEC_MIX_LEVI, RI_SMIX_COMP);
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "release displaces nobody -- OFF is not the mirror of ON (t222)");

    /* A stale section must always be a real laid-out canvas: the shell turns
     * it into a canvas index and a geometry lookup. */
    ri_smix_init(&b);
    ri_smix_press(&b, RI_SEC_MIX_SYNTH2, RI_SMIX_DIST);
    ri_smix_press(&b, RI_SEC_MIX_909, RI_SMIX_DIST);
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SEC_MIX_SYNTH2, "Synth2 displaced");
    RI_ASSERT(ri_smix_stale_section(0) == RI_SMX_STALE_NONE, "null board");
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "already cleared");

    /* Each unit is tracked independently: stealing PCF must not make the
     * board think Dist was stolen too. */
    ri_smix_init(&b);
    ri_smix_press(&b, RI_SEC_MIX_808, RI_SMIX_PCF);
    ri_smix_stale_section(&b);
    ri_smix_press(&b, RI_SEC_MIX_SYNTH1, RI_SMIX_DIST);
    RI_ASSERT(ri_smix_stale_section(&b) == RI_SMX_STALE_NONE,
        "an unowned unit steals from nobody");

    /* ---- the rockers the owner named must exist and be inserts ---- */
    rD = (uint16_t)((RI_SEC_MIX_LEVI << 8) | RI_SMIX_DIST);
    rP = (uint16_t)((RI_SEC_MIX_LEVI << 8) | RI_SMIX_PCF);
    rC = (uint16_t)((RI_SEC_MIX_LEVI << 8) | RI_SMIX_COMP);
    for (s = 0; s < 3u; s++) {
        uint16_t id = (uint16_t)(s == 0 ? rD : s == 1 ? rP : rC);
        const struct RICtlDef *d = ri_ctlreg_find(id);
        RI_ASSERT(d && d->kind == RI_CK_SWITCH, "rocker %04x is a switch", id);
        RI_ASSERT(d && d->bind == RI_BIND_INSERT, "rocker %04x is an insert", id);
    }

    RI_RESULT("mix_insert_lamp");
    return 0;
}
