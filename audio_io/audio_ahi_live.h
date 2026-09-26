/* audio_ahi_live.h — G9.3 live render task + low-level AHI backend.
 * AROS-only. Structure per spec §4.2: PlayerFunc hook only Signal()s,
 * a dedicated render Task Wait()s, renders exactly the device buffer
 * through ri_live_render into caller-owned double buffers, converts f32
 * stereo to negotiated 16-bit with deterministic rounding, hands it to
 * AHI. Under-runs counted (§17 #5), never a hang. Clean stop releases
 * everything (WBS 2.7 close-path lesson).
 */
#ifndef RI_AUDIO_AHI_LIVE_H
#define RI_AUDIO_AHI_LIVE_H

#ifndef __AROS__
#error "audio_ahi_live.h is AROS-only"
#endif

#include <exec/types.h>

struct RILiveSession; /* engine/live.h (opaque here) */

/* Caller-owned live backend. All buffers are caller storage (no alloc):
 * f32 stereo pair (frames each) + s16 interleaved (frames*2). frames is
 * the negotiated device buffer (64..4096, multiple of 64 where possible;
 * OPEN-09 measures 64/128 on the Dell). */
struct AuLive {
    struct RILiveSession *session;
    float *f32_l;
    float *f32_r;
    WORD *s16;
    ULONG frames;
    ULONG xruns;
    ULONG render_us_max;
    volatile int stop;
    volatile int running;
};

int au_live_start(struct AuLive *lv);
int au_live_stop(struct AuLive *lv);
ULONG au_live_xruns(const struct AuLive *lv);
/* Deterministic f32 -> s16 twin of auf_f32_to_s16 (audio.c): clamp,
 * round-half-away, no libm. */
static inline WORD au_live_f32_to_s16(float x) {
    float c = (x > 1.0f) ? 1.0f : ((x < -1.0f) ? -1.0f : x);
    float s = c * 32767.0f;
    if (s >= 0.0f)
        return (WORD)(s + 0.5f);
    return (WORD)(s - 0.5f);
}

#endif
