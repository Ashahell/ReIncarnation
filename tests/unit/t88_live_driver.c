/* t88_live_driver — portability T4: portable live driver == offline.
 * Host backend cadence: driver renders the t81 fixture song in device-buffer
 * chunks (64/128/1024) through ri_livedrv_render; the s16 stream must equal
 * the offline float render passed through the same converter (t81 law).
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
#define TOTAL 4800u

static struct RIPatternBank BA, BB, B808, B909;
static struct RISongTrack TR;
static struct RIEvent SCR1[512];

static uint64_t t_fake_us;
static uint32_t t_step = 111u; /* each render reads the clock twice */

static uint64_t fake_now(void) {
    t_fake_us += t_step;
    return t_fake_us;
}

static void fixture_session(struct RILiveSession *s) {
    const struct RIPatternBank *b4[5];
    uint32_t q;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&BB, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&B909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (q = 0u; q < 32u; q++) {
        ri_pattern_set_length(&BA.pat[q], 16u);
        ri_pattern_set_length(&BB.pat[q], 16u);
        ri_pattern_set_length(&B808.pat[q], 16u);
        ri_pattern_set_length(&B909.pat[q], 16u);
    }
    for (q = 0u; q < 16u; q++)
        ri_p303_set(&BA.pat[0], q, 6u, (q == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&TR);
    b4[0] = &BA;
    b4[1] = &BB;
    b4[2] = &B808;
    b4[3] = &B909;
    b4[4] = 0;
    ri_live_init(s, PPQ, SR, BPM, RI_ENGINE_S303A, SCR1, 512u);
    ri_live_set_banks(s, b4, &TR, 0);
}

/* Offline float render, converted with the driver converter. */
static void offline_s16(int16_t *out) {
    struct RILiveSession s;
    static float fl[TOTAL], fr[TOTAL];
    uint32_t done = 0u, i;
    fixture_session(&s);
    ri_live_play(&s);
    while (done < TOTAL) {
        uint32_t got = ri_live_render(&s, fl + done, fr + done, TOTAL - done);
        if (got == 0u)
            break;
        done += got;
    }
    RI_ASSERT(done == TOTAL, "offline total");
    for (i = 0u; i < TOTAL; i++) {
        out[i * 2u] = ri_livedrv_f32_to_s16(fl[i]);
        out[i * 2u + 1u] = ri_livedrv_f32_to_s16(fr[i]);
    }
}

static void driver_s16(uint32_t chunk, int16_t *out) {
    struct RILiveSession s;
    struct RILiveDriver d;
    static float fl[1024], fr[1024];
    static int16_t buf[2048];
    uint32_t done = 0u;
    fixture_session(&s);
    ri_livedrv_init(&d, &s, chunk, fake_now);
    ri_livedrv_request(&d, RI_LIVE_CMD_PLAY);
    while (done < TOTAL) {
        uint32_t want = TOTAL - done;
        if (want > chunk)
            want = chunk;
        ri_livedrv_render(&d, buf, fl, fr, want);
        memcpy(out + done * 2u, buf, (size_t)want * 2u * sizeof(int16_t));
        done += want;
    }
    RI_ASSERT(ri_atomic_load_acq(&d.buffers) == (TOTAL + chunk - 1u) / chunk, "buffers %u",
        ri_atomic_load_acq(&d.buffers));
}

int main(void) {
    static int16_t ref[TOTAL * 2u], got[TOTAL * 2u];
    static int16_t cap[TOTAL * 2u];
    struct RILiveSession s;
    struct RILiveDriver d;
    static float fl[1024], fr[1024];
    static int16_t buf[2048];
    uint32_t i;
    /* Converter law: clamp + round-half-away, no libm. */
    RI_ASSERT(ri_livedrv_f32_to_s16(0.0f) == 0, "conv zero");
    RI_ASSERT(ri_livedrv_f32_to_s16(1.0f) == 32767, "conv full");
    RI_ASSERT(ri_livedrv_f32_to_s16(-1.0f) == -32767, "conv neg");
    RI_ASSERT(ri_livedrv_f32_to_s16(0.5f) == 16384, "conv half");
    RI_ASSERT(ri_livedrv_f32_to_s16(2.0f) == 32767, "conv clip hi");
    RI_ASSERT(ri_livedrv_f32_to_s16(-2.0f) == -32767, "conv clip lo");
    /* Driver == offline at 64/128/1024-frame buffers. */
    offline_s16(ref);
    driver_s16(64u, got);
    RI_ASSERT(memcmp(ref, got, sizeof ref) == 0, "drv64 == offline");
    driver_s16(128u, got);
    RI_ASSERT(memcmp(ref, got, sizeof ref) == 0, "drv128 == offline");
    driver_s16(1024u, got);
    RI_ASSERT(memcmp(ref, got, sizeof ref) == 0, "drv1024 == offline");
    /* Transport request + xrun + capture + timing. */
    t_fake_us = 0u;
    fixture_session(&s);
    ri_livedrv_init(&d, &s, 256u, fake_now);
    d.cap_buf = cap;
    d.cap_max = TOTAL;
    ri_atomic_store_rel(&d.cap_on, 1u);
    ri_livedrv_request(&d, RI_LIVE_CMD_PLAY);
    ri_livedrv_render(&d, buf, fl, fr, 256u);
    RI_ASSERT(ri_atomic_load_acq(&d.cap_pos) == 256u, "cap 256");
    RI_ASSERT(memcmp(cap, buf, 256u * 2u * sizeof(int16_t)) == 0, "cap == out");
    ri_livedrv_report_late(&d, 2u);
    RI_ASSERT(ri_atomic_load_acq(&d.xruns) == 2u, "xruns");
    RI_ASSERT(ri_atomic_load_acq(&d.render_us_max) == 111u, "max %u",
        ri_atomic_load_acq(&d.render_us_max));
    ri_livedrv_request(&d, RI_LIVE_CMD_STOP);
    ri_livedrv_render(&d, buf, fl, fr, 256u);
    for (i = 0u; i < 256u * 2u; i++) {
        if (buf[i] != 0) {
            RI_ASSERT(0, "stop silent @%u", i);
            break;
        }
    }
    /* Load governor: 256 frames at 48 kHz = 5333 us per buffer, so the
     * threshold 850 per mille is 4533 us of render and the cap 1200 is
     * 6400 us. The yield below the UI is for a machine that CANNOT keep up,
     * so only a CONTINUOUS over-budget load may trip it:
     *  - light load never trips;
     *  - peaks never trip. Owner Dell 2026-10-01: playing ran at 585-638
     *    per mille with repaint spikes, and every crossing of 850 put the
     *    render task below the UI for 2 s, where one repaint cost 1-2
     *    buffers (923 xruns against 630 repaints in one window). The load
     *    has to stay over budget for RI_LIVEDRV_ARM_US (2 s) to trip;
     *  - once tripped it holds 2 s of buffers whatever the load, then
     *    probes, and the next trip needs its own 2 s. */
    {
        uint32_t b, first = 0u, held = 0u;
        t_fake_us = 0u;
        t_step = 1000u; /* 1000 us render: 187 per mille */
        fixture_session(&s);
        ri_livedrv_init(&d, &s, 256u, fake_now);
        for (b = 0u; b < 200u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(!ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 0u,
            "light load stays normal");
        /* The Dell first trip exactly: ten capped buffers in a row (48 ms),
         * then back to 562 per mille. A burst is not a sustained load. */
        for (b = 0u; b < 12u; b++) {
            t_step = 84000u;
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        }
        t_step = 3000u; /* 562 per mille: the playing load on the Dell */
        for (b = 0u; b < 2000u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(!ri_livedrv_overloaded(&d), "capped burst trips (%u buffers in)",
            ri_atomic_load_acq(&d.overloads));
        RI_ASSERT(ri_atomic_load_acq(&d.overloads) == 0u, "capped burst entries");
        /* Peaky: one capped buffer every 20 (the repaint spikes). */
        for (b = 0u; b < 4000u; b++) {
            t_step = (b % 20u == 0u) ? 84000u : 3000u;
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        }
        RI_ASSERT(!ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 0u,
            "peaky load never trips");
        /* Continuous but under the threshold: 843 per mille asymptotes
         * below 850 and must never trip. */
        t_step = 4500u;
        for (b = 0u; b < 2000u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(!ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 0u,
            "843 per mille continuous stays normal");
        /* Continuous over budget: 938 per mille. Half a second is not the
         * 2 s arm; the trip lands at 850-crossing (18 buffers) + 2 s of
         * buffers (376), so at 393 of them. */
        t_fake_us = 0u;
        t_step = 1000u;
        fixture_session(&s);
        ri_livedrv_init(&d, &s, 256u, fake_now);
        t_step = 5000u;
        first = 0u;
        for (b = 0u; b < 500u; b++) {
            ri_livedrv_render(&d, buf, fl, fr, 256u);
            if (b == 99u)
                RI_ASSERT(!ri_livedrv_overloaded(&d) &&
                    ri_atomic_load_acq(&d.overloads) == 0u,
                    "half a second of 938 per mille trips");
            if (first == 0u && ri_livedrv_overloaded(&d))
                first = b + 1u;
        }
        RI_ASSERT(first != 0u, "sustained load never trips");
        RI_ASSERT(first > 360u && first < 430u, "trips after %u buffers (2 s past 850)", first);
        RI_ASSERT(ri_atomic_load_acq(&d.overloads) == 1u, "one entry");
        /* Hold and probe from a fresh trip, so the window starts full. */
        t_fake_us = 0u;
        t_step = 1000u;
        fixture_session(&s);
        ri_livedrv_init(&d, &s, 256u, fake_now);
        t_step = 5000u;
        for (b = 0u; b < 500u && !ri_livedrv_overloaded(&d); b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 1u,
            "fresh driver trips once (%u)", ri_atomic_load_acq(&d.overloads));
        t_step = 50u; /* load gone: still held for the full window */
        for (b = 0u; b < 400u && ri_livedrv_overloaded(&d); b++)
            held++, ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(held >= 370u && held <= 377u, "held %u buffers (2 s)", held);
        for (b = 0u; b < 200u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(!ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 1u,
            "probe at light load stays normal");
        t_step = 5000u; /* still heavy after the probe: a fresh 2 s arm */
        for (b = 0u; b < 200u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(!ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 1u,
            "re-trip is not instant");
        for (b = 0u; b < 300u && !ri_livedrv_overloaded(&d); b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 2u,
            "sustained again trips a second time");
        /* Recovery resets the arm: 1.9 s over budget, one light buffer,
         * 1.9 s over budget again never reaches the 2 s the trip wants. */
        t_fake_us = 0u;
        t_step = 1000u;
        fixture_session(&s);
        ri_livedrv_init(&d, &s, 256u, fake_now);
        t_step = 5000u;
        for (b = 0u; b < 360u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        t_step = 1000u; /* one buffer of headroom */
        ri_livedrv_render(&d, buf, fl, fr, 256u);
        t_step = 5000u;
        for (b = 0u; b < 360u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(!ri_livedrv_overloaded(&d) && ri_atomic_load_acq(&d.overloads) == 0u,
            "one light buffer does not reset the arm");
        for (b = 0u; b < 100u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(ri_livedrv_overloaded(&d), "the reset arm still trips after 2 s (%u)", b);
        t_step = 111u;
    }
    /* A single external stall (Dell 2026-10-01: an 84 ms buffer while the
     * GUI repainted a tab page) is not overload: it must not trip. */
    {
        uint32_t b;
        t_fake_us = 0u;
        t_step = 1000u;
        fixture_session(&s);
        ri_livedrv_init(&d, &s, 256u, fake_now);
        for (b = 0u; b < 100u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        t_step = 84000u;
        ri_livedrv_render(&d, buf, fl, fr, 256u);
        t_step = 30000u; /* the stall spans a few renders */
        ri_livedrv_render(&d, buf, fl, fr, 256u);
        ri_livedrv_render(&d, buf, fl, fr, 256u);
        ri_livedrv_render(&d, buf, fl, fr, 256u);
        t_step = 1000u;
        for (b = 0u; b < 50u; b++)
            ri_livedrv_render(&d, buf, fl, fr, 256u);
        RI_ASSERT(ri_atomic_load_acq(&d.overloads) == 0u, "single stall trips (%u)",
            ri_atomic_load_acq(&d.overloads));
        t_step = 111u;
    }
    RI_RESULT("live_driver");
}
