/* t21_songfile — file-level song path (WBS 2.1 builder file leg).
 * RBNG write -> read -> converter -> walker, all pinned: round-trip
 * step equality, 4 converted steps, 6 emitted events with exact
 * (sample,type,value,flags). Writes under /tmp/ri/run/t21 (audit
 * creates it; write/read failures assert with the codec err string,
 * never crash). No production code is expected to change (codec,
 * converter, walker frozen green) — this pins their composition
 * across the file boundary.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "engine/seq/songsteps.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
#include "project/rbng.h"

#define T21F_DIR "/tmp/ri/run/t21"
#define T21F_SONG T21F_DIR "/song.rbng"

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

int main(void) {
    struct RISong w, r;
    struct RIStep steps[8];
    struct RISegment seg;
    struct RITempoMap map;
    struct RIEvent ev[32];
    char err[256];
    uint32_t n, i;
    static const uint64_t k_sample[6] = { 0, 6000, 12000, 12000, 12000, 18000 };
    static const uint32_t k_type[6] =
        { RI_EV_NOTE_ON, RI_EV_NOTE_ON, RI_EV_NOTE_OFF, RI_EV_NOTE_ON,
          RI_EV_ACCENT, RI_EV_NOTE_OFF };
    static const uint16_t k_value[6] = { 45, 47, 47, 48, 1, 48 };
    static const uint16_t k_flags[6] =
        { 0, RI_EVFLAG_SLIDE, 0, RI_EVFLAG_ACCENT, 0, 0 };

    memset(&w, 0, sizeof w);
    w.tempo = 140u;
    w.ppq = 96u;
    w.nsteps = 4u;
    w.steps[0].note = 45; w.steps[0].flags = 0;
    w.steps[1].note = 47; w.steps[1].flags = RI_RBNG_SLIDE;
    w.steps[2].note = 48; w.steps[2].flags = RI_RBNG_ACCENT;
    w.steps[3].note = 0;  w.steps[3].flags = RI_RBNG_REST;

    CHECK(rbng_write_song(T21F_SONG, &w, err, sizeof err) == 0,
          "write: %s", err);
    memset(&r, 0, sizeof r);
    CHECK(rbng_read_song(T21F_SONG, &r, err, sizeof err) == 0,
          "read: %s", err);
    CHECK(r.tempo == 140u && r.ppq == 96u && r.nsteps == 4u, "song hdr");
    for (i = 0u; i < 4u; i++)
        CHECK(r.steps[i].note == w.steps[i].note &&
              r.steps[i].flags == w.steps[i].flags, "step %u", i);

    CHECK(ri_song_to_steps(&r, steps, 8u) == 4u, "step count");

    seg.start_tick = 0ULL;
    seg.ns_per_quarter = 500000000ULL;
    map.segs = &seg;
    map.n = 1u;
    map.ppq = 96u;
    map.sr = 48000u;

    n = ri_sched_emit_timed(&map, 0ULL, 96u, steps, 4u, 0u, NULL, ev, 32u);
    CHECK(n == 6u, "event count %u want 6", n);
    for (i = 0u; i < n && i < 6u; i++) {
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

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_songfile\n");
    return fails != 0;
}
