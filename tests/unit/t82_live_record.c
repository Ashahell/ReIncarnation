/* t82_live_record — G9.5 recording from the panel (host half).
 * RECORD-state touch writes the back lane AND sounds immediately via the
 * control plane; Stop ends the pass and publishes; a fresh playback
 * session chases the recorded value and renders bit-identical; lane-full
 * raises the sticky flag (shown, never silent).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/live.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"

#define SR 48000.0f
#define N 512u

static struct RIPatternBank BA, BB, B808, B909;
static struct RISongTrack TR;
static struct RIEvent SC[512];

static void fixture(void) {
    uint32_t i;
    ri_bank_init(&BA, 0u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&BB, 1u, RI_PATTERN_KIND_303, 0u);
    ri_bank_init(&B808, 2u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_808);
    ri_bank_init(&B909, 3u, RI_PATTERN_KIND_DRUM, RI_DRUM_CLASS_909);
    for (i = 0u; i < 32u; i++) {
        ri_pattern_set_length(&BA.pat[i], 16u);
        ri_pattern_set_length(&BB.pat[i], 16u);
        ri_pattern_set_length(&B808.pat[i], 16u);
        ri_pattern_set_length(&B909.pat[i], 16u);
    }
    for (i = 0u; i < 16u; i++)
        ri_p303_set(&BA.pat[0], i, 6u,
            (i == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&TR);
}

int main(void) {
    static struct RIAutoEv ev0[16], ev1[16];
    static struct RIAutoPub pub;
    static struct RIAutoPass pass;
    static struct RIAutoCarry cA, cB;
    static struct RIControlPlane ctl;
    const struct RIPatternBank *b4[4];
    struct RILiveSession a, b;
    static float aL[128], aR[128], bL[128], bR[128];
    struct RIAutoLane *bk;

    fixture();
    b4[0] = &BA;
    b4[1] = &BB;
    b4[2] = &B808;
    b4[3] = &B909;
    memset(&pass, 0, sizeof pass);

    ri_auto_pub_init(&pub, ev0, 16u, ev1, 16u);
    ri_ctl_init(&ctl);
    cA.next = 0u;
    cB.next = 0u;

    /* Record: touch in RECORD punches, writes the grid event, sounds now. */
    ri_live_init(&a, 96u, SR, 120.0f, RI_ENGINE_S303A, SC, 512u);
    ri_live_set_banks(&a, b4, &TR, 0);
    ri_live_set_auto(&a, &pub, &cA, &pass);
    ri_live_set_ctl(&a, &ctl);
    ri_live_record(&a);
    RI_ASSERT(a.tr.state == RI_TR_RECORD, "record state");
    RI_ASSERT(ri_live_record_touch(&a, RI_CTL_303A_CUTOFF, 100u) == 0,
        "touch ok");
    bk = ri_auto_pub_back(&pub);
    RI_ASSERT(bk->n == 1u && bk->ev[0].tick == 0u && bk->ev[0].val == 100u,
        "back lane holds the touch");
    RI_ASSERT(ri_ctl_pending(&ctl) == 1u, "control sounds immediately");
    RI_ASSERT(ri_live_render(&a, aL, aR, 128u) == 128u, "record render");
    RI_ASSERT(ri_ctl_pending(&ctl) == 0u, "control drained");
    ri_live_stop(&a);
    RI_ASSERT(a.tr.state == RI_TR_STOPPED, "stop ends pass");

    /* Playback from the published lane: chases the recorded value. */
    ri_live_init(&b, 96u, SR, 120.0f, RI_ENGINE_S303A, SC, 512u);
    ri_live_set_banks(&b, b4, &TR, 0);
    ri_live_set_auto(&b, &pub, &cB, &pass);
    ri_live_play(&b);
    RI_ASSERT(ri_live_render(&b, bL, bR, 128u) == 128u, "playback render");
    RI_ASSERT(!memcmp(aL, bL, sizeof aL), "record == playback L");
    RI_ASSERT(!memcmp(aR, bR, sizeof aR), "record == playback R");

    /* Outside RECORD nothing is stored (conflict law: manual still sounds). */
    ri_live_init(&b, 96u, SR, 120.0f, RI_ENGINE_S303A, SC, 512u);
    ri_live_set_banks(&b, b4, &TR, 0);
    ri_live_set_auto(&b, &pub, &cB, &pass);
    ri_live_set_ctl(&b, &ctl);
    ri_live_play(&b);
    {
        const struct RIAutoLane *fr = ri_auto_pub_front(&pub);
        uint32_t before = fr->n;
        RI_ASSERT(ri_live_record_touch(&b, RI_CTL_303A_RESO, 77u) == 0,
            "manual sounds");
        RI_ASSERT(ri_auto_pub_front(&pub)->n == before,
            "playing touch stores nothing");
        RI_ASSERT(ri_ctl_pending(&ctl) == 1u, "manual goes to the plane");
    }

    /* Lane full is shown, never silent. */
    {
        static struct RIAutoEv f0[4], f1[4];
        static struct RIAutoPub fp;
        static struct RIAutoPass fpass;
        struct RIAutoLane *fb;
        uint32_t tks[4] = { 0u, 12u, 24u, 36u };
        uint16_t cts[4] = { RI_CTL_303A_CUTOFF, RI_CTL_303A_RESO,
            RI_CTL_303A_ENVMOD, RI_CTL_303A_DECAY };
        uint8_t vls[4] = { 1u, 2u, 3u, 4u };
        memset(&fpass, 0, sizeof fpass);
        ri_auto_pub_init(&fp, f0, 4u, f1, 4u);
        fb = ri_auto_pub_back(&fp);
        RI_ASSERT(ri_auto_load_triples(fb, tks, cts, vls, 4u) == 0, "fill");
        RI_ASSERT(ri_auto_touch(fb, &fpass, RI_TR_RECORD, 48u, 96u,
                RI_CTL_303A_ACCENT, 9u) == 2, "full refuses");
        RI_ASSERT((fb->flags & RI_AUTO_FLAG_FULL) != 0u, "FULL shown");
    }

    RI_RESULT("live_record");
}
