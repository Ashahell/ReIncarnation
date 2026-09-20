/*
 * rfdr.mcc.c — RFdr fader MCC shell (Task 12, gate G12).
 *
 * AROS-ONLY. Skins MUIC_Slider (spec §13 widget reuse) with the
 * 100 px full-travel law from gui/knob_logic.c (host-tested, P-18);
 * same fine/commit/capture rules as the knob. Must NEVER enter the
 * host build (see rknb.mcc.c header note; audit gates it).
 */

#ifndef __AROS__
#error "rfdr.mcc.c is AROS-only: Zune MCC shell, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <clib/muimaster_protos.h>
#include "gui/knob_logic.h"

APTR ri_rfdr_create(void) {
    return (APTR)MUI_NewObject(MUIC_Slider,
        MUIA_Numeric_Min, 0,
        MUIA_Numeric_Max, 127,
        MUIA_Numeric_Value, 127,
        TAG_DONE);
}

/* Vertical drag pixels → quantized ctl value (up-drag increases). */
LONG ri_rfdr_drag_value(LONG start, LONG dy_px, BOOL fine) {
    double v = ri_fader_drag_to_value((double)start, (double)dy_px,
                                      fine ? 1 : 0);
    return (LONG)ri_ctl_quantize(v);
}

void ri_rfdr_begin(struct RiGesture *g) {
    ri_gesture_begin(g);
}

int ri_rfdr_move(struct RiGesture *g) {
    ri_gesture_move(g);
    return 0;
}

int ri_rfdr_release(struct RiGesture *g) {
    int before = g ? g->commits : 0;
    ri_gesture_end(g);
    return (g && g->commits != before) ? 1 : 0;
}
