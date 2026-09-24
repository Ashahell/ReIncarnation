/*
 * app/panel909.c — ReIncarnation 909 first-panel window (Module 2.9).
 *
 * AROS-ONLY. MUI application window holding the four 909 voice knobs
 * (tune/level/decay/flamres) as custom RKnB objects
 * (gui/widgets/rknb.mcc.c: MUIC_Numeric subclass rendering the
 * measured 80 px 909 frames — MUIC_Knob retired). Rects come from
 * ri_panel909_knob_rect (host-pinned, t29_layout). A text row under
 * the knobs shows live values (first notify wiring: each knob
 * notifies every value change; the loop refreshes its readout via
 * the host-tested ri_ctl_format_value). The window is
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

extern APTR ri_rknb_create(LONG dflt);
extern void ri_rknb_dispose_class(void);

#define NKNOB 4
#define RET_KNOB_BASE 100

/* Value readout buffers (file-static: MUIA_Text_Contents copies on
 * set, but the initial tag strings must outlive creation). */
static char s_valbuf[NKNOB][4];

int main(void) {
    Object *app = NULL, *win = NULL, *cells[NKNOB], *knobs[NKNOB];
    Object *texts[NKNOB];
    const struct RIPanelDesc *panel = ri_panel_get(3);
    ULONG sigs = 0;
    LONG ret;
    unsigned int i;

    /* 909 panel contract: index 3, exactly the 4 voice controls. */
    if (!panel || panel->nctls != 4u)
        return 5;
    for (i = 0; i < NKNOB; i++) {
        /* Custom RKnB class (measured 80 px 909 frames, NOT MUIC_Knob):
         * per-control default from the panel table (right-click
         * target, TC-2.9.2); fail-closed to mid when the table has
         * no such control. FixWidth/Height match the frame edge so
         * the MUI layout never scales the art. */
        int dflt = ri_panel_default_ctl(panel, 0x0900u + i);
        if (dflt < 0)
            dflt = 64;
        knobs[i] = (Object *)ri_rknb_create(dflt);
        if (!knobs[i])
            return 6;
        SetAttrs(knobs[i], MUIA_FixWidth, 80, MUIA_FixHeight, 80,
            TAG_DONE);
        cells[i] = knobs[i];
        /* Readout cell: fixed 80 wide to sit under its knob. */
        ri_ctl_format_value(s_valbuf[i], 64);
        texts[i] = (Object *)MUI_NewObject(MUIC_Text,
            MUIA_Text_Contents, (IPTR)s_valbuf[i],
            MUIA_FixWidth, 80,
            TAG_DONE);
        if (!texts[i])
            return 7;
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
            MUIA_Group_Spacing, 0,
            Child, (IPTR)MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 0,
                Child, cells[0],
                Child, cells[1],
                Child, cells[2],
                Child, cells[3],
                TAG_DONE),
            Child, (IPTR)MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 0,
                Child, texts[0],
                Child, texts[1],
                Child, texts[2],
                Child, texts[3],
                TAG_DONE),
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
    /* First notify wiring (values finally go somewhere): every knob
     * reports each value change back here; the loop refreshes that
     * knob's readout cell. */
    for (i = 0; i < NKNOB; i++) {
        DoMethod(knobs[i], MUIM_Notify, MUIA_Numeric_Value,
            MUIV_EveryTime, app, 2, MUIM_Application_ReturnID,
            RET_KNOB_BASE + i);
    }
    while ((ret = (LONG)DoMethod(app, MUIM_Application_NewInput,
        &sigs)) != (LONG)MUIV_Application_ReturnID_Quit) {
        if (ret >= RET_KNOB_BASE && ret < RET_KNOB_BASE + NKNOB) {
            unsigned int k = (unsigned int)(ret - RET_KNOB_BASE);
            IPTR v = 0;
            GetAttr(MUIA_Numeric_Value, knobs[k], &v);
            ri_ctl_format_value(s_valbuf[k], (int)v);
            SetAttrs(texts[k], MUIA_Text_Contents,
                (IPTR)s_valbuf[k], TAG_DONE);
        }
        if (sigs)
            Wait(sigs);
    }
    MUI_DisposeObject(app);
    ri_rknb_dispose_class();
    return 0;
}
