/*
 * rlbl.mcc.c — RLbl static label custom MUI class (Module 2.9).
 *
 * AROS-ONLY. Subclasses MUIC_Area, paints an 80x14 cell: measured
 * panel fill (RI_PANEL909_BG) + centered dark text with the window
 * font (graphics Text/TextLength — the knobproof calls). No input
 * handling at all (labels never eat clicks). Must NEVER enter the
 * host build (audit gates it).
 */

#ifndef __AROS__
#error "rlbl.mcc.c is AROS-only: Zune custom class, never in the host build"
#endif

#include <exec/types.h>
#include <graphics/rastport.h>
#include <graphics/gfx.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include "gui/panels.h"
#include "gui/knob_blit.h"
#include "gui/widgets/rlbl.h"

#define RLBL_W 80
#define RLBL_H 14
#define RLBL_TEXT_MAX 24

struct RLblData {
    char text[RLBL_TEXT_MAX];
};

BOOPSI_DISPATCHER_PROTO(IPTR, rlbl_dispatcher, Class *, Object *, Msg);

BOOPSI_DISPATCHER(IPTR, rlbl_dispatcher, cl, obj, msg) {
    struct RLblData *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet *s = (struct opSet *)msg;
        struct TagItem *ti;
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        int i;
        if (!o)
            return (IPTR)NULL;
        d = (struct RLblData *)INST_DATA(cl, o);
        d->text[0] = '\0';
        ti = FindTagItem(MUIA_RLbl_Text, s->ops_AttrList);
        if (ti && ti->ti_Data) {
            const char *src = (const char *)ti->ti_Data;
            for (i = 0; i < RLBL_TEXT_MAX - 1 && src[i]; i++)
                d->text[i] = src[i];
            d->text[i] = '\0';
        }
        return (IPTR)o;
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth = RLBL_W;
        m->MinMaxInfo->MinHeight = RLBL_H;
        m->MinMaxInfo->MaxWidth = RLBL_W;
        m->MinMaxInfo->MaxHeight = RLBL_H;
        m->MinMaxInfo->DefWidth = RLBL_W;
        m->MinMaxInfo->DefHeight = RLBL_H;
        return rc;
    }
    case MUIM_Setup:
    case MUIM_Cleanup:
    case MUIM_Show:
    case MUIM_Hide:
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Draw: {
        /* Paint on ANY Draw (m22 verdict: no DRAWOBJECT flag here). */
        struct RastPort *rp;
        int len, w, x0, y0;
        d = (struct RLblData *)INST_DATA(cl, obj);
        rp = _rp(obj);
        if (!rp)
            return (IPTR)0;
        ri_knob_panel_rect(rp, _left(obj), _top(obj), RLBL_W, RLBL_H,
            RI_PANEL909_BG);
        for (len = 0; len < RLBL_TEXT_MAX && d->text[len]; len++)
            ;
        if (len <= 0)
            return (IPTR)0;
        w = TextLength(rp, (CONST_STRPTR)d->text, (ULONG)len);
        x0 = _left(obj) + (RLBL_W - (int)w) / 2;
        y0 = _top(obj) + RLBL_H - 3;
        if (x0 < _left(obj))
            x0 = _left(obj);
        SetAPen(rp, 1);
        Move(rp, x0, y0);
        Text(rp, (CONST_STRPTR)d->text, (ULONG)len);
        return (IPTR)0;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

static struct MUI_CustomClass *s_rlbl_class = NULL;

struct MUI_CustomClass *ri_rlbl_class(void) {
    if (!s_rlbl_class) {
        s_rlbl_class = MUI_CreateCustomClass(MUIMasterBase, MUIC_Area,
            NULL, sizeof(struct RLblData), (APTR)rlbl_dispatcher);
    }
    return s_rlbl_class;
}

void ri_rlbl_dispose_class(void) {
    if (s_rlbl_class) {
        MUI_DeleteCustomClass(s_rlbl_class);
        s_rlbl_class = NULL;
    }
}

APTR ri_rlbl_create(const char *text) {
    struct MUI_CustomClass *mcc = ri_rlbl_class();
    if (!mcc)
        return NULL;
    return (APTR)NewObject(mcc->mcc_Class, NULL,
        MUIA_RLbl_Text, (IPTR)text,
        TAG_DONE);
}
