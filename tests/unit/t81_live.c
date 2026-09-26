/* t81_live — G9.2 live session core.
 * Chunk-agnostic bit-exactness across device-buffer sizes, with and
 * without an automation lane, and with a control move at a buffer
 * boundary matching the same move as an offline AUTOMATION event.
 * RED-first: stub render returns 0.
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
#define TOTAL 4800u
#define XBOUND 256u

static struct RIPatternBank BA, BB, B808, B909;
static struct RISongTrack TR;
static struct RIEvent SCR1[512], SCR2[512], SCR3[512];

static void fixture(void) {
    const struct RIPatternBank *b4[4];
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
    for (s = 0u; s < 16u; s++)
        ri_p303_set(&BA.pat[0], s, 6u,
            (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&TR);
    (void)b4;
}

static void banks4(const struct RIPatternBank **out) {
    out[0] = &BA;
    out[1] = &BB;
    out[2] = &B808;
    out[3] = &B909;
}

static uint32_t render_live(uint32_t chunk, float *ol, float *or_,
    struct RIControlPlane *ctl, struct RIAutoPub *pub,
    struct RIAutoCarry *carry, int inject_at_bound) {
    struct RILiveSession s;
    const struct RIPatternBank *b4[4];
    uint32_t done = 0u, nbuf = 0u;
    banks4(b4);
    ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SCR1, 512u);
    ri_live_set_banks(&s, b4, &TR, 0);
    if (pub)
        ri_live_set_auto(&s, pub, carry, 0);
    if (ctl)
        ri_live_set_ctl(&s, ctl);
    ri_live_play(&s);
    while (done < TOTAL) {
        uint32_t want = TOTAL - done;
        uint32_t got;
        if (want > chunk)
            want = chunk;
        if (inject_at_bound && ctl && done == XBOUND)
            RI_ASSERT(ri_ctl_send(ctl, RI_CTL_303A_CUTOFF, 20u) == 0, "inject");
        got = ri_live_render(&s, ol + done, or_ + done, want);
        RI_ASSERT(got == want, "live renders full chunk");
        if (got == 0u)
            break;
        done += got;
        nbuf++;
        if (nbuf > 512u)
            break;
    }
    return done;
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

int main(void) {
    static float aL[TOTAL], aR[TOTAL], bL[TOTAL], bR[TOTAL];
    static float cL[TOTAL], cR[TOTAL], dL[TOTAL], dR[TOTAL];
    uint32_t i;
    const struct RILiveMeters *m;

    fixture();

    /* Chunk-agnostic: 64 vs 128 vs 256 vs odd 137. */
    RI_ASSERT(render_live(64u, aL, aR, 0, 0, 0, 0) == TOTAL, "live64 total");
    RI_ASSERT(render_live(128u, bL, bR, 0, 0, 0, 0) == TOTAL, "live128 total");
    RI_ASSERT(render_live(256u, cL, cR, 0, 0, 0, 0) == TOTAL, "live256 total");
    RI_ASSERT(render_live(137u, dL, dR, 0, 0, 0, 0) == TOTAL, "live137 total");
    RI_ASSERT(!differ(aL, bL, TOTAL), "64==128 L");
    RI_ASSERT(!differ(aR, bR, TOTAL), "64==128 R");
    RI_ASSERT(!differ(aL, cL, TOTAL), "64==256 L");
    RI_ASSERT(!differ(aL, dL, TOTAL), "64==137 L");
    {
        uint32_t loud = 0u;
        for (i = 0u; i < TOTAL; i++)
            if (aL[i] != 0.0f || aR[i] != 0.0f)
                loud++;
        RI_ASSERT(loud > TOTAL / 2u, "fixture audible (%u/%u)", loud, TOTAL);
    }

    /* Meters come from the engine, position follows the transport. */
    {
        struct RILiveSession s;
        const struct RIPatternBank *b4[4];
        static float ol[256], or_[256];
        banks4(b4);
        ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SCR2, 512u);
        ri_live_set_banks(&s, b4, &TR, 0);
        ri_live_play(&s);
        RI_ASSERT(ri_live_render(&s, ol, or_, 256u) == 256u, "meter render");
        m = ri_live_meters(&s);
        RI_ASSERT(m != 0, "meters present");
        RI_ASSERT(m != 0 && m->samples == 256u, "meter samples %llu",
            (unsigned long long)(m ? m->samples : 0u));
        RI_ASSERT(m != 0 && m->sec_peak[0] > 0.0f, "303A peak audible");
    }

    /* STOPPED renders silence and holds the cursor. */
    {
        struct RILiveSession s;
        const struct RIPatternBank *b4[4];
        static float ol[128], or_[128];
        uint32_t k, silent = 0u;
        banks4(b4);
        ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SCR3, 512u);
        ri_live_set_banks(&s, b4, &TR, 0);
        RI_ASSERT(ri_live_render(&s, ol, or_, 128u) == 128u, "stopped renders");
        for (k = 0u; k < 128u; k++)
            if (ol[k] == 0.0f && or_[k] == 0.0f)
                silent++;
        RI_ASSERT(silent == 128u, "stopped silence");
    }

    /* Automation lane: chunked vs single-buffer reference bit-exact. */
    {
        static struct RIAutoEv ev0[16], ev1[16];
        static struct RIAutoPub pub;
        static struct RIAutoCarry carry;
        static float eL[TOTAL], eR[TOTAL], fL[TOTAL], fR[TOTAL];
        uint32_t ticks[1] = { 10u };
        uint16_t ctls[1] = { RI_CTL_303A_CUTOFF };
        uint8_t vals[1] = { 100u };
        struct RIAutoLane *bk;
        ri_auto_pub_init(&pub, ev0, 16u, ev1, 16u);
        bk = ri_auto_pub_back(&pub);
        RI_ASSERT(ri_auto_load_triples(bk, ticks, ctls, vals, 1u) == 0, "lane load");
        ri_auto_pub_request(&pub);
        carry.next = 0u;
        RI_ASSERT(render_live(64u, eL, eR, 0, &pub, &carry, 0) == TOTAL, "auto64");
        ri_auto_pub_init(&pub, ev0, 16u, ev1, 16u);
        bk = ri_auto_pub_back(&pub);
        RI_ASSERT(ri_auto_load_triples(bk, ticks, ctls, vals, 1u) == 0, "lane reload");
        ri_auto_pub_request(&pub);
        carry.next = 0u;
        RI_ASSERT(render_live(TOTAL, fL, fR, 0, &pub, &carry, 0) == TOTAL, "auto1buf");
        RI_ASSERT(!differ(eL, fL, TOTAL), "auto chunk-agnostic L");
        RI_ASSERT(!differ(eR, fR, TOTAL), "auto chunk-agnostic R");
        RI_ASSERT(differ(eL, aL, TOTAL), "automation changes sound");
    }

    /* Control move at a buffer boundary matches an offline AUTOMATION event. */
    {
        static float gL[TOTAL], gR[TOTAL];
        static struct RIEvent off[512];
        struct RIEngine e;
        struct RIPlayer pl;
        const struct RIPatternBank *b4[4];
        struct RISegment seg;
        struct RITempoMap map;
        uint32_t n = 0u, seq = 0u, k;
        struct RIControlPlane ctl;
        static struct RIAutoEv q0[16], q1[16];
        static struct RIAutoPub pub2;
        static struct RIAutoCarry carry2;
        banks4(b4);
        ri_ctl_init(&ctl);
        ri_auto_pub_init(&pub2, q0, 16u, q1, 16u);
        carry2.next = 0u;
        RI_ASSERT(render_live(64u, gL, gR, &ctl, &pub2, &carry2, 1) == TOTAL, "ctl live");
        RI_ASSERT(differ(gL, aL, TOTAL), "control move audible");
        /* Offline: same player events + the same move as AUTOMATION at XBOUND. */
        seg.start_tick = 0u;
        seg.ns_per_quarter = (uint64_t)(60000000000.0 / (double)BPM);
        map.segs = &seg;
        map.n = 1u;
        map.ppq = PPQ;
        map.sr = (uint32_t)SR;
        ri_player_init(&pl, b4, &TR, 0u);
        n = ri_player_block(&pl, &TR, 0, &map, PPQ, 0u, 20u, off, 512u);
        {
            uint64_t xs = ri_map_tick(&map, 0u);
            (void)xs;
        }
        /* Find absolute sample of XBOUND via map of tick 1 (250 samples):
         * XBOUND=256 is inside tick 1, so the control at 256 sorts after
         * tick-1 events and before tick-2. Insert directly at 256. */
        RI_ASSERT(n + 1u < 512u, "offline fits");
        off[n].sample = XBOUND;
        off[n].type = RI_EV_AUTOMATION;
        off[n].device = 0u;
        off[n].voice = 0u;
        off[n].value = RI_CTL_303A_CUTOFF;
        off[n].flags = 20u;
        off[n].seq = seq + n;
        n++;
        for (k = 1u; k < n; k++) {
            struct RIEvent key = off[k];
            uint64_t j = k;
            while (j > 0u && ri_event_less(&key, &off[j - 1u])) {
                off[j] = off[j - 1u];
                j--;
            }
            off[j] = key;
        }
        {
            static float hL[TOTAL], hR[TOTAL];
            ri_engine_init(&e);
            ri_engine_defaults(&e);
            ri_engine_load(&e, off, n, TOTAL, RI_ENGINE_S303A);
            RI_ASSERT(ri_engine_render(&e, hL, hR, TOTAL, SR) == TOTAL, "offline render");
            RI_ASSERT(!differ(gL, hL, TOTAL), "live ctl == offline automation L");
            RI_ASSERT(!differ(gR, hR, TOTAL), "live ctl == offline automation R");
        }
    }

    RI_RESULT("live");
}
