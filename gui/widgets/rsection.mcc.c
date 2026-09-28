/*
 * rsection.mcc.c — RSection: every laid-out ReBirth section as one canvas
 * (§12.10 G4).
 *
 * AROS-ONLY. Subclasses MUIC_Area. What exists and where comes from the
 * registry + measured geometry; what a click does comes from gui/sectui.h;
 * this file only renders and routes the pointer. Section looks are
 * clean-room approximations of the ReBirth panels (manual figures p. 148,
 * p. 153): light synth panel with black keyboard block; dark 808 panel with
 * red LEVEL knobs, white parameter knobs, cream instrument legends and the
 * TR-808 step colours (1-4 red, 5-8 orange, 9-12 yellow, 13-16 white).
 * Knobs/selectors: press + drag (up/right increase, P-18 law in the
 * control's own range), [Shift] fine, right-click = default. Buttons,
 * steps, switches and instrument legends act on press.
 * Lessons kept: pens per screen in Setup (palette screens ignore RGB pens);
 * event handler added in Setup on _win(obj) (never _window); paint on every
 * Draw (Zune does not flag the initial show-time Draw).
 * Must NEVER enter the host build (audit gates widgets/).
 */

#ifndef __AROS__
#error "rsection.mcc.c is AROS-only: Zune custom class, never in the host build"
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
#define __CYBERGRAPHICS_LIBBASE s_rcyber
#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include <clib/alib_protos.h>
#include <string.h>
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/panels.h"
#include "gui/sectui.h"
#include "gui/knob_logic.h"
#include "gui/skin.h"
#include "gui/skin_aros.h"
#include "engine/dsp/kernels.h"
#include "gui/widgets/rsection.h"
#include "app/core/canvas_events.h"

#include "gui/draw/art.h"

#define TAG_SECTION (TAG_USER + 0x52534381u)
#define TAG_ZOOM (TAG_USER + 0x52534382u)

static LONG s_pens[C_NCOL];

struct RSectionData {
    struct RISectUI ui;
    LONG zoom;
    LONG changes;
    struct RICevState cev;   /* drag + arrow-repeat (T3b: owned by canvas_events) */
    BOOL shown;
    struct MUI_EventHandlerNode ehn;
    struct RSectionDiag diag;
    struct RIPanelUI *panel;   /* shared front panel (focus, keys), may be NULL */
    BOOL key_owner;
    struct BitMap *bm;         /* off-screen frame: paint whole, blit once (no flicker) */
    struct RastPort brp;
    int bw, bh;
    char help[64];             /* bubble text for the control under the pointer */
};

/* Own cybergraphics base for exact-colour fills (never the knob_blit global). */
static struct Library *s_rcyber;
#include "gui/widgets/rsection_replay.inc"

static void pens_obtain(Object *obj) {
    struct ColorMap *cm = _screen(obj)->ViewPort.ColorMap;
    ULONG i;
    s_direct_rgb = GetBitMapAttr(_screen(obj)->RastPort.BitMap, BMA_DEPTH) > 8;
    if (s_direct_rgb && !s_rcyber)
        s_rcyber = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    for (i = 0; i < C_NCOL; i++) {
        ULONG c = ri_art_rgb((int)i);
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

/* Double buffer (Dell 2026-09-26: knob drags flickered because every change
 * repainted background then controls straight into the window). The frame is
 * painted into a friend bitmap of the window's and blitted in one go; if the
 * bitmap cannot be had, paint direct as before. */
static void buf_free(struct RSectionData *d) {
    if (d->bm) {
        WaitBlit();
        FreeBitMap(d->bm);
    }
    d->bm = NULL;
    d->bw = d->bh = 0;
}

static void draw_frame(Object *obj, struct RSectionData *d) {
    struct RastPort *wrp = _rp(obj);
    int w = _mwidth(obj), h = _mheight(obj);
    if (w <= 0 || h <= 0)
        return;
    if (!d->bm || d->bw != w || d->bh != h) {
        buf_free(d);
        d->bm = AllocBitMap((ULONG)w, (ULONG)h, GetBitMapAttr(wrp->BitMap, BMA_DEPTH), BMF_MINPLANES, wrp->BitMap);
        if (d->bm) {
            InitRastPort(&d->brp);
            d->brp.BitMap = d->bm;
            d->bw = w;
            d->bh = h;
        }
    }
    if (!d->bm) {
        draw_section(wrp, d, _mleft(obj), _mtop(obj));
        return;
    }
    SetFont(&d->brp, wrp->Font);
    draw_section(&d->brp, d, 0, 0);
    BltBitMapRastPort(d->bm, 0, 0, wrp, _mleft(obj), _mtop(obj), w, h, 0xC0);
}

static void changed(Object *obj, struct RSectionData *d) {
    d->changes++;
    SetAttrs(obj, MUIA_RSection_Changes, d->changes, TAG_DONE);
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

static const struct RIGeoSection *geo(const struct RSectionData *d) {
    return ri_geo_section(d->ui.section == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : d->ui.section);
}

BOOPSI_DISPATCHER_PROTO(IPTR, rsection_dispatcher, Class *, Object *, Msg);

BOOPSI_DISPATCHER(IPTR, rsection_dispatcher, cl, obj, msg) {
    struct RSectionData *d;
    switch (msg->MethodID) {
    case OM_NEW: {
        struct opSet *s = (struct opSet *)msg;
        Object *o = (Object *)DoSuperMethodA(cl, obj, msg);
        if (!o)
            return (IPTR)NULL;
        d = (struct RSectionData *)INST_DATA(cl, o);
        memset(&d->diag, 0, sizeof d->diag);
        d->diag.last_x = d->diag.last_y = -1;
        d->diag.last_hit = 0xFFFFu;
        if (ri_sui_init(&d->ui, (uint8_t)GetTagData(TAG_SECTION, RI_SEC_SYNTH1, s->ops_AttrList)) != 0) {
            CoerceMethod(cl, o, OM_DISPOSE);
            return (IPTR)NULL;
        }
        d->zoom = (LONG)GetTagData(TAG_ZOOM, 0, s->ops_AttrList);
        d->changes = 0;
        ri_cev_init(&d->cev);
        d->shown = FALSE;
        d->panel = (struct RIPanelUI *)GetTagData(MUIA_RSection_Panel, 0, s->ops_AttrList);
        d->key_owner = (BOOL)GetTagData(MUIA_RSection_KeyOwner, FALSE, s->ops_AttrList);
        return (IPTR)o;
    }
    case OM_SET: {
        struct opSet *os = (struct opSet *)msg;
        struct TagItem *tag;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        if ((tag = FindTagItem(MUIA_RSection_Panel, os->ops_AttrList)) != NULL)
            d->panel = (struct RIPanelUI *)tag->ti_Data;
        if ((tag = FindTagItem(MUIA_RSection_KeyOwner, os->ops_AttrList)) != NULL)
            d->key_owner = (BOOL)(tag->ti_Data != 0);
        return DoSuperMethodA(cl, obj, msg);
    }
    case OM_GET: {
        struct opGet *gm = (struct opGet *)msg;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        if (gm->opg_AttrID == MUIA_RSection_Changes) {
            *gm->opg_Storage = (IPTR)d->changes;
            return (IPTR)1;
        }
        if (gm->opg_AttrID == MUIA_RSection_State) {
            *gm->opg_Storage = (IPTR)&d->ui;
            return (IPTR)1;
        }
        if (gm->opg_AttrID == MUIA_RSection_Diag) {
            *gm->opg_Storage = (IPTR)&d->diag;
            return (IPTR)1;
        }
        return DoSuperMethodA(cl, obj, msg);
    }
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        const struct RIGeoSection *g;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        g = geo(d);
        m->MinMaxInfo->MinWidth += ri_geo_px(g->w, (int)d->zoom);
        m->MinMaxInfo->MinHeight += ri_geo_px(g->h, (int)d->zoom);
        m->MinMaxInfo->DefWidth = m->MinMaxInfo->MaxWidth = m->MinMaxInfo->MinWidth;
        m->MinMaxInfo->DefHeight = m->MinMaxInfo->MaxHeight = m->MinMaxInfo->MinHeight;
        return rc;
    }
    case MUIM_Setup: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        if (rc) {
            d = (struct RSectionData *)INST_DATA(cl, obj);
            pens_obtain(obj);
            d->ehn.ehn_Priority = 0;
            d->ehn.ehn_Flags = 0;
            d->ehn.ehn_Object = obj;
            d->ehn.ehn_Class = cl;
            d->ehn.ehn_Events = IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE | IDCMP_INTUITICKS |
                (d->key_owner ? IDCMP_RAWKEY : 0);
            DoMethod(_win(obj), MUIM_Window_AddEventHandler, &d->ehn); /* _win, never _window */
            d->diag.setups++;
        }
        return rc;
    }
    case MUIM_Cleanup:
        d = (struct RSectionData *)INST_DATA(cl, obj);
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, &d->ehn);
        buf_free(d);
        pens_release(obj);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_Show: {
        IPTR rc = DoSuperMethodA(cl, obj, msg);
        d = (struct RSectionData *)INST_DATA(cl, obj);
        d->shown = TRUE;
        d->diag.shows++;
        return rc;
    }
    case MUIM_Hide:
        d = (struct RSectionData *)INST_DATA(cl, obj);
        d->shown = FALSE;
        ri_cev_init(&d->cev);
        return DoSuperMethodA(cl, obj, msg);
    case MUIM_CreateShortHelp: { /* per-control bubble (Zune re-asks after each move) */
        struct MUIP_CreateShortHelp *m = (struct MUIP_CreateShortHelp *)msg;
        int opt = -1;
        uint16_t id;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        id = ri_geo_hit_opt(geo(d), (int)m->mx - _mleft(obj), (int)m->my - _mtop(obj), (int)d->zoom, &opt);
        if (id == 0xFFFFu) {
            /* Background bubble on pattern blocks (owner 2026-09-27):
             * which device this block affects + its role. */
            uint32_t sec = d->ui.section;
            if (sec >= RI_SEC_PAT_SYNTH1 && sec <= RI_SEC_PAT_909 &&
                ri_panel_role_label(sec - RI_SEC_PAT_SYNTH1, d->help, (uint32_t)sizeof d->help))
                return (IPTR)d->help;
            return (IPTR)0;
        }
        if (!ri_ctlreg_help(id, opt, d->help, (uint32_t)sizeof d->help))
            return (IPTR)0;
        return (IPTR)d->help;
    }
    case MUIM_DeleteShortHelp:
        return (IPTR)TRUE;         /* text lives in instance data */
    case MUIM_Draw:
        DoSuperMethodA(cl, obj, msg);
        draw_frame(obj, (struct RSectionData *)INST_DATA(cl, obj));
        return (IPTR)0;
    case MUIM_HandleEvent: {
        struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
        struct IntuiMessage *im = m->imsg;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        if (!im || !d->shown)
            return (IPTR)0;
        if (im->Class == IDCMP_INTUITICKS) {   /* held arrow repeats (p. 18) */
            if (ri_cev_tick(&d->cev, &d->ui) & RI_CEV_CHANGED)
                changed(obj, d);
            return (IPTR)0;
        }
        d->diag.events++;
        if (im->Class == IDCMP_RAWKEY) {   /* Appendix E: one owner per window */
            uint32_t r = ri_cev_key(&d->cev, &d->ui, d->panel,
                d->key_owner ? 1 : 0, im->Code, im->Qualifier);
            if (r & RI_CEV_CHANGED)
                changed(obj, d);
            return (r & RI_CEV_EAT) ? (IPTR)MUI_EventHandlerRC_Eat : (IPTR)0;
        }
        if (im->Class == IDCMP_MOUSEBUTTONS) {
            int lx = im->MouseX - _mleft(obj), ly = im->MouseY - _mtop(obj), opt = -1;
            int kind = (im->Code == SELECTDOWN) ? 0 : (im->Code == SELECTUP) ? 1 :
                (im->Code == MENUDOWN) ? 2 : -1;
            uint32_t r;
            d->diag.buttons++;
            d->diag.last_x = lx;
            d->diag.last_y = ly;
            d->diag.last_hit = ri_geo_hit_opt(geo(d), lx, ly, (int)d->zoom, &opt);
            if (kind < 0)
                return (IPTR)0;
            r = ri_cev_button(&d->cev, &d->ui, d->panel, geo(d), (int)d->zoom,
                lx, ly, _mwidth(obj), _mheight(obj), kind);
            if (r & RI_CEV_CHANGED)
                changed(obj, d);
            return (r & RI_CEV_EAT) ? (IPTR)MUI_EventHandlerRC_Eat : (IPTR)0;
        }
        if (im->Class == IDCMP_MOUSEMOVE && d->cev.drag_id != RI_CEV_NODRAG) {
            int fine = (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
            uint32_t r = ri_cev_move(&d->cev, &d->ui, im->MouseX, im->MouseY, fine);
            if (r & RI_CEV_CHANGED)
                changed(obj, d);
            return (r & RI_CEV_EAT) ? (IPTR)MUI_EventHandlerRC_Eat : (IPTR)0;
        }
        return (IPTR)0;
    }
    default:
        return DoSuperMethodA(cl, obj, msg);
    }
}
BOOPSI_DISPATCHER_END

static struct MUI_CustomClass *s_rsection_class = NULL;

struct MUI_CustomClass *ri_rsection_class(void) {
    if (!s_rsection_class)
        s_rsection_class = MUI_CreateCustomClass(NULL, MUIC_Area, NULL,
            sizeof(struct RSectionData), (APTR)rsection_dispatcher);
    return s_rsection_class;
}

void ri_rsection_dispose_class(void) {
    if (s_rsection_class) {
        MUI_DeleteCustomClass(s_rsection_class);
        s_rsection_class = NULL;
    }
    if (s_rcyber) {
        CloseLibrary(s_rcyber);
        s_rcyber = NULL;
    }
}

APTR ri_rsection_create(ULONG section, LONG zoom) {
    struct MUI_CustomClass *mcc = ri_rsection_class();
    if (!mcc)
        return NULL;
    return (APTR)NewObject(mcc->mcc_Class, NULL,
        TAG_SECTION, section,
        TAG_ZOOM, zoom,
        MUIA_FillArea, FALSE,
        MUIA_ShortHelp, (IPTR)" ", /* non-NULL so Zune asks MUIM_CreateShortHelp */
        TAG_DONE);
}

void ri_rsection_refresh(APTR obj) {
    if (obj)
        MUI_Redraw((Object *)obj, MADF_DRAWOBJECT);
}
