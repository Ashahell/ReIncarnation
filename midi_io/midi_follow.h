/* midi_follow.h — MIDI clock-in follower core (R1, interop spec).
 * Own laws (clean-room): 24ppqn tick stream -> BPM estimate (rolling
 * 24-tick mean, ±25% outlier rejection), lock after 24 consecutive
 * in-tolerance ticks, adaptive dropout (96 missed ticks -> STOP,
 * latched), Start/Continue/Stop/SPP intents for the caller to apply
 * via ri_tr_* (no transport coupling here). PPQ coupling: engine
 * PPQ=96 (P-20), so one SPP beat (16th) = 24 engine ticks. Pure C,
 * host-tested; CAMD wiring is a later slice.
 */
#ifndef RI_MIDI_FOLLOW_H
#define RI_MIDI_FOLLOW_H
#include <stdint.h>

/* Intent kinds (caller applies; follower never touches transport). */
#define RI_FOLLOW_NONE 0u
#define RI_FOLLOW_PLAY_START 1u
#define RI_FOLLOW_CONTINUE 2u
#define RI_FOLLOW_STOP 3u
#define RI_FOLLOW_SEEK 4u

/* Dropout law: silence worth 96 tick-intervals (4 quarters) ends the
 * take with a latched STOP; with fewer than two ticks there is no
 * interval yet, so a fixed 2 s applies. */
#define RI_FOLLOW_COLD_US 2000000u
#define RI_FOLLOW_MISSED_TICKS 96u
/* Lock law: 24 consecutive in-tolerance ticks; tolerance ±1/4. */
#define RI_FOLLOW_LOCK_N 24u

struct RIFollowIntent {
    uint8_t kind;
    uint8_t pad[3];
    uint32_t seek_tick; /* engine ticks, SEEK only */
};

struct RIFollow {
    uint64_t last_us;
    uint32_t ring[RI_FOLLOW_LOCK_N]; /* accepted intervals, us */
    uint64_t sum_us;
    uint32_t n;      /* intervals in ring (<= 24) */
    uint32_t streak; /* consecutive in-tolerance ticks */
    uint8_t have_tick;
    uint8_t stop_latched;
    uint8_t spp_pend; /* 0 idle, 1 want lsb, 2 want msb */
    uint8_t spp_lsb;
};

void midi_follow_init(struct RIFollow *f);
/* 24ppqn tick at now_us (monotonic). Intent always NONE here; the
 * estimate/lock update. 0 ok, 2 bad. */
int midi_follow_tick(struct RIFollow *f, uint64_t now_us,
    struct RIFollowIntent *it);
/* Transport messages. SPP beats are MIDI beats (16ths). 0 ok, 2 bad. */
int midi_follow_start(struct RIFollow *f, struct RIFollowIntent *it);
int midi_follow_continue(struct RIFollow *f, struct RIFollowIntent *it);
int midi_follow_stop(struct RIFollow *f, struct RIFollowIntent *it);
int midi_follow_spp(struct RIFollow *f, uint32_t spp_beats,
    struct RIFollowIntent *it);
/* Realtime byte parser (R1 wire format): F8 tick, FA/FB/FC
 * transport, F2 + lsb + msb song position; FE/tune ignored; channel
 * voice bytes ignored (G7 owns notes/CC). Realtime lands even inside
 * an SPP pair (MIDI law); other status aborts a pending SPP. 0 ok
 * (intent may be NONE), 2 bad. */
int midi_follow_rt(struct RIFollow *f, uint8_t byte, uint64_t now_us,
    struct RIFollowIntent *it);
/* Dropout poll: STOP once when silent past the law, else NONE.
 * 0 ok, 2 bad. */
int midi_follow_poll(struct RIFollow *f, uint64_t now_us,
    struct RIFollowIntent *it);
/* Measured BPM (0 until two ticks); 1 when locked. 0/negative on bad. */
float midi_follow_bpm(const struct RIFollow *f);
int midi_follow_locked(const struct RIFollow *f);

/* Sync-source state (R1 settings core): Internal vs MIDI clock, measured
 * tempo latch, read-only law, dropout fallback. Pure; the caller owns
 * the tempo knob and the transport. */
#define RI_SYNC_INTERNAL 0u
#define RI_SYNC_MIDI 1u

struct RIFollowSync {
    uint8_t source;    /* RI_SYNC_* */
    uint8_t following; /* 1 while locked to MIDI clock */
    uint8_t pad[2];
    float measured_bpm; /* latched while following */
    float held_bpm;     /* frozen on dropout (display keeps reading) */
};

void midi_sync_init(struct RIFollowSync *s);
/* 0 ok, 2 bad source/NULL. Switching source clears following. */
int midi_sync_set_source(struct RIFollowSync *s, uint32_t source);
/* Latch the follower reading (call per block): locked MIDI clock
 * starts/keeps following with the measured tempo. 0 ok, 2 bad. */
int midi_sync_update(struct RIFollowSync *s, uint32_t locked, float bpm);
/* Dropout STOP observed: following ends, held tempo frozen. 0/2. */
int midi_sync_note_drop(struct RIFollowSync *s);
/* 1 when the tempo knob must read the measured value (locked MIDI
 * following); 0 (internal, or MIDI idle) leaves the knob live. */
int midi_sync_knob_locked(const struct RIFollowSync *s);
/* Display tempo: measured while following, held after dropout, 0.0f
 * when internal or never locked (knob owns it). Negative on bad. */
float midi_sync_tempo(const struct RIFollowSync *s);

#endif
