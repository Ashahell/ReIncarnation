/* midi_io/midi_chan.h — MIDI device instances and their channels (M4a).
 * Pure C, host-tested. No engine, no CAMD, no allocation.
 *
 * The rack rule: devices are keyed by INSTANCE, not by message kind. Two
 * devices with the same job are still two devices, and a device has one
 * channel; a channel belongs to one device. Reassigning a channel that is
 * already taken is REFUSED, never silently stolen — the ReBirth remote
 * control (G7) must keep working while a Leviasynth is plugged in.
 *
 * E0 (ledger docs/evidence/midi/ledger.md): instance 0 = the remote on
 * channel 1, instance 1 = the Leviasynth on channel 2. Channels are
 * 1..16 as the manual and the MIDI spec count them (1 = status 0x?0).
 *
 * The split this table exists for: a message on the remote channel goes to
 * the ReBirth panel map (Appendix C, gui/midimap.c) and NOTHING ELSE; a
 * message on the Leviasynth channel goes to the Leviasynth map
 * (midi_io/midi_levi.c) and NOTHING ELSE. A Leviasynth CC must never move
 * a ReBirth control and a ReBirth CC must never move a Leviasynth
 * parameter (t183).
 */
#ifndef RI_MIDICHAN_H
#define RI_MIDICHAN_H
#include <stdint.h>

#define RI_MCHAN_MAX 4u      /* E0: two used, room for two more */
#define RI_MCHAN_REMOTE 0u   /* the ReBirth remote control (G7) */
#define RI_MCHAN_LEVI 1u     /* the Leviasynth */
#define RI_MCHAN_TAKEN 3     /* return code: that channel belongs to another device */

struct RIMidiChan {
    uint8_t role[RI_MCHAN_MAX];  /* RI_MCHAN_* */
    uint8_t ch[RI_MCHAN_MAX];    /* 1..16 */
    uint8_t n;
    uint8_t pad;
};

void midi_chan_defaults(struct RIMidiChan *t);
/* The role bound to `ch` (1..16), or -1 when nobody has it. */
int midi_chan_role(const struct RIMidiChan *t, uint8_t ch);
/* Bind `role` to `ch`, releasing whatever channel it had. 0 ok,
 * RI_MCHAN_TAKEN when `ch` is another device's, 2 on a bad argument. */
int midi_chan_bind(struct RIMidiChan *t, uint8_t role, uint8_t ch);
uint32_t midi_chan_n(const struct RIMidiChan *t);

#endif