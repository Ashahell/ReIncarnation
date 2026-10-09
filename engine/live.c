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
    s->now_us = 0;                 /* no timing until a clock is injected */
    for (i = 0u; i < RI_LIVE_ST_COUNT; i++) {
        s->stages.sum_us[i] = 0u;
        s->stages.max_us[i] = 0u;
        s->stages.n[i] = 0u;
    }
    s->stages.playing_buffers = 0u;
    s->stages.stopped_buffers = 0u;
    s->tr.state = RI_TR_STOPPED;
    s->tr.clicks = 0u;
    s->cursor_ticks = 0u;
    s->ppq = ri_ppq_or_default(ppq);
    s->sr = (sr > 0.0f) ? sr : 48000.0f;
    s->bpm = (bpm >= 20.0f && bpm <= 500.0f) ? bpm : 120.0f;
    s->nspq = (uint64_t)(60000000000.0 / (double)s->bpm);
    if (s->nspq == 0u)
        s->nspq = 500000000ULL;
    ri_atomic_store_rel(&s->sections, sections);
    s->segs[0].start_tick = 0u;
    s->segs[0].ns_per_quarter = s->nspq;
    s->nsegs = 1u;
    s->map.segs = s->segs;
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
    s->locate_tick = 0u;
    s->locate_done = 0u;
    s->seq_next = 0u;
    s->xruns = 0u;
    s->meters_seq.v = 0u;
    /* Init before sharing; plain store safe here. */
    s->tick_rem = 0u;
    for (i = 0u; i < RI_ROUTE_NSECTIONS; i++)
        s->meters.sec_peak[i] = 0.0f;
    for (i = 0u; i < 4u; i++)
        s->meters.fx_peak[i] = 0.0f;
    s->meters.comp_gr = 0.0f;
    s->meters.master_peak[0] = 0.0f;
    s->meters.master_peak[1] = 0.0f;
    s->meters.samples = 0u;
    s->meters.cursor_ticks = 0u;
    s->meters.xruns = 0u;
}

void ri_live_set_sections(struct RILiveSession *s, uint32_t sections) {
    if (!s)
        return;
    ri_atomic_store_rel(&s->sections, sections);
}

void ri_live_set_bpm(struct RILiveSession *s, float bpm) {
    uint64_t nspq;
    if (!s || bpm < 20.0f || bpm > 500.0f)
        return;
    nspq = (uint64_t)(60000000000.0 / (double)bpm);
    if (nspq == 0u)
        nspq = 500000000ULL;
    if (s->nsegs && s->segs[s->nsegs - 1u].ns_per_quarter == nspq) {
        s->bpm = bpm;             /* same rate: only the readout moves */
        return;
    }
    /* A tempo change APPENDS a segment at the current tick: everything
     * already played keeps its map anchor, so map_tick(cursor_ticks) and
     * sample_cursor cannot disagree and the forward-only tick walk keeps
     * moving. Rewriting the one segment in place was the bug: on a tempo
     * DROP the new (slower) rate mapped the current tick to a sample
     * position AHEAD of where the audio actually was, and the engine
     * froze until the audio caught up (M3c: 29 of 143 samples frozen). */
    if (s->nsegs >= RI_LIVE_MAX_SEGS) {
        uint64_t at = s->cursor_ticks;
        s->segs[0].start_tick = 0u;
        s->segs[0].ns_per_quarter = nspq;
        s->nsegs = 1u;
        s->map.n = 1u;
        /* Re-anchor the audio to the same tick, or the collapse stalls it
         * exactly as the old rewrite did. */
        s->sample_cursor = ri_map_tick(&s->map, at);
        s->need_chase = 1;
    } else {
        uint32_t k = s->nsegs++;
        s->segs[k].start_tick = s->cursor_ticks;
        s->segs[k].ns_per_quarter = nspq;
        s->map.n = s->nsegs;
    }
    s->nspq = nspq;
    s->bpm = bpm;
}

uint32_t ri_live_sections(struct RILiveSession *s) {
    if (!s)
        return 0u;
    return ri_atomic_load_acq(&s->sections);
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

void ri_live_locate(struct RILiveSession *s, uint64_t tick) {
    if (!s)
        return;
    s->locate_tick = tick;
    ri_atomic_store_rel(&s->locate_gen, ri_atomic_load_acq(&s->locate_gen) + 1u);
}

/* Render-side half of the locate: re-anchor both cursors and re-init the
 * player at the bar. Same anchor rule as a restart (ri_live_play), and it
 * runs on the stopped path too, so a locate lands whether or not the take
 * is running. One request per generation; the tick is written before the
 * counter is published, so the render never sees a half-pair. */
static void ri_live_locate_apply(struct RILiveSession *s) {
    uint32_t gen = ri_atomic_load_acq(&s->locate_gen);
    uint64_t bar, off, i;
    if (gen == s->locate_done)
        return;
    s->locate_done = gen;
    s->cursor_ticks = s->locate_tick;
    s->sample_cursor = ri_map_tick(&s->map, s->locate_tick);
    bar = ri_seq_bar_at_tick(s->cursor_ticks, s->ppq);
    off = s->cursor_ticks - ri_seq_tick_of_bar(s->ppq, bar);
    ri_player_init(&s->player, s->banks, s->track, bar);
    for (i = 0u; i < RI_SONGTRACK_INSTANCES && off; i++)
        if (s->banks[i]) {
            uint64_t len = (uint64_t)s->banks[i]->pat[s->player.sounding_slot[i] & 31u].length *
                (uint64_t)(s->ppq / 4u);
            if (len)
                s->player.phase_ticks[i] = off % len;
        }
    s->need_chase = 1;
}

void ri_live_play(struct RILiveSession *s) {
    int was_stopped;
    if (!s)
        return;
    was_stopped = s->tr.state == RI_TR_STOPPED;
    ri_tr_play(&s->tr, &s->cursor_ticks);
    /* A start from STOPPED plays the cursor bar's selections from their
     * first step (songs & playlists 2026-09-30: the player kept the state
     * it had when the banks were set or the transport stopped, so a
     * loaded song sounded one bar late). */
    if (was_stopped) {
        uint64_t bar = ri_seq_bar_at_tick(s->cursor_ticks, s->ppq);
        uint64_t off = s->cursor_ticks - ri_seq_tick_of_bar(s->ppq, bar);
        uint32_t i;
        ri_player_init(&s->player, s->banks, s->track, bar);
        /* A resume inside a bar (the first Stop pauses) picks every
         * pattern up at the cursor, not at its first step. */
        for (i = 0u; i < RI_SONGTRACK_INSTANCES && off; i++)
            if (s->banks[i]) {
                uint64_t len = (uint64_t)s->banks[i]->pat[s->player.sounding_slot[i] & 31u].length *
                    (uint64_t)(s->ppq / 4u);
                if (len)
                    s->player.phase_ticks[i] = off % len;
            }
        /* Event samples come from the tempo map (tick 0 = sample 0): the
         * sample cursor must stand where the tick cursor is, or a restart
         * after a stop drops the first bars (songs & playlists 2026-09-30). */
        s->sample_cursor = ri_map_tick(&s->map, s->cursor_ticks);
    }
    s->need_chase = 1;
}

void ri_live_record(struct RILiveSession *s) {
    if (!s)
        return;
    ri_tr_record(&s->tr, &s->cursor_ticks);
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
    for (i = 0u; i < RI_ROUTE_NSECTIONS; i++)
        s->meters.sec_peak[i] = ri_engine_section_peak(&s->eng, i);
    for (i = 0u; i < 4u; i++)
        s->meters.fx_peak[i] = ri_engine_fx_peak(&s->eng, i);
    s->meters.comp_gr = ri_engine_comp_gr(&s->eng);
    s->meters.master_peak[0] = ri_engine_master_peak(&s->eng, 0u);
    s->meters.master_peak[1] = ri_engine_master_peak(&s->eng, 1u);
    s->meters.samples = s->sample_cursor;
    s->meters.cursor_ticks = s->cursor_ticks;
    s->meters.xruns = s->xruns;
}

/* Stage timing (Dell 2026-10-02). NULL clock = one predictable branch per
 * boundary and nothing else, so an offline caller is unaffected. Every sample
 * is clamped by construction: the clock is monotonic and each stage opens and
 * closes exactly once. */
#define RI_STAGE_T(s, k, t) do { \
    if ((s)->now_us) { (t) = (s)->now_us(); } \
} while (0)
#define RI_STAGE_E(s, k, t) do { \
    if ((s)->now_us) { \
        uint64_t u_ = (s)->now_us() - (t); \
        if (u_ > 0xFFFFFFFFu) u_ = 0xFFFFFFFFu; \
        (s)->stages.sum_us[k] += u_; \
        if ((uint32_t)u_ > (s)->stages.max_us[k]) (s)->stages.max_us[k] = (uint32_t)u_; \
        (s)->stages.n[k]++; \
    } \
} while (0)

/* These two hooks are deliberately INDEPENDENT. Cascading one into the other
 * looks tidier and is wrong: the DSP sub-stage table sits inside the render,
 * so arming it adds ~16 clock reads per block. A host test whose clock is a
 * simulation that advances per read -- t88 is calibrated for exactly two reads
 * per buffer -- then measures a render several times longer than it simulated,
 * and its governor laws all fail for a reason that has nothing to do with the
 * governor. Arming both is the backend's job, in the one place it happens. */
void ri_live_set_clock(struct RILiveSession *s, uint64_t (*now_us)(void)) {
    if (s)
        s->now_us = now_us;
}

const struct RILiveStages *ri_live_stages(const struct RILiveSession *s) {
    return s ? &s->stages : 0;
}

uint32_t ri_live_render(struct RILiveSession *s, float *out_l, float *out_r,
    uint32_t frames) {
    /* ts is written by RI_STAGE_T immediately before every RI_STAGE_E; the
     * initialisers only satisfy -Wmaybe-uninitialized, which cannot see
     * through the macros. Neither is read on a path that skips the clock.
     * tt is TOTAL's own: TOTAL encloses every other stage, and they all write
     * ts, so sharing one variable made TOTAL measure from the last inner
     * stage instead of from its own start (t156 caught it). */
    uint64_t s0, s1, t0, t1, ts = 0, tt = 0;
    uint32_t n = 0u, k;
    uint32_t seq;
    uint64_t loop_end = 0u;
    int wrap = 0;
    if (!s || !out_l || !out_r || frames == 0u)
        return 0u;
    if (!s->scratch || s->scratch_cap == 0u)
        return 0u;
    /* A queued locate lands before either cursor is read (M3). */
    ri_live_locate_apply(s);
    if (s->tr.state == RI_TR_STOPPED) {
        RI_STAGE_T(s, RI_LIVE_ST_STOPPED, ts);
        s->stages.stopped_buffers++;
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
        ri_live_meters_begin(s);
        for (k = 0u; k < RI_ROUTE_NSECTIONS; k++)
            s->meters.sec_peak[k] = 0.0f;
        for (k = 0u; k < 4u; k++)
            s->meters.fx_peak[k] = 0.0f;
        s->meters.comp_gr = 0.0f;
        s->meters.master_peak[0] = 0.0f;
        s->meters.master_peak[1] = 0.0f;
        s->meters.samples = s->sample_cursor;
        s->meters.cursor_ticks = s->cursor_ticks;
        s->meters.xruns = s->xruns;
        ri_live_meters_end(s);
        RI_STAGE_E(s, RI_LIVE_ST_STOPPED, ts);
        return frames;
    }
    RI_STAGE_T(s, RI_LIVE_ST_TOTAL, tt);
    RI_STAGE_T(s, RI_LIVE_ST_EVENTS, ts);
    s->stages.playing_buffers++;
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
    RI_STAGE_E(s, RI_LIVE_ST_EVENTS, ts);
    for (k = 1u; k < n; k++) {
        struct RIEvent key = s->scratch[k];
        uint64_t j = k;
        while (j > 0u && ri_event_less(&key, &s->scratch[j - 1u])) {
            s->scratch[j] = s->scratch[j - 1u];
            j--;
        }
        s->scratch[j] = key;
    }
    RI_STAGE_T(s, RI_LIVE_ST_SORT, ts);
    RI_STAGE_E(s, RI_LIVE_ST_SORT, ts);
    RI_STAGE_T(s, RI_LIVE_ST_FILTER, ts);
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
    RI_STAGE_E(s, RI_LIVE_ST_FILTER, ts);
    RI_STAGE_T(s, RI_LIVE_ST_LOAD, ts);
    ri_engine_load(&s->eng, s->scratch, n, frames,
        ri_atomic_load_acq(&s->sections));
    RI_STAGE_E(s, RI_LIVE_ST_LOAD, ts);
    /* Delay clock ownership (owner 2026-09-27: echoes ran at the 140 BPM
     * engine default against a 120 groove): the transport tempo owns it.
     * (Demo is 140 since the Zombie Nation drive; the push stays — it
     * guards any future default/session split.) */
    if (s->eng.tempo != s->bpm)
        ri_engine_set_tempo(&s->eng, s->bpm);
    ri_engine_transport(&s->eng, s->tr.state == RI_TR_PLAYING ? 1u : 0u,
        s->cursor_ticks, s->ppq);
    RI_STAGE_T(s, RI_LIVE_ST_DSP, ts);
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
    RI_STAGE_E(s, RI_LIVE_ST_DSP, ts);
    s->cursor_ticks = t1;
    s->sample_cursor = s1;
    s->seq_next = seq;
    if (wrap) {
        uint64_t ls = (uint64_t)s->loop.start_bar * live_bar_ticks(s->ppq);
        s->cursor_ticks = ls;
        s->sample_cursor = ri_map_tick(&s->map, ls);   /* same anchor rule as a restart */
        s->need_chase = 1;
        if (s->pass)
            ri_auto_punch_out_all(s->pass);
        if (s->carry && s->pub)
            ri_auto_carry_reindex(s->carry, ri_auto_pub_front(s->pub),
                (uint32_t)ls);
    }
    RI_STAGE_T(s, RI_LIVE_ST_METERS, ts);
    ri_live_meters_begin(s);
    live_meters_update(s);
    ri_live_meters_end(s);
    RI_STAGE_E(s, RI_LIVE_ST_METERS, ts);
    RI_STAGE_E(s, RI_LIVE_ST_TOTAL, tt);
    return frames;
}

const struct RILiveMeters *ri_live_meters(const struct RILiveSession *s) {
    if (!s)
        return 0;
    return &s->meters;
}

/* G9b Step 2 meter snapshot protocol: seqlock around the published
 * meters. Render side opens (odd) before writing fields and closes
 * (even) after; the GUI side copies only across a stable even pair.
 * Single writer / single reader; word-sized accesses only. */
void ri_live_meters_begin(struct RILiveSession *s) {
    if (s)
        ri_atomic_fetch_add_rel(&s->meters_seq, 1u);
}

void ri_live_meters_end(struct RILiveSession *s) {
    if (s)
        ri_atomic_fetch_add_rel(&s->meters_seq, 1u);
}

int ri_live_meters_read(const struct RILiveSession *s, struct RILiveMeters *out) {
    uint32_t a, b;
    if (!s || !out)
        return 2;
    a = ri_atomic_load_acq(&s->meters_seq);
    if (a & 1u)
        return 1;
    *out = s->meters;
    b = ri_atomic_load_acq(&s->meters_seq);
    if (a != b)
        return 1;
    return 0;
}
