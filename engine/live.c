/* live.c — G9.2 live session core. See header for the contract.
 * Sample-window inversion: each device buffer [s0,s1) maps to the tick
 * window [t0,t1) with t1 minimal so map(t1) >= s1 (bounded search).
 * Player + automation emit absolute (map-based) samples in [s0,s1);
 * control drains at s0; merged with the §8 sort, rebased by s0, rendered
 * through the one engine. Bit-exact across chunk sizes by construction
 * (absolute positions never depend on chunking).
 */
#include "engine/live.h"
#include <stddef.h>

#define RI_LIVE_T1_BOUND 2048u

static uint64_t live_bar_ticks(uint32_t ppq) {
    return 4u * (uint64_t)ri_ppq_or_default(ppq);
}

void ri_live_init(struct RILiveSession *s, uint32_t ppq, float sr, float bpm,
    uint32_t sections, struct RIEvent *scratch, uint32_t scratch_cap) {
    uint32_t i;
    if (!s)
        return;
    ri_engine_init(&s->eng);
    ri_engine_defaults(&s->eng);
    s->tr.state = RI_TR_STOPPED;
    s->tr.clicks = 0u;
    s->cursor_ticks = 0u;
    s->ppq = ri_ppq_or_default(ppq);
    s->sr = (sr > 0.0f) ? sr : 48000.0f;
    s->bpm = (bpm >= 20.0f && bpm <= 500.0f) ? bpm : 120.0f;
    s->nspq = (uint64_t)(60000000000.0 / (double)s->bpm);
    if (s->nspq == 0u)
        s->nspq = 500000000ULL;
    s->sections = sections;
    s->seg.start_tick = 0u;
    s->seg.ns_per_quarter = s->nspq;
    s->map.segs = &s->seg;
    s->map.n = 1u;
    s->map.ppq = s->ppq;
    s->map.sr = (uint32_t)s->sr;
    if (s->map.sr == 0u)
        s->map.sr = 48000u;
    s->track = 0;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        s->banks[i] = 0;
    s->loop.on = 0u;
    s->loop.start_bar = 0u;
    s->loop.len_bars = 0u;
    s->pub = 0;
    s->carry = 0;
    s->pass = 0;
    s->ctl = 0;
    s->last_front = 0;
    s->need_chase = 0;
    s->scratch = scratch;
    s->scratch_cap = scratch ? scratch_cap : 0u;
    s->qn = 0u;
    s->sample_cursor = 0u;
    s->seq_next = 0u;
    s->xruns = 0u;
    s->tick_rem = 0u;
    for (i = 0u; i < 4u; i++) {
        s->meters.sec_peak[i] = 0.0f;
        s->meters.fx_peak[i] = 0.0f;
    }
    s->meters.comp_gr = 0.0f;
    s->meters.samples = 0u;
    s->meters.cursor_ticks = 0u;
    s->meters.xruns = 0u;
}

void ri_live_set_banks(struct RILiveSession *s,
    const struct RIPatternBank *const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *track, const struct RILoop *loop) {
    uint32_t i;
    if (!s)
        return;
    if (banks) {
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            s->banks[i] = banks[i];
    }
    s->track = track;
    if (loop)
        s->loop = *loop;
    else {
        s->loop.on = 0u;
        s->loop.start_bar = 0u;
        s->loop.len_bars = 0u;
    }
    ri_player_init(&s->player, s->banks, s->track, 0u);
}

void ri_live_set_auto(struct RILiveSession *s, struct RIAutoPub *pub,
    struct RIAutoCarry *carry, struct RIAutoPass *pass) {
    if (!s)
        return;
    s->pub = pub;
    s->carry = carry;
    s->pass = pass;
    s->last_front = pub ? ri_auto_pub_front(pub) : 0;
    if (pub && carry)
        ri_auto_carry_reindex(carry, ri_auto_pub_front(pub),
            (uint32_t)s->cursor_ticks);
}

void ri_live_set_ctl(struct RILiveSession *s, struct RIControlPlane *ctl) {
    if (!s)
        return;
    s->ctl = ctl;
}

void ri_live_play(struct RILiveSession *s) {
    if (!s)
        return;
    ri_tr_play(&s->tr, &s->cursor_ticks);
    s->need_chase = 1;
}

void ri_live_stop(struct RILiveSession *s) {
    uint64_t loop_start = 0u;
    int was_record;
    if (!s)
        return;
    was_record = (s->tr.state == RI_TR_RECORD);
    if (s->loop.on && s->loop.len_bars > 0u)
        loop_start = (uint64_t)s->loop.start_bar * live_bar_ticks(s->ppq);
    ri_tr_stop(&s->tr, &s->cursor_ticks, loop_start, 0u);
    if (was_record && s->pub && s->pass) {
        ri_auto_pass_end(ri_auto_pub_back(s->pub), s->pass);
        ri_auto_pub_request(s->pub);
    }
}

int ri_live_record_touch(struct RILiveSession *s, uint16_t key, uint8_t val) {
    int rc_t = 2, rc_c = 2;
    if (!s)
        return 2;
    if (s->pub) {
        struct RIAutoLane *bk = ri_auto_pub_back(s->pub);
        if (bk && s->pass)
            rc_t = ri_auto_touch(bk, s->pass, s->tr.state,
                (uint32_t)s->cursor_ticks, s->ppq, key, val);
        else if (bk)
            rc_t = 2;
    }
    if (s->ctl)
        rc_c = ri_ctl_send(s->ctl, key, val);
    if (rc_t == 0 || rc_c == 0)
        return 0;
    return 2;
}

static void live_meters_update(struct RILiveSession *s) {
    uint32_t i;
    for (i = 0u; i < 4u; i++)
        s->meters.sec_peak[i] = ri_engine_section_peak(&s->eng, i);
    for (i = 0u; i < 4u; i++)
        s->meters.fx_peak[i] = ri_engine_fx_peak(&s->eng, i);
    s->meters.comp_gr = ri_engine_comp_gr(&s->eng);
    s->meters.samples = s->sample_cursor;
    s->meters.cursor_ticks = s->cursor_ticks;
    s->meters.xruns = s->xruns;
}

uint32_t ri_live_render(struct RILiveSession *s, float *out_l, float *out_r,
    uint32_t frames) {
    uint64_t s0, s1, t0, t1;
    uint32_t n = 0u, k;
    uint32_t seq;
    uint64_t loop_end = 0u;
    int wrap = 0;
    if (!s || !out_l || !out_r || frames == 0u)
        return 0u;
    if (!s->scratch || s->scratch_cap == 0u)
        return 0u;
    if (s->tr.state == RI_TR_STOPPED) {
        for (k = 0u; k < frames; k++) {
            out_l[k] = 0.0f;
            out_r[k] = 0.0f;
        }
        if (s->ctl) {
            /* Stopped knob moves take effect without sound. */
            struct RIEvent tmp[32];
            uint32_t m, q;
            uint32_t dq = 0u;
            (void)dq;
            m = ri_ctl_drain(s->ctl, tmp, 32u, 0u, &s->seq_next);
            for (q = 0u; q < m; q++)
                ri_engine_apply_event(&s->eng, &tmp[q]);
        }
        for (k = 0u; k < 4u; k++) {
            s->meters.sec_peak[k] = 0.0f;
            s->meters.fx_peak[k] = 0.0f;
        }
        s->meters.comp_gr = 0.0f;
        s->meters.samples = s->sample_cursor;
        s->meters.cursor_ticks = s->cursor_ticks;
        s->meters.xruns = s->xruns;
        return frames;
    }
    if (s->pub)
        ri_auto_pub_apply(s->pub);
    if (s->pub && s->carry) {
        const struct RIAutoLane *fr = ri_auto_pub_front(s->pub);
        if (fr != s->last_front) {
            s->last_front = fr;
            ri_auto_carry_reindex(s->carry, fr, (uint32_t)s->cursor_ticks);
        }
    }
    s0 = s->sample_cursor;
    s1 = s0 + (uint64_t)frames;
    t0 = s->cursor_ticks;
    t1 = t0;
    for (k = 0u; k < RI_LIVE_T1_BOUND; k++) {
        if (ri_map_tick(&s->map, t1) >= s1)
            break;
        t1++;
    }
    if (s->loop.on && s->loop.len_bars > 0u) {
        uint64_t ls = (uint64_t)s->loop.start_bar * live_bar_ticks(s->ppq);
        uint64_t le = ls + (uint64_t)s->loop.len_bars * live_bar_ticks(s->ppq);
        loop_end = le;
        if (t1 > le) {
            t1 = le;
            wrap = 1;
        }
        (void)loop_end;
    }
    seq = s->seq_next;
    if (s->need_chase && s->pub && s->carry && n < s->scratch_cap) {
        const struct RIAutoLane *fr = ri_auto_pub_front(s->pub);
        uint32_t room = s->scratch_cap - n;
        uint32_t w;
        ri_auto_carry_reindex(s->carry, fr, (uint32_t)t0);
        w = ri_auto_chase(fr, s->pass, (uint32_t)t0, &s->map, s->ppq,
            s->scratch + n, room, &seq);
        n += w;
        s->need_chase = 0;
    } else if (s->need_chase) {
        s->need_chase = 0;
    }
    if (t1 > t0 && n < s->scratch_cap) {
        uint32_t room = s->scratch_cap - n;
        uint32_t got = ri_player_block(&s->player, s->track, &s->loop,
            &s->map, s->ppq, t0, t1, s->scratch + n, room);
        uint32_t q;
        for (q = 0u; q < got; q++)
            s->scratch[n + q].seq = seq++;
        n += got;
    }
    if (s->pub && s->carry && t1 > t0 && n < s->scratch_cap) {
        const struct RIAutoLane *fr = ri_auto_pub_front(s->pub);
        uint64_t span = t1 - t0;
        uint32_t cnt = (span > 0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)span;
        uint32_t room = s->scratch_cap - n;
        if (cnt > 0u && (uint64_t)(uint32_t)t0 == t0) {
            uint32_t before = n;
            ri_auto_emit_range(fr, s->pass, s->carry, (uint32_t)t0, cnt,
                &s->map, s->ppq, s->scratch, &n, s->scratch_cap, &seq);
            (void)room;
            (void)before;
        }
    }
    if (s->ctl && n < s->scratch_cap) {
        uint32_t room = s->scratch_cap - n;
        uint32_t got = ri_ctl_drain(s->ctl, s->scratch + n, room, s0, &seq);
        n += got;
    }
    for (k = 1u; k < n; k++) {
        struct RIEvent key = s->scratch[k];
        uint64_t j = k;
        while (j > 0u && ri_event_less(&key, &s->scratch[j - 1u])) {
            s->scratch[j] = s->scratch[j - 1u];
            j--;
        }
        s->scratch[j] = key;
    }
    {
        uint32_t w = 0u, r;
        for (r = 0u; r < n; r++) {
            uint64_t a = s->scratch[r].sample;
            if (a < s0 || a >= s1)
                continue;
            s->scratch[w] = s->scratch[r];
            s->scratch[w].sample = a - s0;
            w++;
        }
        n = w;
    }
    ri_engine_load(&s->eng, s->scratch, n, frames, s->sections);
    {
        uint32_t got = ri_engine_render(&s->eng, out_l, out_r, frames, s->sr);
        if (got < frames) {
            for (k = got; k < frames; k++) {
                out_l[k] = 0.0f;
                out_r[k] = 0.0f;
            }
            s->xruns++;
        }
    }
    s->cursor_ticks = t1;
    s->sample_cursor = s1;
    s->seq_next = seq;
    if (wrap) {
        uint64_t ls = (uint64_t)s->loop.start_bar * live_bar_ticks(s->ppq);
        s->cursor_ticks = ls;
        s->need_chase = 1;
        if (s->pass)
            ri_auto_punch_out_all(s->pass);
        if (s->carry && s->pub)
            ri_auto_carry_reindex(s->carry, ri_auto_pub_front(s->pub),
                (uint32_t)ls);
    }
    live_meters_update(s);
    return frames;
}

const struct RILiveMeters *ri_live_meters(const struct RILiveSession *s) {
    if (!s)
        return 0;
    return &s->meters;
}
