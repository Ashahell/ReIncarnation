/*
 * app/main.c — ReIncarnation Classic Intuition window + task wiring
 * (Task 12, gate G12).
 *
 * AROS-ONLY. Opens one Intuition window whose title carries the wired
 * panel count straight from gui/panels.c (the same tables the MCC
 * shells skin), runs a CloseWindow event loop, and exits. Full MUI
 * panel instantiation lands with the on-device acceptance pass; the
 * wiring here proves window + table linkage compiles and links on
 * AROS. Must NEVER enter the host build (audit gates it).
 */

#ifndef __AROS__
#error "app/main.c is AROS-only: Intuition wiring, never in the host build"
#endif

#include <exec/types.h>
#include <exec/ports.h>
#include <utility/tagitem.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "gui/panels.h"

int main(void) {
    struct Window *win;
    struct IntuiMessage *msg;
    struct MsgPort *port;
    ULONG sigs;
    unsigned int npanels = ri_panel_count();
    int done = 0;
    /* Title wires the panel tables into the binary: the count IS the
     * contract (6: 303A/303B/808/909/mixer/transport). */
    static TEXT title[] = "ReIncarnation Classic (6 panels)";
    static struct TagItem wi_tags[8];

    if (npanels != 6u)
        return 5; /* panel-table drift: refuse to run, loudly */

    /* Plain tag array + OpenWindowTagList: avoids the variadic inline
     * wrapper (SDK boost-macro pedantic noise under -Werror). */
    wi_tags[0].ti_Tag = WA_Title;
    wi_tags[0].ti_Data = (IPTR)title;
    wi_tags[1].ti_Tag = WA_Width;
    wi_tags[1].ti_Data = 1024;
    wi_tags[2].ti_Tag = WA_Height;
    wi_tags[2].ti_Data = 768;
    wi_tags[3].ti_Tag = WA_CloseGadget;
    wi_tags[3].ti_Data = TRUE;
    wi_tags[4].ti_Tag = WA_DragBar;
    wi_tags[4].ti_Data = TRUE;
    wi_tags[5].ti_Tag = WA_DepthGadget;
    wi_tags[5].ti_Data = TRUE;
    wi_tags[6].ti_Tag = WA_IDCMP;
    wi_tags[6].ti_Data = IDCMP_CLOSEWINDOW;
    wi_tags[7].ti_Tag = TAG_DONE;
    wi_tags[7].ti_Data = 0;

    win = (struct Window *)OpenWindowTagList(NULL, wi_tags);
    if (!win)
        return 10;

    port = win->UserPort;
    while (!done) {
        sigs = Wait((1UL << port->mp_SigBit));
        (void)sigs;
        while ((msg = (struct IntuiMessage *)GetMsg(port)) != NULL) {
            if (msg->Class == IDCMP_CLOSEWINDOW)
                done = 1;
            ReplyMsg((struct Message *)msg);
        }
    }
    CloseWindow(win);
    return 0;
}
