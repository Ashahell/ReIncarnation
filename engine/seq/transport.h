/* transport.h — transport state machine (spec 2026-09-25 §1).
 * Pure, stdint.h only, no alloc, no IO. Owns state + clicks only;
 * loop bounds arrive as read-only tick inputs, never stored. */
#ifndef RI_TRANSPORT_H
#define RI_TRANSPORT_H
#include <stdint.h>
enum RI_TRANSPORT { RI_TR_STOPPED = 0, RI_TR_PLAYING = 1, RI_TR_RECORD = 2 };
#define RI_SEQ_MAX_BARS 999u
#define RI_PPQ_DEFAULT 96u
#define RI_PPQ_MIN 4u
struct RITransport { uint8_t state; uint8_t clicks; };
/* One normalization: 0 -> default; sub-minimum -> default (keeps ppq/4 exact). */
static inline uint32_t ri_ppq_or_default(uint32_t ppq) {
    if (ppq == 0u || ppq < RI_PPQ_MIN) return RI_PPQ_DEFAULT;
    return ppq;
}
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick);
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_bars);
#endif
