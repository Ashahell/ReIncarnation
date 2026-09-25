/* t74_player — §12.9c streaming player.
 * Task 1 first (RED: player.h does not exist yet).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/player.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };

/* Four banks, one per instance: 303A, 303B, 808, 909. Static: ~17 KB. */
static struct RIPatternBank BA, BB, B808, B909;
static void banks_4(struct RIPatternBank **out4) {
    uint32_t s;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&BB, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&B909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (s = 0u; s < 32u; s++) {
        ri_pattern_set_length(&BA.pat[s], 16u);
        ri_pattern_set_length(&BB.pat[s], 16u);
        ri_pattern_set_length(&B808.pat[s], 16u);
        ri_pattern_set_length(&B909.pat[s], 16u);
    }
    /* Slot 0: one sounding step each (slot 0 is first-class, never silent). */
    ri_p303_set(&BA.pat[0], 0u, 6u, 0u);
    ri_p303_set(&BB.pat[0], 0u, 8u, 0u);
    ri_pdrum_set(&B808.pat[0], 0u, (uint32_t)RI_L808_BD, (uint32_t)RI_HIT_LOW);
    ri_pdrum_set(&B909.pat[0], 0u, (uint32_t)RI_L909_BD, (uint32_t)RI_HIT_LOW);
    out4[0] = &BA; out4[1] = &BB; out4[2] = &B808; out4[3] = &B909;
}

int main(void) {
    struct RIPatternBank *b4[4];
    const struct RIPatternBank *c4[4];
    struct RISongTrack tr;
    struct RIPlayer pl;
    uint32_t i;
    (void)MAP;
    banks_4(b4);
    for (i = 0u; i < 4u; i++) c4[i] = b4[i];
    /* Cold start seeds sounding = pending = track selection at bar 0. */
    ri_track_init(&tr);
    ri_track_capture(&tr, 0u, 0u, 5u);
    ri_track_capture(&tr, 0u, 1u, 7u);
    ri_track_capture(&tr, 0u, 3u, 3u);
    ri_player_init(&pl, c4, &tr, 0u);
    RI_ASSERT(pl.phase_ticks[0] == 0u && pl.phase_ticks[3] == 0u, "cold phase");
    RI_ASSERT(pl.sounding_slot[0] == 5u && pl.pending_slot[0] == 5u, "cold 303A");
    RI_ASSERT(pl.sounding_slot[1] == 7u && pl.pending_slot[1] == 7u, "cold 303B");
    RI_ASSERT(pl.sounding_slot[2] == 0u && pl.pending_slot[2] == 0u, "cold 808 slot0");
    RI_ASSERT(pl.sounding_slot[3] == 3u && pl.pending_slot[3] == 3u, "cold 909");
    RI_ASSERT(pl.track_carry.known == 0u, "cold track carry");
    RI_ASSERT(pl.sched_carry[0].valid == 0u, "cold tie carry");
    RI_ASSERT(pl.banks[0] == &BA && pl.banks[3] == &B909, "bank pointers held");
    /* Start bar past the end clamps to the last valid start (never 999). */
    ri_track_capture(&tr, 998u, 0u, 9u);
    ri_player_init(&pl, c4, &tr, 999u);
    RI_ASSERT(pl.sounding_slot[0] == 9u, "clamp start 999->998");
    ri_player_init(&pl, c4, &tr, 0u);
    /* Refresh swaps ONLY the pointers — never phase/sounding/pending/carries. */
    pl.phase_ticks[1] = 48u;
    pl.sched_carry[1].valid = 1u; pl.sched_carry[1].held_note = 60u;
    pl.track_carry.known = 0x0Fu;
    {
        const struct RIPatternBank *d4[4];
        d4[0] = &BB; d4[1] = &BA; d4[2] = &B909; d4[3] = &B808;
        ri_player_refresh_banks(&pl, d4);
        RI_ASSERT(pl.banks[0] == &BB && pl.banks[1] == &BA, "refresh swaps");
        RI_ASSERT(pl.phase_ticks[1] == 48u, "refresh keeps phase");
        RI_ASSERT(pl.sounding_slot[0] == 5u && pl.pending_slot[0] == 5u, "refresh keeps slots");
        RI_ASSERT(pl.sched_carry[1].valid == 1u, "refresh keeps tie carry");
        RI_ASSERT(pl.track_carry.known == 0x0Fu, "refresh keeps track carry");
    }
    ri_player_init(&pl, c4, &tr, 0u);
    /* NULL safety: NULL player no-ops; NULL banks entry = silent instance
     * (frozen phase), NULL track = slot 0 seeds. Must not crash. */
    ri_player_init(0, c4, &tr, 0u);
    ri_player_refresh_banks(0, c4);
    ri_player_refresh_banks(&pl, 0);
    {
        const struct RIPatternBank *n4[4];
        n4[0] = 0; n4[1] = c4[1]; n4[2] = c4[2]; n4[3] = c4[3];
        ri_player_init(&pl, n4, &tr, 0u);
        RI_ASSERT(pl.banks[0] == 0, "null bank held");
        RI_ASSERT(pl.sounding_slot[0] == 5u && pl.pending_slot[0] == 5u, "null bank still tracks");
    }
    ri_player_init(&pl, c4, 0, 0u);
    RI_ASSERT(pl.sounding_slot[1] == 0u, "null track seeds 0");
    /* Layer guard: player.h may name the seq-side headers, nothing else. */
    {
        FILE *fh = fopen("engine/seq/player.h", "r");
        char line[256];
        int bad = 0, has_model = 0;
        RI_ASSERT(fh != 0, "open player header");
        if (fh) {
            while (fgets(line, sizeof line, fh)) {
                if (strstr(line, "songtrack"))
                    has_model = 1;
                /* BANNED set, permanent: project/engine-voice layers live
                 * downstream. Never delete a name here to make a build pass —
                 * narrow the INCLUDES instead (see Task 4 R7 fallback). */
                if (strstr(line, "rbng.h") || strstr(line, "riseq.h") ||
                    strstr(line, "engine/dsp/") || strstr(line, "RITransport") ||
                    strstr(line, "RISeq") || strstr(line, "mixer") ||
                    strstr(line, "route.h"))
                    bad = 1;
            }
            fclose(fh);
        }
        RI_ASSERT(has_model, "player builds on the track");
        RI_ASSERT(!bad, "player layer leak");
    }
    /* ---- advance + changeover ---- */
    {
        struct RIPlayer pl;
        struct RIEvent ev[256];
        struct RISongTrack tr;
        uint64_t bar = 4u * 96u; /* ticks per bar at ppq 96 */
        uint32_t n, k, s;
        banks_4(b4);
        for (k = 0u; k < 4u; k++) c4[k] = b4[k];
        /* Single-hit 303 slots with distinct pitches (sounding transitions
         * are visible in NOTE_ON values, no oracle needed). */
        for (s = 0u; s < 16u; s++) {
            ri_p303_set(&BB.pat[1], s, 9u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
            ri_p303_set(&BA.pat[2], s, 4u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
            ri_p303_set(&BA.pat[4], s, 2u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
            ri_p303_set(&BA.pat[6], s, 11u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
        }
        ri_pattern_set_length(&BB.pat[1], 6u); /* 144 ticks: ends avoid downbeats */
        /* Unequal lengths, no track change: phase marches per section. */
        ri_pattern_set_length(&BA.pat[0], 16u);   /* 384 ticks = 1 bar */
        ri_pattern_set_length(&BB.pat[0], 8u);    /* 192 ticks = half bar */
        ri_track_init(&tr);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 0u, "len16 wraps in 1 bar");
        RI_ASSERT(pl.phase_ticks[1] == 0u, "len8 wraps twice in 1 bar");
        RI_ASSERT(pl.sounding_slot[0] == 0u, "no change, no flip");
        /* p.20 deferral (R-DEFERRAL): BB len 6 ends at 144/288/432 —
         * none a downbeat. Selection at bar 1 fires its
         * PATTERN_CHANGE at the downbeat; the flip takes effect at the
         * section's pattern end per R10 sample-before-flip, so content
         * at/after the downbeat is already the new slot. Old-pitch
         * survival past the downbeat is geometrically excluded (it
         * would need an old end exactly at the downbeat with the flip
         * skipped — the opposite order to what coincidence pins), so
         * this block asserts the change record + pending/sounding +
         * new content after the end, never old pitch. */
        ri_track_capture(&tr, 1u, 1u, 1u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 2u * bar, ev, 256u);
        RI_ASSERT(pl.pending_slot[1] == 1u, "pending sampled at downbeat");
        {
            uint32_t changes = 0u, newnotes = 0u;
            uint64_t dbs = ri_map_tick(&MAP, ri_seq_tick_of_bar(96u, 1u));
            uint64_t ends = ri_map_tick(&MAP, 432u);
            uint8_t newnote = ri_p303_note(&BB.pat[1].row.r303[0]);
            for (k = 0u; k < n; k++) {
                if (ev[k].type == RI_EV_PATTERN_CHANGE && ev[k].device == 1u) {
                    changes++;
                    RI_ASSERT(ev[k].sample == dbs, "change at downbeat sample");
                }
                if (ev[k].device == 1u && ev[k].type == RI_EV_NOTE_ON &&
                    ev[k].sample >= ends) {
                    RI_ASSERT(ev[k].value == (uint16_t)newnote, "new sounds after end");
                    newnotes++;
                }
            }
            RI_ASSERT(changes == 1u, "one recorded change, got %u", changes);
            RI_ASSERT(newnotes > 0u, "new content after the end (%u)", newnotes);
        }
        RI_ASSERT(pl.sounding_slot[1] == 1u, "sounding flips at pattern end");
        /* COINCIDENCE order: 303A len 16 ends exactly at bar-1 downbeat.
         * The flip must use the just-sampled selection (stale seed must not
         * sound a single occurrence). */
        ri_track_capture(&tr, 1u, 0u, 4u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 2u * bar, ev, 256u);
        RI_ASSERT(pl.sounding_slot[0] == 4u, "coincident downbeat wins");
        {
            uint64_t dbs = ri_map_tick(&MAP, ri_seq_tick_of_bar(96u, 1u));
            uint8_t want = ri_p303_note(&BA.pat[4].row.r303[0]);
            uint32_t good = 0u, badn = 0u;
            for (k = 0u; k < n; k++)
                if (ev[k].device == 0u && ev[k].type == RI_EV_NOTE_ON &&
                    ev[k].sample >= dbs) {
                    if (ev[k].value == (uint16_t)want) good++;
                    else badn++;
                }
            RI_ASSERT(good > 0u && badn == 0u, "post-downbeat content is slot 4 (%u/%u)",
                good, badn);
        }
        /* SAME-BAR overwrite: two captures at one bar, latest wins in the
         * grid; the player samples it once (single change, flip to latest). */
        ri_track_capture(&tr, 3u, 0u, 6u);
        ri_track_capture(&tr, 3u, 0u, 2u);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 4u * bar, ev, 256u);
        RI_ASSERT(pl.pending_slot[0] == 2u, "overwrite latest wins");
        RI_ASSERT(pl.sounding_slot[0] == 2u, "flip to latest");
        {
            uint8_t stale = ri_p303_note(&BA.pat[6].row.r303[0]);
            uint32_t badn = 0u;
            for (k = 0u; k < n; k++)
                if (ev[k].device == 0u && ev[k].type == RI_EV_NOTE_ON &&
                    ev[k].value == (uint16_t)stale)
                    badn++;
            RI_ASSERT(badn == 0u, "superseded slot never sounds (%u)", badn);
        }
        /* Simultaneous live length edits on two unequal instances flip at
         * their own ticks inside one block, never the bar line. */
        ri_pattern_set_length(&BA.pat[2], 8u);    /* 192 ticks */
        ri_pattern_set_length(&B808.pat[0], 4u);  /* 96 ticks */
        ri_track_init(&tr);
        ri_player_init(&pl, c4, &tr, 5u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 5u * bar, 6u * bar, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 0u && pl.phase_ticks[2] == 0u, "edits wrap even");
        /* Bank content mutation sounds with NO refresh (live read). */
        ri_p303_set(&BA.pat[0], 1u, 4u, 0u);
        {
            uint32_t m1 = 0u, m2 = 0u, q, n2;
            struct RIPlayer p2;
            struct RIEvent e2[256];
            ri_player_init(&p2, c4, &tr, 5u);
            n2 = ri_player_block(&p2, &tr, 0, &MAP, 96u, 5u * bar, 6u * bar, e2, 256u);
            for (q = 0u; q < n; q++)
                if (ev[q].device == 0u && ev[q].type == RI_EV_NOTE_ON) m1++;
            for (q = 0u; q < n2; q++)
                if (e2[q].device == 0u && e2[q].type == RI_EV_NOTE_ON) m2++;
            RI_ASSERT(m1 > 0u && m2 == m1 + 1u, "live mutation adds one hit (%u vs %u)", m1, m2);
        }
        /* Phase independence: a tick jump between blocks neither resets
         * phase nor moves the changeover. */
        ri_pattern_set_length(&BA.pat[0], 16u);
        ri_track_init(&tr);
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 192u, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 192u, "half pattern, phase 192");
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 5000u, 5000u + 192u, ev, 256u);
        RI_ASSERT(pl.phase_ticks[0] == 0u, "seek jumps tick, phase still wraps even");
        RI_ASSERT(pl.sounding_slot[0] == 0u, "seek moves no changeover");
        /* Corrupt length: silent, no hang. */
        BA.pat[0].length = 0u;
        ri_player_init(&pl, c4, &tr, 0u);
        n = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, 4u * bar, ev, 256u);
        {
            uint32_t hits = 0u;
            for (k = 0u; k < n; k++)
                if (ev[k].device == 0u && ev[k].type == RI_EV_NOTE_ON) hits++;
            RI_ASSERT(hits == 0u, "corrupt length silent (%u)", hits);
        }
        RI_ASSERT(pl.phase_ticks[0] == 0u, "corrupt length frozen");
        banks_4(b4); /* restore: corruption/mutations must not leak into Task 3 */
    }
    RI_RESULT("player");
}
