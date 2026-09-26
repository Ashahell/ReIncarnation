/* ri_pal_midi.h — MIDI in/out (portability plan T5, §3.5).
 * C99, includes only <stdint.h>.
 * Messages go into an SPSC queue the GUI thread drains into midimap.
 */
#ifndef RI_PAL_MIDI_H
#define RI_PAL_MIDI_H
#include <stdint.h>

typedef void (*ri_midi_in)(void *user, const uint8_t *msg, uint32_t len,
    uint64_t time_us);

int ri_pal_midi_open_in(const char *port, ri_midi_in cb, void *user);
int ri_pal_midi_send(const char *port, const uint8_t *msg, uint32_t len);
void ri_pal_midi_close(void);
/* AROS backend pump: drain one signal batch into the open callback.
 * Call from the app event loop. Host script backend delivers at open
 * (no pump needed); WinMM (T11) signals its own thread. */
void ri_pal_midi_poll(void);

#endif
