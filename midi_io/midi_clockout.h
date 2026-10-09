/* midi_io/midi_clockout.h — clock out (M5, R2): 24 ppqn off the render's
 * sample clock. Pure C, host-tested, no CAMD, no allocation, no float.
 *
 * WHY IT IS NOT AN ACCUMULATOR. A sender task that adds "one tick every N
 * samples" drifts by construction whenever N is not an integer, and 48 kHz
 * at 140 BPM is not an integer (48000/56 = 857.14…). Here the tick count
 * at an absolute sample position is a pure function of that position:
 *
 *   ticks(n) = base + floor(((n - anchor) + lead) * ppq * bpm_milli
 *                            / (sr * 60 * 1000))
 *
 * Nothing accumulates, so there is no residual to carry and no second
 * rounding to disagree with. Long takes cannot drift (t185: exactly 33600
 * ticks in ten minutes at 140 BPM, every interval 857 or 858).
 *
 * THE M3f LAW, ON THE OUTPUT SIDE. `midi_clockout_set_bpm` re-anchors at
 * the change position and keeps the tick count already reached, so ticks
 * already emitted keep the sample positions they were emitted at. Dividing
 * by the new rate without re-anchoring re-times the past — which is what
 * made the M3 owner's ear report that "playback speed suffers".
 *
 * `lead` is the audio output latency in samples: the clock must LEAD the
 * audio by that much, or the master receives the tick after the sound it
 * describes has already left the device. E0, ledgered in
 * docs/evidence/midi/ledger.md.
 *
 * Tempo is milli-BPM (integer) so the render path does no float. 24 ppqn
 * is RI_MIDICLK_PPQ; the app's PPQ is 96, and this module deliberately
 * speaks the wire's own PPQ — midi_io owns no PPQ (M3b's rule).
 */
#ifndef RI_MIDICLKOUT_H
#define RI_MIDICLKOUT_H
#include <stdint.h>

#define RI_MIDICLK_PPQ 24u  /* MIDI 1.0 clock: 24 per quarter note */

struct RIMidiClockOut {
    uint32_t sr;          /* sample rate, Hz (0 = fail closed) */
    uint32_t bpm_milli;   /* tempo, milli-BPM (0 = fail closed) */
    uint32_t ppq;         /* RI_MIDICLK_PPQ; 0 is kept, not defaulted */
    uint32_t lead;        /* output latency to lead by, samples */
    uint64_t anchor;      /* sample position where `base` ticks are done */
    uint64_t base;        /* tick count at `anchor` (the emitted past) */
    uint32_t running;     /* 0 stopped, 1 running */
    uint32_t pad;
};

/* A stopped clock with nothing owed. Nothing here can divide by zero. */
void midi_clockout_init(struct RIMidiClockOut *c, uint32_t sr,
    uint32_t bpm_milli, uint32_t ppq, uint32_t lead_smp);
/* Start (or Continue) at an absolute render sample position: tick 0 is
 * there, and the count restarts from zero. */
void midi_clockout_start(struct RIMidiClockOut *c, uint64_t sample_pos);
void midi_clockout_stop(struct RIMidiClockOut *c);
/* Tempo change at an absolute position. Re-anchors there; the past keeps
 * its tick count and its sample positions (M3f). */
void midi_clockout_set_bpm(struct RIMidiClockOut *c, uint32_t bpm_milli,
    uint64_t sample_pos);
/* Ticks owed at an absolute render sample position, counting from the
 * anchor. 0 when stopped, NULL, or on a zero rate/tempo/ppq. Callers walk
 * this per render block and emit the difference against the last call. */
uint64_t midi_clockout_ticks(const struct RIMidiClockOut *c, uint64_t sample_pos);

#endif
