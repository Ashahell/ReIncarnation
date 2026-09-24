/*
 * rknb.mcc.c — RKnB knob custom MUI class (Module 2.9 art on-device).
 *
 * AROS-ONLY. Subclasses MUIC_Numeric (spec §13 widget reuse: Numeric → Knob,
 * min/max/value/default + notify machinery inherited) and overrides ONLY
 * the visuals and the pointer gesture: MUIM_Draw blits the measured
 * 80 px 909 frame (gui/knob_art.h recipe via gui/knob_blit.h), the
 * vertical drag maps pixels to ctl values through the host-tested
 * gui/knob_logic.c math (P-18), right-click restores the per-control
 * default (TC-2.9.2). MUIC_Knob is retired: nothing in the app tree
 * instantiates it anymore. Must NEVER enter the host build: the
 * #error below fires on any non-AROS compile, scripts/ri_build_host.sh
 * never references this file, and scripts/ri_audit.sh gates both facts
 * (probe_ahi.c precedent).
 */

#ifndef __AROS__
#error "rknb.mcc.c is AROS-only: Zune custom class, never in the host build"
#endif

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include <devices/inputevent.h>
#include <libraries/mui.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <clib/muimaster_protos.h>
#include "gui/knob_logic.h"
#include "gui/knob_blit.h"

#define RKNB_PX 80 /* frame edge, matches RI_KNOB_PX */

struct RKnBData {
    LONG default_val;
    LONG cur; /* last value seen in OM_NEW/OM_SET: the Draw path reads
               * this instead of calling back into the dispatcher
               * (no DoMethod inside MUIM_Draw — render must not
               * reenter). Every programmatic change flows through
               * OM_SET, so this stays exact. */
    LONG drag_start_val;
    WORD drag_start_y;
    BOOL dragging;
    BOOL shown;
    struct RiGesture gesture;
    struct MUI_EventHandlerNode ehn;
};

BOOPSI_DISPATCHER_PROTO(IPTR, rknb_dispatcher, Class *, Object *, Msg);

/* Forward: defined below the dispatcher, used inside it. */
LONG ri_rknb_drag_value(LONG start, LONG dy_px, BOOL fine);
void ri_rknb_begin(struct RiGesture *g);
int ri_rknb_move(struct RiGesture *g);
int ri_rknb_release(struct RiGesture *g);

static int rknb_hit(Object *obj, WORD mx, WORD my) {
    return mx >= _left(obj) && mx < _left(obj) + _width(obj) &&
        my >= _top(obj) && my < _top(obj) + _height(obj);
}

BOOPSI_DISPATCHER(IPTR, rknb_dispatcher, cl, obj, msg) {
    struct RKnBData *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet *s = (struct opSet *)msg;
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!o)
            return (IPTR)NULL;
        d = (struct RKnBData *)INST_DATA(cl, o);
        d->default_val = GetTagData(MUIA_Numeric_Default, 64,
            s->ops_AttrList);
        d->cur = GetTagData(MUIA_Numeric_Value, 64, s->ops_AttrList);
        d->drag_start_val = 64;
        d->drag_start_y = 0;
        d->dragging = FALSE;
        d->shown = FALSE;
        d->gesture.begun = 0;
        d->gesture.moves = 0;
        d->gesture.commits = 0;
        return (IPTR)o;
    }
    case OM_SET: {
        struct opSet *s = (struct opSet *)msg;
        struct TagItem *ti;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        d = (struct RKnBData *)INST_DATA(cl, obj);
        ti = FindTagItem(MUIA_Numeric_Value, s->ops_AttrList);
        if (ti)
            d->cur = (LONG)ti->ti_Data;
        if (d->shown && ti)
            DoMethod(obj, MUIM_Draw, MADF_DRAWOBJECT);
        return rc;
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth = RKNB_PX;
        m->MinMaxInfo->MinHeight = RKNB_PX;
        m->MinMaxInfo->MaxWidth = RKNB_PX;
        m->MinMaxInfo->MaxHeight = RKNB_PX;
        m->MinMaxInfo->DefWidth = RKNB_PX;
        m->MinMaxInfo->DefHeight = RKNB_PX;
        return rc;
    }
    case MUIM_Setup:
    case MUIM_Cleanup:
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        if (rc) {
            d = (struct RKnBData *)INST_DATA(cl, obj);
            d->shown = TRUE;
            d->ehn.ehn_Priority = 0;
            d->ehn.ehn_Flags = MUI_EHF_GUIMODE;
            d->ehn.ehn_Object = obj;
            d->ehn.ehn_Class = cl;
            d->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
            /* NOTE: the handler goes to the MUI window OBJECT (_win),
             * never the Intuition window (_window): MUIM_Window_*
             * methods belong to MUIC_Window, and DoMethod on an
             * Intuition Window* dispatches through garbage (crashed
             * both Dell vehicles 2026-09-24, illegal-address guru).
             * _win vs _window is one letter and a type error. */
            DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                &d->ehn);
        }
        return rc;
    }
    case MUIM_Hide: {
        d = (struct RKnBData *)INST_DATA(cl, obj);
        /* _win (MUI object), never _window — see the Show note. */
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->ehn);
        d->shown = FALSE;
        d->dragging = FALSE;
        return DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_Draw: {
        /* Paint on ANY Draw call: the initial show-time Draw does NOT
         * carry MADF_DRAWOBJECT on this Zune (gated variant painted
         * nothing on device 2026-09-24; ungated paints — evidence
         * over docs). Full-frame repaint is idempotent, so partial
         * updates are harmless. */
        struct MUIP_Draw *m = (struct MUIP_Draw *)msg;
        (void)m;
        d = (struct RKnBData *)INST_DATA(cl, obj);
        ri_knob_blit_one(_rp(obj), (int)d->cur, _left(obj),
            _top(obj));
        return (IPTR)0;
    }
    case MUIM_HandleEvent: {
        struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
        struct IntuiMessage *im = m->imsg;
        d = (struct RKnBData *)INST_DATA(cl, obj);
        if (!im)
            return (IPTR)0;
        if (im->Class == IDCMP_MOUSEBUTTONS) {
            if (im->Code == SELECTDOWN) {
                if (!rknb_hit(obj, im->MouseX, im->MouseY))
                    return (IPTR)0;
                d->dragging = TRUE;
                d->drag_start_y = im->MouseY;
                d->drag_start_val = d->cur;
                ri_rknb_begin(&d->gesture);
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
            if (im->Code == SELECTUP) {
                if (!d->dragging)
                    return (IPTR)0;
                d->dragging = FALSE;
                ri_rknb_release(&d->gesture);
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
            if (im->Code == MENUDOWN) {
                if (!rknb_hit(obj, im->MouseX, im->MouseY))
                    return (IPTR)0;
                SetAttrs(obj, MUIA_Numeric_Value, d->default_val,
                    TAG_DONE);
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
            return (IPTR)0;
        }
        if (im->Class == IDCMP_MOUSEMOVE) {
            LONG v;
            int fine;
            if (!d->dragging)
                return (IPTR)0;
            fine = (im->Qualifier &
                (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
            v = ri_rknb_drag_value(d->drag_start_val,
                (LONG)(d->drag_start_y - im->MouseY), fine);
            ri_rknb_move(&d->gesture);
            SetAttrs(obj, MUIA_Numeric_Value, v, TAG_DONE);
            return (IPTR)MUI_EventHandlerRC_Eat;
        }
        return (IPTR)0;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

static struct MUI_CustomClass *s_rknb_class = NULL;

/* Class lifecycle (explicit: the app opens before building objects,
 * deletes after disposing them; NULL = loud failure upstream). */
struct MUI_CustomClass *ri_rknb_class(void) {
    if (!s_rknb_class) {
        s_rknb_class = MUI_CreateCustomClass(MUIMasterBase, MUIC_Numeric,
            NULL, sizeof(struct RKnBData), (APTR)rknb_dispatcher);
    }
    return s_rknb_class;
}

void ri_rknb_dispose_class(void) {
    if (s_rknb_class) {
        MUI_DeleteCustomClass(s_rknb_class);
        s_rknb_class = NULL;
    }
}

/* Create the knob: 0..127 with a per-control default (right-click
 * target, TC-2.9.2) and mid initial value. Artwork/silhouette
 * acceptance (±2 px @1024x768) is recorded in
 * docs/evidence/gui/acceptance.md, not asserted here. */
APTR ri_rknb_create(LONG dflt) {
    struct MUI_CustomClass *mcc = ri_rknb_class();
    if (!mcc)
        return NULL;
    return (APTR)NewObject(mcc->mcc_Class, NULL,
        MUIA_Numeric_Min, 0,
        MUIA_Numeric_Max, 127,
        MUIA_Numeric_Value, 64,
        MUIA_Numeric_Default, dflt,
        TAG_DONE);
}

/* Vertical drag pixels → quantized ctl value (callers negate screen-y
 * first: up-drag increases). Shift passes fine=1 (×0.1, 1500 px full). */
LONG ri_rknb_drag_value(LONG start, LONG dy_px, BOOL fine) {
    double v = ri_knob_drag_to_value((double)start, (double)dy_px,
                                     fine ? 1 : 0);
    return (LONG)ri_ctl_quantize(v);
}

/* Gesture commit wiring: moves notify (return 0 = no commit yet),
 * release commits exactly one undo unit (return 1). */
void ri_rknb_begin(struct RiGesture *g) {
    ri_gesture_begin(g);
}

int ri_rknb_move(struct RiGesture *g) {
    ri_gesture_move(g);
    return 0;
}

int ri_rknb_release(struct RiGesture *g) {
    int before = g ? g->commits : 0;
    ri_gesture_end(g);
    return (g && g->commits != before) ? 1 : 0;
}
