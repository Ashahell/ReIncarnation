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
#include "engine/dsp/rb808.h"
#include "engine/dsp/rb909.h"
#include "engine/seq/pattern.h"   /* RI_DRUM_CLASS_808 / _909 */
#include "engine/seq/sched.h"    /* RIEvent, RI_EVFLAG_* */
#include "engine/engine.h"       /* RINoteTapRec (the engine's note tap) */

/* Flags for ri_devout_note(). */
#define RI_DEVOUT_LEGATO  0x01u  /* slide: gate stays high, no re-attack */
/* Note-OFF is its own FLAG rather than "velocity 0". Overloading velocity
 * would make a genuine velocity-0 note-on -- which is a note-off in the
 * SMF world and perfectly legal -- unrepresentable, and would put two
 * meanings in one byte that a reader has to guess between. */
#define RI_DEVOUT_NOTE_OFF 0x02u

/* ---------------------------------------------------------------------
 * R6e: the five owner conventions (2026-10-10). Each is a decision, not a
 * default, and each is pinned by t201 with a mutant that undoes it.
 * ------------------------------------------------------------------- */

/* DRUMS GO TO CHANNEL 10 AND IT IS NOT A CHOICE. GM defines percussion on
 * channel 10 and nowhere else: a drum note on channel 3 selects a melodic
 * instrument and plays a wrong pitched tone, or nothing at all. So
 * `ri_devout_emit` OVERRIDES the channel for the 808 and the 909 and counts
 * the override. A user who only wants the 808 out therefore needs no melodic
 * channel configured, which is the other half of the decision. */
#define RI_DEVOUT_GM_PERCUSSION 10u

/* THE PROGRAM, for the one program change sent on enable. GM has no 303
 * program, so ANY value here is a convention -- this one is Electric Bass
 * (pick), the closest analogue to a 303 line, and it is a named constant
 * precisely so that changing it is a one-line edit and not a search. */
#define RI_DEVOUT_PROGRAM_ELECTRIC_BASS_PICK 34u   /* GM 35, 1-based */

struct RIDevOut {
    uint8_t enabled;
    uint8_t assigned[16];    /* per-channel: 1 = a device claims it */
    uint8_t note_on[16];     /* per-channel: gate currently high */
    uint8_t note[16];        /* per-channel: the sounding note */
    uint32_t clamped;        /* values clamped into 0..127 */
    uint32_t refused;        /* disabled, unassigned, or no room */
    uint32_t legato;         /* slides that emitted no note, by design */
    uint32_t emitted;        /* messages actually produced */
    uint32_t override_ch;    /* drum notes moved onto GM channel 10 */
};

void ri_devout_init(struct RIDevOut *d, int on);
int ri_devout_enabled(const struct RIDevOut *d);
/* Set the enabled flag without sending anything. Disabling emits no byte and
 * leaves no partial message: a producer that is off is silent on the wire,
 * not merely ignored by the receiver. */
void ri_devout_enable(struct RIDevOut *d, int on);

/* R6e decision 5: ONE program change, on enable, on the MELODIC channel
 * only -- never per note (that is a stream of noise), and never while
 * disabled. Drums get none, because on channel 10 the kit IS the program.
 *
 * Two bytes: 0xCn, program. Returns 2, or 0 for any refusal: disabled, an
 * unclaimed channel, or a channel above 15 (refused, never clamped -- 0 is
 * the G7 remote and a clamp would select an instrument behind the user's
 * back). The CALLER puts these in `midi_out`'s ring; this module has no ring
 * and sends nothing. */
uint8_t ri_devout_program(void);
uint32_t ri_devout_program_change(struct RIDevOut *d, uint8_t *buf,
    uint32_t cap, uint8_t channel);

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

/* ---------------------------------------------------------------------
 * R6b: the drum note maps.
 *
 * KEYED BY THE ENGINE'S SOUND ID, NOT BY PANEL LANE. The two machines' lane
 * enums disagree -- lane 7 is CB on the 808 and CH on the 909, lane 10 is CH
 * and RC -- so ONE table indexed by lane would play a different instrument on
 * the two machines for four of eleven indices, silently.
 *
 * THE LANE->SOUND STEP IS NOT R6's, AND ALREADY EXISTS. The engine resolves
 * it before anything is emitted: `RI_LANE_TO_RB808_SLOT[i] == i` is PINNED
 * by t53, alongside `rb808_slot_of(RI_808_SLOT_DEFAULT[i]) == i`, so the lane
 * IS the slot and the sound stored in that slot is what the engine triggers.
 * The 808's live mapping is `e->s808.slot[lane]` (user-remappable); the 909's
 * is `RI_LANE_TO_RB909_VOICE[lane]`.
 *
 * I previously wrote here that the 808 table was an unfinished identity table
 * mapping rim shot onto low conga, and used that to call R6c blocked. Both
 * were wrong: I read the `RB808_*` slot MACROS as sound ids, and they are
 * not -- lane 5 IS slot 5, and slot 5 holds sound 8 (rim shot) by default.
 * The step R6c needs was always there.
 *
 * THE NOTES ARE A CONVENTION (General MIDI percussion), not a fact. They are
 * an OWNER REVIEW ITEM, isolated in the two functions below so changing them
 * is one edit and one test rather than a search. t196 pins that they are in
 * range, that distinct sounds do not collide WITHIN a machine, and that an
 * unknown id is refused rather than guessed -- defaulting it to the bass drum
 * would put a hi-hat on the kick, audible and uncounted.
 * ------------------------------------------------------------------- */

/* The 303 needs no map: `RI303Row.key` is a 0..12 scale degree and the
 * engine's `ri_p303_note()` already resolves it against RI_303_BASE_NOTE
 * with the octave flag. A second 303 map here would be a second answer to a
 * question the engine has answered. This returns 0 so the fact is pinned
 * rather than asserted in prose. */
int ri_devout_needs_303_map(void);

uint8_t ri_devout_drum808_note(uint8_t slot);
uint8_t ri_devout_drum909_note(uint8_t voice);
/* Emit one drum hit on `channel`, mapped from `sound`. Writes 3 bytes, or
 * nothing at all for an unknown class or sound id (counted as refused).
 *
 * `channel` is an ARGUMENT, not a default. The first version of this
 * hardcoded channel 0 -- which is the G7 remote -- so every drum hit landed
 * on the one channel the whole feature exists to keep clear, and the only
 * reason it was caught is that the test refused an unclaimed channel and
 * this one had quietly claimed it. The caller owns the device map. */
uint32_t ri_devout_drum(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t drum_class, uint8_t sound);

/* ---------------------------------------------------------------------
 * R6c: the event-to-bytes translation.
 *
 * THE ENGINE HAS ALREADY DONE EVERYTHING ELSE. `ri_p303_note()` puts a real
 * MIDI note in `RIStep.note` before the scheduler runs;
 * `e->s808.slot[lane]` and `RI_LANE_TO_RB909_VOICE[lane]` resolve lanes to
 * sounds. So R6c is not a mapping problem -- it is a TRANSLATION of four
 * flags into three bytes, and that is all this adds.
 * ------------------------------------------------------------------- */
struct RIDevEvent {
    uint8_t note;       /* MIDI note */
    uint8_t vel;        /* accent already folded in */
    uint8_t is_off;     /* a real note-off, not velocity 0 */
    uint8_t is_legato;  /* rest+slide: gate stays high, emit nothing */
};

/* One RIEvent -> one outbound note. Returns 1 when the event becomes a note
 * (on, off or slide) and 0 for every internal event -- flams, accents,
 * pattern changes, automation, meters, transport -- which must produce NO
 * bytes, because a DAW recording them as notes is a DAW full of ghosts. */
int ri_devout_translate(const struct RIEvent *ev, struct RIDevEvent *out);

/* The accent velocity convention. ONE convention for the live path AND the
 * SMF exporter (t198 pins that they agree): if they drift, a loop exported
 * from a live take comes back with different accents, which nobody hears as
 * "wrong" and everybody notices as "this loop feels different". */
uint8_t ri_devout_velocity(uint16_t flags);

/* Emit one translated event.
 *
 * `device` is the ENGINE'S device id, exactly as `RIEvent.device` carries it:
 * 0/1 are the two 303s, 2 is the 808 and 3 is the 909 (sched.h, and
 * engine.c's own dispatch). I first keyed this on `RI_DRUM_CLASS_*` and had
 * to throw it away: that enum has no value meaning "melodic", so the 303 path
 * and the drums shared one parameter and the drums' 0 collided with it --
 * which meant four mutants survived because the melodic path had never run.
 * One parameter, one meaning, taken from the event that already has it.
 *
 * `sound` is the ALREADY-RESOLVED drum sound and is ignored for a melodic
 * note: the CALLER owns the lane->sound step, because the 808's is live and
 * user-remappable and the 909's is static, and a helper that guessed would be
 * wrong for one of them. */
#define RI_DEVOUT_303A 0u
#define RI_DEVOUT_303B 1u
#define RI_DEVOUT_808  2u
#define RI_DEVOUT_909  3u
uint32_t ri_devout_emit(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, uint8_t device, const struct RIDevEvent *x,
    uint8_t sound);
/* One ENGINE TAP RECORD -> bytes. This is the seam R6d crosses: the engine
 * recorded a note without knowing a wire exists, and this is where it
 * becomes three bytes for `midi_out`'s ring.
 *
 * The record already carries the RESOLVED sound (see engine.h), so the
 * lane->sound step is not repeated here and cannot be repeated wrongly.
 *
 * A `RI_NOTEK_LATE_ACCENT` record emits NOTHING: a total accent is applied
 * retroactively to notes already sent, and MIDI has no way to make a note it
 * already sent louder. It is returned as 0 so the caller can count it. */
uint32_t ri_devout_record(struct RIDevOut *d, uint8_t *buf, uint32_t cap,
    uint8_t channel, const struct RINoteTapRec *rec);

/* R6 resolves no lanes: the engine does. Returns 0, pinned so the fact is a
 * test rather than a claim. */
uint32_t ri_devout_resolves_lane(void);

uint32_t ri_devout_clamped(const struct RIDevOut *d);
uint32_t ri_devout_refused(const struct RIDevOut *d);
uint32_t ri_devout_legato(const struct RIDevOut *d);
uint32_t ri_devout_emitted(const struct RIDevOut *d);
/* Drum notes whose channel was overridden onto GM channel 10, counted. */
uint32_t ri_devout_override(const struct RIDevOut *d);

#endif