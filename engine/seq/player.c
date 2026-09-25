/* player.c — streaming player (spec 2026-09-25 §§1–2).
 * State advance is tick-domain; samples come only from ri_map_tick
 * inside the emitters. No alloc, no IO, no mutable static state. */
#include "engine/seq/player.h"
#include <stddef.h> /* NULL (player.h is stdint.h-only by layer law) */

void ri_player_init(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES],
    const struct RISongTrack *t, uint64_t start_bar) {
    uint32_t i;
    uint64_t sb;
    if (!p)
        return;
    sb = start_bar;
    if (sb >= (uint64_t)RI_SONGTRACK_BARS)
        sb = (uint64_t)RI_SONGTRACK_BARS - 1u; /* end boundary never a start */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++) {
        uint8_t sel = t ? ri_track_selected(t, sb, i) : 0u;
        p->phase_ticks[i] = 0u;
        p->sounding_slot[i] = sel;
        p->pending_slot[i] = sel;
        p->sched_carry[i].valid = 0u;
        p->sched_carry[i].held_note = 0u;
        p->sched_carry[i].pad[0] = 0u;
        p->sched_carry[i].pad[1] = 0u;
        p->banks[i] = (banks != 0) ? banks[i] : 0;
    }
    p->track_carry.known = 0u;
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        p->track_carry.prev[i] = 0u;
}

void ri_player_refresh_banks(struct RIPlayer *p,
    const struct RIPatternBank * const banks[RI_SONGTRACK_INSTANCES]) {
    uint32_t i;
    if (!p || !banks)
        return; /* pointers only — never phase, never sounding */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        p->banks[i] = banks[i];
}

/* Live pattern length in ticks (H4: against the block-global step size only;
 * no pattern-local tempo enters here). 0 = silent/frozen (H3). */
static uint64_t player_live_ticks(const struct RIPatternBank *b, uint8_t slot,
    uint32_t step_ticks) {
    const struct RIPattern *pat;
    if (!b)
        return 0u;
    if (slot >= RI_PATTERN_BANK_PATTERNS)
        slot = 0u; /* first-class slot law: corrupt reads as 0, never a crash */
    pat = &b->pat[slot];
    if (pat->length == 0u || pat->length > RI_PATTERN_STEPS)
        return 0u;
    return (uint64_t)pat->length * (uint64_t)step_ticks;
}

static const struct RIPattern *player_slot_pat(const struct RIPatternBank *b,
    uint8_t slot) {
    if (!b)
        return 0;
    if (slot >= RI_PATTERN_BANK_PATTERNS)
        slot = 0u;
    return &b->pat[slot];
}

/* WRAP RULE (mechanical, local — H2): evaluable at the exact instant of the
 * wrap with only values present then. Entry catch-up with no prior emission
 * in this call (last_b == 0) always takes the zero branch: a live-shortened
 * pattern never inherits a tie. Static function, not a macro: identical
 * truth table, no macro-hygiene risk (Task 2 Ruling, see ledger). */
static void player_wrap_carry(struct RISchedCarry *cin,
    const struct RIPatternBank *last_b, uint8_t last_slot,
    const struct RISchedCarry *last_cout,
    const struct RIPatternBank *b, uint8_t snd) {
    if (last_b != 0 && last_b == b && last_slot == snd && last_cout != 0 &&
        last_cout->valid) {
        *cin = *last_cout; /* same bank+slot chains the tie */
    } else {
        cin->valid = 0u;
        cin->held_note = 0u;
        cin->pad[0] = 0u;
        cin->pad[1] = 0u;
    }
}

/* Pure: emit ONE occurrence of one sounding pattern, window-filtered.
 * Touches NO player state: cin is an input, cout an output, appends go
 * through the shared n/cap/seq. Drum occurrences ignore carries
 * (cout stays cin on that path by construction). */
static uint32_t player_emit_occurrence(const struct RIPattern *pat, uint16_t device,
    const struct RITempoMap *map, uint64_t occ_tick, uint32_t ppq,
    const struct RISchedCarry *cin, struct RISchedCarry *cout,
    uint64_t s0, uint64_t s1,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    struct RIEvent scratch[RI_SCHED_MAX_EVENTS];
    struct RISchedCarry ci, co;
    uint32_t m, k, added = 0u;
    if (!pat || !cin || !cout || !out || !n || !seq)
        return 0u;
    ci = *cin;
    co = ci;
    m = ri_sched_emit_pattern(pat, device, map, occ_tick, ppq, NULL,
        &ci, &co, scratch, RI_SCHED_MAX_EVENTS);
    *cout = co;
    for (k = 0u; k < m; k++) {
        if (scratch[k].sample < s0 || scratch[k].sample >= s1)
            continue; /* outside this block */
        if (*n >= cap)
            break; /* tail drops first, deterministic */
        out[*n] = scratch[k];
        out[*n].seq = (*seq)++;
        (*n)++;
        added++;
    }
    return added;
}

#define RI_PLAYER_MAX_WRAPS 64u

/* Per-instance advance: owns its while loop, its wrap rule, its write-back.
 * Only this function mutates phase_ticks[i]/sounding_slot[i]/sched_carry[i]. */
static void player_advance_instance(struct RIPlayer *p, uint32_t i,
    const struct RITempoMap *map, uint32_t ppq, uint32_t step_ticks,
    uint64_t tick_start, uint64_t tick_end, uint64_t s0, uint64_t s1,
    struct RIEvent *out, uint32_t *n, uint32_t cap, uint32_t *seq) {
    const struct RIPatternBank *b;
    const struct RIPatternBank *last_b = 0; /* last EMITTED occurrence */
    uint8_t last_slot = 0u;
    struct RISchedCarry last_cout;
    struct RISchedCarry cin; /* cin of the unfinished occurrence (R4) */
    uint64_t cur, phase, len;
    uint8_t snd;
    uint32_t wraps = 0u;
    if (!p || !map || !out || !n || !seq || i >= RI_SONGTRACK_INSTANCES)
        return;
    b = p->banks[i];
    if (!b)
        return; /* silent instance: frozen. Nothing touched, not even phase. */
    phase = p->phase_ticks[i];
    snd = p->sounding_slot[i];
    cin = p->sched_carry[i];
    last_cout = cin;
    len = player_live_ticks(b, snd, step_ticks);
    if (len == 0u)
        return; /* H3: zero-check BEFORE any while. Trip-count is backstop only. */
    cur = tick_start;
    while (cur < tick_end && wraps <= RI_PLAYER_MAX_WRAPS) {
        while (phase >= len && wraps <= RI_PLAYER_MAX_WRAPS) {
            snd = p->pending_slot[i];
            phase -= len;
            wraps++;
            player_wrap_carry(&cin, last_b, last_slot, &last_cout, b, snd);
            len = player_live_ticks(b, snd, step_ticks);
            if (len == 0u)
                goto writeback; /* shortened onto corruption: freeze */
        }
        if (phase >= len)
            break; /* trip-count exhausted: fail-closed */
        {
            uint64_t seg_end = cur + (len - phase);
            const struct RIPattern *pat;
            struct RISchedCarry cout = cin;
            if (seg_end > tick_end)
                seg_end = tick_end;
            pat = player_slot_pat(b, snd);
            player_emit_occurrence(pat, (uint16_t)i, map, cur - phase, ppq,
                &cin, &cout, s0, s1, out, n, cap, seq);
            last_b = b;
            last_slot = snd;
            last_cout = cout;
            phase += seg_end - cur;
            cur = seg_end;
            if (phase >= len) { /* pattern end inside this block */
                snd = p->pending_slot[i];
                phase -= len;
                player_wrap_carry(&cin, last_b, last_slot, &last_cout, b, snd);
            }
        }
        wraps++;
    }
writeback:
    p->phase_ticks[i] = phase;
    p->sounding_slot[i] = snd;
    p->sched_carry[i] = cin; /* cin of the still-unfinished occurrence (R4) */
}

uint32_t ri_player_block(struct RIPlayer *p, const struct RISongTrack *t,
    const struct RILoop *loop, const struct RITempoMap *map, uint32_t ppq,
    uint64_t tick_start, uint64_t tick_end, struct RIEvent *out, uint32_t cap) {
    uint32_t pq, step_ticks, i, n = 0u, seq = 0u;
    uint64_t s0, s1, ls = 0u, llen = 0u, b, b0, b1;
    struct RILoop l;
    int looping = 0;
    if (!p || !map || !out || cap == 0u || tick_end <= tick_start)
        return 0u; /* fail closed: NO state change */
    if (cap > RI_SCHED_MAX_EVENTS)
        cap = RI_SCHED_MAX_EVENTS;
    pq = ri_ppq_or_default(ppq); /* H4: the ONLY tempo input, normalized once */
    step_ticks = pq / 4u; /* 16th grid, sched.c convention */
    if (step_ticks == 0u)
        step_ticks = 24u;
    s0 = ri_map_tick(map, tick_start);
    s1 = ri_map_tick(map, tick_end);
    if (loop) {
        l = *loop;
        ri_loop_clamp(&l, (uint64_t)RI_SONGTRACK_BARS);
        if (l.on && l.len_bars > 0u) {
            looping = 1;
            ls = (uint64_t)l.start_bar;
            llen = (uint64_t)l.len_bars;
        }
    }
    /* STEP 1: pending + walker — downbeats strictly inside
     * (tick_start, tick_end), folded. A downbeat exactly at tick_end
     * opens the next block; sampling it here would preview a selection
     * whose content lies outside this block (Task 2 Ruling R-STEP1). */
    b0 = ri_seq_bar_at_tick(tick_start, pq);
    b1 = ri_seq_bar_at_tick(tick_end, pq);
    for (b = b0; b <= b1; b++) {
        uint64_t db = ri_seq_tick_of_bar(pq, b);
        uint64_t fb;
        if (db <= tick_start || db >= tick_end)
            continue;
        fb = b;
        if (looping && fb >= ls + llen)
            fb = ls + ((fb - ls) % llen); /* fold PRESERVES PHASE */
        if (fb >= (uint64_t)RI_SONGTRACK_BARS)
            continue; /* past the end with no loop: nothing to say */
        for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
            p->pending_slot[i] = ri_track_selected(t, fb, i); /* ONLY writer */
        ri_track_emit_measure(t, fb, &p->track_carry, map, pq,
            out, &n, cap, &seq);
    }
    /* STEPS 2+3: per-instance advance (order across instances irrelevant —
     * instances share only n/cap/seq, appended in 0..3 order). */
    for (i = 0u; i < RI_SONGTRACK_INSTANCES; i++)
        player_advance_instance(p, i, map, pq, step_ticks,
            tick_start, tick_end, s0, s1, out, &n, cap, &seq);
    /* FINAL: insertion sort by the §8 total key (bounded: n <= cap <= 256). */
    for (b = 1u; b < (uint64_t)n; b++) {
        struct RIEvent key = out[b];
        uint64_t j = b;
        while (j > 0u && ri_event_less(&key, &out[j - 1u])) {
            out[j] = out[j - 1u];
            j--;
        }
        out[j] = key;
    }
    return n;
}
