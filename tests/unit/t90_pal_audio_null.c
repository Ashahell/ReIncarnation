/* t90_pal_audio_null — portability T4: host null audio backend.
 * Deterministic pull cadence: two 1-second renders of a sine pull are
 * byte-identical WAVs with a valid header; late() is 0.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_audio.h"

static uint32_t t_phase;

static void t_pull(void *user, void *out, uint32_t frames) {
    float *f = (float *)out;
    uint32_t i;
    (void)user;
    for (i = 0u; i < frames; i++) {
        float s = sinf((float)(t_phase + i) * 0.1f) * 0.5f;
        f[i * 2u] = s;
        f[i * 2u + 1u] = -s;
        if (i == frames - 1u)
            t_phase += frames;
    }
}

static int render_wav(uint32_t *size_out) {
    /* Default backend path (/tmp/ri/null.wav) and length (2 s): no env. */
    const char *path = "/tmp/ri/null.wav";
    struct ri_audio_cfg c;
    struct ri_audio_info got;
    FILE *f;
    long sz;
    c.want_rate = 48000u;
    c.frames = 256u;
    c.channels = 2u;
    RI_ASSERT(ri_pal_audio_open(&c, t_pull, 0, &got) == 0, "open");
    RI_ASSERT(got.rate == 48000u && got.frames == 256u, "info");
    RI_ASSERT(ri_pal_audio_start() == 0, "start");
    RI_ASSERT(ri_pal_audio_late() == 0u, "late");
    ri_pal_audio_stop();
    ri_pal_audio_close();
    f = fopen(path, "rb");
    RI_ASSERT(f != 0, "wav open");
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fclose(f);
    RI_ASSERT(sz == 44L + 2L * 48000L * 2L * 2L, "wav size %ld", sz);
    if (size_out)
        *size_out = (uint32_t)sz;
    return 0;
}

static uint32_t fnv_file(const char *path, uint32_t *size_out) {
    FILE *f;
    uint32_t h = 2166136261u, sz = 0u;
    int c;
    f = fopen(path, "rb");
    if (!f)
        return 0u;
    while ((c = fgetc(f)) != EOF) {
        h ^= (uint32_t)c;
        h *= 16777619u;
        sz++;
    }
    fclose(f);
    if (size_out)
        *size_out = sz;
    return h;
}

int main(void) {
    const char *p1 = "/tmp/ri/null.wav";
    uint32_t s1 = 0u, s2 = 0u, h1, h2;
    t_phase = 0u;
    RI_ASSERT(render_wav(&s1) == 0, "render a");
    h1 = fnv_file(p1, 0);
    t_phase = 0u;
    RI_ASSERT(render_wav(&s2) == 0, "render b");
    h2 = fnv_file(p1, 0);
    RI_ASSERT(s1 == s2, "sizes");
    RI_ASSERT(h1 != 0u && h1 == h2, "deterministic %08x", h2);
    RI_RESULT("pal_audio_null");
}
