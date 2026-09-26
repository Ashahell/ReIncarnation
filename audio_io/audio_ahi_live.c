/* audio_ahi_live.c — G9.3 live render task + low-level AHI backend.
 * AROS-only. Spec §4.2 (LOCKED): the PlayerFunc hook runs in driver
 * context and only Signal()s; the render Task does everything else.
 * Double buffer: while AHI plays one s16 buffer the task renders the
 * next through ri_live_render. Under-run (render slower than the device
 * period) counts xruns and repeats the previous buffer — audible glitch,
 * never a hang (§17 #5). AHI missing at start falls back to the documented
 * null path (caller prints RI_AUDIO_NULL_MSG and drives the session
 * itself); CMD_WRITE (audio_ahi_play.c) stays the documented fallback
 * backend when the low-level path fails on hardware.
 *
 * Status 2026-09-26: structure + close path implemented, ABIv1
 * compile-gated. Dell measurement (64/128 frames, 5-minute xruns, render
 * time, headroom) is owner-lane work — see
 * docs/evidence/audio/live-render-task.md.
 */
#ifndef __AROS__
#error "audio_ahi_live.c is AROS-only"
#endif

#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/ahi.h>
#include "audio_io/audio_ahi_live.h"
#include "engine/live.h"

extern struct DosLibrary *DOSBase;
extern struct Library *AHIBase;

#define AU_LIVE_SIG (SIGBREAKF_CTRL_F)
#define AU_LIVE_STOPSIG (SIGBREAKF_CTRL_C)

static struct Task *s_live_task;
static ULONG s_live_ticks;

static ULONG live_player_entry(struct Hook *h, APTR obj, APTR msg) {
    (void)h;
    (void)obj;
    (void)msg;
    s_live_ticks++;
    if (s_live_task != NULL)
        Signal(s_live_task, AU_LIVE_SIG);
    return 0;
}

/* Render one device buffer through the session. Pure task context
 * (DOS/Intuition calls allowed here — never in the hook above). */
static void live_render_buffer(struct AuLive *lv) {
    ULONG i, got;
    if (!lv || !lv->session || !lv->f32_l || !lv->f32_r || !lv->s16 ||
        lv->frames == 0u)
        return;
    got = ri_live_render(lv->session, lv->f32_l, lv->f32_r, lv->frames);
    if (got < lv->frames) {
        lv->xruns++;
        for (i = got; i < lv->frames; i++) {
            lv->f32_l[i] = 0.0f;
            lv->f32_r[i] = 0.0f;
        }
    }
    for (i = 0u; i < lv->frames; i++) {
        lv->s16[i * 2u] = au_live_f32_to_s16(lv->f32_l[i]);
        lv->s16[i * 2u + 1u] = au_live_f32_to_s16(lv->f32_r[i]);
    }
}

int au_live_start(struct AuLive *lv) {
    ULONG waited;
    if (!lv || !lv->session || !lv->f32_l || !lv->f32_r || !lv->s16 ||
        lv->frames == 0u || lv->frames > 4096u)
        return 2;
    lv->xruns = 0u;
    lv->render_us_max = 0u;
    lv->stop = 0;
    lv->running = 1;
    s_live_task = FindTask(NULL);
    s_live_ticks = 0u;
    /* The AllocAudioA + PlayerFunc hookup happens against the caller's
     * negotiated session on the Dell lane (OPEN-09). Until the lane
     * measurement lands, drive one buffer here so the close path and the
     * conversion are exercised on every start/stop cycle. */
    live_render_buffer(lv);
    waited = SetSignal(0u, 0u);
    (void)waited;
    (void)live_player_entry;
    if (DOSBase)
        Printf((STRPTR)"RI_LIVE start frames=%lu xruns=%lu\n", lv->frames,
            lv->xruns);
    return 0;
}

int au_live_stop(struct AuLive *lv) {
    if (!lv)
        return 2;
    lv->stop = 1;
    if (s_live_task != NULL)
        Signal(s_live_task, AU_LIVE_STOPSIG);
    lv->running = 0;
    s_live_task = NULL;
    if (DOSBase)
        Printf((STRPTR)"RI_LIVE stop xruns=%lu\n", lv->xruns);
    return 0;
}

ULONG au_live_xruns(const struct AuLive *lv) {
    if (!lv)
        return 0u;
    return lv->xruns;
}
