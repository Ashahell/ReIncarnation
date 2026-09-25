/* gui/panelgeo.h — section panel geometry (§12.10 G2).
 * Pure C. Positions are measured from the ReBirth 2.0.1 Owner's Manual
 * panel figures (E1 layout, measured — never pixel-copied art) in quarter
 * figure-pixel units ("Q": the 303 figure on p. 153 is 366 x 115 px, i.e.
 * 1464 x 460 Q). Ledger: docs/evidence/gui/panel-geometry.md.
 * Every item is tied to a registry control (gui/ctlreg.h) by reg_id, so
 * layout and inventory cannot drift apart; decorations (LEDs, legends,
 * dividers) name the control they belong to.
 */
#ifndef RI_PANELGEO_H
#define RI_PANELGEO_H
#include <stdint.h>

/* Item shapes. */
#define RI_GEO_KNOB 0u    /* cx,cy = centre; w = body diameter; h = tick-ring diameter */
#define RI_GEO_RECT 1u    /* cx,cy = centre; w,h = hit rectangle */
#define RI_GEO_LED 2u     /* decoration: status LED of the owning control */
#define RI_GEO_LEGEND 3u  /* decoration: text legend centre of the owning control */
#define RI_GEO_DIVIDER 4u /* decoration: vertical rule; cx = x, cy = top, h = length */

struct RIGeoItem {
    uint16_t reg_id; /* owning registry control */
    uint8_t shape;   /* RI_GEO_* */
    uint8_t pad;
    int16_t cx, cy, w, h; /* Q units */
};

struct RIGeoSection {
    uint8_t section;      /* RI_SEC_* */
    uint8_t pad;
    int16_t w, h;         /* Q units */
    const struct RIGeoItem *items;
    uint32_t nitems;
};

/* Base render scale: window pixels per figure pixel at zoom 1x
 * (2.0 -> the 303 section is 732 x 230 px; fits 800x600 and 1366x768). */
#define RI_GEO_BASE_SCALE_NUM 2
#define RI_GEO_BASE_SCALE_DEN 1

const struct RIGeoSection *ri_geo_section(uint32_t section); /* NULL if not laid out yet */
/* Q units -> window pixels at a zoom level (0 = 1x, 1 = 1.5x, 2 = 2x),
 * rounded half away from zero; unknown zoom -> 0. */
int ri_geo_px(int q, int zoom);
/* Hit test: the control (reg_id) whose value item contains (x,y) in window
 * pixels at zoom; decorations never hit. Returns 0xFFFF when none. */
uint16_t ri_geo_hit(const struct RIGeoSection *s, int x, int y, int zoom);
#endif
