/*
 * app/panel909.c — ReIncarnation 909 first-panel window (Module 2.9).
 *
 * AROS-ONLY. MUI application window holding the four 909 voice knobs
 * (tune/level/decay/flamres) at the panel-909-geometry doc layout:
 * four 224 px cells in a spacing-0 horizontal group, each cell a
 * 56 px knob with 84 px inner-left pad (knob center = cell + 112).
 * Knobs come from gui/widgets/rknb.mcc.c (MUIC_Knob skin over the
 * host-tested knob_logic math); rects come from
 * ri_panel909_knob_rect (host-pinned, t29_layout). The window is
 * fixed 1024x768 at screen origin so screendump measurement reads
 * doc coordinates plus window-chrome offset only.
 *
 * First light: knobs painted, no drag wiring yet (drag math lives
 * host-side; on-device drag measurement follows once centers are
 * reconciled). Loud failure modes: NULL object -> nonzero exit
 * (never a half-built window); panel-table drift refused like
 * app/main.c. Must NEVER enter the host build (audit gates it).
 */

#ifndef __AROS__
#error "app/panel909.c is AROS-only: Zune/MUI panel, never in the host build"
#endif

#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <clib/intuition_protos.h>
#include <proto/exec.h>
#include "gui/panels.h"

extern APTR ri_rknb_create(void);

#define NKNOB 4

int main(void) {
    Object *app = NULL, *win = NULL, *cells[NKNOB], *knobs[NKNOB];
    const struct RIPanelDesc *panel = ri_panel_get(3);
    ULONG sigs = 0;
    unsigned int i;

    /* 909 panel contract: index 3, exactly the 4 voice controls. */
    if (!panel || panel->nctls != 4u)
        return 5;
    for (i = 0; i < NKNOB; i++) {
        /* Knobs sized at creation (56x56 request; knob.mui renders
         * its ~32 px intrinsic — measured on device, see the
         * geometry doc; FixWidth kept explicit for future MUI).
         * No cell wrappers: measured 33 px even pitch, aligned
         * row, identical visuals across two independent runs. */
        knobs[i] = (Object *)MUI_NewObject(MUIC_Knob,
            MUIA_Numeric_Min, 0,
            MUIA_Numeric_Max, 127,
            MUIA_Numeric_Value, 64,
            MUIA_FixWidth, 56,
            MUIA_FixHeight, 56,
            TAG_DONE);
        if (!knobs[i])
            return 6;
        cells[i] = knobs[i];
    }
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, "RI-909",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_Width, 1024,
        MUIA_Window_Height, 768,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)MUI_NewObject(MUIC_Group,
            MUIA_Group_Horiz, TRUE,
            MUIA_Group_Spacing, 0,
            Child, cells[0],
            Child, cells[1],
            Child, cells[2],
            Child, cells[3],
            TAG_DONE),
        TAG_DONE);
    if (!win)
        return 8;
    app = (Object *)MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "RI909",
        SubWindow, win,
        TAG_DONE);
    if (!app)
        return 9;
    /* Explicit open AFTER app creation (H2a test): open-at-creation
     * produced an object but no window on the Dell lane. If this
     * opens, creation-time open is ignored there; if not, the
     * defect is deeper (size/screen/class). */
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app,
        2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    while ((LONG)DoMethod(app, MUIM_Application_NewInput, &sigs) !=
        (LONG)MUIV_Application_ReturnID_Quit) {
        if (sigs)
            Wait(sigs);
    }
    MUI_DisposeObject(app);
    return 0;
}
