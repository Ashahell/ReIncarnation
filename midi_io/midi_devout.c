/* midi_io/midi_devout.c — R6 note and CC output per device.
 * See midi_devout.h for why the channel map and the slide rule live here.
 */
#include <string.h>

#include "midi_io/midi_devout.h"
#include "gui/ctlreg.h"

void ri_devout_init(struct RIDevOut *d, int on) {
    if (!d)
        return;
    memset(d, 0, sizeof *d);
    d->enabled = on ? 1u : 0u;
    /* Channel 0 is claimed BY DEFAULT and owned by the G7 remote. A voice
     * has to be given a channel explicitly; it never inherits this one. */
    d->assigned[0] = 1u;
}

int ri_devout_enabled(const struct RIDevOut *d) {
    return (d && d->enabled) ? 1 : 0;
}

int ri_devout_assign(struct RIDevOut *d, uint8_t device, uint8_t channel) {
    (void)device;
    if (!d || channel > 15u)
        return 1;
    d->assigned[channel] = 1u;
    return 0;
}

void ri_devout_unassign(struct RIDevOut *d, uint8_t device) {
    (void)device;
    /* With one argument there is nothing to unassign BY DEVICE, so this
     * releases every voice channel and leaves the G7 remote's. The device
     * identity belongs to the caller, which knows its own map. */
    uint32_t i;
    if (!d)
        return;
    for (i = 1u; i < 16u; i++)
        d->assigned[i] = 0u;
}

int ri_devout_is_remote(const struct RIDevOut *d, uint8_t channel) {
    (void)d;
    return channel == 0u ? 1 : 0;
}

/* Clamp into 0..127 and COUNT it. Wrapping is never right: note 200
 * wrapping to 72 is a different instrument, and a caller cannot tell. */
static uint8_t clamp127(struct RIDevOut *d, uint32_t v) {
    if (v > 127u) {
        if (d)
            d->clamped++;
        return 127u;
    }
    return (uint8_t)v;
}

uint32_t ri_devout_note(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t note, uint8_t vel, uint32_t flags) {
    uint8_t ch;
    if (!d || !buf)
        return 0u;
    if (!d->enabled) {
        d->refused++;
        return 0u;
    }
    /* CLAMP the channel, never mask it. Masking 16 to 0 would put a voice
     * on the G7 remote channel, which is the one collision this feature
     * exists to avoid. */
    ch = channel;
    if (ch > 15u) {
        d->clamped++;
        ch = 15u;
    }
    if (!d->assigned[ch]) {
        /* No channel claimed means no sound. Defaulting here is the bug
         * this whole check exists to prevent. */
        d->refused++;
        return 0u;
    }
    if (flags & RI_DEVOUT_LEGATO) {
        /* A slide slews the pitch with the gate already high. Re-attacking
         * is the audible defect; emitting nothing is correct, and counting
         * it is what keeps it from looking like a silent failure. */
        d->legato++;
        return 0u;
    }
    if (cap < 3u) {
        /* All or nothing: a truncated note is not a shorter note. */
        d->refused++;
        return 0u;
    }
    note = clamp127(d, note);
    vel = clamp127(d, vel);
    buf[0] = (uint8_t)((flags & RI_DEVOUT_NOTE_OFF) ? 0x80u : 0x90u) | ch;
    buf[1] = note;
    buf[2] = (uint8_t)((flags & RI_DEVOUT_NOTE_OFF) ? 0x40u : vel);
    if (flags & RI_DEVOUT_NOTE_OFF)
        d->note_on[ch] = 0u;
    else
        d->note_on[ch] = 1u, d->note[ch] = note;
    d->emitted++;
    return 3u;
}

uint32_t ri_devout_cc(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t controller, uint8_t value) {
    uint8_t ch = channel;
    if (!d || !buf)
        return 0u;
    if (!d->enabled) {
        d->refused++;
        return 0u;
    }
    if (ch > 15u) {
        d->clamped++;
        ch = 15u;
    }
    if (cap < 3u) {
        d->refused++;
        return 0u;
    }
    controller = clamp127(d, controller);
    value = clamp127(d, value);
    buf[0] = (uint8_t)(0xB0u | ch);
    buf[1] = controller;
    buf[2] = value;
    d->emitted++;
    return 3u;
}

const char *ri_devout_cc_named(const struct RIDevOut *d, uint8_t controller) {
    const struct RICtlDef *def;
    (void)d;
    /* The G7 registry IS Appendix C. Resolving through it is what keeps
     * this module from carrying a second, divergent copy of the map. */
    def = ri_ctlreg_by_cc(controller);
    if (!def || !def->legend || !def->legend[0])
        return 0;
    return def->legend;
}

uint32_t ri_devout_clamped(const struct RIDevOut *d) { return d ? d->clamped : 0u; }
uint32_t ri_devout_refused(const struct RIDevOut *d) { return d ? d->refused : 0u; }
uint32_t ri_devout_legato(const struct RIDevOut *d) { return d ? d->legato : 0u; }
uint32_t ri_devout_emitted(const struct RIDevOut *d) { return d ? d->emitted : 0u; }