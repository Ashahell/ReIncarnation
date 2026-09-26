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
/* Pass-write marker: RIAutoEv.pad bit set by touch on the event it
 * writes or replaces. Sweep erases only unmarked events (R2). */
#define RI_AUTO_EV_PASS 0x01u
/* A recording pass: punched (play live + erase) and touched (copy-touched
 * source) control sets, both bounded; overflow refuses the new touch. */
struct RIAutoPass {
    uint16_t npunched, ntouched;
    uint16_t punched[RI_AUTO_MAX_TOUCH], touched[RI_AUTO_MAX_TOUCH];
};

/* Lane keys beyond the 303/FX engine IDs (Task 5b/5c, E0 2026-09-26,
 * owner "go"). The engine's drum param IDs (0x040p/0x090p) are shared by
 * every voice, so a (tick, ctl) lane cannot tell BD Level from SD Level;
 * drums get their own key blocks with the voice in the low nibble, and the
 * channel strips get the spec §13 0x0Bxx block above the legacy panel IDs
 * (0x0B00..0x0B0F stay the Task-12 panel table's):
 *   0x0Bsp  strips: s = strip + 1 (1..4 = 303A 303B 808 909, 5 = master),
 *           p = RI_AUTO_MIX_* (level, pan, delay send, dist/pcf/comp insert)
 *   0x0Cpv  808:   p = RI_CTL_808_* & 0xF, v = RB808_* voice; ACCENT is
 *           section-wide and only ever keyed with v = 0
 *   0x0Dpv  909:   p = RI_CTL_909_* & 0xF, v = RB909_* voice; v = 0xF with
 *           LEVEL = the shared CH/OH level knob (909 has 11 voices)
 * Values stay 0..127 (switch rows: 0 off, nonzero on). */
#define RI_AUTO_BLK_MIX 0x0B00u
#define RI_AUTO_BLK_808 0x0C00u
#define RI_AUTO_BLK_909 0x0D00u
#define RI_AUTO_MIX_LEVEL 0u
#define RI_AUTO_MIX_PAN 1u
#define RI_AUTO_MIX_SEND 2u
#define RI_AUTO_MIX_DIST 3u /* + RI_ROUTE_* unit: 3 dist, 4 pcf, 5 comp */
#define RI_AUTO_STRIP_MASTER 4u
#define RI_AUTO_909_HATPAIR 0xFu
#define RI_AUTO_ID_MIX(strip, p) \
    ((uint16_t)(RI_AUTO_BLK_MIX | ((((uint32_t)(strip) + 1u) & 0xFu) << 4) | ((uint32_t)(p) & 0xFu)))
#define RI_AUTO_ID_808(p, v) \
    ((uint16_t)(RI_AUTO_BLK_808 | (((uint32_t)(p) & 0xFu) << 4) | ((uint32_t)(v) & 0xFu)))
#define RI_AUTO_ID_909(p, v) \
    ((uint16_t)(RI_AUTO_BLK_909 | (((uint32_t)(p) & 0xFu) << 4) | ((uint32_t)(v) & 0xFu)))

int ri_auto_allowed(uint16_t ctl); /* 1 on the allow-list, else 0 */
int ri_auto_value(const struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                  uint8_t *out); /* 1 found (latest <= tick), 0 none */
int ri_auto_touch(struct RIAutoLane *l, struct RIAutoPass *p,
                  uint8_t tr_state, uint32_t cursor, uint32_t ppq,
                  uint16_t ctl, uint8_t val); /* punch-in + write; 0/2 */
int ri_auto_sweep(struct RIAutoLane *l, const struct RIAutoPass *p,
                  uint32_t from, uint32_t to,
                  const uint8_t *vals); /* erase unmarked span + re-anchor at `to`; 0/2 */
void ri_auto_punch_out_all(struct RIAutoPass *p); /* loop wrap: keep touched */
void ri_auto_pass_end(struct RIAutoLane *l, struct RIAutoPass *p); /* Stop: clear markers + both sets */
int ri_auto_clear_loop(struct RIAutoLane *l, uint32_t start_tick,
                       uint32_t len_ticks); /* drop [start,start+len) */
int ri_auto_stamp(struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                  uint8_t val); /* exact-tick write, denied refused; 0/2 */
int ri_auto_copy_touched(struct RIAutoLane *l, const struct RIAutoPass *p,
                         uint32_t start, uint32_t end,
                         const uint8_t *vals); /* range clear + start event; 0/2 */
/* Lane <-> triples transfer (codec/GUI bridge; engine never sees
 * RBAutoEv). Load validates all then replaces (unmarked file data);
 * store dumps (refuses when cap < n, *n set 0). rc 0/2. */
int ri_auto_load_triples(struct RIAutoLane *l, const uint32_t *ticks,
                         const uint16_t *ctls, const uint8_t *vals,
                         uint32_t n);
int ri_auto_store_triples(const struct RIAutoLane *l, uint32_t *ticks,
                          uint16_t *ctls, uint8_t *vals, uint32_t cap,
                          uint32_t *n);
#define RI_AUTO_CLIP_EVENTS RI_AUTO_MAX_EVENTS
/* Bar-edit clip: caller-owned storage (R10 — never inline 256 KB).
 * Ticks relative to base_tick; span_ticks is the cut/copy width so
 * paste shifts the tail exactly. */
struct RIAutoClip {
    uint32_t base_tick, span_ticks, n, cap;
    struct RIAutoEv *ev;
};
typedef char ri_auto_clip_small[(sizeof(struct RIAutoClip) < 64u) ? 1 : -1];
int ri_auto_cut(struct RIAutoLane *l, struct RIAutoClip *clip,
                uint64_t start_bar, uint64_t len_bars, uint32_t ppq);
int ri_auto_copy(const struct RIAutoLane *l, struct RIAutoClip *clip,
                 uint64_t start_bar, uint64_t len_bars, uint32_t ppq);
int ri_auto_paste(struct RIAutoLane *l, const struct RIAutoClip *clip,
                  uint64_t at_bar, uint32_t ppq);
int ri_auto_paste_replace(struct RIAutoLane *l, const struct RIAutoClip *clip,
                          uint64_t at_bar, uint32_t ppq);
#endif
