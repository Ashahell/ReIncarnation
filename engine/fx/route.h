/* route.h — insert routing matrix (§12.8): radio exclusivity.
 * Each insert unit (Dist, PCF, Comp) is owned by at most one section at
 * a time (Comp: one section or the master). Assigning a unit elsewhere
 * steals it (ReBirth radio behaviour, manual pp. 59-70). Pure functions:
 * no allocation, no IO, kernels not needed.
 */
#ifndef RI_ROUTE_H
#define RI_ROUTE_H
#include <stdint.h>

#define RI_ROUTE_DIST 0u
#define RI_ROUTE_PCF 1u
#define RI_ROUTE_COMP 2u
#define RI_ROUTE_NUNITS 3u

#define RI_ROUTE_NONE (-1)
#define RI_ROUTE_MASTER 4 /* comp only: stereo master insert */
#define RI_ROUTE_NSECTIONS 4u /* 0=303A 1=303B 2=808 3=909 */

struct RIRoute {
    int8_t owner[RI_ROUTE_NUNITS]; /* -1 none, 0..3 section, 4 master */
};

void ri_route_init(struct RIRoute *r);
/* Assign unit to owner; returns the previous owner. Rejects (returns -2,
 * state unchanged): bad unit id; owner < -1 or > 4; master for dist/pcf.
 * Assigning -1 unassigns. Null route fails closed (-2). */
int ri_route_assign(struct RIRoute *r, uint32_t unit, int owner);
/* Owner of unit, or -2 on bad unit id / null. */
int ri_route_owner(const struct RIRoute *r, uint32_t unit);
/* Bitmask of units (bit u = RI_ROUTE_* id) owned by section; 0 when none.
 * Sections > 3 (incl. master) read 0 — master ownership is queried via
 * ri_route_owner(RI_ROUTE_COMP) == RI_ROUTE_MASTER. */
uint32_t ri_route_section_mask(const struct RIRoute *r, uint32_t section);
#endif
