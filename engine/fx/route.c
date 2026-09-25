/* route.c — radio-exclusivity routing matrix (spec §12.8). */
#include "engine/fx/route.h"

void ri_route_init(struct RIRoute *r) {
    uint32_t u;
    if (!r)
        return;
    for (u = 0; u < RI_ROUTE_NUNITS; u++)
        r->owner[u] = (int8_t)RI_ROUTE_NONE;
}

int ri_route_assign(struct RIRoute *r, uint32_t unit, int owner) {
    int prev;
    if (!r || unit >= RI_ROUTE_NUNITS)
        return -2;
    if (owner < RI_ROUTE_NONE || owner > RI_ROUTE_MASTER)
        return -2;
    if (owner == RI_ROUTE_MASTER && unit != RI_ROUTE_COMP)
        return -2;
    prev = (int)r->owner[unit];
    r->owner[unit] = (int8_t)owner;
    return prev;
}

int ri_route_owner(const struct RIRoute *r, uint32_t unit) {
    if (!r || unit >= RI_ROUTE_NUNITS)
        return -2;
    return (int)r->owner[unit];
}

uint32_t ri_route_section_mask(const struct RIRoute *r, uint32_t section) {
    uint32_t u, m = 0u;
    if (!r || section >= RI_ROUTE_NSECTIONS)
        return 0u;
    for (u = 0; u < RI_ROUTE_NUNITS; u++)
        if (r->owner[u] == (int8_t)section)
            m |= (1u << u);
    return m;
}
