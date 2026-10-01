/* gui/secttr.h — Transport panel front-panel behaviour (§12.10 G3/G4).
 * Pure C, host-tested. ReBirth 2.0.1 Owner's Manual p. 144-146: Shuffle
 * amount, Sync and MIDI In LEDs, Tempo display (20..500 bpm, arrows),
 * Pattern/Song mode lever, Play / Stop / Rewind / Fast Forward / Record,
 * Bar display (arrows move one bar), Loop on/off, Loop Start and Length.
 * The transport laws are the engine's (engine/seq/transport.h, §12.9a):
 * play/stop/record state, the stop-click sequence, clamped bar seeks and
 * the loop clamp — the panel only routes clicks to them. Pattern mode:
 * Rewind, Fast Forward, Record and the Bar arrows have no function
 * (p. 144, 146). Index = registry index.
 */
#ifndef RI_SECTTR_H
#define RI_SECTTR_H
#include <stdint.h>
#include "engine/seq/transport.h"

#define RI_STR_MODE 0u       /* 0 = Pattern mode, 1 = Song mode */
#define RI_STR_TEMPO 1u
#define RI_STR_SHUFFLE 2u
#define RI_STR_BAR 3u
#define RI_STR_PLAY 4u
#define RI_STR_STOP 5u
#define RI_STR_REW 6u
#define RI_STR_FF 7u
#define RI_STR_RECORD 8u
#define RI_STR_LOOP 9u
#define RI_STR_LOOP_START 10u
#define RI_STR_LOOP_LEN 11u
#define RI_STR_MIDI 12u
#define RI_STR_SYNC 13u      /* 0 off, 1 red (downbeat), 2 green */
#define RI_STR_TAP 14u       /* tap tempo: a button, not a value (P9e) */
#define RI_STR_NCTL 15u

/* Tap tempo (P9e): the estimator keeps this many press intervals and
 * forgets everything older; a pause longer than the gap starts a new
 * measurement rather than dragging a stale interval into the average. */
#define RI_STR_TAP_IV 4u
#define RI_STR_TAP_GAP_MS 2000u

struct RISectTr {
    uint8_t section;         /* RI_SEC_TRANSPORT */
    uint8_t song_mode;
    uint8_t midi_led, sync_led;
    int16_t tempo, shuffle;
    struct RITransport tr;
    struct RILoop loop;      /* start_bar 0-based */
    uint64_t cursor;         /* ticks */
    uint32_t ppq;
    uint32_t song_bars;      /* bar count the seeks/loop clamp against */
    uint32_t tap_iv[RI_STR_TAP_IV]; /* kept tap intervals, ms, oldest first */
    uint32_t tap_last;       /* ms of the last counted tap */
    uint8_t tap_n;           /* how many intervals are kept */
    uint8_t tap_have;        /* a first tap is on record */
};

int ri_str_init(struct RISectTr *s);
int ri_str_press(struct RISectTr *s, uint32_t idx);
/* Tap tempo (P9e): one press at `ms` (0 = no clock, ignored). Keeps up to
 * RI_STR_TAP_IV intervals, averages them and writes the tempo through the
 * display's own clamp. Returns 1 when the tempo moved (the repaint
 * contract), 0 otherwise — including for the first tap of a run, which
 * only records the reference. */
int ri_str_tap(struct RISectTr *s, uint32_t ms);
int ri_str_set_value(struct RISectTr *s, uint32_t idx, int v);
int ri_str_reset(struct RISectTr *s, uint32_t idx);
int ri_str_step(struct RISectTr *s, uint32_t idx, int dir);
int ri_str_value(const struct RISectTr *s, uint32_t idx); /* displays 1-based */
int ri_str_led(const struct RISectTr *s, uint32_t idx, uint32_t which);
/* Keypad 1 / 2 (p. 224): Song Position to the Loop Start / Loop End (the
 * bar after the last loop bar, clamped to the last valid start). Song
 * mode only, like the other position controls. Returns 1 when moved. */
int ri_str_goto_loop(struct RISectTr *s, int end);
/* Live feed (G6a): Song mode, playing: the Song Position follows the
 * audio clock — start tick + 16ths played (ppq/4 ticks each); with the
 * Loop on and the start inside it, the position wraps to the Loop Start
 * (p. 73). One-way display projection; returns 1 when the bar changed. */
int ri_str_follow(struct RISectTr *s, uint64_t start_tick, uint64_t sixteenths);
void ri_str_indicator_set(struct RISectTr *s, uint32_t idx, int v); /* MIDI / Sync feed */
#endif
