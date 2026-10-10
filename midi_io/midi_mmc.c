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

/* ---------------------------------------------------------------------
 * MMC out. See midi_io/midi_mmc.h for why this is a separate struct.
 *
 * THE UNIT IS THE WHO POINT. `midi_mmc_out_locate` writes the position in
 * exactly the unit `finish()` above reads it -- 16-bit big-endian MIDI
 * beats, which are sixteenths, the same unit SPP carries. M3b's SPP bug
 * was a wrong unit in this very field, landing a seek four times too
 * early; a mismatch between the writer and the reader of the same field
 * would be the same class of defect arriving from the other direction, and
 * t191 pins it by sending a locate and reading the bytes straight back.
 * ------------------------------------------------------------------- */
#define MMC_PLAY_LEN  6u
#define MMC_LOC_LEN  10u

void midi_mmc_out_init(struct RIMidiMmcOut *m, int on) {
    if (!m)
        return;
    m->enabled = on ? 1u : 0u;
    m->sent = 0u;
    m->refused = 0u;
}

int midi_mmc_out_enabled(const struct RIMidiMmcOut *m) {
    return (m && m->enabled) ? 1 : 0;
}

/* The 6-byte non-sysex transport frame: F0 7E 7F 06 <cmd> F7. */
static uint32_t emit6(struct RIMidiMmcOut *m, uint8_t *buf, uint32_t cap,
    uint8_t cmd) {
    if (!m || !buf)
        return 0u;
    if (!m->enabled) {
        m->refused++;
        return 0u;
    }
    /* ALL OR NOTHING. A SysEx cut short is not a shorter message, it is a
     * stream the receiver must resynchronise out of -- so a short buffer
     * yields no bytes at all rather than a partial frame. */
    if (cap < MMC_PLAY_LEN) {
        m->refused++;
        return 0u;
    }
    buf[0] = SYSEX_START;
    buf[1] = 0x7Eu;            /* non-realtime */
    buf[2] = 0x7Fu;            /* all devices */
    buf[3] = 0x06u;            /* MMC command */
    buf[4] = cmd;
    buf[5] = SYSEX_END;
    m->sent++;
    return MMC_PLAY_LEN;
}

uint32_t midi_mmc_out_play(struct RIMidiMmcOut *m, uint8_t *buf, uint32_t cap) {
    return emit6(m, buf, cap, 0x01u);
}

uint32_t midi_mmc_out_stop(struct RIMidiMmcOut *m, uint8_t *buf, uint32_t cap) {
    return emit6(m, buf, cap, 0x02u);
}

uint32_t midi_mmc_out_locate(struct RIMidiMmcOut *m, uint8_t *buf, uint32_t cap,
    uint32_t pos) {
    if (!m || !buf)
        return 0u;
    if (!m->enabled) {
        m->refused++;
        return 0u;
    }
    if (cap < MMC_LOC_LEN) {
        m->refused++;
        return 0u;
    }
    /* Clamp, never wrap. The field is 16 bits; 0x12345 sent as 0x2345
     * would put a slave somewhere else entirely and nothing on the wire
     * distinguishes the two. */
    if (pos > 0xFFFFu)
        pos = 0xFFFFu;
    buf[0] = SYSEX_START;
    buf[1] = 0x7Eu;
    buf[2] = 0x7Fu;
    buf[3] = 0x06u;
    buf[4] = 0x04u;            /* LOCATE */
    buf[5] = (uint8_t)((pos >> 8) & 0xFFu);   /* big endian, as read */
    buf[6] = (uint8_t)(pos & 0xFFu);
    buf[7] = 0x00u;            /* frames  */
    buf[8] = 0x00u;            /* seconds */
    buf[9] = SYSEX_END;
    m->sent++;
    return MMC_LOC_LEN;
}

uint32_t midi_mmc_out_sent(const struct RIMidiMmcOut *m) {
    return m ? m->sent : 0u;
}

uint32_t midi_mmc_out_refused(const struct RIMidiMmcOut *m) {
    return m ? m->refused : 0u;
}
