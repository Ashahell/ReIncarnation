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
#define RI_ROUTE_MASTER 5 /* comp only: stereo master insert */
#define RI_ROUTE_NSECTIONS 5u /* 0=303A 1=303B 2=808 3=909 4=Levi */
/* Valid owners are RI_ROUTE_NONE and 0..RI_ROUTE_MASTER inclusive: sections are
 * 0..RI_ROUTE_NSECTIONS-1, and master is the one owner above them. Do not
 * hardcode either bound — Levi claiming section 4 is what moved master 4->5. */

struct RIRoute {
    int8_t owner[RI_ROUTE_NUNITS]; /* -1 none, 0..4 section, 5 master */
};

void ri_route_init(struct RIRoute *r);
/* Assign unit to owner; returns the previous owner. Rejects (returns -2,
 * state unchanged): bad unit id; owner < RI_ROUTE_NONE or
 * owner > RI_ROUTE_MASTER; master for dist/pcf.
 * Assigning -1 unassigns. Null route fails closed (-2). */
int ri_route_assign(struct RIRoute *r, uint32_t unit, int owner);
/* Owner of unit, or -2 on bad unit id / null. */
int ri_route_owner(const struct RIRoute *r, uint32_t unit);
/* Bitmask of units (bit u = RI_ROUTE_* id) owned by section; 0 when none.
 * Sections >= RI_ROUTE_NSECTIONS read 0. Every real section DOES get a mask,
 * Levi (4) included — only out-of-range indices read 0. Master ownership is
 * not in any mask: query ri_route_owner(RI_ROUTE_COMP) == RI_ROUTE_MASTER. */
uint32_t ri_route_section_mask(const struct RIRoute *r, uint32_t section);
#endif
