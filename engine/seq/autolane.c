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

/* STUB (Task 2): sweep lands with its tests. */
int ri_auto_sweep(struct RIAutoLane *l, const struct RIAutoPass *p,
                  uint32_t from, uint32_t to) {
    (void)l; (void)p; (void)from; (void)to;
    return 2;
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
    uint32_t g, q, r, tick, lo, hi, at;
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
    lo = 0u;
    hi = (l->n < l->cap) ? l->n : l->cap;
    while (lo < hi) { /* lower bound on (tick, ctl) */
        uint32_t mid = lo + ((hi - lo) >> 1u);
        if (l->ev[mid].tick < tick ||
            (l->ev[mid].tick == tick && l->ev[mid].ctl < ctl))
            lo = mid + 1u;
        else
            hi = mid;
    }
    at = lo;
    if (at < l->n && l->ev[at].tick == tick && l->ev[at].ctl == ctl) {
        l->ev[at].val = val; /* replace: no duplicate growth */
    } else {
        if (l->n >= l->cap) {
            l->flags |= RI_AUTO_FLAG_FULL; /* sticky, never silent */
            return 2;
        }
        memmove(&l->ev[at + 1u], &l->ev[at], (l->n - at) * sizeof l->ev[0]);
        l->ev[at].tick = tick;
        l->ev[at].ctl = ctl;
        l->ev[at].val = val;
        l->ev[at].pad = 0u;
        l->n++;
    }
    if (!pass_has(p->punched, p->npunched, ctl))
        p->punched[p->npunched++] = ctl;
    if (!pass_has(p->touched, p->ntouched, ctl))
        p->touched[p->ntouched++] = ctl;
    return 0;
}
