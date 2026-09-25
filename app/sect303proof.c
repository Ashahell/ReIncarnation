/*
 * app/sect303proof.c — 303 section canvas on-device proof (§12.10 G4).
 *
 * AROS-ONLY. One window holding the RSec303 canvas for Synth 1 at 1x
 * (732 x 230 px) plus a readout row: EDIT STEP, the edit step's key and
 * flags, the Tune/Cutoff values and the change counter — so remote clicks
 * (agent ui_input) can be verified by number as well as by ui_capture.
 * Exit: close gadget or Ctrl-C. Return codes: 0 ok, 5+ build failures.
 * Must NEVER enter the host build (audit gates app/).
 */

#ifndef __AROS__
#error "app/sect303proof.c is AROS-only: Zune proof window, never in the host build"
#endif

#include <exec/types.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/dos.h>
#include "gui/ctlreg.h"
#include "gui/sect303.h"
#include "gui/widgets/rsec303.h"

#define RET_CHANGED 400

static char s_readout[160];
static const struct RSec303Diag *s_diag;

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

static void format_readout(const struct RISect303 *st, LONG changes) {
    const struct RI303Row *r = &st->pat.row.r303[st->edit_step % 16u];
    char *p = s_readout;
    put_str(&p, "STEP ");
    put_num(&p, ri_s303_display(st));
    put_str(&p, " KEY ");
    put_num(&p, r->key);
    put_str(&p, " FL ");
    put_num(&p, r->flags);
    put_str(&p, " PM ");
    put_num(&p, st->pitch_mode);
    put_str(&p, " TUNE ");
    put_num(&p, st->val[RI_S303_TUNE]);
    put_str(&p, " CUT ");
    put_num(&p, st->val[2]);
    put_str(&p, " WAVE ");
    put_num(&p, st->val[RI_S303_WAVE]);
    put_str(&p, " CHG ");
    put_num(&p, changes);
    if (s_diag) {
        put_str(&p, " EV ");
        put_num(&p, s_diag->events);
        put_str(&p, "/");
        put_num(&p, s_diag->buttons);
        put_str(&p, " @");
        put_num(&p, s_diag->last_x);
        put_str(&p, ",");
        put_num(&p, s_diag->last_y);
        put_str(&p, " H");
        put_num(&p, (long)s_diag->last_hit);
        put_str(&p, " S");
        put_num(&p, s_diag->setups);
        put_num(&p, s_diag->shows);
    }
    *p = 0;
}

/* Scripted demo (argument "demo"): drives the SAME behaviour functions a
 * click would (gui/sect303.c) and redraws, so ui_capture can prove the
 * state -> pixels path without pointer injection. Sequence = the manual's
 * "Programming a Pattern from Scratch in Pitch Mode" (p. 42): Pitch Mode
 * on, step 1 Accent + G, step 2 Slide + high C, step 3 Up + E; ends on
 * step 4 with Pitch Mode still lit. */
static void demo(Object *canvas, Object *readout, struct RISect303 *st) {
    static const unsigned char seq[] = {
        RI_S303_PITCHMODE, RI_S303_ACCENT, RI_S303_KEY0 + 7,
        RI_S303_SLIDE, RI_S303_NOTEPAUSE, RI_S303_KEY0 + 12,
        RI_S303_UP, RI_S303_NOTEPAUSE, RI_S303_KEY0 + 4
    };
    unsigned int i;
    for (i = 0; i < sizeof(seq); i++) {
        ri_s303_press(st, seq[i]);
        MUI_Redraw(canvas, MADF_DRAWOBJECT);
        format_readout(st, (LONG)(i + 1));
        SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
        Delay(10);
    }
    ri_s303_set_value(st, 2, 30);   /* Cutoff down */
    ri_s303_set_value(st, 3, 110);  /* Reso up */
    ri_s303_press(st, RI_S303_WAVE);
    MUI_Redraw(canvas, MADF_DRAWOBJECT);
    format_readout(st, 99);
    SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
}

int main(int argc, char **argv) {
    Object *app, *win, *canvas, *readout;
    ULONG sigs = 0;
    LONG ret;
    const struct RISect303 *st = 0;

    canvas = (Object *)ri_rsec303_create(RI_SEC_SYNTH1, 0);
    if (!canvas)
        return 5;
    GetAttr(MUIA_RSec303_State, canvas, (IPTR *)&st);
    GetAttr(MUIA_RSec303_Diag, canvas, (IPTR *)&s_diag);
    format_readout(st, 0);
    readout = (Object *)MUI_NewObject(MUIC_Text,
        MUIA_Text_Contents, (IPTR)s_readout,
        TAG_DONE);
    if (!readout)
        return 6;
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, "RI-303",
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
        MUIA_Application_Title, "RI-303",
        MUIA_Application_Base, "RI303",
        SubWindow, (IPTR)win,
        TAG_DONE);
    if (!app)
        return 8;
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2,
        MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(canvas, MUIM_Notify, MUIA_RSec303_Changes, MUIV_EveryTime, (IPTR)app, 2,
        MUIM_Application_ReturnID, RET_CHANGED);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    if (argc > 1 && argv[1][0] == 'd')
        demo(canvas, readout, (struct RISect303 *)st);

    /* canonical union loop (m60): NewInput serves input, Wait sleeps on
     * app signals | Ctrl-C */
    for (;;) {
        ret = (LONG)DoMethod(app, MUIM_Application_NewInput, &sigs);
        if (ret == (LONG)MUIV_Application_ReturnID_Quit)
            break;
        {   /* refresh on every wake-up: the diag counters move even when
             * no state changes (proves the event plumbing) */
            IPTR ch = 0; /* GetAttr stores a full IPTR: never a LONG (stack smash) */
            GetAttr(MUIA_RSec303_Changes, canvas, &ch);
            format_readout(st, (LONG)ch);
            SetAttrs(readout, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
        }
        sigs |= SIGBREAKF_CTRL_C;
        if (sigs) {
            sigs = Wait(sigs);
            if (sigs & SIGBREAKF_CTRL_C)
                break;
        }
    }
    SetAttrs(win, MUIA_Window_Open, FALSE, TAG_DONE);
    MUI_DisposeObject(app);
    ri_rsec303_dispose_class();
    return 0;
}
