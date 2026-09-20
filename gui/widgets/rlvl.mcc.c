/*
 * rlvl.mcc.c — RVUm level-meter MCC shell (Task 12, gate G12).
 *
 * AROS-ONLY. Skins MUIC_Levelmeter (spec §13 widget reuse) for the
 * mixer meter tap (0..127 log, RI_EV_METER consumer-to-be); display
 * quantization reuses ri_ctl_quantize so meter labels match knob
 * readouts. Must NEVER enter the host build (see rknb.mcc.c note).
 */

#ifndef __AROS__
#error "rlvl.mcc.c is AROS-only: Zune MCC shell, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <clib/muimaster_protos.h>
#include "gui/knob_logic.h"

APTR ri_rlvl_create(void) {
    return (APTR)MUI_NewObject(MUIC_Levelmeter,
        MUIA_Numeric_Min, 0,
        MUIA_Numeric_Max, 127,
        MUIA_Numeric_Value, 0,
        TAG_DONE);
}

/* Meter float in 0..1 → quantized 0..127 display value. */
LONG ri_rlvl_display(double level01) {
    return (LONG)ri_ctl_quantize(level01 * RI_CTL_MAX);
}
