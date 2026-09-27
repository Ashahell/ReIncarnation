/* tabpages.h — tabbed-panel page model (owner 2026-09-27).
 * Pure C, host-tested. Four fixed tabs (Synths/Drums/Mix/FX, stock
 * Register.mui); device visibility selects which device rows appear
 * inside Synths/Drums. Single-sources the device -> (voice section,
 * pattern section, bank instance) mapping the app table hardcodes.
 * Mix/FX are frame groups (mixer board, FX units) with no device rows.
 */
#ifndef RI_TABPAGES_H
#define RI_TABPAGES_H
#include <stdint.h>
#include "gui/visdev.h"

#define RI_TAB_SYNTH 0u
#define RI_TAB_DRUMS 1u
#define RI_TAB_MIX 2u
#define RI_TAB_FX 3u
#define RI_TAB_COUNT 4u

struct RITabDev {
    uint32_t device;    /* 0..3 classic */
    uint32_t voice_sec; /* RI_SEC_* voice canvas */
    uint32_t pat_sec;   /* RI_SEC_PAT_* pattern canvas */
};

const char *ri_tab_title(uint32_t group); /* NULL on bad group */
/* Visible device rows for a group, device order, into out[] (up to cap).
 * Returns rows written; 0 on bad group / NULL set / frame group. */
uint32_t ri_tab_devices(uint32_t group, const struct RIVisSet *vis,
    struct RITabDev *out, uint32_t cap);

#endif
