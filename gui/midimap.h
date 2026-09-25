/* gui/midimap.h — Remote MIDI Control, Standard Mapping (§12.10 G7).
 * Pure C, host-tested. ReBirth 2.0.1 Owner's Manual chapter 13
 * (p. 127–134) + Appendix C (p. 195–204):
 * - one input, ONE channel (p. 134); ReBirth only receives (p. 127);
 * - Control Change = the panel control carrying that controller number in
 *   the registry (gui/ctlreg.c holds Appendix C p. 195–198);
 * - Note On = the switch tables: "Various Switches" (p. 199, all modes),
 *   Pattern Selection (p. 200–201, when Select Patterns is on) and the
 *   focused section's switches (p. 202–204, when Program Synth is on);
 * - MIDI LED: any message except System Exclusive (p. 144);
 * - Sync LED: red on the downbeat, green on the other beats (p. 145).
 * Value laws where the manual is silent are E0 (ledger
 * docs/evidence/gui/midi.md): a CC value 0..127 spans the control's
 * range linearly; a switch is on at >= 64; a note with velocity 0 is a
 * note-off and does nothing.
 */
#ifndef RI_MIDIMAP_H
#define RI_MIDIMAP_H
#include <stdint.h>
#include "gui/panelui.h"

struct RIMidiIn {
    uint8_t channel;        /* 0..15, the Preferences channel */
    uint8_t status;         /* running status (0 = none) */
    uint8_t data[2], ndata;
    uint8_t in_sysex;
    uint32_t clocks;        /* MIDI clocks since Start (24 per quarter) */
    uint16_t midi_led_ms;   /* MIDI-in LED on-time left */
    uint32_t messages, ignored;
};

#define RI_MIDI_LED_MS 100u /* E0: how long the MIDI LED stays lit per message */

void ri_midi_init(struct RIMidiIn *m, uint8_t channel);
/* One complete message (CAMD delivers these). Returns 1 when the panel
 * state changed. */
int ri_midi_msg(struct RIMidiIn *m, struct RIPanelUI *p, uint8_t status, uint8_t d1, uint8_t d2);
/* Raw byte stream (running status, realtime interleave, SysEx skip). */
int ri_midi_byte(struct RIMidiIn *m, struct RIPanelUI *p, uint8_t b);
/* Time passes (ms): MIDI LED off after RI_MIDI_LED_MS. Returns 1 when an
 * LED changed. */
int ri_midi_elapse(struct RIMidiIn *m, struct RIPanelUI *p, uint32_t ms);
#endif
