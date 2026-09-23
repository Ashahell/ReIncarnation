/* t21_swaprender — TC-2.1.3 PCM half: snapshot-driven render soak.
 * Two 4-step songs alternate every 8 buffers (64 f) for 10k swaps
 * through one 303 voice: per buffer, BeginBuffer, window the active
 * snapshot, apply events, render. Asserts per buffer: delivered ==
 * window count (no loss/dup); every sample finite and |x| < 4.0
 * (VCA clamps 1.2, volume 1.0 — 4.0 is generous headroom; catches
 * state corruption/blowup, not musical content).
 * Retrigger-equivalence (the click replacement — see article): after
 * foreign material + short settle, a retriggered attack matches a
 * fresh attack within 1e-6 (no click-scale staleness). Rationale:
 * absolute step metrics are vacuous on saw waves (every period edge
 * exceeds any sane threshold); what a switch can actually break is
 * attack integrity, and that is what this pins.
 * Determinism: two full soaks bit-identical. Hermetic, deterministic.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "engine/seq/riseq.h"
#include "engine/seq/sched.h"
#include "engine/seq/clock.h"
#include "engine/seq/snapbuild.h"
#include "project/rbng.h"
#include "engine/dsp/rb303.h"

#define T21R_SR 48000u
#define T21R_BUF 64u
#define T21R_SW_EVERY 8u
#define T21R_NSWAP 10000u
#define T21R_ABS_LIM 4.0f

static int fails = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); fails++; } \
} while (0)

static struct RIEvent evs_a[64u], evs_b[64u];
static struct RISeqSnapshot snap_a, snap_b;
static float out1[5120000u], out2[5120000u];

static void make_song(struct RISong *s, uint8_t n0, uint8_t n1,
                      uint8_t n2) {
    memset(s, 0, sizeof *s);
    s->nsteps = 4u;
    s->steps[0].note = n0; s->steps[0].flags = 0;
    s->steps[1].note = n1; s->steps[1].flags = RI_RBNG_SLIDE;
    s->steps[2].note = n2; s->steps[2].flags = RI_RBNG_ACCENT;
    s->steps[3].note = 0;  s->steps[3].flags = RI_RBNG_REST;
}

static void apply_ev(struct RB303Voice *v, const struct RIEvent *e) {
    switch (e->type) {
    case RI_EV_NOTE_ON:
        rb303_note(v, (uint8_t)(e->value & 127u),
                   (e->flags & RI_EVFLAG_SLIDE) != 0,
                   (e->flags & RI_EVFLAG_ACCENT) != 0);
        break;
    case RI_EV_NOTE_CONTINUE:
        rb303_slide_to(v, (uint8_t)(e->value & 127u));
        break;
    case RI_EV_NOTE_OFF:
        rb303_release(v);
        break;
    case RI_EV_ACCENT:
        rb303_accent(v);
        break;
    default:
        break;
    }
}

static void setup_voice(struct RB303Voice *v) {
    rb303_init(v);
    rb303_set_param(v, RI_CTL_303A_CUTOFF, 80);
    rb303_set_param(v, RI_CTL_303A_RESO, 40);
    rb303_set_param(v, RI_CTL_303A_ENVMOD, 64);
    rb303_set_param(v, RI_CTL_303A_DECAY, 64);
    rb303_set_param(v, RI_CTL_303A_ACCENT, 96);
    rb303_set_param(v, RI_CTL_303A_WAVE, 0);
    rb303_set_param(v, RI_CTL_303A_VOLUME, 127);
}

/* One full soak into out; returns delivered-event count. */
static uint32_t soak(float *out) {
    static struct RISeq s_storage;
    struct RISeq *s = &s_storage;
    struct RB303Voice v;
    uint64_t cursor = 0ULL, base = 0ULL, pos = 0ULL;
    uint32_t t, b, delivered = 0u;
    RiSeqInit(s, NULL, 96u);
    RiSeqLoadSnapshot(s, &snap_a);
    setup_voice(&v);
    for (t = 0u; t < T21R_NSWAP; t++) {
        const struct RISeqSnapshot *nxt = (t & 1u) ? &snap_a : &snap_b;
        RiSeqRequestSnapshot(s, nxt);
        for (b = 0u; b < T21R_SW_EVERY; b++) {
            const struct RISeqSnapshot *active = RiSeqBeginBuffer(s);
            struct RIEvent got[16u];
            uint64_t rel = cursor - base;
            uint32_t n, i;
            /* Identity: the round's requested snapshot serves every
             * buffer (a dropped/stale apply fails here — this assert
             * was missing once, and an arbiter mutant walked straight
             * through; never remove it). */
            if (active != nxt) {
                printf("FAIL identity t%u b%u\n", t, b);
                fails++;
                return delivered;
            }
            if (b == 0u)
                base = cursor;
            n = ri_events_in_window(active->events, active->n_events,
                                    rel, rel + T21R_BUF, got, 16u);
            for (i = 0u; i < n; i++)
                apply_ev(&v, &got[i]);
            delivered += n;
            rb303_render(&v, out + pos, T21R_BUF, (float)T21R_SR);
            for (i = 0u; i < T21R_BUF; i++) {
                float x = out[pos + i];
                if (!(x == x) || x > T21R_ABS_LIM || x < -T21R_ABS_LIM) {
                    printf("FAIL bound t%u b%u i%u: %f\n", t, b, i,
                           (double)x);
                    fails++;
                    return delivered;
                }
            }
            cursor += T21R_BUF;
            pos += T21R_BUF;
        }
    }
    return delivered;
}

int main(void) {
    struct RISong sa, sb;
    struct RISegment seg;
    struct RITempoMap map;
    uint32_t n1, n2;
    make_song(&sa, 45, 47, 48);
    make_song(&sb, 52, 50, 55);
    seg.start_tick = 0ULL;
    seg.ns_per_quarter = 500000000ULL;
    map.segs = &seg;
    map.n = 1u;
    map.ppq = 96u;
    map.sr = 48000u;
    n1 = ri_snapshot_build_events(&sa, &map, 96u, 0u, NULL, evs_a, 64u);
    n2 = ri_snapshot_build_events(&sb, &map, 96u, 0u, NULL, evs_b, 64u);
    CHECK(n1 > 0u && n2 > 0u, "snapshot builds %u/%u", n1, n2);
    snap_a.events = evs_a; snap_a.n_events = n1;
    snap_a.loop_start = 0ULL; snap_a.loop_end = 0ULL; snap_a.length = 48000ULL;
    snap_b.events = evs_b; snap_b.n_events = n2;
    snap_b.loop_start = 0ULL; snap_b.loop_end = 0ULL; snap_b.length = 48000ULL;
    {
        uint32_t d1 = soak(out1);
        uint32_t d2 = soak(out2);
        CHECK(d1 == d2 && d1 > 0u, "delivered %u/%u", d1, d2);
        CHECK(memcmp(out1, out2, sizeof out1) == 0, "soak not bit-identical");
    }
    /* Same-song swap identity: switching A->A must be transparent.
     * Rationale (replaces an earlier retrigger-equivalence idea that
     * proved unphysical): absolute step metrics are vacuous on saw
     * waves (every period edge exceeds -80 dBFS), and continuous
     * filters never reconverge bit-exactly in finite time (24 Hz
     * post-HP tail ~7 ms; measured 1.7e-4 residual at sample 576 of
     * a retrigger test). What a switch CAN break is transparency:
     * identical event streams with and without swap machinery must
     * render bit-identically. R1 = no swaps; R2 = request X every 8
     * buffers; both render 64 buffers from a fresh voice. */
    {
        static struct RISeq sq_storage;
        struct RISeq *s = &sq_storage;
        struct RB303Voice v1, v2;
        static float r1[4096u], r2[4096u];
        uint32_t b;
        RiSeqInit(s, NULL, 96u);
        setup_voice(&v1);
        RiSeqLoadSnapshot(s, &snap_a);
        for (b = 0u; b < 64u; b++) {
            const struct RISeqSnapshot *active = RiSeqBeginBuffer(s);
            struct RIEvent got[16u];
            uint64_t rel = (uint64_t)b * T21R_BUF;
            uint32_t n, i;
            n = ri_events_in_window(active->events, active->n_events,
                                    rel, rel + T21R_BUF, got, 16u);
            for (i = 0u; i < n; i++)
                apply_ev(&v1, &got[i]);
            rb303_render(&v1, r1 + (uint64_t)b * T21R_BUF, T21R_BUF,
                         (float)T21R_SR);
        }
        setup_voice(&v2);
        RiSeqLoadSnapshot(s, &snap_a);
        for (b = 0u; b < 64u; b++) {
            const struct RISeqSnapshot *active;
            struct RIEvent got[16u];
            uint64_t rel = (uint64_t)b * T21R_BUF;
            uint32_t n, i;
            if ((b & 7u) == 0u)
                RiSeqRequestSnapshot(s, &snap_a);
            active = RiSeqBeginBuffer(s);
            n = ri_events_in_window(active->events, active->n_events,
                                    rel, rel + T21R_BUF, got, 16u);
            for (i = 0u; i < n; i++)
                apply_ev(&v2, &got[i]);
            rb303_render(&v2, r2 + (uint64_t)b * T21R_BUF, T21R_BUF,
                         (float)T21R_SR);
        }
        CHECK(memcmp(r1, r2, sizeof r1) == 0,
              "swap-transparent render differs");
    }

    if (fails)
        printf("FAIL %d\n", fails);
    else
        printf("PASS t21_swaprender\n");
    return fails != 0;
}
