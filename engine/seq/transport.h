/* transport.h — transport state machine (spec 2026-09-25 §1).
 * Pure, stdint.h only, no alloc, no IO. Owns state + clicks only;
 * loop bounds arrive as read-only tick inputs, never stored. */
#ifndef RI_TRANSPORT_H
#define RI_TRANSPORT_H
#include <stdint.h>
enum RI_TRANSPORT { RI_TR_STOPPED = 0, RI_TR_PLAYING = 1, RI_TR_RECORD = 2 };
#define RI_SEQ_MAX_BARS 999u
/* Song geometry, single source: a song is ALWAYS RI_SONG_BARS bars
 * (E1 always-999); valid bar starts are 0 .. RI_SONG_BARS-1, and the
 * end boundary (== RI_SONG_BARS) is never a valid start. */
#define RI_SONG_BARS RI_SEQ_MAX_BARS
#define RI_PPQ_DEFAULT 96u
#define RI_PPQ_MIN 4u
struct RITransport { uint8_t state; uint8_t clicks; };
/* One normalization: 0 -> default; sub-minimum -> default (keeps ppq/4 exact). */
static inline uint32_t ri_ppq_or_default(uint32_t ppq) {
    if (ppq == 0u || ppq < RI_PPQ_MIN) return RI_PPQ_DEFAULT;
    return ppq;
}
/* Downbeat quantizer (pure geometry, no state, no cursor): a flip
 * exactly on a downbeat keeps that bar; a mid-measure flip moves to
 * the next bar; result clamps to the last valid start (never 999).
 * Caller-side record path uses this; the track model stays gate-blind. */
static inline uint64_t ri_bar_quantize_next(uint64_t cursor_ticks, uint32_t ppq) {
    uint64_t p = (uint64_t)ri_ppq_or_default(ppq);
    uint64_t bar_ticks = 4u * p;
    uint64_t bar = cursor_ticks / bar_ticks;
    if (cursor_ticks % bar_ticks != 0u)
        bar++;
    if (bar > RI_SONG_BARS - 1u)
        bar = RI_SONG_BARS - 1u;
    return bar;
}
void ri_tr_play(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_stop(struct RITransport *t, uint64_t *cursor_ticks,
                uint64_t loop_start_tick, uint64_t song_start_tick);
void ri_tr_record(struct RITransport *t, uint64_t *cursor_ticks);
void ri_tr_seek_bars(struct RITransport *t, uint64_t *cursor_ticks, uint32_t ppq,
                     int32_t delta_bars, uint64_t song_bars);
struct RIBarPos { uint16_t bar; uint8_t beat; uint8_t sixteenth; };
/* Free bar helpers (no struct visibility). */
uint64_t ri_seq_tick_of_bar(uint32_t ppq, uint64_t bar);
uint64_t ri_seq_bar_at_tick(uint64_t tick, uint32_t ppq);
/* Display projection (one-way; never engine input). Pure function of
 * (tick, ppq) — no clamp inside; the caller clamps the tick first. */
struct RIBarPos ri_seq_bar_display(uint64_t tick, uint32_t ppq);
/* Loop geometry (song bars, 0-based). Clamp law: normalize to the song
 * (and the 999 ceiling) first; primitives stay total. */
struct RILoop { uint8_t on; uint16_t start_bar; uint16_t len_bars; };
struct RILoopPending { uint8_t valid; uint64_t apply_bar; struct RILoop loop; };
void ri_loop_clamp(struct RILoop *loop, uint64_t song_bars);
void ri_loop_stage(struct RILoopPending *p, const struct RILoop *loop, uint64_t bar_now,
                   uint64_t song_bars);
int ri_loop_poll(struct RILoopPending *p, struct RILoop *live, uint64_t bar_now);
#endif
