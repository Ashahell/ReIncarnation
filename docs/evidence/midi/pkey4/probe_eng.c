#include <stdio.h>
#include <string.h>
#include "engine/engine.h"
#include "engine/seq/sched.h"
#include "project/rbng.h"
#define SR 48000.0f
#define N 4800u
static struct RIEngine e;
static struct RISong song;
static struct RBAutoEv atrk[8192];
static float L[N], R[N];
static void apply_song_auto(struct RIEngine *en) {
    uint32_t i;
    for (i = 0; i < song.natrk; i++) {
        struct RIEvent ev;
        if (song.atrk[i].tick != 0) continue;
        memset(&ev, 0, sizeof ev);
        ev.type = RI_EV_AUTOMATION;
        ev.value = song.atrk[i].ctl;
        ev.flags = song.atrk[i].val & 127u;
        ri_engine_apply_event(en, &ev);
    }
}
static void stats(const char *name) {
    uint32_t i; double p = 0, s = 0;
    for (i = 0; i < N; i++) {
        double a = L[i] < 0 ? -L[i] : L[i];
        double b = R[i] < 0 ? -R[i] : R[i];
        if (a > p) p = a;
        if (b > p) p = b;
        s += (double)L[i]*L[i] + (double)R[i]*R[i];
    }
    printf("%-22s peak %.9f rms %.9f\n", name, p, __builtin_sqrt(s/(2.0*N)));
}
static void render_fixed(struct RIEngine *en) {
    uint32_t got = ri_engine_render(en, L, R, N, SR);
    printf("  (rendered %u)\n", got);
}
int main(void) {
    char err[256];
    struct RIEvent ev[8];
    rbng_song_init(&song);
    song.atrk = atrk; song.atrk_cap = 8192;
    if (rbng_read_song("songs/local/zombie-nation/zombie-nation.rbng", &song, err, sizeof err)) {
        printf("READ FAIL %s\n", err); return 1;
    }
    /* A: live note through the control-plane event shape */
    ri_engine_init(&e); apply_song_auto(&e);
    { struct RIEvent a; memset(&a,0,sizeof a); a.type=RI_EV_AUTOMATION;
      a.value=RI_CTL_LEVI_NOTE; a.flags=(uint16_t)(((0x80u|60u)<<8)|127u);
      ri_engine_apply_event(&e, &a); }
    ri_engine_load(&e, 0, 0, N, RI_ENGINE_SLEVI);
    render_fixed(&e); stats("live note 60 vel127");

    /* B: song note on lane 0 */
    ri_engine_init(&e); apply_song_auto(&e);
    memset(ev, 0, sizeof ev);
    ev[0].sample=0; ev[0].type=RI_EV_NOTE_ON; ev[0].device=4; ev[0].voice=0; ev[0].value=60;
    ri_engine_load(&e, ev, 1, N, RI_ENGINE_SLEVI);
    render_fixed(&e); stats("song note 60 lane0");

    /* C: song chord (pattern 1 row 4: 46,73,34,61) */
    ri_engine_init(&e); apply_song_auto(&e);
    memset(ev, 0, sizeof ev);
    { uint8_t nn[4]={46,73,34,61}; int i;
      for (i=0;i<4;i++){ ev[i].sample=0; ev[i].type=RI_EV_NOTE_ON; ev[i].device=4; ev[i].voice=(uint16_t)i; ev[i].value=nn[i]; } }
    ri_engine_load(&e, ev, 4, N, RI_ENGINE_SLEVI);
    render_fixed(&e); stats("song chord x4");
    return 0;
}
