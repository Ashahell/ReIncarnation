/* t97_visdev — visible-device set for tabbed panels (owner 2026-09-27).
 * Init-all-shown, hide/show round-trip, bad-index fail-closed, title
 * strings come from the wiring table (ri_panel_get), count tracks.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/visdev.h"
#include "gui/panels.h"

int main(void) {
    struct RIVisSet s;
    const char *titles[4] = { "", "", "", "" };
    ri_vis_init(&s);
    RI_ASSERT(ri_vis_count(&s) == 4u, "all shown");
    RI_ASSERT(ri_vis_get(&s, 0u) == 1 && ri_vis_get(&s, 3u) == 1, "get");
    RI_ASSERT(ri_vis_set(&s, 1u, 0) == 0, "hide rc");
    RI_ASSERT(ri_vis_get(&s, 1u) == 0, "hidden");
    RI_ASSERT(ri_vis_count(&s) == 3u, "count 3");
    RI_ASSERT(ri_vis_set(&s, 1u, 1) == 0, "show rc");
    RI_ASSERT(ri_vis_count(&s) == 4u, "count 4");
    RI_ASSERT(ri_vis_set(&s, 4u, 0) == 2, "bad dev");
    RI_ASSERT(ri_vis_set(0, 0u, 0) == 2, "null set");
    RI_ASSERT(ri_vis_get(&s, 9u) == -1, "bad get");
    RI_ASSERT(ri_vis_get(0, 0u) == -1, "null get");
    RI_ASSERT(ri_vis_count(0) == 0u, "null count");
    RI_ASSERT(ri_vis_titles(&s, titles, 4u) == 4u, "titles n");
    RI_ASSERT(!strcmp(titles[0], "303A") && !strcmp(titles[1], "303B") &&
        !strcmp(titles[2], "808") && !strcmp(titles[3], "909"), "titles");
    ri_vis_set(&s, 2u, 0);
    RI_ASSERT(ri_vis_titles(&s, titles, 4u) == 3u, "titles skip");
    RI_ASSERT(!strcmp(titles[2], "909"), "titles order");
    RI_ASSERT(ri_vis_titles(&s, titles, 0u) == 0u, "titles nocap");
    RI_ASSERT(ri_vis_titles(0, titles, 4u) == 0u, "titles null");
    ri_vis_init(0);
    RI_RESULT("visdev");
}
