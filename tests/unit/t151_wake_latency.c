/* t151_wake_latency — the metric that separates "late" from "slow".
 *
 * The Dell 2026-10-02 diagnosis failed because render_us_max was read as if
 * it explained the xruns: it says how long a render took, never how long the
 * render task sat unscheduled after the device asked for a buffer. A task at
 * priority 21 cannot be pre-empted by the GUI at all, so without this metric
 * a GUI repaint cannot be the cause of a late wake — but it could still be
 * the cause of a slow one, and nothing in the build could tell them apart.
 *
 * ri_livedrv_report_wake is the accounting side (portable, pure C99, task
 * side only). The AHI backend stamps the EClock in its interrupt hook and
 * calls it with the priority actually held.
 *
 * Laws:
 *  - wake_us_max rises monotonically and is the slowest sample, not the sum;
 *  - wake_us_sum_ms accumulates and wake_n counts, so the two together give
 *    a mean (the heartbeat logs wake_total for exactly this);
 *  - prio_now records the priority at the wake, including the negative
 *    AU_LIVE_PRI_YIELD (-1), so "late while yielded" is distinguishable from
 *    "late at priority 21" — the difference between the governor's doing
 *    and somebody else's;
 *  - a zero sample is a real sample (an instantly scheduled wake) and must
 *    not be treated as "no sample";
 *  - the accounting is independent of the governor: reporting wakes neither
 *    arms nor trips it, and a trip does not corrupt the wake numbers;
 *  - init clears every wake field, so a second open cannot inherit counts.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "app/core/live_driver.h"
#include "engine/live.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"

#define SR 48000.0f
#define BPM 120.0f
#define PPQ 96u

static struct RIPatternBank BA;
static struct RISongTrack TR;
static struct RIEvent SCR1[512];
static struct RILiveSession s;

static uint64_t t_fake_us;
static uint32_t t_step = 111u;

static uint64_t fake_now(void) {
    t_fake_us += t_step;
    return t_fake_us;
}

static void fixture(struct RILiveDriver *d) {
    const struct RIPatternBank *b4[5]; /* the session reads a fixed 5 slots */
    uint32_t q;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    for (q = 0u; q < 32u; q++)
        ri_pattern_set_length(&BA.pat[q], 16u);
    for (q = 0u; q < 16u; q++)
        ri_p303_set(&BA.pat[0], q, 6u, (q == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&TR);
    b4[0] = &BA;
    b4[1] = 0;
    b4[2] = 0;
    b4[3] = 0;
    b4[4] = 0;
    ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SCR1, 512u);
    ri_live_set_banks(&s, b4, &TR, 0);
    ri_live_play(&s);
    t_fake_us = 0u;
    ri_livedrv_init(d, &s, 256u, fake_now);
}

#define WMAX(d) ri_atomic_load_acq(&(d)->wake_us_max)
#define WSUM(d) ri_atomic_load_acq(&(d)->wake_us_sum_ms)
#define WN(d)   ri_atomic_load_acq(&(d)->wake_n)
#define WPRIO(d) ri_atomic_load_acq(&(d)->prio_now)

int main(void) {
    struct RILiveDriver d;

    /* Fresh state: no wake field survives init, even from a poisoned
     * struct — the field is new, so init has to zero it explicitly. */
    memset(&d, 0xA5, sizeof d);
    fixture(&d);
    RI_ASSERT(WMAX(&d) == 0u, "fresh wake max 0 (%lu)", (unsigned long)WMAX(&d));
    RI_ASSERT(WSUM(&d) == 0u, "fresh wake total 0 (%lu)", (unsigned long)WSUM(&d));
    RI_ASSERT(WN(&d) == 0u, "fresh wake_n 0");
    RI_ASSERT(WPRIO(&d) == 0u, "fresh prio 0");

    /* One sample: max, total and count all move together. */
    ri_livedrv_report_wake(&d, 400u, 21u);
    RI_ASSERT(WMAX(&d) == 400u, "wake max 400 (%lu)", (unsigned long)WMAX(&d));
    RI_ASSERT(WN(&d) == 1u, "wake_n 1");
    RI_ASSERT(WPRIO(&d) == 21u, "prio 21");
    /* 400 us is still 0 ms, so the millisecond total must not round up. */
    RI_ASSERT(WSUM(&d) == 0u, "400us is 0ms of total (%lu)", (unsigned long)WSUM(&d));

    /* max is the SLOWEST sample, not the sum and not the last one. */
    ri_livedrv_report_wake(&d, 120u, 21u);
    ri_livedrv_report_wake(&d, 9000u, 21u);
    ri_livedrv_report_wake(&d, 50u, 21u);
    RI_ASSERT(WMAX(&d) == 9000u, "max is the slowest (%lu)", (unsigned long)WMAX(&d));
    RI_ASSERT(WN(&d) == 4u, "wake_n 4");
    /* 400+120+9000+50 = 9570 us = 9 ms */
    RI_ASSERT(WSUM(&d) == 9u, "wake total 9ms (%lu)", (unsigned long)WSUM(&d));

    /* Zero is a sample: an instantly scheduled wake counts and is stored
     * as 0, and 1 us then raises the max — so a zero max means "every
     * sample so far was instant", not "nothing was sampled". */
    fixture(&d);
    ri_livedrv_report_wake(&d, 0u, 21u);
    RI_ASSERT(WN(&d) == 1u, "zero sample counted (%lu)", (unsigned long)WN(&d));
    RI_ASSERT(WMAX(&d) == 0u, "zero sample max 0");
    ri_livedrv_report_wake(&d, 1u, 21u);
    RI_ASSERT(WMAX(&d) == 1u, "1us raises a zero max");

    /* The priority is recorded as given, negatives included: -1 is
     * AU_LIVE_PRI_YIELD, the whole reason this field exists. */
    fixture(&d);
    ri_livedrv_report_wake(&d, 6000u, 21u);
    ri_livedrv_report_wake(&d, 6000u, (uint32_t)-1);
    RI_ASSERT(WPRIO(&d) == 0xFFFFFFFFu, "yield priority round-trips (%lu)",
        (unsigned long)WPRIO(&d));
    RI_ASSERT(WMAX(&d) == 6000u, "yield sample in max");
    RI_ASSERT(WN(&d) == 2u, "yield sample counted");

    /* Independent of the governor: a trip must not reset or corrupt the
     * wake accounting, and reporting wakes must not trip anything. */
    {
        float sl[256], sr[256];
        int16_t buf[512];
        uint32_t b;
        struct RILiveDriver gd;
        fixture(&gd);
        ri_livedrv_report_wake(&gd, 200u, 21u);
        t_step = 84000u; /* 84 ms buffers: over budget, well past the cap */
        for (b = 0u; b < 700u; b++)
            ri_livedrv_render(&gd, buf, sl, sr, 256u);
        t_step = 111u;
        RI_ASSERT(ri_livedrv_overloaded(&gd), "a sustained 84 ms load trips");
        RI_ASSERT(WN(&gd) == 1u, "rendering does not fabricate wake samples (%lu)",
            (unsigned long)WN(&gd));
        RI_ASSERT(WMAX(&gd) == 200u, "a trip leaves the wake max alone");
        /* ... and reporting a wake does not un-trip or re-arm it. */
        ri_livedrv_report_wake(&gd, 10u, 21u);
        RI_ASSERT(ri_livedrv_overloaded(&gd), "reporting a wake keeps the state");
        RI_ASSERT(ri_atomic_load_acq(&gd.overloads) == 1u,
            "reporting a wake adds no overload entry (%lu)",
            (unsigned long)ri_atomic_load_acq(&gd.overloads));
    }

    /* A NULL driver is a no-op, not a crash. */
    ri_livedrv_report_wake(NULL, 100u, 21u);

    t_step = 111u;
    RI_RESULT("wake_latency");
}