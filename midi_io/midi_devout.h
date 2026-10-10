/* midi_io/midi_devout.h — note and CC output per device (R6).
 * Pure C, host-tested. No CAMD, no allocation, no float.
 *
 * WHAT THIS IS. R6 asks for patterns to play EXTERNAL synths: 303 steps
 * become notes with velocity for accent, 808/909 instruments become a note
 * map, and knob moves go out as CC so a DAW can record ReIncarnation's
 * automation. This module turns one already-sorted engine event into the
 * three bytes that go on the wire, and it owns two decisions that would
 * otherwise be made at every call site:
 *
 *  - WHICH CHANNEL a device plays on. That mapping is the whole risk. The
 *    G7 remote is a documented ONE-channel path (manual p. 134) that has to
 *    keep working while other things are plugged in (midi_chan.h), so a
 *    device with no assigned channel is REFUSED and counted. Defaulting it
 *    to channel 1 would put the 303 on the remote channel, and a note on the
 *    wrong instrument is worse than a note that does not play.
 *  - SLIDE IS LEGATO. A slide keeps the gate high and slews the pitch, so
 *    it must NOT re-attack; emitting a fresh note-on is the audible defect.
 *    It is its own flag rather than a note-on variant for exactly that
 *    reason.
 *
 * WHY CC NUMBERS ARE NOT A TABLE HERE. The controller numbers are Appendix
 * C's, and Appendix C is `gui/ctlreg.c` -- the same registry the G7 INPUT
 * path resolves through `ri_ctlreg_by_cc()`. A second copy of that table in
 * this file would be a second answer to the same question, and the two
 * would drift. So `ri_devout_cc_named()` asks the registry.
 *
 * OFF BY DEFAULT (E0, ledgered; Classic extension, pending owner decision
 * 1). "Off" means NOT ONE BYTE, exactly as for clock out and MMC: a
 * receiver is not obliged to ignore anything.
 *
 * THE BYTE PRODUCER IS NOT HERE. These three bytes go into `midi_out`'s
 * ring like any other outbound message, and the sender task carries them to
 * camd. That is the same path the clock uses, which is why R6 needs no
 * transport of its own and no new AROS-only code.
 */
#ifndef RI_MIDIDEVOUT_H
#define RI_MIDIDEVOUT_H
#include <stdint.h>

/* Flags for ri_devout_note(). */
#define RI_DEVOUT_LEGATO  0x01u  /* slide: gate stays high, no re-attack */
/* Note-OFF is its own FLAG rather than "velocity 0". Overloading velocity
 * would make a genuine velocity-0 note-on -- which is a note-off in the
 * SMF world and perfectly legal -- unrepresentable, and would put two
 * meanings in one byte that a reader has to guess between. */
#define RI_DEVOUT_NOTE_OFF 0x02u

struct RIDevOut {
    uint8_t enabled;
    uint8_t assigned[16];    /* per-channel: 1 = a device claims it */
    uint8_t note_on[16];     /* per-channel: gate currently high */
    uint8_t note[16];        /* per-channel: the sounding note */
    uint32_t clamped;        /* values clamped into 0..127 */
    uint32_t refused;        /* disabled, unassigned, or no room */
    uint32_t legato;         /* slides that emitted no note, by design */
    uint32_t emitted;        /* messages actually produced */
};

void ri_devout_init(struct RIDevOut *d, int on);
int ri_devout_enabled(const struct RIDevOut *d);

/* Claim / release a channel for a device. A device that has not claimed
 * one is refused; it is never defaulted. */
int ri_devout_assign(struct RIDevOut *d, uint8_t device, uint8_t channel);
void ri_devout_unassign(struct RIDevOut *d, uint8_t device);
/* Channel 0 is the G7 remote (manual p. 134) and is marked so a caller can
 * refuse to put a voice on it without hard-coding the number. */
int ri_devout_is_remote(const struct RIDevOut *d, uint8_t channel);

/* One note event. Writes 3 bytes and returns 3, or writes NOTHING and
 * returns 0 for any refusal: disabled, unassigned, no room, or a legato
 * slide (which is not a refusal -- see ri_devout_legato()).
 *
 * `channel` is masked into 0..15 and CLAMPED, never wrapped: a channel of
 * 16 wrapping to 0 would land on the G7 remote. Note and velocity clamp
 * into 0..127 and are counted. */
uint32_t ri_devout_note(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t note, uint8_t vel, uint32_t flags);
/* The note-off form is a REAL 0x8n, never a note-on with velocity 0, which
 * leaves notes hanging in a DAW. Ask for it with RI_DEVOUT_NOTE_OFF. */
uint32_t ri_devout_cc(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t controller, uint8_t value);
/* The Appendix C control behind a controller number, or NULL. Resolved
 * through the SAME registry the G7 input uses (gui/ctlreg.h), never from a
 * table held here. */
const char *ri_devout_cc_named(const struct RIDevOut *d, uint8_t controller);

uint32_t ri_devout_clamped(const struct RIDevOut *d);
uint32_t ri_devout_refused(const struct RIDevOut *d);
uint32_t ri_devout_legato(const struct RIDevOut *d);
uint32_t ri_devout_emitted(const struct RIDevOut *d);

#endif