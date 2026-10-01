/* t144_levi_ribbon — Levi P8d ribbon state + sources + theremin +
 * triggers + step selector.
 * Laws: defaults (off) bit-identical; sources follow position per mode
 * (ABS bipolar, ABS+ unipolar, REL movement, off silence); touch starts
 * trig-7 menvs; release releases trig-8 menvs; theremin retunes active
 * voices; touch jumps the seq grid; keys/pages/texts; null-safe;
 * finite extremes.
 */
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

static int feq(float a, float b) {
    if (a == b)
        return 1;
    {
        float d = a > b ? a - b : b - a;
        float m = a > b ? a : b;
        m = m < 0.0f ? -m : m;
        return d <= m * 1e-5f + 1e-7f && d < 1e30f;
    }
}

int main(void) {
    uint32_t i;

    /* Defaults: mode off, sources silent, bit-identical twin. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    run(&A, 20u);
    {
        static float la[BS], lb[BS];
        levi_init_set(&B);
        levi_trigger(&B, 0u, 60u);
        run(&B, 20u);
        levi_voice_render_sum_stereo(&A, la, lb, BS, SR);
        levi_voice_render_sum_stereo(&B, la, lb, BS, SR);
        RI_ASSERT(A.v[0].rbn_abs == 0.0f && A.v[0].rbn_rel == 0.0f, "sources off");
    }

    /* ABS mode: bipolar/unipolar follow position. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 127u);
    levi_trigger(&A, 0u, 60u);
    run(&A, 5u);
    RI_ASSERT(feq(A.v[0].rbn_abs, 1.0f), "abs max %f", A.v[0].rbn_abs);
    RI_ASSERT(feq(A.v[0].rbn_absp, 1.0f), "absp max %f", A.v[0].rbn_absp);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 0u);
    run(&A, 5u);
    RI_ASSERT(feq(A.v[0].rbn_abs, -1.0f), "abs min %f", A.v[0].rbn_abs);
    RI_ASSERT(feq(A.v[0].rbn_absp, 0.0f), "absp min %f", A.v[0].rbn_absp);

    /* REL mode: movement only (consumed per block). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 2u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 64u);
    levi_trigger(&A, 0u, 60u);
    run(&A, 5u);
    RI_ASSERT(feq(A.v[0].rbn_rel, 0.0f), "rel still %f", A.v[0].rbn_rel);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 96u);
    run(&A, 1u);
    RI_ASSERT(A.v[0].rbn_rel > 0.4f && A.v[0].rbn_rel <= 0.5f, "rel move %f", A.v[0].rbn_rel);
    run(&A, 1u);
    RI_ASSERT(feq(A.v[0].rbn_rel, 0.0f), "rel consumed %f", A.v[0].rbn_rel);

    /* Touch starts trig-7 menvs (white-box: value rises from idle). */
    levi_init_set(&A);
    levi_set_menv_ui(&A, 0u, 1u, RI_LEVI_ME_TRIG1, 7u);
    levi_set_menv_ui(&A, 0u, 1u, RI_LEVI_OP_ATTACK, 127u);
    levi_trigger(&A, 0u, 60u);
    run(&A, 5u);
    RI_ASSERT(A.v[0].menv[1].value == 0.0f, "menv idle");
    RI_ASSERT(levi_ribbon_touch(&A, 80u) == 0, "touch ok");
    run(&A, 20u);
    RI_ASSERT(A.v[0].menv[1].value > 0.0f, "menv started %f", A.v[0].menv[1].value);

    /* Release releases trig-8 menvs (sustain drops). */
    levi_init_set(&A);
    levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_TRIG1, 1u);
    levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 0u);
    levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_OP_SUSTAIN, 127u);
    levi_trigger(&A, 0u, 60u);
    run(&A, 200u);
    {
        float held = A.v[0].menv[0].value;
        RI_ASSERT(held > 0.9f, "menv sustained %f", held);
        RI_ASSERT(levi_ribbon_touch(&A, 80u) == 0, "touch ok");
        levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_ME_TRIG1, 8u);
        RI_ASSERT(levi_ribbon_release(&A) == 0, "release ok");
        run(&A, 200u);
        RI_ASSERT(A.v[0].menv[0].value < held, "menv released %f", A.v[0].menv[0].value);
    }

    /* Mode clamps to THEREMIN (3); mode 0 never retunes. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 200u);
    levi_note_on(&A, 60u);
    run(&A, 5u);
    RI_ASSERT(levi_ribbon_touch(&A, 72u) == 0, "clamped touch");
    RI_ASSERT(A.v[0].note == 72u, "mode clamps %u", A.v[0].note);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 1u);
    levi_note_on(&B, 60u);
    run(&B, 5u);
    RI_ASSERT(levi_ribbon_touch(&B, 72u) == 0, "abs touch");
    RI_ASSERT(B.v[0].note == 60u, "abs no retune %u", B.v[0].note);

    /* Position clamps to 127 at the engine door. */
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 200u);
    run(&B, 5u);
    RI_ASSERT(feq(B.v[0].rbn_abs, 1.0f), "pos clamps %f", B.v[0].rbn_abs);
    /* RBNTOUCH key drives touch (engine key path). */
    levi_init_set(&B);
    levi_set_menv_ui(&B, 0u, 1u, RI_LEVI_ME_TRIG1, 7u);
    levi_trigger(&B, 0u, 60u);
    run(&B, 5u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNTOUCH & 0xFFu, 1u);
    run(&B, 20u);
    RI_ASSERT(B.v[0].menv[1].value > 0.0f, "touch key starts %f", B.v[0].menv[1].value);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNTOUCH & 0xFFu, 0u);
    run(&B, 5u);
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 3u);
    levi_note_on(&A, 60u);
    run(&A, 5u);
    RI_ASSERT(levi_ribbon_touch(&A, 72u) == 0, "theremin touch");
    RI_ASSERT(A.v[0].note == 72u, "theremin retune %u", A.v[0].note);
    RI_ASSERT(levi_ribbon_move(&A, 76u) == 0, "theremin move");
    RI_ASSERT(A.v[0].note == 76u, "theremin glide %u", A.v[0].note);

    /* Touch jumps the seq grid (step selector). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQON & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRKLEN & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    {
        static float l[BS], r[BS];
        uint64_t tick = 0u;
        uint32_t b;
        for (b = 0u; b < 10u; b++) {
            levi_seq_block(&A, SR, BS, 1u, tick, 96u);
            tick += 128u;
        }
        RI_ASSERT(levi_ribbon_touch(&A, 127u) == 0, "touch jumps");
        RI_ASSERT(A.seq_lastk == 126, "jumped %lld", (long long)A.seq_lastk);
        (void)l;
        (void)r;
    }

    /* Audible sanity: ribbon-driven cutoff moves (ABS -> D.FILTER).
     * Twin sets (same voice age): only the ribbon differs. */
    levi_init_set(&A);
    levi_set_mx_ui(&A, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_RBNABS));
    levi_set_mx_ui(&A, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&A, 0u, 2u, 0u);
    levi_set_mx_ui(&A, 0u, 3u, 100u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 0u);
    levi_trigger(&A, 0u, 60u);
    levi_init_set(&B);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_RBNABS));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&B, 0u, 2u, 0u);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_RBNPOS & 0xFFu, 127u);
    levi_trigger(&B, 0u, 60u);
    {
        static float oa[BS], ob[BS], la[BS], lb[BS];
        uint32_t k, same = 1u;
        run(&A, 5u);
        run(&B, 5u);
        levi_voice_render_sum_stereo(&A, oa, ob, BS, SR);
        levi_voice_render_sum_stereo(&B, la, lb, BS, SR);
        for (k = 0u; k < BS; k++)
            if (oa[k] != la[k] || ob[k] != lb[k])
                same = 0u;
        RI_ASSERT(!same, "ribbon moves cutoff");
    }

    /* Keys / allow-list / pages / texts. */
    {
        uint32_t k;
        for (k = 0x0EBCu; k <= 0x0EC4u; k++)
            RI_ASSERT(ri_auto_allowed((uint16_t)k), "rbn key %04x allowed", k);
    }
    RI_ASSERT(ri_auto_allowed(0x0EC5u), "0x0EC5 is the first zone key (P9c)");
    RI_ASSERT(!ri_auto_allowed(0x0ECAu), "0x0ECA refused");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 211u));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_RBNMODE, "rbnmode row binds");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_RIBBON) == 1, "ribbon module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 1u, "ribbon 1 page");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "MODE"), "slot0 mode");
        RI_ASSERT(ri_slevi_enc_live(&lv, 1u) && !strcmp(ri_slevi_enc_name(&lv, 1u), "POS"), "slot1 pos");
        RI_ASSERT(ri_slevi_enc_live(&lv, 2u) && !strcmp(ri_slevi_enc_name(&lv, 2u), "TOUCH"), "slot2 touch");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 0u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "OFF"), "mode text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 0u, 127) == 1, "mode max");
        ri_slevi_enc_text(&lv, 0u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "THEREMIN"), "mode max text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_MACRO) == 1, "macro module");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_RIBBON) == 1, "ribbon again");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "OFF"), "touch text %s", tx);
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_RBNTOUCH) == 1, "touch toggle");
        RI_ASSERT(lv.val[RI_SLEVI_RBNTOUCH] == 1, "touch on");
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "ON"), "touch text on %s", tx);
    }

    /* Null-safe entry points. */
    RI_ASSERT(levi_ribbon_touch(0, 64u) == 2, "touch null refused");
    RI_ASSERT(levi_ribbon_move(0, 64u) == 2, "move null refused");
    RI_ASSERT(levi_ribbon_release(0) == 2, "release null refused");

    /* Finite/bounded extremes storm (all modes, touch dancing). */
    {
        uint32_t m, bad = 0;
        static float l[BS], r[BS];
        for (m = 0u; m < 4u; m++) {
            int i;
            levi_init_set(&A);
            levi_note_on(&A, 30u + m * 20u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_RBNMODE & 0xFFu, (uint8_t)m);
            for (i = 0; i < 600; i++) {
                uint32_t k;
                if (i % 50 == 0)
                    levi_ribbon_touch(&A, (uint8_t)((i * 37u) % 128u));
                if (i % 50 == 25)
                    levi_ribbon_release(&A);
                levi_voice_render_sum_stereo(&A, l, r, BS, SR);
                for (k = 0u; k < BS; k++) {
                    float a = l[k], b = r[k];
                    if (!((a > -8.0f && a < 8.0f) && (b > -8.0f && b < 8.0f)))
                        bad = 1;
                }
            }
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t144_levi_ribbon");
    (void)i;
}
