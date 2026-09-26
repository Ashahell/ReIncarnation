/* autolane.h — automation lane model (spec 2026-09-26 §1-2).
 * Pure, no alloc, no IO, no mutable static state. Caller-owned storage.
 * One direction only: autolane -> transport (ppq + RECORD state). */
#ifndef RI_AUTOLANE_H
#define RI_AUTOLANE_H
#include <stdint.h>
#include "engine/seq/transport.h"

#define RI_AUTO_MAX_EVENTS 32768u /* §5.3 decided; soak with dense songs */
#define RI_AUTO_MAX_TOUCH  64u    /* punched + touched sets per pass */

struct RIAutoEv { uint32_t tick; uint16_t ctl; uint8_t val; uint8_t pad; };
/* Sorted by (tick, ctl). ev sized once by the caller (song load), never
 * in the render path; flags carries RI_AUTO_FLAG_FULL (sticky). */
struct RIAutoLane { uint32_t n, cap, flags; struct RIAutoEv *ev; };
#define RI_AUTO_FLAG_FULL 1u
/* A recording pass: punched (play live + erase) and touched (copy-touched
 * source) control sets, both bounded; overflow refuses the new touch. */
struct RIAutoPass {
    uint16_t npunched, ntouched;
    uint16_t punched[RI_AUTO_MAX_TOUCH], touched[RI_AUTO_MAX_TOUCH];
};

int ri_auto_allowed(uint16_t ctl); /* 1 on the allow-list, else 0 */
int ri_auto_value(const struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                  uint8_t *out); /* 1 found (latest <= tick), 0 none */
int ri_auto_touch(struct RIAutoLane *l, struct RIAutoPass *p,
                  uint8_t tr_state, uint32_t cursor, uint32_t ppq,
                  uint16_t ctl, uint8_t val); /* punch-in + write; 0/2 */
int ri_auto_sweep(struct RIAutoLane *l, const struct RIAutoPass *p,
                  uint32_t from, uint32_t to); /* erase span + pass writes */
#endif
