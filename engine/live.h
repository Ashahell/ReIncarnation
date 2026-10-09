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
#include "platform/pal/ri_pal_thread.h"

#define RI_LIVE_QUEUE 256u /* pending future-event queue (one block max) */
/* Tempo-map segments per take (E0): a tempo change appends one at the
 * current tick, so the played part of the map keeps its anchor. MIDI
 * clock follow rewrites the tempo several times a second, so this is
 * sized in seconds rather than in "musical events": 64 covers ~3.5 s of
 * following, and when it fills, the map collapses to the current rate and
 * re-anchors both cursors together (ri_live_set_bpm), which stalls
 * nothing -- it only rewrites history nobody reads back. */
#define RI_LIVE_MAX_SEGS 64u

struct RILiveMeters {
    float sec_peak[RI_ROUTE_NSECTIONS];
    float fx_peak[4];
    float comp_gr;
    float master_peak[2]; /* S4: post-master L/R (mono path: twins) */
    uint64_t samples;
    uint64_t cursor_ticks;
    uint32_t xruns;
};

/* Render-stage breakdown (Dell 2026-10-02). The render cost was only ever known
 * as one aggregate, and that aggregate was quoted as though it described a
 * playing buffer when it averaged over a mostly-idle session — the "0.6 % of a
 * core" error. These counters are per stage, and ST_STOPPED is kept separate
 * from the playing stages precisely so idle and playing cannot be confused
 * again.
 *
 * now_us is injected (NULL = no timing); the engine makes no OS call itself.
 * sum/max/n are task-side; the GUI reads them as diagnostic-only
 * unsynchronized values, exactly like load_pm. */
#define RI_LIVE_ST_STOPPED 0u /* silence fill + stopped knob drain */
#define RI_LIVE_ST_EVENTS  1u /* pub apply, carry reindex, chase, player, auto, ctl */
#define RI_LIVE_ST_SORT    2u /* the section 8 event sort */
#define RI_LIVE_ST_FILTER  3u /* drop events outside this buffer's window */
#define RI_LIVE_ST_LOAD    4u /* ri_engine_load */
#define RI_LIVE_ST_DSP     5u /* ri_engine_render: voices + fx + mixer */
#define RI_LIVE_ST_METERS  6u /* meter publish */
#define RI_LIVE_ST_TOTAL   7u /* the whole playing render */
#define RI_LIVE_ST_COUNT   8u

struct RILiveStages {
    uint64_t sum_us[RI_LIVE_ST_COUNT];
    uint32_t max_us[RI_LIVE_ST_COUNT];
    uint32_t n[RI_LIVE_ST_COUNT];
    uint32_t playing_buffers; /* buffers that took the playing path */
    uint32_t stopped_buffers; /* buffers that took the silence path */
};

struct RILiveSession {
    struct RIEngine eng;
    struct RILiveStages stages;
    uint64_t (*now_us)(void); /* injected clock; NULL disables stage timing */
    struct RITransport tr;
    uint64_t cursor_ticks;
    uint32_t ppq;
    float sr;
    float bpm;
    uint64_t nspq;
    /* Active-device mask (owner 2026-09-27, rack requirement): crosses the
     * GUI/render boundary as one atomic, read once per render call, so a
     * flip applies at the next block by construction. Disabled = voice
     * never triggers (zero CPU); banks/voices/patterns keep state. */
    ri_atomic_u32 sections;
    /* Tempo map segments. A tempo change APPENDS one at the current tick
     * instead of rewriting the single segment: the map stays anchored for
     * everything already played, so the tick cursor and the audio
     * position cannot disagree. (Rewriting it moved map_tick(cursor)
     * ahead of sample_cursor on a tempo DROP and the forward-only tick
     * walk froze — M3c Dell run: engine frozen on 29 of 143 samples.) */
    struct RISegment segs[RI_LIVE_MAX_SEGS];
    uint32_t nsegs;
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
    /* Locate request (M3 SEEK/START). The render task owns BOTH cursors:
     * while playing it recomputes cursor_ticks from sample_cursor every
     * block, so a store from the app is lost (M3 lane proof: a Start
     * while the demo autoplayed left the engine at tick 12095). The
     * request counter hands the move to the render, which re-anchors
     * both and re-inits the player at the bar. */
    uint64_t locate_tick;
    ri_atomic_u32 locate_gen;
    uint32_t locate_done;
    ri_atomic_u32 meters_seq; /* seqlock around meters (see above) */
    uint64_t tick_rem;
    struct RILiveMeters meters;
};

void ri_live_init(struct RILiveSession *s, uint32_t ppq, float sr, float bpm,
    uint32_t sections, struct RIEvent *scratch, uint32_t scratch_cap);
/* Active-device mask (see the sections field): GUI-task callable at any
 * time; takes effect at the next rendered block. NULL fail-closed
 * (set ignores, get returns 0). */
void ri_live_set_sections(struct RILiveSession *s, uint32_t sections);
uint32_t ri_live_sections(struct RILiveSession *s);
/* GUI-side tempo follow (owner 2026-09-29: the transport knob was
 * display-only). Plain words, same tolerance class as the live-read
 * banks/track; the render task adopts s->bpm into the engine every
 * buffer. Out-of-range and NULL fail closed. */
void ri_live_set_bpm(struct RILiveSession *s, float bpm);
void ri_live_set_banks(struct RILiveSession *s,
    const struct RIPatternBank *const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *track, const struct RILoop *loop);
void ri_live_set_auto(struct RILiveSession *s, struct RIAutoPub *pub,
    struct RIAutoCarry *carry, struct RIAutoPass *pass);
void ri_live_set_ctl(struct RILiveSession *s, struct RIControlPlane *ctl);
void ri_live_play(struct RILiveSession *s);
/* Jump the engine to `tick` (render-owned; see locate_gen above). Works
 * stopped or playing, and returns once the request is queued — the render
 * applies it at the next block, before it reads either cursor. */
void ri_live_locate(struct RILiveSession *s, uint64_t tick);
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
/* Stage clock: NULL (the init default) disables timing entirely, so a
 * host or offline caller pays only the branch. The AHI backend sets its
 * EClock wrapper here once the timer is open. */
void ri_live_set_clock(struct RILiveSession *s, uint64_t (*now_us)(void));
const struct RILiveStages *ri_live_stages(const struct RILiveSession *s);
/* Render-published meter snapshot protocol (G9b Step 2, closes G6b): the
 * render task owns s->meters and bumps meters_seq around every update
 * (odd = update in flight, even = copy coherent). The GUI never reads
 * s->meters directly; it copies through ri_live_meters_read and retries
 * on 1. Single writer (render task) / single reader (GUI) by contract;
 * plain word-sized accesses, no locks. */
void ri_live_meters_begin(struct RILiveSession *s);
void ri_live_meters_end(struct RILiveSession *s);
/* Copy the published meters (livestate scales them for display). Returns
 * 0 ok, 1 busy/torn (sequence moved: drop this frame, retry next), 2 bad
 * args. A 1 is never an error: the next buffer publishes again. */
int ri_live_meters_read(const struct RILiveSession *s, struct RILiveMeters *out);
#endif
