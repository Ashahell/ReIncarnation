/* main_headless.c — host CI proof that the app core runs without AROS
 * (portability plan T8, partial: WAV now; panel PNGs after the T2 canvas).
 * Renders a demo fixture through the portable live driver in device-buffer
 * chunks, pulled by the host null audio backend into a WAV.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "app/core/live_driver.h"
#include "engine/live.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "platform/pal/ri_pal_audio.h"

#define SR 48000u
#define CHUNK 256u
/* Backend defaults render 2 s: 2*48000/256 = 375 buffers. */
#define EXPECT_BUFFERS ((2u * (uint32_t)SR) / CHUNK)

static struct RIPatternBank BA, BB, B808, B909;
static struct RISongTrack TR;
static struct RIEvent SCR[512];
static struct RILiveSession SESS;
static struct RILiveDriver DRV;
static float FL[4096], FR[4096];

static void pull(void *user, void *out, uint32_t frames) {
    float *f = (float *)out;
    static int16_t s16[4096 * 2u];
    uint32_t i;
    (void)user;
    if (frames > 4096u)
        frames = 4096u;
    ri_livedrv_render(&DRV, s16, FL, FR, frames);
    for (i = 0u; i < frames * 2u; i++)
        f[i] = (float)s16[i] / 32768.0f;
}

int main(int argc, char **argv) {
    const struct RIPatternBank *b4[4];
    /* Backend defaults: /tmp/ri/null.wav, 2 s (no env needed). */
    const char *wav = "/tmp/ri/null.wav";
    struct ri_audio_cfg c;
    struct ri_audio_info got;
    uint32_t q, rc = 1u;
    (void)argc;
    (void)argv;
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
    for (q = 0u; q < 16u; q++) {
        ri_p303_set(&BA.pat[0], q, 6u, (q == 0u) ? 0u : (uint8_t)RI_STEP_REST);
        ri_pdrum_set(&B808.pat[0], q, (uint8_t)(q % 4u), (uint8_t)(q % 4u == 0u ? 2u : 1u));
    }
    ri_track_init(&TR);
    b4[0] = &BA;
    b4[1] = &BB;
    b4[2] = &B808;
    b4[3] = &B909;
    ri_live_init(&SESS, 96u, 48000.0f, 120.0f, RI_ENGINE_S303A, SCR, 512u);
    ri_live_set_banks(&SESS, b4, &TR, 0);
    ri_livedrv_init(&DRV, &SESS, CHUNK, 0);
    ri_livedrv_request(&DRV, RI_LIVE_CMD_PLAY);
    c.want_rate = SR;
    c.frames = CHUNK;
    c.channels = 2u;
    if (ri_pal_audio_open(&c, pull, 0, &got) != 0) {
        printf("headless: audio open failed\n");
        return 1;
    }
    if (ri_pal_audio_start() != 0) {
        printf("headless: audio start failed\n");
        return 1;
    }
    printf("headless: %s rate=%u frames=%u buffers=%u xruns=%u\n", wav,
        got.rate, got.frames, ri_atomic_load_acq(&DRV.buffers),
        ri_atomic_load_acq(&DRV.xruns));
    if (ri_atomic_load_acq(&DRV.buffers) == EXPECT_BUFFERS &&
        ri_atomic_load_acq(&DRV.xruns) == 0u)
        rc = 0u;
    ri_pal_audio_stop();
    ri_pal_audio_close();
    return (int)rc;
}
