/* project/smf_bridge.h — R8f: engine events to SMF. Pure C, host-tested,
 * no allocation, no IO.
 *
 * `ri_smf_write` has been built and tested since R7a and has NO CALLER:
 * nothing in the app, the CLI or the UI reaches it. This is the conversion
 * between it and the only event stream in the project, so SMF export and stem
 * export can share one CLI invocation instead of needing two doors.
 *
 * IT IS NOT A RENAME. The two event enums are numbered differently --
 * `RI_EV_NOTE_ON` is 3 and `RI_SMF_EV_NOTE_ON` is 0, `RI_EV_NOTE_OFF` is 2
 * and `RI_SMF_EV_NOTE_OFF` is 1 -- so passing `RIEvent.type` straight through
 * would turn every note-on into a note-off: a file of releases with no
 * attacks, which imports as silence.
 *
 * AND ONE ENGINE EVENT CANNOT CROSS AT ALL.
 *
 * **A SLIDE IS DROPPED AND COUNTED.** `RI_EV_NOTE_CONTINUE` is rest+slide:
 * the gate stays high and the pitch slews. SMF 1.0 has no legato pitch
 * change, and both available fakes are wrong in a way the listener hears --
 * note-off plus note-on is the re-attack R6a exists to refuse, and a bare
 * pitch bend is a *different note* on a track that is supposed to be this
 * pattern. So it is dropped, and `tracks[i].slides` says how many. This is
 * the same class of limit as R6c's late accent and the monophony gap: real,
 * stated, and not hidden. `RI_SMF_SLIDE` is defined in `smf_export.h` and
 * referenced nowhere, so "the flag exists" is not "the writer handles it".
 *
 * THE BRIDGE DOES NOT MAP NOTES. `ri_p303_note()` has already put a real
 * MIDI note in `RIEvent.value` before the scheduler ran, so the number is
 * carried through and a second mapping table would only be a second place to
 * be wrong.
 *
 * SAMPLE POSITIONS ARE CARRIED UNCHANGED. The writer turns samples into
 * ticks against its own `SMF_SR`; second-guessing that here would be a second
 * answer to a question it already answers.
 */
#ifndef RI_SMFBRIDGE_H
#define RI_SMFBRIDGE_H
#include <stdint.h>
#include "engine/seq/sched.h"
#include "project/smf_export.h"

/* One device's worth, into the caller's borrowed array. `channel` is the
 * SMF channel the track lives on, 0..15.
 *
 * Writes at most `cap` events and returns how many, which is the number of
 * NOTE-ON and NOTE-OFF events that FITTED -- so a short `out` truncates
 * rather than overwriting, and the caller can see the count. `t->slides`
 * counts the slides dropped, including any that did not fit. */
uint32_t ri_smf_bridge(struct RISmfTrack *t, struct RISmfEvent *out,
    uint32_t cap, const struct RIEvent *ev, uint32_t nev, uint8_t channel);

/* Every device as its own track, in device order, so track N is always the
 * same instrument and a DAW's track list is readable. `tracks`/`out` are
 * borrowed arrays the caller sized.
 *
 * Returns the number of tracks, or 0 for a NULL, a zero-capacity array, or
 * no events that can cross. **A device with no note events gets NO track**:
 * an empty track in a DAW's list is a file that looks longer than the music.
 *
 * `ppq` AND `bpm_milli` ARE THE CALLER'S, passed straight to
 * `ri_smf_song_init`. The bridge cannot infer either one: ppq is a property
 * of how the song was quantised, not of the events, and tempo has no
 * representation in the engine's event stream at all. Leaving them at zero
 * is not a harmless default -- the delta path treats ppq 0 as "every event
 * is on tick 0", which is a file that plays correctly only if it has one
 * event. t208 pins both. */
uint32_t ri_smf_bridge_song(struct RISmfSong *s, struct RISmfTrack *tracks,
    struct RISmfEvent *out, uint32_t outcap, const struct RIEvent *ev,
    uint32_t nev, uint16_t ppq, uint32_t bpm_milli);

#endif