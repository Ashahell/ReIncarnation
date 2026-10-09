/* midi_io/midi_levi.h — the Leviasynth channel: its own messages, its own
 * map (M4b/M4d). Pure C, host-tested, no engine calls, no allocation.
 *
 * E1 SOURCE. The Leviasynth Keyboard Owner's Manual v1.2.1, "MIDI CC
 * Charts" pp. 168-169 (sorted by CC number, then by module), and the MIDI
 * implementation pages around p. 149-152. The manual text stays out of
 * this repo; only the CC numbers (a wire standard) and the page citations
 * live here. Nothing is copied from midi.guide (CC BY-SA).
 *
 * WHAT THIS IS NOT. It does not touch a ReBirth control. Every message on
 * the Leviasynth channel becomes one action that the caller pushes into
 * the Leviasynth's own path (control plane, note queue, performance
 * signals); a ReBirth CC can never reach here and a Leviasynth CC can
 * never reach the panel map (midi_io/midi_chan.h, t183).
 *
 * engine/dsp/levi.h is included for the RI_CTL_LEVI_* KEY CONSTANTS
 * only: the map is "which Leviasynth parameter does this CC drive", and
 * inventing a second numbering here would be a second truth.
 */
#ifndef RI_MIDILEVI_H
#define RI_MIDILEVI_H
#include <stdint.h>
#include "midi_io/midi_chan.h"

#define RI_LEVI_ACT_NONE 0u
#define RI_LEVI_ACT_PARAM 1u   /* a Leviasynth parameter: key + val 0..127 */
#define RI_LEVI_ACT_NOTE 2u    /* note on/off: note, val = velocity */
#define RI_LEVI_ACT_PERF 3u    /* a performance signal: perf + val */

#define RI_LEVI_PERF_BEND 0u   /* pitch bend, val 0..16383 (14 bit) */
#define RI_LEVI_PERF_WHEEL 1u  /* CC 1 mod wheel, val 0..127 */
#define RI_LEVI_PERF_PRESS 2u  /* channel aftertouch (note unused) */
#define RI_LEVI_PERF_POLYAT 3u /* poly aftertouch, note set */
#define RI_LEVI_PERF_GLIDE 4u  /* CC 65 glide toggle (momentary override) */

struct RIMidiLeviAction {
    uint8_t kind;
    uint8_t note;      /* NOTE/POLYAT: the note number */
    uint16_t key;      /* PARAM: the Leviasynth control-plane key */
    uint16_t val;      /* PARAM/PERF/NOTE value (bend is 14 bit) */
    uint8_t on;        /* NOTE: 1 on / 0 off; PRESS/GLIDE: the new state */
    uint8_t perf;      /* PERF: RI_LEVI_PERF_* */
    uint8_t pad;
};

/* One complete message. Returns 1 when `out` holds an action the caller
 * must push, 0 when the message is not ours (wrong channel, unmapped CC,
 * reserved, unsupported). Never a ReBirth control. */
int midi_levi_message(const struct RIMidiChan *t, uint8_t status, uint8_t d1,
    uint8_t d2, struct RIMidiLeviAction *out);
/* The Leviasynth parameter key a CC drives, 0 when unmapped. */
uint16_t midi_levi_cc_key(uint8_t cc);
/* The manual page the row is cited from (168/169), 0 when unmapped. */
uint32_t midi_levi_cc_page(uint8_t cc);
/* Short reason a CC is not mapped, "" when it is. Kept short: it lands in
 * the ledger and in test failures. */
const char *midi_levi_cc_why(uint8_t cc);

#endif