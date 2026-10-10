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

void ri_devout_enable(struct RIDevOut *d, int on) {
    if (!d)
        return;
    d->enabled = on ? 1u : 0u;
}

uint8_t ri_devout_program(void) {
    return (uint8_t)RI_DEVOUT_PROGRAM_SYNTH_BASS_1;
}

uint32_t ri_devout_program_change(struct RIDevOut *d, uint8_t *buf,
    uint32_t cap, uint8_t channel) {
    if (!d || !buf || cap < 2u || !d->enabled)
        return 0u;
    if (channel > 15u)
        return 0u;       /* refuse, never clamp: 0 is the G7 remote */
    if (!d->assigned[channel])
        return 0u;       /* an unclaimed channel selects nothing on its own */
    buf[0] = (uint8_t)(0xC0u | channel);
    buf[1] = ri_devout_program();
    d->emitted++;
    return 2u;
}

uint32_t ri_devout_override(const struct RIDevOut *d) {
    return d ? d->override_ch : 0u;
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
/* ---------------------------------------------------------------------
 * R6b: the drum note maps. See midi_devout.h for why they are keyed by
 * sound id rather than by panel lane.
 * ------------------------------------------------------------------- */

/* General MIDI percussion. OWNER REVIEW ITEM: conventional, documented,
 * not defended. One table per machine because a sound id means different
 * things on the two, and one table indexed by lane would silently play the
 * wrong instrument on one of them. */
uint8_t ri_devout_drum808_note(uint8_t slot) {
    switch (slot) {
    case RB808_BD: return 36u;   /* bass drum    */
    case RB808_SD: return 38u;   /* snare        */
    case RB808_LT: return 43u;   /* low tom      */
    case RB808_MT: return 45u;   /* mid tom      */
    case RB808_HT: return 47u;   /* hi tom       */
    case RB808_LC: return 64u;   /* low conga    */
    case RB808_MC: return 63u;   /* mid conga    */
    case RB808_HC: return 62u;   /* hi conga     */
    case RB808_RS: return 37u;   /* rim shot     */
    case RB808_CL: return 56u;   /* cowbell      */
    case RB808_CP: return 39u;   /* hand clap    */
    case RB808_CH: return 42u;   /* closed hat   */
    default:      return 0u;    /* unknown id: refused, never guessed */
    }
}

uint8_t ri_devout_drum909_note(uint8_t voice) {
    switch (voice) {
    case RB909_BD: return 36u;
    case RB909_SD: return 38u;
    case RB909_LT: return 43u;
    case RB909_MT: return 45u;
    case RB909_HT: return 47u;
    case RB909_RS: return 37u;
    case RB909_CP: return 39u;
    case RB909_CH: return 42u;
    case RB909_OH: return 46u;   /* open hat */
    case RB909_CR: return 49u;   /* crash    */
    case RB909_RD: return 51u;   /* ride     */
    default:      return 0u;
    }
}

int ri_devout_needs_303_map(void) { return 0; }

uint32_t ri_devout_drum(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t drum_class, uint8_t sound) {
    uint8_t note;
    if (!d || !buf)
        return 0u;
    /* The channel is the CALLER's, passed through rather than assumed. */
    if (!d->enabled || drum_class > RI_DRUM_CLASS_909) {
        d->refused++;
        return 0u;
    }
    note = (drum_class == RI_DRUM_CLASS_808) ? ri_devout_drum808_note(sound)
                                             : ri_devout_drum909_note(sound);
    if (note == 0u) {
        /* Unknown sound. Defaulting to the bass drum would put a hi-hat on
         * the kick: audible, and invisible unless something counts it. */
        d->refused++;
        return 0u;
    }
    /* ri_devout_note applies the channel laws: an unclaimed channel is
     * refused and counted, so the G7 collision outranks the note map. */
    return ri_devout_note(d, buf, cap, channel, note, 0x64u, 0u);
}

/* ---------------------------------------------------------------------
 * R6c: the translation. See midi_devout.h.
 * ------------------------------------------------------------------- */

uint8_t ri_devout_velocity(uint16_t flags) {
    /* OWNER REVIEW ITEM, same convention as the exporter's
     * `ri_smf_velocity()` and t198 pins that the two agree. Accent louder,
     * plain middle; both inside 1..127. */
    return (flags & RI_EVFLAG_ACCENT) ? (uint8_t)112u : (uint8_t)64u;
}

uint32_t ri_devout_resolves_lane(void) { return 0u; }

int ri_devout_translate(const struct RIEvent *ev, struct RIDevEvent *out) {
    if (!ev || !out)
        return 0;
    out->note = (uint8_t)(ev->value & 0x7Fu);
    out->vel = ri_devout_velocity((uint16_t)ev->flags);
    out->is_off = 0u;
    out->is_legato = 0u;
    switch (ev->type) {
    case RI_EV_NOTE_ON:
        return 1;
    case RI_EV_NOTE_OFF:
        out->is_off = 1u;
        return 1;
    case RI_EV_NOTE_CONTINUE:
        /* rest + slide: the gate stays high and the pitch slews. This IS the
         * slide, and treating it as a fresh note-on is the re-attack that
         * midi_devout already refuses to emit. */
        out->is_legato = 1u;
        return 1;
    default:
        /* Flams, accents, pattern changes, automation, meters, transport:
         * internal events. A DAW recording these as notes is a DAW full of
         * ghosts, so they produce NOTHING rather than something plausible. */
        return 0;
    }
}

uint32_t ri_devout_record(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, const struct RINoteTapRec *rec) {
    struct RIDevEvent x;
    if (!d || !rec)
        return 0u;
    /* Anything that is not a plain note is refused, which covers the late
     * accent (nothing to send -- the caller counts it) and any kind added
     * later without this being updated. "Refused, never guessed" is the
     * whole law of this module; a new record kind silently becoming a note
     * is the failure mode of writing the test as `== LATE_ACCENT`. */
    if (rec->kind != RI_NOTEK_NOTE)
        return 0u;
    memset(&x, 0, sizeof x);
    x.note = rec->note;
    x.vel = ri_devout_velocity(rec->flags);
    x.is_off = rec->is_off;
    x.is_legato = (rec->flags & RI_EVFLAG_SLIDE) != 0u;
    return ri_devout_emit(d, buf, cap, channel, rec->device, &x, rec->sound);
}

uint32_t ri_devout_emit(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t device, const struct RIDevEvent *x,
    uint8_t sound) {
    uint8_t note, flags;
    if (!d || !x)
        return 0u;
    /* The device ids are the engine's own (sched.h: 0/1 303A/303B, 2 808,
     * 3 909), so this function and engine.c read the same field. Anything
     * else is a device whose note mapping nobody has written, and guessing
     * one is how a hi-hat ends up on the kick. */
    if (device > RI_DEVOUT_909) {
        d->refused++;
        return 0u;
    }
    flags = x->is_legato ? RI_DEVOUT_LEGATO
          : (x->is_off ? RI_DEVOUT_NOTE_OFF : 0u);
    if (device == RI_DEVOUT_808)
        note = ri_devout_drum808_note(sound);
    else if (device == RI_DEVOUT_909)
        note = ri_devout_drum909_note(sound);
    else {
        note = x->note;   /* melodic: ri_p303_note() already resolved it */
    }
    if (device == RI_DEVOUT_808 || device == RI_DEVOUT_909) {
        /* Decision 2: GM defines percussion on channel 10 and nowhere else,
         * so the caller's melodic channel does not apply to a drum. The
         * override is counted: a session where most notes arrive on a
         * channel the caller never chose should be visible, not silent. */
        if (channel != RI_DEVOUT_GM_PERCUSSION)
            d->override_ch++;
        channel = RI_DEVOUT_GM_PERCUSSION;
        /* Claimed implicitly, because it is not a choice: leaving the
         * assignment requirement in place would mean the override moved the
         * note to a channel nothing had claimed, and every drum would be
         * refused for arriving where it was just sent. The MELODIC channel
         * stays opt-in -- an unclaimed one is still refused there. */
        d->assigned[RI_DEVOUT_GM_PERCUSSION] = 1u;
    }
    if (note == 0u) {
        d->refused++;
        return 0u;
    }
    /* Every path ends at ri_devout_note, so the channel laws -- unclaimed
     * refused, clamps counted, legato silent -- apply to drums too. And the
     * ACCENT reaches a drum hit: routing this through ri_devout_drum()
     * instead would have pinned every drum at one velocity. */
    return ri_devout_note(d, buf, cap, channel, note, x->vel, flags);
}
