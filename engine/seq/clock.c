/* clock.c — rational multi-segment tempo clock (Task 3, gate G3).
 *
 * Conversion is exact rational per tempo segment: single widened
 *   mul_div(dticks, ns_per_quarter * sr, ppq * 1e9)
 * with ONE rounding (Round at schedule, Floor at lookup), re-anchored at
 * every tempo event so error is bounded +-0.5 sample per tempo change and
 * never accumulates. Reverse lookups clamp boundary ticks to the owning
 * segment: segment j owns [start_tick[j], start_tick[j+1]).
 *
 * T1 overflow proof (spec §7): the intermediate dticks*ns_per_quarter*sr
 * with dticks <= 2^40, ns_per_quarter <= 2^40, sr <= 2^18 is <= 2^98,
 * which fits the 128-bit intermediate with 30 bits to spare. The divisor
 * ppq*1e9 fits uint64 for every uint32 ppq (max ~4.3e18 < 2^63), and the
 * quotient fits uint64 across the whole in-proof musical range
 * (e.g. ppq=96: divisor 9.6e10, quotient <= 2^62).
 *
 * Arithmetic has two limbs with identical results: native
 * `unsigned __int128` where available, otherwise an exact two-limb
 * portable fallback (64-bit halves) selected with -DRI_NO_INT128.
 * Pure C99, stdint.h only, no allocation, bounded loops only.
 */
#include "engine/seq/clock.h"

#if defined(RI_NO_INT128) || !defined(__SIZEOF_INT128__)

struct ri_u128 { uint64_t hi; uint64_t lo; };

/* Exact 64x64 -> 128 multiply via 32-bit halves. Exact for all inputs;
 * callers additionally stay inside the T1 proof bounds above. */
static struct ri_u128 ri_mul64(uint64_t a, uint64_t b) {
    uint64_t a0 = (uint32_t)a;
    uint64_t a1 = a >> 32;
    uint64_t b0 = (uint32_t)b;
    uint64_t b1 = b >> 32;
    uint64_t p0 = a0 * b0;
    uint64_t p1 = a0 * b1;
    uint64_t p2 = a1 * b0;
    uint64_t p3 = a1 * b1;
    uint64_t mid = p1 + p2;
    uint64_t mid_carry = (mid < p1) ? (1ULL << 32) : 0ULL;
    struct ri_u128 r;
    r.lo = p0 + (mid << 32);
    r.hi = p3 + (mid >> 32) + mid_carry + ((r.lo < p0) ? 1ULL : 0ULL);
    return r;
}

/* Exact 128x64 -> 128 multiply. Exact whenever the true product fits 128
 * bits; the clock's T1 bounds (<= 2^98) always fit. */
static struct ri_u128 ri_mul_u128_u64(struct ri_u128 t, uint64_t s) {
    struct ri_u128 lo = ri_mul64(t.lo, s);
    struct ri_u128 r;
    r.lo = lo.lo;
    r.hi = lo.hi + t.hi * s;
    return r;
}

static struct ri_u128 ri_add_u64(struct ri_u128 n, uint64_t v) {
    struct ri_u128 r;
    r.lo = n.lo + v;
    r.hi = n.hi + ((r.lo < n.lo) ? 1ULL : 0ULL);
    return r;
}

/* 128/64 -> 64 division with remainder. Binary long division, exactly 128
 * bounded iterations. Requires d > 0 and d <= 2^63-1 (clock divisor
 * ppq*1e9 <= ~4.3e18 always satisfies this), so (rem << 1) | bit fits. */
static uint64_t ri_div_u128_u64(struct ri_u128 n, uint64_t d, uint64_t *rem_out) {
    uint64_t q_hi = 0ULL;
    uint64_t q_lo = 0ULL;
    uint64_t rem = 0ULL;
    int i;
    for (i = 127; i >= 0; i--) {
        uint64_t bit;
        if (i >= 64)
            bit = (n.hi >> (i - 64)) & 1ULL;
        else
            bit = (n.lo >> i) & 1ULL;
        rem = (rem << 1) | bit;
        if (rem >= d) {
            rem -= d;
            if (i >= 64)
                q_hi |= (1ULL << (i - 64));
            else
                q_lo |= (1ULL << i);
        }
    }
    if (rem_out)
        *rem_out = rem;
    /* In-proof clock quotients fit 64 bits, so q_hi is 0 and the value
     * is q_lo; out-of-proof quotients truncate to the low 64 bits. */
    return q_lo | (q_hi << 32 << 32);
}

static uint64_t ri_seg_samples(uint64_t dt, uint64_t nspq, uint64_t sr,
                               uint64_t den, int do_round) {
    struct ri_u128 num = ri_mul_u128_u64(ri_mul64(dt, nspq), sr);
    if (do_round)
        num = ri_add_u64(num, den / 2ULL);
    return ri_div_u128_u64(num, den, 0);
}

#else

__extension__ typedef unsigned __int128 ri_u128n;

static uint64_t ri_seg_samples(uint64_t dt, uint64_t nspq, uint64_t sr,
                               uint64_t den, int do_round) {
    ri_u128n num = (ri_u128n)dt * (ri_u128n)nspq * (ri_u128n)sr;
    if (do_round)
        num += den / 2U;
    return (uint64_t)(num / den);
}

#endif

/* Cached segment-index hint for monotonic schedule walks. Validated on
 * every call (map pointer + range checks), so non-monotonic lookups fall
 * back correctly; it only ever skips segments, never changes the sum. */
static const struct RITempoMap *ri_hint_map = 0;
static uint32_t ri_hint_idx = 0;

static uint32_t ri_find_seg(const struct RITempoMap *m, uint64_t tick) {
    uint32_t k = 0U;
    if (m == ri_hint_map && ri_hint_idx < m->n)
        k = ri_hint_idx;
    for (; k + 1U < m->n && m->segs[k + 1U].start_tick <= tick; k++) {
        /* seek forward to the owning segment */
    }
    for (; k > 0U && m->segs[k].start_tick > tick; k--) {
        /* seek back for non-monotonic lookups */
    }
    ri_hint_map = m;
    ri_hint_idx = k;
    return k;
}

static uint64_t ri_map_tick_mode(const struct RITempoMap *m, uint64_t tick,
                                 int do_round) {
    uint64_t total = 0ULL;
    uint64_t den;
    uint32_t k;
    uint32_t j;
    if (!m || !m->segs || m->n == 0U || m->ppq == 0U || m->sr == 0U)
        return 0ULL;
    den = (uint64_t)m->ppq * 1000000000ULL;
    k = ri_find_seg(m, tick);
    for (j = 0U; j <= k; j++) {
        uint64_t seg_end = (j + 1U < m->n) ? m->segs[j + 1U].start_tick : tick;
        uint64_t dt;
        if (seg_end > tick)
            seg_end = tick;
        if (tick <= m->segs[j].start_tick)
            continue;
        dt = seg_end - m->segs[j].start_tick;
        if (dt == 0ULL)
            continue;
        total += ri_seg_samples(dt, m->segs[j].ns_per_quarter,
                                (uint64_t)m->sr, den, do_round);
    }
    return total;
}

uint64_t ri_map_tick(const struct RITempoMap *m, uint64_t tick) {
    return ri_map_tick_mode(m, tick, 1);
}

uint64_t ri_map_tick_floor(const struct RITempoMap *m, uint64_t tick) {
    return ri_map_tick_mode(m, tick, 0);
}
