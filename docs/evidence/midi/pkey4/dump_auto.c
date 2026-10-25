#include <stdio.h>
#include <string.h>
#include "project/rbng.h"
int main(void) {
    static struct RISong s;
    static struct RBAutoEv atrk[8192];
    char err[256];
    uint32_t i;
    rbng_song_init(&s);
    s.atrk = atrk; s.atrk_cap = 8192;
    if (rbng_read_song("songs/local/zombie-nation/zombie-nation.rbng", &s, err, sizeof err)) {
        printf("READ FAIL %s\n", err); return 1;
    }
    printf("natrk %u\n", s.natrk);
    for (i = 0; i < s.natrk; i++)
        if (s.atrk[i].tick == 0)
            printf("tick %u ctl 0x%04x val %u\n", s.atrk[i].tick, s.atrk[i].ctl, s.atrk[i].val);
    return 0;
}
