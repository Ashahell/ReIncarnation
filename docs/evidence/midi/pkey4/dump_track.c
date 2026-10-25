#include <stdio.h>
#include <string.h>
#include "project/rbng.h"
int main(void) {
    static struct RISong s;
    static struct RBAutoEv atrk[4096];
    char err[256];
    int bar, inst;
    rbng_song_init(&s);
    s.atrk = atrk; s.atrk_cap = 4096;
    if (rbng_read_song("songs/local/zombie-nation/zombie-nation.rbng", &s, err, sizeof err)) {
        printf("READ FAIL %s\n", err); return 1;
    }
    printf("instances %u bars %u\n", (unsigned)RI_SONGTRACK_INSTANCES, (unsigned)RI_SONGTRACK_BARS);
    for (inst = 0; inst < (int)RI_SONGTRACK_INSTANCES; inst++) {
        int first=-1, last=-1, used=0;
        for (bar = 0; bar < (int)RI_SONGTRACK_BARS; bar++) {
            if (s.track.slot[bar][inst]) { used++; if(first<0)first=bar; last=bar; }
        }
        printf("inst %d used %d first %d last %d\n", inst, used, first, last);
    }
    /* print levi instance slots for first 30 used bars */
    inst = 4;
    { int printed=0;
      for (bar = 0; bar < (int)RI_SONGTRACK_BARS && printed < 40; bar++) {
        if (s.track.slot[bar][inst]) { printf("bar %3d -> pat %u\n", bar, s.track.slot[bar][inst]); printed++; }
      }
    }
    return 0;
}
