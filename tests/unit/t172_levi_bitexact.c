/* t172_levi_bitexact — Levi bit-exactness oracle (levi-perf P0).
 *
 * Every case is a fixed script of setter calls + note events at fixed sample
 * offsets, rendered through levi_voice_render_sum_stereo in blocks of 64 and
 * of 256 at 48000 Hz for >= 2 s (release tails included). The hash is FNV-1a
 * 64 over the uint32 bit pattern of every L and R sample (memcpy, no UB).
 *
 * Pins generated on HEAD ebc94ec before any engine change (see evidence
 * docs/evidence/levi-perf/2026-10-06-p0-oracle.md). PASS by construction.
 * Any single-bit change to Levi's output or state sequencing FAILs.
 *
 * Pin regeneration (owner-supervised only): compile with -DRI_T172_GEN
 * and run the binary; it prints the pin table.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_matrix.h"

#define SR 48000.0f
#define SEC (48000u)
#define NC 19u

static struct RILeviSet S;
static float LB[256], RB[256];
static uint64_t HH;

static void feed(float x) {
    uint32_t u;
    memcpy(&u, &x, sizeof u);
    HH ^= (uint64_t)u;
    HH *= 1099511628211ULL;
}

static void seg(uint32_t n, uint32_t blk) {
    uint32_t done = 0u;
    while (done < n) {
        uint32_t w = n - done > blk ? blk : n - done, i;
        levi_voice_render_sum_stereo(&S, LB, RB, w, SR);
        for (i = 0u; i < w; i++) {
            feed(LB[i]);
            feed(RB[i]);
        }
        done += w;
    }
}

static void trig_hold_rel(uint32_t voice, uint8_t note, uint32_t hold, uint32_t tail, uint32_t blk) {
    levi_trigger(&S, voice, note);
    seg(hold, blk);
    levi_release(&S, voice);
    seg(tail, blk);
}

/* c00: every algorithm preset (8 x 0.5 s: 0.375 hold + 0.125 tail). */
static void c_algos(uint32_t blk) {
    uint32_t a;
    levi_init_set(&S);
    for (a = 0u; a < 8u; a++) {
        levi_set_algo(&S, 0u, a);
        trig_hold_rel(0u, 60u, SEC * 3u / 8u, SEC / 8u, blk);
    }
}

/* c01: all 16 classic waves on op 0 (ALLPAR carrier), 0.25 s each. */
static void c_waves_classic(uint32_t blk) {
    uint32_t w;
    levi_init_set(&S);
    levi_set_algo(&S, 0u, RI_LEVI_ALGO_ALLPAR);
    for (w = 0u; w < 16u; w++) {
        levi_set_op_ui(&S, 0u, 0u, RI_LEVI_OP_WAVE, (uint8_t)w);
        trig_hold_rel(0u, 64u, SEC / 8u, SEC / 8u, blk);
    }
}

/* c02: one member of each family + longest CHEBY (7 x ~0.57 s). */
static void c_waves_family(uint32_t blk) {
    static const uint8_t ws[8] = { 16u, 32u, 48u, 64u, 80u, 96u, 112u, 127u };
    uint32_t i;
    levi_init_set(&S);
    levi_set_algo(&S, 0u, RI_LEVI_ALGO_ALLPAR);
    for (i = 0u; i < 8u; i++) {
        levi_set_op_ui(&S, 0u, 0u, RI_LEVI_OP_WAVE, ws[i]);
        trig_hold_rel(0u, 64u, SEC / 8u, SEC / 8u, blk);
    }
}

/* c03: op modes FM/PM/PWM/SYNC/PDSAW/PDSQ + PD default, feedback, invert,
 * direct, mute, solo. */
static void c_opmodes(uint32_t blk) {
    static const uint32_t ms[7] = { RI_LEVI_FM, RI_LEVI_PM, RI_LEVI_PWM,
        RI_LEVI_SYNC, RI_LEVI_PDSAW, RI_LEVI_PDSQ, RI_LEVI_PDPULSE };
    uint32_t i;
    levi_init_set(&S);
    for (i = 0u; i < 7u; i++) {
        levi_set_op_mode(&S, 0u, 1u, ms[i]);
        trig_hold_rel(0u, 59u, SEC / 16u, SEC / 16u, blk);
    }
    levi_init_set(&S);
    levi_set_op_ui(&S, 0u, 1u, RI_LEVI_OP_FEEDBACK, 80u);
    trig_hold_rel(0u, 59u, SEC / 8u, SEC / 8u, blk);
    levi_set_op_ui(&S, 0u, 0u, RI_LEVI_OP_INVERT, 1u);
    trig_hold_rel(0u, 59u, SEC / 8u, SEC / 8u, blk);
    levi_set_op_ui(&S, 0u, 1u, RI_LEVI_OP_DIRECT, 1u);
    trig_hold_rel(0u, 59u, SEC / 8u, SEC / 8u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_MUTELO & 0xFFu, 2u);
    trig_hold_rel(0u, 59u, SEC / 8u, SEC / 8u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_MUTELO & 0xFFu, 0u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_SOLO & 0xFFu, 2u);
    trig_hold_rel(0u, 59u, SEC / 8u, SEC / 8u, blk);
}

/* c04: every digital filter model + dpost/drive variants. */
static void c_dfilt(uint32_t blk) {
    uint32_t t;
    levi_init_set(&S);
    for (t = 0u; t < RI_LEVI_NDF; t++) {
        levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)t);
        trig_hold_rel(0u, 57u, SEC * 3u / 32u, SEC / 32u, blk);
    }
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DPOST & 0xFFu, 1u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DRIVE & 0xFFu, 127u);
    trig_hold_rel(0u, 57u, SEC / 4u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DMORPH & 0xFFu, 127u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)RI_LEVI_DF_VOWEL);
    trig_hold_rel(0u, 57u, SEC / 4u, SEC / 4u, blk);
}

/* c05: analog filter drive 0/max, resonance near self-oscillation. */
static void c_afilt(uint32_t blk) {
    levi_init_set(&S);
    levi_set_param(&S, 0u, RI_LEVI_CUTOFF2, 18000.0f);
    levi_set_param(&S, 0u, RI_LEVI_RESO2, 0.0f);
    trig_hold_rel(0u, 55u, SEC / 4u, SEC / 4u, blk);
    levi_set_param(&S, 0u, RI_LEVI_RESO2, 0.95f);
    trig_hold_rel(0u, 55u, SEC / 2u, SEC / 2u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DRIVE & 0xFFu, 127u);
    levi_set_param(&S, 0u, RI_LEVI_CUTOFF2, 400.0f);
    trig_hold_rel(0u, 55u, SEC / 2u, SEC / 2u, blk);
    levi_set_param(&S, 0u, RI_LEVI_CUTOFF2, 40.0f);
    trig_hold_rel(0u, 55u, SEC / 4u, SEC / 4u, blk);
    /* filter-clamp reach: max cutoff x max keytrack at the top note drives
     * w = pi*fc/sr into the tpt_g clamp, so a clamp mutant is caught. */
    levi_set_param(&S, 0u, RI_LEVI_CUTOFF, 18000.0f);
    levi_set_param(&S, 0u, RI_LEVI_CUTOFF2, 18000.0f);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DKEYTRK & 0xFFu, 127u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_AKEYTRK & 0xFFu, 127u);
    trig_hold_rel(0u, 127u, SEC / 4u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)RI_LEVI_DF_LP_48);
    trig_hold_rel(0u, 127u, SEC / 4u, SEC / 4u, blk);
}

/* c06: morph held at 0, 50, 100 (1 s voice + 0.5 s tail each, 2 voices). */
static void c_morph_held(uint32_t blk) {
    static const uint8_t ps[3] = { 0u, 50u, 100u };
    uint32_t i;
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK44, 0u);
    levi_set_morph(&S, 1u, RI_LEVI_ALGO_STACK44, 0u);
    for (i = 0u; i < 3u; i++) {
        levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK44, ps[i]);
        levi_set_morph(&S, 1u, RI_LEVI_ALGO_PAIRS4, ps[i]);
        levi_trigger(&S, 0u, 60u);
        levi_trigger(&S, 1u, 67u);
        seg(SEC, blk);
        levi_release(&S, 0u);
        levi_release(&S, 1u);
        seg(SEC / 2u, blk);
    }
}

/* c07: morph moved during a held note + ALGO matrix route + 3-slot cross. */
static void c_morph_move(uint32_t blk) {
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    levi_trigger(&S, 0u, 62u);
    seg(SEC / 2u, blk);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 60u);
    seg(SEC / 2u, blk);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    seg(SEC / 2u, blk);
    levi_release(&S, 0u);
    seg(SEC / 2u, blk);
    /* matrix route to ALGO while held */
    levi_trigger(&S, 0u, 62u);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_NOTE, RI_LEVI_DM_ALGO, 0u, 60);
    seg(SEC / 2u, blk);
    levi_release(&S, 0u);
    seg(SEC / 4u, blk);
    /* slot list with 3 entries crossing a step */
    levi_set_amode(&S, 0u, RI_LEVI_AMODE_MORPH);
    levi_set_slot(&S, 0u, 0u, RI_LEVI_ALGO_DUO);
    levi_set_slot(&S, 0u, 1u, RI_LEVI_ALGO_STACK8);
    levi_set_slot(&S, 0u, 2u, RI_LEVI_ALGO_PAIRS4);
    levi_trigger(&S, 0u, 62u);
    seg(SEC / 4u, blk);
    levi_set_mpos(&S, 0u, 100u);
    seg(SEC / 4u, blk);
    levi_set_mpos(&S, 0u, 150u);
    seg(SEC / 4u, blk);
    levi_release(&S, 0u);
    seg(SEC / 4u, blk);
    /* within-step move WITHOUT a state re-copy: mpos 0 -> 60 -> 0 stays
     * in step 0, so bank 1 must have ticked alongside all along. A naive
     * "skip bank 1 when emorph == 0" resumes from stale state here. */
    levi_init_set(&S);
    levi_set_amode(&S, 0u, RI_LEVI_AMODE_MORPH);
    levi_set_slot(&S, 0u, 0u, RI_LEVI_ALGO_DUO);
    levi_set_slot(&S, 0u, 1u, RI_LEVI_ALGO_STACK8);
    levi_set_slot(&S, 0u, 2u, RI_LEVI_SLOT_SILENCE);
    levi_trigger(&S, 0u, 62u);
    seg(SEC / 2u, blk);
    levi_set_mpos(&S, 0u, 60u);
    seg(SEC / 2u, blk);
    levi_set_mpos(&S, 0u, 0u);
    seg(SEC / 4u, blk);
    levi_release(&S, 0u);
    seg(SEC / 4u, blk);
    /* matrix-driven morph through zero: an LFO on ALGO swings emorph
     * across 0 with no setter re-copy between samples. Skipping bank 1
     * at exactly 0 leaves stale phases audible on every crossing. */
    levi_init_set(&S);
    levi_set_morph(&S, 0u, RI_LEVI_ALGO_STACK8, 0u);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_WAVE, RI_LEVI_LW_SINE);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_RATE, 48u);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_LFO0, RI_LEVI_DM_ALGO, 0u, 100);
    levi_trigger(&S, 0u, 62u);
    seg(SEC * 3u / 2u, blk);
    levi_release(&S, 0u);
    seg(SEC / 2u, blk);
}

/* c08: matrix empty, then one route per destination family. */
static void c_matrix_families(uint32_t blk) {
    static const uint8_t mods[14] = { RI_LEVI_DM_OSC1, RI_LEVI_DM_ENV1,
        RI_LEVI_DM_LFO1, RI_LEVI_DM_DFILT, RI_LEVI_DM_AFILT, RI_LEVI_DM_VCA,
        RI_LEVI_DM_ALGO, RI_LEVI_DM_VOICE, RI_LEVI_DM_DELAY, RI_LEVI_DM_REVERB,
        RI_LEVI_DM_PREFX, RI_LEVI_DM_POSTFX, RI_LEVI_DM_ARP, RI_LEVI_DM_SEQ };
    static const uint8_t srcs[14] = { RI_LEVI_MS_OPENV0, RI_LEVI_MS_ENV0,
        RI_LEVI_MS_LFO0, RI_LEVI_MS_NOTE, RI_LEVI_MS_OPENV1, RI_LEVI_MS_VELON,
        RI_LEVI_MS_NOTE, RI_LEVI_MS_WHEEL, RI_LEVI_MS_LFO1, RI_LEVI_MS_ENV0 + 1u,
        RI_LEVI_MS_OPENV2, RI_LEVI_MS_OPENV3, RI_LEVI_MS_LFO2, RI_LEVI_MS_NOTE };
    uint32_t i;
    levi_init_set(&S);
    levi_trigger(&S, 0u, 60u);
    seg(SEC / 4u, blk); /* empty matrix baseline */
    for (i = 0u; i < 14u; i++) {
        ri_levi_matrix_route(&S.mx, i, srcs[i], mods[i], 0u, 50);
        seg(SEC / 8u, blk);
    }
    /* UI setter path + ROUTE gate (empty-cache refresh coverage): clear the
     * program (fast path), program slot 1 NOTE->DFILT via UI fields (the
     * cache must flip), render, gate it off via ROUTE1 (must flip back). */
    for (i = 0u; i < 14u; i++)
        ri_levi_matrix_enable(&S.mx, i, 0u);
    levi_trigger(&S, 1u, 64u);
    seg(SEC / 4u, blk);
    levi_set_mx_ui(&S, 1u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_NOTE));
    levi_set_mx_ui(&S, 1u, 1u, (uint8_t)RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&S, 1u, 2u, 0u);
    levi_set_mx_ui(&S, 1u, 3u, 96u);
    seg(SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_ROUTE1 & 0xFFu, 0u);
    seg(SEC / 4u, blk);
    levi_release(&S, 1u);
    levi_release(&S, 0u);
    seg(SEC / 4u, blk);
}

/* c09: 8 routes at once + macros (knob + button). */
static void c_matrix_heavy(uint32_t blk) {
    uint32_t i;
    levi_init_set(&S);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_OPENV0, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 40);
    ri_levi_matrix_route(&S.mx, 1u, RI_LEVI_MS_LFO0, RI_LEVI_DM_DFILT, 0u, -60);
    ri_levi_matrix_route(&S.mx, 2u, RI_LEVI_MS_ENV0, RI_LEVI_DM_VCA, 0u, 70);
    ri_levi_matrix_route(&S.mx, 3u, RI_LEVI_MS_NOTE, RI_LEVI_DM_AFILT, 0u, 50);
    ri_levi_matrix_route(&S.mx, 4u, RI_LEVI_MS_VELON, RI_LEVI_DM_ENV1 + 2u, 5u, 30);
    ri_levi_matrix_route(&S.mx, 5u, RI_LEVI_MS_WHEEL, RI_LEVI_DM_VOICE, RI_LEVI_DVO_PAN, 80);
    ri_levi_matrix_route(&S.mx, 6u, RI_LEVI_MS_LFO1, RI_LEVI_DM_LFO1, 1u, 50);
    ri_levi_matrix_route(&S.mx, 7u, RI_LEVI_MS_BEND, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 100);
    levi_set_mr_ui(&S, 0u, 0u, 0u, (uint8_t)RI_LEVI_DM_DFILT);
    levi_set_mr_ui(&S, 0u, 0u, 1u, 0u);
    levi_set_mr_ui(&S, 0u, 0u, 2u, 96u);
    S.mx.mknob[0] = 96u;
    levi_set_mr_ui(&S, 1u, 0u, 0u, (uint8_t)RI_LEVI_DM_VCA);
    levi_set_mr_ui(&S, 1u, 0u, 2u, 32u);
    levi_set_mr_ui(&S, 1u, 0u, 3u, 127u);
    S.mx.mbtn[1] = 1u;
    S.pvel = 100u;
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VINIT & 0xFFu, 0u);
    levi_set_param_ui(&S, 1u, RI_CTL_LEVI_VINIT & 0xFFu, 0u);
    levi_set_menv_ui(&S, 0u, 2u, RI_LEVI_ME_LEVEL, 64u);
    levi_set_menv_ui(&S, 1u, 2u, RI_LEVI_ME_LEVEL, 64u);
    for (i = 0u; i < 2u; i++) {
        levi_trigger(&S, i, (uint8_t)(60u + i * 4u));
    }
    seg(SEC * 3u / 2u, blk);
    levi_release(&S, 0u);
    levi_release(&S, 1u);
    seg(SEC / 2u, blk);
    /* routes removed mid-note: the empty-matrix path must clear every
     * stale offset/flag exactly (a fast path that forgets melmod or the
     * *_on clears is caught here). */
    levi_trigger(&S, 0u, 60u);
    seg(SEC / 4u, blk);
    for (i = 0u; i < 8u; i++)
        ri_levi_matrix_enable(&S.mx, i, 0u);
    levi_set_mr_ui(&S, 0u, 0u, 2u, 64u);
    levi_set_mr_ui(&S, 1u, 0u, 2u, 64u);
    S.mx.mknob[0] = 0u;
    S.mx.mbtn[1] = 0u;
    seg(SEC / 2u, blk);
    levi_release(&S, 0u);
    seg(SEC / 2u, blk);
}

/* c10: per-voice LFOs consumed / not, shared trig 1/2 +/- stagger,
 * LFO-triggered mod envelopes. */
static void c_lfo(uint32_t blk) {
    levi_init_set(&S);
    /* not consumed: LFOs run, nothing listens */
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_WAVE, RI_LEVI_LW_SINE);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_RATE, 64u);
    trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
    /* consumed: LFO1 > cutoff */
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_LFO0, RI_LEVI_DM_DFILT, 0u, 80);
    levi_set_lfo_ui(&S, 0u, 0u, RI_LEVI_LP_RATE, 96u);
    trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
    /* shared trig single/off + stagger */
    S.glfo[1].trig = 1u;
    S.glfo[1].phase0 = 0.25f;
    levi_set_lfo_ui(&S, 0u, 2u, RI_LEVI_LP_TRIG, 1u);
    levi_set_lfo_ui(&S, 0u, 2u, RI_LEVI_LP_STAGGER, 64u);
    trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
    S.glfo[2].trig = 2u;
    trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
    /* LFO-triggered mod envelope */
    levi_set_menv_ui(&S, 0u, 1u, RI_LEVI_ME_TRIG1, (uint8_t)(RI_LEVI_TS_LFO1));
    levi_set_menv_ui(&S, 0u, 1u, RI_LEVI_ME_LEVEL, 127u);
    trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
}

/* c11: mod envelopes with non-zero curves + env time scaling via matrix. */
static void c_menv(uint32_t blk) {
    levi_init_set(&S);
    levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_OP_ATTACK, 40u);
    levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_OP_ACURVE, 96u);
    levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_OP_DCURVE, 32u);
    levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_OP_RCURVE, 100u);
    levi_set_menv_ui(&S, 0u, 0u, RI_LEVI_ME_LEVEL, 110u);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 2u, blk);
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_VELON, RI_LEVI_DM_ENV1, 0u, 60);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 2u, blk);
}

/* c12: pan modes/width/spread/per-op pan/centred dual-mono. */
static void c_pans(uint32_t blk) {
    uint32_t m;
    levi_init_set(&S);
    for (m = 0u; m < 3u; m++) {
        levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPANMODE & 0xFFu, (uint8_t)m);
        levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 96u);
        levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VWIDTH & 0xFFu, 127u);
        trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
    }
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPAN & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VSPREAD & 0xFFu, 127u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VOSCPAN1 & 0xFFu, 0u);
    levi_set_param_ui(&S, 0u, (RI_CTL_LEVI_VOSCPAN1 & 0xFFu) + 1u, 127u);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 4u, blk);
    levi_set_param_ui(&S, 1u, RI_CTL_LEVI_VPAN & 0xFFu, 64u);
    levi_trigger(&S, 0u, 60u);
    levi_trigger(&S, 1u, 64u);
    seg(SEC / 2u, blk);
    levi_release(&S, 0u);
    levi_release(&S, 1u);
    seg(SEC / 4u, blk);
}

/* c13: vintage decimation + exact bypass. */
static void c_vintage(uint32_t blk) {
    levi_init_set(&S);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, 100u);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VINTAGE & 0xFFu, 0u);
    trig_hold_rel(0u, 60u, SEC / 4u, SEC / 4u, blk);
}

/* c14: detune/analog-feel/vibrato+delay/glide/glissando/bend/hold. */
static void c_pitch(uint32_t blk) {
    levi_init_set(&S);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VDETUNE & 0xFFu, 96u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VAFEEL & 0xFFu, 80u);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VVIBRATE & 0xFFu, 70u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VVIBAMT & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VVIBDLY & 0xFFu, 40u);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 1u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VGLTIME & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VGLCURVE & 0xFFu, 64u);
    levi_trigger(&S, 0u, 60u);
    seg(SEC / 4u, blk);
    levi_trigger(&S, 0u, 67u);
    seg(SEC / 4u, blk);
    levi_release(&S, 0u);
    seg(SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VGLIDE & 0xFFu, 2u);
    levi_trigger(&S, 0u, 60u);
    seg(SEC / 8u, blk);
    levi_bend(&S, 2.0f);
    seg(SEC / 8u, blk);
    levi_bend(&S, 0.0f);
    levi_glide_hold(&S, 1);
    levi_trigger(&S, 0u, 64u);
    seg(SEC / 4u, blk);
    levi_glide_hold(&S, 0);
    levi_release(&S, 0u);
    seg(SEC / 4u, blk);
}

/* c15: velocity/aftertouch, zones, chord, mono/legato, steal all 8. */
static void c_perf(uint32_t blk) {
    uint32_t i;
    levi_init_set(&S);
    levi_note_vel(&S, 60u, 100u);
    seg(SEC / 4u, blk);
    levi_press(&S, 90u);
    seg(SEC / 4u, blk);
    levi_polyat(&S, 60u, 110u);
    seg(SEC / 4u, blk);
    levi_wheel(&S, 100u);
    seg(SEC / 4u, blk);
    levi_note_off(&S, 60u);
    seg(SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VVEL & 0xFFu, 96u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_VPAT & 0xFFu, 96u);
    levi_note_vel(&S, 62u, 40u);
    seg(SEC / 4u, blk);
    levi_note_off(&S, 62u);
    seg(SEC / 8u, blk);
    levi_perf_set(&S, RI_LEVI_PF_MODE, RI_LEVI_PF_MULTI);
    levi_perf_set(&S, RI_LEVI_PF_SPLITM, RI_LEVI_PF_KEYSPLIT);
    levi_note_vel(&S, 48u, 100u);
    levi_note_vel(&S, 72u, 100u);
    seg(SEC / 4u, blk);
    levi_note_off(&S, 48u);
    levi_note_off(&S, 72u);
    seg(SEC / 8u, blk);
    {
        static const uint8_t ch[3] = { 60u, 64u, 67u };
        levi_chord_set(&S, 0x07u, ch, 3);
        levi_chord_mode(&S, 1);
        levi_note_vel(&S, 60u, 100u);
        seg(SEC / 4u, blk);
        levi_note_off(&S, 60u);
        levi_chord_mode(&S, 0);
        seg(SEC / 8u, blk);
    }
    levi_set_alloc_ui(&S, RI_LEVI_POLY_MONO);
    levi_note_vel(&S, 60u, 100u);
    seg(SEC / 8u, blk);
    levi_note_vel(&S, 64u, 100u);
    seg(SEC / 8u, blk);
    levi_note_off(&S, 64u);
    seg(SEC / 8u, blk);
    levi_set_alloc_ui(&S, RI_LEVI_POLY_ROTATE);
    for (i = 0u; i < 8u; i++)
        levi_note_vel(&S, (uint8_t)(40u + i), 100u);
    seg(SEC / 4u, blk);
    levi_note_vel(&S, 90u, 100u); /* steal */
    seg(SEC / 4u, blk);
    for (i = 0u; i < 8u; i++)
        levi_note_off(&S, (uint8_t)(40u + i));
    levi_note_off(&S, 90u);
    seg(SEC / 4u, blk);
}

/* c16: FX chain bypassed vs active. */
static void c_fx(uint32_t blk) {
    levi_init_set(&S);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 4u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, 1u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DLYFB & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, 2u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_RTIME & 0xFFu, 80u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_RDRYWET & 0xFFu, 64u);
    trig_hold_rel(0u, 60u, SEC, SEC / 2u, blk);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_PTYPE & 0xFFu, 3u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_PP1 & 0xFFu, 64u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_OTYPE & 0xFFu, 8u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_OP1 & 0xFFu, 64u);
    trig_hold_rel(0u, 60u, SEC / 2u, SEC / 2u, blk);
}

/* c17: worst case — 8 voices, STACK8, morph 50, matrix-heavy. */
static void c_worst(uint32_t blk) {
    uint32_t v;
    levi_init_set(&S);
    for (v = 0u; v < RI_LEVI_NVOICES; v++) {
        levi_set_algo(&S, v, RI_LEVI_ALGO_STACK8);
        levi_set_morph(&S, v, RI_LEVI_ALGO_STACK44, 50u);
        levi_set_param_ui(&S, v, RI_CTL_LEVI_DTYPE & 0xFFu, (uint8_t)(v % RI_LEVI_NDF));
        levi_trigger(&S, v, (uint8_t)(48u + v));
    }
    ri_levi_matrix_route(&S.mx, 0u, RI_LEVI_MS_OPENV0, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 40);
    ri_levi_matrix_route(&S.mx, 1u, RI_LEVI_MS_LFO0, RI_LEVI_DM_DFILT, 0u, -60);
    ri_levi_matrix_route(&S.mx, 2u, RI_LEVI_MS_ENV0, RI_LEVI_DM_VCA, 0u, 70);
    ri_levi_matrix_route(&S.mx, 3u, RI_LEVI_MS_NOTE, RI_LEVI_DM_AFILT, 0u, 50);
    ri_levi_matrix_route(&S.mx, 4u, RI_LEVI_MS_VELON, RI_LEVI_DM_ENV1, 0u, 30);
    ri_levi_matrix_route(&S.mx, 5u, RI_LEVI_MS_WHEEL, RI_LEVI_DM_VOICE, RI_LEVI_DVO_PAN, 80);
    ri_levi_matrix_route(&S.mx, 6u, RI_LEVI_MS_LFO1, RI_LEVI_DM_LFO1, 1u, 50);
    ri_levi_matrix_route(&S.mx, 7u, RI_LEVI_MS_BEND, RI_LEVI_DM_OSC1, RI_LEVI_DO_PITCH, 100);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_DLYTYPE & 0xFFu, 1u);
    levi_set_param_ui(&S, 0u, RI_CTL_LEVI_RTYPE & 0xFFu, 1u);
    seg(SEC * 3u / 2u, blk);
    for (v = 0u; v < RI_LEVI_NVOICES; v++)
        levi_release(&S, v);
    seg(SEC / 2u, blk);
}

/* c18: sustained chord + release into a 2 s rest (state must settle exact). */
static void c_tails(uint32_t blk) {
    levi_init_set(&S);
    levi_set_param(&S, 0u, RI_LEVI_RELEASE, 2.0f);
    levi_set_param(&S, 1u, RI_LEVI_RELEASE, 2.0f);
    levi_trigger(&S, 0u, 48u);
    levi_trigger(&S, 1u, 55u);
    seg(SEC / 2u, blk);
    levi_release(&S, 0u);
    levi_release(&S, 1u);
    seg(SEC * 3u / 2u, blk);
}

typedef void (*case_fn)(uint32_t blk);
static const case_fn CASES[NC] = { c_algos, c_waves_classic, c_waves_family,
    c_opmodes, c_dfilt, c_afilt, c_morph_held, c_morph_move, c_matrix_families,
    c_matrix_heavy, c_lfo, c_menv, c_pans, c_vintage, c_pitch, c_perf, c_fx,
    c_worst, c_tails };
static const char *CNAMES[NC] = { "algos", "waves-classic", "waves-family",
    "opmodes", "dfilt", "afilt", "morph-held", "morph-move", "matrix-families",
    "matrix-heavy", "lfo", "menv", "pans", "vintage", "pitch", "perf", "fx",
    "worst", "tails" };

static uint64_t run_case(uint32_t c, uint32_t blk) {
    HH = 1469598103934665603ULL;
    CASES[c](blk);
    return HH;
}

/* Pins (HEAD ebc94ec; evidence docs/evidence/levi-perf/2026-10-06-p0-oracle.md).
 * [case][0] = 64-sample blocks, [1] = 256-sample blocks. */
static const uint64_t PIN[NC][2] = {
    { 0xbfefc6b334453d07ULL, 0xbfefc6b334453d07ULL }, /* algos */
    { 0xbae5afb75a8777edULL, 0xbae5afb75a8777edULL }, /* waves-classic */
    { 0x9ecdcc7d03fdb6d1ULL, 0x9ecdcc7d03fdb6d1ULL }, /* waves-family */
    { 0x0a38d59f078aefb7ULL, 0x0a38d59f078aefb7ULL }, /* opmodes */
    { 0x16d74a0fd96af697ULL, 0x16d74a0fd96af697ULL }, /* dfilt */
    { 0x5b9d81b00b11f339ULL, 0x5b9d81b00b11f339ULL }, /* afilt */
    { 0x9749746e2471cb37ULL, 0x9749746e2471cb37ULL }, /* morph-held */
    { 0x7daa9642007770b7ULL, 0x7daa9642007770b7ULL }, /* morph-move */
    { 0x4f1fb54e613409f7ULL, 0x4f1fb54e613409f7ULL }, /* matrix-families */
    { 0x77414e2c69c339b3ULL, 0x77414e2c69c339b3ULL }, /* matrix-heavy */
    { 0x4e263c195232d5e3ULL, 0x4e263c195232d5e3ULL }, /* lfo */
    { 0xf5d1cc15607002dfULL, 0xf5d1cc15607002dfULL }, /* menv */
    { 0x905c14d696354f40ULL, 0x905c14d696354f40ULL }, /* pans */
    { 0x03ee4487947b6ab7ULL, 0x03ee4487947b6ab7ULL }, /* vintage */
    { 0x87c18d650c9d8d11ULL, 0x87c18d650c9d8d11ULL }, /* pitch */
    { 0x0077fcf3691f9513ULL, 0x0077fcf3691f9513ULL }, /* perf */
    { 0xf9c5eb3ca4175411ULL, 0xf9c5eb3ca4175411ULL }, /* fx */
    { 0x034b904ad757af71ULL, 0x034b904ad757af71ULL }, /* worst */
    { 0xa55b87500bb3715dULL, 0xa55b87500bb3715dULL }, /* tails */
};

int main(void) {
    uint32_t c;
    for (c = 0u; c < NC; c++) {
        uint64_t h64 = run_case(c, 64u);
        uint64_t h256 = run_case(c, 256u);
#ifdef RI_T172_GEN
        printf("    { 0x%016llxULL, 0x%016llxULL }, /* %s */\n",
            (unsigned long long)h64, (unsigned long long)h256, CNAMES[c]);
#else
        RI_ASSERT(h64 == PIN[c][0], "case %s blk64: got %016llx want %016llx",
            CNAMES[c], (unsigned long long)h64, (unsigned long long)PIN[c][0]);
        RI_ASSERT(h256 == PIN[c][1], "case %s blk256: got %016llx want %016llx",
            CNAMES[c], (unsigned long long)h256, (unsigned long long)PIN[c][1]);
#endif
    }
#ifndef RI_T172_GEN
    RI_RESULT("t172_levi_bitexact");
#else
    return 0;
#endif
}
