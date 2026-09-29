/* t118_levi_seq_player — SEQ phrase loop in the song path (v2 feature
 * 3c-ii). Player-owned seq cfg (default off): off = baseline Levi
 * emission; on = occurrence plays the first-N-steps phrase looping;
 * deterministic across re-init; fail-closed setter.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/seq/player.h"
#include "engine/seq/pattern.h"

#define SRU 48000u
static const struct RISegment SEG0[] = { { 0, 428571428ULL } }; /* 140 BPM */
static const struct RITempoMap MAP = { SEG0, 1, 96, SRU };
static struct RIPatternBank B5[5];
static struct RIEvent EV[512];

static uint32_t levi_ons(const struct RIEvent *ev, uint32_t n, uint8_t note) {
    uint32_t i, c = 0u;
    for (i = 0u; i < n; i++)
        if (ev[i].device == 4u && ev[i].type == RI_EV_NOTE_ON &&
            (note == 0u || ev[i].value == note))
            c++;
    return c;
}

int main(void) {
    const struct RIPatternBank *c5[5];
    struct RISongTrack tr;
    struct RIPlayer pl;
    struct RILeviSeqCfg four = { 1u, 4u, { 0u, 0u } };
    uint32_t i, n0, n1;
    uint64_t bar = 384u; /* ppq 96, 4/4 */
    for (i = 0u; i < 5u; i++) {
        uint32_t kind = i < 2u ? RI_PATTERN_KIND_303 : i < 4u ? RI_PATTERN_KIND_DRUM : RI_PATTERN_KIND_LEVI;
        uint32_t cls = i == 2u ? RI_DRUM_CLASS_808 : i == 3u ? RI_DRUM_CLASS_909 : 0u;
        ri_bank_init(&B5[i], i, kind, cls);
        ri_pattern_set_length(&B5[i].pat[0], 16u);
        c5[i] = &B5[i];
    }
    RI_ASSERT(ri_levi_set(&B5[4].pat[0], 0u, 0u, 60u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&B5[4].pat[0], 1u, 0u, 62u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&B5[4].pat[0], 2u, 0u, 64u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&B5[4].pat[0], 3u, 0u, 65u, 1) == 0, "set");
    RI_ASSERT(ri_levi_set(&B5[4].pat[0], 8u, 0u, 67u, 1) == 0, "set");
    ri_track_init(&tr);
    /* Fail-closed setter. */
    RI_ASSERT(ri_player_levi_seq(0, &four) == 2, "null player");
    /* Baseline: phrase default off (5 sounding steps). */
    ri_player_init(&pl, c5, &tr, 0u);
    n0 = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, EV, 512u);
    RI_ASSERT(levi_ons(EV, n0, 0u) == 5u, "baseline 5 ons, got %u", levi_ons(EV, n0, 0u));
    /* On len 4: first four steps loop over the bar (16 ons), the
     * step-8 67 never sounds. */
    ri_player_init(&pl, c5, &tr, 0u);
    RI_ASSERT(ri_player_levi_seq(&pl, &four) == 0, "set seq");
    n1 = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, EV, 512u);
    RI_ASSERT(levi_ons(EV, n1, 0u) == 16u, "phrase 16 ons, got %u", levi_ons(EV, n1, 0u));
    RI_ASSERT(levi_ons(EV, n1, 67u) == 0u, "67 truncated out");
    /* Stream order: Levi strikes non-decreasing in time (no
     * scrambling across the phrase wrap). Gate balance is NOT
     * asserted here: a fully-held lane retriggers every step with
     * its release at the window edge (v1 same — the next block owns
     * the seam); balance of the rewrite path itself is pinned in
     * t114, direct emission in t102. */
    {
        uint32_t k;
        uint64_t prev = 0u;
        for (k = 0u; k < n1; k++)
            if (EV[k].device == 4u) {
                RI_ASSERT(EV[k].sample >= prev, "strike order");
                prev = EV[k].sample;
            }
    }
    /* Deterministic replay. */
    {
        struct RIEvent EV2[512];
        uint32_t n2;
        ri_player_init(&pl, c5, &tr, 0u);
        RI_ASSERT(ri_player_levi_seq(&pl, &four) == 0, "reset seq");
        n2 = ri_player_block(&pl, &tr, 0, &MAP, 96u, 0u, bar, EV2, 512u);
        RI_ASSERT(n2 == n1 && !memcmp(EV, EV2, (size_t)n1 * sizeof EV[0]), "replay");
    }
    RI_RESULT("leviseqplayer");
}
