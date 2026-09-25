/*
 * app/sectproof.c — section canvas on-device proof (§12.10 G4).
 *
 * AROS-ONLY. usage: RISECT [303|808] [demo]
 * One window with the RSection canvas for the chosen section at 1x plus a
 * readout row, so state can be verified by number as well as by ui_capture.
 * "demo" drives the same behaviour calls a click makes (gui/sectui.h):
 *   303 — manual p. 42 "programming from scratch in Pitch Mode" + knobs;
 *   808 — BD four-on-the-floor, CH offbeats, AC on 5 and 13, LT->LC switch,
 *         CH left selected (steps show the CH row), BD Level/Tone moved.
 * Exit: close gadget or Ctrl-C. Return codes: 0 ok, 5+ build failures.
 * Must NEVER enter the host build (audit gates app/).
 */

#ifndef __AROS__
#error "app/sectproof.c is AROS-only: Zune proof window, never in the host build"
#endif

#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/widgets/rsection.h"

static char s_readout[160];

static void put_num(char **p, long v) {
    char t[12];
    int n = 0;
    if (v < 0) {
        *(*p)++ = '-';
        v = -v;
    }
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 11);
    while (n)
        *(*p)++ = t[--n];
}

static void put_str(char **p, const char *s) {
    while (*s)
        *(*p)++ = *s++;
}

static void format_readout(const struct RISectUI *ui, const struct RSectionDiag *dg, long changes) {
    char *p = s_readout;
    if (ui->section == RI_SEC_808) {
        const struct RISect808 *s = &ui->u.s808;
        unsigned int st;
        put_str(&p, "SEL ");
        put_num(&p, s->val[RI_S808_SELECT]);
        put_str(&p, " ROW ");
        for (st = 0; st < 16; st++)
            *p++ = ri_sui_led(ui, RI_S808_STEP0 + st, 0) ? 'X' : '.';
        put_str(&p, " BD ");
        put_num(&p, s->pat.row.drum[0].on);
        put_str(&p, " LC ");
        put_num(&p, s->val[9]);
    } else {
        const struct RISect303 *s = &ui->u.s303;
        const struct RI303Row *r = &s->pat.row.r303[s->edit_step % 16u];
        put_str(&p, "STEP ");
        put_num(&p, ri_s303_display(s));
        put_str(&p, " KEY ");
        put_num(&p, r->key);
        put_str(&p, " FL ");
        put_num(&p, r->flags);
        put_str(&p, " PM ");
        put_num(&p, s->pitch_mode);
        put_str(&p, " CUT ");
        put_num(&p, s->val[2]);
    }
    put_str(&p, " CHG ");
    put_num(&p, changes);
    if (dg) {
        put_str(&p, " EV ");
        put_num(&p, dg->events);
        put_str(&p, "/");
        put_num(&p, dg->buttons);
    }
    *p = 0;
}

static void demo(Object *canvas, struct RISectUI *ui) {
    unsigned int i;
    if (ui->section == RI_SEC_808) {
        for (i = 0; i < 16; i += 4)
            ri_sui_press(ui, RI_S808_STEP0 + i);           /* BD */
        ri_sui_set(ui, RI_S808_SELECT, 11);                 /* CH */
        for (i = 2; i < 16; i += 4)
            ri_sui_press(ui, RI_S808_STEP0 + i);
        ri_sui_set(ui, RI_S808_SELECT, 0);                  /* AC */
        ri_sui_press(ui, RI_S808_STEP0 + 4);
        ri_sui_press(ui, RI_S808_STEP0 + 12);
        ri_sui_press(ui, 9);                                /* LT -> LC */
        ri_sui_set(ui, 1, 120);                             /* BD Level */
        ri_sui_set(ui, 2, 20);                              /* BD Tone */
        ri_sui_set(ui, RI_S808_SELECT, 11);                 /* show the CH row */
    } else {
        static const unsigned char seq[] = {
            RI_S303_PITCHMODE, RI_S303_ACCENT, RI_S303_KEY0 + 7, RI_S303_SLIDE, RI_S303_NOTEPAUSE,
            RI_S303_KEY0 + 12, RI_S303_UP, RI_S303_NOTEPAUSE, RI_S303_KEY0 + 4
        };
        for (i = 0; i < sizeof(seq); i++)
            ri_sui_press(ui, seq[i]);
        ri_sui_set(ui, 2, 30);
        ri_sui_set(ui, 3, 110);
        ri_sui_press(ui, RI_S303_WAVE);
    }
    ri_rsection_refresh(canvas);
}

int main(int argc, char **argv) {
    Object *app, *win, *canvas, *readout;
    ULONG sigs = 0, section = RI_SEC_SYNTH1;
    LONG ret;
    struct RISectUI *ui = 0;
    const struct RSectionDiag *dg = 0;
    int i, do_demo = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '8')
            section = RI_SEC_808;
        else if (argv[i][0] == 'd')
            do_demo = 1;
    }
    canvas = (Object *)ri_rsection_create(section, 0);
    if (!canvas)
        return 5;
    GetAttr(MUIA_RSection_State, canvas, (IPTR *)&ui);
    GetAttr(MUIA_RSection_Diag, canvas, (IPTR *)&dg);
    format_readout(ui, dg, 0);
    readout = (Object *)MUI_NewObject(MUIC_Text, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
    if (!readout)
        return 6;
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, section == RI_SEC_808 ? "RI-808" : "RI-303",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)MUI_NewObject(MUIC_Group,
            MUIA_Group_Spacing, 2,
            Child, (IPTR)canvas,
            Child, (IPTR)readout,
            TAG_DONE),
        TAG_DONE);
    if (!win)
        return 7;
    app = (Object *)MUI_NewObject(MUIC_Application,
        MUIA_Application_Title, "RI-SECT",
        MUIA_Application_Base, "RISECT",
        SubWindow, (IPTR)win,
        TAG_DONE);
    if (!app)
        return 8;
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    if (do_demo) {
        demo(canvas, ui);
        format_readout(ui, dg, 0);
        SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
    }
    /* canonical union loop (m60) */
    for (;;) {
        IPTR ch = 0; /* GetAttr stores a full IPTR (never a LONG: stack smash, 2026-09-25) */
        ret = (LONG)DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (ret == (LONG)MUIV_Application_ReturnID_Quit)
            break;
        GetAttr(MUIA_RSection_Changes, canvas, &ch);
        format_readout(ui, dg, (long)ch);
        SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
        sigs |= SIGBREAKF_CTRL_C;
        sigs = Wait(sigs);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
    }
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    ri_rsection_dispose_class();
    return 0;
}
