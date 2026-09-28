/* t100_activation — engine sections enable/disable at block boundary.
 * Owner 2026-09-27 (rack requirement: user decides which devices are
 * ACTIVE; disabled = zero CPU, state kept). The sections word crosses
 * the GUI/render boundary as one atomic, read once per render call, so
 * a flip applies at the next block by construction (the industry-standard
 * lock-free flag pattern for UI->audio control).
 * - getter round-trips; NULL fail-closed;
 * - clearing 303A removes its contribution (== an all-REST bank session);
 * - re-enabling mid-stream sounds again.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/live.h"
#include "engine/engine.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"

#define SR 48000.0f
#define BPM 120.0f
#define PPQ 96u
#define TOTAL 60000u
#define ALL (RI_ENGINE_S303A | RI_ENGINE_S303B | RI_ENGINE_S808 | RI_ENGINE_S909)

static struct RIPatternBank BA, BB, B808, B909, BLEVI;
static struct RISongTrack TR;
static struct RIEvent SCR[512];

static void fixture(int sounding) {
    uint32_t s;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&BB, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&B909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_bank_init(&BLEVI, 4u, RI_PATTERN_KIND_LEVI, 0u);
    for (s = 0u; s < 32u; s++) {
        ri_pattern_set_length(&BA.pat[s], 16u);
        ri_pattern_set_length(&BB.pat[s], 16u);
        ri_pattern_set_length(&B808.pat[s], 16u);
        ri_pattern_set_length(&B909.pat[s], 16u);
        ri_pattern_set_length(&BLEVI.pat[s], 16u);
    }
    for (s = 0u; s < 16u; s++)
        ri_p303_set(&BA.pat[0], s, 6u, sounding ? 0u : (uint8_t)RI_STEP_REST);
    if (sounding) {
        /* B-minor-ish triad stab on step 0 (lanes = voices). */
        ri_levi_set(&BLEVI.pat[0], 0u, 0u, 59u, 1);
        ri_levi_set(&BLEVI.pat[0], 0u, 1u, 62u, 1);
        ri_levi_set(&BLEVI.pat[0], 0u, 2u, 67u, 1);
    }
    ri_track_init(&TR);
}

static void render_all(uint32_t sections, float *ol, float *or_) {
    struct RILiveSession s;
    const struct RIPatternBank *b5[5];
    uint32_t done = 0u;
    b5[0] = &BA;
    b5[1] = &BB;
    b5[2] = &B808;
    b5[3] = &B909;
    b5[4] = &BLEVI;
    ri_live_init(&s, PPQ, SR, BPM, sections, SCR, 512u);
    ri_live_set_banks(&s, b5, &TR, 0);
    ri_live_play(&s);
    while (done < TOTAL) {
        uint32_t got = ri_live_render(&s, ol + done, or_ + done, TOTAL - done);
        RI_ASSERT(got > 0u, "renders");
        if (got == 0u)
            break;
        done += got;
    }
    RI_ASSERT(done == TOTAL, "full render");
}

static uint32_t energy(const float *b, uint32_t from, uint32_t n) {
    /* Scaled Debut: nonzero-sample count (deterministic, D1). */
    uint32_t i, k = 0u;
    for (i = from; i < from + n; i++)
        if (b[i] != 0.0f)
            k++;
    return k;
}

int main(void) {
    static float on_l[TOTAL], on_r[TOTAL], off_l[TOTAL], off_r[TOTAL];
    static float rest_l[TOTAL], rest_r[TOTAL], re_l[TOTAL], re_r[TOTAL];
    static float levi_l[TOTAL], levi_r[TOTAL], nolevi_l[TOTAL];
    struct RILiveSession s;
    const struct RIPatternBank *b5[5];
    uint32_t done = 0u;
    /* Sections bits are one-per-device (rail mask law). */
    RI_ASSERT(ALL == 0x0Fu, "mask law");
    /* Getter round-trip + NULL fail-closed (RED on new API). */
    fixture(1);
    b5[0] = &BA;
    b5[1] = &BB;
    b5[2] = &B808;
    b5[3] = &B909;
    b5[4] = &BLEVI;
    ri_live_init(&s, PPQ, SR, BPM, ALL, SCR, 512u);
    RI_ASSERT(ri_live_sections(&s) == ALL, "get init");
    ri_live_set_sections(&s, RI_ENGINE_S808);
    RI_ASSERT(ri_live_sections(&s) == RI_ENGINE_S808, "get set");
    ri_live_set_sections(0, ALL);
    RI_ASSERT(ri_live_sections(0) == 0u, "null get");
    ri_live_set_sections(&s, ALL);
    /* Clearing 303A removes its contribution... */
    render_all(ALL, on_l, on_r);
    render_all(ALL & (uint32_t)~RI_ENGINE_S303A, off_l, off_r);
    RI_ASSERT(energy(on_l, 0u, TOTAL) > 0u, "303A sounds");
    RI_ASSERT(memcmp(on_l, off_l, sizeof on_l) != 0, "disable differs");
    /* ...down to zero: identical to an all-REST bank. */
    fixture(0);
    render_all(ALL, rest_l, rest_r);
    RI_ASSERT(memcmp(off_l, rest_l, sizeof off_l) == 0, "zero contribution");
    /* Re-enabling mid-stream sounds again (state kept: step 4 hits). */
    fixture(1);
    ri_live_init(&s, PPQ, SR, BPM, ALL & (uint32_t)~RI_ENGINE_S303A, SCR, 512u);
    ri_live_set_banks(&s, b5, &TR, 0);
    ri_live_play(&s);
    while (done < TOTAL) {
        uint32_t want = TOTAL - done, got;
        if (want > 12000u)
            want = 12000u;
        if (done == 24000u)
            ri_live_set_sections(&s, ALL);
        got = ri_live_render(&s, re_l + done, re_r + done, want);
        if (got == 0u)
            break;
        done += got;
    }
    RI_ASSERT(done == TOTAL, "re full render");
    RI_ASSERT(energy(re_l, 0u, 24000u) == 0u, "quiet while disabled");
    RI_ASSERT(energy(re_l, 24000u, TOTAL - 24000u) > 0u, "sounds after enable");
    RI_ASSERT(memcmp(re_l, off_l, sizeof re_l) != 0, "re differs from off");
    /* Levi gate (owner 2026-09-28, option A 5th bit): the 0x10 bit adds
     * the triad; clearing it renders bit-identical to the 4-device mix. */
    {
        uint32_t all5 = ALL | RI_ENGINE_SLEVI;
        RI_ASSERT(all5 == 0x1Fu, "mask law 5");
        fixture(1);
        render_all(all5, levi_l, levi_r);
        render_all(ALL, nolevi_l, re_r);
        RI_ASSERT(memcmp(on_l, levi_l, sizeof on_l) != 0, "levi contributes");
        RI_ASSERT(memcmp(on_l, nolevi_l, sizeof on_l) == 0, "levi gated silent");
    }
    RI_RESULT("activation");
}
