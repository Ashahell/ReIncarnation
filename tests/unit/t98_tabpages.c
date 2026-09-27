/* t98_tabpages — tabbed-panel page model (owner 2026-09-27).
 * Fixed group titles; Synths/Drums rows follow the visible set in
 * device order with voice+pattern sections; Mix/FX are frameless;
 * fail-closed on bad group / NULL / no capacity.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/tabpages.h"
#include "gui/visdev.h"
#include "gui/ctlreg.h"

int main(void) {
    struct RIVisSet vis;
    struct RITabDev rows[4];
    ri_vis_init(&vis);
    RI_ASSERT(ri_tab_title(RI_TAB_SYNTH) && !strcmp(ri_tab_title(RI_TAB_SYNTH), "Synths"), "t synth");
    RI_ASSERT(ri_tab_title(RI_TAB_DRUMS) && !strcmp(ri_tab_title(RI_TAB_DRUMS), "Drums"), "t drums");
    RI_ASSERT(ri_tab_title(RI_TAB_MIX) && !strcmp(ri_tab_title(RI_TAB_MIX), "Mix"), "t mix");
    RI_ASSERT(ri_tab_title(RI_TAB_FX) && !strcmp(ri_tab_title(RI_TAB_FX), "FX"), "t fx");
    RI_ASSERT(ri_tab_title(4u) == 0, "t bad");
    RI_ASSERT(ri_tab_devices(RI_TAB_SYNTH, &vis, rows, 4u) == 2u, "synth n");
    RI_ASSERT(rows[0].device == 0u && rows[0].voice_sec == RI_SEC_SYNTH1 &&
        rows[0].pat_sec == RI_SEC_PAT_SYNTH1, "synth row0");
    RI_ASSERT(rows[1].device == 1u && rows[1].voice_sec == RI_SEC_SYNTH2 &&
        rows[1].pat_sec == RI_SEC_PAT_SYNTH2, "synth row1");
    RI_ASSERT(ri_tab_devices(RI_TAB_DRUMS, &vis, rows, 4u) == 2u, "drums n");
    RI_ASSERT(rows[0].device == 2u && rows[0].voice_sec == RI_SEC_808 &&
        rows[0].pat_sec == RI_SEC_PAT_808, "drums row0");
    RI_ASSERT(rows[1].device == 3u && rows[1].voice_sec == RI_SEC_909 &&
        rows[1].pat_sec == RI_SEC_PAT_909, "drums row1");
    ri_vis_set(&vis, 1u, 0);
    RI_ASSERT(ri_tab_devices(RI_TAB_SYNTH, &vis, rows, 4u) == 1u, "hide");
    RI_ASSERT(rows[0].device == 0u, "hide order");
    ri_vis_set(&vis, 1u, 1);
    RI_ASSERT(ri_tab_devices(RI_TAB_SYNTH, &vis, rows, 1u) == 1u, "cap");
    RI_ASSERT(ri_tab_devices(RI_TAB_MIX, &vis, rows, 4u) == 0u, "mix frame");
    RI_ASSERT(ri_tab_devices(RI_TAB_FX, &vis, rows, 4u) == 0u, "fx frame");
    RI_ASSERT(ri_tab_devices(9u, &vis, rows, 4u) == 0u, "bad group");
    RI_ASSERT(ri_tab_devices(RI_TAB_SYNTH, 0, rows, 4u) == 0u, "null vis");
    RI_ASSERT(ri_tab_devices(RI_TAB_SYNTH, &vis, 0, 4u) == 0u, "null out");
    RI_ASSERT(ri_tab_devices(RI_TAB_SYNTH, &vis, rows, 0u) == 0u, "no cap");
    RI_RESULT("tabpages");
}
