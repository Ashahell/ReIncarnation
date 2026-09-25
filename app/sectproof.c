/*
 * app/sectproof.c — section canvas on-device proof (§12.10 G4).
 *
 * AROS-ONLY. usage: RISECT [303|808|909|mix|fx] [demo]
 * One window with the RSection canvas for the chosen section at 1x plus a
 * readout row, so state can be verified by number as well as by ui_capture.
 * "demo" drives the same behaviour calls a click makes (gui/sectui.h):
 *   303 — manual p. 42 "programming from scratch in Pitch Mode" + knobs;
 *   808 — BD four-on-the-floor, CH offbeats, AC on 5 and 13, LT->LC switch,
 *         CH left selected (steps show the CH row), BD Level/Tone moved;
 *   909 — BD low/high alternating on 1,5,9,13, SD flam on 5 and 13, CH
 *         selected with low hits on the off-beats, BD Tune / Flam moved;
 *   mix — the four section mixers + Master side by side on ONE board:
 *         808 muted, PCF on Synth 1 then stolen by the 808 (p. 62), Dist
 *         on the 909, Comp on the Master, faders/Pan/Delay moved, meters
 *         fed fixed test levels;
 *   fx  — PCF, Delay, Dist, Comp side by side: all on, PCF pattern 12 by
 *         twelve arrow-up clicks, BP, sliders moved; Delay steps 3 -> 6 by
 *         arrows, triplets, Pan/F.Back moved; Dist Amount/Shape; Comp
 *         Ratio/Threshold, input meters and level reduction fed.
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
static Object *s_mix[5];

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
    if (ui->section >= RI_SEC_PCF && ui->section <= RI_SEC_COMP) {
        unsigned int k;
        for (k = 0; k < 4; k++) {
            const struct RISectUI *f = 0;
            GetAttr(MUIA_RSection_State, s_mix[k], (IPTR *)&f);
            put_str(&p, k == 0 ? "PCF " : k == 1 ? " DLY " : k == 2 ? " DST " : " CMP ");
            put_num(&p, f->u.fx.val[0]);
            *p++ = ':';
            put_num(&p, f->u.fx.val[2]);
            *p++ = '/';
            put_num(&p, f->u.fx.val[3]);
        }
    } else if (ri_smix_strip(ui->section) >= 0) {
        const struct RIMixBoard *b = ui->u.mix.board;
        unsigned int st;
        put_str(&p, "ON ");
        for (st = 0; st < 4; st++)
            *p++ = b->val[st][RI_SMIX_ONOFF] ? '1' : '0';
        put_str(&p, " DIST ");
        put_num(&p, ri_route_owner(&b->route, RI_ROUTE_DIST));
        put_str(&p, " PCF ");
        put_num(&p, ri_route_owner(&b->route, RI_ROUTE_PCF));
        put_str(&p, " COMP ");
        put_num(&p, ri_route_owner(&b->route, RI_ROUTE_COMP));
        put_str(&p, " LVL ");
        for (st = 0; st < 4; st++) {
            put_num(&p, b->val[st][RI_SMIX_LEVEL]);
            *p++ = '/';
        }
        put_num(&p, b->val[4][RI_SMST_LEVEL]);
    } else if (ui->section == RI_SEC_909) {
        const struct RISect909 *s = &ui->u.s909;
        unsigned int st;
        put_str(&p, "SEL ");
        put_num(&p, s->val[RI_S909_SELECT]);
        put_str(&p, " ROW ");
        for (st = 0; st < 16; st++)
            *p++ = ".LHF"[ri_sui_led(ui, RI_S909_STEP0 + st, 0) & 3];
        put_str(&p, " FLAM ");
        put_num(&p, s->val[RI_S909_FLAM]);
    } else if (ui->section == RI_SEC_808) {
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
    if (ui->section >= RI_SEC_PCF && ui->section <= RI_SEC_COMP) {
        struct RISectUI *f[4];
        for (i = 0; i < 4; i++) {
            GetAttr(MUIA_RSection_State, s_mix[i], (IPTR *)&f[i]);
            ri_sui_press(f[i], RI_SFX_ONOFF);
            ri_sfx_meter_set(&f[i]->u.fx, RI_SFX_METER, 70 + 20 * (int)i);
        }
        for (i = 0; i < 12; i++)
            ri_sui_step(f[0], RI_SFX_PCF_PATTERN, 1);
        ri_sui_press(f[0], RI_SFX_PCF_MODE);
        ri_sui_set(f[0], 4, 90);
        ri_sui_set(f[0], 5, 40);
        ri_sui_set(f[0], 6, 110);
        ri_sui_set(f[0], 7, 20);
        for (i = 0; i < 3; i++)
            ri_sui_step(f[1], RI_SFX_DLY_STEPS, 1);
        ri_sui_press(f[1], RI_SFX_DLY_TRIPLET);
        ri_sui_set(f[1], 4, 20);
        ri_sui_set(f[1], 5, 90);
        ri_sui_set(f[2], 2, 110);
        ri_sui_set(f[2], 3, 30);
        ri_sui_set(f[3], 2, 100);
        ri_sui_set(f[3], 3, 40);
        ri_sfx_meter_set(&f[3]->u.fx, RI_SFX_COMP_GR, 70);
        for (i = 0; i < 4; i++)
            ri_rsection_refresh(s_mix[i]);
        (void)canvas;
        return;
    }
    if (ri_smix_strip(ui->section) >= 0) {
        struct RIMixBoard *b = ui->u.mix.board;
        static const unsigned char lvl[4] = { 110, 90, 100, 120 }, pan[4] = { 30, 98, 64, 64 };
        static const unsigned char dly[4] = { 80, 0, 40, 0 }, mtr[4] = { 100, 60, 0, 127 };
        (void)canvas;
        ri_smix_press(b, RI_SEC_MIX_808, RI_SMIX_ONOFF);          /* mute 808 */
        ri_smix_press(b, RI_SEC_MIX_SYNTH1, RI_SMIX_PCF);
        ri_smix_press(b, RI_SEC_MIX_808, RI_SMIX_PCF);            /* steals it */
        ri_smix_press(b, RI_SEC_MIX_909, RI_SMIX_DIST);
        ri_smix_press(b, RI_SEC_MASTER, RI_SMST_COMP);
        for (i = 0; i < 4; i++) {
            ri_smix_set_value(b, RI_SEC_MIX_SYNTH1 + i, RI_SMIX_LEVEL, lvl[i]);
            ri_smix_set_value(b, RI_SEC_MIX_SYNTH1 + i, RI_SMIX_PAN, pan[i]);
            ri_smix_set_value(b, RI_SEC_MIX_SYNTH1 + i, RI_SMIX_DELAY, dly[i]);
            ri_smix_meter_set(b, RI_SEC_MIX_SYNTH1 + i, 0, mtr[i]);
        }
        ri_smix_set_value(b, RI_SEC_MASTER, RI_SMST_LEVEL, 90);
        ri_smix_meter_set(b, RI_SEC_MASTER, 0, 96);
        ri_smix_meter_set(b, RI_SEC_MASTER, 1, 127);
        for (i = 0; i < 5; i++)
            ri_rsection_refresh(s_mix[i]);
        return;
    }
    if (ui->section == RI_SEC_909) {
        for (i = 0; i < 16; i += 4) {
            ri_sui_press(ui, RI_S909_STEP0 + i);          /* BD low */
            if (i == 4 || i == 12)
                ri_sui_press(ui, RI_S909_STEP0 + i);      /* -> high */
        }
        ri_sui_set(ui, RI_S909_SELECT, 2);                /* SD */
        ri_sui_press(ui, RI_S909_FLAMBTN);
        ri_sui_press(ui, RI_S909_STEP0 + 4);
        ri_sui_press(ui, RI_S909_STEP0 + 12);
        ri_sui_press(ui, RI_S909_FLAMBTN);
        ri_sui_set(ui, RI_S909_SELECT, 8);                /* CH */
        for (i = 2; i < 16; i += 4)
            ri_sui_press(ui, RI_S909_STEP0 + i);
        ri_sui_set(ui, 2, 90);                            /* BD Tune */
        ri_sui_set(ui, RI_S909_FLAM, 30);
        ri_rsection_refresh(canvas);
        return;
    }
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
    int i, do_demo = 0, mix = 0, fx = 0;
    Object *row = 0;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '8')
            section = RI_SEC_808;
        else if (argv[i][0] == '9')
            section = RI_SEC_909;
        else if (argv[i][0] == 'm')
            mix = 1;
        else if (argv[i][0] == 'f')
            fx = 1;
        else if (argv[i][0] == 'd')
            do_demo = 1;
    }
    if (fx) {                          /* the four effect units */
        for (i = 0; i < 4; i++) {
            s_mix[i] = (Object *)ri_rsection_create(RI_SEC_PCF + (ULONG)i, 0);
            if (!s_mix[i])
                return 5;
        }
        GetAttr(MUIA_RSection_State, s_mix[0], (IPTR *)&ui);
        canvas = s_mix[0];
        row = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)s_mix[3], TAG_DONE);
        if (!row)
            return 5;
        section = RI_SEC_PCF;
        mix = 2;
    } else if (mix) {                  /* four mixers + master, one shared board */
        struct RISectUI *u = 0;
        for (i = 0; i < 5; i++) {
            s_mix[i] = (Object *)ri_rsection_create(i < 4 ? RI_SEC_MIX_SYNTH1 + (ULONG)i : RI_SEC_MASTER, 0);
            if (!s_mix[i])
                return 5;
            GetAttr(MUIA_RSection_State, s_mix[i], (IPTR *)&u);
            if (i == 0)
                ui = u;
            else
                ri_sui_bind_board(u, ui->u.mix.board);
        }
        canvas = s_mix[0];
        row = (Object *)MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, MUIA_Group_Spacing, 2,
            Child, (IPTR)s_mix[0], Child, (IPTR)s_mix[1], Child, (IPTR)s_mix[2], Child, (IPTR)s_mix[3],
            Child, (IPTR)s_mix[4], TAG_DONE);
        if (!row)
            return 5;
        section = RI_SEC_MIX_SYNTH1;
    } else {
        canvas = (Object *)ri_rsection_create(section, 0);
        if (!canvas)
            return 5;
        row = canvas;
    }
    if (!mix)
        GetAttr(MUIA_RSection_State, canvas, (IPTR *)&ui);
    GetAttr(MUIA_RSection_Diag, canvas, (IPTR *)&dg);
    format_readout(ui, dg, 0);
    readout = (Object *)MUI_NewObject(MUIC_Text, MUIA_Text_Contents, (IPTR)s_readout, TAG_DONE);
    if (!readout)
        return 6;
    win = (Object *)MUI_NewObject(MUIC_Window,
        MUIA_Window_Title, mix == 2 ? "RI-FX" : mix ? "RI-MIX" : section == RI_SEC_808 ? "RI-808" : section == RI_SEC_909 ? "RI-909" : "RI-303",
        MUIA_Window_LeftEdge, 0,
        MUIA_Window_TopEdge, 0,
        MUIA_Window_CloseGadget, TRUE,
        MUIA_Window_DepthGadget, TRUE,
        MUIA_Window_DragBar, TRUE,
        MUIA_Window_RootObject, (IPTR)MUI_NewObject(MUIC_Group,
            MUIA_Group_Spacing, 2,
            Child, (IPTR)row,
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
