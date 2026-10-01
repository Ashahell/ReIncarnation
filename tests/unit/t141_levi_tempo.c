/* t141_levi_tempo — Levi P8a device tempo + BPM sync (closes P7d).
 * Laws: defaults (140, flags off) bit-identical; pure helpers exact;
 * set_tempo clamps/defaults/null; tempo moves BPM-on output, never
 * BPM-off output (LFO rate, LFO delay/fade, ENV segments, delay snap);
 * flags default off; keys/pages/texts; null-safe; finite extremes.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/levi_fx.h"
#include "engine/dsp/levi_matrix.h"
#include "engine/seq/autolane.h"
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"

#define SR 48000.0f
#define N 4800u
#define NL 48000u

static struct RILeviSet A, B;
static float oa[N], ob[N];
static float la[N], lb[N];
static float xa[NL], xb[NL];
static float ya[NL], yb[NL];

static void stereo(struct RILeviSet *s, float *l, float *r, uint32_t n) {
    levi_voice_render_sum_stereo(s, l, r, n, SR);
}

static int differ(const float *a, const float *b, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        if (a[i] != b[i])
            return 1;
    return 0;
}

static int feq(float a, float b) {
    float d, m;
    if (a == b)
        return 1;
    d = a > b ? a - b : b - a;
    m = (a > b ? a : b);
    m = m < 0.0f ? -m : m;
    return d <= m * 1e-5f + 1e-7f && d < 1e30f;
}

int main(void) {
    /* Pure helpers exact. */
    RI_ASSERT(feq(ri_levi_beats_time(0u, 120.0f), 0.0f), "beats 0");
    RI_ASSERT(feq(ri_levi_beats_time(127u, 120.0f), 2.0f), "beats 127 @120");
    RI_ASSERT(feq(ri_levi_beats_time(127u, 60.0f), 4.0f), "beats 127 @60");
    RI_ASSERT(feq(ri_levi_beats_time(64u, 120.0f), 64.0f / 127.0f * 2.0f), "beats mid");
    RI_ASSERT(feq(ri_levi_beats_time(127u, 0.0f), 4.0f * 60.0f / 140.0f), "bad bpm falls back to 140");
    RI_ASSERT(feq(ri_levi_lfo_sync_hz(0u, 120.0f), 0.5f), "lfo slowest @120");
    RI_ASSERT(feq(ri_levi_lfo_sync_hz(127u, 120.0f), 64.0f), "lfo fastest @120");
    RI_ASSERT(feq(ri_levi_lfo_sync_hz(127u, 60.0f), 32.0f), "lfo fastest @60");
    RI_ASSERT(feq(ri_levi_delay_snap(0.13f, 120.0f), 0.125f), "snap 16th @120");
    RI_ASSERT(feq(ri_levi_delay_snap(0.001f, 120.0f), 0.001f), "snap floor");
    RI_ASSERT(feq(ri_levi_delay_snap(3.0f, 120.0f), 2.0f), "snap ceiling");

    /* set_tempo: clamp, default, null. */
    levi_init_set(&A);
    RI_ASSERT(A.tempo_bpm == 140.0f, "tempo default %f", A.tempo_bpm);
    RI_ASSERT(levi_set_tempo(&A, 120.0f) == 0 && A.tempo_bpm == 120.0f, "tempo set");
    RI_ASSERT(levi_set_tempo(&A, 10.0f) == 0 && A.tempo_bpm == 20.0f, "tempo lo clamp");
    RI_ASSERT(levi_set_tempo(&A, 900.0f) == 0 && A.tempo_bpm == 500.0f, "tempo hi clamp");
    RI_ASSERT(levi_set_tempo(0, 120.0f) == 2, "tempo null refused");

    /* Flags default off; defaults bit-identical to a clean twin. */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_set_tempo(&B, 96.0f);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N) && !differ(ob, lb, N), "flags off: tempo inert");

    /* LFO BPM: tempo moves synced output, never unsynced output.
     * LFO0 routes to D.FILTER cutoff so the rate is audible. */
    levi_init_set(&A);
    levi_set_mx_ui(&A, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&A, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&A, 0u, 2u, 0u);
    levi_set_mx_ui(&A, 0u, 3u, 100u);
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_RATE, 80u);
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_LEVEL, 127u);
    levi_set_tempo(&A, 120.0f);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&B, 0u, 2u, 0u);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_RATE, 80u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_LEVEL, 127u);
    levi_set_tempo(&B, 60.0f);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(!differ(oa, la, N), "lfo free: tempo inert");
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_BPM, 1u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_BPM, 1u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "lfo synced: tempo moves");

    /* LFO delay/fade sync the same way (delay 0 vs long at 120bpm). */
    levi_init_set(&A);
    levi_set_mx_ui(&A, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&A, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&A, 0u, 2u, 0u);
    levi_set_mx_ui(&A, 0u, 3u, 100u);
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_RATE, 80u);
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_LEVEL, 127u);
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_DELAY, 0u);
    levi_set_lfo_ui(&A, 0u, 0u, RI_LEVI_LP_BPM, 1u);
    levi_set_tempo(&A, 120.0f);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, oa, ob, N);
    levi_init_set(&B);
    levi_set_mx_ui(&B, 0u, 0u, (uint8_t)ri_levi_ms_to_ui(RI_LEVI_MS_LFO0));
    levi_set_mx_ui(&B, 0u, 1u, RI_LEVI_DM_DFILT);
    levi_set_mx_ui(&B, 0u, 2u, 0u);
    levi_set_mx_ui(&B, 0u, 3u, 100u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_RATE, 80u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_LEVEL, 127u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_DELAY, 127u);
    levi_set_lfo_ui(&B, 0u, 0u, RI_LEVI_LP_BPM, 1u);
    levi_set_tempo(&B, 120.0f);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, la, lb, N);
    RI_ASSERT(differ(oa, la, N), "lfo delay synced moves");

    /* ENV BPM: op flag on + tempo change moves; flag off never. */
    levi_init_set(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 127u);
    levi_set_tempo(&A, 120.0f);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, xa, xb, NL);
    levi_init_set(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_ATTACK, 127u);
    levi_set_tempo(&B, 60.0f);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ya, yb, NL);
    RI_ASSERT(!differ(xa, ya, NL), "env free: tempo inert");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_OPBPM0 & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_OPBPM0 & 0xFFu, 1u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, xa, xb, NL);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ya, yb, NL);
    RI_ASSERT(differ(xa, ya, NL), "env synced: tempo moves");

    /* Menv BPM: ENV1 prewired to D.FILTER cutoff; flag on + tempo
     * change moves, flag off never. */
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DENV1 & 0xFFu, 127u);
    levi_set_menv_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 127u);
    levi_set_tempo(&A, 120.0f);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, xa, xb, NL);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DENV1 & 0xFFu, 127u);
    levi_set_menv_ui(&B, 0u, 0u, RI_LEVI_OP_ATTACK, 127u);
    levi_set_tempo(&B, 60.0f);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ya, yb, NL);
    RI_ASSERT(!differ(xa, ya, NL), "menv free: tempo inert");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_MEBPM0 & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_MEBPM0 & 0xFFu, 1u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, xa, xb, NL);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ya, yb, NL);
    RI_ASSERT(differ(xa, ya, NL), "menv synced: tempo moves");

    /* Flags apply section-wide (set via any voice, all voices follow). */
    levi_init_set(&A);
    levi_set_param_ui(&A, 3u, RI_CTL_LEVI_OPBPM0 & 0xFFu, 1u);
    levi_set_param_ui(&A, 3u, RI_CTL_LEVI_OPBPM7 & 0xFFu, 1u);
    levi_set_param_ui(&A, 3u, RI_CTL_LEVI_MEBPM4 & 0xFFu, 1u);
    RI_ASSERT(A.v[0].opbpm[0] && A.v[5].opbpm[0], "opbpm section-wide");
    RI_ASSERT(A.v[0].opbpm[7] && A.v[5].opbpm[7], "opbpm7 range end");
    RI_ASSERT(A.v[0].mebpm[4] && A.v[5].mebpm[4], "mebpm4 range end");
    levi_init_set(&A);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
    levi_set_tempo(&A, 120.0f);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, xa, xb, NL);
    levi_init_set(&B);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 84u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
    levi_set_tempo(&B, 96.0f);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ya, yb, NL);
    RI_ASSERT(!differ(xa, ya, NL), "delay free: tempo inert");
    levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYBPM & 0xFFu, 1u);
    levi_set_param_ui(&B, 0u, RI_CTL_LEVI_DLYBPM & 0xFFu, 1u);
    levi_trigger(&A, 0u, 60u);
    stereo(&A, xa, xb, NL);
    levi_trigger(&B, 0u, 60u);
    stereo(&B, ya, yb, NL);
    RI_ASSERT(differ(xa, ya, NL), "delay snapped: tempo moves");

    /* Keys / allow-list / pages / texts. */
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_OPBPM0), "opbpm0 allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_OPBPM7), "opbpm7 allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_MEBPM0), "mebpm0 allowed");
    RI_ASSERT(ri_auto_allowed(RI_CTL_LEVI_MEBPM4), "mebpm4 allowed");
    RI_ASSERT(!ri_auto_allowed(0x0EBFu), "0x0EBF refused (P8d)");
    {
        struct RISectLevi lv;
        char tx[16];
        uint16_t key = 0u;
        int kv = -1;
        memset(&lv, 0, sizeof lv);
        ri_slevi_init(&lv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_VOICE) == 1, "voice module first");
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_OSC) == 1, "osc module");
        lv.page = 1u;
        RI_ASSERT(ri_slevi_enc_live(&lv, 7u) && !strcmp(ri_slevi_enc_name(&lv, 7u), "BPM SYNC"), "op bpm slot");
        ri_slevi_enc_text(&lv, 7u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "FREE"), "op bpm text %s", tx);
        /* Encoder round-trip: slot 7 sets the page op's flag + key. */
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 7u, 127) == 1, "op bpm set");
        RI_ASSERT(ri_slevi_ctl_key(&lv, RI_SLEVI_ENC0 + 7u, &key, &kv) == 1 &&
            key == RI_CTL_LEVI_OPBPM0 && kv == 1, "op bpm key %04x=%d", key, kv);
        ri_slevi_enc_text(&lv, 7u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "SYNC"), "op bpm text on %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_ENV1) == 1, "env module");
        lv.page = 0u;
        RI_ASSERT(ri_slevi_enc_live(&lv, 7u) && !strcmp(ri_slevi_enc_name(&lv, 7u), "BPM SYNC"), "me bpm slot");
        ri_slevi_enc_text(&lv, 7u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "FREE"), "me bpm text %s", tx);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_ENC0 + 7u, 127) == 1, "me bpm set");
        RI_ASSERT(ri_slevi_ctl_key(&lv, RI_SLEVI_ENC0 + 7u, &key, &kv) == 1 &&
            key == RI_CTL_LEVI_MEBPM0 && kv == 1, "me bpm key %04x=%d", key, kv);
        RI_ASSERT(ri_slevi_set_value(&lv, RI_SLEVI_MODULE, (int)RI_SLEVI_M_LFO1) == 1, "lfo module");
        lv.page = 1u;
        RI_ASSERT(ri_slevi_enc_live(&lv, 2u) && !strcmp(ri_slevi_enc_name(&lv, 2u), "BPM SYNC"), "lfo bpm slot");
        ri_slevi_enc_text(&lv, 2u, tx, sizeof tx);
        RI_ASSERT(!strcmp(tx, "FREE"), "lfo bpm text %s", tx);
    }

    /* Null-safe entry points. */
    RI_ASSERT(levi_set_tempo(0, 120.0f) == 2, "tempo null");

    /* Finite/bounded extremes storm (all sync on, tempo corners). */
    {
        uint32_t bad = 0;
        float bpms[3] = { 20.0f, 140.0f, 500.0f };
        int bi;
        for (bi = 0; bi < 3; bi++) {
            int i, o;
            levi_init_set(&A);
            levi_set_tempo(&A, bpms[bi]);
            for (o = 0; o < 8; o++)
                levi_set_param_ui(&A, 0u, (uint32_t)((RI_CTL_LEVI_OPBPM0 & 0xFFu) + o), 1u);
            for (o = 0; o < 5; o++) {
                levi_set_lfo_ui(&A, 0u, (uint32_t)o, RI_LEVI_LP_BPM, 1u);
                levi_set_lfo_ui(&A, 0u, (uint32_t)o, RI_LEVI_LP_RATE, 127u);
            }
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYBPM & 0xFFu, 1u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DBYPASS & 0xFFu, 1u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYTIME & 0xFFu, 127u);
            levi_set_param_ui(&A, 0u, RI_CTL_LEVI_DLYDRYWET & 0xFFu, 127u);
            levi_trigger(&A, 0u, 60u);
            for (i = 0; i < 4800; i++) {
                float l, r;
                levi_voice_render_sum_stereo(&A, ob, la, 1u, SR);
                l = ob[0];
                r = la[0];
                if (!((l > -8.0f && l < 8.0f) && (r > -8.0f && r < 8.0f)))
                    bad = 1;
            }
        }
        RI_ASSERT(!bad, "extremes bounded");
    }

    RI_RESULT("t141_levi_tempo");
}
