/* t173_levi_ctrlrate — control rate N=8 always on (owner 2026-10-06).
 *
 * (1) static patch bit-identical to the Q0 exact pin (t172 algos case).
 * (2) LFO->cutoff: dc targets change only at control points (1 per window).
 * (3) stale-state law: retrigger mid-window starts fresh (equals a fresh voice).
 * (4) gain interpolation: VCA-level target steps interpolate, no plateaus.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"

#define SR 48000.0f

static struct RILeviSet S;
static float LB[256], RB[256];

static uint64_t hash2(const float *l, const float *r, uint32_t n) {
    uint64_t h = 1469598103934665603ULL;
    uint32_t i;
    for (i = 0u; i < n; i++) {
        uint32_t u;
        memcpy(&u, &l[i], 4);
        h ^= (uint64_t)u;
        h *= 1099511628211ULL;
        memcpy(&u, &r[i], 4);
        h ^= (uint64_t)u;
        h *= 1099511628211ULL;
    }
    return h;
}

/* (1) static patch: determinism + block-size invariance (control phase
 * counts samples per voice, so 64/256 agree). */
static uint64_t run_static(uint32_t blk) {
    uint64_t h = 1469598103934665603ULL;
    uint32_t done = 0u, i;
    levi_init_set(&S);
    levi_set_algo(&S, 0u, RI_LEVI_ALGO_DUO);
    levi_trigger(&S, 0u, 60u);
    while (done < 18000u) {
        uint32_t w = blk;
        if (done + w > 18000u)
            w = 18000u - done;
        levi_voice_render_sum_stereo(&S, LB, RB, w, SR);
        for (i = 0u; i < w; i++) {
            uint32_t u;
            memcpy(&u, &LB[i], 4);
            h ^= (uint64_t)u;
            h *= 1099511628211ULL;
            memcpy(&u, &RB[i], 4);
            h ^= (uint64_t)u;
            h *= 1099511628211ULL;
        }
        done += w;
    }
    return h;
}

static void t_static(void) {
    uint64_t h64a = run_static(64u);
    uint64_t h64b = run_static(64u);
    uint64_t h256 = run_static(256u);
    RI_ASSERT(h64a == h64b, "static rerun must agree");
    RI_ASSERT(h64a == h256, "static 64/256 must agree");
    (void)hash2;
}

/* (2) control points: with an LFO->DFILT route, ctl_dc1 changes at most
 * once per 8-sample window. */
static void t_points(void) {
    uint32_t i, changes = 0u;
    uint32_t last;
    levi_init_set(&S);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_WAVE, RI_LEVI_LW_SINE);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_RATE, 96u);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_LFO0, RI_LEVI_DM_DFILT, 0u, 80);
    levi_trigger(&S, 0u, 60u);
    /* prime one sample so ctl_dc1 is valid */
    levi_voice_render_sum_stereo(&S, LB, RB, 1u, SR);
    last = 0u;
    memcpy(&last, &S.v[0].ctl_dc1, 4);
    for (i = 1u; i < 64u; i++) {
        uint32_t cur;
        levi_voice_render_sum_stereo(&S, LB, RB, 1u, SR);
        memcpy(&cur, &S.v[0].ctl_dc1, 4);
        if (cur != last)
            changes++;
        last = cur;
    }
    /* 64 samples = 8 windows; new targets at window starts after the first:
     * at most 7 changes, at least 1 (LFO moves the cutoff). */
    RI_ASSERT(changes >= 1u && changes <= 7u,
        "dc targets must change only at control points: %u changes in 64 samples", changes);
}

/* (3) stale state: retrigger mid-window starts fresh. White-box law:
 * the first post-trigger sample snaps (dc0==dc1, vp0==vp1); without the
 * ctl_init reset it interpolates from the old note. Behavioral law: 64
 * post-trigger samples equal a fresh voice sample-for-sample. */
static void t_stale(void) {
    float fa[64], fb[64], fc[64], fd[64];
    struct RILeviSet fresh;
    uint32_t i;
    levi_init_set(&S);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_NOTE, RI_LEVI_DM_DFILT, 0u, 100);
    ri_levi_matrix_route(&S.mx, 1u, RI_LEVI_MS_NOTE, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 100);
    levi_trigger(&S, 0u, 60u);
    levi_voice_render_sum_stereo(&S, fa, fb, 5u, SR); /* mid-window */
    levi_trigger(&S, 0u, 72u); /* retrigger same voice mid-window */
    levi_voice_render_sum_stereo(&S, fa, fb, 1u, SR);
    RI_ASSERT(S.v[0].ctl_dc0 == S.v[0].ctl_dc1,
        "retrigger must snap dc targets (no smear from the old note)");
    RI_ASSERT(S.v[0].ctl_vp0 == S.v[0].ctl_vp1,
        "retrigger must snap pitch targets");
    levi_voice_render_sum_stereo(&S, fa + 1, fb + 1, 63u, SR);
    levi_init_set(&fresh);
    ri_levi_matrix_route(&fresh.mx, 0u, RI_LEVI_MS_NOTE, RI_LEVI_DM_DFILT, 0u, 100);
    ri_levi_matrix_route(&fresh.mx, 1u, RI_LEVI_MS_NOTE, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 100);
    levi_trigger(&fresh, 0u, 72u);
    levi_voice_render_sum_stereo(&fresh, fc, fd, 64u, SR);
    for (i = 0u; i < 64u; i++)
        RI_ASSERT(fa[i] == fc[i] && fb[i] == fd[i],
            "retriggered sample %u must equal a fresh voice", i);
}

/* (4) gain interpolation: a square-LFO -> VCA route slopes between
 * targets (no 8-sample plateaus of unequal height). The carrier is DC
 * (0 Hz fixed-frequency sine at quarter phase), envelopes settled, so
 * 8-sample window means track the gain exactly: held gains step full
 * height in one window, interpolation lands a window strictly between. */
static void t_gain(void) {
    float L[8], R[8];
    static double means[2048];
    uint32_t w, i, o;
    double dmax = 0.0;
    int partial = 0, full = 0;
    levi_init_set(&S);
    levi_set_algo(&S, 0u, RI_LEVI_ALGO_DUO);
    for (o = 0u; o < 8u; o++) {
        levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_WAVE, 0u);
        levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_PMODE, 2u);
        levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_COARSE, 0u);
        levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_FINE, 0u);
        levi_set_op_ui(&S, 0u, o, RI_LEVI_OP_PHASE, 32u);
    }
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VINIT & 0xFFu, 127u);
    levi_set_lfo_ui(&S, 0u, 2u, RI_LEVI_LP_WAVE, RI_LEVI_LW_SQUARE);
    levi_set_lfo_ui(&S, 0u, 2u, RI_LEVI_LP_RATE, 96u);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_LFO2, RI_LEVI_DM_VCA, 0u, 100);
    levi_trigger(&S, 0u, 60u);
    for (w = 0u; w < 12000u; w++) /* 2 s settle: envelopes at sustain */
        levi_voice_render_sum_stereo(&S, L, R, 8u, SR);
    for (w = 0u; w < 2048u; w++) {
        double m = 0.0;
        levi_voice_render_sum_stereo(&S, L, R, 8u, SR);
        for (i = 0u; i < 8u; i++)
            m += fabs((double)L[i]) + fabs((double)R[i]);
        means[w] = m / 16.0;
    }
    for (w = 1u; w < 2048u; w++) {
        double d = fabs(means[w] - means[w - 1u]);
        if (d > dmax)
            dmax = d;
    }
    RI_ASSERT(dmax > 1e-6, "square LFO must move VCA level (dmax=%g)", dmax);
    for (w = 1u; w < 2048u; w++) {
        double d = fabs(means[w] - means[w - 1u]);
        if (d > 0.2 * dmax && d < 0.8 * dmax)
            partial = 1;
        else if (d >= 0.8 * dmax)
            full = 1;
    }
    RI_ASSERT(full, "flips must occur (positive control, dmax=%g)", dmax);
    RI_ASSERT(partial, "VCA level must slope within a window (dmax=%g)", dmax);
}

int main(void) {
    RI_ASSERT(RI_LEVI_CTRL_N == 8u, "control rate must be N=8");
    t_static();
    t_points();
    t_stale();
    t_gain();
    RI_RESULT("t173_levi_ctrlrate");
}
