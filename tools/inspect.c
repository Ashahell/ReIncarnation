/* tools/inspect — RBNM/RBNG validator/dumper (Task 9 G9 + Task 13 G13).
 *
 * usage:
 *   inspect --rbnm FILE       validate pack: chunk lengths, S909/MANF/
 *                             SMPL presence + cross-refs, manifest
 *                             completeness. Prints "RBNM OK ..." or
 *                             "RBNM INVALID: <reason>".
 *   inspect --manifest FILE   validate a MANIFEST.txt alone (same line
 *                             rules). Prints "MANIFEST OK ..." or
 *                             "MANIFEST INVALID: <reason>".
 *   inspect --rbng FILE       validate song: VERS/SONG/PATT/AUTO/MODR/
 *                             CPRG presence, ranges, forward-compat
 *                             (unknown optional skipped + counted).
 *                             Prints "RBNG OK ..." (+ CPRG/unknown
 *                             counts) or "RBNG INVALID: <reason>".
 *   inspect --wav FILE        validate a 16-bit mono 48 kHz WAV header
 *                             (RIFF/WAVE/fmt 16/PCM/mono/16-bit/48k +
 *                             data-size consistency; host fallback when
 *                             `sox --i` is unavailable). Prints
 *                             "WAV OK ..." or "WAV INVALID: <reason>".
 *   inspect --rbnm-reserialize SRC DST
 *                             validate SRC then re-emit every chunk
 *                             verbatim to DST (unknown/CPRG preservation
 *                             proof; byte-identical on success).
 * exit: 0 valid, 1 invalid (with reason), 2 usage/IO error.
 * Corrupt input MUST exit 1/2 with a reason — never crash, never hang
 * (the 500-mutation fuzz in scripts/ri_fuzz.sh asserts exactly this).
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "project/rbnm.h"
#include "project/rbng.h"

static int read_text(const char *path, char *dst, uint32_t cap) {
    FILE *f = fopen(path, "rb");
    uint32_t n = 0;
    int ch;
    if (!f) {
        printf("inspect: cannot open %s\n", path);
        return 2;
    }
    while ((ch = fgetc(f)) != EOF) {
        if (n + 1u >= cap) {
            printf("inspect: file too large %s\n", path);
            fclose(f);
            return 2;
        }
        dst[n++] = (char)ch;
    }
    fclose(f);
    dst[n] = '\0';
    return 0;
}

static int cmd_rbnm(const char *path) {
    char err[192];
    int rc = rbnm_validate_file(path, err, sizeof err);
    if (rc == 0) {
        printf("RBNM OK: %s\n", path);
        return 0;
    }
    printf("RBNM INVALID: %s: %s\n", path, err);
    return 1;
}

static int cmd_manifest(const char *path) {
    static char text[262144];
    char err[192];
    int rc;
    if (read_text(path, text, sizeof text) != 0)
        return 2;
    rc = rbnm_validate_manifest_text(text, err, sizeof err);
    if (rc == 0) {
        printf("MANIFEST OK: %s\n", path);
        return 0;
    }
    printf("MANIFEST INVALID: %s: %s\n", path, err);
    return 1;
}

static int cmd_rbng(const char *path) {
    static struct RISong s;
    static char err[192];
    char cprg[136];
    if (rbng_read_song(path, &s, err, sizeof err) != 0) {
        printf("RBNG INVALID: %s: %s\n", path, err);
        return 1;
    }
    if (rbng_cprg_line(path, cprg, sizeof cprg) != 0)
        cprg[0] = '\0';
    printf("RBNG OK: %s (tempo=%u ppq=%u steps=%u auto=%u mods=%u CPRG='%s' unknown=%u art=%s)\n",
        path, s.tempo, s.ppq, s.nsteps, s.nauto, s.nmods, cprg,
        s.nunknown, rbng_art_fallback(&s) ? "fallback" : "full");
    return 0;
}

/* WAV header validation (16-bit mono 48 kHz, the render.c contract).
 * All reads bounded: short files fail closed with a reason. */
static uint16_t wav_u16(const unsigned char *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t wav_u32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int cmd_wav(const char *path) {
    static unsigned char hdr[64];
    FILE *f = fopen(path, "rb");
    uint32_t n = 0, riff_n, data_n;
    int ch;
    if (!f) {
        printf("inspect: cannot open %s\n", path);
        return 2;
    }
    while (n < sizeof hdr && (ch = fgetc(f)) != EOF)
        hdr[n++] = (unsigned char)ch;
    fclose(f);
    if (n < 44u) {
        printf("WAV INVALID: %s: short header\n", path);
        return 1;
    }
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVEfmt ", 8) != 0) {
        printf("WAV INVALID: %s: not RIFF/WAVE\n", path);
        return 1;
    }
    if (wav_u32(hdr + 16) != 16u || wav_u16(hdr + 20) != 1u ||
        wav_u16(hdr + 22) != 1u || wav_u32(hdr + 24) != 48000u ||
        wav_u16(hdr + 34) != 16u) {
        printf("WAV INVALID: %s: want fmt16/pcm/mono/48k/16-bit\n", path);
        return 1;
    }
    if (memcmp(hdr + 36, "data", 4) != 0) {
        printf("WAV INVALID: %s: missing data chunk\n", path);
        return 1;
    }
    riff_n = wav_u32(hdr + 4);
    data_n = wav_u32(hdr + 40);
    if (riff_n != 36u + data_n || (data_n & 1u) != 0u) {
        printf("WAV INVALID: %s: size mismatch\n", path);
        return 1;
    }
    printf("WAV OK: %s (mono16 48k, %u samples)\n", path, data_n / 2u);
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "--rbnm-reserialize") == 0) {
        char err[192];
        if (rbnm_reserialize(argv[2], argv[3], err, sizeof err) != 0) {
            printf("RBNM INVALID: %s: %s\n", argv[2], err);
            return 1;
        }
        printf("RBNM RESERIALIZED: %s -> %s\n", argv[2], argv[3]);
        return 0;
    }
    if (argc != 3) {
        printf("usage: inspect --rbnm FILE | --manifest FILE | --rbng FILE | --wav FILE\n");
        printf("       inspect --rbnm-reserialize SRC DST\n");
        return 2;
    }
    if (strcmp(argv[1], "--rbnm") == 0)
        return cmd_rbnm(argv[2]);
    if (strcmp(argv[1], "--manifest") == 0)
        return cmd_manifest(argv[2]);
    if (strcmp(argv[1], "--rbng") == 0)
        return cmd_rbng(argv[2]);
    if (strcmp(argv[1], "--wav") == 0)
        return cmd_wav(argv[2]);
    printf("usage: inspect --rbnm FILE | --manifest FILE | --rbng FILE | --wav FILE\n");
    printf("       inspect --rbnm-reserialize SRC DST\n");
    return 2;
}
