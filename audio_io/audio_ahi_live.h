/* audio_ahi_live.h — G9.3 live render task + low-level AHI backend.
 * AROS-only. Spec §4.2 (LOCKED): the AHI hook runs in driver context and
 * only Signal()s; a dedicated render Task owns AHI and the session's render
 * side, Wait()s on the hook signal, renders exactly one device buffer through
 * ri_live_render into the free half of a double buffer (two AHI dynamic
 * sounds), converts f32 stereo to 16-bit with deterministic rounding and
 * queues it (AHI_SetSound, AHISF_NONE). Late buffers are counted as xruns
 * (AHI repeats the last buffer: audible glitch, never a hang — §17 #5).
 * The GUI never touches the session's render state: transport requests and
 * knob moves travel as a volatile request word and the SPSC control plane.
 */
#ifndef RI_AUDIO_AHI_LIVE_H
#define RI_AUDIO_AHI_LIVE_H

#ifndef __AROS__
#error "audio_ahi_live.h is AROS-only"
#endif

#include <exec/types.h>

struct RILiveSession; /* engine/live.h (opaque here) */

#define AU_LIVE_MAXFRAMES 4096u
#define AU_LIVE_CMD_NONE 0
#define AU_LIVE_CMD_PLAY 1
#define AU_LIVE_CMD_STOP 2

/* One live backend (one per process). Fields marked (out) are written by
 * the render task and read by the GUI (word-sized, read-only there). */
struct AuLive {
    ULONG frames;              /* device buffer, 64..AU_LIVE_MAXFRAMES */
    ULONG want_rate;           /* requested mix rate */
    ULONG mix_freq;            /* (out) negotiated mix rate: run the session at this */
    ULONG mode_id;             /* (out) AHI audio mode */
    volatile ULONG xruns;      /* (out) late buffers (AHI repeated one) */
    volatile ULONG buffers;    /* (out) buffers rendered */
    volatile ULONG render_us_max; /* (out) slowest buffer render, microseconds (EClock) */
    volatile ULONG render_us_sum_ms; /* (out) total render time, milliseconds */
    ULONG period_us;           /* (out) device period frames/mix_freq, microseconds */
    volatile LONG cmd;         /* GUI -> task transport request (AU_LIVE_CMD_*) */
    volatile LONG state;       /* 0 idle, 1 negotiated, 2 playing, -1 failed, 3 ended */
    LONG err;                  /* failure step (1 port, 2 device, 3 mode, 4 alloc, 5 load, 6 task, 7 open timeout) */
    struct RILiveSession *session; /* set by au_live_run */
    /* Capture (RIAPP 'W'): the task copies each rendered s16 half here
     * while cap_on; the GUI owns the buffer and writes the WAV after
     * turning cap_on off (no file IO in the render task). */
    WORD *cap_buf;             /* interleaved stereo s16, GUI-allocated */
    ULONG cap_max;             /* capacity, frames */
    volatile ULONG cap_pos;    /* frames captured */
    volatile LONG cap_on;
};

/* Spawn the render task, open ahi.device (AHI_NO_UNIT, device-as-library),
 * pick the best stereo HiFi mode for want_rate, allocate one channel with
 * two dynamic sounds and a SoundFunc hook, read the actual mix rate back.
 * The handshake is bounded (10 s): a missing or misbehaving driver (fault
 * or hang inside the mode scan) returns nonzero and the caller falls back
 * to the null backend; the abandoned task unwinds quietly on its open
 * generation and never touches lv or signals again.
 * Returns 0 ok (lv->mix_freq set, task waiting for au_live_run) or nonzero
 * (nothing held; caller falls back to the null backend). */
int au_live_open(struct AuLive *lv, ULONG frames, ULONG want_rate);
/* Hand the session (initialised at lv->mix_freq) to the task and start
 * the stream. 0 ok. */
int au_live_run(struct AuLive *lv, struct RILiveSession *s);
/* Transport request, applied by the task at the next buffer start. */
void au_live_request(struct AuLive *lv, LONG cmd);
/* Stop the stream, free AHI, end the task; safe after a failed open. */
void au_live_close(struct AuLive *lv);

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
