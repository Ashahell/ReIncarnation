/* t187_midi_mmc — M5c: MMC in, on the same intent path the follower uses.
 *
 * The parse already existed and was unwired (`midi_mmc_cmd`), so what is
 * new here is the part that was missing and is easy to get wrong:
 *
 *  - SysEx is VARIABLE length. F0 ... F7 is one message, and the existing
 *    parser wants the whole 6-byte transport command in one buffer. Feeding
 *    it a byte at a time yields -1 every time, so the accumulator is the
 *    feature.
 *  - A truncated or over-long SysEx must not desynchronise the stream: the
 *    parser fails closed, and the accumulator must return to idle rather
 *    than swallow everything after it.
 *  - MMC 06 04 is LOCATE, and its position is in a different unit from SPP
 *    (16-bit binary of MIDI beats = SIXTEENTHS, big-endian). Getting that
 *    wrong is a locate to the wrong bar, which is the M3b SPP bug exactly.
 *  - Off by default, like clock out.
 *
 * Laws:
 * - Play/Deferred-Play -> the PLAY_START intent; Stop -> STOP; Locate -> a
 *   SEEK intent carrying sixteenths. Anything else -> no intent at all,
 *   including pause, which is a transport state this engine does not have.
 * - The intents are the FOLLOWER's, so MMC and MIDI clock drive one path.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "midi_io/midi_mmc.h"

int main(void) {
    static struct RIMidiMmc mm;
    uint8_t sysex[8];
    uint32_t n, i, intent;

    /* --- off by default ------------------------------------------------ */
    midi_mmc_init(&mm, 0u);
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x06u);
    midi_mmc_feed(&mm, 0x02u);         /* play */
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(mm.out == RI_FOLLOW_NONE, "disabled parser emits no intent");

    /* --- play, one byte at a time (the accumulator is the feature) ----- */
    midi_mmc_init(&mm, 1u);
    sysex[0] = 0xF0u; sysex[1] = 0x7Fu; sysex[2] = 0x7Fu;
    sysex[3] = 0x06u; sysex[4] = 0x02u; sysex[5] = 0xF7u;
    for (i = 0u; i < 6u; i++)
        midi_mmc_feed(&mm, sysex[i]);
    intent = midi_mmc_take(&mm);
    RI_ASSERT(intent == RI_FOLLOW_PLAY_START, "MMC play (%lu)",
        (unsigned long)intent);

    /* Deferred play is the same intent: the engine has no deferred state. */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x06u);
    midi_mmc_feed(&mm, 0x03u);
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_PLAY_START, "deferred play");
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_NONE, "and it is consumed once");

    /* Stop. */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x06u);
    midi_mmc_feed(&mm, 0x01u);
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_STOP, "MMC stop");

    /* --- LOCATE: 06 04 hh mm ss ff, 16-bit BE, in SIXTEENTHS ------------- */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x06u);
    midi_mmc_feed(&mm, 0x04u);
    midi_mmc_feed(&mm, 0x01u);        /* hh = high byte */
    midi_mmc_feed(&mm, 0x00u);        /* mm = low byte  -> 0x0100 = 256 */
    midi_mmc_feed(&mm, 0x00u);        /* ss */
    midi_mmc_feed(&mm, 0x00u);        /* ff */
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_SEEK, "MMC locate is a seek");
    RI_ASSERT(mm.sixteenths == 256u, "located 256 sixteenths (%u)",
        (unsigned)mm.sixteenths);
    /* 40 beats is 640 sixteenths -- the same bar the SPP proof located, so
     * a master sending both must land in the same place. */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x06u);
    midi_mmc_feed(&mm, 0x04u);
    midi_mmc_feed(&mm, 0x02u);        /* hh */
    midi_mmc_feed(&mm, 0x80u);        /* mm -> 0x0280 = 640 */
    midi_mmc_feed(&mm, 0x00u);
    midi_mmc_feed(&mm, 0x00u);
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_SEEK, "locate 640");
    RI_ASSERT(mm.sixteenths == 640u, "40 beats = 640 sixteenths (%u)",
        (unsigned)mm.sixteenths);

    /* --- unknown commands and a broken frame fail closed, and RESYNC ---- */
    /* Pause (09) is a transport state this engine does not implement. */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0x06u);
    midi_mmc_feed(&mm, 0x09u);
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_NONE, "pause is not a command");
    RI_ASSERT(mm.rejected == 1u, "and it was counted (%u)",
        (unsigned)mm.rejected);

    /* A stream that starts mid-SysEx must not swallow the next real one:
     * a stray F0 with no terminator, then a complete play. */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x01u);        /* not a 7F device id */
    midi_mmc_feed(&mm, 0x02u);
    for (i = 0u; i < 6u; i++)
        midi_mmc_feed(&mm, sysex[i]);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_PLAY_START,
        "a bad frame does not eat the next good one");
    RI_ASSERT(mm.overflow == 0u, "and did not overflow the buffer (%u)",
        (unsigned)mm.overflow);

    /* An F0 inside an F0 restarts the frame rather than nesting. */
    midi_mmc_feed(&mm, 0xF0u);
    midi_mmc_feed(&mm, 0x7Fu);
    midi_mmc_feed(&mm, 0xF0u);        /* restart */
    for (i = 1u; i < 6u; i++)
        midi_mmc_feed(&mm, sysex[i]);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_PLAY_START,
        "F0 inside F0 restarts the frame");

    /* --- an over-long SysEx is dropped and counted, not truncated ------- */
    midi_mmc_feed(&mm, 0xF0u);
    for (n = 0u; n < RI_MMC_SYSEX_MAX + 4u; n++)
        midi_mmc_feed(&mm, 0x01u);
    midi_mmc_feed(&mm, 0xF7u);
    RI_ASSERT(mm.overflow == 1u, "an over-long SysEx is counted (%u)",
        (unsigned)mm.overflow);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_NONE,
        "and produces no intent");
    /* ...and the parser is usable again straight afterwards. */
    for (i = 0u; i < 6u; i++)
        midi_mmc_feed(&mm, sysex[i]);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_PLAY_START, "resynced");

    /* --- garbage between frames must not produce an intent -------------- */
    midi_mmc_init(&mm, 1u);
    midi_mmc_feed(&mm, 0x90u);
    midi_mmc_feed(&mm, 0x45u);
    midi_mmc_feed(&mm, 0x64u);
    RI_ASSERT(midi_mmc_take(&mm) == RI_FOLLOW_NONE, "channel voice ignored");
    RI_ASSERT(midi_mmc_in_sysex(&mm) == 0, "and leaves us idle");

    RI_RESULT("midi-mmc");
}
