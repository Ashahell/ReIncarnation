/*
 * rstp.mcc.c — RStp step-button MCC shell (Task 12, gate G12).
 *
 * AROS-ONLY. Skins MUIC_Numericbutton (spec §13 widget reuse); click
 * toggles (ri_step_toggle), LED chase follows ri_chase_step with the
 * ≤33 ms update budget (ri_led_lag_ok). 16th-note chase stutter-free
 * at 174 BPM is recorded in docs/evidence/gui/acceptance.md. Must
 * NEVER enter the host build (see rknb.mcc.c header note).
 */

#ifndef __AROS__
#error "rstp.mcc.c is AROS-only: Zune MCC shell, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <clib/muimaster_protos.h>
#include "gui/knob_logic.h"

APTR ri_rstp_create(void) {
    return (APTR)MUI_NewObject(MUIC_Numericbutton,
        MUIA_Numeric_Min, 0,
        MUIA_Numeric_Max, 1,
        MUIA_Numeric_Value, 0,
        TAG_DONE);
}

/* Click toggles the step (0 → 1 → 0). */
LONG ri_rstp_click(LONG state) {
    return (LONG)ri_step_toggle((int)state);
}

/* Chase position for a transport beat position (16ths); the caller
 * lights that step's LED iff ri_rstp_led_due reports on time. */
LONG ri_rstp_chase(double beat_pos_16ths, LONG nsteps) {
    return (LONG)ri_chase_step(beat_pos_16ths, (int)nsteps);
}

int ri_rstp_led_due(double lag_ms) {
    return ri_led_lag_ok(lag_ms);
}
