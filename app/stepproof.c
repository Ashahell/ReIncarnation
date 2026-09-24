/*
 * app/stepproof.c — step-button on-device proof (Module 2.10 start).
 *
 * AROS-ONLY. Opens a window with 16 RStp step buttons (MUIC skin over
 * the host-tested ri_step_toggle math) in one row plus a pattern
 * readout showing the 16-bit step state as decimal (host-tested
 * ri_ctl_format_count handles the full 0..65535 range). Clicking a
 * button toggles its state; the loop re-reads all buttons on any
 * change and refreshes the readout. Exit code = build failures only
 * (0 = all sixteen live). Screendump + owner clicks prove toggle
 * behavior by number. Must NEVER enter the host build (audit gates
 * it).
 */

#ifndef __AROS__
#error "app/stepproof.c is AROS-only: Zune/MUI proof window, never in the host build"
#endif

#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include <clib/intuition_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <stdint.h>
#include "gui/panels.h"

extern APTR ri_rstp_create(void);

#define NSTEPS 16
#define RET_STEP_BASE 300

/* Pattern readout buffer (file-static: contents copied on set). */
static char s_patbuf[8];

int main(void) {
    Object *app = NULL, *win = NULL, *steps[NSTEPS], *pat;
    ULONG sigs = 0;
    LONG ret;
    unsigned int i;

    for (i = 0; i < NSTEPS; i++) {
        steps[i] = (Object *)ri_rstp_create();
        if (!steps[i])
            return 6;
    }
    ri_ctl_format_count(s_patbuf, 0);
    pat = (Object *)MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (IPTR)s_patbuf,
        TAG_DONE);
    if (!pat)
        return 7;
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, "RI-STEPS",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_Width, 512,
        MUIA_Window_Height, 120,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)MUI_NewObject(MUIC_Group,
            MUIA_Group_Spacing, 0,
            Child, (IPTR)MUI_NewObject(MUIC_Group,
                MUIA_Group_Horiz, TRUE,
                MUIA_Group_Spacing, 0,
                Child, steps[0],
                Child, steps[1],
                Child, steps[2],
                Child, steps[3],
                Child, steps[4],
                Child, steps[5],
                Child, steps[6],
                Child, steps[7],
                Child, steps[8],
                Child, steps[9],
                Child, steps[10],
                Child, steps[11],
                Child, steps[12],
                Child, steps[13],
                Child, steps[14],
                Child, steps[15],
                TAG_DONE),
            Child, pat,
            TAG_DONE),
        TAG_DONE);
    if (!win)
        return 8;
    app = (Object *)MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "RISTEPS",
        SubWindow, win,
        TAG_DONE);
    if (!app)
        return 9;
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, app,
        2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    for (i = 0; i < NSTEPS; i++) {
        DoMethod(steps[i], MUIM_Notify, MUIA_Numeric_Value,
            MUIV_EveryTime, app, 2, MUIM_Application_ReturnID,
            RET_STEP_BASE + i);
    }
    while ((ret = (LONG)DoMethod(app, MUIM_Application_NewInput,
        &sigs)) != (LONG)MUIV_Application_ReturnID_Quit) {
        if (ret >= RET_STEP_BASE && ret < RET_STEP_BASE + NSTEPS) {
            unsigned long pattern = 0;
            for (i = 0; i < NSTEPS; i++) {
                IPTR v = 0;
                GetAttr(MUIA_Numeric_Value, steps[i], &v);
                if (v)
                    pattern |= (1u << i);
            }
            ri_ctl_format_count(s_patbuf, pattern);
            SetAttrs(pat, MUIA_Text_Contents, (IPTR)s_patbuf,
                TAG_DONE);
        }
        if (sigs)
            Wait(sigs);
    }
    MUI_DisposeObject(app);
    return 0;
}
