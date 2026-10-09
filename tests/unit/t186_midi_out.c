/* t186_midi_out — M5b: the outbound producer. Bytes, not CAMD.
 *
 * The plan's rule for clock out is "no CAMD in the render path": the
 * render hands this module the sample position, and this module fills a
 * caller-owned byte ring that a separate sender task drains. So the laws
 * here are about the BYTES and their order, and the confinement gate
 * (no AROS include outside platform/aros) is what keeps camd out.
 *
 * Laws:
 * - OFF BY DEFAULT (E0, pending owner decision 1 on Classic-vs-Power):
 *   an un-enabled producer emits nothing at all, not even a transport
 *   edge. A feature that is off must be silent on the wire, not merely
 *   ignored by the receiver.
 * - Start = FA, Continue (already running) = FB, Stop = FC. Stop really
 *   stops: no clock byte follows it.
 * - 24 ppqn off the render's sample clock (t185 owns the arithmetic; this
 *   pins that the producer emits what the schedule says, once each).
 * - SPP on locate, while STOPPED only — the same law the follower has
 *   for inbound SPP, so the two ends agree about when SPP is legal.
 * - The ring drops oldest and counts, and a 3-byte SPP is all-or-nothing:
 *   a half-sent SPP is a wire-level lie, not a dropped packet.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_out.h"

#define SR 48000u

/* Drain everything, reporting the byte count. */
static uint32_t drain(struct RIMidiOut *o, uint8_t *buf, uint32_t cap) {
    return midi_out_read(o, buf, cap);
}

static uint32_t count_byte(const uint8_t *b, uint32_t n, uint8_t v) {
    uint32_t i, k = 0u;
    for (i = 0u; i < n; i++)
        if (b[i] == v)
            k++;
    return k;
}

int main(void) {
    static struct RIMidiOut o;
    static uint8_t buf[4096];
    uint32_t n, i, f8;

    /* --- OFF BY DEFAULT: not one byte on the wire ---------------------- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_start(&o, 0u);
    midi_out_render(&o, 100000u);
    midi_out_stop(&o);
    midi_out_locate(&o, 160u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 0u,
        "a disabled producer emits nothing (%u bytes)",
        drain(&o, buf, sizeof buf));
    RI_ASSERT(o.dropped == 0u, "nothing dropped either");

    /* --- the transport bytes, in order --------------------------------- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 1u && buf[0] == 0xFAu, "start is FA alone (%u/%02x)",
        (unsigned)n, n ? buf[0] : 0u);

    /* Continuing while already running is FB, not a second FA. */
    midi_out_start(&o, 1000u);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 1u && buf[0] == 0xFBu, "continue is FB (%02x)",
        n ? buf[0] : 0u);

    /* --- clocks: 24 ppqn, each exactly once ---------------------------- */
    midi_out_render(&o, 96000u);          /* 96 ticks at 1000 samples/tick */
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 96u, "96 ticks in 96000 samples (%u)", (unsigned)n);
    f8 = count_byte(buf, n, 0xF8u);
    RI_ASSERT(f8 == n, "every clock byte is F8 (%u of %u)",
        (unsigned)f8, (unsigned)n);

    /* Re-rendering the same span emits nothing: a tick is emitted once. */
    midi_out_render(&o, 96000u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 0u,
        "the same position twice does not re-send");
    /* Moving forward by exactly one tick emits exactly one more. */
    midi_out_render(&o, 97000u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 1u, "one more sample-tick");

    /* --- Stop really stops --------------------------------------------- */
    midi_out_stop(&o);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 1u && buf[0] == 0xFCu, "stop is FC (%02x)",
        n ? buf[0] : 0u);
    midi_out_render(&o, 500000u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 0u, "no clock after stop");
    RI_ASSERT(midi_out_pending(&o) == 0u, "ring empty after stop");

    /* --- turning it back OFF silences the wire ------------------------ */
    /* The setting is read at runtime, not only at startup, so
     * enable(o, 0) has to actually stop output. `init` leaving it off is a
     * different law, and pinning only that one left a mutant that ignored
     * the argument completely alive. */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 1u, "enabled emits (%u)", (unsigned)n);
    midi_out_enable(&o, 0);
    midi_out_start(&o, 0u);
    midi_out_render(&o, 100000u);
    midi_out_stop(&o);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 0u, "enable(0) silences the wire again (%u)", (unsigned)n);

    /* --- SPP on locate, stopped only ----------------------------------- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    /* SPP counts sixteenth notes: 160 = 0xA0 = lsb 0x20, msb 0x01. */
    midi_out_locate(&o, 160u);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 3u && buf[0] == 0xF2u && buf[1] == 0x20u && buf[2] == 0x01u,
        "SPP 160 sixteenths = F2 20 01 (%u bytes: %02x %02x %02x)",
        (unsigned)n, n > 0 ? buf[0] : 0u, n > 1 ? buf[1] : 0u,
        n > 2 ? buf[2] : 0u);
    /* A locate of more than 16383 sixteenths is refused, not wrapped: a
     * wrapped SPP would seek the master somewhere nobody asked for. */
    midi_out_locate(&o, 40000u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 0u, "an out-of-range SPP is refused");

    /* While RUNNING, SPP is counted and ignored — the follower's own E0,
     * so both ends of the wire agree. */
    midi_out_start(&o, 0u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 1u, "start drained");
    midi_out_locate(&o, 64u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 0u, "no SPP while running");
    RI_ASSERT(o.spp_ignored == 1u, "and it was counted (%u)",
        (unsigned)o.spp_ignored);

    /* --- a tempo change keeps the wire in step ------------------------- */
    midi_out_init(&o, SR, 120000u, 0u);
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 1u, "start drained");
    midi_out_render(&o, 10000u);          /* 10 ticks */
    RI_ASSERT(drain(&o, buf, sizeof buf) == 10u, "10 ticks");
    midi_out_set_bpm(&o, 140000u, 10000u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 0u,
        "a tempo change is not a byte on the wire");
    midi_out_render(&o, 58000u);          /* one second later at 140 BPM */
    RI_ASSERT(drain(&o, buf, sizeof buf) == 56u,
        "56 ticks in the second after the change (%u)",
        (unsigned)drain(&o, buf, sizeof buf));

    /* --- overflow drops oldest and counts, and never half-sends SPP ----- */
    midi_out_init(&o, SR, 500000u, 0u);   /* 500 BPM: 200 ticks/s, so
                                           * 100000 samples owe 416 ticks
                                           * against a 256-byte ring */
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    RI_ASSERT(drain(&o, buf, sizeof buf) == 1u, "start drained");
    midi_out_render(&o, 100000u);         /* far more ticks than the ring */
    RI_ASSERT(o.dropped > 0u, "overflow counted (%u)", (unsigned)o.dropped);
    n = midi_out_pending(&o);
    RI_ASSERT(n <= RI_MIDIOUT_CAP, "pending never exceeds the cap (%u)", n);
    n = drain(&o, buf, sizeof buf);
    /* Whatever survived is still only whole clock bytes: the SPP rule is
     * about not emitting a fragment, and F8 is one byte by construction. */
    for (i = 0u; i < n; i++)
        RI_ASSERT(buf[i] == 0xF8u, "survivor byte %u is %02x, not F8",
            (unsigned)i, (unsigned)buf[i]);

    /* --- the ring's precondition, pinned rather than assumed ----------- */
    RI_ASSERT((RI_MIDIOUT_CAP & (RI_MIDIOUT_CAP - 1u)) == 0u,
        "RI_MIDIOUT_CAP must be a power of two (%u)", (unsigned)RI_MIDIOUT_CAP);

    /* --- fail closed ---------------------------------------------------- */
    midi_out_init(&o, 0u, 120000u, 0u);   /* zero sample rate */
    midi_out_enable(&o, 1);
    midi_out_start(&o, 0u);
    midi_out_render(&o, 48000u);
    n = drain(&o, buf, sizeof buf);
    RI_ASSERT(n == 1u, "a zero sample rate still sends the Start (%u)",
        (unsigned)n);
    RI_ASSERT(count_byte(buf, n, 0xF8u) == 0u, "and no clocks");
    RI_ASSERT(midi_out_pending(0) == 0u, "null producer");

    RI_RESULT("midi-out");
}
