/* ri_pal_midi.h — MIDI in/out (portability plan T5, §3.5).
 * C99, includes only <stdint.h>.
 * Messages go into an SPSC queue the GUI thread drains into midimap.
 */
#ifndef RI_PAL_MIDI_H
#define RI_PAL_MIDI_H
#include <stdint.h>

typedef void (*ri_midi_in)(void *user, const uint8_t *msg, uint32_t len,
    uint64_t time_us);

struct RIMidiBridge;

int ri_pal_midi_open_in(const char *port, ri_midi_in cb, void *user);
/* `port` names the destination cluster; NULL/"" sends to the one
 * ri_pal_midi_open_in() was given. A 1-3 byte message is a short MIDI
 * message; a longer one MUST be a complete SysEx (F0 ... F7, 7-bit data)
 * and goes out as SysEx on the same link. */
int ri_pal_midi_send(const char *port, const uint8_t *msg, uint32_t len);
void ri_pal_midi_close(void);
/* Bridge router (M2): the backend feeds it; queue-direct users (RIAPP)
 * drain it. NULL until open_in succeeds. cb may be NULL (queues only). */
struct RIMidiBridge *ri_pal_midi_bridge(void);
/* AROS backend pump: drain one signal batch into the open callback.
 * Call from the app event loop. Host script backend delivers at open
 * (no pump needed); WinMM (T11) signals its own thread. */
void ri_pal_midi_poll(void);

/* M5e2: clock out's SENDER. The render only fills the producer's ring
 * (midi_out), so the bytes still have to reach camd from somewhere that is
 * not the render -- a task of its own, so the clock is not bunched into
 * whatever rhythm the UI happens to be running at.
 *
 * `o` is BORROWED, not owned, and must outlive the sender: the render
 * writes it, the sender reads it, and both use the ring's SPSC discipline
 * (render = producer, sender = consumer). `port` is the camd cluster this
 * sender sends to; NULL/"" means the one ri_pal_midi_open_in() was given.
 * The backend MUST use `port` when it is named -- an input driver port and
 * its output port are different clusters (R12).
 *
 * 0 started, non-zero refused (no timer device, no task, already running).
 * `ri_pal_midi_send_stop()` joins the task. A backend that has no task
 * concept may implement these by pumping inline on the caller's context,
 * which is legal for correctness and only affects wire timing.
 */
struct RIMidiOut;
int ri_pal_midi_send_start(const char *port, struct RIMidiOut *o);
/* Resolve camd.library WITHOUT opening a port. camd's CreateMidi/AddMidiLink
 * are inline calls through the library base, so any tool that talks CAMD
 * directly must call this first or it takes an illegal memory access on a
 * NULL base -- which is exactly how MIDIRX died on its first run.
 * 0 ok, non-zero refused. */
int ri_pal_midi_init_lib(void);
void ri_pal_midi_send_stop(void);
/* Counters for the ev-log: messages handed to camd, refusals. */
uint32_t ri_pal_midi_sent(void);
uint32_t ri_pal_midi_send_errors(void);
/* Clock messages among those sent: `sent` counts the WHOLE ring, so it
 * cannot be compared with a note count on its own. This one is a guest
 * COUNTER, not an estimate from BPM x duration -- the R12 O1 accounting
 * depends on being able to name the clock half of `sent` directly. */
uint32_t ri_pal_midi_clocks(void);
/* Note-ON messages among those sent (3-byte 0x9n with a non-zero velocity).
 * `sent` counts the whole ring and cannot tell a duplicated note-on from a
 * note-off; this counter can, and it says whether a duplicate left RIAPP. */
uint32_t ri_pal_midi_noteons(void);
/* R12 O2 diagnostic: note-ons with exactly the duplicated-accent signature
 * (status 0x90, note 36, velocity 112). A wire count above this one means
 * the extra copy appeared below CAMD, not in the app. Remove with the R12
 * duplicate finding. */
uint32_t ri_pal_midi_n36a(void);

#endif
