#include <stdio.h>
#include <string.h>
#include "project/rbng.h"
int main(void) {
    static struct RISong s;
    static struct RBAutoEv atrk[4096];
    char err[256];
    int b, p, r;
    rbng_song_init(&s);
    s.atrk = atrk; s.atrk_cap = 4096;
    if (rbng_read_song("songs/local/zombie-nation/zombie-nation.rbng", &s, err, sizeof err)) {
        printf("READ FAIL %s\n", err); return 1;
    }
    printf("tempo %u ppq %u bars track inst=%u\n", s.tempo, s.ppq, (unsigned)RI_SONGTRACK_INSTANCES);
    for (b = 0; b < s.nbanks; b++) {
        struct RIPatternBank *bk = &s.bank[b];
        printf("bank %d kind %u drum_class %u\n", b, bk->kind, bk->drum_class);
        if (bk->kind != RI_PATTERN_KIND_LEVI) continue;
        for (p = 0; p < 32; p++) {
            struct RIPattern *pt = &bk->pat[p];
            int used = 0, maxnotes = 0;
            for (r = 0; r < 16; r++) {
                int n = 0, L;
                for (L = 0; L < RI_LEVI_LANES; L++) if (pt->row.levi[r].note[L]) n++;
                if (pt->row.levi[r].on) used++;
                if (n > maxnotes) maxnotes = n;
            }
            if (!used) continue;
            printf("  pat %2d used %2d maxnotes %d\n", p, used, maxnotes);
            for (r = 0; r < 16; r++) {
                int L; printf("    r%02d on=%02x notes:", r, pt->row.levi[r].on);
                for (L = 0; L < RI_LEVI_LANES; L++) printf(" %3u", pt->row.levi[r].note[L]);
                printf("\n");
            }
        }
    }
    /* which patterns the track selects for the levi instance */
    return 0;
}
