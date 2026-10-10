/* t191_mmc_out — MMC out (R3's "and add sending").
 *
 * The law that matters is the ROUND TRIP. MMC in reads LOCATE's position
 * as 16-bit big-endian MIDI beats = sixteenths (midi_mmc.h, M3b's lesson:
 * the intent speaks the wire's own unit). If MMC out wrote the same field
 * in any other unit -- bars, ticks, milliseconds -- then a locate sent to a
 * slave and read back would land somewhere else, and nothing on the wire
 * would say so. M3b's SPP bug was exactly a wrong unit in this field.
 *
 * So the test sends a locate, feeds the bytes straight back through the IN
 * path, and requires the sixteenths to survive unchanged.
 */
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_mmc.h"

int main(void) {
    static struct RIMidiMmcOut o;
    struct RIMidiMmc in;
    uint8_t buf[16];
    uint32_t n, cap;

    /* --- off by default: not one byte reaches the buffer -------------- */
    midi_mmc_out_init(&o, 0);
    n = midi_mmc_out_play(&o, buf, sizeof buf);
    RI_ASSERT(n == 0u, "disabled play emits nothing (%u)", (unsigned)n);
    n = midi_mmc_out_stop(&o, buf, sizeof buf);
    RI_ASSERT(n == 0u, "disabled stop emits nothing (%u)", (unsigned)n);
    n = midi_mmc_out_locate(&o, buf, sizeof buf, 1234u);
    RI_ASSERT(n == 0u, "disabled locate emits nothing (%u)", (unsigned)n);

    /* --- enabled: the canonical MMC transport frames ------------------ */
    midi_mmc_out_init(&o, 1);
    n = midi_mmc_out_play(&o, buf, sizeof buf);
    RI_ASSERT(n == 6u, "play is 6 bytes (%u)", (unsigned)n);
    RI_ASSERT(buf[0] == 0xF0u && buf[1] == 0x7Eu && buf[2] == 0x7Fu &&
        buf[3] == 0x06u && buf[4] == 0x01u && buf[5] == 0xF7u,
        "play frame F0 7E 7F 06 01 F7 (%02X %02X %02X %02X %02X %02X)",
        buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);

    n = midi_mmc_out_stop(&o, buf, sizeof buf);
    RI_ASSERT(n == 6u && buf[4] == 0x02u, "stop frame is 06 02 (%u, %02X)",
        (unsigned)n, buf[4]);

    /* --- locate carries the SAME unit the reader expects -------------- */
    n = midi_mmc_out_locate(&o, buf, sizeof buf, 1234u);
    RI_ASSERT(n == 10u, "locate is 10 bytes (%u)", (unsigned)n);
    RI_ASSERT(buf[3] == 0x06u && buf[4] == 0x04u, "locate is 06 04");
    /* 1234 = 0x04D2, big endian in the hh:mm field. */
    RI_ASSERT(buf[5] == 0x04u && buf[6] == 0xD2u,
        "1234 as big-endian beats, not little (%02X %02X)", buf[5], buf[6]);

    /* THE ROUND TRIP. Out then in, same unit, unchanged value. */
    midi_mmc_init(&in, 1);
    {
        uint32_t i;
        for (i = 0u; i < n; i++)
            midi_mmc_feed(&in, buf[i]);
    }
    RI_ASSERT(midi_mmc_take(&in) == RI_FOLLOW_SEEK, "the loopback reads as a seek");
    RI_ASSERT(in.sixteenths == 1234u,
        "1234 survives the round trip in the unit the reader uses (%u)",
        (unsigned)in.sixteenths);

    /* A value whose bytes DIFFER under either endianness, so a mutant that
     * swaps them cannot pass by luck on a palindromic case. */
    n = midi_mmc_out_locate(&o, buf, sizeof buf, 0x1234u);
    midi_mmc_init(&in, 1);
    {
        uint32_t i;
        for (i = 0u; i < n; i++)
            midi_mmc_feed(&in, buf[i]);
    }
    (void)midi_mmc_take(&in);
    RI_ASSERT(in.sixteenths == 0x1234u,
        "0x1234 round trips (%04X)", (unsigned)in.sixteenths);

    /* --- a position beyond 16 bits is CLAMPED, never truncated -------- */
    /* The wire field is 16 bits. Truncating 0x12345 to 0x2345 would send a
     * slave to a different place with no error anywhere; clamping says so
     * by refusing to go past the end. */
    n = midi_mmc_out_locate(&o, buf, sizeof buf, 0x12345u);
    RI_ASSERT(n == 10u, "an over-range locate still emits a frame (%u)",
        (unsigned)n);
    RI_ASSERT(buf[5] == 0xFFu && buf[6] == 0xFFu,
        "over-range clamps to the end of the field (%02X %02X)",
        buf[5], buf[6]);

    /* --- a buffer too small emits NOTHING, not a truncated frame ------ */
    /* EVERY short size, not just one. The first version of this case only
     * tried cap=3, and a mutant that loosened the guard from "6" to "4"
     * survived it -- while a 4- or 5-byte buffer would then have had SIX
     * bytes written into it. The boundary is the law; one probe below it
     * is not. */
    for (cap = 0u; cap < 6u; cap++) {
        n = midi_mmc_out_play(&o, buf, cap);
        RI_ASSERT(n == 0u, "a %u-byte buffer yields no play frame (%u)",
            (unsigned)cap, (unsigned)n);
    }
    for (cap = 0u; cap < 10u; cap++) {
        n = midi_mmc_out_locate(&o, buf, cap, 16u);
        RI_ASSERT(n == 0u, "a %u-byte buffer yields no locate frame (%u)",
            (unsigned)cap, (unsigned)n);
    }
    /* ...and exactly-fits is still allowed, or the guard would be
     * over-tight by one and a caller with a right-sized buffer refused. */
    n = midi_mmc_out_play(&o, buf, 6u);
    RI_ASSERT(n == 6u, "exactly six bytes is enough (%u)", (unsigned)n);
    n = midi_mmc_out_locate(&o, buf, 10u, 16u);
    RI_ASSERT(n == 10u, "exactly ten bytes is enough (%u)", (unsigned)n);

    /* --- the counters are the ev-log's, so they must move ------------ */
    midi_mmc_out_init(&o, 1);
    RI_ASSERT(midi_mmc_out_sent(&o) == 0u, "nothing sent yet");
    RI_ASSERT(midi_mmc_out_refused(&o) == 0u, "nothing refused yet");
    (void)midi_mmc_out_play(&o, buf, 6u);
    (void)midi_mmc_out_stop(&o, buf, 6u);
    (void)midi_mmc_out_locate(&o, buf, 10u, 32u);
    RI_ASSERT(midi_mmc_out_sent(&o) == 3u, "three frames sent (%u)",
        (unsigned)midi_mmc_out_sent(&o));
    /* A short buffer and a disabled call are DIFFERENT events: one is a
     * caller's mistake, the other is the feature being off. They are
     * counted apart so the log can tell them apart. */
    (void)midi_mmc_out_play(&o, buf, 2u);
    RI_ASSERT(midi_mmc_out_refused(&o) == 1u, "a short buffer is refused (%u)",
        (unsigned)midi_mmc_out_refused(&o));
    midi_mmc_out_init(&o, 0);
    (void)midi_mmc_out_play(&o, buf, 6u);
    RI_ASSERT(midi_mmc_out_sent(&o) == 0u, "disabled sends nothing");
    RI_ASSERT(midi_mmc_out_refused(&o) == 1u, "a disabled call is refused too");
    midi_mmc_out_init(&o, 1);

    /* --- nulls are inert ----------------------------------------------- */
    RI_ASSERT(midi_mmc_out_play(0, buf, sizeof buf) == 0u, "null sink");
    RI_ASSERT(midi_mmc_out_stop(0, buf, sizeof buf) == 0u, "null stop");
    RI_ASSERT(midi_mmc_out_locate(0, buf, sizeof buf, 1u) == 0u, "null locate");
    RI_ASSERT(midi_mmc_out_play(&o, 0, 16u) == 0u, "null buffer");
    midi_mmc_out_init(0, 1);   /* must not fault */

    /* --- one enable, one authority: init clears the E0 ---------------- */
    midi_mmc_out_init(&o, 1);
    RI_ASSERT(midi_mmc_out_enabled(&o) == 1u, "enabled after init(1)");
    midi_mmc_out_init(&o, 0);
    RI_ASSERT(midi_mmc_out_enabled(&o) == 0u, "disabled after init(0)");

    RI_RESULT("mmc-out");
}