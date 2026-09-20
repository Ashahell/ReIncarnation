/* midi.h — MIDI learn map + clock/MMC + flood cap + backend status
 * (Task 13, gate G13). Spec §13: CAMD backend, note→step, CC→ctl_id
 * learn, clock/MMC; hot-unplug survival (TC-2.8.4); flood shed at a
 * documented per-buffer cap (P-19 OPEN value: executor choice 64,
 * recorded in docs/evidence/formats/midi.md).
 *
 * This TU is pure/host-tested (no CAMD includes). The AROS CAMD
 * backend is midi_io/camd_backend.c (AROS-only, guarded, host-excluded
 * per the probe_ahi.c precedent); it drives this map.
 */
#ifndef RI_MIDI_H
#define RI_MIDI_H
#include <stdint.h>

/* P-19 OPEN value (executor choice, soak-gated): max MIDI events taken
 * per engine buffer before shedding. Excess is counted and dropped
 * oldest-first; playback timing never stalls on MIDI (§17 failure 4). */
#define RI_MIDI_MAX_PER_BUFFER 64u

/* Backend status codes (hot-unplug, TC-2.8.4). */
#define RI_MIDI_OK 0
#define RI_MIDI_NO_DEVICE 1
#define RI_MIDI_TIMEOUT 2

/* CC→control-ID learn map. Unmapped CCs fail closed (-1): no MIDI
 * input ever moves an unlearned control. */
struct RIMidiLearn {
    int32_t cc_to_ctl[128];
};

void midi_learn_init(struct RIMidiLearn *m);
void midi_learn(struct RIMidiLearn *m, uint32_t cc, uint32_t ctl);
int32_t midi_cc_lookup(const struct RIMidiLearn *m, uint32_t cc);

/* note→step + velocity accent (spec §13 note→step bridge). */
uint32_t midi_note_step(uint8_t note);
uint8_t midi_note_accent(uint8_t vel);

/* MMC sysex: returns 1 = play/deferred-play, 0 = stop,
 * -1 = not an MMC transport command this engine implements (pause and
 * anything else fail closed). */
int midi_mmc_cmd(const void *buf, uint32_t n);

/* Simulated master-clock tick counter (24 ticks/quarter, i.e. ppq/24
 * recorder quantum at 96 ppq): the drift test advances 100 bars and
 * asserts exact ticks. */
struct RIMidiClock {
    uint32_t ticks;
};

void midi_clock_init(struct RIMidiClock *c);
void midi_clock_advance(struct RIMidiClock *c);
uint32_t midi_clock_drift(const struct RIMidiClock *c, uint32_t expect);

/* Flood shed buffer: oldest-first drop beyond the cap, counted. */
struct RIFlood {
    uint8_t ev[RI_MIDI_MAX_PER_BUFFER];
    uint32_t len;
    uint32_t dropped;
};

void midi_flood_init(struct RIFlood *f);
void midi_flood_push(struct RIFlood *f, uint8_t b);
uint32_t midi_flood_len(const struct RIFlood *f);
uint8_t midi_flood_get(const struct RIFlood *f, uint32_t i);

/* Backend status (host: stub state machine; AROS: CAMD bridge state).
 * Unplug returns RI_MIDI_TIMEOUT; the transport MUST continue
 * (midi_transport_alive stays 1 — asserted in t1_formats). */
int midi_backend_status(void);
void midi_backend_unplug(void);
void midi_backend_replug(void);
int midi_transport_alive(void);
#endif
