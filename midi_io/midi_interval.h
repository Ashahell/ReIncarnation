/* midi_io/midi_interval.h — arrival-interval statistics for a MIDI clock
 * wire (M5 proof, R2). Pure C, host-tested. No clock, no CAMD, no AROS.
 *
 * WHY IT NEVER READS A CLOCK. Arrival stamps are handed IN by whoever
 * actually saw the message arrive — in the shipped path, the M2 receiver
 * task in platform/aros/midi_camd.c, which stamps each message with EClock
 * the moment its signal wakes it. That separation is the whole point:
 * MIDIRX read its own clock at the top of a 2 ms poll loop, so it measured
 * how often it LOOKED rather than how often bytes CAME. At 140 BPM, where
 * ticks are ~17.9 ms apart, that quantisation would have been most of the
 * reported spread, and the instrument would have been the thing under test.
 *
 * The statistics are kept deliberately plain — count, extremes, mean,
 * spread, per-slice counts — because their job is to be read by a person
 * deciding whether a take held, not to be a pretty number.
 */
#ifndef RI_MIDIINTERVAL_H
#define RI_MIDIINTERVAL_H
#include <stdint.h>

/* Ten equal slices of the run's window: drift shows up as unequal slices
 * even when the mean and the spread both look fine. */
#define RI_MIDIINT_SLICES 10u

#define RI_MIDIINT_NONE     0u   /* fewer than two arrivals: no interval */
#define RI_MIDIINT_STEADY   1u
#define RI_MIDIINT_JITTERY  2u

struct RIMidiInterval {
    uint64_t first_us;        /* first arrival stamp */
    uint64_t prev_us;         /* the one before the last arrival */
    uint64_t sum_us;          /* summed intervals */
    uint32_t count;           /* arrivals seen */
    uint32_t intervals;       /* count - 1, once there are two */
    uint32_t min_us, max_us;
    uint32_t first_int_us, last_int_us;
    uint32_t backwards;       /* arrivals that did not advance the clock */
    uint32_t window_us;       /* 0 = no slicing */
    uint32_t slice_idx;       /* slice the most recent interval landed in */
    uint32_t slice[RI_MIDIINT_SLICES];
};

/* `window_us` is the whole run's expected length, 0 for no slicing. */
void midi_interval_init(struct RIMidiInterval *s, uint32_t window_us);
/* One ARRIVAL, stamped in microseconds by the caller's own clock. */
void midi_interval_add(struct RIMidiInterval *s, uint64_t arrival_us);

uint32_t midi_interval_count(const struct RIMidiInterval *s);
uint32_t midi_interval_intervals(const struct RIMidiInterval *s);
uint32_t midi_interval_min_us(const struct RIMidiInterval *s);
uint32_t midi_interval_max_us(const struct RIMidiInterval *s);
uint32_t midi_interval_mean_us(const struct RIMidiInterval *s);
/* max - min: the number that says whether the wire kept to the schedule. */
uint32_t midi_interval_jitter_us(const struct RIMidiInterval *s);
uint32_t midi_interval_backwards(const struct RIMidiInterval *s);
uint32_t midi_interval_slice(const struct RIMidiInterval *s, uint32_t idx);
uint32_t midi_interval_slice_total(const struct RIMidiInterval *s);
uint32_t midi_interval_verdict(const struct RIMidiInterval *s);

#endif
