/* gui/widgets/rsection.h — RSection: one canvas class for every laid-out
 * section (§12.10 G4). AROS-ONLY. Draws from the control registry
 * (gui/ctlreg.h), the measured geometry (gui/panelgeo.h) and the section
 * behaviour interface (gui/sectui.h); owns its pixels.
 */
#ifndef RI_RSECTION_H
#define RI_RSECTION_H

#ifndef __AROS__
#error "rsection.h is AROS-only: Zune custom class API, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include "gui/sectui.h"
#include "gui/panelui.h"

struct MUI_CustomClass;

/* Read-only LONG: bumps once per state change; notify on it. */
#define MUIA_RSection_Changes (TAG_USER + 0x52534301u)
/* Read-only pointer: struct RISectUI * of the live state. */
#define MUIA_RSection_State (TAG_USER + 0x52534302u)
/* Read-only pointer: const struct RSectionDiag * (event plumbing proof). */
#define MUIA_RSection_Diag (TAG_USER + 0x52534303u)

/* Settable pointer: struct RIPanelUI * shared by every canvas of a window
 * (focus bar, click-to-focus, keyboard). NULL = standalone section. */
#define MUIA_RSection_Panel (TAG_USER + 0x52534304u)
/* Settable BOOL: this canvas takes the window's raw keys for the panel
 * (exactly one canvas per window). */
#define MUIA_RSection_KeyOwner (TAG_USER + 0x52534305u)

struct RSectionDiag {
    LONG events;          /* MUIM_HandleEvent calls with a message */
    LONG buttons;         /* of which IDCMP_MOUSEBUTTONS */
    LONG last_x, last_y;  /* last button position, canvas-local px */
    ULONG last_hit;       /* reg_id hit, 0xFFFF none */
    LONG setups, shows;   /* lifecycle calls seen */
};

struct MUI_CustomClass *ri_rsection_class(void);
void ri_rsection_dispose_class(void);
/* section = RI_SEC_*; zoom 0 = 1x, 1 = 1.5x, 2 = 2x. NULL = class failed. */
APTR ri_rsection_create(ULONG section, LONG zoom);
/* Redraw after the state was changed from outside (demo / automation). */
void ri_rsection_refresh(APTR obj);
#endif
