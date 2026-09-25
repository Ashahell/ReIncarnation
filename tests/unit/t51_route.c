/* t51_route — §12.8 routing matrix: radio exclusivity (pure functions).
 *   - init: all units unowned (-1).
 *   - assign dist->0, pcf->1, comp->2: owners read back; section masks
 *     carry the unit bits.
 *   - radio: reassign dist 0->1 steals (0 loses it, returns prev owner 0).
 *   - comp may sit on master (4); dist/pcf reject master (return -2,
 *     unchanged); owners < -1 / > 4 rejected (return -2, unchanged).
 *   - bad unit id rejected (owner query returns -2).
 * RED-first: route.h does not exist yet.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "engine/fx/route.h"

int main(void) {
    struct RIRoute r;
    ri_route_init(&r);
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_DIST) == -1, "dist owned");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_PCF) == -1, "pcf owned");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_COMP) == -1, "comp owned");

    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_DIST, 0) == -1, "dist prev");
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_PCF, 1) == -1, "pcf prev");
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_COMP, 2) == -1, "comp prev");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_DIST) == 0, "dist not on 0");
    RI_ASSERT(ri_route_section_mask(&r, 0) == 1u, "mask0 %u",
        ri_route_section_mask(&r, 0));
    RI_ASSERT(ri_route_section_mask(&r, 1) == 2u, "mask1");
    RI_ASSERT(ri_route_section_mask(&r, 2) == 4u, "mask2");

    /* Radio steal: dist 0 -> 1. */
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_DIST, 1) == 0, "steal prev != 0");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_DIST) == 1, "dist not on 1");
    RI_ASSERT(ri_route_section_mask(&r, 0) == 0u, "mask0 not cleared");
    RI_ASSERT(ri_route_section_mask(&r, 1) == 3u, "mask1 not union");

    /* Comp on master; dist/pcf cannot go there. */
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_COMP, RI_ROUTE_MASTER) == 2,
        "comp master prev");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_COMP) == RI_ROUTE_MASTER,
        "comp not master");
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_DIST, RI_ROUTE_MASTER) == -2,
        "dist master accepted");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_DIST) == 1, "dist moved!");
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_PCF, RI_ROUTE_MASTER) == -2,
        "pcf master accepted");

    /* Out-of-range owners rejected, state unchanged. */
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_COMP, 5) == -2, "owner 5 taken");
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_COMP, -2) == -2, "owner -2 taken");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_COMP) == RI_ROUTE_MASTER,
        "comp moved!");
    RI_ASSERT(ri_route_assign(&r, 9u, 0) == -2, "bad unit taken");
    RI_ASSERT(ri_route_owner(&r, 9u) == -2, "bad unit query");

    /* Unassign back to none. */
    RI_ASSERT(ri_route_assign(&r, RI_ROUTE_DIST, -1) == 1, "unassign prev");
    RI_ASSERT(ri_route_owner(&r, RI_ROUTE_DIST) == -1, "dist still owned");

    RI_RESULT("route");
}
