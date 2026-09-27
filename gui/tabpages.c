/* tabpages.c — tab page model bodies (owner 2026-09-27). */
#include "gui/tabpages.h"
#include "gui/ctlreg.h"

static const char *const RI_TAB_TITLES[RI_TAB_COUNT] = {
    "Synths", "Drums", "Mix", "FX"
};

/* Classic device -> canvas sections (explicit: the single source the
 * app table derives from; never arithmetic on section IDs). */
static const uint8_t RI_TAB_VOICE[RI_VIS_MAX] = {
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909
};
static const uint8_t RI_TAB_PAT[RI_VIS_MAX] = {
    RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909
};

const char *ri_tab_title(uint32_t group) {
    if (group >= RI_TAB_COUNT)
        return 0;
    return RI_TAB_TITLES[group];
}

uint32_t ri_tab_devices(uint32_t group, const struct RIVisSet *vis,
    struct RITabDev *out, uint32_t cap) {
    uint32_t lo, hi, d, n = 0u;
    if (!vis || !out)
        return 0u;
    if (group == RI_TAB_SYNTH) {
        lo = 0u;
        hi = 2u;
    } else if (group == RI_TAB_DRUMS) {
        lo = 2u;
        hi = RI_VIS_MAX;
    } else {
        return 0u;
    }
    for (d = lo; d < hi; d++) {
        if (!vis->vis[d] || n >= cap)
            continue;
        out[n].device = d;
        out[n].voice_sec = RI_TAB_VOICE[d];
        out[n].pat_sec = RI_TAB_PAT[d];
        n++;
    }
    return n;
}
