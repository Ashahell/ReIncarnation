/* t129_levi_osc — Levi per-oscillator params + envelopes (fidelity plan
 * P2, owner 2026-09-30; manual pp. 35-48, 54, 71-75).
 * Laws: defaults re-sent are a no-op (UI defaults == v1 voice); own wave
 * set finite + distinct, wave 0 is the exact sine; pitch modes (semitone
 * octave doubles, frequency mode ignores the key, keytrack 0 fixes the
 * pitch); levels (initial level sounds without the envelope); modes
 * belong to the MODULATOR; feedback only in Freq/Phase Mod; Direct Out;
 * envelope attack time, curves, quantize, loops, freerun; bias knobs;
 * 0x0F keys allowed and applied by the engine.
 */
#include <stdio.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/kernels.h"
#include "engine/engine.h"
#include "engine/seq/autolane.h"
#include "engine/seq/sched.h"

#define SR 48000.0f
#define N 48000u

static struct RILeviSet A, B;
static float oa[N], ob[N];

static void render(struct RILeviSet *s, float *o, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++)
        o[i] = levi_voice_render(&s->v[0], 0, SR);
}

static uint32_t crossings(const float *o, uint32_t from, uint32_t n) {
    uint32_t i, c = 0u;
    for (i = from + 1u; i < from + n; i++)
        if ((o[i - 1u] < 0.0f) != (o[i] < 0.0f))
            c++;
    return c;
}

static float peak(const float *o, uint32_t from, uint32_t n) {
    uint32_t i;
    float p = 0.0f;
    for (i = from; i < from + n; i++) {
        float a = o[i] < 0.0f ? -o[i] : o[i];
        if (a > p)
            p = a;
    }
    return p;
}

/* A plain carrier: DUO algorithm, modulator silenced, filters open. */
static void plain(struct RILeviSet *s) {
    levi_init_set(s);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF, 18000.0f);
    levi_set_param(s, 0u, RI_LEVI_CUTOFF2, 18000.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO, 0.0f);
    levi_set_param(s, 0u, RI_LEVI_RESO2, 0.0f);
    levi_set_op_ui(s, 0u, 1u, RI_LEVI_OP_ENVL, 64u); /* modulator level 0 */
}

int main(void) {
    uint32_t w, o, p, k;
    /* ---- Defaults re-sent change nothing (UI defaults == v1 floats). ---- */
    levi_init_set(&A);
    levi_init_set(&B);
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        for (p = 0u; p < RI_LEVI_OP_NPARAM; p++)
            if (p < RI_LEVI_OP_DELAY || p == RI_LEVI_OP_SPEED || p >= RI_LEVI_OP_ACURVE)
                RI_ASSERT(levi_set_op_ui(&B, 0u, o, p, (uint8_t)ri_levi_op_default(o, p)) == 0, "default %u/%u", o, p);
    RI_ASSERT(levi_trigger(&A, 0u, 59u) == 0 && levi_trigger(&B, 0u, 59u) == 0, "trig");
    render(&A, oa, 9600u);
    render(&B, ob, 9600u);
    RI_ASSERT(memcmp(oa, ob, 9600u * sizeof(float)) == 0, "re-sent defaults are the v1 voice");
    RI_ASSERT(levi_set_op_ui(&B, 0u, 8u, 0u, 0u) == 2 && levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_NPARAM, 0u) == 2 &&
        levi_set_op_ui(0, 0u, 0u, 0u, 0u) == 2 && levi_set_op_ui(&B, 6u, 0u, 0u, 0u) == 2, "fail-closed");

    /* ---- Own wave set. ---- */
    for (w = 0u; w < RI_LEVI_NWAVES; w++) {
        int differs = w == 0u;
        for (k = 0u; k < 256u; k++) {
            float ph = (float)k / 256.0f, v = ri_levi_wave(w, ph, 0.01f);
            RI_ASSERT(v > -1.6f && v < 1.6f, "wave %u bounded at %u: %f", w, k, v);
            if (v != ri_levi_wave(0u, ph, 0.01f))
                differs = 1;
        }
        RI_ASSERT(differs, "wave %u differs from sine", w);
        RI_ASSERT(ri_levi_wave_name(w)[0] != 0, "wave %u named", w);
    }
    RI_ASSERT(ri_levi_wave(0u, 0.125f, 0.01f) == ri_sin(0.125f * 6.2831853f), "wave 0 is the exact sine");
    RI_ASSERT(ri_levi_wave_name(RI_LEVI_NWAVES)[0] == 0, "bad wave unnamed");

    /* ---- Pitch: ratio default at A4 is 440 Hz; semitone +12 doubles it. ---- */
    plain(&A);
    levi_trigger(&A, 0u, 69u);
    render(&A, oa, N);
    k = crossings(oa, 4800u, 24000u);
    RI_ASSERT(k >= 435u && k <= 445u, "440 Hz carrier: %u crossings / 0.5 s", k);
    plain(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_PMODE, 0u);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_COARSE, 76u); /* +12 semitones */
    levi_trigger(&A, 0u, 69u);
    render(&A, oa, N);
    k = crossings(oa, 4800u, 24000u);
    RI_ASSERT(k >= 870u && k <= 890u, "semitone +12 -> 880 Hz: %u", k);
    /* Frequency mode: the key does not move the pitch. */
    plain(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_PMODE, 2u);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_COARSE, 60u); /* ~1050 Hz */
    levi_trigger(&A, 0u, 48u);
    render(&A, oa, N);
    plain(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_PMODE, 2u);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_COARSE, 60u);
    levi_trigger(&B, 0u, 84u);
    render(&B, ob, N);
    RI_ASSERT(crossings(oa, 4800u, 24000u) == crossings(ob, 4800u, 24000u) && crossings(oa, 4800u, 24000u) > 900u,
        "frequency mode fixed: %u vs %u", crossings(oa, 4800u, 24000u), crossings(ob, 4800u, 24000u));
    /* Keytrack 0 %: two keys, one pitch. */
    plain(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_KEYTRK, 64u);
    levi_trigger(&A, 0u, 40u);
    render(&A, oa, N);
    plain(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_KEYTRK, 64u);
    levi_trigger(&B, 0u, 80u);
    render(&B, ob, N);
    RI_ASSERT(crossings(oa, 4800u, 24000u) == crossings(ob, 4800u, 24000u), "keytrack 0 fixes the pitch");

    /* ---- Levels: initial level sounds with the envelope at zero. ---- */
    plain(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ENVL, 64u);   /* env level 0 */
    levi_trigger(&A, 0u, 69u);
    render(&A, oa, 4800u);
    RI_ASSERT(peak(oa, 2400u, 2400u) < 1e-6f, "no init, no env: silent");
    plain(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ENVL, 64u);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_INIT, 127u);
    levi_trigger(&A, 0u, 69u);
    render(&A, oa, 4800u);
    RI_ASSERT(peak(oa, 2400u, 2400u) > 0.5f, "initial level sounds: %f", peak(oa, 2400u, 2400u));

    /* ---- Modes belong to the modulator (manual p. 35). ---- */
    levi_init_set(&A);
    levi_trigger(&A, 0u, 59u);
    render(&A, oa, 9600u);
    levi_init_set(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_MODE, RI_LEVI_PWM); /* the carrier's own mode */
    levi_trigger(&B, 0u, 59u);
    render(&B, ob, 9600u);
    RI_ASSERT(memcmp(oa, ob, 9600u * sizeof(float)) == 0, "a carrier's mode does not change its sound");
    for (w = RI_LEVI_PM; w < RI_LEVI_NMODES; w++) {
        levi_init_set(&B);
        levi_set_op_ui(&B, 0u, 1u, RI_LEVI_OP_ENVL, 127u);     /* full modulation */
        levi_set_op_ui(&B, 0u, 1u, RI_LEVI_OP_MODE, (uint8_t)w);
        levi_trigger(&B, 0u, 59u);
        render(&B, ob, 9600u);
        levi_init_set(&A);
        levi_set_op_ui(&A, 0u, 1u, RI_LEVI_OP_ENVL, 127u);
        levi_trigger(&A, 0u, 59u);
        render(&A, oa, 9600u);
        RI_ASSERT(memcmp(oa, ob, 9600u * sizeof(float)) != 0, "modulator mode %u changes the carrier", w);
        for (k = 0u; k < 9600u; k++)
            RI_ASSERT(ob[k] > -8.0f && ob[k] < 8.0f, "mode %u finite", w);
    }
    /* ---- Feedback: Freq/Phase Mod only. ---- */
    plain(&A);
    levi_trigger(&A, 0u, 59u);
    render(&A, oa, 9600u);
    plain(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_FEEDBACK, 100u);
    levi_trigger(&B, 0u, 59u);
    render(&B, ob, 9600u);
    RI_ASSERT(memcmp(oa, ob, 9600u * sizeof(float)) != 0, "feedback colours a Freq Mod oscillator");
    plain(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_MODE, RI_LEVI_PDSAW);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_FEEDBACK, 100u);
    levi_trigger(&B, 0u, 59u);
    render(&B, ob, 9600u);
    RI_ASSERT(memcmp(oa, ob, 9600u * sizeof(float)) == 0, "feedback dead in PD mode");
    /* ---- Direct Out: a modulator joins the mix. ---- */
    plain(&A);
    levi_set_op_ui(&A, 0u, 1u, RI_LEVI_OP_ENVL, 127u);
    levi_set_op_ui(&A, 0u, 1u, RI_LEVI_OP_MODE, RI_LEVI_PWM); /* PW: its own output is not audio */
    levi_trigger(&A, 0u, 59u);
    render(&A, oa, 9600u);
    plain(&B);
    levi_set_op_ui(&B, 0u, 1u, RI_LEVI_OP_ENVL, 127u);
    levi_set_op_ui(&B, 0u, 1u, RI_LEVI_OP_MODE, RI_LEVI_PWM);
    levi_set_op_ui(&B, 0u, 1u, RI_LEVI_OP_DIRECT, 1u);
    levi_trigger(&B, 0u, 59u);
    render(&B, ob, 9600u);
    RI_ASSERT(memcmp(oa, ob, 9600u * sizeof(float)) != 0, "direct out adds the modulator");

    /* ---- Envelope: attack time, curve, quantize, loop, freerun. ---- */
    plain(&A);
    levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 40u);        /* ~0.34 s */
    levi_trigger(&A, 0u, 69u);
    render(&A, oa, N);
    RI_ASSERT(peak(oa, 0u, 480u) < 0.1f && peak(oa, 24000u, 2400u) > 0.5f, "slow attack rises");
    RI_ASSERT(A.v[0].st[0][0].env.times[RI_LEVI_SEG_A] > 0.3f && A.v[0].st[0][0].env.times[RI_LEVI_SEG_A] < 0.4f,
        "attack 40 = %f s", A.v[0].st[0][0].env.times[RI_LEVI_SEG_A]);
    plain(&B);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_ATTACK, 40u);
    levi_set_op_ui(&B, 0u, 0u, RI_LEVI_OP_ACURVE, 0u);         /* exp: slow start */
    levi_trigger(&B, 0u, 69u);
    render(&B, ob, N);
    RI_ASSERT(peak(ob, 7000u, 480u) < peak(oa, 7000u, 480u), "exp attack starts slower: %f < %f",
        peak(ob, 7000u, 480u), peak(oa, 7000u, 480u));
    {   /* quantize: the envelope takes few distinct levels */
        struct RILeviEnv *e = &A.v[0].st[0][0].env;
        float seen[64];
        uint32_t ns = 0u, j;
        plain(&A);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 60u);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_QUANT, 3u);      /* 4 steps */
        levi_trigger(&A, 0u, 69u);
        for (k = 0u; k < N; k++) {
            float q;
            levi_voice_render(&A.v[0], 0, SR);
            q = (float)(int)(e->value * (float)e->quant + 0.5f) / (float)e->quant;
            for (j = 0u; j < ns && seen[j] != q; j++)
                ;
            if (j == ns && ns < 64u)
                seen[ns++] = q;
        }
        RI_ASSERT(e->quant == 4u && ns <= 5u, "quantize 4 steps: %u levels", ns);
    }
    {   /* loop 2x over Delay>Attack: the attack runs twice before hold/decay */
        struct RILeviEnv *e = &A.v[0].st[0][0].env;
        uint32_t attacks = 0u, prev = RI_LEVI_SEG_IDLE;
        float pv = 0.0f;
        plain(&A);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 30u);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_STAGELOOP, 0u);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_LOOP, 1u);       /* 2 passes */
        levi_trigger(&A, 0u, 69u);
        for (k = 0u; k < N; k++) {
            levi_voice_render(&A.v[0], 0, SR);
            if (e->stage == RI_LEVI_SEG_A && (prev != RI_LEVI_SEG_A || e->value < pv - 0.5f))
                attacks++;                   /* entry, or the loop's restart */
            prev = e->stage;
            pv = e->value;
        }
        RI_ASSERT(attacks == 2u, "loop 2 passes: %u attacks", attacks);
    }
    {   /* freerun: an early note-off still reaches the sustain stage */
        struct RILeviEnv *e = &A.v[0].st[0][0].env;
        uint32_t reached = 0u;
        plain(&A);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_ATTACK, 40u);
        levi_set_op_ui(&A, 0u, 0u, RI_LEVI_OP_FREERUN, 1u);
        /* P5: ENV 3 (the VCA) would close the voice at note-off; hold it open. */
        levi_set_param_ui(&A, 0u, RI_CTL_LEVI_VINIT & 0xFFu, 127u);
        levi_trigger(&A, 0u, 69u);
        render(&A, oa, 480u);
        levi_release(&A, 0u);
        for (k = 0u; k < N && !reached; k++) {
            levi_voice_render(&A.v[0], 0, SR);
            reached = e->stage == RI_LEVI_SEG_S || (e->stage == RI_LEVI_SEG_R && e->value > 0.9f);
        }
        RI_ASSERT(reached, "freerun runs to sustain before release");
    }

    /* ---- Bias knobs: env level down quiets, attack up slows. ---- */
    plain(&A);
    levi_trigger(&A, 0u, 69u);
    render(&A, oa, 9600u);
    plain(&B);
    RI_ASSERT(levi_set_param_ui(&B, 0u, RI_CTL_LEVI_BIAS_ENVL & 0xFFu, 32u) == 0, "bias envl");
    levi_trigger(&B, 0u, 69u);
    render(&B, ob, 9600u);
    RI_ASSERT(peak(ob, 4800u, 4800u) < peak(oa, 4800u, 4800u) * 0.7f, "negative level bias quiets");
    plain(&B);
    RI_ASSERT(levi_set_param_ui(&B, 0u, RI_CTL_LEVI_BIAS_ATK & 0xFFu, 127u) == 0, "bias attack");
    levi_trigger(&B, 0u, 69u);
    render(&B, ob, 9600u);
    RI_ASSERT(peak(ob, 0u, 120u) < peak(oa, 0u, 120u), "positive attack bias slows the attack");

    /* ---- Keys: 0x0F block allowed and applied by the engine. ---- */
    RI_ASSERT(ri_auto_allowed(RI_LEVI_OPKEY(7u, RI_LEVI_OP_VELENV)) && ri_auto_allowed(RI_LEVI_OPKEY(0u, RI_LEVI_OP_TGT3)) &&
        !ri_auto_allowed(0x1300u) &&
        ri_auto_allowed(RI_CTL_LEVI_BIAS_REL), "op keys allowed");
    {
        static struct RIEngine E;
        struct RIEvent a;
        ri_engine_init(&E);
        memset(&a, 0, sizeof a);
        a.type = RI_EV_AUTOMATION;
        a.device = 4u;
        a.value = RI_LEVI_OPKEY(2u, RI_LEVI_OP_WAVE);
        a.flags = 33u;
        ri_engine_apply_event(&E, &a);
        RI_ASSERT(E.slevi.v[0].op[2].wave == 33u && E.slevi.v[5].op[2].wave == 33u, "engine applies op key");
    }
    RI_RESULT("levi_osc");
}
