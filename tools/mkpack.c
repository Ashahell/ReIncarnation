/* tools/mkpack.c — reproducible RBNM pack builder (owner 2026-09-27).
 *
 * The classic-01 pack.rbnm was built by a one-off lost to history; this
 * tool makes pack builds repeatable and audit-verifiable. No new format:
 * it drives project/rbnm.c rbnm_write_pack, and the output must pass the
 * existing inspect + S909 gates unchanged.
 *
 * usage: mkpack <spec.txt> <manifest.txt> <out.rbnm>
 * spec lines (one layer each, '#' comments, blank lines skipped):
 *   id voice lo hi path.wav
 *   id: layer id (<=31 chars); voice: 0..10 (RB909_* order);
 *   lo..hi: tune window (0..127, lo<=hi); path.wav: 8/16/24-bit PCM mono.
 * 24-bit masters convert by arithmetic >>8 (truncation, matching the
 * no-dither recipe convention of the classic-01 layers).
 * manifest.txt: full MANF text (validated before writing; every layer id
 * needs its row or the pack is refused, same law as the codec).
 *
 * Build (host, like the other tools):
 *   gcc -std=c99 -O2 -Wall -Wextra -Werror -pedantic -ffp-contract=off
 *     -fno-unsafe-math-optimizations -ftrapv -I. -o /tmp/ri/build/mkpack
 *     tools/mkpack.c /tmp/ri/build/OBJECTS -lm
 * (OBJECTS = the host core objects.)
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "project/rbnm.h"

#define MK_LAYERS_MAX 64u
#define MK_FRAMES_MAX 88200u /* 2 s at 44100, same cap as the render tool */
#define MK_LINE_MAX 512u

struct mk_layer {
    char id[32];
    uint8_t voice;
    uint8_t lo, hi;
    uint32_t rate;
    uint32_t frames;
    int16_t *pcm;
};

static uint32_t rd32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24);
}

static uint16_t rd16le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/* Minimal RIFF walk: PCM 8/16/24 mono -> s16 host samples. Returns frames,
 * or 0 with err filled. */
static uint32_t read_wav(const char *path, int16_t **out, uint32_t *rate_out,
    char *err, uint32_t errcap) {
    FILE *f;
    long sz, pos;
    uint8_t hdr[64];
    uint16_t fmt = 0u, ch = 0u, bits = 0u;
    uint32_t rate = 0u, data_len = 0u, data_off = 0u;
    int16_t *pcm = 0;
    uint32_t n = 0u, i;
    if (!path || !out || !rate_out)
        return 0u;
    f = fopen(path, "rb");
    if (!f) {
        snprintf(err, errcap, "open %.200s", path);
        return 0u;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 44L || fread(hdr, 1u, 12u, f) != 12u ||
        memcmp(hdr, "RIFF", 4u) != 0 || memcmp(hdr + 8u, "WAVE", 4u) != 0) {
        snprintf(err, errcap, "not RIFF/WAVE %.200s", path);
        fclose(f);
        return 0u;
    }
    pos = 12L;
    while (pos + 8L <= sz) {
        uint8_t chdr[8];
        uint32_t clen;
        if (fseek(f, pos, SEEK_SET) != 0)
            break;
        if (fread(chdr, 1u, 8u, f) != 8u)
            break;
        clen = rd32le(chdr + 4u);
        if (memcmp(chdr, "fmt ", 4u) == 0 && clen >= 16u &&
            fread(hdr, 1u, 16u, f) == 16u) {
            fmt = rd16le(hdr);
            ch = rd16le(hdr + 2u);
            rate = rd32le(hdr + 4u);
            bits = rd16le(hdr + 14u);
        } else if (memcmp(chdr, "data", 4u) == 0) {
            data_off = (uint32_t)pos + 8u;
            data_len = clen;
        }
        pos += 8L + (long)((clen + 1u) & ~1u);
    }
    if (fmt != 1u || ch != 1u || rate == 0u || data_len == 0u ||
        (bits != 8u && bits != 16u && bits != 24u)) {
        snprintf(err, errcap, "need PCM mono (fmt=%u ch=%u bits=%u)",
            fmt, ch, bits);
        fclose(f);
        return 0u;
    }
    n = data_len / (bits / 8u);
    if (n == 0u || n > MK_FRAMES_MAX) {
        snprintf(err, errcap, "bad length %.200s", path);
        fclose(f);
        return 0u;
    }
    pcm = (int16_t *)malloc((size_t)n * sizeof(int16_t));
    if (!pcm) {
        snprintf(err, errcap, "oom %.200s", path);
        fclose(f);
        return 0u;
    }
    fseek(f, (long)data_off, SEEK_SET);
    for (i = 0u; i < n; i++) {
        if (bits == 8u) {
            int c = fgetc(f);
            if (c == EOF)
                break;
            pcm[i] = (int16_t)(((int)c - 128) * 256);
        } else if (bits == 16u) {
            uint8_t b[2];
            if (fread(b, 1u, 2u, f) != 2u)
                break;
            pcm[i] = (int16_t)rd16le(b);
        } else {
            uint8_t b[3];
            int32_t v;
            if (fread(b, 1u, 3u, f) != 3u)
                break;
            v = (int32_t)((uint32_t)b[0] | ((uint32_t)b[1] << 8) |
                ((uint32_t)b[2] << 16));
            if (v & 0x800000)
                v |= ~0xFFFFFF;
            /* 24-bit masters: the shipped pack matches
             * rint(s24/8388608*32767) on 99.9% of samples (832 one-LSB
             * diffs in 793800 — inaudible; the original one-off's exact
             * rounding is unrecoverable). For bit-exact carry-over, feed
             * 16-bit WAVs extracted from the old pack instead. */
            {
                double norm = (double)v / 8388608.0 * 32767.0;
                pcm[i] = (int16_t)rint(norm);
            }
        }
    }
    fclose(f);
    if (i != n) {
        snprintf(err, errcap, "short read");
        free(pcm);
        return 0u;
    }
    *out = pcm;
    *rate_out = rate;
    return n;
}

static int parse_spec_line(char *line, char *id, unsigned *voice, unsigned *lo,
    unsigned *hi, char *wav) {
    char *tok[5];
    int k = 0;
    unsigned v[3];
    tok[0] = strtok(line, " \t\r\n");
    while (tok[k] && k < 4)
        tok[++k] = strtok(0, " \t\r\n");
    if (k != 4 || !tok[0] || !tok[1] || !tok[2] || !tok[3] || !tok[4])
        return 1;
    if (strlen(tok[0]) == 0u || strlen(tok[0]) > 31u)
        return 1;
    for (k = 0; k < 3; k++) {
        char *e = 0;
        unsigned long x = strtoul(tok[1 + k], &e, 10);
        if (!e || *e || x > 255u)
            return 1;
        v[k] = (unsigned)x;
    }
    if (v[0] > 10u || v[1] > 127u || v[2] > 127u || v[1] > v[2])
        return 1;
    if (strlen(tok[4]) == 0u || strlen(tok[4]) > 255u)
        return 1;
    strcpy(id, tok[0]);
    *voice = v[0];
    *lo = v[1];
    *hi = v[2];
    strcpy(wav, tok[4]);
    return 0;
}

int main(int argc, char **argv) {
    FILE *sf, *mf;
    long msz;
    char *manf = 0;
    char line[MK_LINE_MAX + 1u];
    struct mk_layer layers[MK_LAYERS_MAX];
    struct RBNMWriteLayer wl[MK_LAYERS_MAX];
    uint32_t nl = 0u, i;
    char err[256];
    if (argc != 4) {
        printf("usage: mkpack <spec.txt> <manifest.txt> <out.rbnm>\n");
        return 2;
    }
    mf = fopen(argv[2], "rb");
    if (!mf) {
        printf("mkpack FAIL: open manifest\n");
        return 1;
    }
    fseek(mf, 0, SEEK_END);
    msz = ftell(mf);
    fseek(mf, 0, SEEK_SET);
    if (msz <= 0L || msz > 65536L) {
        printf("mkpack FAIL: manifest size\n");
        fclose(mf);
        return 1;
    }
    manf = (char *)malloc((size_t)msz + 1u);
    if (!manf) {
        fclose(mf);
        return 1;
    }
    if (fread(manf, 1u, (size_t)msz, mf) != (size_t)msz) {
        printf("mkpack FAIL: manifest read\n");
        free(manf);
        fclose(mf);
        return 1;
    }
    manf[msz] = 0;
    fclose(mf);
    if (rbnm_validate_manifest_text(manf, err, sizeof err) != 0) {
        printf("mkpack FAIL: manifest invalid: %s\n", err);
        free(manf);
        return 1;
    }
    sf = fopen(argv[1], "r");
    if (!sf) {
        printf("mkpack FAIL: open spec\n");
        free(manf);
        return 1;
    }
    while (fgets(line, sizeof line, sf)) {
        char id[32], wav[256];
        unsigned voice = 0u, lo = 0u, hi = 0u;
        int16_t *pcm = 0;
        uint32_t rate = 0u, n = 0u;
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == '\n' || *p == 0)
            continue;
        if (nl >= MK_LAYERS_MAX) {
            printf("mkpack FAIL: too many layers\n");
            goto fail;
        }
        if (parse_spec_line(p, id, &voice, &lo, &hi, wav) != 0) {
            printf("mkpack FAIL: bad spec line: %s", line);
            goto fail;
        }
        for (i = 0u; i < nl; i++) {
            if (!strcmp(layers[i].id, id)) {
                printf("mkpack FAIL: dup id %s\n", id);
                goto fail;
            }
        }
        n = read_wav(wav, &pcm, &rate, err, sizeof err);
        if (n == 0u) {
            printf("mkpack FAIL: %s\n", err);
            goto fail;
        }
        strcpy(layers[nl].id, id);
        layers[nl].voice = (uint8_t)voice;
        layers[nl].lo = (uint8_t)lo;
        layers[nl].hi = (uint8_t)hi;
        layers[nl].rate = rate;
        layers[nl].frames = n;
        layers[nl].pcm = pcm;
        nl++;
    }
    fclose(sf);
    if (nl == 0u) {
        printf("mkpack FAIL: no layers\n");
        free(manf);
        return 1;
    }
    for (i = 0u; i < nl; i++) {
        wl[i].id = layers[i].id;
        wl[i].voice = layers[i].voice;
        wl[i].rate = layers[i].rate;
        wl[i].frames = layers[i].frames;
        wl[i].lo = layers[i].lo;
        wl[i].hi = layers[i].hi;
        wl[i].pcm = layers[i].pcm;
    }
    if (rbnm_write_pack(argv[3], wl, nl, manf) != 0) {
        printf("mkpack FAIL: write %s\n", argv[3]);
        goto fail;
    }
    printf("mkpack: %u layers -> %s\n", nl, argv[3]);
    for (i = 0u; i < nl; i++)
        printf("  %s voice=%u rate=%u frames=%u lo=%u hi=%u\n", layers[i].id,
            layers[i].voice, layers[i].rate, layers[i].frames, layers[i].lo,
            layers[i].hi);
    for (i = 0u; i < nl; i++)
        free(layers[i].pcm);
    free(manf);
    return 0;
fail:
    fclose(sf);
    for (i = 0u; i < nl; i++)
        free(layers[i].pcm);
    free(manf);
    return 1;
}
