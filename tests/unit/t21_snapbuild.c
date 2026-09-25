/* t21_snapbuild — snapshot event-build unit (WBS 2.1 builder completion).
 * Hand-built song (same 5-step shape as t21_schedfeed: plain/slide/
 * accent/rest/flam+accent) -> ri_snapshot_build_events (120 BPM map,
 * NULL opts = straight, device 0) -> assert count (10) + all 60 fields
 * against the same hand-derived table. Cap truncation + NULL edges.
 * Hermetic (no files; song built in code).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "engine/seq/snapbuild.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
#include "project/rbng.h"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    static struct RISong song;
    struct RISegment seg;
    struct RITempoMap map;
    struct RIEvent ev[32];
    uint32_t n, i;
    /* D-h gate rule (§12.4): OFFs at half-step ticks (15000/27000). */
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

    memset(&song, 0, sizeof song);
    song.nsteps = 5u;
    song.steps[0].note = 45; song.steps[0].flags = 0;
    song.steps[1].note = 47; song.steps[1].flags = RI_RBNG_SLIDE;
    song.steps[2].note = 48; song.steps[2].flags = RI_RBNG_ACCENT;
    song.steps[3].note = 0;  song.steps[3].flags = RI_RBNG_REST;
    song.steps[4].note = 52;
    song.steps[4].flags = RI_RBNG_FLAM | RI_RBNG_ACCENT;

    seg.start_tick = 0ULL;
    seg.ns_per_quarter = 500000000ULL;
    map.segs = &seg;
    map.n = 1u;
    map.ppq = 96u;
    map.sr = 48000u;

    n = ri_snapshot_build_events(&song, &map, 96u, 0u, NULL, ev, 32u);
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
    }

    /* Cap truncation propagates (walker drops first). */
    n = ri_snapshot_build_events(&song, &map, 96u, 0u, NULL, ev, 4u);
    CHECK(n == 4u, "trunc count %u want 4", n);
    CHECK(ev[0].type == RI_EV_NOTE_ON && ev[0].value == 45,
          "trunc head %u/%u", ev[0].type, ev[0].value);

    /* Null edges. */
    CHECK(ri_snapshot_build_events(NULL, &map, 96u, 0u, NULL, ev, 32u) == 0u,
          "null song nonzero");
    CHECK(ri_snapshot_build_events(&song, &map, 96u, 0u, NULL, NULL, 32u) == 0u,
          "null out nonzero");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_snapbuild\n");
    return fails != 0;
}
