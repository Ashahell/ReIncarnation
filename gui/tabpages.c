/* tabpages.c — tab page model bodies (owner 2026-09-27). */
#include "gui/tabpages.h"
#include "gui/ctlreg.h"

static const char *const RI_TAB_TITLES[RI_TAB_COUNT] = {
    "Synths", "Drums", "Levi", "Mix", "FX"
};

/* Classic device -> canvas sections (explicit: the single source the
 * app table derives from; never arithmetic on section IDs). Levi (dev 4,
 * owner 2026-09-28) has its own tab (owner photo verdict). */
static const uint8_t RI_TAB_VOICE[RI_VIS_MAX] = {
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909, RI_SEC_LEVI
};
static const uint8_t RI_TAB_PAT[RI_VIS_MAX] = {
    RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909, RI_SEC_PAT_LEVI
};

/* Tab membership is explicit (device lists, not ranges: Levi breaks
 * contiguity at dev 4). */
static const uint8_t RI_TAB_SYNTH_DEVS[] = { 0u, 1u };
static const uint8_t RI_TAB_DRUMS_DEVS[] = { 2u, 3u };
static const uint8_t RI_TAB_LEVI_DEVS[] = { 4u };

const char *ri_tab_title(uint32_t group) {
    if (group >= RI_TAB_COUNT)
        return 0;
    return RI_TAB_TITLES[group];
}

uint32_t ri_tab_devices(uint32_t group, const struct RIVisSet *vis,
    struct RITabDev *out, uint32_t cap) {
    const uint8_t *devs;
    uint32_t ndev, k, n = 0u;
    if (!vis || !out)
        return 0u;
    if (group == RI_TAB_SYNTH) {
        devs = RI_TAB_SYNTH_DEVS;
        ndev = (uint32_t)(sizeof RI_TAB_SYNTH_DEVS / sizeof RI_TAB_SYNTH_DEVS[0]);
    } else if (group == RI_TAB_DRUMS) {
        devs = RI_TAB_DRUMS_DEVS;
        ndev = (uint32_t)(sizeof RI_TAB_DRUMS_DEVS / sizeof RI_TAB_DRUMS_DEVS[0]);
    } else if (group == RI_TAB_LEVI) {
        devs = RI_TAB_LEVI_DEVS;
        ndev = (uint32_t)(sizeof RI_TAB_LEVI_DEVS / sizeof RI_TAB_LEVI_DEVS[0]);
    } else {
        return 0u;
    }
    for (k = 0u; k < ndev; k++) {
        uint32_t d = devs[k];
        if (d >= RI_VIS_MAX || !vis->vis[d] || n >= cap)
            continue;
        out[n].device = d;
        out[n].voice_sec = RI_TAB_VOICE[d];
        out[n].pat_sec = RI_TAB_PAT[d];
        n++;
    }
    return n;
}
