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
#include <exec/devices.h>
#include <stdint.h>
#include <intuition/classes.h>
#include <intuition/classusr.h>
#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <graphics/view.h>
#include <devices/inputevent.h>
#include <devices/timer.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/timer.h>
#define __CYBERGRAPHICS_LIBBASE s_rcyber
#include <cybergraphx/cybergraphics.h>
#include <inline/cybergraphics.h>
#include <proto/muimaster.h>
#include <proto/utility.h>
#include "gui/panelui.h" /* RI_RSEC_BOX_* + ri_rsection_box_why */
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
    int dmg_x0, dmg_y0, dmg_x1, dmg_y1; /* S3: canvas-local damage box */
    BOOL dmg_valid;
    int dmg_why;                 /* RI_RSEC_BOX_* who asked for this box */
};

/* Reason the caller is about to ask for a box repaint, set by
 * ri_rsection_set_box_why and consumed by the next refresh_box on any
 * canvas. A module global because the GUI has one caller
 * (meter_round) and threading it through every call site would change
 * signatures for no gain; both run on the GUI task. */
static int s_box_why;

/* EClock draw timing (S3 Dell proof): UNIT_ECLOCK opened once, GUI side.
 * TimerBase is weak: RIAPP already defines it strong (audio task side);
 * standalone links (RISECT) resolve to this zero fallback. */
__attribute__((weak)) struct Device *TimerBase;
static struct MsgPort *s_tport;
static struct timerequest *s_treq;
static ULONG s_efreq;

static void eclock_open(void) {
    struct EClockVal t0;
    if (TimerBase) {
        /* RIAPP's audio side opened timer.device first: still read the
         * EClock rate, else every draw timing is dropped (draw n=0). */
        if (!s_efreq)
            s_efreq = ReadEClock(&t0);
        return;
    }
    s_tport = CreateMsgPort();
    if (s_tport)
        s_treq = (struct timerequest *)CreateIORequest(s_tport, sizeof *s_treq);
    if (s_treq && OpenDevice((STRPTR)"timer.device", UNIT_ECLOCK,
        (struct IORequest *)s_treq, 0) == 0) {
        TimerBase = s_treq->tr_node.io_Device;
        s_efreq = ReadEClock(&t0);
    }
}

static ULONG eclock_us(const struct EClockVal *a, const struct EClockVal *b) {
    uint64_t x = ((uint64_t)a->ev_hi << 32) | (uint64_t)a->ev_lo;
    uint64_t y = ((uint64_t)b->ev_hi << 32) | (uint64_t)b->ev_lo;
    if (y < x || !s_efreq)
        return 0u;
    return (ULONG)((y - x) * 1000000ULL / s_efreq);
}

/* Wall clock for tap tempo (P9e), in ms. 0 when timer.device is not open:
 * the estimator treats that as "no clock" and ignores the tap, which is
 * the honest answer — a tap with a guessed time would set a wrong tempo. */
static ULONG eclock_ms(void) {
    struct EClockVal t0;
    uint64_t v;
    if (!s_efreq)
        return 0u;
    (void)ReadEClock(&t0);
    v = ((uint64_t)t0.ev_hi << 32) | (uint64_t)t0.ev_lo;
    return (ULONG)(v * 1000ULL / s_efreq);
}

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

static const struct RIGeoSection *geo(const struct RSectionData *d);

static void draw_frame(Object *obj, struct RSectionData *d) {
    struct RastPort *wrp = _rp(obj);
    int w = _mwidth(obj), h = _mheight(obj);
    struct EClockVal t0, t1, tb;
    int timed = 0;
    ULONG us;
    if (w <= 0 || h <= 0)
        return;
    if (!d->bm || d->bw != w || d->bh != h) {
        buf_free(d);
        d->bm = AllocBitMap((ULONG)w, (ULONG)h, GetBitMapAttr(wrp->BitMap, BMA_DEPTH), BMF_MINPLANES, wrp->BitMap);
        d->diag.alloc_n++;
        if (d->bm) {
            InitRastPort(&d->brp);
            d->brp.BitMap = d->bm;
            d->bw = w;
            d->bh = h;
        }
        d->dmg_valid = FALSE; /* fresh bitmap: full paint below */
    }
    if (!d->bm) {
        draw_section(wrp, d, _mleft(obj), _mtop(obj));
        return;
    }
    SetFont(&d->brp, wrp->Font);
    eclock_open();
    if (TimerBase && s_efreq) {
        ReadEClock(&t0);
        timed = 1;
    }
    if (d->dmg_valid) {
        struct ri_dlist dl;
        int x0 = d->dmg_x0, y0 = d->dmg_y0, x1 = d->dmg_x1, y1 = d->dmg_y1;
        d->dmg_why = RI_RSEC_BOX_NONE;
        d->dmg_valid = FALSE;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= w) x1 = w - 1;
        if (y1 >= h) y1 = h - 1;
        if (x1 >= x0 && y1 >= y0) {
            build_dl(&d->brp, d, 0, 0, &dl); /* CPU only; cheap vs blits */
            if (replay_dl_dmg(&d->brp, &dl, ri_skin_aros_for(d->ui.section),
                x0, y0, x1, y1)) {
                BltBitMapRastPort(d->bm, x0, y0, wrp, _mleft(obj) + x0, _mtop(obj) + y0,
                    x1 - x0 + 1, y1 - y0 + 1, 0xC0);
                if (timed) {
                    int why = d->dmg_why;
                    ReadEClock(&t1);
                    us = eclock_us(&t0, &t1);
                    if (us > d->diag.dp_max)
                        d->diag.dp_max = us;
                    d->diag.dp_sum += us;
                    d->diag.dp_n++;
                    /* The same sample also lands in its reason bucket, so
                     * the aggregate dp_* and the per-reason split always
                     * agree on the count. */
                    if (us > d->diag.dpw_max[why])
                        d->diag.dpw_max[why] = us;
                    d->diag.dpw_sum[why] += us;
                    d->diag.dpw_n[why]++;
                }
                return;
            }
            /* bail-out (system text / imageless skin): full below */
        }
    }
    draw_section(&d->brp, d, 0, 0);
    if (timed)
        ReadEClock(&tb);
    BltBitMapRastPort(d->bm, 0, 0, wrp, _mleft(obj), _mtop(obj), w, h, 0xC0);
    if (timed) {
        ReadEClock(&t1);
        us = eclock_us(&tb, &t1);
        if (us > d->diag.blit_max)
            d->diag.blit_max = us;
        us = eclock_us(&t0, &t1);
        if (us > d->diag.df_max)
            d->diag.df_max = us;
        d->diag.df_sum += us;
        d->diag.df_n++;
    }
}

static void changed_id(Object *obj, struct RSectionData *d, uint16_t hit) {
    d->changes++;
    SetAttrs(obj, MUIA_RSection_Changes, d->changes, TAG_DONE);
    /* S3: a damage-box id repaints its box (DRAWUPDATE); wide controls,
     * unknown boxes and keys (panel routing may touch any canvas) go full. */
    if (hit != 0xFFFFu && !ri_geo_wide(hit) && d->bm) {
        const struct RIGeoSection *g = geo(d);
        int x0, y0, x1, y1;
        if (g && ri_geo_bbox(g, hit, (int)d->zoom, &x0, &y0, &x1, &y1) == 0) {
            d->dmg_x0 = x0;
            d->dmg_y0 = y0;
            d->dmg_x1 = x1;
            d->dmg_y1 = y1;
            d->dmg_valid = TRUE;
            MUI_Redraw(obj, MADF_DRAWUPDATE);
            return;
        }
    }
    d->dmg_valid = FALSE;
    MUI_Redraw(obj, MADF_DRAWOBJECT);
}

static void changed(Object *obj, struct RSectionData *d) {
    changed_id(obj, d, d->diag.last_hit);
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
        d->dmg_valid = FALSE;
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
        if ((tag = FindTagItem(MUIA_RSection_Zoom, os->ops_AttrList)) != NULL) {
            LONG z = (LONG)tag->ti_Data;
            /* Transport lives compact; content canvases take 0..2. */
            if ((d->ui.section == RI_SEC_TRANSPORT) ? (z == RI_GEO_ZOOM_COMPACT)
                : (z >= 0 && z <= 2)) {
                if (z != d->zoom) {
                    d->zoom = z;
                    buf_free(d);
                    d->dmg_valid = FALSE;
                }
            }
        }
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
        d = (struct RSectionData *)INST_DATA(cl, obj);
        /* Only our own DRAWUPDATE may use the damage box. A box queued
         * while the canvas sat on a hidden tab (meters, chase lamps)
         * would otherwise turn the page switch's full draw into a box
         * repaint and leave the previous tab on screen (owner Dell
         * 2026-09-30). */
        if (!(((struct MUIP_Draw *)msg)->flags & MADF_DRAWUPDATE))
            d->dmg_valid = FALSE;
        draw_frame(obj, d);
        return (IPTR)0;
    case MUIM_HandleEvent: {
        struct MUIP_HandleEvent *m = (struct MUIP_HandleEvent *)msg;
        struct IntuiMessage *im = m->imsg;
        d = (struct RSectionData *)INST_DATA(cl, obj);
        if (!im || !d->shown)
            return (IPTR)0;
        if (im->Class == IDCMP_INTUITICKS) {   /* held arrow repeats (p. 18) */
            if (ri_cev_tick(&d->cev, &d->ui) & RI_CEV_CHANGED)
                changed_id(obj, d, d->cev.rep_idx);
            return (IPTR)0;
        }
        d->diag.events++;
        if (im->Class == IDCMP_RAWKEY) {   /* Appendix E: one owner per window */
            uint32_t r = ri_cev_key(&d->cev, &d->ui, d->panel,
                d->key_owner ? 1 : 0, im->Code, im->Qualifier);
            if (r & RI_CEV_CHANGED)
                changed_id(obj, d, 0xFFFFu); /* panel routing: full */
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
                lx, ly, _mwidth(obj), _mheight(obj), kind,
                kind == 0 ? eclock_ms() : 0u);
            if (r & RI_CEV_CHANGED)
                changed(obj, d);
            return (r & RI_CEV_EAT) ? (IPTR)MUI_EventHandlerRC_Eat : (IPTR)0;
        }
        if (im->Class == IDCMP_MOUSEMOVE && d->cev.drag_id != RI_CEV_NODRAG) {
            int fine = (im->Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) != 0;
            uint32_t r = ri_cev_move(&d->cev, &d->ui, im->MouseX, im->MouseY, fine);
            if (r & RI_CEV_CHANGED)
                changed_id(obj, d, d->cev.drag_id);
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
    face_templates_free();
    if (TimerBase && s_treq) {
        CloseDevice((struct IORequest *)s_treq);
        TimerBase = NULL;
    }
    if (s_treq) {
        DeleteIORequest((struct IORequest *)s_treq);
        s_treq = NULL;
    }
    if (s_tport) {
        DeleteMsgPort(s_tport);
        s_tport = NULL;
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

void ri_rsection_refresh_box_why(APTR obj, int x0, int y0, int x1, int y1, int why) {
    Object *o = (Object *)obj;
    struct RSectionData *d;
    int w, h;
    if (!o)
        return;
    if (s_rsection_class && !((struct RSectionData *)INST_DATA(s_rsection_class->mcc_Class, o))->shown)
        return; /* hidden: MUIM_Show's full draw covers it */
    if (!s_rsection_class || x1 < x0 || y1 < y0) {
        MUI_Redraw(o, MADF_DRAWOBJECT);
        return;
    }
    d = (struct RSectionData *)INST_DATA(s_rsection_class->mcc_Class, o);
    w = _mwidth(o);
    h = _mheight(o);
    if (w <= 0 || h <= 0 || !d->bm) {
        MUI_Redraw(o, MADF_DRAWOBJECT);
        return;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= w) x1 = w - 1;
    if (y1 >= h) y1 = h - 1;
    if (x1 < x0 || y1 < y0) {
        MUI_Redraw(o, MADF_DRAWOBJECT);
        return;
    }
    d->dmg_x0 = x0;
    d->dmg_y0 = y0;
    d->dmg_x1 = x1;
    d->dmg_y1 = y1;
    d->dmg_why = ri_rsection_box_why(why);
    d->dmg_valid = TRUE;
    MUI_Redraw(o, MADF_DRAWUPDATE);
}

void ri_rsection_set_box_why(int why) {
    s_box_why = ri_rsection_box_why(why);
}

void ri_rsection_refresh_box(APTR obj, int x0, int y0, int x1, int y1) {
    ri_rsection_refresh_box_why(obj, x0, y0, x1, y1, s_box_why);
    s_box_why = RI_RSEC_BOX_NONE;
}
