/* midi_io/midi_mmc.h — MMC in (M5c, R3), on the follower's intent path.
 * Pure C, host-tested. No CAMD, no allocation, no float.
 *
 * WHY AN ACCUMULATOR. `midi_mmc_cmd()` (midi_io/midi.h) already parses a
 * complete 6-byte transport command and answers play/stop/not-a-command.
 * What it does not have is a stream: SysEx is variable length, and handing
 * a byte-at-a-time CAMD feed to a 6-byte parser yields "not a command"
 * every single time. So the feature here is the framing, and the framing is
 * where the bugs live:
 *
 *  - a truncated or over-long SysEx must RESYNC, not swallow the rest of
 *    the stream (one bad frame would otherwise cost every later command);
 *  - an F0 seen inside an F0 restarts the frame rather than nesting;
 *  - LOCATE's position is 16-bit big-endian MIDI beats = SIXTEENTHS, the
 *    same unit SPP uses. M3b's SPP bug was exactly a wrong unit here.
 *
 * The intents emitted are the FOLLOWER's (RI_MMC_*), so a master sending
 * MIDI clock and a master sending MMC drive one path rather than two.
 *
 * OFF BY DEFAULT (E0, ledgered; Classic extension, pending owner decision
 * 1). Note also that **Ableton Live itself neither sends nor receives MMC**
 * (interop spec §0.9), so an MMC proof against Live proves nothing about
 * the feature -- which is why this is host-pinned and lane-logged rather
 * than ear-proved against a DAW.
 */
#ifndef RI_MIDIMMC_H
#define RI_MIDIMMC_H
#include <stdint.h>
#include "midi_io/midi_follow.h"   /* the follower's RI_FOLLOW_* intents */

#define RI_MMC_SYSEX_MAX 16u   /* a transport command is 6; 16 is generous */

struct RIMidiMmc {
    uint8_t buf[RI_MMC_SYSEX_MAX];
    uint8_t n;             /* bytes accumulated in the current frame */
    uint8_t enabled;       /* 0 until midi_mmc_init(_, 1) */
    uint8_t in_sysex;      /* 1 between F0 and F7 */
    uint8_t over;          /* this frame is already counted as dropped */
    uint32_t rejected;     /* well-formed frames we do not act on */
    uint32_t overflow;     /* frames dropped for length, counted once each */
    uint32_t out;          /* the pending intent, cleared by midi_mmc_take */
    uint32_t sixteenths;   /* the LOCATE/SEEK payload of the last intent */
};

/* `on` is the E0 setting: 0 leaves every byte ignored. */
void midi_mmc_init(struct RIMidiMmc *m, int on);
/* Feed ONE inbound byte (running status, SysEx and realtime interleaved).
 * Safe on NULL. */
void midi_mmc_feed(struct RIMidiMmc *m, uint8_t b);
/* Take the intent produced by the last completed frame, or
 * RI_FOLLOW_NONE. Consuming it clears it, so a caller that forgets is
 * silent rather than repeating a command every poll. */
uint32_t midi_mmc_take(struct RIMidiMmc *m);
int midi_mmc_in_sysex(const struct RIMidiMmc *m);

#endif
