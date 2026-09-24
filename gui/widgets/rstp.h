/* gui/widgets/rstp.h — RStp step-button custom class API (2.10).
 *
 * AROS-ONLY. Custom Numeric subclass with own 32 px rendering +
 * click-toggle (stock MUIC_Numericbutton ignores Fix sizes — m36).
 * Must NEVER enter the host build (audit gates it).
 */
#ifndef RI_RSTP_H
#define RI_RSTP_H

#ifndef __AROS__
#error "rstp.h is AROS-only: Zune custom class API, never in the host build"
#endif

#include <exec/types.h>
#include <libraries/mui.h>
#include "gui/knob_logic.h"

struct MUI_CustomClass;

struct MUI_CustomClass *ri_rstp_class(void);
void ri_rstp_dispose_class(void);

/* Chase playhead (BOOL): set on the current step, cleared on the
 * previous. Renders as a warm white edge; the app drives it from
 * its beat clock and self-measures lag (TC-2.10.x). */
#define MUIA_RStp_Chase (TAG_USER + 0x5253u)

/* Create one step button (0/1, initial 0). NULL = class failed. */
APTR ri_rstp_create(void);

/* Thin shells over knob_logic (host-tested math). */
LONG ri_rstp_click(LONG state);
LONG ri_rstp_chase(double beat_pos_16ths, LONG nsteps);
int ri_rstp_led_due(double lag_ms);

#endif
