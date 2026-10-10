/* project/stem_wav.c — R8 stem WAV writer. See stem_wav.h. */
#include <stdio.h>
#include <string.h>

#include "project/stem_wav.h"

uint32_t stem_wav_u16(const uint8_t *p) {
    return p ? ((uint32_t)p[0] | ((uint32_t)p[1] << 8)) : 0u;
}

uint32_t stem_wav_u32(const uint8_t *p) {
    return p ? ((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24)) : 0u;
}

static uint32_t put32(uint8_t *o, uint32_t at, uint32_t v) {
    o[at + 0u] = (uint8_t)(v & 0xFFu);
    o[at + 1u] = (uint8_t)((v >> 8) & 0xFFu);
    o[at + 2u] = (uint8_t)((v >> 16) & 0xFFu);
    o[at + 3u] = (uint8_t)((v >> 24) & 0xFFu);
    return at + 4u;
}

static uint32_t put16(uint8_t *o, uint32_t at, uint32_t v) {
    o[at + 0u] = (uint8_t)(v & 0xFFu);
    o[at + 1u] = (uint8_t)((v >> 8) & 0xFFu);
    return at + 2u;
}

static uint32_t put_tag(uint8_t *o, uint32_t at, const char *t) {
    o[at + 0u] = (uint8_t)t[0];
    o[at + 1u] = (uint8_t)t[1];
    o[at + 2u] = (uint8_t)t[2];
    o[at + 3u] = (uint8_t)t[3];
    return at + 4u;
}

/* Bytes per sample, interleaved. 24-bit is THREE -- the usual off-by-one,
 * because it is not a byte count and a writer that rounds it up writes 32
 * bits of header over 24 bits of data and produces a file that skips. */
static uint32_t bytes_per_sample(uint32_t depth) {
    if (depth == STEM_WAV_PCM16)
        return 2u;
    if (depth == STEM_WAV_PCM24)
        return 3u;
    if (depth == STEM_WAV_F32)
        return 4u;
    return 0u;
}

uint32_t stem_wav_write(uint8_t *out, uint32_t cap, const float *samples,
    uint32_t frames, uint32_t channels, uint32_t rate, uint32_t depth,
    const char *path) {
    uint32_t bps, align, data_bytes, total, at = 0u, i, c;
    (void)path;
    if (!out || !samples || frames == 0u || channels == 0u || channels > 2u || rate == 0u)
        return 0u;
    bps = bytes_per_sample(depth);
    /* An unsupported depth is REFUSED, not rounded: a 20-bit request that
     * quietly became 24 would produce a file that opens and plays the wrong
     * thing, which is worse than no file. */
    if (bps == 0u)
        return 0u;
    align = bps * channels;
    data_bytes = frames * align;
    total = 44u + data_bytes;
    if (cap < total)
        return 0u;

    at = put_tag(out, at, "RIFF");
    /* The REAL byte count. 0 or 0xFFFFFFFF here is the classic unopenable
     * WAV, and there is deliberately no way to ask for one. */
    at = put32(out, at, total - 8u);
    at = put_tag(out, at, "WAVE");
    at = put_tag(out, at, "fmt ");
    at = put32(out, at, 16u);
    /* IEEE float is format 3. PCM is 1. Writing 1 with 32 bits produces a
     * file that opens and plays as noise. */
    at = put16(out, at, (depth == STEM_WAV_F32) ? 3u : 1u);
    at = put16(out, at, channels);
    at = put32(out, at, rate);
    at = put32(out, at, rate * align);          /* byte rate */
    at = put16(out, at, align);                 /* block align */
    at = put16(out, at, depth);
    at = put_tag(out, at, "data");
    at = put32(out, at, data_bytes);

    for (i = 0u; i < frames; i++) {
        for (c = 0u; c < channels; c++) {
            /* INTERLEAVED: channel-major inside a frame. A writer that
             * emits all of channel 0 then all of channel 1 produces a file
             * that plays one channel, then silence. */
            float v = samples[i * channels + c];
            if (v > 1.0f)
                v = 1.0f;
            if (v < -1.0f)
                v = -1.0f;
            if (depth == STEM_WAV_F32) {
                uint32_t bits;
                memcpy(&bits, &v, sizeof bits);
                at = put32(out, at, bits);
            } else if (depth == STEM_WAV_PCM16) {
                int32_t s16 = (int32_t)(v * 32767.0f);
                at = put16(out, at, (uint32_t)(uint16_t)(int16_t)s16);
            } else {
                int32_t s24 = (int32_t)(v * 8388607.0f);
                out[at + 0u] = (uint8_t)(s24 & 0xFF);
                out[at + 1u] = (uint8_t)((s24 >> 8) & 0xFF);
                out[at + 2u] = (uint8_t)((s24 >> 16) & 0xFF);
                at += 3u;
            }
        }
    }
    return at;
}

void stem_set_init(struct RIStemSet *s, uint32_t rate, uint32_t bpm_milli) {
    if (!s)
        return;
    memset(s, 0, sizeof *s);
    s->rate = rate;
    s->bpm_milli = bpm_milli;
    s->depth = STEM_WAV_PCM24;
    s->common_frames = 0u;
}

/* The common length is the LONGEST stem. Taking the first, the average, or
 * the shortest would each silently drop or invent audio on some channel. */
static void recount(struct RIStemSet *s) {
    uint32_t i, longest = 0u, padded = 0u;
    for (i = 0u; i < s->nstems; i++) {
        if (s->frames[i] > longest)
            longest = s->frames[i];
    }
    for (i = 0u; i < s->nstems; i++) {
        if (s->frames[i] < longest)
            padded++;
    }
    s->common_frames = longest;
    s->padded = padded;
}

int stem_set_add(struct RIStemSet *s, const float *samples, uint32_t frames,
    uint32_t channels) {
    if (!s || !samples || frames == 0u || channels == 0u || channels > 2u)
        return 1;
    if (s->nstems >= RI_STEM_MAX)
        return 1;   /* say so rather than drop the last stem silently */
    s->samples[s->nstems] = samples;
    s->frames[s->nstems] = frames;
    s->channels[s->nstems] = (uint8_t)channels;
    s->nstems++;
    recount(s);
    return 0;
}

uint32_t stem_set_frames(const struct RIStemSet *s) { return s ? s->common_frames : 0u; }
uint32_t stem_set_padded(const struct RIStemSet *s) { return s ? s->padded : 0u; }
uint32_t stem_set_count(const struct RIStemSet *s) { return s ? s->nstems : 0u; }

uint32_t stem_set_write(const struct RIStemSet *s, uint32_t idx, uint8_t *out,
    uint32_t cap) {
    static float pad[65536];
    const float *src;
    uint32_t n;
    if (!s || idx >= s->nstems)
        return 0u;
    if (s->frames[idx] == s->common_frames)
        return stem_wav_write(out, cap, s->samples[idx], s->common_frames,
            s->channels[idx], s->rate, s->depth, 0);
    if (s->common_frames > 65536u)
        return 0u;
    /* Pad with SILENCE to the common length, never trim. A stem trimmed at
     * the end is a song that ends early on one channel. */
    memset(pad, 0, (size_t)s->common_frames * sizeof *pad);
    memcpy(pad, s->samples[idx], (size_t)s->frames[idx] * sizeof *pad);
    src = pad;
    n = stem_wav_write(out, cap, src, s->common_frames, s->channels[idx],
        s->rate, s->depth, 0);
    return n;
}

uint32_t stem_set_meta(const struct RIStemSet *s, char *out, uint32_t cap) {
    int n;
    if (!s || !out || cap == 0u)
        return 0u;
    n = snprintf(out, (size_t)cap,
        "tempo=%lu.%03lu rate=%lu frames=%lu stems=%lu depth=%u",
        (unsigned long)(s->bpm_milli / 1000u),
        (unsigned long)(s->bpm_milli % 1000u),
        (unsigned long)s->rate, (unsigned long)s->common_frames,
        (unsigned long)s->nstems, (unsigned)s->depth);
    return (n < 0 || (uint32_t)n >= cap) ? 0u : (uint32_t)n;
}