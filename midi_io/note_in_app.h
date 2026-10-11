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

/* THE RING DEPTH: a MEMORY bound. It is the fixed array's size and nothing
 * else. */
#define RI_NOTEIN_RING_CAP 64u

/* P-19, THE FLOOD CAP: a RATE bound, per device. 64 pending notes is a
 * little over four bars of sixteenths at any plausible tempo (96 sixteenths
 * to the bar), so a legitimate drum roll never loses a hit -- and a fixed
 * depth means a flood cannot grow a heap, which is the stall the spec
 * actually names.
 *
 * THE TWO NUMBERS ARE DELIBERATELY SEPARATE. The ring bounds memory; the
 * cap bounds rate. R5b had one constant doing both jobs and calling it
 * `RI_NOTEIN_LIVE_CAP`, which is how a memory bound silently becomes a
 * note-dropping policy. They default to the same value because that is a
 * sensible default, not because they are the same thing.
 *
 * OWNER RECOMMENDED 2026-10-11; awaiting the owner's number. */
#define RI_NOTEIN_FLOOD_CAP 64u

/* A resolved note waiting for the next pump. */
struct RINoteLiveSlot {
    uint8_t dev;
    uint8_t type;      /* RI_EV_NOTE_ON or RI_EV_NOTE_OFF */
    uint8_t voice;     /* the drum LANE; 0 for a 303 */
    uint8_t value;     /* the 303 pitch; 0 for a drum */
    uint8_t flags;     /* RI_EVFLAG_* */
};

struct RINoteLive {
    struct RINoteLiveSlot ring[RI_NOTEIN_RING_CAP];
    uint32_t head;      /* oldest */
    uint32_t queued;
    uint32_t pumped;
    /* PER DEVICE. A flood on the 808 must not consume the 303A's budget --
     * otherwise one loud channel silences a melodic one, and the symptom is
     * "my bass line disappeared", which nobody can diagnose from a global
     * counter. */
    uint32_t pending[RI_NOTEIN_DEV_COUNT];
    uint32_t dropped[RI_NOTEIN_DEV_COUNT];
    /* DROPPED BEFORE ANY DEVICE IS KNOWN: an unresolved note has no device
     * (R5a leaves `dev` at 0, which is 303A, so attributing it to a device
     * would be a guess -- and my first version guessed wrong, writing the
     * 909's counter). Its own slot, not a device's. */
    uint32_t dropped_unresolved;
    uint32_t cap;       /* the flood cap, per device */
};

void ri_notein_live_init(struct RINoteLive *l);

/* Set the per-device flood cap. Returns 0, or non-zero for NULL, a cap of
 * 0, or a cap past the ring depth.
 *
 * REFUSED RATHER THAN CLAMPED. Clamping to the ring depth would be a silent
 * policy choice: a caller asking for 1000 and getting 64 would not know.
 *
 * Returns 0 and leaves the cap unchanged on a refusal, so a bad value
 * cannot leave a session with no cap at all. */
int ri_notein_live_set_cap(struct RINoteLive *l, uint32_t cap);

/* Queue a RESOLVED note. Returns 0, or non-zero for a NULL, a device that
 * is not one of the four, a drum note with no lane, or a 303 note-off (the
 * 303 has a gate; a release is a NOTE_OFF event, which this ring does not
 * carry -- see the ledger, that is a known gap).
 *
 * AN UNRESOLVED NOTE IS NOT QUEUED. It is dropped and counted: queuing
 * something that will never resolve costs a slot and hides the fault.
 *
 * A 303 RELEASE IS QUEUED (R5c). R5b refused it, which meant a live 303
 * note never stopped: it rang on until its envelope finished, every time.
 * A drum release is still refused -- a drum has no gate to release.
 *
 * AT THE DEVICE'S CAP THE OLDEST GOES AND THE NEWEST GOES IN. The player is
 * listening to the newest hit; a note from a buffer ago is the one that can
 * be dropped. This is the convention the outbound ring already uses, and the
 * drop is counted against THAT DEVICE so the log says which channel
 * flooded. */
int ri_notein_push(struct RINoteLive *l, const struct RINoteInEvent *ev);

/* Stamp up to `cap` queued notes at `stamp` and write them to `out`, oldest
 * first. Returns how many were written. `seq` is the caller's sequence
 * base; the events take `seq`, `seq+1`, ... so the caller can advance its own
 * counter by the return value.
 *
 * **THE STAMP IS THE CALLER'S, AND THAT IS THE WHOLE POINT.** R5b took a
 * `struct RIEngine *` and stamped at `e->cursor`, which is right for a
 * one-shot renderer and **silently wrong for the live session**:
 * `ri_live_render` windows every event to `[s0, s1)` where
 * `s0 = s->sample_cursor`, and `ri_engine_load` resets `e->cursor` to 0 on
 * every call -- once per buffer. So `e->cursor` is always 0, and sample 0 is
 * inside the window only for the FIRST buffer. Every later buffer's live
 * notes would be stamped below `s0` and **dropped by the filter with no
 * error and no count**: the first note sounds and the rest vanish, which is
 * a defect that gets blamed on the MIDI input.
 *
 * So there is no engine parameter to be wrong about, and two call sites with
 * different absolutes cannot reach each other's.
 *
 * **SEQ COMES FROM THE SESSION'S ONE COUNTER.** The §8 key ends in `seq` and
 * every other producer in `ri_live_render` draws from `s->seq_next`. A pump
 * that invented its own sequence space would place a live note ahead of or
 * behind every pattern event at the same sample for an accident of
 * magnitude rather than a decision.
 *
 * A `cap` SMALLER THAN THE QUEUE writes what fitted and LEAVES THE REST: the
 * caller pumps again next buffer, at the next window origin. */
uint32_t ri_notein_pump(struct RINoteLive *l, uint64_t stamp, uint32_t seq,
    struct RIEvent *out, uint32_t cap);

#endif
