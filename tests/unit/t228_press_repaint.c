/* t228_press_repaint — which controls need a repaint when their value
 * changes, so the repaint can be COUPLED to the change instead of patched in
 * per caller.
 *
 * WHY THIS EXISTS. Four defects this session shared one shape. Two of them
 * were "correct model, no repaint": the displaced FX lamp (P-FX4) and the
 * mixer insert rockers (P-MIX2). Both were fixed by adding a refresh call at
 * one more call site, which is how a class becomes a sequence of near-misses.
 *
 * The root cause is structural and now measured twice over:
 *   - `rsection.mcc.c` has no `Invalidate` and no self-redraw, so every
 *     repaint is an explicit `ri_rsection_refresh` from the shell;
 *   - `sync_values` -- the per-frame loop that sees EVERY panel change --
 *     pushes values but repaints nothing.
 *
 * On AROS Zune a draw request is honoured 1:1 (`mui_redraw.c` calls
 * `MUIM_Draw` inline, no coalescing), so an unrefreshed control is not
 * "eventually right": it freezes until unrelated code redraws the canvas.
 *
 * So the fix is to repaint the box of whatever changed, in `sync_values`,
 * once -- rather than at each call site that turns out to need it.
 *
 * THE DECISION THAT IS WORTH TESTING is not the box maths (that is
 * `ri_geo_bbox`, already portable and exercised) but WHICH controls should
 * get one. Getting it wrong in either direction costs something real:
 * repainting a METER box that `meter_round` already owns means two
 * invalidations per level change, and this widget counts redundant
 * invalidations (`dpr_rep`) because they were implicated in a 1.36 s stall.
 * Excluding a control that needs it reinstates exactly the freeze class this
 * is fixing.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/ctlreg.h"
#include "gui/sectfx.h"
#include "gui/sectmix.h"
#include "gui/sectlevi.h"

int main(void) {
    /* METER: the meter loops own these. meter_round and the FX meter loop
     * already refresh the meter box whenever the LEVEL changes, which happens
     * every frame and not on a press. Repainting them here too would double
     * the invalidations for no visual gain -- the exact redundancy the widget
     * counts. */
    RI_ASSERT(!ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_PCF << 8) | 1u))), "FX meter is not press-repainted");
    RI_ASSERT(!ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_MIX_LEVI << 8) | 1u))), "strip meter is not press-repainted");
    RI_ASSERT(!ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_COMP << 8) | RI_SFX_COMP_GR))), "COMP GR bar is not press-repainted");

    /* Everything a user can actually change gets one. Dropping any of these
     * reinstates a stale-pixel bug of the P-FX4 / P-MIX2 class. */
    RI_ASSERT(ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_PCF << 8) | 0u))), "FX On/Off repaints");
    RI_ASSERT(ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_MIX_SYNTH1 << 8) | RI_SMIX_PCF))), "insert rocker repaints");
    RI_ASSERT(ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_MIX_SYNTH1 << 8) | 0u))), "strip mute repaints");
    RI_ASSERT(ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_MIX_LEVI << 8) | 4u))), "delay send knob repaints");
    RI_ASSERT(ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_DELAY << 8) | 2u))), "delay Steps display repaints");
    RI_ASSERT(ri_ctl_press_repaints(ri_ctlreg_find(
            (uint16_t)((RI_SEC_LEVI << 8) | RI_SLEVI_STEP0))), "a sequencer step repaints");

    /* A knob DRAG fires the change counter continuously, so the decision to
     * repaint must not depend on how the value arrived. This is asserted as
     * the absence of any value-dependence rather than by trying to model a
     * drag: the predicate takes only the definition. */
    RI_ASSERT(ri_ctl_press_repaints(0) == 0, "null refused");
    RI_ASSERT(!ri_ctl_press_repaints(ri_ctlreg_find(0xFFFFu)), "unknown id refused");

    /* Every METER row in the registry must be excluded, not just the three
     * named above: a census catches a meter added later that would otherwise
     * start double-invalidating without anyone noticing. */
    {
        unsigned sec, idx;
        int meters = 0, leaked = 0;
        for (sec = 0; sec < 32u; sec++)
            for (idx = 0; idx < 256u; idx++) {
                const struct RICtlDef *d =
                    ri_ctlreg_find((uint16_t)((sec << 8) | idx));
                if (!d || d->kind != RI_CK_METER)
                    continue;
                meters++;
                if (ri_ctl_press_repaints(d))
                    leaked++;
            }
        RI_ASSERT(meters > 10, "registry has %d meter rows to exclude", meters);
        RI_ASSERT(leaked == 0, "no METER row may be press-repainted (%d leaked)", leaked);
    }

    RI_RESULT("press_repaint");
    return 0;
}
