#include <stdint.h>
#include <stdio.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/clock.h"
#include "engine/seq/sched.h"

/* 120 BPM: quarter = 0.5 s = 500,000,000 ns. 140 BPM: 60/140 s, rounded. */
static const struct RISegment seg120[] = { { 0ULL, 500000000ULL } };
static const struct RITempoMap map120 = { seg120, 1U, 96U, 48000U };

/* 120 -> 140 at tick 384 (one 4/4 bar at PPQ=96). */
static const struct RISegment seg120_140[] = {
    { 0ULL, 500000000ULL },
    { 384ULL, 428571429ULL }
};
static const struct RITempoMap map120_140 = { seg120_140, 2U, 96U, 48000U };

int main(void) {
    /* 1. single-segment exactness: 96 ticks @120 BPM/48k = exactly 24000. */
    RI_ASSERT(ri_map_tick(&map120, 0ULL) == 0ULL, "tick0 %llu",
              (unsigned long long)ri_map_tick(&map120, 0ULL));
    RI_ASSERT(ri_map_tick(&map120, 96ULL) == 24000ULL, "tick96 %llu",
              (unsigned long long)ri_map_tick(&map120, 96ULL));
    RI_ASSERT(ri_map_tick(&map120, 192ULL) == 48000ULL, "tick192 %llu",
              (unsigned long long)ri_map_tick(&map120, 192ULL));
    RI_ASSERT(ri_map_tick_floor(&map120, 96ULL) == 24000ULL, "floor tick96 %llu",
              (unsigned long long)ri_map_tick_floor(&map120, 96ULL));

    /* 2. multi-segment crossing + boundary ownership. */
    {
        uint64_t s383 = ri_map_tick(&map120_140, 383ULL);
        uint64_t s384 = ri_map_tick(&map120_140, 384ULL);
        uint64_t s385 = ri_map_tick(&map120_140, 385ULL);
        /* Ticks below the boundary see only the old segment. */
        RI_ASSERT(s383 == ri_map_tick(&map120, 383ULL), "s383 %llu vs %llu",
                  (unsigned long long)s383,
                  (unsigned long long)ri_map_tick(&map120, 383ULL));
        /* Boundary tick itself is continuous (new segment owns [384, ...)). */
        RI_ASSERT(s384 == ri_map_tick(&map120, 384ULL), "s384 %llu vs %llu",
                  (unsigned long long)s384,
                  (unsigned long long)ri_map_tick(&map120, 384ULL));
        /* The first post-boundary tick advances at the NEW rate:
         * round(428571429*48000/(96*1e9)) = round(214.2857) = 214,
         * while the old rate would give 250. */
        RI_ASSERT(s385 - s384 == 214ULL, "new-rate step %llu",
                  (unsigned long long)(s385 - s384));
        /* 96 ticks into the new segment: round(20571.428592) = 20571. */
        RI_ASSERT(ri_map_tick(&map120_140, 480ULL) - s384 == 20571ULL,
                  "new-rate bar %llu",
                  (unsigned long long)(ri_map_tick(&map120_140, 480ULL) - s384));
        RI_ASSERT(ri_map_tick_floor(&map120_140, 385ULL) - s384 == 214ULL,
                  "floor new-rate step %llu",
                  (unsigned long long)(ri_map_tick_floor(&map120_140, 385ULL) - s384));
    }

    /* 3. Round-Floor relation: difference in {0,1} over 100k ticks, both maps. */
    {
        uint64_t bad = 0ULL;
        uint64_t t;
        for (t = 0ULL; t < 100000ULL; t++) {
            uint64_t r = ri_map_tick(&map120_140, t);
            uint64_t f = ri_map_tick_floor(&map120_140, t);
            if (!(r >= f && r - f <= 1ULL))
                bad++;
            r = ri_map_tick(&map120, t);
            f = ri_map_tick_floor(&map120, t);
            if (!(r >= f && r - f <= 1ULL))
                bad++;
        }
        RI_ASSERT(bad == 0ULL, "round-floor violations %llu", (unsigned long long)bad);
    }

    /* 4. 24 h monotonic sweep: 48000*86400 ticks in 2^20 steps. */
    {
        const uint64_t end = 48000ULL * 86400ULL;
        const uint64_t steps = 1ULL << 20;
        uint64_t prev = ri_map_tick(&map120, 0ULL);
        uint64_t bad = 0ULL;
        uint64_t i;
        for (i = 1ULL; i <= steps; i++) {
            uint64_t t = (i * end) / steps;
            uint64_t cur = ri_map_tick(&map120, t);
            if (cur < prev)
                bad++;
            prev = cur;
        }
        RI_ASSERT(bad == 0ULL, "monotonic violations %llu", (unsigned long long)bad);
        RI_ASSERT(prev == ri_map_tick(&map120, end), "sweep end %llu vs %llu",
                  (unsigned long long)prev,
                  (unsigned long long)ri_map_tick(&map120, end));
    }

    /* 5. same-sample ordering NOTE_OFF < NOTE_ON < ACCENT (+ tiebreaks). */
    {
        struct RIEvent off = { 1000ULL, RI_EV_NOTE_OFF, 2U, 0U, 60U, 0U, 0U };
        struct RIEvent on = { 1000ULL, RI_EV_NOTE_ON, 2U, 0U, 60U, 0U, 1U };
        struct RIEvent acc = { 1000ULL, RI_EV_ACCENT, 2U, 0U, 1U, 0U, 2U };
        struct RIEvent off_late = { 1001ULL, RI_EV_NOTE_OFF, 2U, 0U, 60U, 0U, 0U };
        struct RIEvent on_dev0 = { 1000ULL, RI_EV_NOTE_ON, 0U, 0U, 60U, 0U, 0U };
        struct RIEvent on_dev1 = { 1000ULL, RI_EV_NOTE_ON, 1U, 0U, 60U, 0U, 0U };
        RI_ASSERT(RI_EV_TRANSPORT == 0U && RI_EV_NOTE_OFF == 2U &&
                  RI_EV_NOTE_ON == 3U && RI_EV_ACCENT == 5U &&
                  RI_EV_METER == 9U, "event type ids");
        RI_ASSERT(ri_event_less(&off, &on) && !ri_event_less(&on, &off),
                  "off<on");
        RI_ASSERT(ri_event_less(&on, &acc) && !ri_event_less(&acc, &on),
                  "on<acc");
        RI_ASSERT(ri_event_less(&off, &acc) && !ri_event_less(&acc, &off),
                  "off<acc transitive");
        RI_ASSERT(ri_event_less(&on, &off_late) && !ri_event_less(&off_late, &on),
                  "sample orders first");
        RI_ASSERT(ri_event_less(&on_dev0, &on_dev1) &&
                  !ri_event_less(&on_dev1, &on_dev0), "device tiebreak");
        RI_ASSERT(!ri_event_less(&off, &off), "irreflexive");
    }

    RI_RESULT("clock");
}
