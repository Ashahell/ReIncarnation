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
    RI_RESULT("player");
}
