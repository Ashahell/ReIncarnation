/* t143_levi_seq2 — Levi P8c device sequencer tracks + step record.
 * Laws: seq off bit-identical; realtime record captures the held split;
 * step record writes the cursor; playback cycles steps in mono order;
 * trig multiplies; prob gates; drift shifts; entropy wobbles; transpose
 * shifts; mode series alternates; length wraps; clear wipes; stopped
 * transport silent; macro record/playback; DM_SEQ routes; keys/pages/
 * texts; null-safe; finite extremes.
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
#define PPQ 96u

static struct RILeviSet A, B;

static void seq_run(struct RILeviSet *s, uint64_t *tick, uint32_t nblocks) {
    static float l[BS], r[BS];
    uint32_t i;
    for (i = 0u; i < nblocks; i++) {
        levi_seq_block(s, SR, BS, 1u, *tick, PPQ);
        levi_voice_render_sum_stereo(s, l, r, BS, SR);
        *tick += 128u;   /* 128-tick blocks: 12 blocks per 16th @96ppq */
    }
}

static void seq_on(struct RILeviSet *s) {
    levi_set_param_ui(s, 0u, RI_CTL_LEVI_SEQON & 0xFFu, 1u);
}

int main(void) {
    uint32_t i;
    uint64_t tick;

    /* Seq off (default): bit-identical with transport pushed. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    tick = 0u;
    seq_run(&A, &tick, 40u);
    {
        uint8_t na[RI_LEVI_NVOICES];
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            na[i] = A.v[i].note;
        levi_init_set(&B);
        levi_note_on(&B, 60u);
        tick = 0u;
        seq_run(&B, &tick, 40u);
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            RI_ASSERT(B.v[i].note == na[i], "seq off deterministic %u", i);
    }

    /* Realtime record captures the held split (C-E-G -> t1 C,G t2 E). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    levi_note_on(&A, 64u);
    levi_note_on(&A, 67u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    RI_ASSERT(A.seq_t1[0].note[0] == 60u && A.seq_t1[0].note[1] == 67u, "t1 split %u %u",
        A.seq_t1[0].note[0], A.seq_t1[0].note[1]);
    RI_ASSERT(A.seq_t2[0].note[0] == 64u, "t2 split %u", A.seq_t2[0].note[0]);
    RI_ASSERT(A.seq_t1[0].vel == 100u && A.seq_t1[0].gate == 100u, "record defaults");
    RI_ASSERT(A.seq_t1[0].trig == 1u && A.seq_t1[0].prob == 127u, "step defaults");

    /* Step record writes the cursor while stopped. */
    levi_init_set(&A);
    levi_note_on(&A, 62u);
    levi_note_on(&A, 65u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQSTEP & 0xFFu, 5u);
    {
        static float l[BS], r[BS];
        for (i = 0u; i < 10u; i++) {
            levi_seq_block(&A, SR, BS, 0u, 0u, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
        }
    }
    RI_ASSERT(A.seq_t1[5].note[0] == 62u && A.seq_t2[5].note[0] == 65u, "step record %u %u",
        A.seq_t1[5].note[0], A.seq_t2[5].note[0]);

    /* Playback strikes recorded steps (single note: exact trajectory). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    /* Transpose +12: strikes land on 72, stale user note stays 60. */
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRANSP & 0xFFu, 96u);
    {
        uint8_t tr[300], k;
        static float l[BS], r[BS];
        tick = 0u;
        for (k = 0u; k < 100u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tr[k] = A.v[0].note;
            tick += 128u;
        }
        /* Step 0 strikes C+12 — first strike lands in block 0. */
        RI_ASSERT(tr[0] == 72u, "playback first %u", tr[0]);
        /* Every block strikes recorded C+12 (single-note steps). */
        {
            uint32_t bad = 0u;
            for (k = 0u; k < 100u; k++)
                if (tr[k] != 72u)
                    bad = 1u;
            RI_ASSERT(!bad, "playback single tone");
        }
    }

    /* Trig multiplies strikes (counter over identical windows). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    seq_run(&A, &tick, 120u);
    {
        uint32_t n1 = A.seq_nstr;
        levi_init_set(&B);
        levi_note_on(&B, 60u);
        seq_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQSTRIG & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
        levi_set_tempo(&B, 140.0f);
        tick = 0u;
        seq_run(&B, &tick, 30u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
        levi_note_off(&B, 60u);
        tick = 0u;
        seq_run(&B, &tick, 120u);
        RI_ASSERT(B.seq_nstr >= n1 * 3u + 3u, "trig multiplies %u vs %u", B.seq_nstr, n1);
    }

    /* Probability 0 silences the step. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQSPROB & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    seq_run(&A, &tick, 120u);
    RI_ASSERT(A.seq_nstr == 0u, "prob mutes %u", A.seq_nstr);

    /* Drift shifts strike timing (strike counter lags: +126 ticks of
     * drift parks step 0 ~105 blocks out). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    levi_note_on(&A, 64u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    levi_note_off(&A, 64u);
    tick = 0u;
    {
        static float l[BS], r[BS];
        uint32_t k;
        for (k = 0u; k < 7u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tick += 16u;
        }
        RI_ASSERT(A.seq_nstr >= 4u, "straight strikes %u", A.seq_nstr);
        levi_init_set(&B);
        levi_note_on(&B, 60u);
        levi_note_on(&B, 64u);
        seq_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQSDRIFT & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQDRIFT & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
        levi_set_tempo(&B, 140.0f);
        tick = 0u;
        seq_run(&B, &tick, 30u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
        levi_note_off(&B, 60u);
        levi_note_off(&B, 64u);
        tick = 0u;
        {
            static float l[BS], r[BS];
            uint32_t n0 = B.seq_nstr, b;
            for (b = 0u; b < 7u; b++) {
                levi_seq_block(&B, SR, BS, 1u, tick, PPQ);
                levi_voice_render_sum_stereo(&B, l, r, BS, SR);
                tick += 16u;
            }
            RI_ASSERT(B.seq_nstr == n0, "drifted silent %u", B.seq_nstr - n0);
        }
    }

    /* Entropy wobbles pitch. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQSENTR & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    {
        uint8_t tr[120], k, wob = 0u;
        static float l[BS], r[BS];
        for (k = 0u; k < 120u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tr[k] = A.v[0].note;
            tick += 128u;
        }
        for (k = 0u; k < 120u; k++)
            if (tr[k] != 60u)
                wob = 1u;
        RI_ASSERT(wob, "entropy wobbles");
    }

    /* Transpose shifts everything. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRANSP & 0xFFu, 96u);
    levi_note_off(&A, 60u);
    tick = 0u;
    {
        uint8_t tr[30], k;
        static float l[BS], r[BS];
        for (k = 0u; k < 30u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tr[k] = A.v[0].note;
            tick += 128u;
        }
        RI_ASSERT(tr[0] == 72u, "transpose +12 %u", tr[0]);
    }

    /* Swing shifts odd steps (strike counter at checkpoint: straight
     * has fired step 1, swung still waits). Step = 96 ticks. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    {
        static float l[BS], r[BS];
        uint32_t b, n0 = A.seq_nstr;
        for (b = 0u; b < 7u; b++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tick += 16u;
        }
        RI_ASSERT(A.seq_nstr - n0 == 2u, "straight steps %u", A.seq_nstr - n0);
        levi_init_set(&B);
        levi_set_alloc_ui(&B, RI_LEVI_POLY_MONO);
        levi_note_on(&B, 60u);
        seq_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQSWING & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
        levi_set_tempo(&B, 140.0f);
        tick = 0u;
        seq_run(&B, &tick, 30u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
        levi_note_off(&B, 60u);
        tick = 0u;
        {
            uint32_t n0 = B.seq_nstr;
            for (b = 0u; b < 7u; b++) {
                levi_seq_block(&B, SR, BS, 1u, tick, PPQ);
                levi_voice_render_sum_stereo(&B, l, r, BS, SR);
                tick += 16u;
            }
            RI_ASSERT(B.seq_nstr - n0 == 1u, "swung waits %u", B.seq_nstr - n0);
        }
    }

    /* Mode clamps to SERIES (2): out-of-range plays. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQMODE & 0xFFu, 200u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    seq_run(&A, &tick, 120u);
    RI_ASSERT(A.seq_nstr > 0u, "mode clamps %u", A.seq_nstr);

    /* Stopped transport strikes nothing. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    {
        static float l[BS], r[BS];
        uint32_t n0 = A.seq_nstr, b;
        tick = 0u;
        for (b = 0u; b < 60u; b++) {
            levi_seq_block(&A, SR, BS, 0u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tick += 128u;
        }
        RI_ASSERT(A.seq_nstr == n0, "stopped silent");
    }

    /* Division knob sets step density (~4x strikes at 127 vs 0). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    seq_run(&A, &tick, 120u);
    {
        uint32_t nslow = A.seq_nstr;
        levi_init_set(&B);
        levi_note_on(&B, 60u);
        seq_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
        levi_set_tempo(&B, 140.0f);
        tick = 0u;
        seq_run(&B, &tick, 30u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
        levi_note_off(&B, 60u);
        tick = 0u;
        seq_run(&B, &tick, 120u);
        RI_ASSERT(B.seq_nstr > nslow * 3u, "division density %u vs %u", B.seq_nstr, nslow);
    }

    /* Restart drains stale futures (planted far-future strike never
     * fires after a tick reset; cleared tracks stay silent). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQCLEAR & 0xFFu, 127u);
    A.seq_pend_tick[0] = 5000u;
    A.seq_pend_note[0] = 60u;
    A.seq_pend_off[0] = 100;
    A.seq_npend = 1u;
    tick = 0u;
    {
        static float l[BS], r[BS];
        uint32_t n0 = A.seq_nstr, b;
        for (b = 0u; b < 100u; b++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tick += 128u;
        }
        RI_ASSERT(A.seq_nstr == n0, "restart drains %u", A.seq_nstr - n0);
    }
    /* Clear drains too (planted strike never fires past a clear). */
    A.seq_pend_tick[0] = tick + 5000u;
    A.seq_pend_note[0] = 60u;
    A.seq_pend_off[0] = 100;
    A.seq_npend = 1u;
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQCLEAR & 0xFFu, 127u);
    {
        static float l[BS], r[BS];
        uint32_t n0 = A.seq_nstr, b;
        for (b = 0u; b < 100u; b++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tick += 128u;
        }
        RI_ASSERT(A.seq_nstr == n0, "clear drains %u", A.seq_nstr - n0);
    }

    /* Series alternates tracks per loop (3 blocks/loop). */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_note_on(&A, 60u);
    levi_note_on(&A, 64u);
    levi_note_on(&A, 67u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQMODE & 0xFFu, 2u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRKLEN & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    levi_note_off(&A, 64u);
    levi_note_off(&A, 67u);
    tick = 0u;
    {
        uint8_t tr[24], k;
        static float l[BS], r[BS];
        for (k = 0u; k < 24u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tr[k] = A.v[0].note;
            tick += 32u;
        }
        /* Loops end at blocks 2, 5, 8, ...: 67, 64 alternating. */
        RI_ASSERT(tr[2] == 67u && tr[5] == 64u && tr[8] == 67u && tr[11] == 64u,
            "series alternates %u %u %u %u", tr[2], tr[5], tr[8], tr[11]);
    }

    /* Length wraps the loop (record C then D alone; trklen 2 plays
     * both, trklen 1 only step 0). Step = 96 ticks at rate 0. */
    levi_init_set(&A);
    levi_set_alloc_ui(&A, RI_LEVI_POLY_MONO);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRKLEN & 0xFFu, 1u);
    levi_set_tempo(&A, 140.0f);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_note_on(&A, 60u);
    {
        static float l[BS], r[BS];
        uint32_t b;
        tick = 0u;
        for (b = 0u; b < 7u; b++) {
            if (tick >= 192u) {
                levi_note_off(&A, 60u);
                levi_note_off(&A, 62u);
                levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
            } else if (tick >= 96u) {
                levi_note_off(&A, 60u);
                levi_note_on(&A, 62u);
            }
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            tick += 32u;
        }
    }
    RI_ASSERT(A.seq_t1[0].note[0] == 60u, "step0 C %u", A.seq_t1[0].note[0]);
    RI_ASSERT(A.seq_t1[1].note[0] == 62u, "step1 D %u", A.seq_t1[1].note[0]);
    {
        /* trklen 2 hits D; trklen 1 never leaves C. */
        uint8_t k, hitd = 0u, stray = 0u;
        static float l[BS], r[BS];
        tick = 0u;
        for (k = 0u; k < 200u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            if (A.v[0].note == 62u)
                hitd = 1u;
            tick += 32u;
        }
        RI_ASSERT(hitd, "trklen 2 reaches D");
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRKLEN & 0xFFu, 0u);
        tick = 0u;
        for (k = 0u; k < 200u; k++) {
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            if (A.v[0].note == 62u)
                stray = 1u;
            tick += 32u;
        }
        RI_ASSERT(!stray, "trklen 1 stays");
    }

    /* Clear wipes to rests. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQCLEAR & 0xFFu, 127u);
    RI_ASSERT(A.seq_t1[0].note[0] == 255u && A.seq_t2[0].note[0] == 255u, "clear wipes");
    {
        uint32_t n0 = A.seq_nstr;
        tick = 0u;
        seq_run(&A, &tick, 120u);
        RI_ASSERT(A.seq_nstr == n0, "clear silent %u", A.seq_nstr - n0);
    }

    /* Macro record + playback follows knobs. */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MKNOB0 & 0xFFu, 100u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    RI_ASSERT(A.seq_macro[0][0] == 100u, "macro captured %u", A.seq_macro[0][0]);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MKNOB0 & 0xFFu, 0u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    RI_ASSERT(A.mx.mknob[0] == 100u, "macro played %u", A.mx.mknob[0]);

    /* Gate knob shapes note length (instant release + active-blocks). */
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQGATE & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    {
        uint32_t actA = 0u, actB = 0u, b, v, o;
        static float l[BS], r[BS];
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                levi_set_op_ui(&A, v, o, RI_LEVI_OP_RELEASE, 0u);
        for (b = 0u; b < 120u; b++) {
            uint32_t k;
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < RI_LEVI_NVOICES; k++)
                actA += A.v[k].active ? 1u : 0u;
            tick += 128u;
        }
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQGATE & 0xFFu, 127u);
        tick = 0u;
        for (b = 0u; b < 120u; b++) {
            uint32_t k;
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < RI_LEVI_NVOICES; k++)
                actB += A.v[k].active ? 1u : 0u;
            tick += 128u;
        }
        RI_ASSERT(actB > actA, "gate knob moves %u vs %u", actB, actA);
    }

    /* DM_SEQ module: 8 params; route fills offsets; pinned offsets
     * move the gate timing (instant release + active-blocks). */
    RI_ASSERT(ri_levi_dm_nparam(RI_LEVI_DM_SEQ) == 8u, "seq nparam");
    levi_init_set(&B);
    levi_note_on(&B, 60u);
    seq_on(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_SEQ);
    levi_set_mx_ui(&B, 0u, 2u, RI_LEVI_DS_GATE);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_set_tempo(&B, 140.0f);
    tick = 0u;
    seq_run(&B, &tick, 30u);
    {
        uint32_t any = 0u;
        for (i = 0u; i < RI_LEVI_NVOICES; i++)
            any |= B.v[i].sxm_on;
        RI_ASSERT(any, "dm seq route fills");
    }
    levi_init_set(&A);
    levi_note_on(&A, 60u);
    seq_on(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQGATE & 0xFFu, 0u);
    levi_set_tempo(&A, 140.0f);
    tick = 0u;
    seq_run(&A, &tick, 30u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
    levi_note_off(&A, 60u);
    tick = 0u;
    {
        /* Pinned offsets move the gate timing: instant release makes
         * active-blocks track gate length directly (5% vs 78%). */
        uint32_t actA = 0u, actB = 0u;
        static float l[BS], r[BS];
        uint32_t b, v, o;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                levi_set_op_ui(&A, v, o, RI_LEVI_OP_RELEASE, 0u);
        for (b = 0u; b < 120u; b++) {
            uint32_t k;
            levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&A, l, r, BS, SR);
            for (k = 0u; k < RI_LEVI_NVOICES; k++)
                actA += A.v[k].active ? 1u : 0u;
            tick += 128u;
        }
        levi_init_set(&B);
        levi_note_on(&B, 60u);
        seq_on(&B);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQGATE & 0xFFu, 0u);
        levi_set_tempo(&B, 140.0f);
        tick = 0u;
        seq_run(&B, &tick, 30u);
        levi_set_param_ui(&B, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 0u);
        levi_note_off(&B, 60u);
        tick = 0u;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            for (o = 0u; o < RI_LEVI_NOPS; o++)
                levi_set_op_ui(&B, v, o, RI_LEVI_OP_RELEASE, 0u);
        for (b = 0u; b < 120u; b++) {
            uint32_t k;
            for (v = 0u; v < RI_LEVI_NVOICES; v++) {
                B.v[v].sxm[RI_LEVI_DS_GATE] = 1.0f;
                B.v[v].sxm_on = 1u;
            }
            levi_seq_block(&B, SR, BS, 1u, tick, PPQ);
            levi_voice_render_sum_stereo(&B, l, r, BS, SR);
            for (k = 0u; k < RI_LEVI_NVOICES; k++)
                actB += B.v[k].active ? 1u : 0u;
            tick += 128u;
        }
        RI_ASSERT(actB > actA, "dm seq gate moves %u vs %u", actB, actA);
    }

    /* Keys / allow-list / pages / texts. */
    {
        uint32_t k;
        for (k = 0x0EADu; k <= 0x0EBBu; k++)
            RI_ASSERT(ri_auto_allowed((uint16_t)k), "seq key %04x allowed", k);
    }
    RI_ASSERT(!ri_auto_allowed(0x0EBCu), "0x0EBC refused");
    {
        const struct RICtlDef *d = ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | 196u));
        RI_ASSERT(d && d->engine_id == RI_CTL_LEVI_SEQRATE, "seqrate row binds");
    }
    {
        struct RISectLevi lv;
        char tx[16];
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_SEQ) == 1, "seq module");
        RI_ASSERT(ri_slevi_page_count(&lv) == 2u, "seq 2 pages");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "LENGTH"), "slot0 len");
        RI_ASSERT(ri_slevi_enc_live(&lv, 1u) && !strcmp(ri_slevi_enc_name(&lv, 1u), "RATE"), "slot1 rate");
        RI_ASSERT(ri_slevi_press(&lv, RI_SLEVI_PAGEDN) == 1, "page 2");
        RI_ASSERT(ri_slevi_enc_live(&lv, 0u) && !strcmp(ri_slevi_enc_name(&lv, 0u), "TRK LEN"), "p2 trklen");
        lv.page = 0u;
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "PARA"), "mode text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 2u, 127) == 1, "mode max");
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "SERIES"), "mode max text %s", tx);
    }

    /* Null-safe entry points. */
    levi_seq_block(0, SR, BS, 1u, 0u, PPQ);

    /* Finite/bounded extremes storm (all modes, max everything). */
    {
        uint32_t m, bad = 0;
        static float l[BS], r[BS];
        for (m = 0u; m < 3u; m++) {
            int i;
            levi_init_set(&A);
            levi_note_on(&A, 60u);
            levi_note_on(&A, 40u);
            seq_on(&A);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQREC & 0xFFu, 1u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQRATE & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQMODE & 0xFFu, (uint8_t)m);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQSWING & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQGATE & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQPROB & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQDRIFT & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRANSP & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_SEQTRKLEN & 0xFFu, 127u);
            levi_set_tempo(&A, 140.0f);
            tick = 0u;
            seq_run(&A, &tick, 200u);
            levi_note_off(&A, 60u);
            levi_note_off(&A, 40u);
            for (i = 0; i < 200; i++) {
                uint32_t k;
                levi_seq_block(&A, SR, BS, 1u, tick, PPQ);
                levi_voice_render_sum_stereo(&A, l, r, BS, SR);
                tick += 128u;
                for (k = 0u; k < BS; k++) {
                    float a = l[k];
                    if (!((a > -8.0f && a < 8.0f) && (r[k] > -8.0f && r[k] < 8.0f)))
                        bad = 1;
                }
            }
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t143_levi_seq2");
}
