/* t110_levi_layout — Levi hardware-family layout (owner 2026-09-28,
 * photo verdict: "looks nothing like it").
 * Functional blocks in hardware order left to right
 * (MODULE, OSC, ALGORITHM, DIGITAL FILTER, ANALOG FILTER, ENVELOPE),
 * own near-black panel + amber titles (never ASM teal), own font path,
 * no ASM marks. Behavioural: geometry order + drawn titles/colours.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/art.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/panelgeo.h"

static struct ri_dcmd T_BACK[24576];
static char T_SPOOL[32768];

/* min cx of OPTION items for a registry index, or -1 */
static int optcx(uint32_t idx) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_LEVI);
    uint32_t i;
    int m = -1;
    for (i = 0u; i < s->nitems; i++)
        if ((s->items[i].reg_id & 0xFFu) == idx && s->items[i].shape == RI_GEO_OPTION)
            if (m < 0 || s->items[i].cx < m)
                m = s->items[i].cx;
    return m;
}

/* cy of the KNOB/RECT value item for a registry index, or -1 */
static int valcy(uint32_t idx) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_LEVI);
    uint32_t i;
    for (i = 0u; i < s->nitems; i++)
        if ((s->items[i].reg_id & 0xFFu) == idx &&
            (s->items[i].shape == RI_GEO_KNOB || s->items[i].shape == RI_GEO_RECT))
            return s->items[i].cy;
    return -1;
}

/* cx of the KNOB/RECT value item for a registry index, or -1 */
static int valcx(uint32_t idx) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_LEVI);
    uint32_t i;
    for (i = 0u; i < s->nitems; i++)
        if ((s->items[i].reg_id & 0xFFu) == idx &&
            (s->items[i].shape == RI_GEO_KNOB || s->items[i].shape == RI_GEO_RECT))
            return s->items[i].cx;
    return -1;
}

int main(void) {
    static const char *const titles[] = { "MODULE", "OSC", "ALGORITHM",
        "DIGITAL FILTER", "ANALOG FILTER", "ENVELOPE", "LEVI",
        "ARP", "SEQ", "MATRIX", "FX" };
    struct RISectUI ui;
    struct ri_dlist dl;
    uint32_t k, t;
    int found[11] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int bg = 0;
    /* Block order left to right across the canvas. */
    RI_ASSERT(optcx(RI_SLEVI_OPSEL) >= 0, "opsel placed");
    RI_ASSERT(valcx(3u) >= 0, "ratio placed");
    RI_ASSERT(optcx(RI_SLEVI_ALGO) >= 0, "algo placed");
    RI_ASSERT(optcx(RI_SLEVI_FTYPE) >= 0, "ftype placed");
    RI_ASSERT(valcx(RI_SLEVI_CUTOFF2) >= 0, "cutoff2 placed");
    RI_ASSERT(valcx(RI_SLEVI_ATTACK) >= 0, "attack placed");
    RI_ASSERT(valcx(RI_SLEVI_STEP0) >= 0, "steps placed");
    RI_ASSERT(optcx(RI_SLEVI_OPSEL) < valcx(3u), "module before osc %d<%d",
        optcx(RI_SLEVI_OPSEL), valcx(3u));
    RI_ASSERT(valcx(3u) < optcx(RI_SLEVI_ALGO), "osc before algorithm %d<%d",
        valcx(3u), optcx(RI_SLEVI_ALGO));
    RI_ASSERT(optcx(RI_SLEVI_ALGO) < optcx(RI_SLEVI_FTYPE), "algorithm before digital %d<%d",
        optcx(RI_SLEVI_ALGO), optcx(RI_SLEVI_FTYPE));
    RI_ASSERT(optcx(RI_SLEVI_FTYPE) < valcx(RI_SLEVI_CUTOFF2), "digital before analog %d<%d",
        optcx(RI_SLEVI_FTYPE), valcx(RI_SLEVI_CUTOFF2));
    RI_ASSERT(valcx(RI_SLEVI_CUTOFF2) < valcx(RI_SLEVI_ATTACK), "analog before envelope %d<%d",
        valcx(RI_SLEVI_CUTOFF2), valcx(RI_SLEVI_ATTACK));
    RI_ASSERT(valcy(0u) < valcy(RI_SLEVI_STEP0), "voice band above chord %d<%d",
        valcy(0u), valcy(RI_SLEVI_STEP0));
    /* Own palette, never ASM teal. */
    RI_ASSERT(ri_art_rgb(C_LEVI_PANEL) == 0x141518u, "panel near-black, got %06x",
        ri_art_rgb(C_LEVI_PANEL));
    RI_ASSERT(ri_art_rgb(C_LEVI_HEAD) == 0xD8A93Cu, "head amber, got %06x",
        ri_art_rgb(C_LEVI_HEAD));
    RI_ASSERT(ri_art_rgb(C_LEVI_RULE) == 0x3A3D45u, "rule steel, got %06x",
        ri_art_rgb(C_LEVI_RULE));
    /* Drawn titles in head colour; background first; no ASM marks. */
    RI_ASSERT(ri_sui_init(&ui, RI_SEC_LEVI) == 0, "init");
    ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
    ri_draw_section(&dl, &ui, RI_SEC_LEVI, 0, 0, 0, 0, 0, 0);
    RI_ASSERT(dl.n > 0u, "non-empty");
    for (k = 0u; k < dl.n; k++) {
        if (dl.cmd[k].op == RI_D_RECT && !bg) {
            RI_ASSERT(dl.cmd[k].rgb == 0x141518u, "bg near-black first, got %06x",
                dl.cmd[k].rgb);
            bg = 1;
        }
        if (dl.cmd[k].op != RI_D_TEXT || !dl.cmd[k].text)
            continue;
        RI_ASSERT(!strstr(dl.cmd[k].text, "ASM"), "no ASM mark: %s", dl.cmd[k].text);
        for (t = 0u; t < 11u; t++)
            if (!strcmp(dl.cmd[k].text, titles[t])) {
                RI_ASSERT(dl.cmd[k].rgb == 0xD8A93Cu, "title %s amber, got %06x",
                    titles[t], dl.cmd[k].rgb);
                found[t] = 1;
            }
    }
    for (t = 0u; t < 11u; t++)
        RI_ASSERT(found[t], "title missing: %s", titles[t]);
    RI_RESULT("levi_layout");
}
