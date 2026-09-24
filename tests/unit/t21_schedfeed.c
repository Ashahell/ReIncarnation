/* t21_schedfeed — converter->walker feed integration (WBS 2.1 builder).
 * Hand-built song (plain/slide/accent/rest/flam+accent) -> ri_song_to_steps
 * -> ri_sched_emit_timed (opts NULL = straight, 120 BPM map, device 0) ->
 * assert count (10) + per-event (sample,type,value,flags) + the §8 sort
 * proof (same-sample triple lands NOTE_ON,ACCENT,FLAM with emit seq
 * 6,8,7). Expectations hand-derived from the walker contract (NOT from
 * running it): map(tick)=tick*250 samples exact; gate/slide/accent/flam
 * rules per sched.c; final NOTE_OFF at pattern end tick 120.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "engine/seq/songsteps.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
#include "project/rbng.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct RISong song;
    struct RIStep steps[8];
    struct RISegment seg;
    struct RITempoMap map;
    struct RIEvent ev[32];
    uint32_t n, i;
    /* D-h gate rule (§12.4): OFFs fall at half-step ticks now (15000/27000),
     * and emit right after their ON (seq follows emission order, NOT sample
     * order — the final insertion sort sets stream order). Types/values/flags
     * are unchanged; only OFF samples + seq permutation move. */
    static const uint64_t k_sample[10] =
        { 0, 6000, 12000, 12000, 12000, 15000, 24000, 24000, 24000, 27000 };
    static const uint32_t k_type[10] =
        { RI_EV_NOTE_ON, RI_EV_NOTE_ON, RI_EV_NOTE_OFF, RI_EV_NOTE_ON,
          RI_EV_ACCENT, RI_EV_NOTE_OFF, RI_EV_NOTE_ON, RI_EV_ACCENT,
          RI_EV_FLAM, RI_EV_NOTE_OFF };
    static const uint16_t k_value[10] = { 45, 47, 47, 48, 1, 48, 52, 1, 0, 52 };
    static const uint16_t k_flags[10] =
        { 0, RI_EVFLAG_SLIDE, 0, RI_EVFLAG_ACCENT, 0, 0,
          RI_EVFLAG_ACCENT, 0, RI_EVFLAG_FLAM2, 0 };
    static const uint32_t k_seq[10] = { 0, 1, 2, 3, 5, 4, 6, 9, 8, 7 };

    memset(&song, 0, sizeof song);
    song.nsteps = 5u;
    song.steps[0].note = 45; song.steps[0].flags = 0;
    song.steps[1].note = 47; song.steps[1].flags = RI_RBNG_SLIDE;
    song.steps[2].note = 48; song.steps[2].flags = RI_RBNG_ACCENT;
    song.steps[3].note = 0;  song.steps[3].flags = RI_RBNG_REST;
    song.steps[4].note = 52;
    song.steps[4].flags = RI_RBNG_FLAM | RI_RBNG_ACCENT;

    CHECK(ri_song_to_steps(&song, steps, 8u) == 5u, "step count");

    seg.start_tick = 0ULL;
    seg.ns_per_quarter = 500000000ULL;
    map.segs = &seg;
    map.n = 1u;
    map.ppq = 96u;
    map.sr = 48000u;

    n = ri_sched_emit_timed(&map, 0ULL, 96u, steps, 5u, 0u, NULL, ev, 32u);
    CHECK(n == 10u, "event count %u want 10", n);
    for (i = 0u; i < n && i < 10u; i++) {
        CHECK(ev[i].sample == k_sample[i], "ev%u sample %llu want %llu", i,
              (unsigned long long)ev[i].sample,
              (unsigned long long)k_sample[i]);
        CHECK(ev[i].type == k_type[i], "ev%u type %u want %u", i,
              ev[i].type, k_type[i]);
        CHECK(ev[i].value == k_value[i], "ev%u value %u want %u", i,
              ev[i].value, k_value[i]);
        CHECK(ev[i].flags == k_flags[i], "ev%u flags %u want %u", i,
              ev[i].flags, k_flags[i]);
        CHECK(ev[i].device == 0u && ev[i].voice == 0u,
              "ev%u dev/voice %u/%u", i, ev[i].device, ev[i].voice);
        CHECK(ev[i].seq == k_seq[i], "ev%u seq %u want %u", i,
              ev[i].seq, k_seq[i]);
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_schedfeed\n");
    return fails != 0;
}
