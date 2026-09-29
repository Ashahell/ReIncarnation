/* t115_master_live — S4b master as song data (2026-09-29, owner reversal).
 * Owner 2026-09-29 reverses the S4 monitoring E0: master strip level is
 * song data on lane key 0x0B50 (deviation from the manual p. 72-73). The
 * render task applies it to the post-master gain, RECORD writes lane
 * events, ATRK carries it. Unity is bit-neutral; other values scale by
 * the P-17 square law; post-master L/R peaks publish (mono twins).
 * RED-first: stub plane refuses master (send == 2, allow-list shut,
 * touch writes no lane, ATRK rejects the ID).
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/live.h"
#include "engine/engine.h"
#include "engine/seq/pattern.h"
#include "engine/seq/songtrack.h"
#include "engine/seq/ctlplane.h"
#include "engine/seq/autolane.h"
#include "project/rbng.h"

#define SR 48000.0f
#define BPM 120.0f
#define PPQ 96u
#define TOTAL 4800u

static struct RIPatternBank BA, BB, B808, B909;
static struct RISongTrack TR;
static struct RIEvent SC1[512], SC2[512], SC3[512];

static void fixture(void) {
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
        ri_p303_set(&BA.pat[0], s, 6u, (s == 0u) ? 0u : (uint8_t)RI_STEP_REST);
    ri_track_init(&TR);
}

static void banks4(const struct RIPatternBank **out) {
    out[0] = &BA;
    out[1] = &BB;
    out[2] = &B808;
    out[3] = &B909;
    out[4] = 0;
}

/* Live render of the fixture; master < 0 means no master moves. */
static uint32_t render_master(uint32_t chunk, int master, float *ol, float *or_) {
    struct RILiveSession s;
    struct RIControlPlane ctl;
    const struct RIPatternBank *b4[5];
    uint32_t done = 0u, nbuf = 0u;
    banks4(b4);
    ri_ctl_init(&ctl);
    ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SC1, 512u);
    ri_live_set_banks(&s, b4, &TR, 0);
    ri_live_set_ctl(&s, &ctl);
    if (master >= 0)
        RI_ASSERT(ri_ctl_send(&ctl, RI_CTL_MASTER_LEVEL, (uint8_t)master) == 0,
            "master send %d", master);
    ri_live_play(&s);
    while (done < TOTAL) {
        uint32_t want = TOTAL - done, got;
        if (want > chunk)
            want = chunk;
        got = ri_live_render(&s, ol + done, or_ + done, want);
        RI_ASSERT(got == want, "live renders chunk");
        if (got == 0u)
            break;
        done += got;
        if (++nbuf > 512u)
            break;
    }
    return done;
}

static double rms_tail(const float *l, const float *r, uint32_t n) {
    double acc = 0.0;
    uint32_t i;
    for (i = 0u; i < n; i++) {
        acc += (double)l[i] * l[i];
        acc += (double)r[i] * r[i];
    }
    return sqrt(acc / (double)(2u * n));
}

int main(void) {
    static float aL[TOTAL], aR[TOTAL], bL[TOTAL], bR[TOTAL], cL[TOTAL], cR[TOTAL];
    uint32_t i;
    fixture();

    /* Plane: master is song data (admitted like any lane key). */
    {
        struct RIControlPlane p;
        struct RIEvent ev[8];
        uint32_t seq = 0u, n;
        ri_ctl_init(&p);
        RI_ASSERT(ri_auto_allowed(RI_CTL_MASTER_LEVEL), "master allowed (S4b)");
        RI_ASSERT(ri_ctl_send(&p, RI_CTL_MASTER_LEVEL, 100u) == 0, "send admits master");
        n = ri_ctl_drain(&p, ev, 8u, 0u, &seq);
        RI_ASSERT(n == 1u, "drain one");
        RI_ASSERT(ev[0].type == RI_EV_AUTOMATION && ev[0].value == RI_CTL_MASTER_LEVEL &&
            (ev[0].flags & 127u) == 100u, "drain automation");
    }

    /* Gain: explicit unity is bit-neutral; 64 scales by the square law. */
    RI_ASSERT(render_master(256u, -1, aL, aR) == TOTAL, "base total");
    RI_ASSERT(render_master(256u, 127, bL, bR) == TOTAL, "unity total");
    RI_ASSERT(render_master(256u, 64, cL, cR) == TOTAL, "scaled total");
    {
        uint32_t loud = 0u;
        for (i = 0u; i < TOTAL; i++)
            if (aL[i] != 0.0f || aR[i] != 0.0f)
                loud++;
        RI_ASSERT(loud > TOTAL / 2u, "fixture audible");
    }
    {
        uint32_t same = 1u;
        for (i = 0u; i < TOTAL; i++)
            if (aL[i] != bL[i] || aR[i] != bR[i])
                same = 0u;
        RI_ASSERT(same, "unity bit-neutral");
    }
    {
        double ra = rms_tail(aL + TOTAL - 2048u, aR + TOTAL - 2048u, 2048u);
        double rc = rms_tail(cL + TOTAL - 2048u, cR + TOTAL - 2048u, 2048u);
        double want = (64.0 / 127.0) * (64.0 / 127.0);
        RI_ASSERT(ra > 0.0, "base level %f", ra);
        RI_ASSERT(rc / ra > want - 0.02 && rc / ra < want + 0.02,
            "master ratio %f want %f", rc / ra, want);
    }

    /* Meters: post-master peaks publish (mono path: L/R twins). */
    {
        struct RILiveSession s;
        struct RIControlPlane ctl;
        const struct RIPatternBank *b4[5];
        static float ol[256], or_[256];
        banks4(b4);
        ri_ctl_init(&ctl);
        ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SC2, 512u);
        ri_live_set_banks(&s, b4, &TR, 0);
        ri_live_set_ctl(&s, &ctl);
        ri_live_play(&s);
        RI_ASSERT(ri_live_render(&s, ol, or_, 256u) == 256u, "meter render");
        RI_ASSERT(ri_engine_master_peak(&s.eng, 0u) > 0.0f, "master L peak");
        RI_ASSERT(ri_engine_master_peak(&s.eng, 1u) ==
            ri_engine_master_peak(&s.eng, 0u), "master twins");
        {
            struct RILiveMeters m;
            int rc, tries = 0;
            memset(&m, 0, sizeof m);
            do {
                rc = ri_live_meters_read(&s, &m);
            } while (rc == 1 && ++tries < 4);
            RI_ASSERT(rc == 0, "snapshot read");
            RI_ASSERT(m.master_peak[0] > 0.0f, "snapshot master peak");
            RI_ASSERT(m.master_peak[1] == m.master_peak[0], "snapshot twins");
        }
    }

    /* Recorder: RECORD writes a master lane event; PLAY sounds but writes
     * nothing (touch refuses off-record, the plane send still lands). */
    {
        static struct RIAutoEv ev0[16], ev1[16];
        static struct RIAutoPub pub;
        static struct RIAutoPass pass;
        static struct RIAutoCarry cA;
        static struct RIControlPlane ctl;
        const struct RIPatternBank *b4[5];
        struct RILiveSession s;
        struct RIAutoLane *bk;
        uint8_t v = 0u;
        banks4(b4);
        memset(&pass, 0, sizeof pass);
        ri_auto_pub_init(&pub, ev0, 16u, ev1, 16u);
        ri_ctl_init(&ctl);
        cA.next = 0u;
        ri_live_init(&s, PPQ, SR, BPM, RI_ENGINE_S303A, SC3, 512u);
        ri_live_set_banks(&s, b4, &TR, 0);
        ri_live_set_auto(&s, &pub, &cA, &pass);
        ri_live_set_ctl(&s, &ctl);
        ri_live_record(&s);
        bk = ri_auto_pub_back(&pub);
        RI_ASSERT(ri_live_record_touch(&s, RI_CTL_MASTER_LEVEL, 64u) == 0, "master touch sends");
        RI_ASSERT(ri_auto_value(bk, (uint32_t)s.cursor_ticks, RI_CTL_MASTER_LEVEL, &v) == 1 &&
            v == 64u, "master records");
        RI_ASSERT(ri_live_record_touch(&s, RI_CTL_303A_CUTOFF, 100u) == 0, "cutoff touch sends");
        RI_ASSERT(ri_auto_value(bk, (uint32_t)s.cursor_ticks, RI_CTL_303A_CUTOFF, &v) == 1 &&
            v == 100u, "cutoff records");
        ri_live_play(&s);
        {
            /* Advance past the recorded tick so the PLAY touch below
             * cannot hide behind the RECORD event it must not repeat. */
            static float dL[192], dR[192];
            RI_ASSERT(ri_live_render(&s, dL, dR, 192u) == 192u, "advance");
            RI_ASSERT(s.cursor_ticks > 0u, "cursor advanced");
        }
        RI_ASSERT(ri_live_record_touch(&s, RI_CTL_MASTER_LEVEL, 32u) == 0, "play touch sends");
        RI_ASSERT(ri_auto_value(bk, (uint32_t)s.cursor_ticks, RI_CTL_MASTER_LEVEL, &v) == 1 &&
            v == 64u, "play writes no lane (carried record)");
    }

    /* ATRK carries the master key through a song round trip. */
    {
        static struct RISong s, r;
        static struct RBAutoEv wbuf[4], rbuf[4];
        static char err[256];
        uint32_t st;
        rbng_song_init(&s);
        rbng_song_init(&r);
        s.nsteps = 16u;
        for (st = 0u; st < 16u; st++) {
            s.steps[st].note = (uint8_t)(45 + (st % 8));
            s.steps[st].flags = 0u;
        }
        wbuf[0].tick = 96u;
        wbuf[0].ctl = RI_CTL_MASTER_LEVEL;
        wbuf[0].val = 90u;
        s.atrk = wbuf;
        s.atrk_cap = 4u;
        s.natrk = 1u;
        r.atrk = rbuf;
        r.atrk_cap = 4u;
        RI_ASSERT(rbng_write_song("/tmp/ri/run/t115-m.rbng", &s, err,
            sizeof err) == 0, "write master atrk: %s", err);
        RI_ASSERT(rbng_read_song("/tmp/ri/run/t115-m.rbng", &r, err,
            sizeof err) == 0, "read master atrk: %s", err);
        RI_ASSERT(r.natrk == 1u && r.atrk[0].ctl == RI_CTL_MASTER_LEVEL &&
            r.atrk[0].val == 90u, "atrk carries master");
    }
    RI_RESULT("master_live");
}
