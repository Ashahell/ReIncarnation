/* levi.c — Levi FM voice bank bodies (owner 2026-09-28, v1 slice 3a).
 * 2-op FM/PM + per-operator DAHDSR + resonant lowpass. Render-contract
 * safe: bounded, no allocation, no IO; denormal-safe (ftz states, short
 * envelopes end exact).
 */
#include "engine/dsp/levi.h"
#include "engine/dsp/kernels.h"

/* E0 DAHDSR shape: fast pluck (delays/holds 0, attack 5 ms, decay
 * 300 ms to sustain 0.8, release 150 ms). Panel owns these later. */
#define LEVI_T_D 0.0f
#define LEVI_T_A 0.005f
#define LEVI_T_H 0.0f
#define LEVI_T_D2 0.300f
#define LEVI_T_R 0.150f
#define LEVI_SUS 0.8f
#define LEVI_REST_LEVEL 0.0001f /* -80 dBFS: inaudible, rests exact */

static float ftz(float x) {
    return (x > -1e-18f && x < 1e-18f) ? 0.0f : x;
}

static float note_hz(uint8_t note) {
    return 440.0f * ri_pow2(((float)note - 69.0f) / 12.0f);
}

static void env_reset(struct RILeviEnv *e) {
    e->times[0] = LEVI_T_D;
    e->times[1] = LEVI_T_A;
    e->times[2] = LEVI_T_H;
    e->times[3] = LEVI_T_D2;
    e->times[4] = 0.0f;
    e->times[5] = LEVI_T_R;
    e->sustain = LEVI_SUS;
    e->value = 0.0f;
    e->stage = RI_LEVI_SEG_D;
    e->stage_t = 0.0f;
}

/* One envelope sample; returns 1 while sounding, 0 at rest end. */
static int env_tick(struct RILeviEnv *e, float sr) {
    float dt, span, from, to;
    if (!e || sr <= 0.0f)
        return 0;
    if (e->stage == RI_LEVI_SEG_IDLE)
        return 0;
    if (e->stage == RI_LEVI_SEG_S) {
        if (e->loop) {
            /* Contour loop: sustain falls back to attack (own loop
             * law; release still rests via R). */
            e->stage = RI_LEVI_SEG_A;
            e->stage_t = 0.0f;
        } else {
            return 1;
        }
    }
    dt = 1.0f / sr;
    e->stage_t += dt;
    span = e->times[e->stage];
    from = e->value;
    switch (e->stage) {
    case RI_LEVI_SEG_D:
        to = 0.0f;
        break;
    case RI_LEVI_SEG_A:
        to = 1.0f;
        break;
    case RI_LEVI_SEG_H:
        to = 1.0f;
        break;
    case RI_LEVI_SEG_D2:
        to = e->sustain;
        break;
    default: /* RI_LEVI_SEG_R */
        to = 0.0f;
        break;
    }
    if (span <= 0.0f || e->stage_t >= span) {
        e->value = to;
        e->stage_t = 0.0f;
        if (e->stage == RI_LEVI_SEG_R) {
            e->stage = RI_LEVI_SEG_IDLE;
            e->value = 0.0f;
            return 0;
        }
        e->stage++;
        if (e->stage == RI_LEVI_SEG_S)
            e->value = e->sustain;
        return 1;
    }
    e->value = from + (to - from) * (e->stage_t / span);
    return 1;
}

static void op_state_reset(struct RILeviOpState *st, float freq) {
    st->phase = 0.0f;
    st->freq = freq;
    st->ps = 0.0f;
    /* Runtime only: times/sustain/loop are voice params (set_param),
     * preserved across triggers (unlike v1 constants). */
    st->env.value = 0.0f;
    st->env.stage = RI_LEVI_SEG_D;
    st->env.stage_t = 0.0f;
}

/* Own preset topologies (functional shapes; mod_src per op, -1 = carrier).
 * 0 DUO: v1 pair; 1 ALLPAR: 8 carriers; 2 STACK8: one chain into op0;
 * 3 STACK44: chains into op0/op4; 4 STACK422: chain + two pairs;
 * 5 PAIRS4: four pairs; 6 STACK332: 3+3+2; 7 STACK62: 6-chain + pair. */
static const int8_t RI_LEVI_PRESET_SRC[RI_LEVI_ALGO_N][RI_LEVI_NOPS] = {
    { -1, 0, -1, -1, -1, -1, -1, -1 },
    { -1, -1, -1, -1, -1, -1, -1, -1 },
    { -1, 0, 1, 2, 3, 4, 5, 6 },
    { -1, 0, 1, 2, -1, 4, 5, 6 },
    { -1, 0, 1, 2, -1, 4, -1, 6 },
    { -1, 0, -1, 2, -1, 4, -1, 6 },
    { -1, 0, 1, -1, 3, 4, -1, 6 },
    { -1, 0, 1, 2, 3, 4, -1, 6 },
};

static const uint8_t RI_LEVI_PRESET_LIVE[RI_LEVI_ALGO_N][RI_LEVI_NOPS] = {
    { 1, 1, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
};

/* Render order: modulators before their carriers (depth-descending).
 * Graph is acyclic by construction (presets) or validation (custom). */
static void order_compute_src(const int8_t *src, uint8_t *order) {
    uint8_t depth[RI_LEVI_NOPS], done[RI_LEVI_NOPS];
    uint32_t i, k, placed = 0u;
    for (i = 0u; i < RI_LEVI_NOPS; i++) {
        int cur = (int)i, d = 0;
        while (cur >= 0 && d <= (int)RI_LEVI_NOPS) {
            cur = src[cur];
            d++;
        }
        depth[i] = (uint8_t)(d - 1);
        done[i] = 0u;
        order[i] = (uint8_t)i;
    }
    for (k = 0u; k < RI_LEVI_NOPS; k++) {
        uint32_t best = RI_LEVI_NOPS;
        for (i = 0u; i < RI_LEVI_NOPS; i++)
            if (!done[i] && (best == RI_LEVI_NOPS || depth[i] > depth[best]))
                best = i;
        if (best == RI_LEVI_NOPS)
            break; /* unreachable: placed counts every op once */
        order[placed++] = (uint8_t)best;
        done[best] = 1u;
    }
    for (; placed < RI_LEVI_NOPS; placed++)
        order[placed] = 0u;
}

static void bank_preset(struct RILeviVoice *v, uint32_t bank,
    uint32_t algo) {
    uint32_t i;
    int8_t *src = bank ? v->mod_srcB : v->mod_src;
    uint8_t *ord = bank ? v->orderB : v->order;
    uint8_t *live = bank ? v->liveB : v->live;
    for (i = 0u; i < RI_LEVI_NOPS; i++) {
        src[i] = RI_LEVI_PRESET_SRC[algo][i];
        live[i] = RI_LEVI_PRESET_LIVE[algo][i];
    }
    order_compute_src(src, ord);
}

static void voice_preset(struct RILeviVoice *v, uint32_t algo) {
    bank_preset(v, 0u, algo);
    v->algo = (uint8_t)algo;
}

void levi_init_set(struct RILeviSet *s) {
    uint32_t i, o;
    if (!s)
        return;
    for (i = 0u; i < RI_LEVI_NVOICES; i++) {
        struct RILeviVoice *v = &s->v[i];
        uint32_t b;
        v->active = 0u;
        v->note = 0u;
        v->algoB = RI_LEVI_ALGO_DUO;
        v->morph = 0u;
        for (o = 0u; o < RI_LEVI_NOPS; o++) {
            v->op[o].ratio = RI_LEVI_DEF_RATIO;
            v->op[o].level = 1.0f;
            v->op[o].mode = RI_LEVI_FM;
        }
        for (b = 0u; b < 2u; b++) {
            bank_preset(v, b, RI_LEVI_ALGO_DUO);
            for (o = 0u; o < RI_LEVI_NOPS; o++) {
                v->st[b][o].phase = 0.0f;
                v->st[b][o].freq = 440.0f;
                v->st[b][o].ps = 0.0f;
                env_reset(&v->st[b][o].env);
                v->st[b][o].env.stage = RI_LEVI_SEG_IDLE;
                v->st[b][o].env.loop = 0u;
            }
        }
        voice_preset(v, RI_LEVI_ALGO_DUO);
        v->cutoff = RI_LEVI_DEF_CUTOFF;
        v->reso = RI_LEVI_DEF_RESO;
        v->level = 1.0f;
        v->drive = 0.0f;
        v->ftype = RI_LEVI_FTYPE_LP;
        v->cutoff2 = RI_LEVI_DEF_CUTOFF;
        v->reso2 = RI_LEVI_DEF_RESO;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        v->lp3 = 0.0f;
        v->lp4 = 0.0f;
    }
    s->arpon = 0u;
    s->arprate = 64u;
    s->seqon = 0u;
    s->seqlen = 16u;
}

int levi_trigger(struct RILeviSet *s, uint32_t voice, uint8_t note) {
    struct RILeviVoice *v;
    float f;
    uint32_t o, b;
    if (!s || voice >= RI_LEVI_NVOICES || note > 127u)
        return 2;
    v = &s->v[voice];
    f = note_hz(note);
    v->active = 1u;
    v->note = note;
    for (b = 0u; b < 2u; b++) {
        uint8_t *live = b ? v->liveB : v->live;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (live[o])
                op_state_reset(&v->st[b][o], f * v->op[o].ratio);
    }
    v->lp1 = 0.0f;
    v->lp2 = 0.0f;
    v->lp3 = 0.0f;
    v->lp4 = 0.0f;
    return 0;
}

void levi_release(struct RILeviSet *s, uint32_t voice) {
    struct RILeviVoice *v;
    uint32_t o, b;
    if (!s || voice >= RI_LEVI_NVOICES)
        return;
    v = &s->v[voice];
    for (b = 0u; b < 2u; b++) {
        uint8_t *live = b ? v->liveB : v->live;
        for (o = 0u; o < RI_LEVI_NOPS; o++) {
            if (live[o] && v->st[b][o].env.stage != RI_LEVI_SEG_IDLE) {
                v->st[b][o].env.stage = RI_LEVI_SEG_R;
                v->st[b][o].env.stage_t = 0.0f;
            }
        }
    }
}

int levi_set_param(struct RILeviSet *s, uint32_t voice, uint32_t id,
    float value) {
    struct RILeviVoice *v;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES)
        return 2;
    v = &s->v[voice];
    switch (id) {
    case RI_LEVI_CUTOFF:
        if (!(value >= 40.0f && value <= 18000.0f))
            return 2;
        v->cutoff = value;
        return 0;
    case RI_LEVI_RESO:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        v->reso = value;
        return 0;
    case RI_LEVI_MODE:
        if (!(value >= 0.0f && value <= (float)(RI_LEVI_NMODES - 1u)))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->op[o].mode = (uint8_t)value;
        return 0;
    case RI_LEVI_RATIO:
        if (!(value >= 0.25f && value <= 64.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o] && v->mod_src[o] >= 0)
                v->op[o].ratio = value;
        return 0;
    case RI_LEVI_FTYPE:
        if (value != (float)RI_LEVI_FTYPE_LP && value != (float)RI_LEVI_FTYPE_HP &&
            value != (float)RI_LEVI_FTYPE_BP && value != (float)RI_LEVI_FTYPE_NOTCH)
            return 2;
        v->ftype = (uint8_t)value;
        return 0;
    case RI_LEVI_DRIVE:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        v->drive = value;
        return 0;
    case RI_LEVI_CUTOFF2:
        if (!(value >= 40.0f && value <= 18000.0f))
            return 2;
        v->cutoff2 = value;
        return 0;
    case RI_LEVI_RESO2:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        v->reso2 = value;
        return 0;
    case RI_LEVI_ATTACK:
    case RI_LEVI_DECAY:
    case RI_LEVI_RELEASE: {
        uint32_t tt = id == RI_LEVI_ATTACK ? RI_LEVI_SEG_A
            : id == RI_LEVI_DECAY ? RI_LEVI_SEG_D2 : RI_LEVI_SEG_R;
        if (!(value >= 0.001f && value <= 2.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->st[0][o].env.times[tt] = v->st[1][o].env.times[tt] = value;
        return 0;
    }
    case RI_LEVI_SUSTAIN:
        if (!(value >= 0.0f && value <= 1.0f))
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->st[0][o].env.sustain = v->st[1][o].env.sustain = value;
        return 0;
    case RI_LEVI_LOOP:
        if (value != 0.0f && value != 1.0f)
            return 2;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            if (v->live[o])
                v->st[0][o].env.loop = v->st[1][o].env.loop = (uint8_t)value;
        return 0;
    default:
        return 2;
    }
}

/* Resonant multimode (clean-room 2-pole Chamberlin SVF + driven
 * 24 dB LP cascade): stage 1 selects the LP/HP/BP/notch tap, drive
 * saturates pre-filter, stage 2 re-filters the tap at the same tuning
 * (hardware digital-into-analog path, own topology). Cutoff/reso/drive
 * modulate per render; states flushed (denormal-safe). f is clamped to
 * 1.0 (Dell 2026-09-28: the 1.8 ceiling admitted tunings past the
 * stability limit — inf/NaN ~300 samples after trigger, latched). */
static float lp_step(struct RILeviVoice *v, float x, float sr) {
    float f, q, hp, bp, lp, tap, k, xd, hp2, bp2, lp2;
    if (!(sr > 0.0f))
        return 0.0f;
    f = 2.0f * ri_sin(3.14159265f * v->cutoff / sr);
    if (f > 1.0f)
        f = 1.0f;
    if (f < 0.02f)
        f = 0.02f;
    q = 1.0f - v->reso * 0.85f;
    if (q < 0.05f)
        q = 0.05f;
    k = v->drive < 0.0f ? 0.0f : v->drive > 1.0f ? 1.0f : v->drive;
    xd = x * (1.0f + 4.0f * k) / (1.0f + 4.0f * k * (x < 0.0f ? -x : x));
    hp = xd - v->lp1 * q - v->lp2;
    bp = v->lp1 + f * hp;
    lp = v->lp2 + f * bp;
    v->lp1 = ftz(bp);
    v->lp2 = ftz(lp);
    if (v->ftype == RI_LEVI_FTYPE_HP)
        tap = hp;
    else if (v->ftype == RI_LEVI_FTYPE_BP)
        tap = bp;
    else if (v->ftype == RI_LEVI_FTYPE_NOTCH)
        tap = lp + hp;
    else
        tap = lp;
    {
        float f2 = 2.0f * ri_sin(3.14159265f * v->cutoff2 / sr);
        float q2 = 1.0f - v->reso2 * 0.85f;
        if (f2 > 1.0f)
            f2 = 1.0f;
        if (f2 < 0.02f)
            f2 = 0.02f;
        if (q2 < 0.05f)
            q2 = 0.05f;
        hp2 = tap - v->lp3 * q2 - v->lp4;
        bp2 = v->lp3 + f2 * hp2;
    }
    lp2 = v->lp4 + f * bp2;
    v->lp3 = ftz(bp2);
    v->lp4 = ftz(lp2);
    return lp2;
}

int levi_set_param_ui(struct RILeviSet *s, uint32_t voice, uint32_t id,
    uint8_t val) {
    float f;
    if (!s || voice >= RI_LEVI_NVOICES)
        return 2;
    switch (id) {
    case RI_LEVI_CUTOFF:
        f = 40.0f * ri_pow2(((float)val / 127.0f) * 8.5f);
        if (f > 18000.0f)
            f = 18000.0f;
        return levi_set_param(s, voice, id, f);
    case RI_LEVI_RESO:
        return levi_set_param(s, voice, id, (float)val / 127.0f);
    case RI_LEVI_MODE:
        return levi_set_param(s, voice, id, val != 0u ? 1.0f : 0.0f);
    case RI_LEVI_RATIO:
        return levi_set_param(s, voice, id,
            0.25f * ri_pow2(((float)val / 127.0f) * 8.0f));
    case (RI_CTL_LEVI_ALGO & 0xFFu): /* ALGO */
        return levi_set_algo(s, voice, val <= 7u ? val : (uint32_t)(val >> 4));
    case (RI_CTL_LEVI_ALGOB & 0xFFu): /* ALGOB */ {
        uint32_t a = val <= 7u ? val : (uint32_t)(val >> 4);
        struct RILeviVoice *v;
        if (a >= RI_LEVI_ALGO_N)
            return 2;
        v = &s->v[voice];
        bank_preset(v, 1u, a);
        v->algoB = (uint8_t)a;
        return 0;
    }
    case (RI_CTL_LEVI_MORPH & 0xFFu): /* MORPH */
        return levi_set_morph(s, voice, s->v[voice].algoB, val > 100u ? 100u : val);
    case (RI_CTL_LEVI_OPMODE & 0xFFu): /* OPMODE */ {
        uint32_t op = (uint32_t)val >> 4u, mode = (uint32_t)val & 0xFu;
        if (op >= RI_LEVI_NOPS || mode >= RI_LEVI_NMODES)
            return 2;
        return levi_set_op_mode(s, voice, op, mode);
    }
    case (RI_CTL_LEVI_FTYPE & 0xFFu): /* FTYPE */
        return levi_set_param(s, voice, RI_LEVI_FTYPE,
            val <= 3u ? (float)val : (float)(val >> 5));
    case (RI_CTL_LEVI_DRIVE & 0xFFu): /* DRIVE */
        return levi_set_param(s, voice, RI_LEVI_DRIVE, (float)val / 127.0f);
    case (RI_CTL_LEVI_CUTOFF2 & 0xFFu): /* CUTOFF2 */
        f = 40.0f * ri_pow2(((float)val / 127.0f) * 8.5f);
        if (f > 18000.0f)
            f = 18000.0f;
        return levi_set_param(s, voice, RI_LEVI_CUTOFF2, f);
    case (RI_CTL_LEVI_RESO2 & 0xFFu): /* RESO2 */
        return levi_set_param(s, voice, RI_LEVI_RESO2, (float)val / 127.0f);
    case (RI_CTL_LEVI_ATTACK & 0xFFu): /* ATTACK */
    case (RI_CTL_LEVI_DECAY & 0xFFu): /* DECAY */
    case (RI_CTL_LEVI_RELEASE & 0xFFu): { /* RELEASE */
        float t = 0.001f * ri_pow2(((float)val / 127.0f) * 11.0f);
        uint32_t pid = id == (RI_CTL_LEVI_ATTACK & 0xFFu) ? RI_LEVI_ATTACK
            : id == (RI_CTL_LEVI_DECAY & 0xFFu) ? RI_LEVI_DECAY : RI_LEVI_RELEASE;
        if (t > 2.0f)
            t = 2.0f;
        return levi_set_param(s, voice, pid, t);
    }
    case (RI_CTL_LEVI_SUSTAIN & 0xFFu): /* SUSTAIN */
        return levi_set_param(s, voice, RI_LEVI_SUSTAIN, (float)val / 127.0f);
    case (RI_CTL_LEVI_LOOP & 0xFFu): /* LOOP */
        return levi_set_param(s, voice, RI_LEVI_LOOP, val != 0u ? 1.0f : 0.0f);
    case (RI_CTL_LEVI_ARPON & 0xFFu): /* ARPON (device arp gate) */
        s->arpon = val != 0u ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_ARPRATE & 0xFFu): /* ARPRATE (device arp rate) */
        s->arprate = val;
        return 0;
    case (RI_CTL_LEVI_SEQON & 0xFFu): /* SEQON (device seq gate) */
        s->seqon = val != 0u ? 1u : 0u;
        return 0;
    case (RI_CTL_LEVI_SEQLEN & 0xFFu): /* SEQLEN (device seq length) */
        s->seqlen = val < 1u ? 1u : val > 16u ? 16u : val;
        return 0;
    default:
        return 2;
    }
}

/* One morph-bank pass: carriers under this bank's routing into mix.
 * Returns the carrier mix; ORs envelope activity into *any_on. */
static float voice_pass(struct RILeviVoice *v, uint32_t bank, float sr,
    int *any_on) {
    float opout[RI_LEVI_NOPS] = { 0.0f }, mix = 0.0f;
    const int8_t *src = bank ? v->mod_srcB : v->mod_src;
    const uint8_t *ord = bank ? v->orderB : v->order;
    const uint8_t *live = bank ? v->liveB : v->live;
    uint32_t k, j;
    for (k = 0u; k < RI_LEVI_NOPS; k++) {
        uint32_t i = ord[k];
        struct RILeviOp *p = &v->op[i];
        struct RILeviOpState *o = &v->st[bank][i];
        float m, osc;
        int on;
        if (i >= RI_LEVI_NOPS || !live[i]) {
            opout[k & (RI_LEVI_NOPS - 1u)] = 0.0f;
            continue;
        }
        on = env_tick(&o->env, sr);
        *any_on |= on;
        if (!on) {
            opout[i] = 0.0f;
            continue;
        }
        /* Feeders render first by topological order, so their slots
         * are filled (zero-init covers custom edits mid-flight). */
        m = 0.0f;
        for (j = 0u; j < RI_LEVI_NOPS; j++)
            if (src[j] == (int)i)
                m += opout[j];
        if (p->mode == RI_LEVI_FM)
            o->phase += (o->freq + o->freq * RI_LEVI_MOD_INDEX * m) / sr;
        else
            o->phase += o->freq / sr;
        if (o->phase >= 1.0f)
            o->phase -= 1.0f;
        if (o->phase < 0.0f)
            o->phase += 1.0f;
        switch (p->mode) {
        case RI_LEVI_PM:
            osc = ri_sin((o->phase + RI_LEVI_MOD_INDEX * m) * 6.2831853f);
            break;
        case RI_LEVI_PWM: {
            float w = 0.5f + 0.4f * m;
            osc = (o->phase < w ? 1.0f : -1.0f) * 0.7f;
            break;
        }
        case RI_LEVI_SYNC: {
            if (o->ps <= 0.0f && m > 0.0f)
                o->phase = 0.5f + 0.5f * (m > 1.0f ? 1.0f : m);
            osc = 2.0f * o->phase - 1.0f;
            break;
        }
        case RI_LEVI_PDSAW: {
            float ph = ri_sin(o->phase * 6.2831853f);
            float am = m < 0.0f ? -m : m;
            osc = (ph + m * ph * ph) / (1.0f + am);
            break;
        }
        case RI_LEVI_PDSQ: {
            float ph = ri_sin(o->phase * 6.2831853f);
            float kk = 2.0f * (m < 0.0f ? -m : m);
            float aa = ph < 0.0f ? -ph : ph;
            osc = ph * (1.0f + kk) / (1.0f + kk * aa);
            break;
        }
        case RI_LEVI_PDPULSE: {
            float ph = ri_sin(o->phase * 6.2831853f);
            float depth = m < 0.0f ? -m : m;
            float sq = o->phase < 0.25f || o->phase >= 0.75f ? 0.8f : -0.8f;
            osc = ph + (sq - ph) * (depth > 1.0f ? 1.0f : depth);
            break;
        }
        default: /* FM */
            osc = ri_sin(o->phase * 6.2831853f);
            break;
        }
        o->ps = m;
        opout[i] = osc * p->level * o->env.value;
        if (src[i] < 0)
            mix += opout[i];
    }
    return mix;
}

float levi_voice_render(struct RILeviVoice *v, float sr) {
    float mixA, mixB, mix, out;
    int any_on = 0;
    if (!v || !v->active || !(sr > 0.0f))
        return 0.0f;
    mixA = voice_pass(v, 0u, sr, &any_on);
    mixB = voice_pass(v, 1u, sr, &any_on);
    /* Exact endpoints (bit-identity with no-morph / pure-B voices);
     * the slide blends between them. */
    if (v->morph == 0u)
        mix = mixA;
    else if (v->morph >= 100u)
        mix = mixB;
    else
        mix = mixA + (mixB - mixA) * ((float)v->morph / 100.0f);
    if (!any_on) {
        v->active = 0u;
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        v->lp3 = 0.0f;
        v->lp4 = 0.0f;
        return 0.0f;
    }
    out = lp_step(v, mix, sr);
    if (!(out > -1e20f && out < 1e20f)) {
        /* Non-finite latch guard (Dell 2026-09-28): a poisoned filter
         * state must self-heal to silence, never mute the mix. */
        v->lp1 = 0.0f;
        v->lp2 = 0.0f;
        v->lp3 = 0.0f;
        v->lp4 = 0.0f;
        return 0.0f;
    }
    return out * v->level;
}

void levi_voice_render_sum(struct RILeviSet *s, float *out, uint32_t n,
    float sr) {
    uint32_t i, v;
    if (!s || !out)
        return;
    for (i = 0u; i < n; i++) {
        float m = 0.0f;
        for (v = 0u; v < RI_LEVI_NVOICES; v++)
            m += levi_voice_render(&s->v[v], sr);
        out[i] = m;
    }
}

int levi_set_algo(struct RILeviSet *s, uint32_t voice, uint32_t algo) {
    if (!s || voice >= RI_LEVI_NVOICES || algo >= RI_LEVI_ALGO_N)
        return 2;
    voice_preset(&s->v[voice], algo);
    return 0;
}

int levi_algo_get(const struct RILeviSet *s, uint32_t voice) {
    if (!s || voice >= RI_LEVI_NVOICES)
        return -1;
    return (int)s->v[voice].algo;
}

int levi_set_route(struct RILeviSet *s, uint32_t voice, uint32_t op,
    int src) {
    struct RILeviVoice *v;
    int cur;
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return 2;
    if (src < -1 || src >= (int)RI_LEVI_NOPS)
        return 2;
    v = &s->v[voice];
    /* Acyclic only: src's chain must not reach op (self hits at once). */
    for (cur = src; cur >= 0; cur = v->mod_src[cur])
        if (cur == (int)op)
            return 2;
    v->mod_src[op] = (int8_t)src;
    v->live[op] = 1u;
    v->algo = RI_LEVI_ALGO_CUSTOM;
    order_compute_src(v->mod_src, v->order);
    return 0;
}

int levi_route_get(const struct RILeviSet *s, uint32_t voice,
    uint32_t op) {
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return -2;
    return (int)s->v[voice].mod_src[op];
}

int levi_set_op_mode(struct RILeviSet *s, uint32_t voice, uint32_t op,
    uint32_t mode) {
    if (!s || voice >= RI_LEVI_NVOICES || op >= RI_LEVI_NOPS)
        return 2;
    if (mode >= RI_LEVI_NMODES)
        return 2;
    s->v[voice].op[op].mode = (uint8_t)mode;
    return 0;
}

int levi_set_morph(struct RILeviSet *s, uint32_t voice, uint32_t algoB,
    uint32_t pos) {
    struct RILeviVoice *v;
    uint32_t o;
    if (!s || voice >= RI_LEVI_NVOICES || algoB >= RI_LEVI_ALGO_N)
        return 2;
    if (pos > 100u)
        return 2;
    v = &s->v[voice];
    bank_preset(v, 1u, algoB);
    v->algoB = (uint8_t)algoB;
    for (o = 0u; o < RI_LEVI_NOPS; o++)
        v->st[1][o] = v->st[0][o];
    v->morph = (uint8_t)pos;
    return 0;
}

int levi_morph_get(const struct RILeviSet *s, uint32_t voice) {
    if (!s || voice >= RI_LEVI_NVOICES)
        return -1;
    return (int)s->v[voice].morph;
}
