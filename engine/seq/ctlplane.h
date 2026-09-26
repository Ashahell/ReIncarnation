/* ctlplane.h — GUI -> render control plane (G9.1, §12.11).
 * Fixed-capacity SPSC ring of lane-key messages, drained at buffer start
 * into RI_EV_AUTOMATION events. Pure, no heap, no IO, no mutable static
 * state. Caller owns the struct (static/stack). Single writer (GUI) and
 * single reader (render) by contract; host tests drive both sides inline.
 */
#ifndef RI_CTLPLANE_H
#define RI_CTLPLANE_H
#include <stdint.h>
#include "engine/seq/sched.h"

#define RI_CTL_CAP 256u /* power of two; mask indexing */

struct RIControlMsg { uint16_t key; uint8_t val; uint8_t flags; };

struct RIControlPlane {
    struct RIControlMsg buf[RI_CTL_CAP];
    uint32_t head;    /* writer count (monotonic) */
    uint32_t tail;    /* reader count (monotonic) */
    uint32_t dropped; /* overflow coalesce/drop count (writer side) */
    uint32_t refused; /* refused-key count (writer side) */
};

void ri_ctl_init(struct RIControlPlane *p);
/* Enqueue one knob/MIDI move. 0 ok, 2 refused (key not on the
 * ri_auto_allowed list; counted, never stored). Overflow keeps the newest
 * value per key (coalesce in place) or drops the oldest for a new key;
 * both count one `dropped`. val is 0..127 (masked to 7 bits on drain). */
int ri_ctl_send(struct RIControlPlane *p, uint16_t key, uint8_t val);
uint32_t ri_ctl_pending(const struct RIControlPlane *p);
/* Drain up to cap entries in FIFO order into AUTOMATION events at `sample`
 * (the buffer's first sample). Leftover stays queued on cap pressure
 * (resume next buffer, never lost). Returns drained count. `seq` is the
 * caller's insertion counter, bumped per event. NULL/0-cap drains nothing. */
uint32_t ri_ctl_drain(struct RIControlPlane *p, struct RIEvent *out,
    uint32_t cap, uint64_t sample, uint32_t *seq);
#endif
