/* t28_wavdepth — M2.5, TC-2.13.4 (WAV export depth):
 *
 * Export is 16-bit-only in both writers (tools/render.c write_wav,
 * audio_io/audio.c auf_write_wav_header + s16 loop); TC-2.13.4
 * requires 16/24-bit (44.1 kHz + AIFF stay OPEN, noted below).
 *
 *   - 16-bit golden structural (property): tests/golden/303/
 *     math-dc.wav parses field-exact (RIFF size, WAVEfmt, fmt 16 /
 *     PCM / mono / 48000, data size, file size == 44 + data).
 *   - auf_wav_header matrix (RED driver): (depth, total, sr) over
 *     {16,24} x {0, 1, 48000} x {48000} — every field exact
 *     (riff/data sizes, rate = sr*bps, align, bits). Does not exist
 *     yet → this file fails to BUILD (feature-absent manifest).
 *   - auf_f32_to_s24 exact bytes (RED driver): 1.0 -> FF FF 7F,
 *     -1.0 -> 01 00 80 (i.e. -8388607: float->int truncates toward
 *     zero, twin-consistent with the 16-bit -32767 edge), 0.0 -> 00,
 *     +-2.0 clamp to full scale. Does not exist yet → build-RED
 *     with the above.
 *   - 24-bit golden structural: tests/golden/303/dc-24.wav parses
 *     field-exact (bits 24, align 3, rate 144000, size == 44 + 3N).
 *   - AuRenderToFileDepth null-fail-closed (rc 2, no crash).
 *
 * CWD convention: repo root (golden paths, same as t1_fx).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* audio_io/audio.c export surface (depth-parameterized). */
void auf_wav_header(unsigned char hdr[44], uint32_t total, uint8_t depth,
    uint32_t sr);
int32_t auf_f32_to_s24(float x);
int AuRenderToFileDepth(void *ao, const char *path, uint32_t ms,
    uint8_t depth);

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static long fsize(const char *path) {
    long n = -1;
    FILE *f = fopen(path, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        n = ftell(f);
        fclose(f);
    }
    return n;
}

/* Structural parse shared by both goldens; returns data bytes or -1. */
static long parse_wav(const char *path, uint8_t want_depth, uint32_t want_sr) {
    unsigned char h[44];
    uint32_t data, riff, rate;
    long n;
    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("FAIL missing %s\n", path);
        fails++;
        return -1;
    }
    if (fread(h, 1, 44, f) != 44) {
        printf("FAIL short header %s\n", path);
        fclose(f);
        fails++;
        return -1;
    }
    fclose(f);
    CHECK(memcmp(h, "RIFF", 4) == 0, "%s riff magic", path);
    CHECK(memcmp(h + 8, "WAVEfmt ", 8) == 0, "%s wavefmt magic", path);
    CHECK(rd32(h + 16) == 16u, "%s fmt size %u", path, rd32(h + 16));
    CHECK(h[20] == 1u && h[21] == 0u, "%s audioformat", path);
    CHECK(h[22] == 1u && h[23] == 0u, "%s channels", path);
    CHECK(rd32(h + 24) == want_sr, "%s rate %u", path, rd32(h + 24));
    rate = want_sr * (want_depth / 8u);
    CHECK(rd32(h + 28) == rate, "%s byterate %u want %u", path,
        rd32(h + 28), rate);
    CHECK(h[32] == (want_depth / 8u) && h[33] == 0u, "%s align", path);
    CHECK(h[34] == want_depth && h[35] == 0u, "%s bits", path);
    CHECK(memcmp(h + 36, "data", 4) == 0, "%s data magic", path);
    data = rd32(h + 40);
    riff = rd32(h + 4);
    CHECK(riff == 36u + data, "%s riff %u want %u", path, riff, 36u + data);
    n = fsize(path);
    CHECK(n == (long)(44u + data), "%s filesize %ld want %u", path, n,
        44u + data);
    return (long)data;
}

int main(void) {
    unsigned char h[44];
    uint32_t totals[3] = { 0u, 1u, 48000u };
    uint8_t depths[2] = { 16u, 24u };
    uint32_t ti, di;
    long data;

    /* --- 16-bit golden structural (property) --- */
    data = parse_wav("tests/golden/303/math-dc.wav", 16u, 48000u);
    if (data > 0)
        printf("math-dc.wav: data %ld bytes valid\n", data);

    /* --- header matrix over the new builder --- */
    for (di = 0; di < 2; di++) {
        for (ti = 0; ti < 3; ti++) {
            uint32_t bps = depths[di] / 8u;
            uint32_t bytes = totals[ti] * bps;
            auf_wav_header(h, totals[ti], depths[di], 48000u);
            CHECK(rd32(h + 4) == 36u + bytes, "riff d=%u n=%u",
                depths[di], totals[ti]);
            CHECK(rd32(h + 28) == 48000u * bps, "rate d=%u",
                depths[di]);
            CHECK(h[32] == bps, "align d=%u", depths[di]);
            CHECK(h[34] == depths[di], "bits d=%u", depths[di]);
            CHECK(rd32(h + 40) == bytes, "data d=%u n=%u",
                depths[di], totals[ti]);
        }
    }
    printf("header matrix 2x3 exact\n");

    /* --- s24 packing exact bytes. Negative edge is twin-consistent
     * with auf_f32_to_s16: float->int truncates toward zero, so
     * -1.0 -> (int32_t)(-8388607.5) = -8388607 (0xFF800001), just
     * as -1.0 -> -32767 in 16-bit. Full-scale symmetry (0x800000)
     * would diverge the twins; the contract is identical rounding,
     * more bits. --- */
    CHECK(auf_f32_to_s24(1.0f) == 0x7FFFFF, "fs+ got 0x%x",
        (unsigned)auf_f32_to_s24(1.0f));
    CHECK(auf_f32_to_s24(-1.0f) == -8388607, "fs- got 0x%x",
        (unsigned)auf_f32_to_s24(-1.0f));
    CHECK(auf_f32_to_s24(0.0f) == 0, "zero got 0x%x",
        (unsigned)auf_f32_to_s24(0.0f));
    CHECK(auf_f32_to_s24(2.0f) == 0x7FFFFF, "clamp+ got 0x%x",
        (unsigned)auf_f32_to_s24(2.0f));
    CHECK(auf_f32_to_s24(-2.0f) == -8388607, "clamp- got 0x%x",
        (unsigned)auf_f32_to_s24(-2.0f));

    /* --- 24-bit golden structural --- */
    data = parse_wav("tests/golden/303/dc-24.wav", 24u, 48000u);
    if (data > 0)
        printf("dc-24.wav: data %ld bytes valid\n", data);

    /* --- depth wrapper null-fail-closed --- */
    CHECK(AuRenderToFileDepth(0, 0, 0, 24u) == 2, "depth null rc");

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t28_wavdepth\n");
    return fails != 0;
}
