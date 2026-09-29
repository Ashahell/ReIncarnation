/* t127_midi_sync — sync-source state (R1 interop settings core).
 * Internal vs MIDI source; measured latch while locked; knob read-only
 * law; dropout freezes held tempo and ends following; fail-closed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_follow.h"

int main(void) {
    struct RIFollowSync s;
    midi_sync_init(&s);
    /* Fail-closed. */
    RI_ASSERT(midi_sync_set_source(0, RI_SYNC_MIDI) == 2, "src null");
    RI_ASSERT(midi_sync_set_source(&s, 7u) == 2, "src bad");
    RI_ASSERT(midi_sync_update(0, 1u, 120.0f) == 2, "upd null");
    RI_ASSERT(midi_sync_note_drop(0) == 2, "drop null");
    RI_ASSERT(midi_sync_knob_locked(0) == 0, "lock null");
    RI_ASSERT(midi_sync_tempo(0) < 0.0f, "tempo null");
    /* Default: internal, knob live, display 0. */
    RI_ASSERT(midi_sync_knob_locked(&s) == 0, "init unlocked");
    RI_ASSERT(midi_sync_tempo(&s) == 0.0f, "init tempo");
    /* MIDI source, unlocked clock: knob still live. */
    RI_ASSERT(midi_sync_set_source(&s, RI_SYNC_MIDI) == 0, "midi src");
    RI_ASSERT(midi_sync_knob_locked(&s) == 0, "unlocked live");
    RI_ASSERT(midi_sync_update(&s, 0u, 999.0f) == 0, "upd unlocked");
    RI_ASSERT(midi_sync_knob_locked(&s) == 0, "still live");
    RI_ASSERT(midi_sync_tempo(&s) == 0.0f, "unlocked latches nothing");
    /* Lock: following starts, knob frozen at measured. */
    RI_ASSERT(midi_sync_update(&s, 1u, 120.5f) == 0, "upd locked");
    RI_ASSERT(midi_sync_knob_locked(&s) == 1, "frozen");
    RI_ASSERT(midi_sync_tempo(&s) == 120.5f, "measured %f", midi_sync_tempo(&s));
    /* Dropout: following ends, held tempo frozen for display. */
    RI_ASSERT(midi_sync_note_drop(&s) == 0, "drop");
    RI_ASSERT(midi_sync_knob_locked(&s) == 0, "live again");
    RI_ASSERT(midi_sync_tempo(&s) == 120.5f, "held %f", midi_sync_tempo(&s));
    /* Relock resumes following. */
    RI_ASSERT(midi_sync_update(&s, 1u, 121.0f) == 0, "relock");
    RI_ASSERT(midi_sync_tempo(&s) == 121.0f, "remeasured");
    /* Back to internal clears following. */
    RI_ASSERT(midi_sync_set_source(&s, RI_SYNC_INTERNAL) == 0, "internal");
    RI_ASSERT(midi_sync_knob_locked(&s) == 0, "internal live");
    RI_ASSERT(midi_sync_tempo(&s) == 0.0f, "internal tempo");
    /* Reselecting MIDI starts idle (no stale following). */
    RI_ASSERT(midi_sync_set_source(&s, RI_SYNC_MIDI) == 0, "reselect");
    RI_ASSERT(midi_sync_knob_locked(&s) == 0, "reselect idle");
    RI_RESULT("midisync");
}
