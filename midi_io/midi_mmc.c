/* midi_io/midi_mmc.c — MMC in (M5c, R3). See midi_mmc.h.
 *
 * The command bytes are parsed by midi_mmc_cmd() in midi.h, untouched and
 * already failing closed on anything it does not implement — so the
 * accumulator stores the LEADING F0 too, which makes its buffer exactly
 * what midi_mmc_cmd() expects and leaves one truth about the command
 * bytes. This file is only the framing: when is a SysEx complete, and how
 * does a bad frame avoid costing the stream every later command.
 */
#include <string.h>

#include "midi_io/midi.h"
#include "midi_io/midi_mmc.h"

#define SYSEX_START 0xF0u
#define SYSEX_END   0xF7u

void midi_mmc_init(struct RIMidiMmc *m, int on) {
    if (!m)
        return;
    memset(m, 0, sizeof *m);
    m->enabled = on ? 1u : 0u;
}

/* A completed frame: buf[0] = F0 ... buf[n-1] = F7. */
static void finish(struct RIMidiMmc *m, uint32_t n) {
    int r;
    m->in_sysex = 0u;
    m->n = 0u;
    if (!m->enabled)
        return;
    if (m->over) {
        m->over = 0u;            /* a frame, not a byte count */
        return;
    }
    /* LOCATE first: midi_mmc_cmd does not know 06 04, so asking it first
     * would classify a locate as "not a command". 06 04 hh mm ss ff is 10
     * bytes, and hh:mm is 16-bit big-endian MIDI beats -- a MIDI beat IS a
     * sixteenth, the same unit SPP carries (M3b's lesson: the intent speaks
     * the wire's own unit, and a wrong unit here is a seek to the wrong
     * bar). */
    if (n == 10u && m->buf[3] == 0x06u && m->buf[4] == 0x04u) {
        m->sixteenths = ((uint32_t)m->buf[5] << 8) | (uint32_t)m->buf[6];
        m->out = RI_FOLLOW_SEEK;
        return;
    }
    r = midi_mmc_cmd(m->buf, n);
    if (r == 1) {
        m->out = RI_FOLLOW_PLAY_START;   /* play and deferred play */
        return;
    }
    if (r == 0) {
        m->out = RI_FOLLOW_STOP;
        return;
    }
    m->rejected++;       /* a well-formed SysEx we do not act on (pause,
                          * non-MMC manufacturer, anything else) */
}

void midi_mmc_feed(struct RIMidiMmc *m, uint8_t b) {
    if (!m || !m->enabled)
        return;
    if (b == SYSEX_START) {
        /* An F0 inside an F0 RESTARTS the frame. Nesting would let one
         * truncated frame swallow every command after it. */
        m->in_sysex = 1u;
        m->n = 1u;
        m->buf[0] = SYSEX_START;    /* so midi_mmc_cmd sees the whole frame */
        m->over = 0u;
        return;
    }
    if (!m->in_sysex)
        return;                     /* channel voice, realtime, anything else */
    if (m->n >= RI_MMC_SYSEX_MAX) {
        /* Too long to be a transport command: drop it and count it, but stay
         * in the frame until F7 so the terminator is not read as data. */
        /* Counted ONCE per dropped frame, not once per excess byte: the
         * thing worth seeing is "how many frames were thrown away", and a
         * byte count scales with the sender's noise rather than with the
         * damage. */
        if (!m->over) {
            m->over = 1u;
            m->overflow++;
        }
        m->n = (uint8_t)RI_MMC_SYSEX_MAX;
        return;
    }
    m->buf[m->n++] = b;
    if (b == SYSEX_END)
        finish(m, (uint32_t)m->n);
}

uint32_t midi_mmc_take(struct RIMidiMmc *m) {
    uint32_t k;
    if (!m)
        return RI_FOLLOW_NONE;
    k = m->out;
    m->out = RI_FOLLOW_NONE;
    return k;
}

int midi_mmc_in_sysex(const struct RIMidiMmc *m) {
    return (m && m->in_sysex) ? 1 : 0;
}
