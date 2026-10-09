/* midi_io/midi_clockout.c — the drift-free clock-out schedule (M5, R2).
 * See midi_clockout.h for the law. One division per call, no accumulator,
 * no float, no CAMD.
 */
#include "midi_io/midi_clockout.h"

void midi_clockout_init(struct RIMidiClockOut *c, uint32_t sr,
    uint32_t bpm_milli, uint32_t ppq, uint32_t lead_smp) {
    if (!c)
        return;
    c->sr = sr;
    c->bpm_milli = bpm_milli;
    c->ppq = ppq;   /* 0 stays 0: fail closed, do not hide a caller bug */
    c->lead = lead_smp;
    c->anchor = 0u;
    c->base = 0u;
    c->running = 0u;
    c->pad = 0u;
}

void midi_clockout_start(struct RIMidiClockOut *c, uint64_t sample_pos) {
    if (!c)
        return;
    c->anchor = sample_pos;
    c->base = 0u;        /* a take counts its own ticks from zero */
    c->running = 1u;
}

void midi_clockout_stop(struct RIMidiClockOut *c) {
    if (c)
        c->running = 0u;
}

void midi_clockout_set_bpm(struct RIMidiClockOut *c, uint32_t bpm_milli,
    uint64_t sample_pos) {
    uint64_t reached;
    if (!c)
        return;
    /* The tick count AT the change, under the OLD rate, is what the
     * already-emitted ticks add up to. Reading it with the new rate is
     * the re-timing bug: the past would move. */
    reached = midi_clockout_ticks(c, sample_pos);
    c->bpm_milli = bpm_milli;
    c->anchor = sample_pos;
    c->base = reached;
}

uint64_t midi_clockout_ticks(const struct RIMidiClockOut *c,
    uint64_t sample_pos) {
    uint64_t span, num, den;
    if (!c)
        return 0u;
    if (!c->running || !c->sr || !c->bpm_milli || !c->ppq)
        return c->base;
    /* Before the anchor less the lead: nothing is owed yet. Written to
     * avoid the underflow that `sample_pos - anchor` would produce for a
     * caller asking about a position before the take started. */
    if (sample_pos + (uint64_t)c->lead < c->anchor)
        return c->base;
    span = (sample_pos - c->anchor) + (uint64_t)c->lead;
    num = span * (uint64_t)c->ppq * (uint64_t)c->bpm_milli;
    den = (uint64_t)c->sr * 60000u;      /* sr * 60 s * 1000 (milli-BPM) */
    return c->base + num / den;
}
