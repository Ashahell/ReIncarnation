/* live.h — G9.2 live session core (pure, host).
 * Streaming render through the one engine in device-buffer chunks:
 * transport + player + automation publish/carry + control plane merged
 * with the §8 sort, rendered in 64-frame engine blocks. Caller owns all
 * storage (no heap, no IO, no mutable static state).
 */
#ifndef RI_LIVE_H
#define RI_LIVE_H
#include <stdint.h>
#include "engine/engine.h"
#include "engine/seq/transport.h"
#include "engine/seq/player.h"
#include "engine/seq/autolane.h"
#include "engine/seq/autolane_emit.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/clock.h"

#define RI_LIVE_QUEUE 256u /* pending future-event queue (one block max) */

struct RILiveMeters {
    float sec_peak[4];
    float fx_peak[4];
    float comp_gr;
    uint64_t samples;
    uint64_t cursor_ticks;
    uint32_t xruns;
};

struct RILiveSession {
    struct RIEngine eng;
    struct RITransport tr;
    uint64_t cursor_ticks;
    uint32_t ppq;
    float sr;
    float bpm;
    uint64_t nspq;
    uint32_t sections;
    struct RISegment seg;
    struct RITempoMap map;
    struct RIPlayer player;
    const struct RISongTrack *track;
    const struct RIPatternBank *banks[RI_SONGTRACK_INSTANCES];
    struct RILoop loop;
    struct RIAutoPub *pub;
    struct RIAutoCarry *carry;
    struct RIAutoPass *pass;
    struct RIControlPlane *ctl;
    const struct RIAutoLane *last_front;
    int need_chase;
    struct RIEvent *scratch;
    uint32_t scratch_cap;
    struct RIEvent queue[RI_LIVE_QUEUE];
    uint32_t qn;
    uint64_t sample_cursor;
    uint32_t seq_next;
    uint32_t xruns;
    uint64_t tick_rem;
    struct RILiveMeters meters;
};

void ri_live_init(struct RILiveSession *s, uint32_t ppq, float sr, float bpm,
    uint32_t sections, struct RIEvent *scratch, uint32_t scratch_cap);
void ri_live_set_banks(struct RILiveSession *s,
    const struct RIPatternBank *const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *track, const struct RILoop *loop);
void ri_live_set_auto(struct RILiveSession *s, struct RIAutoPub *pub,
    struct RIAutoCarry *carry, struct RIAutoPass *pass);
void ri_live_set_ctl(struct RILiveSession *s, struct RIControlPlane *ctl);
void ri_live_play(struct RILiveSession *s);
void ri_live_record(struct RILiveSession *s);
void ri_live_stop(struct RILiveSession *s);
/* RECORD-state knob record (G9.5 host half): control-plane send for
 * immediate sound plus ri_auto_touch on the back lane. 0 ok / 2 refused. */
int ri_live_record_touch(struct RILiveSession *s, uint16_t key, uint8_t val);
/* Render exactly `frames` stereo samples (0 on bad args). STOPPED renders
 * silence. PLAYING/RECORD advances ticks/samples, merges player +
 * automation + control events, renders bit-exact across chunk sizes. */
uint32_t ri_live_render(struct RILiveSession *s, float *out_l, float *out_r,
    uint32_t frames);
const struct RILiveMeters *ri_live_meters(const struct RILiveSession *s);
#endif
