/* live_driver.h — portable live driver (portability plan T4).
 * Owns the double-buffer policy, f32->s16 conversion, xrun accounting,
 * the transport request word and W capture. The backend only provides
 * the pull cadence (AHI hook, null timer, WASAPI event) and calls
 * ri_livedrv_render once per device buffer.
 * Pure C99, no OS calls; time comes from an injected now_us clock
 * (AROS EClock wrapper, host test clock; ri_time_us PAL lands in T8).
 * All storage caller-owned; bounded loops; no allocation.
 */
#ifndef RI_LIVE_DRIVER_H
#define RI_LIVE_DRIVER_H
#include <stdint.h>

#include "engine/live.h"
#include "platform/pal/ri_pal_thread.h"

#define RI_LIVE_CMD_NONE 0
#define RI_LIVE_CMD_PLAY 1
#define RI_LIVE_CMD_STOP 2

struct RILiveDriver {
    struct RILiveSession *session; /* caller-owned, init at device rate */
    uint32_t frames;               /* device buffer, 64..4096 */
    uint64_t (*now_us)(void);      /* backend monotonic clock (nullable) */
    ri_atomic_u32 cmd;             /* GUI -> render transport request */
    ri_atomic_u32 xruns;           /* backend-observed late buffers */
    ri_atomic_u32 buffers;         /* buffers rendered */
    ri_atomic_u32 render_us_max;   /* slowest buffer render, microseconds */
    ri_atomic_u32 render_us_sum_ms; /* total render time, milliseconds */
    uint64_t us_acc;               /* task-side microsecond accumulator */
    /* W capture: task copies each rendered s16 half here while cap_on;
     * the owner writes the WAV after turning cap_on off (no IO here). */
    int16_t *cap_buf;              /* interleaved stereo s16, owner buffer */
    uint32_t cap_max;              /* capacity, frames */
    ri_atomic_u32 cap_pos;         /* frames captured */
    ri_atomic_u32 cap_on;
    /* Load governor (owner Dell 2026-09-30: a render task above the UI
     * that cannot keep up never sleeps and freezes every lower task,
     * menus included; only the pointer moves). Load = render time over
     * the buffer period, per mille, smoothed 1/8. At RI_LIVEDRV_OVER_PM
     * the backend drops the render task below the UI for
     * RI_LIVEDRV_OVER_US, then probes again at its normal priority. */
    uint32_t load_pm;              /* task side: smoothed load, per mille */
    uint64_t over_left_us;         /* task side: overload time remaining */
    ri_atomic_u32 overloaded;      /* 1 while the backend should yield */
    ri_atomic_u32 overloads;       /* overload entries (heartbeat) */
};

#define RI_LIVEDRV_OVER_PM 850u
#define RI_LIVEDRV_OVER_US 2000000u

void ri_livedrv_init(struct RILiveDriver *d, struct RILiveSession *s,
    uint32_t frames, uint64_t (*now_us)(void));
/* Transport request, applied at the next buffer start. */
void ri_livedrv_request(struct RILiveDriver *d, int cmd);
/* Backend-observed under-run (device repeated a buffer). */
void ri_livedrv_report_late(struct RILiveDriver *d, uint32_t n);
/* Render exactly one device buffer of stereo s16 into out[2*frames].
 * scratch_fl/scratch_fr are caller float buffers of >= frames. */
void ri_livedrv_render(struct RILiveDriver *d, int16_t *out,
    float *scratch_fl, float *scratch_fr, uint32_t frames);

/* Governor verdict for the backend (1 = run below the UI). */
int ri_livedrv_overloaded(struct RILiveDriver *d);

/* Deterministic f32 -> s16 (twin of the AROS backend converter):
 * clamp, round-half-away, no libm. */
int16_t ri_livedrv_f32_to_s16(float x);

#endif
