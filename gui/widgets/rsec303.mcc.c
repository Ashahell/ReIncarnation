/*
 * rsec303.mcc.c — RSec303: the whole 303 section as one canvas (§12.10 G4).
 *
 * AROS-ONLY. Subclasses MUIC_Area. Every pixel is drawn here from:
 *   gui/ctlreg.h   — which controls exist, legends, ranges, defaults
 *   gui/panelgeo.h — where they are (measured from the manual figure)
 *   gui/sect303.h  — what a click does (ReBirth step-entry rules)
 * so this file holds only rendering and pointer plumbing. Look is a
 * clean-room approximation of the ReBirth synth panel (light panel, black
 * keyboard block, silver knobs with tick rings, red LEDs, red EDIT STEP
 * readout); no artwork is copied.
 * Knobs: press + drag (up/right increase, P-18 law in the control's own
 * range), [Shift] fine, right-click = default. Buttons/keys act on press.
 * Must NEVER enter the host build (audit gates widgets/).
 */

#ifndef __AROS__
#error "rsec303.mcc.c is AROS-only: Zune custom class, never in the host build"
#endif

#include <exec/types.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <devices/inputevent.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/sect303.h"
#include "gui/knob_logic.h"
#include "engine/dsp/kernels.h"
#include "gui/widgets/rsec303.h"

/* Clean-room palette (0xRRGGBB); pens obtained per screen in MUIM_Setup so
 * drawing works on palette (LUT) and truecolour screens alike. */
enum { C_PANEL, C_PANEL_DK, C_BLACK, C_WHITEKEY, C_BTN, C_BTN_HI, C_BTN_LO,
       C_LED_ON, C_LED_OFF, C_KNOB, C_KNOB_RIM, C_TICK, C_TEXT, C_TEXT_INV,
       C_SEG_BG, C_SEG, C_DISABLED, C_NCOL };
static const ULONG RI_S303_RGB[C_NCOL] = {
    0xD6D6CEu, 0x8C8C84u, 0x141414u, 0xF4F4F0u, 0xB4B4AEu, 0xF0F0EAu, 0x5A5A56u,
    0xFF2A1Au, 0x5A1410u, 0xC8C8C4u, 0x3C3C3Cu, 0x2A2A2Au, 0x1E1E1Eu, 0xF0F0F0u,
    0x280808u, 0xFF3020u, 0x9C9C96u
};
static LONG s_pens[C_NCOL]; /* valid between Setup and Cleanup */

struct RSec303Data {
    struct RISect303 st;
    LONG zoom;
    LONG changes;
    uint16_t drag_id;     /* reg_id being dragged, 0xFFFF none */
    LONG drag_n0;         /* start position in 0..127 knob space */
    double acc_dx, acc_dy;
    WORD last_x, last_y;
    BOOL shown;
    struct MUI_EventHandlerNode ehn;
    struct RSec303Diag diag;
};

static void pen(struct RastPort *rp, ULONG col) {
    SetAPen(rp, (ULONG)s_pens[col < C_NCOL ? col : C_BLACK]);
}

static void pens_obtain(Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    ULONG i;
    for (i = 0; i < C_NCOL; i++) {
        ULONG c = RI_S303_RGB[i];
        s_pens[i] = ObtainBestPen(cm, (c >> 16 & 0xFF) * 0x01010101u,
            (c >> 8 & 0xFF) * 0x01010101u, (c & 0xFF) * 0x01010101u,
            OBP_Precision, PRECISION_EXACT, TAG_DONE);
    }
}

static void pens_release(Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    ULONG i;
    for (i = 0; i < C_NCOL; i++)
        if (s_pens[i] >= 0)
            ReleasePen(cm, (ULONG)s_pens[i]);
}

static void fill_rect(struct RastPort *rp, int x0, int y0, int x1, int y1, ULONG rgb) {
    if (x1 < x0 || y1 < y0)
        return;
    pen(rp, rgb);
    RectFill(rp, x0, y0, x1, y1);
}

static void fill_circle(struct RastPort *rp, int cx, int cy, int r, ULONG rgb) {
    int dy;
    pen(rp, rgb);
    for (dy = -r; dy <= r; dy++) {
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r)
            dx--;
        RectFill(rp, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void line(struct RastPort *rp, int x0, int y0, int x1, int y1, ULONG rgb) {
    pen(rp, rgb);
    Move(rp, x0, y0);
    Draw(rp, x1, y1);
}

static void text_c(struct RastPort *rp, int cx, int cy, const char *s0, ULONG rgb) {
    char s[24]; /* panel legends are upper case, as on the ReBirth panel */
    int n = 0, w;
    while (s0[n] && n < 23) {
        s[n] = (s0[n] >= 'a' && s0[n] <= 'z') ? (char)(s0[n] - 32) : s0[n];
        n++;
    }
    s[n] = 0;
    pen(rp, rgb);
    SetDrMd(rp, JAM1);
    w = TextLength(rp, (STRPTR)s, n);
    Move(rp, cx - w / 2, cy + rp->TxBaseline / 2);
    Text(rp, (STRPTR)s, n);
}

static void bevel(struct RastPort *rp, int x0, int y0, int x1, int y1, BOOL in) {
    fill_rect(rp, x0, y0, x1, y1, C_BTN);
    line(rp, x0, y0, x1, y0, in ? C_BTN_LO : C_BTN_HI);
    line(rp, x0, y0, x0, y1, in ? C_BTN_LO : C_BTN_HI);
    line(rp, x0, y1, x1, y1, in ? C_BTN_HI : C_BTN_LO);
    line(rp, x1, y0, x1, y1, in ? C_BTN_HI : C_BTN_LO);
}

/* value -> 0..127 knob space, and back, in the control's own range */
static LONG to_n(const struct RICtlDef *d, int v) {
    int span = d->max_v - d->min_v;
    return span > 0 ? (LONG)(((long)(v - d->min_v) * 127 + span / 2) / span) : 0;
}
static int from_n(const struct RICtlDef *d, LONG n) {
    int span = d->max_v - d->min_v;
    return d->min_v + (int)(((long)n * span + 63) / 127);
}

static void draw_knob(struct RastPort *rp, int cx, int cy, int body, int ring,
    const struct RICtlDef *d, int v) {
    int k, rb = body / 2, rr = ring / 2;
    for (k = 0; k <= 10; k++) {                 /* 11 ticks, -135..+135 deg */
        float a = (float)((-135 + 27 * k) - 90) * 0.0174533f;
        float c = ri_sin(a + 1.5707963f), s = ri_sin(a);
        line(rp, cx + (int)(c * (float)(rb + 2)), cy + (int)(s * (float)(rb + 2)),
            cx + (int)(c * (float)rr), cy + (int)(s * (float)rr), C_TICK);
    }
    fill_circle(rp, cx, cy, rb, C_KNOB_RIM);
    fill_circle(rp, cx, cy, rb - 2, d->bind != RI_BIND_NONE ? C_KNOB : C_DISABLED);
    {
        int mdeg = ri_knob_pointer_mdeg((int)to_n(d, v));
        float a = ((float)mdeg / 1000.0f - 90.0f) * 0.0174533f;
        float c = ri_sin(a + 1.5707963f), s = ri_sin(a);
        line(rp, cx + (int)(c * (float)(rb / 3)), cy + (int)(s * (float)(rb / 3)),
            cx + (int)(c * (float)(rb - 3)), cy + (int)(s * (float)(rb - 3)), C_BLACK);
    }
}

static void draw_section(Object *obj, struct RSec303Data *dd) {
    struct RastPort *rp = _rp(obj);
    const struct RIGeoSection *g = ri_geo_section(dd->st.section == RI_SEC_SYNTH2 ?
        RI_SEC_SYNTH1 : dd->st.section);
    int ox = _mleft(obj), oy = _mtop(obj), z = (int)dd->zoom;
    uint32_t i;
    char buf[4];
#define PX(q) ri_geo_px((q), z)
    if (!g)
        return;
    fill_rect(rp, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, C_PANEL);
    line(rp, ox, oy + PX(185), ox + PX(g->w) - 1, oy + PX(185), C_PANEL_DK);
    /* keyboard block: black frame, white key bodies, black key bodies */
    fill_rect(rp, ox + PX(185), oy + PX(200), ox + PX(855), oy + PX(435), C_BLACK);
    for (i = 0; i < 13; i++) {
        static const int black[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
        const struct RIGeoItem *it = 0;
        uint32_t j;
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT)
                it = &g->items[j];
        if (it && !black[i])
            fill_rect(rp, ox + PX(it->cx - 38), oy + PX(208), ox + PX(it->cx + 38), oy + PX(428), C_WHITEKEY);
    }
    for (i = 0; i < 13; i++) { /* black key bodies over the white keys */
        static const int black2[13] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0 };
        uint32_t j;
        if (!black2[i])
            continue;
        for (j = 0; j < g->nitems; j++)
            if ((g->items[j].reg_id & 0xFFu) == RI_S303_KEY0 + i && g->items[j].shape == RI_GEO_RECT)
                fill_rect(rp, ox + PX(g->items[j].cx - 28), oy + PX(208),
                    ox + PX(g->items[j].cx + 28), oy + PX(320), C_BLACK);
    }
    fill_rect(rp, ox + PX(860), oy + PX(290), ox + PX(1250), oy + PX(320), C_BLACK);
    for (i = 0; i < g->nitems; i++) {
        const struct RIGeoItem *it = &g->items[i];
        uint32_t idx = it->reg_id & 0xFFu;
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((dd->st.section << 8) | idx));
        int cx = ox + PX(it->cx), cy = oy + PX(it->cy);
        if (!d)
            continue;
        switch (it->shape) {
        case RI_GEO_KNOB:
            draw_knob(rp, cx, cy, PX(it->w), PX(it->h), d, dd->st.val[idx]);
            break;
        case RI_GEO_RECT: {
            int hw = PX(it->w) / 2, hh = PX(it->h) / 2;
            if (idx == RI_S303_DISPLAY) {
                int n = ri_s303_display(&dd->st);
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_SEG_BG);
                buf[0] = (char)('0' + n / 10);
                buf[1] = (char)('0' + n % 10);
                buf[2] = 0;
                text_c(rp, cx, cy, buf, C_SEG);
            } else if (idx >= RI_S303_KEY0 && idx < RI_S303_KEY0 + 13u) {
                bevel(rp, cx - hw, cy - hh, cx + hw, cy + hh, FALSE);
            } else if (idx == RI_S303_WAVE) {
                /* saw | square switch: lever on the selected side */
                fill_rect(rp, cx - hw, cy - hh, cx + hw, cy + hh, C_BLACK);
                if (dd->st.val[idx])
                    bevel(rp, cx + 2, cy - hh + 2, cx + hw - 2, cy + hh - 2, FALSE);
                else
                    bevel(rp, cx - hw + 2, cy - hh + 2, cx - 2, cy + hh - 2, FALSE);
                text_c(rp, cx - hw / 2, cy, dd->st.val[idx] ? "" : "SAW", C_TEXT);
                text_c(rp, cx + hw / 2, cy, dd->st.val[idx] ? "SQR" : "", C_TEXT);
            } else {
                bevel(rp, cx - hw, cy - hh, cx + hw, cy + hh, FALSE);
            }
            break;
        }
        case RI_GEO_LED: {
            /* Note/Pause owns two LEDs: first = note, second = pause */
            uint32_t which = 0, j;
            for (j = 0; j < i; j++)
                if (g->items[j].reg_id == it->reg_id && g->items[j].shape == RI_GEO_LED)
                    which++;
            fill_circle(rp, cx, cy, PX(it->w) / 2 + 1,
                ri_s303_led(&dd->st, idx, which) ? C_LED_ON : C_LED_OFF);
            break;
        }
        case RI_GEO_LEGEND: {
            ULONG col = (idx >= RI_S303_DOWN && idx <= RI_S303_SLIDE) ? C_TEXT_INV : C_TEXT;
            text_c(rp, cx, cy, idx == RI_S303_DISPLAY ? "EDIT STEP" : d->legend, col);
            break;
        }
        case RI_GEO_DIVIDER:
            line(rp, cx, cy, cx, cy + PX(it->h), C_BLACK);
            break;
        default:
            break;
        }
    }
#undef PX
}

static void changed(Object *obj, struct RSec303Data *d) {
    d->changes++;
    SetAttrs(obj, MUIA_RSec303_Changes, d->changes, TAG_DONE);
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

BOOPSI_DISPATCHER_PROTO(IPTR, rsec303_dispatcher, Class *, Object *, Msg);

BOOPSI_DISPATCHER(IPTR, rsec303_dispatcher, cl, obj, msg) {
    struct RSec303Data *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet *s = (struct opSet *)msg;
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!o)
            return (IPTR)NULL;
        d = (struct RSec303Data *)INST_DATA(cl, o);
        ri_s303_init(&d->st, (uint8_t)GetTagData(TAG_USER + 0x52533381u, RI_SEC_SYNTH1,
            s->ops_AttrList));
        d->zoom = (LONG)GetTagData(TAG_USER + 0x52533382u, 0, s->ops_AttrList);
        d->changes = 0;
        d->diag.events = 0;
        d->diag.setups = 0;
        d->diag.shows = 0;
        d->diag.buttons = 0;
        d->diag.last_x = -1;
        d->diag.last_y = -1;
        d->diag.last_hit = 0xFFFFu;
        d->drag_id = 0xFFFFu;
        d->shown = FALSE;
        return (IPTR)o;
    }
    case OM_GET: {
        struct opGet *gm = (struct opGet *)msg;
        d = (struct RSec303Data *)INST_DATA(cl, obj);
        if (gm->opg_AttrID == MUIA_RSec303_Changes) {
            *gm->opg_Storage = (IPTR)d->changes;
            return (IPTR)1;
        }
        if (gm->opg_AttrID == MUIA_RSec303_Diag) {
            *gm->opg_Storage = (IPTR)&d->diag;
            return (IPTR)1;
        }
        if (gm->opg_AttrID == MUIA_RSec303_State) {
            *gm->opg_Storage = (IPTR)&d->st;
            return (IPTR)1;
        }
        return DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        const struct RIGeoSection *g = ri_geo_section(RI_SEC_SYNTH1);
        d = (struct RSec303Data *)INST_DATA(cl, obj);
        m->MinMaxInfo->MinWidth += ri_geo_px(g->w, (int)d->zoom);
        m->MinMaxInfo->MinHeight += ri_geo_px(g->h, (int)d->zoom);
        m->MinMaxInfo->DefWidth = m->MinMaxInfo->MinWidth;
        m->MinMaxInfo->DefHeight = m->MinMaxInfo->MinHeight;
        m->MinMaxInfo->MaxWidth = m->MinMaxInfo->MinWidth;
        m->MinMaxInfo->MaxHeight = m->MinMaxInfo->MinHeight;
        return rc;
    }
    case MUIM_Setup: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        if (rc) {
            d = (struct RSec303Data *)INST_DATA(cl, obj);
            pens_obtain(obj);
            d->ehn.ehn_Priority = 0;
            d->ehn.ehn_Flags = 0;
            d->ehn.ehn_Object = obj;
            d->ehn.ehn_Class = cl;
            d->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE;
            DoMethod(_win(obj), MUIM_Window_AddEventHandler, &d->ehn); /* _win, never _window */
            d->diag.setups++;
        }
        return rc;
    }
    case MUIM_Cleanup:
        d = (struct RSec303Data *)INST_DATA(cl, obj);
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->ehn);
        pens_release(obj);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        d = (struct RSec303Data *)INST_DATA(cl, obj);
        d->shown = TRUE; /* Area's Show result is not a gate on Zune */
        d->diag.shows++;
        return rc;
    }
    case MUIM_Hide:
        d = (struct RSec303Data *)INST_DATA(cl, obj);
        d->shown = FALSE;
        d->drag_id = 0xFFFFu;
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Draw:
        /* paint on any Draw (Zune does not flag the initial show-time Draw) */
        DoSuperMethodA(cl, obj, msg);
        draw_section(obj, (struct RSec303Data *)INST_DATA(cl, obj));
        return (IPTR)0;
    case MUIM_HandleEvent: {
        struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
        struct IntuiMessage *im = m->imsg;
        const struct RIGeoSection *g = ri_geo_section(RI_SEC_SYNTH1);
        d = (struct RSec303Data *)INST_DATA(cl, obj);
        if (!im || !d->shown)
            return (IPTR)0;
        d->diag.events++;
        if (im->Class == IDCMP_MOUSEBUTTONS) {
            int lx = im->MouseX - _mleft(obj), ly = im->MouseY - _mtop(obj);
            uint16_t id = ri_geo_hit(g, lx, ly, (int)d->zoom);
            d->diag.buttons++;
            d->diag.last_x = lx;
            d->diag.last_y = ly;
            d->diag.last_hit = id;
            if (im->Code == SELECTUP) {
                if (d->drag_id == 0xFFFFu)
                    return (IPTR)0;
                d->drag_id = 0xFFFFu;
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
            if (id == 0xFFFFu)
                return (IPTR)0;
            {
                uint32_t idx = id & 0xFFu;
                const struct RICtlDef *cd = ri_ctlreg_find((uint16_t)((d->st.section << 8) | idx));
                if (!cd)
                    return (IPTR)0;
                if (im->Code == MENUDOWN) {
                    if (ri_s303_reset(&d->st, idx))
                        changed(obj, d);
                    return (IPTR)MUI_EventHandlerRC_Eat;
                }
                if (im->Code != SELECTDOWN)
                    return (IPTR)0;
                if (cd->kind == RI_CK_KNOB) {
                    d->drag_id = id;
                    d->drag_n0 = to_n(cd, d->st.val[idx]);
                    d->acc_dx = 0.0;
                    d->acc_dy = 0.0;
                    d->last_x = im->MouseX;
                    d->last_y = im->MouseY;
                } else if (ri_s303_press(&d->st, idx)) {
                    changed(obj, d);
                }
                return (IPTR)MUI_EventHandlerRC_Eat;
            }
        }
        if (im->Class == IDCMP_MOUSEMOVE && d->drag_id != 0xFFFFu) {
            uint32_t idx = d->drag_id & 0xFFu;
            const struct RICtlDef *cd = ri_ctlreg_find((uint16_t)((d->st.section << 8) | idx));
            int fine = (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
            double n;
            d->acc_dx += (double)(im->MouseX - d->last_x);
            d->acc_dy += (double)(d->last_y - im->MouseY);
            d->last_x = im->MouseX;
            d->last_y = im->MouseY;
            ri_knob_clamp_acc((double)d->drag_n0, &d->acc_dx, &d->acc_dy, fine);
            n = ri_knob_drag_to_value((double)d->drag_n0, d->acc_dx, d->acc_dy, fine);
            if (cd && ri_s303_set_value(&d->st, idx, from_n(cd, (LONG)ri_ctl_quantize(n))))
                changed(obj, d);
            return (IPTR)MUI_EventHandlerRC_Eat;
        }
        return (IPTR)0;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

static struct MUI_CustomClass *s_rsec303_class = NULL;

struct MUI_CustomClass *ri_rsec303_class(void) {
    if (!s_rsec303_class)
        s_rsec303_class = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
            sizeof(struct RSec303Data), (APTR)rsec303_dispatcher);
    return s_rsec303_class;
}

void ri_rsec303_dispose_class(void) {
    if (s_rsec303_class) {
        MUI_DeleteCustomClass(s_rsec303_class);
        s_rsec303_class = NULL;
    }
}

APTR ri_rsec303_create(ULONG section, LONG zoom) {
    struct MUI_CustomClass *mcc = ri_rsec303_class();
    if (!mcc)
        return NULL;
    return (APTR)NewObject(mcc->mcc_Class, NULL,
        TAG_USER + 0x52533381u, section,
        TAG_USER + 0x52533382u, zoom,
        MUIA_FillArea, FALSE,
        TAG_DONE);
}
