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
#define RI_MIDI_SET_MMC_OUT 5u /* MMC out 0/1 (M5h, off) -- SEPARATE from
                                * clk_out on purpose: clock out and MMC out
                                * are different features with different
                                * consequences on a slave, and one E0
                                * switch for both would make the safe
                                * choice (clock only) impossible to express. */
#define RI_MIDI_SET_DEV_OUT  6u /* note out 0/1 (R6, off) -- SEPARATE from
                                * clk_out for the SAME reason: someone
                                * slaving their own drum machine to our
                                * clock wants the clock and specifically NOT
                                * the notes, or turning on our 808 fires the
                                * very machine they are driving. The RING and
                                * the SENDER stay shared. */
#define RI_MIDI_SET_NOTE_CH  7u /* melodic note channel 0..16 (R6f); 0 =
                                * UNASSIGNED, and never defaulted -- a
                                * default would put the 303 on a channel
                                * nobody chose, and channel 0 is the G7
                                * remote. There is deliberately NO drum
                                * channel setting: GM defines percussion on
                                * channel 10 and it is not a choice, so a
                                * control for it could not do anything. */

struct RIMidiSettings {
    char cluster[64];  /* input cluster name */
    uint8_t channel;   /* 1..16, user-facing (midimap takes channel - 1) */
    uint8_t sync;      /* RI_SYNC_* */
    uint8_t levi_ch;   /* 1..16 */
    uint8_t clk_out;   /* 0/1 */
    int16_t lat_ms;
    uint8_t mmc_out;   /* 0/1 */
    uint8_t dev_out;   /* 0/1, note out (R6) */
    uint8_t note_ch;   /* 0 = unassigned, else 1..16 (R6f) */
    int16_t pad;
};

/* The melodic note channel for R6, 0-BASED, or -1 for none.
 *
 * OWNER DECISION 2026-10-10: LEVIASYNTH FOR MELODIES. When `note_ch` is
 * unset the melodic notes follow the Leviasynth channel, and the Leviasynth
 * channel is 2 by default. This is a decision, not an accident, and it lives
 * here rather than in `riapp` so it is one tested function rather than glue.
 *
 * `RIAPP_MIDI_NOTECH` still overrides it, so a user who wants the melodic
 * output somewhere other than their Leviasynth patch can say so.
 *
 * NOTE THE DIRECTION. M4 receives Leviasynth notes IN on this channel and
 * R6 sends melodic notes OUT on it -- opposite directions, so they do not
 * collide. They CAN loop, and only through the user's own routing: a DAW
 * that echoes channel 2 back into ReIncarnation's input would retrigger the
 * Leviasynth. That is a property of the patch bay, not of this code, and it
 * is the thing to check first if the Leviasynth ever plays itself.
 *
 * Drums are unaffected: GM channel 10, and no channel needed at all. */
int midi_settings_note_ch(const struct RIMidiSettings *s);

void midi_settings_defaults(struct RIMidiSettings *s);
/* 0 accepted, 1 bad field/value (previous value kept). */
int midi_settings_set(struct RIMidiSettings *s, uint32_t field, long v);
#endif
