/*
 * rknb.mcc.c — RKnB knob MCC shell (Task 12, gate G12).
 *
 * AROS-ONLY. Skins MUIC_Knob (spec §13 widget reuse: Numeric → Knob,
 * min/max/step + stringify) via MCC subclassing; the VALUE MATH lives
 * in gui/knob_logic.c (host-tested, P-18) and this shell only wires
 * drag pixels to ctl values plus notify/commit events. Must NEVER
 * enter the host build: the #error below fires on any non-AROS
 * compile, scripts/ri_build_host.sh never references this file, and
 * scripts/ri_audit.sh gates both facts (probe_ahi.c precedent).
 */

#ifndef __AROS__
#error "rknb.mcc.c is AROS-only: Zune MCC shell, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <clib/muimaster_protos.h>
#include "gui/knob_logic.h"

/* Create the skinned knob: 0..127, default 64 (mid). Artwork/silhouette
 * acceptance (±2 px @1024x768) is recorded in
 * docs/evidence/gui/acceptance.md, not asserted here. */
APTR ri_rknb_create(void) {
    return (APTR)MUI_NewObject(MUIC_Knob,
        MUIA_Numeric_Min, 0,
        MUIA_Numeric_Max, 127,
        MUIA_Numeric_Value, 64,
        TAG_DONE);
}

/* Vertical drag pixels → quantized ctl value (callers negate screen-y
 * first: up-drag increases). Shift passes fine=1 (×0.1, 1500 px full). */
LONG ri_rknb_drag_value(LONG start, LONG dy_px, BOOL fine) {
    double v = ri_knob_drag_to_value((double)start, (double)dy_px,
                                     fine ? 1 : 0);
    return (LONG)ri_ctl_quantize(v);
}

/* Gesture commit wiring: moves notify (return 0 = no commit yet),
 * release commits exactly one undo unit (return 1). */
void ri_rknb_begin(struct RiGesture *g) {
    ri_gesture_begin(g);
}

int ri_rknb_move(struct RiGesture *g) {
    ri_gesture_move(g);
    return 0;
}

int ri_rknb_release(struct RiGesture *g) {
    int before = g ? g->commits : 0;
    ri_gesture_end(g);
    return (g && g->commits != before) ? 1 : 0;
}
