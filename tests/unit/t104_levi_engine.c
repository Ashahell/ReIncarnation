/* t104_levi_engine — Levi as 5th engine instance (owner 2026-09-28).
 * Option A: the Classic 4-wide world widens (instances, route, mixer
 * buses, sections bit 0x10). Shape asserts + behavior: device-4 chord
 * pattern sounds through the live session; without the SLEVI bit it is
 * silent (activation law covers instance 4).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/live.h"
#include "engine/engine.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/mixer/mixer.h"
#include "engine/fx/route.h"

#define SR 48000.0f
#define BPM 140.0f
#define PPQ 96u
#define TOTAL 48000u
#define ALL5 (RI_ENGINE_S303A | RI_ENGINE_S303B | RI_ENGINE_S808 | \
    RI_ENGINE_S909 | RI_ENGINE_SLEVI)

static struct RIPatternBank B0, B1, B2, B3, B4;
static struct RISongTrack TR;
static struct RIEvent SCR[512];

static void fixture(void) {
    uint32_t s;
    ri_bank_init(&B0, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B1, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B2, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&B3, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    ri_bank_init(&B4, 4u, RI_PATTERN_KIND_LEVI, 0u);
    for (s = 0u; s < 32u; s++) {
        ri_pattern_set_length(&B0.pat[s], 16u);
        ri_pattern_set_length(&B1.pat[s], 16u);
        ri_pattern_set_length(&B2.pat[s], 16u);
        ri_pattern_set_length(&B3.pat[s], 16u);
        ri_pattern_set_length(&B4.pat[s], 16u);
    }
    /* B-minor-ish triad stabs on steps 0 and 8 (lanes = voices). */
    ri_levi_set(&B4.pat[0], 0u, 0u, 59u, 1);
    ri_levi_set(&B4.pat[0], 0u, 1u, 62u, 1);
    ri_levi_set(&B4.pat[0], 0u, 2u, 67u, 1);
    ri_levi_set(&B4.pat[0], 8u, 0u, 57u, 1);
    ri_levi_set(&B4.pat[0], 8u, 1u, 62u, 1);
    ri_track_init(&TR);
}

static uint32_t render_mask(uint32_t sections, float *ol, float *or_) {
    struct RILiveSession s;
    const struct RIPatternBank *b5[5];
    uint32_t done = 0u;
    b5[0] = &B0;
    b5[1] = &B1;
    b5[2] = &B2;
    b5[3] = &B3;
    b5[4] = &B4;
    ri_live_init(&s, PPQ, SR, BPM, sections, SCR, 512u);
    ri_live_set_banks(&s, b5, &TR, 0);
    ri_live_play(&s);
    while (done < TOTAL) {
        uint32_t got = ri_live_render(&s, ol + done, or_ + done, TOTAL - done);
        if (got == 0u)
            break;
        done += got;
    }
    return done;
}

static uint32_t energy(const float *b, uint32_t n) {
    uint32_t i, k = 0u;
    for (i = 0u; i < n; i++)
        if (b[i] != 0.0f)
            k++;
    return k;
}

int main(void) {
    static float on_l[TOTAL], on_r[TOTAL], off_l[TOTAL], off_r[TOTAL];
    /* Shape: the 4-wide world is 5-wide (RED now). */
    RI_ASSERT(RI_SONGTRACK_INSTANCES == 5u, "instances");
    RI_ASSERT(RI_ROUTE_NSECTIONS == 5u, "route");
    RI_ASSERT(RI_MIX_NBUS == 5u, "buses");
    RI_ASSERT(ALL5 == 0x1Fu, "mask law");
    fixture();
    RI_ASSERT(render_mask(ALL5, on_l, on_r) == TOTAL, "full render");
    RI_ASSERT(energy(on_l, TOTAL) > 1000u, "levi sounds %u",
        energy(on_l, TOTAL));
    RI_ASSERT(render_mask(ALL5 & (uint32_t)~RI_ENGINE_SLEVI, off_l, off_r) == TOTAL,
        "gated render");
    RI_ASSERT(energy(off_l, TOTAL) == 0u, "silent without bit");
    RI_ASSERT(memcmp(on_l, off_l, sizeof on_l) != 0, "bit matters");
    RI_RESULT("leviengine");
}
