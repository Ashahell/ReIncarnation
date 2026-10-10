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
    d->wake_us_max.v = 0u;
    d->wake_us_sum_ms.v = 0u;
    d->wake_n.v = 0u;
    d->prio_now.v = 0u;
    d->us_acc = 0u;
    d->wake_acc = 0u;
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

/* Wake latency: the backend calls this as it begins a buffer, with the
 * microseconds between its wake source firing and the render starting, and
 * the priority the task held at that moment. This is the metric that
 * separates "late" (the task was not scheduled in time) from "slow" (the
 * render itself took too long) — render_us_max alone cannot tell those
 * apart. Task side only; the GUI reads the atomics. */
void ri_livedrv_report_wake(struct RILiveDriver *d, uint32_t us, uint32_t prio) {
    if (!d)
        return;
    if (us > ri_atomic_load_acq(&d->wake_us_max))
        ri_atomic_store_rel(&d->wake_us_max, us);
    ri_atomic_store_rel(&d->prio_now, prio);
    d->wake_acc += us;
    ri_atomic_store_rel(&d->wake_us_sum_ms, (uint32_t)(d->wake_acc / 1000ULL));
    ri_atomic_fetch_add_rel(&d->wake_n, 1u);
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
    /* Clamped only to keep the smoothing in range; an outside stall cannot
     * trip the governor because the trip needs RI_LIVEDRV_ARM_US of
     * continuous over-budget load (one mechanism, review 2026-10-05). */
    load = ((uint64_t)us * 1000u / period > 4000u) ? 4000u : (uint32_t)((uint64_t)us * 1000u / period);
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

/* R6d: the engine's note tap -> the byte producer -> the ring.
 *
 * BOUNDED, and bounded for a reason the tap's own bounds do not cover: a
 * caller that attached a producer and then stopped draining must not turn
 * into unbounded per-block work. 32 notes is far above one buffer's worth
 * (a 256-frame block at 48 kHz is 5.3 ms, which at 140 BPM is under one
 * sixteenth) and the rest waits for the next block rather than being lost.
 *
 * The late accent emits nothing and is counted. See ri_devout_record(). */
#define RI_LIVEDRV_DEVBUDGET 32u

static void livedrv_devout(struct RILiveDriver *d) {
    struct RINoteTapRec rec[RI_LIVEDRV_DEVBUDGET];
    uint32_t got, i;
    if (!d || !d->dev_out || !d->clk_out || !d->session)
        return;                 /* off by default: not one byte leaves */
    got = ri_engine_note_read(&d->session->eng, rec, RI_LIVEDRV_DEVBUDGET);
    for (i = 0u; i < got; i++) {
        uint8_t buf[3];
        uint32_t w;
        if (rec[i].kind == RI_NOTEK_LATE_ACCENT) {
            d->late_accents++;
            continue;
        }
        /* The MELODIC channel is the user's choice and is refused when
         * unassigned -- never defaulted, and refused HERE rather than
         * downstream because ri_devout_note() clamps rather than refuses.
         * The DRUM channel is not a choice at all: GM defines percussion on
         * channel 10, so a user who only wants the 808 out does not have to
         * configure a melodic channel first. */
        if (rec[i].device != RI_DEVOUT_808 && rec[i].device != RI_DEVOUT_909
            && (d->note_ch < 0 || d->note_ch > 15)) {
            d->devout_refused++;
            continue;
        }
        w = ri_devout_record(d->dev_out, buf, sizeof buf,
            (uint8_t)d->note_ch, &rec[i]);
        if (w)
            midi_out_put(d->clk_out, buf, w);   /* all-or-nothing */
    }
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
        /* A transport edge that never reaches the wire makes a clock
         * meaningless, so the producer is told here. The render still only
         * fills a ring: no send, no CAMD. */
        if (d->clk_out) {
            if (cmd == RI_LIVE_CMD_PLAY)
                midi_out_start(d->clk_out, d->session
                    ? d->session->sample_cursor : 0u);
            else if (cmd == RI_LIVE_CMD_STOP)
                midi_out_stop(d->clk_out);
        }
    }
    if (d->now_us)
        t0 = d->now_us();
    if (d->session) {
        uint32_t got = ri_live_render(d->session, scratch_fl, scratch_fr, frames);
        /* M5: the clock-out schedule is driven by the AUDIO clock, so it is
         * fed here and nowhere else. `sample_cursor` is the position the
         * audio just reached, which is why the ticks cannot drift from it.
         */
        if (d->clk_out)
            midi_out_render(d->clk_out, d->session->sample_cursor);
        /* R6d, after the clock render and after any FA the transport edge
         * put in above: a note must never precede the start that gives it
         * a tempo. */
        livedrv_devout(d);
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
