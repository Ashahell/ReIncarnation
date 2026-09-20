/* midi.c — pure MIDI learn/clock/MMC/flood/status (Task 13, G13).
 * No allocation, no libm, no platform includes: host + AROS clean.
 */
#include "midi_io/midi.h"

void midi_learn_init(struct RIMidiLearn *m) {
    int i;
    for (i = 0; i < 128; i++)
        m->cc_to_ctl[i] = -1;
}

void midi_learn(struct RIMidiLearn *m, uint32_t cc, uint32_t ctl) {
    if (!m || cc >= 128u || ctl > 0xffffu)
        return;
    m->cc_to_ctl[cc] = (int32_t)ctl;
}

int32_t midi_cc_lookup(const struct RIMidiLearn *m, uint32_t cc) {
    if (!m || cc >= 128u)
        return -1;
    return m->cc_to_ctl[cc];
}

uint32_t midi_note_step(uint8_t note) {
    return (uint32_t)note % 16u;
}

uint8_t midi_note_accent(uint8_t vel) {
    return vel >= 64u ? 1u : 0u;
}

int midi_mmc_cmd(const void *buf, uint32_t n) {
    const unsigned char *p = (const unsigned char *)buf;
    /* MMC transport: F0 7F <dev> 06 <cmd> F7. Commands: 01 stop,
     * 02 play, 03 deferred play. Anything else (incl. 09 pause: a
     * separate transport state this engine does not implement) is
     * not a command: -1, fail closed. */
    if (!p || n != 6u || p[0] != 0xf0u || p[1] != 0x7fu || p[3] != 0x06u ||
        p[5] != 0xf7u)
        return -1;
    if (p[4] == 0x02u || p[4] == 0x03u)
        return 1;
    if (p[4] == 0x01u)
        return 0;
    return -1;
}

void midi_clock_init(struct RIMidiClock *c) {
    c->ticks = 0;
}

void midi_clock_advance(struct RIMidiClock *c) {
    c->ticks++;
}

uint32_t midi_clock_drift(const struct RIMidiClock *c, uint32_t expect) {
    uint32_t d = c->ticks > expect ? c->ticks - expect : expect - c->ticks;
    return d;
}

void midi_flood_init(struct RIFlood *f) {
    f->len = 0;
    f->dropped = 0;
}

void midi_flood_push(struct RIFlood *f, uint8_t b) {
    uint32_t i;
    if (f->len < RI_MIDI_MAX_PER_BUFFER) {
        f->ev[f->len++] = b;
        return;
    }
    /* Shed oldest-first: shift down, count the drop. */
    for (i = 1u; i < f->len; i++)
        f->ev[i - 1u] = f->ev[i];
    f->ev[f->len - 1u] = b;
    f->dropped++;
}

uint32_t midi_flood_len(const struct RIFlood *f) {
    return f ? f->len : 0u;
}

uint8_t midi_flood_get(const struct RIFlood *f, uint32_t i) {
    if (!f || i >= f->len)
        return 0;
    return f->ev[i];
}

/* Host stub state machine (AROS: mirrored by the CAMD bridge). */
static int RI_MIDI_STATE = RI_MIDI_OK;

int midi_backend_status(void) {
    return RI_MIDI_STATE;
}

void midi_backend_unplug(void) {
    RI_MIDI_STATE = RI_MIDI_TIMEOUT;
}

void midi_backend_replug(void) {
    RI_MIDI_STATE = RI_MIDI_OK;
}

int midi_transport_alive(void) {
    /* Transport never depends on the MIDI device: timeout drops
     * pending MIDI events (counted bridge-side) and playback
     * continues (TC-2.8.4). */
    return 1;
}
