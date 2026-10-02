/* t154_box_reason_capture — the reason must be read BEFORE the damage box
 * is cleared, and the unattributed bucket must be visible.
 *
 * The Dell A,B,B,A (2026-10-02) is what caught this: every `box_*` field in
 * the draw line read 0/0/0 across four runs, in an arm whose whole purpose was
 * to attribute repaints. The cause was a two-line ordering bug —
 *
 *     d->dmg_why = RI_RSEC_BOX_NONE;      <- cleared here
 *     ...
 *     int why = d->dmg_why;                <- read here, always NONE
 *
 * so every sample landed in NONE, and NONE was not one of the three buckets
 * the heartbeat printed. Two independent failures that cancelled: a reason
 * destroyed before use, and a bucket nobody printed. Either alone would have
 * shown zeros; together they looked like "no box repaints happened", which is
 * the one conclusion the run could not have drawn correctly.
 *
 * This is a pure test of the capture discipline, mirrored because the real
 * draw_frame() is AROS-only. The invariant it pins is the order of operations
 * and the completeness of the reporting set — not the widget.
 *
 * Laws:
 *  - the reason survives being read before the box is invalidated, and is
 *    gone after: a cleared box carries no reason;
 *  - a captured reason is the one the CALLER set, not a default;
 *  - the reported set covers every reason code, including NONE, so a sample
 *    can never be recorded into a bucket that is not printed;
 *  - the printed set and the code range are the same set (4 == 4), which is
 *    the property that failed;
 *  - a caller that sets no reason leaves NONE visible rather than silently
 *    inheriting the previous caller.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/panelui.h"

/* Mirrors RSectionData's damage-box fields: the box and its reason. */
struct dmg {
    int valid;
    int why;
};

static void set_box(struct dmg *d, int why) {
    d->valid = 1;
    d->why = ri_rsection_box_why(why);
}

/* Mirrors the draw path: capture the reason, THEN clear, exactly as the fix
 * does. Returns the reason that should have been attributed. */
static int draw_frame_capture(struct dmg *d) {
    int why = d->why;
    d->why = RI_RSEC_BOX_NONE;
    d->valid = 0;
    return why;
}

int main(void) {
    struct dmg d;

    /* Every reason code is in the reporting set. 4 codes, 4 printed buckets;
     * this is the assertion whose absence let the bug through. */
    RI_ASSERT(RI_RSEC_BOX_COUNT == 4, "four reason codes (%d)", RI_RSEC_BOX_COUNT);
    RI_ASSERT(RI_RSEC_BOX_COUNT == RI_RSEC_BOX_OTHER + 1,
        "the reported set spans NONE..OTHER inclusively");

    /* A captured reason is the caller's, not the default. */
    memset(&d, 0, sizeof d);
    set_box(&d, RI_RSEC_BOX_STEPS);
    RI_ASSERT(d.why == RI_RSEC_BOX_STEPS, "the caller's reason is stored");
    RI_ASSERT(draw_frame_capture(&d) == RI_RSEC_BOX_STEPS,
        "STEPS survives the capture (%d)", draw_frame_capture(&d));

    memset(&d, 0, sizeof d);
    set_box(&d, RI_RSEC_BOX_BAR);
    RI_ASSERT(draw_frame_capture(&d) == RI_RSEC_BOX_BAR,
        "BAR survives the capture (%d)", draw_frame_capture(&d));

    /* The capture clears the box: a later draw sees no reason at all, which
     * is what stops one caller's reason bleeding into the next. */
    memset(&d, 0, sizeof d);
    set_box(&d, RI_RSEC_BOX_BAR);
    RI_ASSERT(draw_frame_capture(&d) == RI_RSEC_BOX_BAR, "first capture");
    RI_ASSERT(d.valid == 0, "the box is invalidated");
    RI_ASSERT(d.why == RI_RSEC_BOX_NONE, "the reason is cleared");

    /* The bug's exact signature: reading the reason AFTER the clear always
     * yields NONE, for every reason a caller can legitimately pass. */
    {
        int codes[RI_RSEC_BOX_COUNT];
        int i;
        codes[0] = RI_RSEC_BOX_NONE;
        codes[1] = RI_RSEC_BOX_STEPS;
        codes[2] = RI_RSEC_BOX_BAR;
        codes[3] = RI_RSEC_BOX_OTHER;
        for (i = 0; i < RI_RSEC_BOX_COUNT; i++) {
            memset(&d, 0, sizeof d);
            set_box(&d, codes[i]);
            d.why = RI_RSEC_BOX_NONE;      /* the old, wrong order */
            RI_ASSERT(d.why != codes[i] || codes[i] == RI_RSEC_BOX_NONE,
                "reading after the clear cannot recover code %d", codes[i]);
            memset(&d, 0, sizeof d);
            set_box(&d, codes[i]);
            RI_ASSERT(draw_frame_capture(&d) == codes[i],
                "reading before the clear recovers code %d", codes[i]);
        }
    }

    /* A caller that sets no reason leaves NONE visible, not inherited. */
    memset(&d, 0, sizeof d);
    set_box(&d, RI_RSEC_BOX_OTHER);
    (void)draw_frame_capture(&d);
    set_box(&d, RI_RSEC_BOX_NONE); /* explicit: unattributed */
    RI_ASSERT(draw_frame_capture(&d) == RI_RSEC_BOX_NONE,
        "an explicit unattributed box stays NONE");

    /* An out-of-range reason folds onto OTHER and is still attributable. */
    memset(&d, 0, sizeof d);
    set_box(&d, 4242);
    RI_ASSERT(d.why == RI_RSEC_BOX_OTHER, "junk folds to OTHER");
    RI_ASSERT(draw_frame_capture(&d) == RI_RSEC_BOX_OTHER,
        "and the fold survives the capture");

    RI_RESULT("box_reason_capture");
}