/* t21_looppcm — TC-2.1.2 PCM half: loop-region double render equality.
 * 16-step song at tempo 120 (halves identical: pluck at steps 0 and 8,
 * rests elsewhere) rendered once through the file path; PCM bytes of
 * loop iteration 1 [44,44+192000) vs iteration 2 [44+192000,44+384000)
 * must be byte-identical (8 steps * 12000 samples * 2 B @48k).
 * On mismatch the test reports max abs int16 diff + first diff offset
 * (characterizes state carryover: settle-gap song vs loop-reset design)
 * and fails. File under /tmp/ri/run/t21 (audit-created).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "audio_io/audio.h"

#define T21L_DIR "/tmp/ri/run/t21"
#define T21L_SONG T21L_DIR "/loop.rbng"
#define T21L_WAV T21L_DIR "/loop.wav"
/* 16th = 24 ticks = 6000 samples @120 BPM/48 kHz; 8-step loop = 48000.
 * File total = last NOTE_OFF (step 9 gate-fraction @51000, D-h §12.4)
 * + 1 s tail (48000). */
#define T21L_HALF_SAMPLES 48000u
#define T21L_HALF_BYTES (T21L_HALF_SAMPLES * 2u)
#define T21L_WANT_SIZE (44u + (51000u + 48000u) * 2u)

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static const char *k_lines[] = {
    "tempo=120",
    "step note=45",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step note=45",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    "step rest=1",
    NULL
};

int main(void) {
    FILE *f;
    struct AudioObject *ao;
    struct TagItem song_tag[2];
    uint32_t src, bus;
    int i, rc;
    long fsize;
    static unsigned char pcm[400000];
    size_t nr;

    f = fopen(T21L_SONG, "w");
    CHECK(f != NULL, "song open");
    if (!f)
        return 1;
    for (i = 0; k_lines[i]; i++)
        fprintf(f, "%s\n", k_lines[i]);
    fclose(f);

    ao = AuCreateObject(NULL, NULL);
    CHECK(ao != NULL, "AuCreateObject NULL");
    if (!ao)
        return 1;
    song_tag[0].ti_Tag = AUTA_SongPath;
    song_tag[0].ti_Data = (uintptr_t)T21L_SONG;
    song_tag[1].ti_Tag = 0;
    song_tag[1].ti_Data = 0;
    src = AuAddSource(ao, song_tag);
    CHECK(src != 0, "AuAddSource");
    bus = AuAddBus(ao, "master", NULL);
    CHECK(bus != 0, "AuAddBus");
    CHECK(AuConnect(ao, src, bus) == 0, "AuConnect");
    rc = AuStart(ao);
    CHECK(rc == 0, "AuStart rc %d", rc);
    CHECK(AuRenderToFile(ao, T21L_WAV, 0) == 0, "AuRenderToFile");
    AuStop(ao);

    f = fopen(T21L_WAV, "rb");
    CHECK(f != NULL, "wav open");
    if (!f)
        return 1;
    fseek(f, 0, SEEK_END);
    fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    CHECK(fsize == (long)T21L_WANT_SIZE, "wav size %ld want %u", fsize,
          T21L_WANT_SIZE);
    nr = fread(pcm, 1, sizeof pcm, f);
    fclose(f);
    CHECK(nr == (size_t)fsize, "short read");
    if (nr != (size_t)fsize || fsize != (long)T21L_WANT_SIZE)
        return 1;

    if (memcmp(pcm + 44, pcm + 44 + T21L_HALF_BYTES, T21L_HALF_BYTES) != 0) {
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
