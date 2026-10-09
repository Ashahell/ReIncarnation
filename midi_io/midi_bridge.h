/* midi_bridge.h — MIDI bridge core (M2 interop, R1 applied).
 * Pure C, host-tested. One owner per byte class: realtime and system
 * common go to the owned follower (intents queue, transport side in
 * M3); channel voice goes to the message queue (GUI drains it into
 * midimap, G7 behaviour unchanged, LEDs included). Every byte also
 * reaches the message queue, exactly as RISECT remote observes it.
 * Bounded SPSC rings, caller-owned, no allocation, no IO. A bridge
 * task (AROS) or the script backend (host) feeds; the app drains.
 */
#ifndef RI_MIDI_BRIDGE_H
#define RI_MIDI_BRIDGE_H
#include <stdint.h>
#include "platform/pal/ri_pal_thread.h"
#include "midi_io/midi_follow.h"

/* P-19 flood cap (E0, ledgered): oldest dropped and counted. */
#define RI_MBR_CH_CAP 256u /* channel messages */
#define RI_MBR_IN_CAP 64u  /* follower intents */

struct RIMidiMsg {
    uint8_t b[3];
    uint8_t n;      /* bytes used (1..3) */
    uint8_t pad[3];
    uint64_t t_us;  /* arrival stamp (bridge task clock; script time on host) */
};

struct RIMidiIntent {
    struct RIFollowIntent it;
    uint64_t t_us;
};

struct RIMidiBridge {
    struct RIFollow follow;
    struct RIMidiMsg ch[RI_MBR_CH_CAP];
    ri_atomic_u32 ch_head; /* writer count (bridge side) */
    ri_atomic_u32 ch_tail; /* reader count (app side) */
    uint32_t ch_dropped;   /* writer side */
    struct RIMidiIntent in[RI_MBR_IN_CAP];
    ri_atomic_u32 in_head;
    ri_atomic_u32 in_tail;
    uint32_t in_dropped;   /* writer side */
    uint32_t camd_dropped; /* backend-reported CAMD buffer-full events */
    uint32_t link_lost;    /* backend-reported cluster loss (sticky to close) */
};

void midi_bridge_init(struct RIMidiBridge *b);
/* One CAMD message (status + data as delivered). Realtime/system common
 * also run the follower; a non-NONE intent queues. Every message queues
 * for the GUI. NULL bridge is a no-op. */
void midi_bridge_feed(struct RIMidiBridge *b, uint8_t status, uint8_t d1,
    uint8_t d2, uint64_t now_us);
/* Drain up to cap entries in FIFO order. Returns drained count. */
uint32_t midi_bridge_read_ch(struct RIMidiBridge *b, struct RIMidiMsg *out,
    uint32_t cap);
uint32_t midi_bridge_read_in(struct RIMidiBridge *b, struct RIMidiIntent *out,
    uint32_t cap);
uint32_t midi_bridge_pending_ch(const struct RIMidiBridge *b);
uint32_t midi_bridge_pending_in(const struct RIMidiBridge *b);
/* Backend reports (bridge-task side). */
void midi_bridge_note_camd_drop(struct RIMidiBridge *b);
void midi_bridge_note_link(struct RIMidiBridge *b, uint32_t lost);

/* MIDI settings (M2, E0 pending owner decision 3): per-machine ENVARC:
 * input cluster, remote channel, sync source, latency offset, Levi
 * channel, clock-out on/off. Never in the song. */
#define RI_MIDI_SET_CHANNEL 0u /* remote channel 1..16 */
#define RI_MIDI_SET_SYNC 1u    /* RI_SYNC_INTERNAL/RI_SYNC_MIDI */
#define RI_MIDI_SET_LEVI_CH 2u /* Leviasynth channel 1..16 (M4) */
#define RI_MIDI_SET_CLK_OUT 3u /* clock/MMC out 0/1 (M5, off) */
#define RI_MIDI_SET_LAT_MS 4u  /* latency offset ms -200..+200 (M3) */

struct RIMidiSettings {
    char cluster[64];  /* input cluster name */
    uint8_t channel;   /* 1..16, user-facing (midimap takes channel - 1) */
    uint8_t sync;      /* RI_SYNC_* */
    uint8_t levi_ch;   /* 1..16 */
    uint8_t clk_out;   /* 0/1 */
    int16_t lat_ms;
    int16_t pad;
};

void midi_settings_defaults(struct RIMidiSettings *s);
/* 0 accepted, 1 bad field/value (previous value kept). */
int midi_settings_set(struct RIMidiSettings *s, uint32_t field, long v);
#endif
