/*
 * rstp.mcc.c — RStp step-button custom MUI class (Module 2.10 device).
 *
 * AROS-ONLY. Subclasses MUIC_Numeric (spec §13 widget reuse: min/max/
 * value + notify inherited) with own 32 px rendering + click-toggle
 * gesture. Stock MUIC_Numericbutton ignores FixWidth/Height (proven
 * on device m36: 14 px targets, clicks landed between buttons), so a
 * skin cannot size it — custom class like RKnB. Chase-LED highlight
 * and accent/flam glows arrive with the beat clock (later slice);
 * this slice owns geometry + toggle + notify. Must NEVER enter the
 * host build (audit gates it).
 */

#ifndef __AROS__
#error "rstp.mcc.c is AROS-only: Zune custom class, never in the host build"
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
#include "gui/knob_logic.h"
#include "gui/knob_blit.h"
#include "gui/widgets/rstp.h"

#define RSTP_PX 32 /* frame edge */
#define RSTP_OFF 0x3a3a38u /* step off: dark grey */
#define RSTP_ON 0xc04028u /* step on: warm red (ReBirth LED language) */
#define RSTP_EDGE 0x1c1c1au /* frame edge: near-black */

struct RStpData {
    BOOL armed; /* press began inside (click completes on release) */
    BOOL shown;
    struct MUI_EventHandlerNode ehn;
};

BOOPSI_DISPATCHER_PROTO(IPTR, rstp_dispatcher, Class *, Object *, Msg);

static int rstp_hit(Object *obj, WORD mx, WORD my) {
    return mx >= _left(obj) && mx < _left(obj) + _width(obj) &&
        my >= _top(obj) && my < _top(obj) + _height(obj);
}

/* Paint the 32 px step: fill by state + 1 px edge on all sides. */
static void rstp_paint(struct RastPort *rp, int x, int y, int on) {
    uint32_t fill = on ? RSTP_ON : RSTP_OFF;
    ri_knob_panel_rect(rp, x, y, RSTP_PX, RSTP_PX, fill);
    ri_knob_panel_rect(rp, x, y, RSTP_PX, 1, RSTP_EDGE);
    ri_knob_panel_rect(rp, x, y + RSTP_PX - 1, RSTP_PX, 1, RSTP_EDGE);
    ri_knob_panel_rect(rp, x, y, 1, RSTP_PX, RSTP_EDGE);
    ri_knob_panel_rect(rp, x + RSTP_PX - 1, y, 1, RSTP_PX, RSTP_EDGE);
}

BOOPSI_DISPATCHER(IPTR, rstp_dispatcher, cl, obj, msg) {
    struct RStpData *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!o)
            return (IPTR)NULL;
        d = (struct RStpData *)INST_DATA(cl, o);
        d->armed = FALSE;
        d->shown = FALSE;
        return (IPTR)o;
    }
    case OM_SET: {
        struct opSet *s = (struct opSet *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        d = (struct RStpData *)INST_DATA(cl, obj);
        if (d->shown && FindTagItem(MUIA_Numeric_Value, s->ops_AttrList))
            DoMethod(obj, MUIM_Draw, MADF_DRAWOBJECT);
        return rc;
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth = RSTP_PX;
        m->MinMaxInfo->MinHeight = RSTP_PX;
        m->MinMaxInfo->MaxWidth = RSTP_PX;
        m->MinMaxInfo->MaxHeight = RSTP_PX;
        m->MinMaxInfo->DefWidth = RSTP_PX;
        m->MinMaxInfo->DefHeight = RSTP_PX;
        return rc;
    }
    case MUIM_Setup:
    case MUIM_Cleanup:
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        if (rc) {
            d = (struct RStpData *)INST_DATA(cl, obj);
            d->shown = TRUE;
            d->ehn.ehn_Priority = 0;
            d->ehn.ehn_Flags = MUI_EHF_GUIMODE;
            d->ehn.ehn_Object = obj;
            d->ehn.ehn_Class = cl;
            d->ehn.ehn_Events = IDCMP_MOUSEBUTTONS;
            /* _win (MUI window OBJECT), never _window: MUIM_Window_*
             * on an Intuition Window* dispatches through garbage
             * (m21 guru, illegal address). */
            DoMethod(_win(obj), MUIM_Window_AddEventHandler,
                &d->ehn);
        }
        return rc;
    }
    case MUIM_Hide: {
        d = (struct RStpData *)INST_DATA(cl, obj);
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->ehn);
        d->shown = FALSE;
        d->armed = FALSE;
        return DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_Draw: {
        /* Paint on ANY Draw: show-time Draw carries no DRAWOBJECT
         * flag on this Zune (m22 verdict on knobs, same toolkit). */
        LONG v = 0;
        d = (struct RStpData *)INST_DATA(cl, obj);
        (void)d;
        DoMethod(obj, OM_GET, MUIA_Numeric_Value, &v);
        rstp_paint(_rp(obj), _left(obj), _top(obj), v != 0);
        return (IPTR)0;
    }
    case MUIM_HandleEvent: {
        struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
        struct IntuiMessage *im = m->imsg;
        d = (struct RStpData *)INST_DATA(cl, obj);
        if (!im)
            return (IPTR)0;
        if (im->Class != IDCMP_MOUSEBUTTONS)
            return (IPTR)0;
        if (im->Code == SELECTDOWN) {
            if (!rstp_hit(obj, im->MouseX, im->MouseY))
                return (IPTR)0;
            d->armed = TRUE;
            return (IPTR)MUI_EventHandlerRC_Eat;
        }
        if (im->Code == SELECTUP) {
            if (!d->armed)
                return (IPTR)0;
            d->armed = FALSE;
            if (rstp_hit(obj, im->MouseX, im->MouseY)) {
                LONG v = 0;
                DoMethod(obj, OM_GET, MUIA_Numeric_Value, &v);
                SetAttrs(obj, MUIA_Numeric_Value,
                    v ? 0 : 1, TAG_DONE);
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

static struct MUI_CustomClass *s_rstp_class = NULL;

struct MUI_CustomClass *ri_rstp_class(void) {
    if (!s_rstp_class) {
        s_rstp_class = MUI_CreateCustomClass(MUIMasterBase, MUIC_Numeric,
            NULL, sizeof(struct RStpData), (APTR)rstp_dispatcher);
    }
    return s_rstp_class;
}

void ri_rstp_dispose_class(void) {
    if (s_rstp_class) {
        MUI_DeleteCustomClass(s_rstp_class);
        s_rstp_class = NULL;
    }
}

/* Create the step button (0/1, initial 0). Click toggles
 * (ri_step_toggle, host-pinned); artwork/silhouette acceptance is
 * recorded in docs/evidence/gui/acceptance.md, not asserted here. */
APTR ri_rstp_create(void) {
    struct MUI_CustomClass *mcc = ri_rstp_class();
    if (!mcc)
        return NULL;
    return (APTR)NewObject(mcc->mcc_Class, NULL,
        MUIA_Numeric_Min, 0,
        MUIA_Numeric_Max, 1,
        MUIA_Numeric_Value, 0,
        TAG_DONE);
}

/* Click toggles the step (0 → 1 → 0). */
LONG ri_rstp_click(LONG state) {
    return (LONG)ri_step_toggle((int)state);
}

/* Chase position for a transport beat position (16ths); the caller
 * lights that step's LED iff ri_rstp_led_due reports on time. */
LONG ri_rstp_chase(double beat_pos_16ths, LONG nsteps) {
    return (LONG)ri_chase_step(beat_pos_16ths, (int)nsteps);
}

int ri_rstp_led_due(double lag_ms) {
    return ri_led_lag_ok(lag_ms);
}
