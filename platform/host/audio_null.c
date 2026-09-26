/* audio_null.c — host (CI) backend for ri_pal_audio (portability plan T4).
 * Synchronous and deterministic: start() runs the pull loop on the calling
 * thread for RI_NULL_SECONDS (default 2) and writes a 16-bit stereo WAV to
 * RI_NULL_WAV (default /tmp/ri/null.wav). pull() delivers interleaved
 * float stereo when info.format == RI_FMT_F32. late() is always 0.
 */
#include "platform/pal/ri_pal_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static struct ri_audio_cfg s_cfg;
static ri_audio_pull s_pull;
static void *s_user;
static int s_open;
static int s_started;
static struct ri_audio_info s_got;

static uint32_t env_u32(const char *name, uint32_t dflt) {
    const char *e = getenv(name);
    unsigned long v;
    if (!e || !e[0])
        return dflt;
    v = strtoul(e, 0, 10);
    if (v < 1u || v > 600u)
        return dflt;
    return (uint32_t)v;
}

int ri_pal_audio_open(const struct ri_audio_cfg *c, ri_audio_pull pull,
    void *user, struct ri_audio_info *got) {
    if (!c || !pull || !got || c->frames < 64u || c->frames > 4096u)
        return 1;
    s_cfg = *c;
    s_pull = pull;
    s_user = user;
    s_got.rate = c->want_rate ? c->want_rate : 48000u;
    s_got.frames = c->frames;
    s_got.format = RI_FMT_F32;
    snprintf(s_got.name, sizeof s_got.name, "null");
    *got = s_got;
    s_open = 1;
    s_started = 0;
    return 0;
}

static void wav_head(FILE *f, uint32_t rate, uint32_t frames) {
    uint32_t data = frames * 2u * 2u, riff = 36u + data;
    uint32_t sr = rate;
    uint16_t ch = 2u, bits = 16u, tag = 1u;
    uint32_t byte_rate = rate * 4u;
    uint16_t block = 4u;
    fwrite("RIFF", 1u, 4u, f);
    fwrite(&riff, 4u, 1u, f);
    fwrite("WAVE", 1u, 4u, f);
    fwrite("fmt ", 1u, 4u, f);
    {
        uint32_t sz = 16u;
        fwrite(&sz, 4u, 1u, f);
    }
    fwrite(&tag, 2u, 1u, f);
    fwrite(&ch, 2u, 1u, f);
    fwrite(&sr, 4u, 1u, f);
    fwrite(&byte_rate, 4u, 1u, f);
    fwrite(&block, 2u, 1u, f);
    fwrite(&bits, 2u, 1u, f);
    fwrite("data", 1u, 4u, f);
    fwrite(&data, 4u, 1u, f);
}

static int16_t f32_to_s16(float x) {
    float c = (x > 1.0f) ? 1.0f : ((x < -1.0f) ? -1.0f : x);
    float s = c * 32767.0f;
    return (int16_t)(s >= 0.0f ? s + 0.5f : s - 0.5f);
}

int ri_pal_audio_start(void) {
    const char *path;
    FILE *f;
    uint32_t secs, total, done = 0u, n = 0u;
    float *buf = 0;
    if (!s_open || !s_pull || s_started)
        return 1;
    path = getenv("RI_NULL_WAV");
    if (!path || !path[0])
        path = "/tmp/ri/null.wav";
    secs = env_u32("RI_NULL_SECONDS", 2u);
    total = secs * s_got.rate;
    f = fopen(path, "wb");
    if (!f)
        return 1;
    buf = (float *)malloc((size_t)s_got.frames * 2u * sizeof(float));
    if (!buf) {
        fclose(f);
        return 1;
    }
    wav_head(f, s_got.rate, total);
    while (done < total) {
        uint32_t want = total - done, i;
        int16_t s[8192 * 2u];
        if (want > s_got.frames)
            want = s_got.frames;
        if (want > 8192u)
            want = 8192u;
        s_pull(s_user, buf, want);
        for (i = 0u; i < want * 2u; i++)
            s[i] = f32_to_s16(buf[i]);
        if (fwrite(s, sizeof(int16_t), want * 2u, f) != want * 2u)
            break;
        done += want;
        n++;
        (void)n;
    }
    free(buf);
    fclose(f);
    s_started = 1;
    return done == total ? 0 : 1;
}

void ri_pal_audio_stop(void) {
    s_started = 0;
}

void ri_pal_audio_close(void) {
    s_open = 0;
    s_started = 0;
    s_pull = 0;
    s_user = 0;
}

uint32_t ri_pal_audio_late(void) {
    return 0u;
}
