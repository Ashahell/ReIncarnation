/* midi_io/midi_out.h — outbound MIDI (M5b): clock out and MMC's byte
 * producer. Pure C, host-tested. NO CAMD, no allocation, no float.
 *
 * WHY A PRODUCER AND NOT A SENDER. The plan's rule for clock out is "no
 * CAMD in the render path": the render calls `midi_out_render()` with its
 * sample position and nothing else, and this module fills a caller-owned
 * byte ring that a separate sender task drains. The confinement gate (no
 * AROS include outside platform/aros) is what keeps camd out, so the rule
 * is enforced by the build rather than by discipline.
 *
 * OFF BY DEFAULT (E0, ledgered; "Classic extension, pending owner
 * decision 1"). `midi_out_enable(o, 0)` — the initial state — means no
 * byte reaches the ring at all, not even a transport edge. A feature that
 * is off has to be silent on the wire rather than merely ignored by the
 * receiver, because a receiver is not obliged to ignore anything.
 *
 * The 24 ppqn schedule is `midi_clockout`'s and is not restated here
 * (t185 owns the arithmetic, t186 owns the bytes).
 *
 * SPP on locate follows the SAME law the follower has for inbound SPP:
 * legal while stopped, counted and ignored while running (`sppp_ignored`).
 * Two ends that disagree about when SPP is legal is a bug waiting for a
 * take.
 */
#ifndef RI_MIDIOUT_H
#define RI_MIDIOUT_H
#include <stdint.h>
#include "midi_io/midi_clockout.h"

/* Byte ring. Sized so one render block at 60 ppqn cannot fill it: at
 * 48 kHz / 256 frames a 60 ppqn clock is at most 1 byte per block, and
 * 256 bytes is far above any plausible per-block burst.
 * MUST be a power of two: occupancy is `(head - tail) & (CAP - 1)` because
 * the indices wrap, and a plain subtraction reads 4294967295 the moment
 * the reader overtakes the writer. t186 exercises the overtake. */
#define RI_MIDIOUT_CAP 256u

struct RIMidiOut {
    struct RIMidiClockOut clk;
    uint8_t buf[RI_MIDIOUT_CAP];
    uint32_t head;          /* writer index (render) */
    uint32_t tail;          /* reader index (sender task) */
    uint32_t dropped;       /* oldest-dropped-and-counted, P-19 style */
    uint32_t spp_ignored;   /* SPP refused because the transport runs */
    uint64_t last_tick;     /* ticks already emitted (see midi_out_render) */
    /* Sender-side framing state, owned BY THE PRODUCER rather than kept in
     * a function static: a static would be shared by every instance, so two
     * devices would interleave half-built messages, and it would survive
     * from one producer to the next with nothing to reset it. */
    uint8_t fmsg[3];        /* the message being assembled */
    uint8_t fn;             /* its length so far */
    uint8_t fhave;          /* 1 while one is being assembled */
    uint8_t enabled;        /* 0 until midi_out_enable */
    uint8_t running;        /* 1 between start/continue and stop */
    uint16_t pad;
};

void midi_out_init(struct RIMidiOut *o, uint32_t sr, uint32_t bpm_milli,
    uint32_t lead_smp);
/* The E0 on/off setting. Off (the initial state) emits nothing. */
void midi_out_enable(struct RIMidiOut *o, int on);
/* Transport edges. Start from stopped is FA; a start while already running
 * is FB (Continue). Both are no-ops while disabled. */
void midi_out_start(struct RIMidiOut *o, uint64_t sample_pos);
void midi_out_stop(struct RIMidiOut *o);
/* Song Position Pointer on locate, in SIXTEENTHS (M3b's unit: the intent
 * speaks the wire's own, not the app's PPQ). Stopped only; while running
 * it is counted in `spp_ignored` and sent nowhere. A value above 16383 is
 * refused rather than wrapped. */
void midi_out_locate(struct RIMidiOut *o, uint32_t sixteenths);
/* Called from the render with its absolute sample position. Emits one F8
 * per tick the schedule owes, exactly once each. */
void midi_out_render(struct RIMidiOut *o, uint64_t sample_pos);
/* Tempo change at an absolute position; re-anchors without re-timing the
 * past (M3f), and emits no byte. */
void midi_out_set_bpm(struct RIMidiOut *o, uint32_t bpm_milli,
    uint64_t sample_pos);
/* Drain the ring into `out`, oldest first. 0 when empty or NULL. */
uint32_t midi_out_read(struct RIMidiOut *o, uint8_t *out, uint32_t cap);
uint32_t midi_out_pending(const struct RIMidiOut *o);

/* The sender side. `ri_pal_midi_send` takes a whole message (1..3 bytes),
 * while the ring is a byte STREAM, so something has to frame them — and
 * framing is where a MIDI sender goes wrong: send F2 and its two data
 * bytes as three messages and the receiver reads a locate with a stale
 * byte in it.
 *
 * FRAMING LAW: a byte with the high bit SET is a status byte and starts a
 * new message; data bytes accumulate behind it; a message is emitted when
 * the next status byte arrives or the byte budget is full. A partial tail
 * stays buffered rather than being sent as a fragment.
 *
 * `sink(msg, len, user)` is called once per message, oldest first, and
 * should return 0 on success and non-zero on failure (which stops the
 * pump). Returns the number of bytes consumed from the ring, including a
 * partial tail left buffered. `budget` caps the bytes pulled per call so
 * the sender task cannot starve the render. */
typedef int (*ri_midi_sink)(const uint8_t *msg, uint32_t len, void *user);
uint32_t midi_out_pump(struct RIMidiOut *o, ri_midi_sink sink, void *user,
    uint32_t budget);

#endif
