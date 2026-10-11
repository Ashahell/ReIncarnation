/* midi_io/note_in.c — R5. See note_in.h for the owner decisions. */
#include <string.h>
#include "engine/seq/pattern.h"   /* RI_LANE_TO_RB808_SLOT / _RB909_VOICE */
#include "engine/dsp/rb909.h"
#include "midi_io/note_in.h"

/* THE GM PERCUSSION LAYOUT (owner, 2026-10-11).
 *
 * A PARTIAL map, and that is the point. It claims the GM numbers this build
 * speaks for the instruments the machines actually have; every other GM
 * number resolves to RI_NOTEIN_NO_INSTRUMENT and is refused and counted. A
 * complete GM table would map 27 numbers onto 12 instruments, and the extra
 * 15 would each be a claim that the sound exists here when it does not --
 * a cowbell played as a clap, silently, which is worse than a refusal.
 *
 * The instrument number is a LANE (RI_DRUM_CLASSIC_LANES order), because
 * that is the identity both voice tables are keyed by and `RI_LANE_TO_
 * RB808_SLOT` is the identity map (pinned by t53). */
static const struct { uint8_t note, inst; } RI_NOTEIN_GM[] = {
    { 36u, 0u },   /* bass drum      */
    { 38u, 1u },   /* snare          */
    { 42u, 11u },  /* closed hat     */
    { 41u, 2u },   /* low tom        */
    { 45u, 3u },   /* mid tom        */
    { 48u, 4u },   /* high tom       */
    { 49u, 8u },   /* rimshot        */
    { 54u, 10u },  /* handclap       */
    { 56u, 6u },   /* cowbell        */
    { 58u, 7u },   /* cymbal         */
    { 46u, 9u }    /* open hat       */
};

uint32_t ri_notein_instruments(void) {
    return (uint32_t)(sizeof RI_NOTEIN_GM / sizeof RI_NOTEIN_GM[0]);
}

uint32_t ri_notein_lookup(uint32_t note) {
    uint32_t i;
    /* ABOVE 127 IS NOT A LOOKUP. The MIDI note range ends at 127, and a
     * note past it is a caller bug rather than a percussion number; folding
     * it into the table would mean indexing something that is not there. */
    if (note > 127u)
        return RI_NOTEIN_NO_INSTRUMENT;
    for (i = 0u; i < ri_notein_instruments(); i++)
        if (RI_NOTEIN_GM[i].note == note)
            return RI_NOTEIN_GM[i].inst;
    return RI_NOTEIN_NO_INSTRUMENT;
}

uint32_t ri_notein_voice(uint32_t dev, uint32_t inst) {
    if (inst >= RI_DRUM_CLASSIC_LANES)
        return 0xFFFFFFFFu;
    /* THE TWO MACHINES HAVE DIFFERENT VOICE NUMBERS FOR THE SAME LANE.
     * The 808's lane order is BD SD LT MT HT LC MC RS CL CP CH and the
     * 909's is BD SD CH OH CR RD LT MT HT RS CP -- so lane 6 is the 808's
     * low cowbell and the 909's low ride. Passing one number to both plays
     * the wrong machine's voice on one of them. */
    if (dev == RI_NOTEIN_DEV_909)
        return RI_LANE_TO_RB909_VOICE[inst];
    return RI_LANE_TO_RB808_SLOT[inst];
}

void ri_notein_init(struct RINoteIn *n) {
    if (!n)
        return;
    memset(n, 0, sizeof *n);
    /* THE ARRAY IS DEVICE PLUS ONE, so this memset already means "nothing
     * claimed". The plus one is what makes a zeroed struct safe, which a
     * plain device index could never be: device 0 (303A) is a legal value
     * and therefore indistinguishable from "unset".
     *
     * There is deliberately no `ready` flag here: with the +1 encoding a
     * zeroed struct already refuses everything, and a second way of saying
     * "nothing is claimed" is a second thing to keep true. */
}

int ri_notein_bind(struct RINoteIn *n, uint32_t ch, uint32_t dev) {
    uint32_t i;
    if (!n || ch == 0u || ch > 16u || dev >= RI_NOTEIN_DEV_COUNT)
        return 1;
    if (n->ch_dev1[ch] != 0u && n->ch_dev1[ch] != (uint8_t)(dev + 1u))
        return 1;      /* already someone else's */
    /* REBINDING THE SAME DEVICE RELEASES ITS OLD CHANNEL. A setting change
     * must not leave a channel live that the panel no longer names -- that
     * is how a device ends up answering on two channels at once. */
    for (i = 1u; i <= 16u; i++)
        if (i != ch && n->ch_dev1[i] == (uint8_t)(dev + 1u))
            n->ch_dev1[i] = 0u;
    n->ch_dev1[ch] = (uint8_t)(dev + 1u);
    /* R6e: the drums are claimed IMPLICITLY and counted, so a session can
     * report that a drum channel came alive without anyone having set it. */
    if ((dev == RI_NOTEIN_DEV_808 || dev == RI_NOTEIN_DEV_909) &&
            !n->drums_implicit) {
        n->drums_implicit = 1u;
        n->drums_claimed++;
    }
    return 0;
}

int ri_notein_unclaimed(const struct RINoteIn *n, uint32_t ch) {
    /* CHANNEL 0 IS ALWAYS UNCLAIMED HERE. It is the ReBirth remote (G7,
     * Appendix C) and it is not a note channel; treating it as an unclaimed
     * note channel is how a remote message ends up playing a drum. */
    if (!n || ch == 0u || ch > 16u)
        return 1;
    return n->ch_dev1[ch] == 0u;
}

int ri_notein_note(struct RINoteIn *n, struct RINoteInEvent *ev) {
    uint32_t dev;
    if (!n || !ev || ev->note > 127u) {
        if (ev)
            ev->resolved = 0u;
        if (n)
            n->refused++;
        return 1;
    }
    /* AN UNINITIALISED STRUCT IS REFUSED BY THE ENCODING ITSELF: every
     * `ch_dev1` entry is 0, which means nothing claimed, so an unclaimed
     * channel is refused and counted above. A caller who forgets
     * ri_notein_init gets a silent module, not channel 0 playing the
     * 303A -- which is what a plain device index would have given. */
    ev->resolved = 0u;
    if (ri_notein_unclaimed(n, ev->ch)) {
        n->refused++;
        return 1;
    }
    dev = (uint32_t)n->ch_dev1[ev->ch] - 1u;
    if (dev >= RI_NOTEIN_DEV_COUNT) {
        n->refused++;
        return 1;
    }
    ev->dev = (uint8_t)dev;
    /* `resolved` IS SET ON EACH SUCCESS PATH, NOT ON THE WAY IN. I first
     * set it just before the device switch, which is before every refusal
     * below -- so a refused drum release came back with `resolved == 1`
     * and `dev == 0`, and the app ring queued the note the player was
     * trying to stop. A single flag set at the top of a function with six
     * exits is a flag that means "I got past the first check". */
    if (dev == RI_NOTEIN_DEV_303A || dev == RI_NOTEIN_DEV_303B) {
        /* THE 303 TAKES THE PITCH AS IT COMES. `ri_p303_note` goes the
         * other way (a pattern row to a MIDI number); a DAW has already
         * chosen a pitch and applying a second map would shift every note
         * a third. */
        ev->pitch = ev->note;
        ev->inst = 0u;
        if (!ev->on) {
            /* A RELEASE WITH NOTHING HELD is a refusal, not a note. This is
             * how a stuck note is FOUND rather than heard: a DAW sending a
             * release for a note that never arrived is a real fault in the
             * clip, and silently dropping it makes it invisible. */
            if (!n->held[dev]) {
                n->refused++;
                return 1;
            }
            n->held[dev] = 0u;
            ev->accent = 0u;
            ev->slide = 0u;
            ev->resolved = 1u;
            return 0;
        }
        /* LEGATO FROM OVERLAP (owner, 2026-10-11). A DAW has no slide
         * message, so the only signal a piano roll carries is a note-on
         * while the previous note is still held -- which is the gate state
         * this module already keeps. */
        ev->slide = n->held[dev] ? 1u : 0u;
        if (ev->slide)
            n->slides++;
        n->held[dev] = 1u;
        /* ONE NAMED THRESHOLD, not a comparison in the handler. */
        ev->accent = (ev->vel >= RI_NOTEIN_ACCENT_VEL) ? 1u : 0u;
        ev->resolved = 1u;
        return 0;
    }
    /* A DRUM IS NEVER A SLIDE. A 16th roll is exactly the overlap a 303
     * slide detects, and a slide would lengthen every hit after the first. */
    ev->pitch = 0u;
    ev->accent = 0u;
    ev->slide = 0u;
    if (!ev->on) {
        n->refused++;
        return 1;       /* a drum has no gate to release */
    }
    ev->inst = (uint8_t)ri_notein_lookup(ev->note);
    if (ev->inst == RI_NOTEIN_NO_INSTRUMENT) {
        /* AN UNMAPPED GM NOTE IS REFUSED AND COUNTED. Playing the nearest
         * instrument would be a guess that sounds like an answer. */
        n->refused++;
        return 1;
    }
    ev->resolved = 1u;
    return 0;
}
