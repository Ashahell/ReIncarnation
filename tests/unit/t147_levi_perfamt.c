/* t147_levi_perfamt — Levi P9b performance amounts: the six panel
 * amounts (DVEL/DPAT/AVEL/APAT/VVEL/VPAT) and the two stored envelope
 * velocity params (per-op VELENV, mod-env VELCRV).
 * Laws: the amounts default to none and an explicit none is bit-identical;
 * the UI law is the P5 ENV-amount scale (64 = none, 127/0 = +/-1); velocity
 * is bipolar (soft with a positive amount equals hard with a negative one)
 * while pressure is unipolar (no pressure is neutral, bit for bit); the two
 * filters take the P5 octaves law (an exact reference cutoff pins the scale,
 * because an octave error would otherwise hide in the 20 Hz..20 kHz clamp);
 * the VCA amount scales the level and floors at silence; op VELENV scales the
 * envelope level only, mod-env VELCRV scales the envelope value (and its
 * matrix source, which reads the same value); the amounts belong to the voice
 * they were set on; keys, registry rows, panel slots and texts.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define BS 256u

static struct RILeviSet A, B;

static void run(struct RILeviSet *s, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++)
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
}

static int feq(float a, float b, float tol) {
    float d = a > b ? a - b : b - a;
    float m = a > b ? a : b;
    m = m < 0.0f ? -m : m;
    return d <= m * tol + 1e-6f;
}

/* 1 when the two sets' next stereo blocks differ (same-age twins). */
static int diff_block(struct RILeviSet *a, struct RILeviSet *b) {
    static float la[BS], lb[BS], oa[BS], ob[BS];
    uint32_t k;
    levi_voice_render_sum_stereo(a, la, lb, BS, SR);
    levi_voice_render_sum_stereo(b, oa, ob, BS, SR);
    for (k = 0u; k < BS; k++)
        if (la[k] != oa[k] || lb[k] != ob[k])
            return 1;
    return 0;
}

/* Peak of the next stereo block: the VCA amount is a pure gain, so the peak
 * ratio is the law itself. */
static float peak_of(struct RILeviSet *s) {
    static float l[BS], r[BS];
    uint32_t k;
    float p = 0.0f;
    levi_voice_render_sum_stereo(s, l, r, BS, SR);
    for (k = 0u; k < BS; k++) {
        float m = l[k] > 0.0f ? l[k] : -l[k];
        if (m > p)
            p = m;
        m = r[k] > 0.0f ? r[k] : -r[k];
        if (m > p)
            p = m;
    }
    return p;
}

/* Peak of the next mono block (the legacy sum keeps the same amounts). */
static float peak_mono(struct RILeviSet *s) {
    static float o[BS];
    uint32_t k;
    float p = 0.0f;
    levi_voice_render_sum(s, o, BS, SR);
    for (k = 0u; k < BS; k++) {
        float m = fabsf(o[k]);
        if (m > p)
            p = m;
    }
    return p;
}

/* 1 when the two sets' next mono blocks differ (same-age twins). */
static int same_mono(struct RILeviSet *a, struct RILeviSet *b) {
    static float oa[BS], ob[BS];
    uint32_t k;
    levi_voice_render_sum(a, oa, BS, SR);
    levi_voice_render_sum(b, ob, BS, SR);
    for (k = 0u; k < BS; k++)
        if (oa[k] != ob[k])
            return 0;
    return 1;
}

/* One panel amount on one voice (the section-wide apply is engine.c's). */
static void amt(struct RILeviSet *s, uint32_t voice, uint32_t key, uint8_t val) {
    RI_ASSERT(levi_set_param_ui(s, voice, key & 0xFFu, val) == 0, "amount %04x set", key);
}

/* Route source -> VCA level, depth 100 (the destination a source is
 * guaranteed to move: the level scales the amp by 1 + x). */
static void route_vca(struct RILeviSet *s, uint32_t src) {
    levi_set_mx_ui(s, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(src));
    levi_set_mx_ui(s, 0u, 1u, RI_LEVI_DM_VCA);
    levi_set_mx_ui(s, 0u, 2u, 0u);
    levi_set_mx_ui(s, 0u, 3u, 100u);
}

/* Twin pair: both sets fresh, same note and same velocity. */
static void twins(uint32_t note, uint8_t vel) {
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_note_vel(&A, (uint8_t)note, vel) == 1, "twin A note");
    RI_ASSERT(levi_note_vel(&B, (uint8_t)note, vel) == 1, "twin B note");
}

int main(void) {
    /* ---- Keys and the UI law (P5 ENV-amount scale). ---- */
    RI_ASSERT(RI_CTL_LEVI_DVEL == 0x0EBFu && RI_CTL_LEVI_DPAT == 0x0EC0u &&
        RI_CTL_LEVI_AVEL == 0x0EC1u && RI_CTL_LEVI_APAT == 0x0EC2u &&
        RI_CTL_LEVI_VVEL == 0x0EC3u && RI_CTL_LEVI_VPAT == 0x0EC4u, "amount keys");
    levi_init_set(&A);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 64u);
    RI_ASSERT(feq(A.v[0].dvel, 0.0f, 1e-6f) && A.v[0].dvel == 0.0f, "none is exactly zero");
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 127u);
    RI_ASSERT(feq(A.v[0].dvel, 1.0f, 1e-5f), "max %f", A.v[0].dvel);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 0u);
    RI_ASSERT(feq(A.v[0].dvel, -1.0f, 1e-5f), "min %f", A.v[0].dvel);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 200u);
    RI_ASSERT(feq(A.v[0].dvel, 1.0f, 1e-5f), "clamped high %f", A.v[0].dvel);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 100u);
    RI_ASSERT(feq(A.v[0].dvel, 36.0f / 63.0f, 1e-5f), "mid %f", A.v[0].dvel);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 64u);
    amt(&A, 0u, RI_CTL_LEVI_DPAT, 0u);
    amt(&A, 0u, RI_CTL_LEVI_AVEL, 127u);
    amt(&A, 0u, RI_CTL_LEVI_APAT, 127u);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 0u);
    amt(&A, 0u, RI_CTL_LEVI_VPAT, 127u);
    RI_ASSERT(feq(A.v[0].dpat, -1.0f, 1e-5f) && feq(A.v[0].avel, 1.0f, 1e-5f) &&
        feq(A.v[0].apat, 1.0f, 1e-5f) && feq(A.v[0].vvel, -1.0f, 1e-5f) &&
        feq(A.v[0].vpat, 1.0f, 1e-5f), "the other five take the same law");

    /* ---- Defaults, and an explicit none that moves nothing. ---- */
    levi_init_set(&A);
    RI_ASSERT(feq(A.v[0].dvel, 0.0f, 1e-6f) && feq(A.v[0].dpat, 0.0f, 1e-6f) &&
        feq(A.v[0].avel, 0.0f, 1e-6f) && feq(A.v[0].apat, 0.0f, 1e-6f) &&
        feq(A.v[0].vvel, 0.0f, 1e-6f) && feq(A.v[0].vpat, 0.0f, 1e-6f), "amount defaults");
    RI_ASSERT(feq(A.v[0].op[0].venv, 0.0f, 1e-6f), "op VELENV default");
    RI_ASSERT(feq(A.v[0].mvelcrv[0], 0.0f, 1e-6f), "menv VELCRV default");
    RI_ASSERT(ri_levi_op_default(0u, RI_LEVI_OP_VELENV) == 0 &&
        ri_levi_menv_default(0u, RI_LEVI_ME_VELCRV) == 64, "UI defaults 0 / 64");
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 64u);
    amt(&A, 0u, RI_CTL_LEVI_DPAT, 64u);
    amt(&A, 0u, RI_CTL_LEVI_AVEL, 64u);
    amt(&A, 0u, RI_CTL_LEVI_APAT, 64u);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 64u);
    amt(&A, 0u, RI_CTL_LEVI_VPAT, 64u);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_VELENV, 0u) == 0, "op velenv set");
    RI_ASSERT(levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_VELCRV, 64u) == 0, "menv velcrv set");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "an explicit none is bit-identical");

    /* ---- DVEL: bipolar velocity on the digital cutoff, +/-8 octaves. ---- */
    /* 10240 Hz with -1 at full velocity is exactly the 40 Hz reference: the
     * scale is pinned without sitting against the 20 Hz clamp. */
    twins(60u, 127u);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 40.0f) == 0, "reference cutoff");
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "DVEL -1 x 8 octaves is the 40 Hz twin");
    /* A hard note opens, a soft note closes: the sign decides which end. */
    twins(60u, 127u);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(diff_block(&A, &B), "a hard note opens the filter");
    twins(60u, 127u);
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(diff_block(&A, &B), "the negative end closes it");
    /* Velocity is bipolar: soft with + is hard with -. */
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 127u);
    amt(&B, 0u, RI_CTL_LEVI_DVEL, 0u);
    levi_note_vel(&A, 60u, 0u);
    levi_note_vel(&B, 60u, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "velocity is symmetric about the amount sign");

    /* ---- DPAT: unipolar per-key pressure, no pressure is neutral. ---- */
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_DPAT, 127u);
    levi_polyat(&A, 60u, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "no pressure is neutral");
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_DPAT, 127u);
    levi_polyat(&A, 60u, 127u);
    levi_polyat(&B, 60u, 0u);
    run(&A, 2u);
    run(&B, 2u);
    RI_ASSERT(diff_block(&A, &B), "full pressure opens the filter");
    /* Pressure is unipolar, so its scale is pinned from the other end: the
     * negative end at full pressure is the same 40 Hz reference. */
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 40.0f) == 0, "reference cutoff");
    amt(&A, 0u, RI_CTL_LEVI_DPAT, 0u);
    levi_note_vel(&A, 60u, 90u);
    levi_polyat(&A, 60u, 127u);
    levi_note_vel(&B, 60u, 90u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "DPAT -1 at full pressure is the 40 Hz twin");

    /* ---- AVEL / APAT: the same laws on the analog cutoff. ---- */
    twins(60u, 127u);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF2, 10240.0f) == 0, "analog cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF2, 40.0f) == 0, "reference cutoff");
    amt(&A, 0u, RI_CTL_LEVI_AVEL, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "AVEL -1 x 8 octaves is the 40 Hz twin");
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_AVEL, 127u);
    levi_polyat(&A, 60u, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "AVEL: no pressure is neutral");
    /* Analog keytrack scales the cutoff by 4 at the default, so the moves
     * laws run from a low base where +8 octaves is not clamped away. */
    twins(60u, 127u);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF2, 100.0f) == 0, "analog cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF2, 100.0f) == 0, "analog cutoff");
    amt(&A, 0u, RI_CTL_LEVI_AVEL, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(diff_block(&A, &B), "AVEL moves the analog cutoff");
    twins(60u, 90u);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF2, 100.0f) == 0, "analog cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF2, 100.0f) == 0, "analog cutoff");
    amt(&A, 0u, RI_CTL_LEVI_APAT, 127u);
    levi_polyat(&A, 60u, 127u);
    run(&A, 2u);
    run(&B, 2u);
    RI_ASSERT(diff_block(&A, &B), "APAT moves the analog cutoff");
    /* The analog amount must not touch the digital cutoff (and vice versa). */
    twins(60u, 127u);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(diff_block(&A, &B), "DVEL leaves the analog cutoff alone");

    /* ---- VVEL / VPAT: the VCA level, and silence at the negative end. ---- */
    twins(60u, 127u);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(feq(peak_of(&A) / peak_of(&B), 2.0f, 2e-3f), "a hard note is twice as loud %f",
        peak_of(&A) / peak_of(&B));
    twins(60u, 127u);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 0u);
    run(&A, 5u);
    RI_ASSERT(peak_of(&A) == 0.0f, "the negative end closes the VCA");
    twins(60u, 0u);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(feq(peak_of(&A) / peak_of(&B), 2.0f, 2e-3f), "a soft note with -1 is twice as loud");
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_VPAT, 127u);
    levi_polyat(&A, 60u, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "VPAT: no pressure is neutral");
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_VPAT, 127u);
    levi_polyat(&A, 60u, 127u);
    run(&A, 2u);
    run(&B, 2u);
    RI_ASSERT(feq(peak_of(&A) / peak_of(&B), 2.0f, 2e-3f), "full pressure doubles the VCA");
    /* The mono sum carries the same amounts (the legacy path is not a
     * second design: same laws, same scale, same signs). */
    twins(60u, 127u);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(feq(peak_mono(&A) / peak_mono(&B), 2.0f, 2e-3f), "mono path doubles too");
    twins(60u, 90u);
    amt(&A, 0u, RI_CTL_LEVI_VPAT, 127u);
    levi_polyat(&A, 60u, 127u);
    run(&A, 2u);
    run(&B, 2u);
    RI_ASSERT(feq(peak_mono(&A) / peak_mono(&B), 2.0f, 2e-3f), "mono pressure doubles too");
    {
        /* Each filter amount at its negative end is the 40 Hz reference on
         * the mono path too (a 40 Hz twin pins the 8-octave scale). */
        static const uint32_t KEY[4] = { RI_CTL_LEVI_DVEL, RI_CTL_LEVI_AVEL, RI_CTL_LEVI_DPAT,
            RI_CTL_LEVI_APAT };
        static const uint32_t CUT[4] = { RI_LEVI_CUTOFF, RI_LEVI_CUTOFF2, RI_LEVI_CUTOFF,
            RI_LEVI_CUTOFF2 };
        uint32_t k;
        for (k = 0u; k < 4u; k++) {
            twins(60u, 127u);
            RI_ASSERT(levi_set_param(&A, 0u, CUT[k], 10240.0f) == 0, "cutoff");
            RI_ASSERT(levi_set_param(&B, 0u, CUT[k], 40.0f) == 0, "reference cutoff");
            amt(&A, 0u, KEY[k], 0u);
            if (k >= 2u)
                levi_polyat(&A, 60u, 127u);
            run(&A, 5u);
            run(&B, 5u);
            RI_ASSERT(same_mono(&A, &B), "mono amount %04x is the 40 Hz twin", KEY[k]);
        }
    }
    /* Velocity is bipolar on the mono VCA too: a soft note with the negative
     * end doubles, a hard note with the positive end doubles. */
    levi_init_set(&A);
    levi_init_set(&B);
    amt(&A, 0u, RI_CTL_LEVI_VVEL, 0u);
    levi_note_vel(&A, 60u, 0u);
    levi_note_vel(&B, 60u, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(feq(peak_mono(&A) / peak_mono(&B), 2.0f, 2e-3f),
        "mono soft note with -1 doubles %g", peak_mono(&A) / peak_mono(&B));
    /* Velocity is bipolar on the mono path as well. */
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_set_param(&A, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    RI_ASSERT(levi_set_param(&B, 0u, RI_LEVI_CUTOFF, 10240.0f) == 0, "digital cutoff");
    amt(&A, 0u, RI_CTL_LEVI_DVEL, 127u);
    amt(&B, 0u, RI_CTL_LEVI_DVEL, 0u);
    levi_note_vel(&A, 60u, 0u);
    levi_note_vel(&B, 60u, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(same_mono(&A, &B), "mono velocity is symmetric about the amount sign");

    /* ---- The amount belongs to the voice it was set on. ---- */
    twins(60u, 127u);
    amt(&A, 1u, RI_CTL_LEVI_VVEL, 127u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "another voice's amount stays there");

    /* ---- Per-op VELENV: scales the envelope level only. ---- */
    twins(60u, 0u);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_VELENV, 127u) == 0, "op velenv");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(peak_of(&A) < peak_of(&B) * 0.75f, "a zero-velocity note closes the op envelope %f vs %f",
        peak_of(&A), peak_of(&B));
    /* The op level starts near the 0..1 ceiling, so a hard note with half
     * the depth is compared at a lower envelope level (away from the clamp). */
    twins(60u, 127u);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ENVL, 96u) == 0, "op envl");
    RI_ASSERT(levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_ENVL, 96u) == 0, "op envl");
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_VELENV, 127u) == 0, "op velenv");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(peak_of(&A) > peak_of(&B) * 1.3f, "a hard note opens the op envelope %f vs %f",
        peak_of(&A), peak_of(&B));
    twins(60u, 0u);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_VELENV, 64u) == 0, "op velenv");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(peak_of(&A) < peak_of(&B) * 0.75f, "the same amount closes it at zero velocity");
    /* It scales the level term, not the initial level: with the envelope out
     * of the way a zero-velocity note keeps the initial level. */
    twins(60u, 0u);
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_INIT, 127u) == 0, "op init");
    RI_ASSERT(levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_INIT, 127u) == 0, "op init");
    RI_ASSERT(levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_VELENV, 127u) == 0, "op velenv");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(feq(peak_of(&A), peak_of(&B), 1e-4f), "INIT is not scaled %f vs %f",
        peak_of(&A), peak_of(&B));

    /* ---- Mod-env VELCRV: a curve on the envelope value (and its source). ---- */
    levi_init_set(&A);
    levi_init_set(&B);
    route_vca(&A, RI_LEVI_MS_ENV0);
    route_vca(&B, RI_LEVI_MS_ENV0);
    levi_note_vel(&A, 60u, 127u);
    levi_note_vel(&B, 60u, 127u);
    RI_ASSERT(levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_VELCRV, 127u) == 0, "menv velcrv");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(peak_of(&A) > peak_of(&B) * 1.15f, "a hard note raises the ENV 1 source %f vs %f",
        peak_of(&A), peak_of(&B));
    /* The curve is bipolar about the amount sign: +1 at zero velocity and
     * -1 at full velocity both close the envelope to nothing. */
    levi_init_set(&A);
    levi_init_set(&B);
    route_vca(&A, RI_LEVI_MS_ENV0);
    route_vca(&B, RI_LEVI_MS_ENV0);
    levi_note_vel(&A, 60u, 0u);
    levi_note_vel(&B, 60u, 127u);
    RI_ASSERT(levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_VELCRV, 127u) == 0, "menv velcrv");
    RI_ASSERT(levi_set_menv_ui(&B, 0u, 0u, RI_LEVI_ME_VELCRV, 0u) == 0, "menv velcrv min");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "the curve is symmetric about the amount sign");
    levi_init_set(&A);
    levi_init_set(&B);
    route_vca(&A, RI_LEVI_MS_ENV0);
    route_vca(&B, RI_LEVI_MS_ENV0);
    levi_note_vel(&A, 60u, 0u);
    levi_note_vel(&B, 60u, 0u);
    RI_ASSERT(levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_VELCRV, 0u) == 0, "menv velcrv min");
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(peak_of(&A) > peak_of(&B) * 1.15f, "the negative end doubles it at zero velocity");
    /* The curve is per envelope: ENV 2 keeps its own law. */
    levi_init_set(&A);
    levi_init_set(&B);
    RI_ASSERT(levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_VELCRV, 127u) == 0, "menv 1 velcrv");
    RI_ASSERT(levi_set_menv_ui(&A, 0u, 1u, RI_LEVI_ME_VELCRV, 127u) == 0, "menv 2 velcrv");
    levi_set_menv_ui(&B, 0u, 0u, RI_LEVI_ME_VELCRV, 64u);
    levi_set_menv_ui(&B, 0u, 1u, RI_LEVI_ME_VELCRV, 64u);
    levi_note_vel(&A, 60u, 0u);
    levi_note_vel(&B, 60u, 0u);
    run(&A, 5u);
    run(&B, 5u);
    RI_ASSERT(!diff_block(&A, &B), "zero velocity leaves both envelopes alone");

    /* ---- Keys, registry rows, panel slots, texts. ---- */
    {
        uint32_t k;
        static const uint32_t KEY[6] = { RI_CTL_LEVI_DVEL, RI_CTL_LEVI_DPAT, RI_CTL_LEVI_AVEL,
            RI_CTL_LEVI_APAT, RI_CTL_LEVI_VVEL, RI_CTL_LEVI_VPAT };
        static const char *const LEG[6] = { "D Vel", "D Polyat", "A Vel", "A Polyat", "V Vel",
            "V Polyat" };
        static const char *const GRP[6] = { "Filter", "Filter", "Analog", "Analog", "VCA", "VCA" };
        for (k = 0u; k < 6u; k++) {
            const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | (215u + k)));
            RI_ASSERT(d && d->engine_id == KEY[k], "row %u binds", 215u + k);
            RI_ASSERT(d->kind == RI_CK_KNOB && d->min_v == 0 && d->max_v == 127 && d->def_v == 64,
                "row %u knob 0..127 default 64", 215u + k);
            RI_ASSERT(d->automatable && d->bind == RI_BIND_LEVI && !strcmp(d->group, GRP[k]) &&
                !strcmp(d->legend, LEG[k]), "row %u group/legend/bind", 215u + k);
            RI_ASSERT(ri_auto_allowed((uint16_t)KEY[k]), "key %04x allowed", KEY[k]);
        }
        RI_ASSERT(ri_auto_allowed(0x0EC4u), "0x0EC4 allowed");
        RI_ASSERT(!ri_auto_allowed(0x0EC5u) == 0, "0x0EC5 is the first zone key (P9c)");
        RI_ASSERT(RI_SLEVI_DVEL == 215u && RI_SLEVI_VPAT == 220u && RI_SLEVI_NCTL == 226u,
            "panel rows (225 + the zone page, P9c)");
    }
    {
        struct RISectLevi lv;
        uint32_t slot;
        static const uint32_t MOD[3] = { RI_SLEVI_M_DFILT, RI_SLEVI_M_AFILT, RI_SLEVI_M_VCA };
        static const uint32_t ROW[3][2] = { { RI_SLEVI_DVEL, RI_SLEVI_DPAT },
            { RI_SLEVI_AVEL, RI_SLEVI_APAT },
            { RI_SLEVI_VVEL, RI_SLEVI_VPAT } };
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        for (slot = 0u; slot < 3u; slot++) {
            RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)MOD[slot]) == 1, "module");
            RI_ASSERT(ri_slevi_enc_live(&lv, 5u) && ri_slevi_enc_live(&lv, 6u), "slots live");
            RI_ASSERT(!strcmp(ri_slevi_enc_name(&lv, 5u), "VEL>ENV") &&
                !strcmp(ri_slevi_enc_name(&lv, 6u), "POLYAT"), "slot names");
            /* Plain registry rows carry their key in ctlreg (the ctl_key path is for
             * the module param slots), so the panel law is the row value. */
            RI_ASSERT(lv.val[ROW[slot][0]] == 64 && lv.val[ROW[slot][1]] == 64, "panel default 64");
            lv.page = 0u;
            ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
            RI_ASSERT(!strcmp(tx, "0"), "amount text %s", tx);
            RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 5u, 101) == 1, "enc +37");
            RI_ASSERT(lv.val[ROW[slot][0]] == 101, "enc lands on the row");
            ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
            RI_ASSERT(!strcmp(tx, "+37"), "amount max text %s", tx);
            RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 5u, 27) == 1, "enc -37");
            ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
            RI_ASSERT(!strcmp(tx, "-37"), "amount min text %s", tx);
            /* The pressure row reads the same signed scale. */
            RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 6u, 101) == 1, "enc +37");
            ri_slevi_enc_text(&lv, 6u, tx, sizeof tx);
            RI_ASSERT(!strcmp(tx, "+37"), "pressure amount max text %s", tx);
            RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 6u, 27) == 1, "enc -37");
            ri_slevi_enc_text(&lv, 6u, tx, sizeof tx);
            RI_ASSERT(!strcmp(tx, "-37"), "pressure amount min text %s", tx);
            RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 6u) == 1 &&
                lv.val[ROW[slot][1]] == 64, "pressure row reset");
            RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 5u) == 1, "reset");
            RI_ASSERT(lv.val[ROW[slot][0]] == 64, "reset to none");
        }
    }

    /* ---- The two envelope params reach the panel slots P5 drew dim. ---- */
    {
        struct RISectLevi lv;
        uint16_t key;
        int val;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_VCA);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_OSC) == 1, "osc module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 5u, "osc pages");
        ri_slevi_press(&lv, RI_SLEVI_PAGEDN);
        ri_slevi_press(&lv, RI_SLEVI_PAGEDN);
        ri_slevi_press(&lv, RI_SLEVI_PAGEDN);
        RI_ASSERT(ri_slevi_ctl_key(&lv, RI_SLEVI_ENC0 + 5u, &key, &val) == 1 &&
            key == RI_LEVI_OPKEY(0u, RI_LEVI_OP_VELENV) && val == 0, "op VEL>ENV slot %04x", key);
        ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "0%"), "op velenv text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 5u, 127) == 1 &&
            lv.opv[0][RI_LEVI_OP_VELENV] == 127, "enc edits op velenv");
        ri_slevi_enc_text(&lv, 5u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "100%"), "op velenv max text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 5u) == 1 && lv.opv[0][RI_LEVI_OP_VELENV] == 0,
            "op velenv reset");
        ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_ENV1);
        ri_slevi_press(&lv, RI_SLEVI_PAGEDN);
        ri_slevi_press(&lv, RI_SLEVI_PAGEDN);
        RI_ASSERT(ri_slevi_ctl_key(&lv, RI_SLEVI_ENC0 + 4u, &key, &val) == 1 &&
            key == RI_LEVI_MEKEY(0u, RI_LEVI_ME_VELCRV) && val == 64, "ENV 1 vel crv slot %04x", key);
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "0%"), "vel crv text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 4u, 127) == 1 &&
            lv.mev[0][RI_LEVI_ME_VELCRV] == 127, "enc edits vel crv");
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "+100%"), "vel crv max text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 4u, 0) == 1, "vel crv min");
        ri_slevi_enc_text(&lv, 4u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "-100%"), "vel crv min text %s", tx);
        RI_ASSERT(ri_slevi_reset(&lv, RI_SLEVI_ENC0 + 4u) == 1 && lv.mev[0][RI_LEVI_ME_VELCRV] == 64,
            "vel crv reset");
    }

    /* ---- Null-safety and bounded extremes. ---- */
    RI_ASSERT(levi_set_param_ui(0, 0u, RI_CTL_LEVI_DVEL & 0xFFu, 127u) == 2, "null refused");
    RI_ASSERT(levi_set_param_ui(&A, RI_LEVI_NVOICES, RI_CTL_LEVI_DVEL & 0xFFu, 127u) == 2, "voice refused");
    {
        uint32_t i;
        static float l[BS], r[BS];
        for (i = 0u; i < 400u; i++) {
            uint32_t k, bad = 0u;
            float a, b;
            if (i % 8u == 0u) {
                levi_note_vel(&A, (uint8_t)(30u + i % 60u), (uint8_t)(i * 37u));
                levi_polyat(&A, (uint8_t)(30u + i % 60u), (uint8_t)(i * 91u));
                amt(&A, 0u, RI_CTL_LEVI_DVEL, (uint8_t)i);
                amt(&A, 0u, RI_CTL_LEVI_DPAT, (uint8_t)(i * 3u));
                amt(&A, 0u, RI_CTL_LEVI_AVEL, (uint8_t)(255u - i));
                amt(&A, 0u, RI_CTL_LEVI_APAT, (uint8_t)(i * 7u));
                amt(&A, 0u, RI_CTL_LEVI_VVEL, (uint8_t)(i * 11u));
                amt(&A, 0u, RI_CTL_LEVI_VPAT, (uint8_t)(i * 5u));
                levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_VELENV, (uint8_t)i);
                levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_VELCRV, (uint8_t)i);
            }
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < BS; k++) {
                a = l[k];
                b = r[k];
                if (!(a > -1e20f && a < 1e20f) || !(b > -1e20f && b < 1e20f))
                    bad = 1u;
            }
            RI_ASSERT(!bad, "extremes stay finite at %u", i);
        }
    }
    RI_RESULT("levi_perfamt");
}