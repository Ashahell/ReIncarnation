/* tools/rbsc — RBS song script compiler (songs & playlists, owner
 * 2026-09-30). Parses a song script (project/songscript.h) and writes an
 * RBNG song through the real codec, then reads it back to prove it
 * loads. Deterministic: the same script gives the same bytes.
 *
 * usage: rbsc IN.rbs OUT.rbng
 * exit: 0 ok, 2 usage / IO / parse / codec error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project/songscript.h"

#define RBSC_MAXAUTO 65536u

static struct RISong song, back;
static struct RBAutoEv ev[RBSC_MAXAUTO], evb[RBSC_MAXAUTO];

int main(int argc, char **argv) {
    static char text[4u << 20];
    static char err[256];
    FILE *f;
    size_t n;
    uint32_t b, bars = 0u, i;
    if (argc != 3) {
        printf("usage: rbsc IN.rbs OUT.rbng\n");
        return 2;
    }
    if (!(f = fopen(argv[1], "rb"))) {
        printf("rbsc: cannot open %s\n", argv[1]);
        return 2;
    }
    n = fread(text, 1, sizeof text - 1u, f);
    fclose(f);
    if (n >= sizeof text - 1u) {
        printf("rbsc: %s too large\n", argv[1]);
        return 2;
    }
    text[n] = '\0';
    rbng_song_init(&song);
    song.atrk = ev;
    song.atrk_cap = RBSC_MAXAUTO;
    if (ri_rbs_parse(text, &song, err, sizeof err) != 0) {
        printf("rbsc: %s: %s\n", argv[1], err);
        return 2;
    }
    if (rbng_write_song(argv[2], &song, err, sizeof err) != 0) {
        printf("rbsc: write %s: %s\n", argv[2], err);
        return 2;
    }
    rbng_song_init(&back);
    back.atrk = evb;
    back.atrk_cap = RBSC_MAXAUTO;
    if (rbng_read_song(argv[2], &back, err, sizeof err) != 0) {
        printf("rbsc: read-back %s: %s\n", argv[2], err);
        return 2;
    }
    for (b = 0u; b < RI_SONGTRACK_BARS; b++)
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            if (back.track.slot[b][i])
                bars = b + 1u;
    printf("rbsc: %s -> %s: tempo %u, %u banks, %u automation events, track through bar %u\n", argv[1], argv[2],
        (unsigned)back.tempo, (unsigned)back.nbanks, (unsigned)back.natrk, (unsigned)bars);
    return 0;
}
