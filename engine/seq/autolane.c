/* autolane.c — automation lane model (spec 2026-09-26 §2).
 * Gate-blind model + caller-side record path (transport state in).
 * No alloc, no IO, no mutable static state. */
#include "engine/seq/autolane.h"
#include "engine/dsp/rb303.h" /* allow-list IDs (engine side, no GUI) */
#include <string.h> /* memmove for sorted insert */

/* Allow-list (R-ALLOW, ledger): exactly the 16 303 control IDs — the
 * only IDs with defined end-to-end delivery. Sorted for binary search;
 * unknown or excluded IDs are refused (fail-closed). */
static const uint16_t RI_AUTO_ALLOW[] = {
    RI_CTL_303A_CUTOFF, RI_CTL_303A_RESO, RI_CTL_303A_ENVMOD,
    RI_CTL_303A_DECAY, RI_CTL_303A_ACCENT, RI_CTL_303A_WAVE,
    RI_CTL_303A_VOLUME, RI_CTL_303A_TUNE,
    RI_CTL_303B_CUTOFF, RI_CTL_303B_RESO, RI_CTL_303B_ENVMOD,
    RI_CTL_303B_DECAY, RI_CTL_303B_ACCENT, RI_CTL_303B_WAVE,
    RI_CTL_303B_VOLUME, RI_CTL_303B_TUNE
};

int ri_auto_allowed(uint16_t ctl) {
    uint32_t lo = 0u, hi = sizeof RI_AUTO_ALLOW / sizeof RI_AUTO_ALLOW[0];
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1u);
        if (RI_AUTO_ALLOW[mid] == ctl)
            return 1;
        if (RI_AUTO_ALLOW[mid] < ctl)
            lo = mid + 1u;
        else
            hi = mid;
    }
    return 0;
}

/* STUBS (Task 3): chase/emission/chunk map land with their tests. */

/* ---- shared sorted-array core (all mutation precomputed: fail-closed) ---- */

static uint32_t lane_live_n(const struct RIAutoLane *l) {
    return (l->n < l->cap) ? l->n : l->cap;
}

/* Lower bound on (tick, ctl). */
static uint32_t lane_lower(const struct RIAutoLane *l, uint32_t n,
                           uint32_t tick, uint16_t ctl) {
    uint32_t lo = 0u, hi = n;
    while (lo < hi) {
        uint32_t mid = lo + ((hi - lo) >> 1u);
        if (l->ev[mid].tick < tick ||
            (l->ev[mid].tick == tick && l->ev[mid].ctl < ctl))
            lo = mid + 1u;
        else
            hi = mid;
    }
    return lo;
}

/* Erase this ctl's UNMARKED events in [from, to); marked pass writes
 * survive (R2). Returns the removed count. */
static uint32_t lane_erase_unmarked(struct RIAutoLane *l, uint16_t ctl,
                                    uint64_t from, uint64_t to) {
    uint32_t r = 0u, w = 0u, n;
    if (!l || !l->ev)
        return 0u;
    n = lane_live_n(l);
    for (r = 0u; r < n; r++) {
        uint64_t t = l->ev[r].tick;
        if (l->ev[r].ctl == ctl && t >= from && t < to &&
            (l->ev[r].pad & RI_AUTO_EV_PASS) == 0u)
            continue; /* erased */
        if (w != r)
            l->ev[w] = l->ev[r];
        w++;
    }
    l->n = w;
    return n - w;
}

/* Forward: marked insert below needs the plain insert. */
static int lane_insert(struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                       uint8_t val);

/* Insert-or-replace at (tick, ctl) with the pass marker set. */
static int lane_insert_marked(struct RIAutoLane *l, uint32_t tick,
                              uint16_t ctl, uint8_t val) {
    uint32_t n, at;
    if (lane_insert(l, tick, ctl, val) != 0)
        return 2;
    n = lane_live_n(l);
    at = lane_lower(l, n, tick, ctl);
    if (at < n && l->ev[at].tick == tick && l->ev[at].ctl == ctl)
        l->ev[at].pad |= RI_AUTO_EV_PASS;
    return 0;
}

/* Insert-or-replace at (tick, ctl). 0 ok / 2 full-or-corrupt. */
static int lane_insert(struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                       uint8_t val) {
    uint32_t n, at;
    if (!l || !l->ev)
        return 2;
    if (l->n > l->cap)
        return 2;
    n = l->n;
    at = lane_lower(l, n, tick, ctl);
    if (at < n && l->ev[at].tick == tick && l->ev[at].ctl == ctl) {
        l->ev[at].val = val; /* replace: no duplicate growth */
        return 0;
    }
    if (n >= l->cap) {
        l->flags |= RI_AUTO_FLAG_FULL; /* sticky, never silent */
        return 2;
    }
    memmove(&l->ev[at + 1u], &l->ev[at], (n - at) * sizeof l->ev[0]);
    l->ev[at].tick = tick;
    l->ev[at].ctl = ctl;
    l->ev[at].val = val;
    l->ev[at].pad = 0u;
    l->n = n + 1u;
    return 0;
}

/* Remove this ctl's events in [from, to); returns the removed count. */
static uint32_t lane_erase_ctl(struct RIAutoLane *l, uint16_t ctl,
                               uint64_t from, uint64_t to) {
    uint32_t r = 0u, w = 0u, n;
    if (!l || !l->ev)
        return 0u;
    n = lane_live_n(l);
    for (r = 0u; r < n; r++) {
        uint64_t t = l->ev[r].tick;
        if (l->ev[r].ctl == ctl && t >= from && t < to)
            continue; /* erased */
        if (w != r)
            l->ev[w] = l->ev[r];
        w++;
    }
    l->n = w;
    return n - w;
}

static uint64_t lane_bar_ticks(uint32_t ppq) {
    return 4u * (uint64_t)ri_ppq_or_default(ppq);
}

/* Bar window [t0,t1) in ticks; end_tick is bar 999 (drop boundary). */
static int lane_bar_win(uint64_t bar, uint64_t len, uint32_t ppq,
                        uint64_t *t0, uint64_t *t1, uint64_t *end);

void ri_auto_punch_out_all(struct RIAutoPass *p) {
    if (!p)
        return;
    p->npunched = 0u; /* touched set kept for Copy Touched */
}

/* STUBS replaced (Task 2a): sweep body + pass_end below. */
int ri_auto_sweep(struct RIAutoLane *l, const struct RIAutoPass *p,
                  uint32_t from, uint32_t to,
                  const uint8_t *vals) {
    uint32_t k, freed = 0u, exist = 0u, need;
    uint64_t n;
    if (!l || !l->ev || !p)
        return 2;
    if (l->n > l->cap)
        return 2;
    if (from >= to)
        return 0; /* empty sweep: no-op */
    /* Precompute (no mutation): erased unmarked span events per punched
     * ctl, plus survivors already sitting at `to` (replace, no growth).
     * NULL vals = erase only. */
    for (k = 0u; k < p->npunched; k++) { /* freed count first */
        uint32_t q2;
        for (q2 = 0u; q2 < l->n; q2++) {
            uint64_t t = l->ev[q2].tick;
            if (l->ev[q2].ctl != p->punched[k])
                continue;
            if (t >= from && t < to &&
                (l->ev[q2].pad & RI_AUTO_EV_PASS) == 0u)
                freed++;
        }
    }
    /* Survivors at `to` (any mark — re-anchor replaces them). Keys unique. */
    exist = 0u;
    for (k = 0u; k < p->npunched; k++) {
        uint32_t q2;
        for (q2 = 0u; q2 < l->n; q2++)
            if (l->ev[q2].ctl == p->punched[k] && l->ev[q2].tick == to) {
                exist++;
                break;
            }
    }
    n = (uint64_t)l->n;
    need = (vals && p->npunched > exist) ? (uint32_t)(p->npunched - exist) : 0u;
    if (n < freed)
        return 2;
    if (n - freed + need > l->cap) {
        l->flags |= RI_AUTO_FLAG_FULL;
        return 2;
    }
    for (k = 0u; k < p->npunched; k++) {
        lane_erase_unmarked(l, p->punched[k], from, to);
        if (vals && lane_insert_marked(l, to, p->punched[k], vals[k]) != 0)
            return 2; /* unreachable after precompute; fail-closed */
    }
    return 0;
}

void ri_auto_pass_end(struct RIAutoLane *l, struct RIAutoPass *p) {
    uint32_t q, n;
    if (!l || !l->ev || !p)
        return;
    n = lane_live_n(l);
    for (q = 0u; q < n; q++)
        l->ev[q].pad &= (uint8_t)~RI_AUTO_EV_PASS;
    p->npunched = 0u;
    p->ntouched = 0u;
}

/* (old sweep body removed with the ppq signature; 2a body lands below) */

int ri_auto_clear_loop(struct RIAutoLane *l, uint32_t start_tick,
                       uint32_t len_ticks) {
    uint32_t r = 0u, w = 0u, n;
    uint64_t end;
    if (!l || !l->ev)
        return 2;
    if (l->n > l->cap)
        return 2;
    if (len_ticks == 0u)
        return 0; /* empty range: no-op */
    end = (uint64_t)start_tick + (uint64_t)len_ticks;
    n = l->n;
    for (r = 0u; r < n; r++) {
        uint64_t t = l->ev[r].tick;
        if (t >= start_tick && (uint64_t)t < end)
            continue; /* dropped */
        if (w != r)
            l->ev[w] = l->ev[r];
        w++;
    }
    l->n = w;
    return 0;
}

int ri_auto_stamp(struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                  uint8_t val) {
    if (!l || !l->ev)
        return 2;
    if (!ri_auto_allowed(ctl))
        return 2;
    return lane_insert(l, tick, ctl, val); /* exact tick, no quantize */
}

int ri_auto_copy_touched(struct RIAutoLane *l, const struct RIAutoPass *p,
                         uint32_t start, uint32_t end,
                         const uint8_t *vals) {
    uint32_t k, freed = 0u, q;
    uint64_t n;
    if (!l || !l->ev || !p)
        return 2;
    if (l->n > l->cap)
        return 2;
    if (end <= start)
        return 2; /* empty range: caller error, fail closed */
    if (p->ntouched > 0u && !vals)
        return 2;
    for (k = 0u; k < p->ntouched; k++)
        for (q = 0u; q < l->n; q++) {
            uint64_t t = l->ev[q].tick;
            if (l->ev[q].ctl == p->touched[k] && t >= start && t < end)
                freed++;
        }
    n = (uint64_t)l->n;
    if (n - freed + p->ntouched > l->cap) {
        l->flags |= RI_AUTO_FLAG_FULL;
        return 2;
    }
    for (k = 0u; k < p->ntouched; k++) {
        lane_erase_ctl(l, p->touched[k], start, end);
        /* Copy anchors are pass products: marked so a still-punched
         * control's later sweep spares them (pass_end unmarks). */
        if (lane_insert_marked(l, start, p->touched[k], vals[k]) != 0)
            return 2; /* unreachable after precompute; fail-closed */
    }
    return 0;
}

int ri_auto_cut(struct RIAutoLane *l, struct RIAutoClip *clip,
                uint64_t start_bar, uint64_t len_bars, uint32_t ppq) {
    uint64_t t0, t1, end, q, cutlen, w = 0u;
    if (!l || !l->ev || !clip)
        return 2;
    if (l->n > l->cap)
        return 2;
    if (lane_bar_win(start_bar, len_bars, ppq, &t0, &t1, &end) != 0)
        return 2;
    (void)end; /* cut shifts within the song; paste drops past the end */
    if (len_bars == 0u) { /* empty cut: empty clip, lane untouched */
        clip->base_tick = (t0 > (uint64_t)0xFFFFFFFFu) ? 0xFFFFFFFFu : (uint32_t)t0;
        clip->span_ticks = 0u;
        clip->n = 0u;
        return 0;
    }
    if (t0 > (uint64_t)0xFFFFFFFFu)
        return 2;
    cutlen = t1 - t0;
    if (cutlen > (uint64_t)0xFFFFFFFFu)
        return 2;
    for (q = 0u; q < l->n; q++) { /* validate: clip fits */
        uint64_t t = l->ev[q].tick;
        if (t < t0 || t >= t1)
            continue;
        if (w >= RI_AUTO_CLIP_EVENTS)
            return 2;
        w++;
    }
    clip->base_tick = (uint32_t)t0;
    clip->span_ticks = (uint32_t)cutlen;
    clip->n = 0u;
    for (q = 0u; q < l->n; q++) { /* extract (lane order is sorted) */
        uint64_t t = l->ev[q].tick;
        if (t < t0 || t >= t1)
            continue;
        clip->ev[clip->n].tick = (uint32_t)(t - t0);
        clip->ev[clip->n].ctl = l->ev[q].ctl;
        clip->ev[clip->n].val = l->ev[q].val;
        clip->ev[clip->n].pad = 0u;
        clip->n++;
    }
    for (w = 0u, q = 0u; q < l->n; q++) { /* remove + shift left */
        uint64_t t = l->ev[q].tick;
        if (t >= t0 && t < t1)
            continue;
        if (t >= t1)
            t -= cutlen;
        l->ev[w].tick = (uint32_t)t;
        l->ev[w].ctl = l->ev[q].ctl;
        l->ev[w].val = l->ev[q].val;
        l->ev[w].pad = 0u;
        w++;
    }
    l->n = w;
    return 0;
}

/* Bar window [t0,t1) in ticks; end_tick is bar 999 (drop boundary).
 * Forward-declared above (first use is ri_auto_cut). */
static int lane_bar_win(uint64_t bar, uint64_t len, uint32_t ppq,
                        uint64_t *t0, uint64_t *t1, uint64_t *end) {
    uint64_t bt = lane_bar_ticks(ppq);
    if (bt == 0u)
        return 2;
    *t0 = bar * bt;
    if (len > ((uint64_t)0xFFFFFFFFFFFFFFFFu - *t0) / bt)
        return 2;
    *t1 = *t0 + len * bt;
    *end = (uint64_t)999u * bt;
    return 0;
}

int ri_auto_copy(const struct RIAutoLane *l, struct RIAutoClip *clip,
                 uint64_t start_bar, uint64_t len_bars, uint32_t ppq) {
    uint64_t t0, t1, end, q;
    uint32_t w = 0u;
    if (!l || !l->ev || !clip)
        return 2;
    if (lane_bar_win(start_bar, len_bars, ppq, &t0, &t1, &end) != 0)
        return 2;
    (void)end; /* copy keeps the whole window; paste drops past the end */
    if (t0 > (uint64_t)0xFFFFFFFFu || t1 - t0 > (uint64_t)0xFFFFFFFFu)
        return 2; /* absurd window: fail closed, clip untouched */
    for (q = 0u; q < l->n; q++) { /* validate first: all-or-nothing */
        uint64_t t = l->ev[q].tick;
        if (t < t0 || t >= t1)
            continue;
        if (w >= RI_AUTO_CLIP_EVENTS)
            return 2;
        w++;
    }
    clip->base_tick = (uint32_t)t0;
    clip->span_ticks = (uint32_t)(t1 - t0);
    clip->n = 0u;
    for (q = 0u; q < l->n; q++) {
        uint64_t t = l->ev[q].tick;
        if (t < t0 || t >= t1)
            continue;
        clip->ev[clip->n].tick = (uint32_t)(t - t0);
        clip->ev[clip->n].ctl = l->ev[q].ctl;
        clip->ev[clip->n].val = l->ev[q].val;
        clip->ev[clip->n].pad = 0u;
        clip->n++;
    }
    return 0;
}

int ri_auto_paste(struct RIAutoLane *l, const struct RIAutoClip *clip,
                  uint64_t at_bar, uint32_t ppq) {
    uint64_t bt, target, end_tick, q, span;
    uint32_t kept_ins = 0u, drop_shift = 0u, i;
    uint64_t final_worst;
    if (!l || !l->ev || !clip)
        return 2;
    if (l->n > l->cap)
        return 2;
    bt = lane_bar_ticks(ppq);
    if (bt == 0u)
        return 2;
    target = at_bar * bt;
    end_tick = (uint64_t)999u * bt;
    span = clip->span_ticks;
    if (clip->n > RI_AUTO_CLIP_EVENTS)
        return 2;
    /* Precompute (no mutation): surviving shifted + surviving inserts.
     * Ties resolve to replace inside lane_insert, which can only shrink
     * the total — so this bound is conservative and fail-closed. */
    for (q = 0u; q < l->n; q++) {
        uint64_t t = l->ev[q].tick;
        uint64_t nt = (t >= target) ? t + span : t;
        if (t >= target && (nt >= end_tick || nt > (uint64_t)0xFFFFFFFFu))
            drop_shift++;
    }
    for (q = 0u; q < clip->n; q++) {
        uint64_t nt = target + (uint64_t)clip->ev[q].tick;
        if (nt >= end_tick || nt > (uint64_t)0xFFFFFFFFu)
            continue;
        kept_ins++;
    }
    final_worst = (uint64_t)l->n - drop_shift + kept_ins;
    if (final_worst > l->cap) {
        l->flags |= RI_AUTO_FLAG_FULL;
        return 2;
    }
    /* Shift the tail right in place (order preserved: everything moved
     * stays at or above target, everything kept stays below). Dropped
     * events are compacted out in the same pass. */
    {
        uint32_t r = 0u, w = 0u, n = l->n;
        for (r = 0u; r < n; r++) {
            uint64_t t = l->ev[r].tick;
            uint64_t nt = (t >= target) ? t + span : t;
            if (t >= target && (nt >= end_tick || nt > (uint64_t)0xFFFFFFFFu))
                continue;
            l->ev[w].tick = (uint32_t)nt;
            l->ev[w].ctl = l->ev[r].ctl;
            l->ev[w].val = l->ev[r].val;
            l->ev[w].pad = 0u;
            w++;
        }
        l->n = w;
    }
    /* Merge the surviving inserts (replace on exact key tie). */
    for (i = 0u; i < clip->n; i++) {
        uint64_t nt = target + (uint64_t)clip->ev[i].tick;
        if (nt >= end_tick || nt > (uint64_t)0xFFFFFFFFu)
            continue;
        if (lane_insert(l, (uint32_t)nt, clip->ev[i].ctl,
                        clip->ev[i].val) != 0)
            return 2; /* unreachable after the bound; fail-closed */
    }
    return 0;
}

int ri_auto_paste_replace(struct RIAutoLane *l, const struct RIAutoClip *clip,
                          uint64_t at_bar, uint32_t ppq) {
    uint64_t bt, target, end_tick, span, q;
    uint32_t kept = 0u;
    if (!l || !l->ev || !clip)
        return 2;
    if (l->n > l->cap)
        return 2;
    bt = lane_bar_ticks(ppq);
    if (bt == 0u)
        return 2;
    target = at_bar * bt;
    end_tick = (uint64_t)999u * bt;
    span = clip->span_ticks;
    if (clip->n > RI_AUTO_CLIP_EVENTS)
        return 2;
    if (target > (uint64_t)0xFFFFFFFFu)
        return 2;
    for (q = 0u; q < clip->n; q++) { /* surviving inserts first */
        uint64_t nt = target + (uint64_t)clip->ev[q].tick;
        if (nt >= end_tick || nt > (uint64_t)0xFFFFFFFFu)
            continue;
        kept++;
    }
    { /* clear [target, target+span), then merge (replace on tie) */
        uint64_t cend = target + span;
        uint32_t r = 0u, w = 0u, n = l->n, i, cleared = 0u, q2;
        for (q2 = 0u; q2 < n; q2++) { /* count first: all-or-nothing */
            uint64_t t = l->ev[q2].tick;
            if (t >= target && (uint64_t)t < cend)
                cleared++;
        }
        if (n - cleared + kept > l->cap) {
            l->flags |= RI_AUTO_FLAG_FULL;
            return 2;
        }
        for (r = 0u; r < n; r++) {
            uint64_t t = l->ev[r].tick;
            if (t >= target && (uint64_t)t < cend)
                continue;
            if (w != r)
                l->ev[w] = l->ev[r];
            w++;
        }
        l->n = w;
        for (i = 0u; i < clip->n; i++) {
            uint64_t nt = target + (uint64_t)clip->ev[i].tick;
            if (nt >= end_tick || nt > (uint64_t)0xFFFFFFFFu)
                continue;
            if (lane_insert(l, (uint32_t)nt, clip->ev[i].ctl,
                            clip->ev[i].val) != 0)
                return 2; /* unreachable after the bound; fail-closed */
        }
    }
    return 0;
}

/* Latest event with tick' <= tick and ctl match (found/not-found out,
 * never a sentinel: 127 is a legal value). */
int ri_auto_value(const struct RIAutoLane *l, uint32_t tick, uint16_t ctl,
                  uint8_t *out) {
    uint32_t lo = 0u, hi, i;
    if (!l || !l->ev || !out)
        return 0;
    hi = (l->n < l->cap) ? l->n : l->cap;
    while (lo < hi) { /* upper bound on tick */
        uint32_t mid = lo + ((hi - lo) >> 1u);
        if (l->ev[mid].tick <= tick)
            lo = mid + 1u;
        else
            hi = mid;
    }
    for (i = lo; i > 0u; i--) { /* backwards: first match is latest */
        if (l->ev[i - 1u].ctl == ctl) {
            *out = l->ev[i - 1u].val;
            return 1;
        }
    }
    return 0;
}

static int pass_has(const uint16_t *set, uint16_t n, uint16_t ctl) {
    uint16_t k;
    for (k = 0u; k < n; k++)
        if (set[k] == ctl)
            return 1;
    return 0;
}

/* Punch-in + write: refuse-first (state, allow-list, grid law, set
 * space), then quantize forward, then insert-or-replace keeping
 * (tick, ctl) sort. The touch lands in both pass sets (no duplicates). */
int ri_auto_touch(struct RIAutoLane *l, struct RIAutoPass *p,
                  uint8_t tr_state, uint32_t cursor, uint32_t ppq,
                  uint16_t ctl, uint8_t val) {
    uint32_t g, q, r, tick;
    if (!l || !l->ev || !p)
        return 2;
    if (l->n > l->cap)
        return 2; /* corrupt count: fail closed, never memmove past cap */
    if (tr_state != (uint8_t)RI_TR_RECORD)
        return 2;
    if (!ri_auto_allowed(ctl))
        return 2;
    g = (uint32_t)ri_ppq_or_default(ppq) / 8u;
    if ((uint32_t)ri_ppq_or_default(ppq) % 8u != 0u || g == 0u)
        return 2; /* no integer 32nd at this ppq */
    q = cursor / g;
    r = cursor % g;
    if (r != 0u) {
        if (q + 1u > (uint32_t)0xFFFFFFFFu / g)
            return 2; /* forward step would overflow: fail closed */
        q++;
    }
    tick = q * g;
    if (!pass_has(p->punched, p->npunched, ctl) && p->npunched >= RI_AUTO_MAX_TOUCH)
        return 2;
    if (!pass_has(p->touched, p->ntouched, ctl) && p->ntouched >= RI_AUTO_MAX_TOUCH)
        return 2;
    if (lane_insert_marked(l, tick, ctl, val) != 0)
        return 2;
    if (!pass_has(p->punched, p->npunched, ctl))
        p->punched[p->npunched++] = ctl;
    if (!pass_has(p->touched, p->ntouched, ctl))
        p->touched[p->ntouched++] = ctl;
    return 0;
}
