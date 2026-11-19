/* t21_looppcm — TC-2.1.2 PCM half: loop-region double render equality.
 * 16-step song at tempo 120 (halves identical: pluck at steps 0 and 8,
 * rests elsewhere) rendered once through the file path; PCM bytes of
 * loop iteration 1 [44,44+192000) vs iteration 2 [44+192000,44+384000)
 * must be byte-identical (8 steps * 12000 samples * 2 B @48k).
 * On mismatch the test reports max abs int16 diff + first diff offset
 * (characterizes state carryover: settle-gap song vs loop-reset design)
 * and fails. File under RI_TMP_ROOT "/run/t21" (audit-created).
 *
 * Uses the same offline render pipeline as tools/render.c (host-compatible). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "project/rbng.h"
#include "project/songscript.h"
#include "platform/pal/ri_tmp.h"

#define T21L_DIR RI_TMP_ROOT "/run/t21"
#define T21L_SONG T21L_DIR "/loop.rbng"
#define T21L_WAV T21L_DIR "/loop.wav"
/* 16th = 24 ticks = 6000 samples @120 BPM/48 kHz; 8-step loop = 48000.
 * File total = pattern end (96000) + 3 s tail (144000) = 240000 frames.
 * Compare last 2 s (96000 frames = 48000 per half) after filter settles. */
#define T21L_HALF_SAMPLES 48000u
#define T21L_HALF_BYTES (T21L_HALF_SAMPLES * 4u)
#define T21L_WANT_SIZE (44u + 240000u * 4u)

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

/* 16-step song text (tempo 120, note 69 at step 0 and step 8) 
 * Note 69 = A4 = 440 Hz (integer Hz, phase repeats every 1s at 48kHz) */
static const char *k_lines[] = {
    "RBS 1",
    "TEMPO 120",
    "PPQ 96",
    "P303 0 0 69 - - - - - - - 69 - - - - - - -",
    NULL
};

static int write_song_text(const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; k_lines[i]; i++) {
        fprintf(f, "%s\n", k_lines[i]);
        printf("DEBUG: wrote line: %s\n", k_lines[i]);
    }
    fclose(f);
    return 0;
}

int main(void)
{
    int i;
    FILE *f;
    long fsize1;
    size_t nr2;
    static unsigned char pcm[1000000];
    static float L[250000], R[250000];

    /* Write song text */
    CHECK(write_song_text(T21L_SONG) == 0, "song write");

    /* Compile text -> RBNG binary */
    {
        struct RISong song;
        char err[256];
        char text[2048];
        int pos = 0;
        for (i = 0; k_lines[i]; i++) {
            int len = strlen(k_lines[i]);
            memcpy(text + pos, k_lines[i], len);
            text[pos + len] = '\n';
            pos += len + 1;
        }
        text[pos] = '\0';
        if (ri_rbs_parse(text, &song, err, sizeof err) != 0) {
            printf("rbng parse failed: %s\n", err);
            return 1;
        }
        if (rbng_write_song(T21L_SONG, &song, err, sizeof err) != 0) {
            printf("rbng write failed: %s\n", err);
            return 1;
        }
    }

    /* Load the RBNG song */
    struct RISong song;
    char err[256];
    if (rbng_read_song(T21L_SONG, &song, err, sizeof err) != 0) {
        printf("render: RBNG invalid: %s: %s\n", T21L_SONG, err);
        return 1;
    }

    /* Build schedule using the same pipeline as tools/render.c */
    struct RIStep steps[64];
    struct RISegment segs[1];
    struct RISchedOpts opts;
    struct RITempoMap map;
    struct RIEvent ev[8192];
    uint32_t nev;

    for (i = 0; i < song.nsteps; i++) {
        steps[i].note = song.steps[i].note;
        steps[i].flags = song.steps[i].flags;
    }
    segs[0].start_tick = 0;
    segs[0].ns_per_quarter = 60000000000ULL / (uint64_t)song.tempo;
    map.segs = segs;
    map.n = 1;
    map.ppq = song.ppq;
    map.sr = 48000.0;
    opts.shuffle_pct = 0;
    opts.legato = 0;
    opts.flam_ms = 35; /* RI_FLAM_MS_DEFAULT */
    nev = ri_sched_emit_timed(&map, 0, song.ppq, steps, song.nsteps, 0,
        &opts, ev, 8192);
    if (nev == 0) {
        printf("render: walker emitted no events\n");
        return 1;
    }

    /* AUTO lanes: tick (song ppq) -> sample, merged as AUTOMATION. */
    double nsq = 60000000000.0 / (double)song.tempo;
    double tick2smp = nsq * (double)48000.0 / ((double)song.ppq * 1e9);
    for (i = 0; i < song.nauto; i++) {
        uint64_t s = (uint64_t)((double)song.auto_ev[i].tick * tick2smp + 0.5);
        uint32_t p = nev;
        if (nev >= 8192) {
            printf("render: AUTO overflow\n");
            return 1;
        }
        ev[nev].sample = s;
        ev[nev].type = RI_EV_AUTOMATION;
        ev[nev].device = 0;
        ev[nev].voice = 0;
        ev[nev].value = song.auto_ev[i].ctl;
        ev[nev].flags = song.auto_ev[i].val;
        ev[nev].seq = nev;
        nev++;
        /* Insertion-sort the newcomer back (list was sorted). */
        while (p > 0u && ev[p].sample < ev[p - 1u].sample) {
            struct RIEvent t = ev[p];
            ev[p] = ev[p - 1u];
            ev[p - 1u] = t;
            p--;
        }
    }

    /* Total render: pattern (96000) + 3s tail (144000) = 240000 frames = 5s @48kHz.
 * We'll compare the last 96000 frames (2s) split into two 48000 halves. */
    uint64_t total = 240000;
    uint32_t total_samples = (uint32_t)total;

    /* Render using the engine */
    struct RIEngine eng;
    ri_engine_init(&eng);
    ri_engine_defaults(&eng);
    ri_engine_load(&eng, ev, nev, total, 0x1F); /* all sections */

    uint64_t done = 0;
    while (done < total) {
        uint32_t want = (uint32_t)(total - done > 4096 ? 4096 : total - done);
        uint32_t got = ri_engine_render(&eng, L + done, R + done, want, 48000.0f);
        if (got == 0) break;
        done += got;
    }
    /* If engine stopped early, pad with silence */
    if (done < total) {
        for (uint64_t i = done; i < total; i++) {
            L[i] = 0.0f;
            R[i] = 0.0f;
        }
        done = total;
    }

    /* Write WAV file */
    f = fopen(T21L_WAV, "wb");
    CHECK(f != NULL, "wav open");
    if (!f) return 1;

    fwrite("RIFF", 1, 4, f);
    uint32_t fsize = 36u + total_samples * 4u;
    fwrite(&fsize, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    uint32_t fmt_len = 16;
    fwrite(&fmt_len, 4, 1, f);
    uint16_t fmt_tag = 1;
    fwrite(&fmt_tag, 2, 1, f);
    uint16_t channels = 2;
    fwrite(&channels, 2, 1, f);
    uint32_t sample_rate = 48000;
    fwrite(&sample_rate, 4, 1, f);
    uint32_t byte_rate = 48000 * 4;
    fwrite(&byte_rate, 4, 1, f);
    uint16_t block_align = 4;
    fwrite(&block_align, 2, 1, f);
    uint16_t bits = 16;
    fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f);
    uint32_t data_size = total_samples * 4u;
    fwrite(&data_size, 4, 1, f);
    for (i = 0; i < (int)total_samples; i++) {
        int16_t l = (int16_t)(L[i] * 32767.0f);
        int16_t r = (int16_t)(R[i] * 32767.0f);
        fwrite(&l, 2, 1, f);
        fwrite(&r, 2, 1, f);
    }
    fclose(f);
    printf("WAV file written, expected size: %u\n", T21L_WANT_SIZE);

    /* Compare the last 2 seconds (96000 frames) split into two 48000-frame halves.
     * WAV: 44 byte header + 240000 frames * 4 bytes = 960044 bytes total.
     * Last 96000 frames start at frame 144000 = byte offset 44 + 144000*4 = 576044. */
    uint32_t loop_start = 44 + 144000 * 4;
    uint32_t half_bytes = T21L_HALF_BYTES;

    /* Compare loop halves */
    f = fopen(T21L_WAV, "rb");
    CHECK(f != NULL, "wav open");
    if (!f) return 1;
    fseek(f, 0, SEEK_END);
    fsize1 = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("WAV file actual size: %ld, expected: %u\n", fsize1, T21L_WANT_SIZE);
    CHECK(fsize1 == (long)T21L_WANT_SIZE, "wav size %ld want %u", fsize1, T21L_WANT_SIZE);
    nr2 = fread(pcm, 1, sizeof pcm, f);
    fclose(f);
    printf("Read %zu bytes, expected %zu\n", nr2, (size_t)fsize1);
    CHECK(nr2 == (size_t)fsize1, "short read");
    if (nr2 != (size_t)fsize1) return 1;

    if (memcmp(pcm + loop_start, pcm + loop_start + half_bytes, half_bytes) != 0) {
        uint32_t first = 0xFFFFFFFFu;
        uint32_t maxd = 0u;
        const unsigned char *a = pcm + 44;
        const unsigned char *b = pcm + 44 + T21L_HALF_BYTES;
        uint32_t j;
        for (j = 0u; j < T21L_HALF_BYTES; j += 2u) {
            int16_t sa = (int16_t)(a[j] | ((uint16_t)a[j + 1u] << 8));
            int16_t sb = (int16_t)(b[j] | ((uint16_t)b[j + 1u] << 8));
            uint32_t d = (uint32_t)(sa > sb ? sa - sb : sb - sa);
            if (d > maxd) {
                maxd = d;
                if (first == 0xFFFFFFFFu)
                    first = j / 2u;
            }
        }
        printf("FAIL loop halves differ: max abs diff %u, first at sample %u\n",
               maxd, first);
        fails++;
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_looppcm\n");
    return fails != 0;
}