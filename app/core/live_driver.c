/* live_driver.c — portable live driver bodies (portability plan T4).
 * Mirrors audio_io/audio_ahi_live.c render_half minus AHI/EClock.
 */
#include "app/core/live_driver.h"

int16_t ri_livedrv_f32_to_s16(float x) {
    float c = (x > 1.0f) ? 1.0f : ((x < -1.0f) ? -1.0f : x);
    float s = c * 32767.0f;
    if (s >= 0.0f)
        return (int16_t)(s + 0.5f);
    return (int16_t)(s - 0.5f);
}

void ri_livedrv_init(struct RILiveDriver *d, struct RILiveSession *s,
    uint32_t frames, uint64_t (*now_us)(void)) {
    if (!d)
        return;
    d->session = s;
    d->frames = (frames >= 64u && frames <= 4096u) ? frames : 256u;
    d->now_us = now_us;
    d->cmd.v = (uint32_t)RI_LIVE_CMD_NONE;
    d->xruns.v = 0u;
    d->buffers.v = 0u;
    d->render_us_max.v = 0u;
    d->render_us_sum_ms.v = 0u;
    d->us_acc = 0u;
    d->cap_buf = 0;
    d->cap_max = 0u;
    d->cap_pos.v = 0u;
    d->cap_on.v = 0u;
    d->load_pm = 0u;
    d->over_run_us = 0u;
    d->over_left_us = 0u;
    d->overloaded.v = 0u;
    d->overloads.v = 0u;
    /* Init on the owning task before sharing; plain stores safe here. */
}

void ri_livedrv_request(struct RILiveDriver *d, int cmd) {
    if (d)
        ri_atomic_store_rel(&d->cmd, (uint32_t)cmd);
}

void ri_livedrv_report_late(struct RILiveDriver *d, uint32_t n) {
    if (d && n)
        ri_atomic_fetch_add_rel(&d->xruns, n);
}

int ri_livedrv_overloaded(struct RILiveDriver *d) {
    return d ? (int)ri_atomic_load_acq(&d->overloaded) : 0;
}

/* One buffer's timing into the governor (task side). */
static void governor(struct RILiveDriver *d, uint32_t us, uint32_t frames) {
    uint64_t period;
    uint32_t load;
    if (!d->session || !(d->session->sr > 0.0f))
        return;
    period = (uint64_t)((float)frames * 1000000.0f / d->session->sr);
    if (period == 0u)
        return;
    /* One buffer counts at most 1.2x busy: a stall from outside (the Dell
     * blocks audio ~50-84 ms while a tab page repaints) must not trip the
     * governor on its own; only renders that stay over budget do. */
    load = ((uint64_t)us * 1000u / period > RI_LIVEDRV_LOAD_CAP_PM) ? RI_LIVEDRV_LOAD_CAP_PM :
        (uint32_t)((uint64_t)us * 1000u / period);
    d->load_pm = (d->load_pm * 7u + load) / 8u;
    if (ri_atomic_load_acq(&d->overloaded)) {
        if (d->over_left_us > period) {
            d->over_left_us -= period;
            return;
        }
        d->over_left_us = 0u;
        d->load_pm = 0u; /* probe afresh at normal priority */
        d->over_run_us = 0u; /* so the next entry needs its own arm */
        ri_atomic_store_rel(&d->overloaded, 0u);
        return;
    }
    /* Continuous over budget only (owner Dell 2026-10-01). Playing on the
     * Dell runs at 585-638 per mille with spikes from the UI, and every
     * crossing of the threshold put the render task below the UI, where a
     * repaint costs 1-2 buffers (485 and 923 xruns in two windows, 9 and
     * 13 entries). A peak is not a machine that cannot keep up; the load
     * has to stay over RI_LIVEDRV_OVER_PM for RI_LIVEDRV_ARM_US. */
    if (d->load_pm < RI_LIVEDRV_OVER_PM) {
        d->over_run_us = 0u;
        return;
    }
    d->over_run_us += period;
    if (d->over_run_us < RI_LIVEDRV_ARM_US)
        return;
    d->over_run_us = 0u;
    d->over_left_us = RI_LIVEDRV_OVER_US;
    ri_atomic_store_rel(&d->overloaded, 1u);
    ri_atomic_fetch_add_rel(&d->overloads, 1u);
}

void ri_livedrv_render(struct RILiveDriver *d, int16_t *out,
    float *scratch_fl, float *scratch_fr, uint32_t frames) {
    uint32_t i, n, pos, k;
    int cmd;
    uint64_t t0 = 0u, t1 = 0u;
    if (!d || !out || !scratch_fl || !scratch_fr || frames == 0u)
        return;
    if (frames > d->frames)
        frames = d->frames;
    cmd = (int)ri_atomic_load_acq(&d->cmd);
    if (cmd != RI_LIVE_CMD_NONE) {
        ri_atomic_store_rel(&d->cmd, (uint32_t)RI_LIVE_CMD_NONE);
        if (d->session) {
            if (cmd == RI_LIVE_CMD_PLAY)
                ri_live_play(d->session);
            else if (cmd == RI_LIVE_CMD_STOP)
                ri_live_stop(d->session);
        }
    }
    if (d->now_us)
        t0 = d->now_us();
    if (d->session) {
        uint32_t got = ri_live_render(d->session, scratch_fl, scratch_fr, frames);
        for (i = got; i < frames; i++)
            scratch_fl[i] = scratch_fr[i] = 0.0f;
    } else {
        for (i = 0u; i < frames; i++)
            scratch_fl[i] = scratch_fr[i] = 0.0f;
    }
    for (i = 0u; i < frames; i++) {
        out[i * 2u] = ri_livedrv_f32_to_s16(scratch_fl[i]);
        out[i * 2u + 1u] = ri_livedrv_f32_to_s16(scratch_fr[i]);
    }
    if (ri_atomic_load_acq(&d->cap_on) && d->cap_buf) {
        n = frames;
        pos = ri_atomic_load_acq(&d->cap_pos);
        if (n > d->cap_max - pos)
            n = d->cap_max - pos;
        for (k = 0u; k < n * 2u; k++)
            d->cap_buf[pos * 2u + k] = out[k];
        ri_atomic_store_rel(&d->cap_pos, pos + n);
        if (ri_atomic_load_acq(&d->cap_pos) >= d->cap_max)
            ri_atomic_store_rel(&d->cap_on, 0u);
    }
    if (d->now_us) {
        uint32_t us;
        t1 = d->now_us();
        us = (uint32_t)(t1 - t0);
        if (us > ri_atomic_load_acq(&d->render_us_max))
            ri_atomic_store_rel(&d->render_us_max, us);
        d->us_acc += us;
        ri_atomic_store_rel(&d->render_us_sum_ms, (uint32_t)(d->us_acc / 1000ULL));
        governor(d, us, frames);
    }
    ri_atomic_fetch_add_rel(&d->buffers, 1u);
}
