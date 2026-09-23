/*
 * app/knobproof.c — knob-art on-device proof (Module 2.9 art).
 *
 * AROS-ONLY. Opens a SmartRefresh 160x88 Intuition window at screen
 * origin (the locked compact geometry) and blits the four 909 knob
 * frames at the doc centers via gui/knob_blit.c, then runs a
 * CloseWindow event loop. Exit code = number of knobs that failed
 * to paint (0 = all four painted). Screendump measurement reads
 * centers back against docs/evidence/gui/panel-909-geometry.md.
 * Must NEVER enter the host build (audit gates it).
 */

#ifndef __AROS__
#error "app/knobproof.c is AROS-only: Intuition proof window, never in the host build"
#endif

#include <exec/types.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include "gui/panels.h"

int ri_knob_blit_one(struct RastPort *rp, int value, int dx, int dy);

static const unsigned int KCTL[4] = { 0x0900u, 0x0901u, 0x0902u, 0x0903u };
static const int KCX[4] = { 30, 63, 96, 129 };
static const int KCY = 52;

int main(void) {
    struct Window *win;
    struct IntuiMessage *msg;
    struct MsgPort *port;
    ULONG sigs;
    const struct RIPanelDesc *panel = ri_panel_get(3);
    int done = 0, bad = 0, i;
    static TEXT title[] = "RI-KNOBS";
    static struct TagItem wi_tags[10];

    if (!panel || panel->nctls != 4u)
        return 5;
    wi_tags[0].ti_Tag = WA_Title;
    wi_tags[0].ti_Data = (IPTR)title;
    wi_tags[1].ti_Tag = WA_Left;
    wi_tags[1].ti_Data = 0;
    wi_tags[2].ti_Tag = WA_Top;
    wi_tags[2].ti_Data = 0;
    wi_tags[3].ti_Tag = WA_Width;
    wi_tags[3].ti_Data = 160;
    wi_tags[4].ti_Tag = WA_Height;
    wi_tags[4].ti_Data = 88;
    wi_tags[5].ti_Tag = WA_CloseGadget;
    wi_tags[5].ti_Data = TRUE;
    wi_tags[6].ti_Tag = WA_DragBar;
    wi_tags[6].ti_Data = TRUE;
    wi_tags[7].ti_Tag = WA_SmartRefresh;
    wi_tags[7].ti_Data = TRUE;
    wi_tags[8].ti_Tag = WA_IDCMP;
    wi_tags[8].ti_Data = IDCMP_CLOSEWINDOW;
    wi_tags[9].ti_Tag = TAG_DONE;
    wi_tags[9].ti_Data = 0;

    win = (struct Window *)OpenWindowTagList(NULL, wi_tags);
    if (!win)
        return 10;
    for (i = 0; i < 4; i++) {
        int v = ri_panel_default_ctl(panel, KCTL[i]);
        int rc;
        if (v < 0)
            v = 64;
        /* 64px frames centered on the doc centers. */
        rc = ri_knob_blit_one(win->RPort, v, KCX[i] - 32, KCY - 32);
        if (rc <= 0)
            bad++;
    }
    if (bad) {
        CloseWindow(win);
        return 20 + bad;
    }
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
