/* midi_io/midi_out.c — outbound MIDI bytes (M5b). See midi_out.h.
 *
 * The ring is single-producer (the render) and single-consumer (the
 * sender task), so head/tail need no atomics here: the PAL atomics live
 * at the boundary that actually crosses tasks, which is the sender side.
 * Overflow is oldest-dropped-and-counted, the P-19 policy the inbound
 * bridge already uses — a clock byte that arrives late is worse than one
 * that never arrives.
 */
#include <string.h>

#include "midi_io/midi_out.h"

#define MIDI_F8   0xF8u
#define MIDI_FA   0xFAu
#define MIDI_FB   0xFBu
#define MIDI_FC   0xFCu
#define MIDI_F2   0xF2u

/* head and tail are MONOTONIC counters, not masked indices. Two earlier
 * forms of this ring were wrong and the test found both:
 *
 *  - masked indices make `head - tail` mean two different things -- empty
 *    and full are the SAME value -- so a full ring silently overwrote
 *    instead of dropping, and `dropped` stayed 0 while the oldest bytes
 *    were being eaten;
 *  - subtracting masked indices without masking the result reads
 *    4294967295 the moment the reader overtakes the writer.
 *
 * Monotonic counters make occupancy exactly `head - tail` with no
 * ambiguity, at the cost of one reserved slot: the ring holds
 * RI_MIDIOUT_CAP - 1 bytes. CAP must stay a power of two because the
 * INDEX is still masked to reach the array. */
static uint32_t room_of(const struct RIMidiOut *o) {
    return RI_MIDIOUT_CAP - 1u - (o->head - o->tail);
}

/* One byte, dropping the oldest if the ring is full. */
static void put(struct RIMidiOut *o, uint8_t b) {
    if (!o || !o->enabled)
        return;
    if (room_of(o) == 0u) {
        o->tail++;
        o->dropped++;
    }
    o->buf[o->head & (RI_MIDIOUT_CAP - 1u)] = b;
    o->head++;
}

/* n bytes as ONE unit: either all of them land or none do. A half-sent
 * 3-byte SPP is a wire-level lie — the receiver would read a position
 * made of a stale byte and a fresh one. */
static void put_run(struct RIMidiOut *o, const uint8_t *b, uint32_t n) {
    uint32_t room;
    if (!o || !o->enabled || !b || n == 0u)
        return;
    room = room_of(o);
    if (room < n) {
        /* Make room by dropping from the tail, then re-check: if it still
         * does not fit, send nothing at all. */
        uint32_t need = n - room;
        if (need >= RI_MIDIOUT_CAP - 1u)
            return;               /* a message longer than the ring */
        o->tail += need;
        o->dropped += need;
        room = room_of(o);
        if (room < n)
            return;
    }
    {
        uint32_t i;
        for (i = 0u; i < n; i++) {
            o->buf[o->head & (RI_MIDIOUT_CAP - 1u)] = b[i];
            o->head++;
        }
    }
}

void midi_out_init(struct RIMidiOut *o, uint32_t sr, uint32_t bpm_milli,
    uint32_t lead_smp) {
    if (!o)
        return;
    memset(o, 0, sizeof *o);
    midi_clockout_init(&o->clk, sr, bpm_milli, RI_MIDICLK_PPQ, lead_smp);
    o->enabled = 0u;      /* OFF BY DEFAULT */
    o->running = 0u;
}

void midi_out_enable(struct RIMidiOut *o, int on) {
    if (!o)
        return;
    o->enabled = on ? 1u : 0u;
    if (!o->enabled)
        o->running = 0u;  /* disabling is a stop, with no FC on the wire */
}

void midi_out_start(struct RIMidiOut *o, uint64_t sample_pos) {
    if (!o || !o->enabled)
        return;
    /* Already running: this is a Continue, and MIDI 1.0 spells the
     * difference FB vs FA. Re-anchoring here would restart the count. */
    if (o->running) {
        put(o, MIDI_FB);
        return;
    }
    midi_clockout_start(&o->clk, sample_pos);
    o->last_tick = 0u;      /* a take counts from zero */
    o->running = 1u;
    put(o, MIDI_FA);
}

void midi_out_stop(struct RIMidiOut *o) {
    if (!o || !o->enabled)
        return;
    midi_clockout_stop(&o->clk);
    o->running = 0u;
    put(o, MIDI_FC);
}

void midi_out_locate(struct RIMidiOut *o, uint32_t sixteenths) {
    uint8_t spp[3];
    if (!o || !o->enabled)
        return;
    if (o->running) {
        o->spp_ignored++;     /* the follower's inbound law, mirrored */
        return;
    }
    if (sixteenths > 16383u)
        return;               /* refuse, never wrap: a wrapped SPP seeks
                               * somewhere nobody asked for */
    spp[0] = MIDI_F2;
    spp[1] = (uint8_t)(sixteenths & 0x7Fu);
    spp[2] = (uint8_t)((sixteenths >> 7) & 0x7Fu);
    put_run(o, spp, 3u);
}

void midi_out_render(struct RIMidiOut *o, uint64_t sample_pos) {
    uint64_t owed;
    if (!o || !o->enabled || !o->running)
        return;
    /* CUMULATIVE, not the difference between this call and the last one:
     * a skipped or coalesced call then emits everything it owed instead
     * of silently dropping that many clock bytes. A clock that quietly
     * loses bytes is a take that quietly drifts.
     */
    owed = midi_clockout_ticks(&o->clk, sample_pos);
    while (o->last_tick < owed) {
        put(o, MIDI_F8);
        o->last_tick++;
    }
}

void midi_out_set_bpm(struct RIMidiOut *o, uint32_t bpm_milli,
    uint64_t sample_pos) {
    if (!o || !o->enabled)
        return;
    midi_clockout_set_bpm(&o->clk, bpm_milli, sample_pos);
}

uint32_t midi_out_read(struct RIMidiOut *o, uint8_t *out, uint32_t cap) {
    uint32_t n = 0u;
    if (!o || !out)
        return 0u;
    while (o->tail != o->head && n < cap) {
        out[n++] = o->buf[o->tail & (RI_MIDIOUT_CAP - 1u)];
        o->tail++;
    }
    return n;
}

uint32_t midi_out_pending(const struct RIMidiOut *o) {
    return o ? o->head - o->tail : 0u;
}
