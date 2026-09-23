/* t32_aiff — M2.5, TC-2.13.4/WBS-2.13 AIFF half (plain AIFF, PCM):
 *
 * No AIFF writer exists anywhere in the tree (only WBS/plan prose).
 * Scope: plain big-endian AIFF (FORM/AIFF + COMM + SSND, no MARK/
 * INST), 16|24-bit, 44100|48000 Hz — the export matrix the WAV
 * work established. AIFF-C/sowt is out (different container).
 *
 *   - Goldens structural (RED while missing): tests/golden/303/
 *     dc-aiff.wav + first-light-aiff.wav parse field-exact
 *     (FORM/AIFF magic, COMM 18 (channels/frames/bits/80-bit
 *     rate), SSND offset+blocksize zero, sizes consistent).
 *   - auf_aiff_header matrix (RED driver): {16,24} x {0,1,48000}
 *     x {44100,48000} field-exact, including the 80-bit extended
 *     rate bytes verified against ffmpeg-generated references
 *     (44100 = 40 0E AC 44 00.., 48000 = 40 0E BB 80 00..;
 *     /tmp/ri_build/ref16-*.aiff, od-dumped 2026-09-23).
 *     Does not exist yet → build-RED with the items below.
 *   - Bad sr (22050) / bad depth (8) → rc 2 (no silent fallback).
 *   - AuRenderToFileAiff null-fail-closed (rc 2, no crash).
 *
 * CWD convention: repo root (golden paths, same as t28/t31).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* audio_io/audio.c AIFF export surface. */
int auf_aiff_header(unsigned char hdr[54], uint32_t frames, uint8_t depth,
    uint32_t sr);
int AuRenderToFileAiff(void *ao, const char *path, uint32_t ms,
    uint8_t depth);

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static uint32_t rd32be(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
        ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/* Structural parse; returns data bytes or -1. */
static long parse_aiff(const char *path, uint8_t want_depth,
    uint32_t want_sr) {
    unsigned char h[54];
    uint32_t form_size, frames, ssnd_size, bps;
    long fsize = -1;
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("FAIL missing %s\n", path);
        fails++;
        return -1;
    }
    if (fread(h, 1, 54, f) != 54) {
        printf("FAIL short header %s\n", path);
        fclose(f);
        fails++;
        return -1;
    }
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fclose(f);
    bps = want_depth / 8u;
    CHECK(memcmp(h, "FORM", 4) == 0, "%s FORM", path);
    CHECK(memcmp(h + 8, "AIFF", 4) == 0, "%s AIFF", path);
    CHECK(memcmp(h + 12, "COMM", 4) == 0, "%s COMM", path);
    CHECK(rd32be(h + 16) == 18u, "%s comm size", path);
    CHECK(h[20] == 0u && h[21] == 1u, "%s mono", path);
    frames = rd32be(h + 22);
    CHECK(h[26] == 0u && h[27] == want_depth, "%s bits", path);
    if (want_sr == 44100u)
        CHECK(memcmp(h + 28, "\x40\x0e\xac\x44\x00\x00\x00\x00\x00\x00",
            10) == 0, "%s ext44100", path);
    else if (want_sr == 48000u)
        CHECK(memcmp(h + 28, "\x40\x0e\xbb\x80\x00\x00\x00\x00\x00\x00",
            10) == 0, "%s ext48000", path);
    else
        CHECK(0, "%s unexpected sr %u", path, want_sr);
    CHECK(memcmp(h + 38, "SSND", 4) == 0, "%s SSND", path);
    ssnd_size = rd32be(h + 42);
    CHECK(rd32be(h + 46) == 0u, "%s ssnd offset", path);
    CHECK(rd32be(h + 50) == 0u, "%s ssnd blocksize", path);
    CHECK(ssnd_size == 8u + frames * bps, "%s ssnd size", path);
    form_size = rd32be(h + 4);
    CHECK(form_size == (uint32_t)(fsize - 8), "%s form size", path);
    printf("%s: sr %u depth %u frames %u\n", path, want_sr, want_depth,
        frames);
    return (long)(frames * bps);
}

int main(void) {
    unsigned char h[54];
    uint32_t totals[3] = { 0u, 1u, 48000u };
    uint8_t depths[2] = { 16u, 24u };
    uint32_t srs[2] = { 44100u, 48000u };
    uint32_t ti, di, si;

    /* --- goldens structural --- */
    parse_aiff("tests/golden/303/dc-aiff.wav", 16u, 48000u);
    parse_aiff("tests/golden/303/first-light-aiff.wav", 16u, 48000u);

    /* --- header matrix over the new builder --- */
    for (di = 0; di < 2; di++) {
        for (si = 0; si < 2; si++) {
            for (ti = 0; ti < 3; ti++) {
                uint32_t bps = depths[di] / 8u;
                uint32_t bytes = totals[ti] * bps;
                int rc = auf_aiff_header(h, totals[ti], depths[di],
                    srs[si]);
                CHECK(rc == 0, "builder rc d=%u sr=%u n=%u",
                    depths[di], srs[si], totals[ti]);
                CHECK(memcmp(h, "FORM", 4) == 0, "form d=%u",
                    depths[di]);
                CHECK(rd32be(h + 4) == 46u + bytes, "formsize d=%u n=%u",
                    depths[di], totals[ti]);
                CHECK(memcmp(h + 12, "COMM", 4) == 0, "comm d=%u",
                    depths[di]);
                CHECK(rd32be(h + 22) == totals[ti], "frames d=%u",
                    depths[di]);
                CHECK(h[27] == depths[di], "bits d=%u", depths[di]);
                CHECK(memcmp(h + 38, "SSND", 4) == 0, "ssnd d=%u",
                    depths[di]);
                CHECK(rd32be(h + 42) == 8u + bytes, "ssndsize d=%u",
                    depths[di]);
            }
        }
    }
    printf("aiff header matrix 2x2x3 exact\n");

    /* --- bad sr / bad depth fail closed --- */
    CHECK(auf_aiff_header(h, 100, 16u, 22050u) == 2, "bad sr accepted");
    CHECK(auf_aiff_header(h, 100, 8u, 48000u) == 2, "bad depth accepted");

    /* --- wrapper null-fail-closed --- */
    CHECK(AuRenderToFileAiff(0, 0, 0, 16u) == 2, "aiff null rc");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t32_aiff\n");
    return fails != 0;
}
