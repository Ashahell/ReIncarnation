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

static uint64_t fake_now(void) {
    t_fake_us += 111u;
    return t_fake_us;
}

static void fixture_session(struct RILiveSession *s) {
    const struct RIPatternBank *b4[4];
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
    RI_RESULT("live_driver");
}
