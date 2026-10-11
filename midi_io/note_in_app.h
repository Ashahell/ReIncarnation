/* midi_io/note_in_app.h — R5b: the app actually plays the note.
 *
 * R5a resolves a MIDI message into "device 2, lane 0, accent 0". NOTHING
 * CALLED IT. This is the wiring: a resolved note becomes an engine event on
 * the right device, at the right moment, in the right order.
 *
 * **THE ENGINE IS DRIVEN BY EVENTS, NOT BY A SECOND NOTE PATH.**
 * `RI_EV_NOTE_ON` already reaches `rb303_note(v, pitch, slide, accent)` for
 * devices 0 and 1 and `rb808_trigger` / `rb909_trigger` for 2 and 3. So a
 * live note is a real `RIEvent`, not a new call into the voices: a second
 * path into the machines would be a second answer to "when does a 303
 * sound", and it would be wrong in a way nothing would report.
 *
 * **THE EVENT IS STAMPED AT THE CURSOR, NOT AT "NOW".** MIDI arrives in a
 * drain between audio buffers; the engine is mid-song. Handing it an event
 * at the drain's own sample position would place notes in the gap between
 * buffers, where the cursor will never be. So the live tap queues, and
 * `ri_notein_pump` stamps from the engine's cursor when it is called.
 *
 * **A LIVE NOTE DOES NOT DISTURB THE SONG.** It goes in its own ring beside
 * the pattern's events rather than being merged into them, so a note the
 * user plays cannot renumber or overwrite a scheduled one.
 *
 * **THE DRUM PATH CARRIES THE LANE, NOT THE 909 VOICE.** The engine does
 * `voice = RI_LANE_TO_RB909_VOICE[lane]` itself; doing it here too would be
 * the same map in two places.
 *
 * No allocation, no IO. It turns resolved notes into events; the caller
 * hands them to the engine's live tap.
 */
#ifndef RI_NOTEIN_APP_H
#define RI_NOTEIN_APP_H
#include <stdint.h>
#include "engine/engine.h"
#include "midi_io/note_in.h"

/* 64 is PENDING-NOTE DEPTH, not a flood cap -- see the ledger. It is sized
 * to admit a legitimate 16th roll plus a few bars of anything else, and to
 * be a fixed bound so a flood can never grow a heap. */
#define RI_NOTEIN_LIVE_CAP 64u

/* A resolved note waiting for the next pump. */
struct RINoteLiveSlot {
    uint8_t dev;
    uint8_t voice;     /* the drum LANE; 0 for a 303 */
    uint8_t value;     /* the 303 pitch; 0 for a drum */
    uint8_t flags;     /* RI_EVFLAG_* */
};

struct RINoteLive {
    struct RINoteLiveSlot ring[RI_NOTEIN_LIVE_CAP];
    uint32_t head;      /* oldest */
    uint32_t queued;
    uint32_t dropped;   /* overflowed, or refused at push */
    uint32_t pumped;
};

void ri_notein_live_init(struct RINoteLive *l);

/* Queue a RESOLVED note. Returns 0, or non-zero for a NULL, a device that
 * is not one of the four, a drum note with no lane, or a 303 note-off (the
 * 303 has a gate; a release is a NOTE_OFF event, which this ring does not
 * carry -- see the ledger, that is a known gap).
 *
 * AN UNRESOLVED NOTE IS NOT QUEUED. It is dropped and counted: queuing
 * something that will never resolve costs a slot and hides the fault. */
int ri_notein_push(struct RINoteLive *l, const struct RINoteInEvent *ev);

/* Stamp up to `cap` queued notes at the engine's current cursor and write
 * them to `out` in the §8 order, oldest first. Returns how many were
 * written.
 *
 * A `cap` SMALLER THAN THE QUEUE writes what fitted and LEAVES THE REST:
 * the caller pumps again next buffer. An earlier version of a similar
 * buffer in this project refused everything, which is correct behaviour for
 * a stem file and useless here. */
uint32_t ri_notein_pump(struct RINoteLive *l, struct RIEngine *e,
    struct RIEvent *out, uint32_t cap);

#endif
