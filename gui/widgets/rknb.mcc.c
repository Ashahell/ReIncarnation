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
#include <exec/ports.h>
#include <exec/io.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <utility/tagitem.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <libraries/mui.h>
#include <proto/exec.h>
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
    double acc_dx; /* accumulated drag since SELECTDOWN (double: LONG
                    * truncation would strand the clamped extremes) */
    double acc_dy;
    WORD last_x; /* last seen pointer (window coords) */
    WORD last_y;
    WORD warp_wx; /* drag-start point, window coords (warp target) */
    WORD warp_wy;
    WORD warp_sx; /* same point, screen coords (warp API speaks screen) */
    WORD warp_sy;
    BOOL dragging;
    BOOL shown;
    struct RiGesture gesture;
    struct MUI_EventHandlerNode ehn;
};

/* Screen-edge margin (px) that triggers a warp-back during drag. */

/* input.device for pointer warps (class lifetime; NULL = fail-soft
 * to absolute positioning, i.e. pre-grab behavior at screen edges). */
static struct MsgPort *s_inport = NULL;
static struct IOStdReq *s_inreq = NULL;

static void rknb_input_open(void) {
    if (s_inreq)
        return;
    s_inport = CreateMsgPort();
    if (!s_inport)
        return;
    s_inreq = (struct IOStdReq *)CreateIORequest(s_inport,
        sizeof(struct IOStdReq));
    if (!s_inreq) {
        DeleteMsgPort(s_inport);
        s_inport = NULL;
        return;
    }
    if (OpenDevice("input.device", 0, (struct IORequest *)s_inreq, 0)
        != 0) {
        DeleteIORequest((struct IORequest *)s_inreq);
        s_inreq = NULL;
        DeleteMsgPort(s_inport);
        s_inport = NULL;
    }
}

static void rknb_input_close(void) {
    if (s_inreq) {
        CloseDevice((struct IORequest *)s_inreq);
        DeleteIORequest((struct IORequest *)s_inreq);
        s_inreq = NULL;
    }
    if (s_inport) {
        DeleteMsgPort(s_inport);
        s_inport = NULL;
    }
}

/* Warp the pointer to absolute screen (sx, sy). Silent no-op when
 * input.device is unavailable. */
static void rknb_warp(struct Screen *scr, int sx, int sy) {
    struct IEPointerPixel px;
    struct InputEvent ev;
    if (!s_inreq || !scr)
        return;
    px.iepp_Screen = scr;
    px.iepp_Position.X = (WORD)sx;
    px.iepp_Position.Y = (WORD)sy;
    ev.ie_NextEvent = NULL;
    ev.ie_Class = IECLASS_NEWPOINTERPOS;
    ev.ie_SubClass = IESUBCLASS_PIXEL;
    ev.ie_Code = IECODE_NOBUTTON;
    ev.ie_Qualifier = 0;
    ev.ie_X = 0;
    ev.ie_Y = 0;
    ev.ie_EventAddress = (APTR)&px;
    s_inreq->io_Command = IND_ADDEVENT;
    s_inreq->io_Data = (APTR)&ev;
    s_inreq->io_Length = sizeof(ev);
    DoIO((struct IORequest *)s_inreq);
}

BOOPSI_DISPATCHER_PROTO(IPTR, rknb_dispatcher, Class *, Object *, Msg);

/* Forward: defined below the dispatcher, used inside it. */
LONG ri_rknb_drag_value(LONG start, double dx_px, double dy_px,
    BOOL fine);
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
        d->acc_dx = 0.0;
        d->acc_dy = 0.0;
        d->last_x = 0;
        d->last_y = 0;
        d->warp_wx = 0;
        d->warp_wy = 0;
        d->warp_sx = 0;
        d->warp_sy = 0;
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
                struct Window *w;
                if (!rknb_hit(obj, im->MouseX, im->MouseY))
                    return (IPTR)0;
                d->dragging = TRUE;
                d->acc_dx = 0.0;
                d->acc_dy = 0.0;
                d->last_x = im->MouseX;
                d->last_y = im->MouseY;
                d->warp_wx = im->MouseX;
                d->warp_wy = im->MouseY;
                d->drag_start_val = d->cur;
                /* Warp target in screen coords. TRAP (m28): IDCMP
                 * MouseX/Y are relative to the window's OUTER origin,
                 * so screen = LeftEdge + Mouse — do NOT add Border*
                 * (borders offset the RastPort/content space, which is
                 * where _left/_top live and already measured exact).
                 * Adding the border biased EVERY warp by (10,25):
                 * runaway values + dead reversal. */
                w = _window(obj);
                if (w) {
                    d->warp_sx = (WORD)(w->LeftEdge + im->MouseX);
                    d->warp_sy = (WORD)(w->TopEdge + im->MouseY);
                } else {
                    d->warp_sx = -1000;
                    d->warp_sy = -1000;
                }
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
            d->acc_dx += (double)(im->MouseX - d->last_x);
            d->acc_dy += (double)(d->last_y - im->MouseY);
            d->last_x = im->MouseX;
            d->last_y = im->MouseY;
            /* Clamp travel to the value range: reversal bites at
             * once instead of unwinding dead overshoot (the grab
             * pins the pointer, so overshoot is unbounded). */
            ri_knob_clamp_acc((double)d->drag_start_val, &d->acc_dx,
                &d->acc_dy, fine);
            v = ri_rknb_drag_value(d->drag_start_val, d->acc_dx,
                d->acc_dy, fine);
            ri_rknb_move(&d->gesture);
            SetAttrs(obj, MUIA_Numeric_Value, v, TAG_DONE);
            /* Pointer grab, revised (edge-triggered warp yanked the
             * pointer across the screen on long drags — hostile):
             * warp back to the drag start after EVERY move, so the
             * pointer hovers near the knob while physical motion
             * accrues 1:1 into acc (next delta is measured from the
             * warp target). Edges become unreachable; no teleports.
             * Fail-soft: without input.device this block is skipped
             * and acc degrades exactly to absolute positioning. The
             * warp's own mousemove lands on last_* → zero delta. */
            if (s_inreq) {
                struct Screen *s = _screen(obj);
                if (s) {
                    rknb_warp(s, d->warp_sx, d->warp_sy);
                    d->last_x = d->warp_wx;
                    d->last_y = d->warp_wy;
                }
            }
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
        if (s_rknb_class)
            rknb_input_open();
    }
    return s_rknb_class;
}

void ri_rknb_dispose_class(void) {
    if (s_rknb_class) {
        MUI_DeleteCustomClass(s_rknb_class);
        s_rknb_class = NULL;
    }
    rknb_input_close();
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

/* Drag pixels → quantized ctl value (both axes: right and up
 * increase; screen-y negated first, screen-x passes through).
 * Shift passes fine=1 (×0.1, 1500 px full). */
LONG ri_rknb_drag_value(LONG start, double dx_px, double dy_px,
    BOOL fine) {
    double v = ri_knob_drag_to_value((double)start, dx_px, dy_px,
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
