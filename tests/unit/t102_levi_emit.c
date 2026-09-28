/* t102_levi_emit — Levi chord emission (owner 2026-09-28, v1 slice 2).
 * Per sounding lane: NOTE_ON(device, voice=lane, value=note) at the
 * step tick; NOTE_OFF(value = held pitch) at the next step boundary
 * for lanes that stop, and at the occurrence end for still-sounding
 * lanes (the §8 gate law, drum-shaped loop). Sorted output, fail-closed.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/pattern.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };
static struct RIEvent OB[256];

static uint32_t emit_all(const struct RIPattern *p, uint16_t dev) {
    return ri_sched_emit_pattern(p, dev, &MAP, 0u, 96u, NULL, 0, 0, OB, 256u);
}

static int find_ev(uint32_t n, uint32_t type, uint16_t voice, uint16_t value) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (OB[i].type == type && OB[i].voice == voice && OB[i].value == value)
            return (int)i;
    return -1;
}

int main(void) {
    struct RIPattern p;
    uint32_t n, i;
    int a, b, c, d;
    uint64_t t0, t1, tend;
    ri_pattern_init(&p, RI_PATTERN_KIND_LEVI, 0u);
    ri_pattern_set_length(&p, 16u);
    /* Two-note chord, then a single that drops lane 1. */
    RI_ASSERT(ri_levi_set(&p, 0u, 0u, 60u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&p, 0u, 1u, 64u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&p, 1u, 0u, 62u, 1) == 0, "set");
    RI_ASSERT(ri_pattern_valid(&p) == 0, "valid");
    n = emit_all(&p, 4u);
    RI_ASSERT(n == 5u, "5 events, got %u", n);
    t0 = ri_map_tick(&MAP, 0u);
    t1 = ri_map_tick(&MAP, 24u);
    a = find_ev(n, RI_EV_NOTE_ON, 0u, 60u);
    b = find_ev(n, RI_EV_NOTE_ON, 1u, 64u);
    c = find_ev(n, RI_EV_NOTE_OFF, 1u, 64u);
    d = find_ev(n, RI_EV_NOTE_ON, 0u, 62u);
    RI_ASSERT(a >= 0 && b >= 0 && c >= 0 && d >= 0, "all four");
    if (a < 0 || b < 0 || c < 0 || d < 0)
        return 1;
    RI_ASSERT(OB[(uint32_t)a].sample == t0, "on0 @t0");
    RI_ASSERT(OB[(uint32_t)b].sample == t0, "on1 @t0");
    RI_ASSERT(OB[(uint32_t)c].sample == t1, "off1 @t1");
    RI_ASSERT(OB[(uint32_t)d].sample == t1, "on0b @t1");
    RI_ASSERT(OB[(uint32_t)a].device == 4u, "device");
    {
        /* Lane 0 rings through step 1, off at step 2. */
        int e = find_ev(n, RI_EV_NOTE_OFF, 0u, 62u);
        RI_ASSERT(e >= 0, "off0 @t2");
        if (e >= 0)
            RI_ASSERT(OB[(uint32_t)e].sample == ri_map_tick(&MAP, 48u), "off0 sample");
    }
    for (i = 1u; i < n; i++)
        RI_ASSERT(OB[i].sample >= OB[i - 1u].sample, "sorted %u", i);
    /* Still sounding at the occurrence end -> OFF at the end tick. */
    ri_pattern_init(&p, RI_PATTERN_KIND_LEVI, 0u);
    ri_pattern_set_length(&p, 16u);
    RI_ASSERT(ri_levi_set(&p, 15u, 2u, 67u, 1) == 0, "set end");
    n = emit_all(&p, 4u);
    RI_ASSERT(n == 2u, "on+off, got %u", n);
    tend = ri_map_tick(&MAP, 16u * 24u);
    a = find_ev(n, RI_EV_NOTE_ON, 2u, 67u);
    b = find_ev(n, RI_EV_NOTE_OFF, 2u, 67u);
    RI_ASSERT(a >= 0 && b >= 0, "end pair");
    if (a < 0 || b < 0)
        return 1;
    RI_ASSERT(OB[(uint32_t)b].sample == tend, "off at end");
    /* Silent pattern -> nothing; bad inputs -> nothing. */
    ri_pattern_init(&p, RI_PATTERN_KIND_LEVI, 0u);
    ri_pattern_set_length(&p, 16u);
    RI_ASSERT(emit_all(&p, 4u) == 0u, "silent");
    p.kind = 9u;
    RI_ASSERT(emit_all(&p, 4u) == 0u, "bad kind");
    RI_ASSERT(ri_sched_emit_pattern(0, 4u, &MAP, 0u, 96u, NULL, 0, 0, OB, 256u) == 0u, "null pat");
    RI_ASSERT(ri_sched_emit_pattern(&p, 4u, &MAP, 0u, 96u, NULL, 0, 0, 0, 256u) == 0u, "null out");
    RI_RESULT("leviemit");
}
