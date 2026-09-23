/*
 * app/knobproof.c — knob-art on-device proof (Module 2.9 art).
 *
 * AROS-ONLY. Opens a SmartRefresh RI_PANEL909_W/H Intuition window
 * at screen origin and paints the first-panel composition via
 * gui/knob_blit.c: background fill, divider rules, knob labels
 * (system pen 1), and the four 909 knob frames at the doc centers
 * with panel-default values. Then runs a CloseWindow event loop.
 * Exit code = number of paint failures (0 = full composition).
 * Screendump measurement reads the composition back against
 * docs/evidence/gui/panel-909-geometry.md. Must NEVER enter the
 * host build (audit gates it).
 */

#ifndef __AROS__
#error "app/knobproof.c is AROS-only: Intuition proof window, never in the host build"
#endif

#include <exec/types.h>
#include <graphics/rastport.h>
#include <intuition/intuition.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdint.h>
#include "gui/panels.h"
#include "gui/knob_blit.h"
int ri_knob_panel_rect(struct RastPort *rp, int x, int y, int w, int h,
    uint32_t rgb);

static const unsigned int KCTL[4] = { 0x0900u, 0x0901u, 0x0902u, 0x0903u };
static const int KCX[4] = { 40, 110, 180, 250 };
static const int KCY = 52;
static const char *KNOB_NAMES[4] = { "TUNE", "LEVEL", "DECAY", "FLAMRES" };
static const int KDIV[3] = { 75, 145, 215 };

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
    wi_tags[3].ti_Data = RI_PANEL909_W;
    wi_tags[4].ti_Tag = WA_Height;
    wi_tags[4].ti_Data = RI_PANEL909_H;
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
    /* Background + divider rules (blitted exact colors, no pens). */
    if (ri_knob_panel_rect(win->RPort, 0, 0, RI_PANEL909_W, RI_PANEL909_H,
        RI_PANEL909_BG) <= 0)
        bad++;
    for (i = 0; i < 3; i++) {
        if (ri_knob_panel_rect(win->RPort, KDIV[i], 0, 1, RI_PANEL909_H,
            0x8a8a84u) <= 0)
            bad++;
    }
    /* Knob labels (system pen 1) centered over each knob. */
    SetAPen(win->RPort, 1);
    for (i = 0; i < 4; i++) {
        int len = 0;
        const char *s = KNOB_NAMES[i];
        while (s[len])
            len++;
        {
            int w = TextLength(win->RPort, (CONST_STRPTR)s, len);
            Move(win->RPort, KCX[i] - w / 2, 10);
            Text(win->RPort, (CONST_STRPTR)s, len);
        }
    }
    /* Knob frames at doc centers with panel defaults. */
    for (i = 0; i < 4; i++) {
        int v = ri_panel_default_ctl(panel, KCTL[i]);
        int rc;
        if (v < 0)
            v = 64;
        /* 80px frames centered on the doc centers. */
        rc = ri_knob_blit_one(win->RPort, v, KCX[i] - 40, KCY - 40);
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
