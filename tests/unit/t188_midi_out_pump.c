/* t188_midi_out_pump — M5d: framing the byte stream into whole messages.
 *
 * The ring holds a byte STREAM; the transport underneath takes whole
 * messages of 1..3 bytes. Framing is therefore the sender's job, and it is
 * where a MIDI sender goes wrong: send F2 and its two data bytes as three
 * separate messages and the receiver reads a locate built from a stale
 * byte — which is the same "half-sent SPP is a wire-level lie" law t186
 * pins on the producer side, arriving from the other direction.
 *
 * Laws:
 * - a byte with the high bit set is a STATUS byte and starts a new
 *   message; data bytes accumulate behind it;
 * - a message is emitted whole, and a partial tail stays buffered rather
 *   than going out as a fragment;
 * - a budget caps the bytes pulled per call, so the sender task cannot
 *   starve the render;
 * - a sink that fails stops the pump, and the bytes it refused are still
 *   consumed from the ring (they are stale once sent) but nothing after
 *   them is taken — the caller retries next tick.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_out.h"

#define SR 48000u

/* A recording sink, so the test can assert on messages, not bytes. */
struct rec {
    uint8_t msg[64][4];
    uint8_t len[64];
    uint32_t n;
    int fail_at;      /* -1 never fails */
};

static int sink_rec(const uint8_t *m, uint32_t len, void *user) {
    struct rec *r = (struct rec *)user;
    if (r->fail_at >= 0 && (int)r->n == r->fail_at)
        return 1;
    if (r->n >= 64u)
        return 1;
    memcpy(r->msg[r->n], m, len);
    r->len[r->n] = (uint8_t)len;
    r->n++;
    return 0;
}

int main(void) {
    static struct RIMidiOut o;
    static struct rec r;
    uint32_t used, i;

    /* --- three clocks are three messages, not one of three bytes ------ */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    midi_out_render(&o, 3000u);          /* 3 ticks */
    memset(&r, 0, sizeof r);
    r.fail_at = -1;
    used = midi_out_pump(&o, sink_rec, &r, 64u);
    RI_ASSERT(used == 4u, "consumed 4 bytes (FA + 3 clocks), got %u",
        (unsigned)used);
    RI_ASSERT(r.n == 4u, "4 messages, got %u", (unsigned)r.n);
    RI_ASSERT(r.len[0] == 1u && r.msg[0][0] == 0xFAu, "message 0 is FA");
    for (i = 1u; i < 4u; i++)
        RI_ASSERT(r.len[i] == 1u && r.msg[i][0] == 0xF8u,
            "message %u is one F8 (%u bytes)", (unsigned)i,
            (unsigned)r.len[i]);

    /* --- an SPP is ONE three-byte message ------------------------------ */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_locate(&o, 640u);           /* 640 = lsb 0x00, msb 0x05 */
    memset(&r, 0, sizeof r);
    r.fail_at = -1;
    used = midi_out_pump(&o, sink_rec, &r, 64u);
    RI_ASSERT(used == 3u, "SPP is 3 bytes, got %u", (unsigned)used);
    RI_ASSERT(r.n == 1u, "SPP is ONE message, got %u", (unsigned)r.n);
    RI_ASSERT(r.len[0] == 3u && r.msg[0][0] == 0xF2u &&
        r.msg[0][1] == 0x00u && r.msg[0][2] == 0x05u,
        "SPP 640 = F2 00 05 in one message");

    /* --- a partial tail stays buffered, it is not sent as a fragment --- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_locate(&o, 640u);
    memset(&r, 0, sizeof r);
    r.fail_at = -1;
    /* Budget of 2 takes F2 and one data byte: nothing is emitted, and the
     * bytes are still consumed, so the next call sees the third byte and
     * frames the whole message. */
    used = midi_out_pump(&o, sink_rec, &r, 2u);
    RI_ASSERT(r.n == 0u, "no fragment went out (%u messages)", (unsigned)r.n);
    RI_ASSERT(used == 2u, "but the bytes were taken (%u)", (unsigned)used);
    /* The third byte completes the 3-byte message, so it goes out whole
     * rather than waiting for a status byte that may never come. */
    used = midi_out_pump(&o, sink_rec, &r, 64u);
    RI_ASSERT(r.n == 1u, "the third byte completes it (%u)", (unsigned)r.n);
    RI_ASSERT(r.len[0] == 3u && r.msg[0][0] == 0xF2u && r.msg[0][2] == 0x05u,
        "and it is the whole message (%u bytes)", (unsigned)r.len[0]);

    /* --- a budget really does cap the work ---------------------------- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    midi_out_render(&o, 100000u);        /* 100 clocks */
    memset(&r, 0, sizeof r);
    r.fail_at = -1;
    used = midi_out_pump(&o, sink_rec, &r, 10u);
    RI_ASSERT(used == 10u, "the budget is honoured (%u)", (unsigned)used);
    RI_ASSERT(r.n == 10u, "10 one-byte messages (%u)", (unsigned)r.n);
    RI_ASSERT(midi_out_pending(&o) == 91u, "91 bytes left (%u)",
        (unsigned)midi_out_pending(&o));

    /* --- a failing sink stops the pump -------------------------------- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    midi_out_render(&o, 10000u);         /* 10 clocks */
    memset(&r, 0, sizeof r);
    r.fail_at = 3;                        /* the 4th message is refused */
    used = midi_out_pump(&o, sink_rec, &r, 64u);
    RI_ASSERT(r.n == 3u, "stopped at the failing message (%u)", (unsigned)r.n);
    /* The refused message's own byte HAS been consumed -- it was stale once
     * offered -- but nothing after it is taken. 11 bytes in, 4 pulled. */
    RI_ASSERT(midi_out_pending(&o) == 7u,
        "the refused message and the rest are still in the ring (%u)",
        (unsigned)midi_out_pending(&o));

    /* --- nulls are inert ---------------------------------------------- */
    RI_ASSERT(midi_out_pump(0, sink_rec, &r, 8u) == 0u, "null producer");
    RI_ASSERT(midi_out_pump(&o, 0, &r, 8u) == 0u, "null sink");
    RI_ASSERT(midi_out_pending(&o) == 7u, "and a null sink consumes nothing");

    RI_RESULT("midi-out-pump");
}
