/* gui/widgets/rsec303.h — RSec303: whole 303 section canvas (§12.10 G4).
 * AROS-ONLY. One custom Area draws the section from the registry
 * (gui/ctlreg.h), the measured geometry (gui/panelgeo.h) and the
 * host-tested behaviour (gui/sect303.h); it owns its pixels.
 */
#ifndef RI_RSEC303_H
#define RI_RSEC303_H

#ifndef __AROS__
#error "rsec303.h is AROS-only: Zune custom class API, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include "gui/sect303.h"

struct MUI_CustomClass;

/* Read-only LONG: bumps once per state change (click, drag step, reset);
 * notify on it to refresh readouts. */
#define MUIA_RSec303_Changes (TAG_USER + 0x52533301u)
/* Read-only pointer: const struct RISect303 * of the live state. */
#define MUIA_RSec303_State (TAG_USER + 0x52533302u)

/* Read-only pointer: const struct RSec303Diag * (event plumbing proof). */
#define MUIA_RSec303_Diag (TAG_USER + 0x52533303u)
struct RSec303Diag {
    LONG events;   /* MUIM_HandleEvent calls with a message */
    LONG buttons;  /* of which IDCMP_MOUSEBUTTONS */
    LONG last_x, last_y; /* last button position, canvas-local px */
    ULONG last_hit;      /* reg_id hit, 0xFFFF none */
    LONG setups, shows;  /* lifecycle calls seen */
};

struct MUI_CustomClass *ri_rsec303_class(void);
void ri_rsec303_dispose_class(void);
/* section = RI_SEC_SYNTH1 / RI_SEC_SYNTH2; zoom 0 = 1x, 1 = 1.5x, 2 = 2x. */
APTR ri_rsec303_create(ULONG section, LONG zoom);
#endif
