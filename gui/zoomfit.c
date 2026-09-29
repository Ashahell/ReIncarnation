/* gui/zoomfit.c — pure Fit-mode zoom choice (S5, 2026-09-29).
 * Content sizes come from gui/panelgeo canvas geometry plus the app's
 * rack layout (mirrors app/riapp.c: tab_device_page rows, rack_page_gap
 * modules/seams/rails/gap, tab_rail/tab_strip inners, root spacing).
 * Rack furniture reuses gui/draw/art.h pixel constants; the few remaining
 * font-dependent widths (rail/tab strips) are ledgered estimates — they
 * never bind (Mix page dominates at every zoom; asserted in t121).
 */
#include "gui/zoomfit.h"

#include "gui/panelgeo.h"
#include "gui/ctlreg.h"
#include "gui/tabpages.h"
#include "gui/visdev.h"
#include "gui/draw/art.h"

/* App-layout mirrors (app/riapp.c tag values). */
#define ZFIT_ROW_SPACING 2
#define ZFIT_PAGE_SPACING 2
#define ZFIT_ROOT_SPACING 4
#define ZFIT_ROOT_INNER 12       /* 6 + 6 */
#define ZFIT_RAIL_INNER 6        /* top 3 + bottom 3 */
#define ZFIT_TAB_INNER 6         /* top 3 + bottom 3 */
#define ZFIT_RAIL_SPACING 6
#define ZFIT_TAB_SPACING 4
#define ZFIT_MIX_GAP 6           /* double-seam gap before MASTER */
#define ZFIT_RAIL_W_EST 560      /* font texts: never binds, see above */
#define ZFIT_TAB_W_EST 560       /* font texts: never binds, see above */

static const struct RIGeoSection *sec_geo(uint32_t sec) {
    const struct RIGeoSection *g = ri_geo_section(sec);
    /* Same SYNTH2->SYNTH1 rule as the canvas (rsection geo()). */
    if (!g && sec == RI_SEC_SYNTH2)
        g = ri_geo_section(RI_SEC_SYNTH1);
    return g;
}

static int sec_w(uint32_t sec, int zoom) {
    const struct RIGeoSection *g = sec_geo(sec);
    return g ? ri_geo_px(g->w, zoom) : 0;
}

static int sec_h(uint32_t sec, int zoom) {
    const struct RIGeoSection *g = sec_geo(sec);
    return g ? ri_geo_px(g->h, zoom) : 0;
}

static int imax(int a, int b) {
    return a > b ? a : b;
}

/* One tab page (all devices shown = widest): rows for device tabs,
 * module columns for Mix/FX. */
static void tab_page_size(uint32_t tab, int zoom, int *w, int *h) {
    if (tab == RI_TAB_SYNTH || tab == RI_TAB_DRUMS || tab == RI_TAB_LEVI) {
        struct RIVisSet vis;
        struct RITabDev rows[5];
        uint32_t n, r;
        int pw = 0, ph = 0;
        ri_vis_init(&vis);
        n = ri_tab_devices(tab, &vis, rows, 5u);
        for (r = 0u; r < n; r++) {
            int rw = 0, rh = 0;
            if (rows[r].pat_sec != RI_TAB_PAT_NONE) {
                rw += sec_w(rows[r].pat_sec, zoom) + ZFIT_ROW_SPACING;
                rh = imax(rh, sec_h(rows[r].pat_sec, zoom));
            }
            rw += sec_w(rows[r].voice_sec, zoom);
            rh = imax(rh, sec_h(rows[r].voice_sec, zoom));
            pw = imax(pw, rw);
            ph += (r > 0u ? ZFIT_PAGE_SPACING : 0) + rh;
        }
        *w = pw;
        *h = ph;
        return;
    }
    if (tab == RI_TAB_MIX) {
        /* Mirrors riapp mx[]: five strips + MASTER past the gap. */
        static const uint32_t secs[6] = { RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2,
            RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MIX_LEVI, RI_SEC_MASTER };
        uint32_t i;
        int mw = 0, mh = 0;
        for (i = 0u; i < 6u; i++) {
            /* rack_slot: one seam each side; spacing 0 between slots. */
            mw += sec_w(secs[i], zoom) + 2 * RI_ART_SEAM_W;
            mh = imax(mh, sec_h(secs[i], zoom));
            if (i == 4u)
                mw += ZFIT_MIX_GAP;
        }
        *w = mw + 2 * RI_ART_RAIL_W;
        *h = mh;
        return;
    }
    if (tab == RI_TAB_FX) {
        /* Mirrors riapp fx_page: PCF, Delay, Dist, Comp, no gap. */
        static const uint32_t secs[4] = { RI_SEC_PCF, RI_SEC_DELAY,
            RI_SEC_DIST, RI_SEC_COMP };
        uint32_t i;
        int mw = 0, mh = 0;
        for (i = 0u; i < 4u; i++) {
            mw += sec_w(secs[i], zoom) + 2 * RI_ART_SEAM_W;
            mh = imax(mh, sec_h(secs[i], zoom));
        }
        *w = mw + 2 * RI_ART_RAIL_W;
        *h = mh;
        return;
    }
    *w = 0;
    *h = 0;
}

int ri_zoomfit_content(int zoom, int *w, int *h) {
    int trw, trh, pw = 0, ph = 0, t;
    uint32_t tab;
    if ((zoom != 0 && zoom != 1 && zoom != 2) || !w || !h)
        return 2;
    /* Transport stays compact (S5 preview decision). */
    trw = sec_w(RI_SEC_TRANSPORT, RI_GEO_ZOOM_COMPACT) + 2 * RI_ART_SEAM_W;
    trh = sec_h(RI_SEC_TRANSPORT, RI_GEO_ZOOM_COMPACT);
    for (tab = 0u; tab < RI_TAB_COUNT; tab++) {
        int tw = 0, th = 0;
        tab_page_size(tab, zoom, &tw, &th);
        pw = imax(pw, tw);
        ph = imax(ph, th);
    }
    *w = trw;
    if (ZFIT_RAIL_W_EST > *w)
        *w = ZFIT_RAIL_W_EST;
    if (ZFIT_TAB_W_EST > *w)
        *w = ZFIT_TAB_W_EST;
    if (pw > *w)
        *w = pw;
    *w += ZFIT_ROOT_INNER;
    t = trh + ZFIT_ROOT_SPACING + (RI_ART_POWER_H + ZFIT_RAIL_INNER) +
        ZFIT_ROOT_SPACING + (RI_ART_TAB_H + ZFIT_TAB_INNER) +
        ZFIT_ROOT_SPACING + ph + ZFIT_ROOT_INNER;
    *h = t;
    return 0;
}

int ri_zoom_fit(int scr_w, int scr_h, int chrome_w, int chrome_h) {
    int z, w, h;
    if (scr_w <= 0 || scr_h <= 0 || chrome_w < 0 || chrome_h < 0)
        return 0;
    for (z = 2; z >= 0; z--) {
        if (ri_zoomfit_content(z, &w, &h) != 0)
            continue;
        if (w + chrome_w <= scr_w && h + chrome_h <= scr_h)
            return z;
    }
    return 0;
}

int ri_zoom_parse(const char *s, unsigned n) {
    /* Tolerate trailing whitespace/newline (shell-written files). */
    while (n > 0u && (s[n - 1u] == '\n' || s[n - 1u] == '\r' ||
        s[n - 1u] == ' ' || s[n - 1u] == '\t'))
        n--;
    if (!s || n == 0u)
        return RI_ZOOMFIT_FIT;
    if (n == 3u && s[0] == 'f' && s[1] == 'i' && s[2] == 't')
        return RI_ZOOMFIT_FIT;
    if (n == 1u && s[0] >= '0' && s[0] <= '2')
        return (int)(s[0] - '0');
    return RI_ZOOMFIT_FIT;
}

int ri_zoom_format(int zoom, char *out, unsigned cap) {
    if (!out || cap == 0u)
        return 0;
    if (zoom == RI_ZOOMFIT_FIT) {
        if (cap < 3u)
            return 0;
        out[0] = 'f';
        out[1] = 'i';
        out[2] = 't';
        return 3;
    }
    if (zoom >= 0 && zoom <= 2) {
        out[0] = (char)('0' + zoom);
        return 1;
    }
    return 0;
}

/* Guard (owner breakage 2026-09-29: 1.5x on 1366x768 drops rail/tabs):
 * explicit want clamped to what fits; FIT re-fits. Unknown screen
 * (non-positive) keeps a valid want, 0 otherwise. */
int ri_zoom_clamp(int scr_w, int scr_h, int chrome_w, int chrome_h, int want) {
    int w, h, fit;
    if (want != RI_ZOOMFIT_FIT && (want < 0 || want > 2))
        return 0;
    if (scr_w <= 0 || scr_h <= 0 || chrome_w < 0 || chrome_h < 0)
        return (want == RI_ZOOMFIT_FIT) ? 0 : want;
    if (want != RI_ZOOMFIT_FIT && ri_zoomfit_content(want, &w, &h) == 0 &&
        w + chrome_w <= scr_w && h + chrome_h <= scr_h)
        return want;
    fit = ri_zoom_fit(scr_w, scr_h, chrome_w, chrome_h);
    return fit;
}
