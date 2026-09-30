/* t110_levi_layout — Levi hardware panel layout (fidelity plan P1/P2,
 * owner 2026-09-30: "look as closely like the actual hardware as
 * possible ... keep using the current font, in an appropriate
 * contrasting color"; supersedes the 2026-09-28 own-palette rule).
 * Behavioural: the measured hardware block order (left column, CV/Gate
 * and Arp & Seq, Main Systems, Master Control with the encoders around
 * the display, Osc Env Level & Bias, Digital then Analog filter; the
 * Algorithm block left of Module Select), graphite panel first, section
 * titles drawn in the teal legend colour, no maker marks in any string.
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

/* cx of the first item (any shape) for a registry index, or -1. */
static int cxof(uint32_t idx, int shape) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_LEVI);
    uint32_t i;
    for (i = 0u; i < s->nitems; i++)
        if ((s->items[i].reg_id & 0xFFu) == idx && (shape < 0 || s->items[i].shape == shape))
            return s->items[i].cx;
    return -1;
}

static int cyof(uint32_t idx) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_LEVI);
    uint32_t i;
    for (i = 0u; i < s->nitems; i++)
        if ((s->items[i].reg_id & 0xFFu) == idx)
            return s->items[i].cy;
    return -1;
}

/* cx of the OPTION item of idx with option value opt, or -1. */
static int optcx(uint32_t idx, int opt) {
    const struct RIGeoSection *s = ri_geo_section(RI_SEC_LEVI);
    uint32_t i;
    for (i = 0u; i < s->nitems; i++)
        if ((s->items[i].reg_id & 0xFFu) == idx && s->items[i].shape == RI_GEO_OPTION &&
            s->items[i].opt == opt)
            return s->items[i].cx;
    return -1;
}

int main(void) {
    static const char *const titles[] = { "CV / GATE", "ARPEGGIATOR & SEQUENCER CONTROL", "MAIN SYSTEMS",
        "MASTER CONTROL", "OSC ENV LEVEL & BIAS", "DIGITAL FILTER", "ANALOG FILTER", "ALGORITHM",
        "MODULE SELECT" };
    enum { NT = sizeof(titles) / sizeof(titles[0]) };
    struct RISectUI ui;
    struct ri_dlist dl;
    uint32_t k, t;
    int found[NT], bg = 0;
    int arp = cxof(RI_SLEVI_ARPON, RI_GEO_RECT), enc1 = cxof(RI_SLEVI_ENC0, RI_GEO_KNOB);
    int lcd = cxof(RI_SLEVI_PAGE, RI_GEO_RECT), bias = cxof(RI_SLEVI_BIAS_ENVL, RI_GEO_KNOB);
    int dcut = cxof(RI_SLEVI_CUTOFF, RI_GEO_KNOB), acut = cxof(RI_SLEVI_CUTOFF2, RI_GEO_KNOB);
    int algo = cxof(RI_SLEVI_ALGO, RI_GEO_KNOB), dfilt = optcx(RI_SLEVI_MODULE, (int)RI_SLEVI_M_DFILT);
    memset(found, 0, sizeof found);
    /* Hardware block order, left to right. */
    RI_ASSERT(arp >= 0 && enc1 >= 0 && lcd >= 0 && bias >= 0 && dcut >= 0 && acut >= 0, "blocks placed");
    RI_ASSERT(arp < enc1, "arp & seq before master control %d<%d", arp, enc1);
    RI_ASSERT(enc1 < lcd && lcd < cxof(RI_SLEVI_ENC0 + 3u, RI_GEO_KNOB), "encoders around the display");
    RI_ASSERT(cxof(RI_SLEVI_ENC0 + 3u, RI_GEO_KNOB) < bias, "master control before osc env bias");
    RI_ASSERT(bias < dcut && dcut < acut, "bias, digital, analog %d<%d<%d", bias, dcut, acut);
    RI_ASSERT(algo >= 0 && dfilt >= 0 && algo < dfilt, "algorithm before module select %d<%d", algo, dfilt);
    RI_ASSERT(cyof(RI_SLEVI_ENC0) < cyof(RI_SLEVI_PAGE) && cyof(RI_SLEVI_PAGE) < cyof(RI_SLEVI_ENC0 + 4u),
        "encoders 1-4 above, 5-8 below the display");
    RI_ASSERT(cyof(RI_SLEVI_CUTOFF) < cyof(RI_SLEVI_STEP0) && cyof(RI_SLEVI_STEP0) < cyof(RI_SLEVI_KEY0),
        "top panel, ribbon, keybed from top to bottom");
    /* Drawn: graphite panel first, teal titles, no maker marks. */
    RI_ASSERT(ri_sui_init(&ui, RI_SEC_LEVI) == 0, "init");
    ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
    ri_draw_section(&dl, &ui, RI_SEC_LEVI, 1, 0, 0, 0, 0, 0);
    RI_ASSERT(dl.n > 0u, "non-empty");
    for (k = 0u; k < dl.n; k++) {
        if (dl.cmd[k].op == RI_D_RECT && !bg) {
            RI_ASSERT(dl.cmd[k].rgb == ri_art_rgb(C_LEVI_PANEL), "graphite panel first, got %06x", dl.cmd[k].rgb);
            bg = 1;
        }
        if (dl.cmd[k].op != RI_D_TEXT || !dl.cmd[k].text)
            continue;
        RI_ASSERT(!strstr(dl.cmd[k].text, "ASM") && !strstr(dl.cmd[k].text, "LEVIASYNTH"),
            "no maker mark: %s", dl.cmd[k].text);
        for (t = 0u; t < NT; t++)
            if (!strcmp(dl.cmd[k].text, titles[t])) {
                RI_ASSERT(dl.cmd[k].rgb == ri_art_rgb(C_LEVI_TEAL), "title %s teal, got %06x", titles[t],
                    dl.cmd[k].rgb);
                found[t] = 1;
            }
    }
    for (t = 0u; t < NT; t++)
        RI_ASSERT(found[t], "title missing: %s", titles[t]);
    RI_RESULT("levi_layout");
}
