/* art.h — backend-free section art (portability plan T2).
 * Transliterated from gui/widgets/rsection.mcc.c painters: same geometry,
 * same colours (as RGB tokens), same strings. Backends replay the list
 * (AROS pens + friend bitmap, host software rasterizer).
 * Text pointers in the list are borrowed (literals or caller-stable
 * buffers) and valid until the next ri_dlist_clear; both backends replay
 * synchronously inside the draw call.
 */
#ifndef RI_DRAW_ART_H
#define RI_DRAW_ART_H
#include <stdint.h>

#include "gui/draw/canvas.h"
#include "gui/sectui.h"
#include "gui/panelui.h"
#include "gui/panelgeo.h"
#include "gui/skin.h"

/* Panel palette indices (were the pen table in rsection.mcc.c). */
enum {
    C_PANEL, C_PANEL_DK, C_BLACK, C_WHITEKEY, C_BTN, C_BTN_HI, C_BTN_LO,
    C_LED_ON, C_LED_OFF, C_KNOB, C_KNOB_RIM, C_TICK, C_TEXT, C_TEXT_INV,
    C_SEG_BG, C_SEG, C_DISABLED,
    C_808_PANEL, C_808_LINE, C_KNOB_RED, C_KNOB_WHITE, C_CREAM, C_CREAM_LIT,
    C_STEP_RED, C_STEP_ORANGE, C_STEP_YELLOW, C_STEP_WHITE, C_LAMP_OFF,
    C_909_PANEL, C_909_BAR, C_909_ORANGE, C_909_KNOB, C_LAMP_LOW, C_LAMP_FLAM, C_909_STEP,
    C_MIX_PANEL, C_MIX_HEAD, C_MIX_HEADTX, C_MIX_SLOT, C_MIX_GREEN, C_MIX_GREEN_OFF, C_MIX_KNOB, C_MIX_TEXT,
    C_FX_PANEL, C_FX_HEAD, C_SEG_DIM, C_PAT_HEAD, C_PAT_SEL, C_TR_PANEL,
    C_LEVI_PANEL,
    C_LEVI_HEAD,
    C_LEVI_RULE,
    /* Leviasynth hardware panel (fidelity plan P1, 2026-09-30): graphite
     * boxes, teal legends on black caps, aluminium knobs, per-osc cap
     * colours (1 teal .. 8 blue), LCD. Own values, measured by eye. */
    C_LEVI_BOX, C_LEVI_EDGE, C_LEVI_TEAL, C_LEVI_CAP, C_LEVI_SILVER, C_LEVI_LABEL, C_LEVI_LCD,
    C_LEVI_O2, C_LEVI_O3, C_LEVI_O4, C_LEVI_O5, C_LEVI_O6, C_LEVI_O7, C_LEVI_O8, C_LEVI_REDTX,
    C_LEVI_DIM, C_LEVI_TEALDIM, C_LEVI_LABELDIM, C_LEVI_REDDIM,
    C_NCOL
};

uint32_t ri_art_rgb(int idx);
/* Reverse map RGB -> palette index (AROS pen lookup); -1 when unknown. */
int ri_art_index(uint32_t rgb);

/* Shared painters (all coords canvas px, colours as C_* indices). */
void ri_art_rect(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int col);
void ri_art_line(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int col);
void ri_art_circle(struct ri_dlist *dl, int cx, int cy, int r, int col);
void ri_art_bevel(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int face);
/* Centred upper-cased legend (text_c equivalent). */
void ri_art_text_c(struct ri_dlist *dl, int cx, int cy, const char *s0, int col);
/* Centred upper-cased legend in an explicit face (S2 headers that read
 * above the section face, e.g. mixer strip names). Face outside 1..3
 * falls back to the list state (ri_draw_text_face clamps). */
void ri_art_text_c_face(struct ri_dlist *dl, int cx, int cy, const char *s0, int col, int face);
/* Hardware-look finish (2026-09-28 GUI review): colour math, shaded
 * discs, panels, screws, LEDs. Colours here are RGB (0xRRGGBB); backends
 * replay any RGB (AROS: direct colour on hi/truecolor screens, nearest
 * palette pen on palette screens). */
uint32_t ri_art_mix(uint32_t a, uint32_t b, int t256);   /* 0 = a .. 256 = b */
uint32_t ri_art_shade(uint32_t c, int pct);              /* +: toward white, -: toward black */
int ri_art_luma(uint32_t c);                             /* 0..255 */
void ri_art_disc_grad(struct ri_dlist *dl, int cx, int cy, int r, uint32_t top, uint32_t bot);
void ri_art_panel(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t base, int brushed);
void ri_art_screw(struct ri_dlist *dl, int cx, int cy, int r, uint32_t panel);
void ri_art_led(struct ri_dlist *dl, int cx, int cy, int r, int col, int lit, uint32_t panel);
/* Knob: shadow + skirt + graded body + concave cap + highlight + thick
 * pointer; panel = the colour it sits on (shadow + tick contrast). */
void ri_art_knob(struct ri_dlist *dl, int cx, int cy, int body, int ring, int face,
    int ptr, int ticks, float deg, uint32_t panel);
void ri_art_meter(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int level, int nseg, int master);
void ri_art_fader(struct ri_dlist *dl, int cx, int y0, int y1, int w, int pos, int n, int skinned);
void ri_art_key_909(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int z, int lamp);
void ri_art_note_glyph(struct ri_dlist *dl, int x, int y, int v, int col);
void ri_art_lcd_bg(struct ri_dlist *dl, int x0, int y0, int x1, int y1);
void ri_art_led_digits(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int v, int ndig);
void ri_art_gr_row(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int v);
void ri_art_tr_key(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t idx, int lit, int z);
void ri_art_arrow_btn(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int up);
/* Running-light chase (G6a): step lamp at the section's playhead. */
int ri_art_chase(const struct RIPanelUI *panel, uint8_t section, uint32_t step);
int ri_art_step_colour_808(uint32_t step);
const char *ri_art_legend_909(const char *leg);
const char *ri_art_legend_fx(const char *leg);
const char *ri_art_909_opt(uint32_t opt);
const char *ri_art_808_opt(uint32_t opt);

/* Rack furniture (owner 2026-09-28): the Mix/FX bays and the device rail
 * are dark brushed metal between steel rack rails with screws; modules
 * sit in seams; devices switch with lit power buttons. Pure art, any size. */
#define RI_ART_BAY_BASE 0x2A2B2Fu  /* dark anodised plate */
#define RI_ART_RAIL_W 18           /* rack rail width, px */
#define RI_ART_SEAM_W 2            /* one module edge; two meet as a groove */
#define RI_ART_POWER_H 26          /* power-button chip height, px */
#define RI_ART_TAB_H 24            /* hardware tab key height, px */
/* Brushed dark plate, grain keyed to (x0, y0) so partial redraws match. */
void ri_art_bay(struct ri_dlist *dl, int x0, int y0, int x1, int y1);
/* Steel rack rail: vertical grain, rolled edges, 1U slotted holes, a
 * screw in the first and last hole. */
void ri_art_rack_rail(struct ri_dlist *dl, int x0, int y0, int x1, int y1);
/* Module edge: right = 0 left edge, 1 right edge (dark outside). */
void ri_art_seam(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int right);
/* Power button chip: round moulded cap whose power glyph is the LED (lit
 * green on, dim off), label centred right of it; pressed sinks the cap. */
void ri_art_power(struct ri_dlist *dl, int x0, int y0, int x1, int y1, const char *label,
    int on, int pressed);
/* Hardware tab key (S1, 2026-09-28): a moulded dark key with a 3 px LED
 * strip above the label. Active = lit strip + 1 px latched + darker face;
 * pressed = 1 px sink + darker face. Label is a palette C_* (pens). */
void ri_art_tab(struct ri_dlist *dl, int x0, int y0, int x1, int y1, const char *label,
    int active, int pressed);

/* Backgrounds (g = geometry section, ox/oy origin, z = zoom index). */
void ri_art_bg_303(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z);
void ri_art_bg_808(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z);
void ri_art_bg_909(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z);
/* Levi controls (fidelity plan P1): every geometry item of the Levi
 * section is drawn here (caps, knobs, encoders with LED rings, display
 * page, 7-segment algorithm readout, ribbon steps, keybed keys). */
void ri_art_levi_item(struct ri_dlist *dl, const struct RIGeoItem *it, const struct RICtlDef *d,
    const struct RISectUI *ui, int cx, int cy, int hw, int hh, int z);
void ri_art_bg_levi(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z);
void ri_art_bg_mix(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z, int master);
void ri_art_bg_fx(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z, uint8_t sec);
void ri_art_bg_pat(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z, uint8_t sec);
void ri_art_bg_tr(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z,
    const struct ri_text_metrics *tm);
void ri_art_focus_bar(struct ri_dlist *dl, uint8_t section,
    const struct RIPanelUI *panel, int ox, int oy, int z);
/* Edge-anchored legend (text_at equivalent): align<0 right edge at x,
 * align>0 left edge at x. Uses tm when available, else centred at x. */
void ri_art_text_at(struct ri_dlist *dl, int x, int cy, const char *t, int col, int align,
    const struct ri_text_metrics *tm);

/* Full section (plan §3.3 signature + panel): records background +
 * skin/images + every control for (section, zoom, skin). panel may be NULL
 * (chase lamps + focus bar off). tm supplies text metrics for the two
 * content decisions that depend on font width (909 option shortening,
 * text_at anchoring); AROS passes its metrics, keeping those decisions
 * bit-identical with the old direct path. */
void ri_draw_section(struct ri_dlist *out, const struct RISectUI *ui, uint8_t section, int zoom,
    int ox, int oy, const struct ri_text_metrics *tm, const struct RISkin *skin,
    const struct RIPanelUI *panel);

#endif
