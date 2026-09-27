/* art_section.c — full section dispatcher (portability plan T2).
 * Transliterated from draw_section in gui/widgets/rsection.mcc.c.
 */
#include "gui/draw/art.h"

#include <string.h>
#include "gui/ctlreg.h"
#include "gui/panelgeo.h"
#include "gui/sectui.h"
#include "gui/panelui.h"
#include "gui/knob_logic.h"
#include "gui/skin.h"
#include "gui/sect303.h"
#include "gui/sect808.h"
#include "gui/sect909.h"
#include "gui/sectmix.h"
#include "gui/sectfx.h"
#include "gui/sectpat.h"
#include "gui/secttr.h"
#include "engine/dsp/kernels.h"

static long sec_to_n(const struct RICtlDef *d, int v) {
    int span = d->max_v - d->min_v;
    return span > 0 ? (long)(((long)(v - d->min_v) * 127 + span / 2) / span) : 0;
}

/* Skin image emit: part slot idx of skin at (x, y). Backend blits the zoom
 * cache entry (AROS) or its decoded part (host). */
static void sec_image(struct ri_dlist *dl, int x, int y, int idx, uint32_t frame) {
    ri_draw_image(dl, x, y, (uint16_t)idx, (uint16_t)frame);
}

void ri_draw_section(struct ri_dlist *out, const struct RISectUI *ui, uint8_t section, int zoom,
    int ox, int oy, const struct ri_text_metrics *tm, const struct RISkin *skin,
    const struct RIPanelUI *panel) {
    const struct RIGeoSection *g;
    uint8_t gs;
    int z = zoom, is808, is909, ismix, isfx, ispat, istr;
    int txt, skinned = 0;
    uint32_t i;
    char buf[4];
#define PX(q) ri_geo_px((q), z)
    if (!out || !ui)
        return;
    gs = section == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : section;
    g = ri_geo_section(gs);
    is808 = section == RI_SEC_808;
    is909 = section == RI_SEC_909;
    ismix = ri_smix_strip(section) >= 0;
    isfx = section >= RI_SEC_PCF && section <= RI_SEC_COMP;
    ispat = section >= RI_SEC_PAT_SYNTH1 && section <= RI_SEC_PAT_909;
    istr = section == RI_SEC_TRANSPORT;
    txt = is808 ? C_CREAM : ismix || isfx || ispat || istr ? C_MIX_TEXT : C_TEXT;
    if (!g)
        return;
    if (skin) {
        int bg = ri_skin_find(skin, gs, RI_CK_KNOB, "background");
        if (bg >= 0 && skin->parts[bg].frames > 0u) {
            sec_image(out, ox, oy, bg, 0u);
            skinned = 1;
        }
    }
    if (skinned && ismix) {
        int k;
        if (section == RI_SEC_MASTER)
            for (k = 0; k < 8; k++)
                ri_art_line(out, ox + PX(128), oy + PX(120 + 25 * k), ox + PX(202), oy + PX(120 + 25 * k), C_MIX_KNOB);
        else
            for (k = 0; k < 7; k++)
                ri_art_line(out, ox + PX(32), oy + PX(262 + 28 * k), ox + PX(118), oy + PX(262 + 28 * k), C_MIX_KNOB);
    }
    if (!skinned) {
        if (is808)
            ri_art_bg_808(out, g, ox, oy, z);
        else if (is909)
            ri_art_bg_909(out, g, ox, oy, z);
        else if (ismix)
            ri_art_bg_mix(out, g, ox, oy, z, section == RI_SEC_MASTER);
        else if (isfx)
            ri_art_bg_fx(out, g, ox, oy, z, section);
        else if (ispat)
            ri_art_bg_pat(out, g, ox, oy, z, section);
        else if (istr)
            ri_art_bg_tr(out, g, ox, oy, z, tm);
        else
            ri_art_bg_303(out, g, ox, oy, z);
    }
    if (ispat)
        ri_art_focus_bar(out, section, panel, ox, oy, z);
    for (i = 0; i < g->nitems; i++) {
        const struct RIGeoItem *it = &g->items[i];
        uint32_t idx = it->reg_id & 0xFFu;
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((section << 8) | idx));
        int cx = ox + PX(it->cx), cy = oy + PX(it->cy);
        int hw = PX(it->w) / 2, hh = PX(it->h) / 2;
        int v;
        if (!d)
            continue;
        v = ri_sui_value(ui, idx);
        switch (it->shape) {
        case RI_GEO_KNOB:
            if (d->kind == RI_CK_SELECTOR) {             /* 808 instrument selector */
                ri_art_knob(out, cx, cy, PX(it->w), PX(it->h), C_BLACK, C_LED_ON, 0,
                    208.0f + 28.2f * (float)v);
            } else {
                int face = d->bind == RI_BIND_NONE ? C_DISABLED
                    : is909 ? C_909_KNOB : ismix || isfx || istr ? C_MIX_KNOB
                    : !is808 ? C_KNOB : !strcmp(d->legend, "Level") ? C_KNOB_RED : C_KNOB_WHITE;
                int done = 0;
                if (skin) {
                    const char *part = ri_skin_knob_role(gs, (uint32_t)ri_geo_px(it->w, 2),
                        (uint32_t)ri_geo_px(it->h, 2));
                    if (part) {
                        int pi = ri_skin_find(skin, gs, RI_CK_KNOB, part);
                        if (pi >= 0 && skin->parts[pi].frames > 0u) {
                            sec_image(out, cx - hw, cy - hh, pi,
                                ri_skin_frame((uint32_t)sec_to_n(d, v), 127u, skin->parts[pi].frames));
                            done = 1;
                        }
                    }
                }
                if (!done)
                    ri_art_knob(out, cx, cy, PX(it->w), PX(it->h), face, is909 ? C_909_ORANGE : C_BLACK, !is808,
                        (float)ri_knob_pointer_mdeg((int)sec_to_n(d, v)) / 1000.0f);
            }
            break;
        case RI_GEO_RECT:
            if (ispat && idx == RI_SPAT_LENGTH) {
                ri_art_led_digits(out, cx - hw, cy - hh, cx + hw, cy + hh, v, 2);
            } else if (ispat && idx == RI_SPAT_SHUFFLE) {
                ri_art_bevel(out, cx - hw, cy - hh, cx + hw, cy + hh, v ? C_PAT_SEL : C_WHITEKEY);
            } else if (ispat) {                                            /* section lamp */
                ri_art_bevel(out, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                ri_art_rect(out, cx - hw + 3, cy - hh + 3, cx + hw - 3, cy + hh - 3,
                    ri_sui_led(ui, idx, 0) ? C_MIX_GREEN : C_MIX_GREEN_OFF);
            } else if (istr && d->kind == RI_CK_DISPLAY) {
                ri_art_led_digits(out, cx - hw, cy - hh, cx + hw, cy + hh, v, 3);
            } else if (istr && d->kind == RI_CK_BUTTON) {
                ri_art_tr_key(out, cx - hw, cy - hh, cx + hw, cy + hh, idx, ri_sui_led(ui, idx, 0), z);
            } else if (istr && d->kind == RI_CK_LED) {
                ri_art_circle(out, cx, cy, hw + 1, v == 2 ? C_MIX_GREEN : v ? C_LED_ON : C_LED_OFF);
            } else if (istr) {                                             /* Pattern/Song, Loop levers */
                ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                ri_art_bevel(out, cx - hw + 2, v ? cy - hh + 2 : cy + 1, cx + hw - 2, v ? cy - 1 : cy + hh - 2, C_BTN);
            } else if (isfx && d->kind == RI_CK_METER && idx == RI_SFX_COMP_GR) {
                ri_art_gr_row(out, cx - hw, cy - hh, cx + hw, cy + hh, v);
            } else if (isfx && d->kind == RI_CK_SELECTOR) {
                ri_art_led_digits(out, cx - hw, cy - hh, cx + hw, cy + hh, v, 2);
            } else if (isfx && d->kind == RI_CK_SWITCH && idx != RI_SFX_ONOFF) {  /* vertical lever */
                ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                ri_art_bevel(out, cx - hw + 2, v ? cy - hh + 2 : cy + 1, cx + hw - 2, v ? cy - 1 : cy + hh - 2, C_BTN);
            } else if (ismix || isfx) {
                if (d->kind == RI_CK_METER) {
                    ri_art_meter(out, cx - hw, cy - hh, cx + hw, cy + hh, v, section == RI_SEC_MASTER ? 12 : isfx ? 3 : 4,
                        section == RI_SEC_MASTER);
                } else if (d->kind == RI_CK_FADER) {
                    ri_art_fader(out, cx, cy - hh, cy + hh, 2 * hw, PX(it->w) * 3 / 5, (int)sec_to_n(d, v), skinned);
                } else if (idx == 0 && section != RI_SEC_MASTER) {   /* mute / bypass lamp button */
                    ri_art_bevel(out, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                    ri_art_rect(out, cx - hw + 3, cy - hh + 3, cx + hw - 3, cy + hh - 3,
                        ri_sui_led(ui, idx, 0) ? C_MIX_GREEN : C_MIX_GREEN_OFF);
                } else {                                                    /* insert rocker */
                    ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_MIX_SLOT);
                    ri_art_bevel(out, cx - hw + 2, cy - hh + 2, cx + hw - 2, cy + hh - 2,
                        ri_sui_led(ui, idx, 0) ? C_WHITEKEY : C_BTN);
                }
            } else if (d->kind == RI_CK_DISPLAY) {
                int n = ri_sui_display(ui, idx);
                ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_SEG_BG);
                buf[0] = (char)('0' + n / 10);
                buf[1] = (char)('0' + n % 10);
                buf[2] = 0;
                ri_art_text_c(out, cx, cy, buf, C_SEG);
            } else if (d->kind == RI_CK_STEP && is808) {
                uint32_t st = idx - RI_S808_STEP0;
                ri_art_bevel(out, cx - hw, cy - hh, cx + hw, cy + hh, ri_art_step_colour_808(st));
                ri_art_rect(out, cx - hw / 3, cy - hh + 3, cx + hw / 3, cy - hh + 3 + PX(10),
                    ri_art_chase(panel, section, st) ? C_WHITEKEY : ri_sui_led(ui, idx, 0) ? C_LED_ON : C_LAMP_OFF);
            } else if (d->kind == RI_CK_STEP && is909) {
                int st = ri_sui_led(ui, idx, 0);
                int lamp = st == 1 ? (ri_sui_value(ui, RI_S909_SELECT) == 0 ? C_LED_ON : C_LAMP_LOW)
                    : st == 2 ? C_LED_ON : st == 3 ? C_LAMP_FLAM : C_LAMP_OFF;
                if (ri_art_chase(panel, section, idx - RI_S909_STEP0))
                    lamp = C_WHITEKEY;
                ri_art_key_909(out, cx - hw, cy - hh, cx + hw, cy + hh, z, lamp);
            } else if (d->kind == RI_CK_SWITCH && is909) {     /* Flam button */
                ri_art_key_909(out, cx - hw, cy - hh, cx + hw, cy + hh, z, v ? C_LED_ON : C_LAMP_OFF);
            } else if (d->kind == RI_CK_SWITCH && is808) {     /* sound switch: slot + lever */
                ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_BLACK);
                if (v)
                    ri_art_rect(out, cx - hw + 2, cy, cx + hw - 2, cy + hh - 2, C_BTN);
                else
                    ri_art_rect(out, cx - hw + 2, cy - hh + 2, cx + hw - 2, cy, C_BTN);
            } else if (section != RI_SEC_808 && idx == RI_S303_WAVE) {
                ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_BLACK);
                if (v)
                    ri_art_bevel(out, cx + 2, cy - hh + 2, cx + hw - 2, cy + hh - 2, C_BTN);
                else
                    ri_art_bevel(out, cx - hw + 2, cy - hh + 2, cx - 2, cy + hh - 2, C_BTN);
                ri_art_text_c(out, v ? cx + hw / 2 : cx - hw / 2, cy, v ? "SQR" : "SAW", C_TEXT);
            } else {
                ri_art_bevel(out, cx - hw, cy - hh, cx + hw, cy + hh, C_BTN);
            }
            break;
        case RI_GEO_OPTION: {
            int lit = v == it->opt;
            if (ispat) {                                  /* numbered / lettered key */
                char t[2];
                t[0] = (char)(idx == RI_SPAT_BANK ? 'A' + it->opt : '1' + it->opt);
                t[1] = 0;
                ri_art_bevel(out, cx - hw + 1, cy - hh + 1, cx + hw - 1, cy + hh - 1, lit ? C_PAT_SEL : C_WHITEKEY);
                ri_art_text_c(out, cx, cy, t, C_BLACK);
                break;
            }
            if (is909) {                                  /* outlined legend box, LED left */
                const char *t = ri_art_909_opt((uint32_t)it->opt);
                int tw = (tm && tm->width) ? tm->width(tm->ctx, t) : 0;
                if (tw + PX(40) > 2 * hw)
                    t = it->opt == 1 ? "BASS" : it->opt == 2 ? "SNARE" : t; /* bar above names the drum */
                ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, C_909_BAR);
                ri_art_rect(out, cx - hw + 1, cy - hh + 1, cx + hw - 1, cy + hh - 1, C_909_PANEL);
                ri_art_circle(out, cx - hw + PX(12), cy, PX(5), lit ? C_LED_ON : C_LED_OFF);
                ri_art_text_c(out, cx + PX(8), cy, t, C_BLACK);
                break;
            }
            ri_art_rect(out, cx - hw, cy - hh, cx + hw, cy + hh, lit ? C_CREAM_LIT : C_CREAM);
            if (lit)
                ri_art_line(out, cx - hw, cy + hh, cx + hw, cy + hh, C_LED_ON);
            ri_art_text_c(out, cx, cy, ri_art_808_opt((uint32_t)it->opt), C_BLACK);
            break;
        }
        case RI_GEO_LED: {
            uint32_t which = 0, j;
            for (j = 0; j < i; j++)
                if (g->items[j].reg_id == it->reg_id && g->items[j].shape == RI_GEO_LED)
                    which++;
            ri_art_circle(out, cx, cy, PX(it->w) / 2 + 1, ri_sui_led(ui, idx, which)
                ? (istr ? C_MIX_GREEN : C_LED_ON) : (istr ? C_MIX_GREEN_OFF : C_LED_OFF));
            break;
        }
        case RI_GEO_STEPPER:
            ri_art_arrow_btn(out, cx - hw, cy - hh, cx + hw, cy + hh, it->opt != 0);
            break;
        case RI_GEO_LEGEND: {
            /* inverse only on the 303's black Down/Up/Accent/Slide strip: the same
             * indices on the 909 (CP/CH/OH/CC) drew white labels (Dell 2026-09-26) */
            int is303 = section == RI_SEC_SYNTH1 || section == RI_SEC_SYNTH2;
            int col = (is303 && idx >= RI_S303_DOWN && idx <= RI_S303_SLIDE) ? C_TEXT_INV : txt;
            const char *s = d->legend;
            if (is303 && idx == RI_S303_DISPLAY)
                s = "EDIT STEP";
            else if (is808 && d->kind == RI_CK_SWITCH) /* the alternate sound's legend */
                s = idx == 9 ? "LC" : idx == 12 ? "MC" : idx == 15 ? "HC" : idx == 17 ? "CL" : "MA";
            else if (is909)
                s = ri_art_legend_909(s);
            else if (isfx || istr)
                s = ri_art_legend_fx(s);
            else if (ispat)
                s = idx == RI_SPAT_LENGTH ? "STEPS" : s;
            ri_art_text_c(out, cx, cy, s, col);
            break;
        }
        case RI_GEO_DIVIDER:
            ri_art_line(out, cx, cy, cx, cy + PX(it->h), is808 ? C_808_LINE : C_BLACK);
            break;
        default:
            break;
        }
    }
#undef PX
}
