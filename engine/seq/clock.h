/* clock.h — rational multi-segment tempo clock (Task 3, gate G3). */
#ifndef RI_CLOCK_H
#define RI_CLOCK_H
#include <stdint.h>
/* Exact rational per segment, ONE rounding.
 * Rounding lock: Round at schedule, Floor at lookup — changing this breaks D0.
 * ri_map_tick rounds (schedule time), ri_map_tick_floor truncates (lookup).
 * PPQ=96 (P-20). stdint.h only.
 */
struct RISegment { uint64_t start_tick; uint64_t ns_per_quarter; };
struct RITempoMap { const struct RISegment *segs; uint32_t n; uint32_t ppq; uint32_t sr; };
/* PPQ=96 (P-20). Exact rational per segment, ONE rounding. */
uint64_t ri_map_tick(const struct RITempoMap *m, uint64_t tick);        /* Round */
uint64_t ri_map_tick_floor(const struct RITempoMap *m, uint64_t tick);  /* Floor */
#endif
