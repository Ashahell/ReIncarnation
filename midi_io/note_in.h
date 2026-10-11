/* midi_io/note_in.h — R5: an external sequencer plays the ReBirth voices.
 *
 * R6 (outbound) shipped long ago. This is the other half. The Leviasynth
 * already has note input (channel 2, its own chart, RI_MCHAN_LEVI); these
 * are the four ReBirth devices.
 *
 * OWNER DECISIONS 2026-10-11:
 *
 *  1. **THE SAME SHAPE AS R6f/R6e.** Drums claim their channel implicitly
 *     and are counted; the 303s are opt-in. An inbound map that disagreed
 *     with the outbound one would mean a device that is silent on the way
 *     out and loud on the way in.
 *  2. **GM PERCUSSION LAYOUT** for the 808 and 909. Every DAW already knows
 *     it, so a GM drum track plays with no setup.
 *  3. **A SLIDE IS LEGATO FROM OVERLAP.** A DAW has no slide message; the
 *     only signal a piano roll carries is a note-on while the previous note
 *     is held -- which is the engine's own gate state.
 *
 * **A CLAIMED CHANNEL IS NOT AN ANIMATED CHANNEL.** An unclaimed channel is
 * refused and counted, never defaulted: silently binding a channel nobody
 * asked for is how a remote-control message ends up playing a drum.
 *
 * **A NOTE IS RESOLVED BY CHANNEL, NEVER BY PITCH.** GM percussion numbers
 * overlap the 303's pitch range, so the channel is the only thing that knows
 * which machine plays it.
 *
 * **UNRECOGNISED IS REFUSED AND COUNTED, NOT DROPPED.** A note that
 * resolves to nothing is a note the caller asked for and did not get.
 *
 * No allocation, no IO, no engine: it resolves notes. The caller plays them.
 */
#ifndef RI_NOTEIN_H
#define RI_NOTEIN_H
#include <stdint.h>

/* Devices, in MIXER SECTION order, so a section number is a device number
 * and nothing has to translate. */
#define RI_NOTEIN_DEV_303A 0u
#define RI_NOTEIN_DEV_303B 1u
#define RI_NOTEIN_DEV_808  2u
#define RI_NOTEIN_DEV_909  3u
#define RI_NOTEIN_DEV_COUNT 4u

/* Not an instrument. Distinct from slot 0: returning 0 for an unmapped note
 * would play the bass drum for a cowbell nobody asked for. */
#define RI_NOTEIN_NO_INSTRUMENT 0xFFu

/* The 808/909 instrument count this build speaks. */
uint32_t ri_notein_instruments(void);

/* The instrument a GM percussion note plays, or RI_NOTEIN_NO_INSTRUMENT.
 *
 * `note` above 127 is NOT an instrument either -- it is out of the MIDI
 * range, and folding it into the table would be a lookup in a table that
 * does not exist. */
uint32_t ri_notein_lookup(uint32_t note);

/* The engine voice a device plays instrument `inst` on. 808 and 909 have
 * DIFFERENT voice numbers for the same instrument -- the 909's lane order
 * puts the cowbell and the ride where the 808 has clap and rimshot -- so a
 * shared number would play the wrong machine's voice. */
uint32_t ri_notein_voice(uint32_t dev, uint32_t inst);

/* A note resolved against the bindings. The caller supplies the channel it
 * arrived on; `ch` is NOT taken from a global, because the whole resolution
 * is "which device claimed this channel". */
struct RINoteInEvent {
    uint8_t ch;
    uint8_t dev;      /* out: which device plays it */
    uint8_t note;     /* in */
    uint8_t vel;      /* in */
    uint8_t on;       /* in: 1 note-on, 0 note-off */
    uint8_t pitch;    /* out: the 303 pitch, unchanged */
    uint8_t inst;     /* out: the drum instrument */
    uint8_t accent;   /* out: the 303 accent */
    uint8_t slide;    /* out: legato from overlap */
    uint8_t held;     /* in: 1 = this device already has a note sounding */
};

struct RINoteIn {
    /* Channel 0 is INCLUDED and is never bindable: index 0 is the remote
     * (G7), so an array starting at 1 would need a separate "minus one"
     * everywhere it is read. */
    /* Channel -> device PLUS ONE, so a zeroed array honestly means
     * "nothing claimed" and device 0 needs no special case. */
    uint8_t ch_dev1[17];
    /* NO `ready` FLAG, AND THAT IS THE POINT. I added one, guarded every
     * entry point with it, and NI -- "the guard is dropped" -- then
     * SURVIVED, because with the index stored one higher than the device a
     * zeroed struct already means "nothing claimed" and every path refuses
     * it anyway. The flag was a second way of saying something the
     * encoding already says.
     *
     * The bug it was written for was real, though: device 0 IS 303A, so a
     * plain device index makes a zeroed array read as "every channel is
     * bound to 303A". My first fix -- rewriting zeros to 0xFF inside
     * bind() -- could not work either, because after binding 303A that
     * entry IS legitimately zero and the next bind swept it away, which is
     * exactly the failure the first test run showed. The fix is the +1. */
    uint8_t reserved[4];
    uint8_t held[RI_NOTEIN_DEV_COUNT];
    uint8_t drums_implicit;   /* R6e: the drums claimed their channel */
    uint32_t drums_claimed;
    uint32_t refused;         /* notes that resolved to nothing */
    uint32_t slides;
};

/* Bind a channel to a device. Returns 0, or non-zero for: NULL, channel 0
 * (the REMOTE, which cannot be a note channel), channel above 16, a device
 * past the last, or a channel already bound to a DIFFERENT device.
 *
 * REBINDING THE SAME DEVICE RELEASES ITS OLD CHANNEL. A setting change must
 * not leave a channel live that the panel no longer names. */
/* Put a struct in its initial state. REQUIRED before any other entry
 * point, and a zeroed struct is NOT the same thing -- see the note on
 * `ch_dev1`. `ri_notein_init` is a memset; it is worth calling anyway
 * because it is where that requirement is documented. */
void ri_notein_init(struct RINoteIn *n);

int ri_notein_bind(struct RINoteIn *n, uint32_t ch, uint32_t dev);

/* 1 if no device has claimed `ch`. Always true for channel 0: the remote
 * control channel is not a note channel and must never be treated as one. */
int ri_notein_unclaimed(const struct RINoteIn *n, uint32_t ch);

/* Resolve one note. Returns 0, or non-zero for a NULL, an unclaimed or
 * remote channel, an unmapped GM note on a drum channel, a release with no
 * note held, or an out-of-range note. Every refusal increments `refused`. */
int ri_notein_note(struct RINoteIn *n, struct RINoteInEvent *ev);

/* The velocity at or above which a 303 note is an accent. R6e fixed the
 * OUTBOUND convention (112 accented, 64 plain); this is the inbound inverse
 * and it is ONE constant, for the same reason ri_smf_velocity is a single
 * function rather than a comparison scattered through a handler. */
#define RI_NOTEIN_ACCENT_VEL 96u

#endif
