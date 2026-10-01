/* sectlevi.c — Levi section behaviour bodies (owner 2026-09-28). */
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"
#include "engine/dsp/levi.h"
#include "engine/dsp/kernels.h"

/* Page-slot codes (see the page UI block below): a panel control index
 * (>= 0), a dead slot (engine in a later phase), or a per-oscillator
 * param of the page's oscillator (SLOT_OP) or of oscillator k on a group
 * page (SLOT_GOP). */
#define SLOT_DEAD (-1)
#define SLOT_OP(p) (-(16 + (int)(p)))
#define SLOT_GOP(p) (-(64 + (int)(p)))
/* Per-op ENV BPM flag (fidelity P8a): param 32, inside the OP range. */
#define SLOT_OPBPM (-(16 + 32))
/* Per-menv BPM flag (P8a): -150 sits in the free gap (LF ends at -144,
 * matrix starts at -160) and is matched exactly, never by range. */
#define SLOT_MEBPM (-150)
#define SLOT_IS_OP(x) ((x) <= -16 && (x) > -64)
#define SLOT_IS_GOP(x) ((x) <= -64 && (x) > -96)
#define SLOT_PARAM(x) ((uint32_t)(SLOT_IS_GOP(x) ? -(x) - 64 : -(x) - 16))
/* ENV 1-5 / LFO 1-5 params of the page's unit (fidelity P5). */
#define SLOT_ME(p) (-(96 + (int)(p)))
#define SLOT_LF(p) (-(128 + (int)(p)))
#define SLOT_IS_ME(x) ((x) <= -96 && (x) > -128)
#define SLOT_IS_LF(x) ((x) <= -128 && (x) > -144)
#define SLOT_MPARAM(x) ((uint32_t)(SLOT_IS_LF(x) ? -(x) - 128 : -(x) - 96))
/* Matrix route r field f / macro m route r field f (P5b). */
#define SLOT_MX(r, f) (-(160 + (int)(r) * 4 + (int)(f)))
#define SLOT_MR(m, r, f) (-(288 + ((int)(m) * 8 + (int)(r)) * 4 + (int)(f)))
#define SLOT_IS_MX(x) ((x) <= -160 && (x) > -288)
#define SLOT_IS_MR(x) ((x) <= -288 && (x) > -544)

static const struct RICtlDef *def(const struct RISectLevi *s, uint32_t idx) {
    (void)s;
    return ri_ctlreg_find((uint16_t)((RI_SEC_LEVI << 8) | idx));
}

int ri_slevi_init(struct RISectLevi *s) {
    uint32_t i;
    if (!s)
        return 2;
    s->section = RI_SEC_LEVI;
    s->sel = 0u;
    s->edit_step = 0u;
    s->opsel = 0u;
    for (i = 0u; i < RI_LEVI_NOPS; i++)
        s->opmode[i] = RI_LEVI_FM;
    for (i = 0u; i < RI_SLEVI_NCTL; i++) {
        const struct RICtlDef *d = def(s, i);
        s->val[i] = d ? d->def_v : 0;
    }
    s->val[RI_SLEVI_OPMODE] = 0; /* packed op*16+mode, op 0 FM */
    s->page = 0u;
    s->pad[0] = s->pad[1] = s->pad[2] = 0u;
    {
        uint32_t o, p;
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            for (p = 0u; p < RI_LEVI_OP_NPARAM; p++)
                s->opv[o][p] = (uint8_t)ri_levi_op_default(o, p);
        for (o = 0u; o < RI_LEVI_NMENV; o++)
            for (p = 0u; p < RI_LEVI_OP_NPARAM; p++)
                s->mev[o][p] = (uint8_t)ri_levi_menv_default(o, p);
        for (o = 0u; o < RI_LEVI_NLFO; o++)
            for (p = 0u; p < 16u; p++)
                s->lfv[o][p] = (uint8_t)(p < RI_LEVI_LP_N ? ri_levi_lfo_default(p) : 0);
        for (o = 0u; o < RI_LEVI_MX_NSLOTS; o++) {
            s->mxv[o][0] = s->mxv[o][1] = s->mxv[o][2] = 0u;
            s->mxv[o][3] = 64u;
        }
        for (o = 0u; o < RI_LEVI_NMACRO; o++)
            for (p = 0u; p < RI_LEVI_MACRO_NR; p++) {
                s->mrv[o][p][0] = s->mrv[o][p][1] = s->mrv[o][p][3] = 0u;
                s->mrv[o][p][2] = 64u;
            }
        for (o = 0u; o < RI_LEVI_NOPS; o++)
            s->opbpm[o] = 0u;
        for (o = 0u; o < RI_LEVI_NMENV; o++)
            s->mebpm[o] = 0u;
        s->bpmpad[0] = s->bpmpad[1] = s->bpmpad[2] = 0u;
    }
    ri_pattern_init(&s->pat, RI_PATTERN_KIND_LEVI, 0u);
    return 0;
}

int ri_slevi_press(struct RISectLevi *s, uint32_t idx) {
    uint32_t e;
    if (!s || idx >= RI_SLEVI_NCTL)
        return 0;
    if (idx == RI_SLEVI_STEP) {
        s->edit_step = (uint8_t)((s->edit_step + 1u) % RI_PATTERN_STEPS);
        return 1;
    }
    if (idx == RI_SLEVI_PAGEUP || idx == RI_SLEVI_PAGEDN) {
        uint32_t n = ri_slevi_page_count(s);
        uint8_t pg = idx == RI_SLEVI_PAGEUP ? (uint8_t)(s->page ? s->page - 1u : 0u)
            : (uint8_t)(s->page + 1u < n ? s->page + 1u : s->page);
        if (pg == s->page)
            return 0;
        s->page = pg;
        return 1;
    }
    if (idx == RI_SLEVI_MODE) {
        s->val[RI_SLEVI_MODE] = (int16_t)(s->val[RI_SLEVI_MODE] ? 0 : 1);
        return 1;
    }
    if (idx == RI_SLEVI_ARPON) {
        /* Bound arp gate (v2 feature 3b): panel truth toggles like
         * MODE; automation emit travels app-side via the knob syncs. */
        s->val[RI_SLEVI_ARPON] = (int16_t)(s->val[RI_SLEVI_ARPON] ? 0 : 1);
        return 1;
    }
    if (idx == RI_SLEVI_SEQON) {
        /* Bound seq gate (v2 feature 3c): same MODE-style toggle. */
        s->val[RI_SLEVI_SEQON] = (int16_t)(s->val[RI_SLEVI_SEQON] ? 0 : 1);
        return 1;
    }
    if (idx == RI_SLEVI_FXPRE || idx == RI_SLEVI_FXPOST) {
        /* Bound FX gates (fidelity P7c): panel truth toggles like MODE;
         * automation emit travels app-side via the knob syncs. */
        s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
        return 1;
    }
    if (idx == RI_SLEVI_RBNTOUCH) {
        /* Ribbon touch (fidelity P8d): panel truth toggles like MODE. */
        s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
        return 1;
    }
    if (idx >= RI_SLEVI_ROUTE0 && idx < RI_SLEVI_ROUTE0 + 8u) {
        /* Bound route gates (v2 feature 4c): panel truth toggles like
         * MODE; automation emit travels app-side via the knob syncs. */
        s->val[idx] = (int16_t)(s->val[idx] ? 0 : 1);
        return 1;
    }
    if (idx < RI_SLEVI_NCTL) {
        /* Generic front-panel truth for UI-only switches (ARP/SEQ/
         * MATRIX/FX ride here until their engines land; bound controls
         * travel the automation path instead). Knobs arrive via set. */
        const struct RICtlDef *d = def(s, idx);
        if (d && d->kind == RI_CK_SWITCH && d->bind == RI_BIND_NONE) {
            int v = s->val[idx] ? d->min_v : d->max_v;
            if (s->val[idx] == v)
                return 0;
            s->val[idx] = (int16_t)v;
            return 1;
        }
    }
    if (idx == RI_SLEVI_BACK) {
        s->edit_step = (uint8_t)((s->edit_step + RI_PATTERN_STEPS - 1u) % RI_PATTERN_STEPS);
        return 1;
    }
    if (idx >= RI_SLEVI_STEP0 && idx < RI_SLEVI_STEP0 + RI_PATTERN_STEPS) {
        uint32_t st = idx - RI_SLEVI_STEP0;
        if (ri_levi_on(&s->pat, st, s->sel))
            return ri_levi_set(&s->pat, st, s->sel,
                (uint8_t)ri_levi_get(&s->pat, st, s->sel), 0) == 0;
        return ri_levi_set(&s->pat, st, s->sel, RI_SLEVI_MIDDLE_C, 1) == 0;
    }
    if (idx >= RI_SLEVI_KEY0 && idx < RI_SLEVI_KEY0 + RI_SLEVI_KEYS) {
        e = s->edit_step % RI_PATTERN_STEPS;
        return ri_levi_set(&s->pat, e, s->sel,
            (uint8_t)(RI_SLEVI_MIDDLE_C + (idx - RI_SLEVI_KEY0)), 1) == 0;
    }
    return 0;
}

static int slot(const struct RISectLevi *s, uint32_t k, const char **name);
static int enc_of(uint32_t idx);
static int enc_set(struct RISectLevi *s, uint32_t k, int v);
static uint32_t slot_op(const struct RISectLevi *s, int t, uint32_t k);
static uint8_t *mod_val(const struct RISectLevi *s, int t, int *lo, int *hi, uint16_t *key, int *dv);
static void put_str(char *buf, uint32_t cap, const char *src);
static void cat_str(char *buf, uint32_t cap, const char *src);
static void cat_num(char *buf, uint32_t cap, int v);

int ri_slevi_set_value(struct RISectLevi *s, uint32_t idx, int v) {
    const struct RICtlDef *d = s && idx < RI_SLEVI_NCTL ? def(s, idx) : 0;
    if (!d)
        return 0;
    if (enc_of(idx) >= 0)
        return enc_set(s, (uint32_t)enc_of(idx), v);
    if (idx == RI_SLEVI_MODULE) {
        int m = v < 0 ? 0 : v >= (int)RI_SLEVI_NMOD ? (int)RI_SLEVI_NMOD - 1 : v;
        if (s->val[idx] == m)
            return 0;
        s->val[idx] = (int16_t)m;
        s->page = 0u;
        return 1;
    }
    if (idx == RI_SLEVI_SELECT) {
        uint8_t sel = v < 0 ? 0u : v > 5 ? 5u : (uint8_t)v;
        if (s->sel == sel)
            return 0;
        s->sel = sel;
        return 1;
    }
    if (idx == RI_SLEVI_ALGO || idx == RI_SLEVI_ALGOB) {
        /* Algorithm select with the slot-1 mirror (engine does the same). */
        int w = v < 0 ? 0 : v > 63 ? 63 : v;
        if (s->val[idx] == w)
            return 0;
        s->val[idx] = (int16_t)w;
        if (idx == RI_SLEVI_ALGO)
            s->val[RI_SLEVI_SLOT0] = (int16_t)w;   /* ALGO is slot 1 (engine does the same) */
        return 1;
    }
    if (idx == RI_SLEVI_SLOT0) {
        int w = v < 0 ? 0 : v > 63 ? 63 : v;
        if (s->val[idx] == w)
            return 0;
        s->val[idx] = (int16_t)w;
        s->val[RI_SLEVI_ALGO] = (int16_t)w;
        return 1;
    }
    if (idx == RI_SLEVI_OPSEL) {
        uint8_t o = v < 0 ? 0u : v > 7 ? 7u : (uint8_t)v;
        int packed = (int)o * 16 + s->opmode[o];
        if (s->opsel == o && s->val[RI_SLEVI_MODULE] == (int16_t)RI_SLEVI_M_OSC) {
            /* Pressing the open oscillator again steps its pages (manual
             * p. 34: repeated presses select the other pages). */
            s->page = (uint8_t)((s->page + 1u) % ri_slevi_page_count(s));
            s->val[RI_SLEVI_OPMODE] = (int16_t)packed;
            return 1;
        }
        if (s->val[RI_SLEVI_MODULE] != (int16_t)RI_SLEVI_M_OSC)
            s->page = 0u;                 /* Page Recall keeps the page across oscillators */
        s->opsel = o;
        s->val[RI_SLEVI_MODULE] = (int16_t)RI_SLEVI_M_OSC; /* OSC n opens its page */
        s->val[RI_SLEVI_OPMODE] = (int16_t)packed;
        return 1;
    }
    if (idx == RI_SLEVI_OPMODE) {
        /* Shared knob: panel truth per op, packed op*16+mode on the wire. */
        int m = v < 0 ? 0 : v > 6 ? 6 : v;
        int packed = (int)s->opsel * 16 + m;
        if (s->val[idx] == packed && s->opmode[s->opsel] == (uint8_t)m)
            return 0;
        s->opmode[s->opsel] = (uint8_t)m;
        s->val[idx] = (int16_t)packed;
        return 1;
    }
    if (d->kind != RI_CK_KNOB && d->kind != RI_CK_SWITCH && d->kind != RI_CK_SELECTOR)
        return 0;
    if (v < d->min_v)
        v = d->min_v;
    if (v > d->max_v)
        v = d->max_v;
    if (s->val[idx] == v)
        return 0;
    s->val[idx] = (int16_t)v;
    return 1;
}

int ri_slevi_reset(struct RISectLevi *s, uint32_t idx) {
    const struct RICtlDef *d;
    if (s && enc_of(idx) >= 0) {          /* an encoder resets its target */
        uint32_t k = (uint32_t)enc_of(idx);
        int t = slot(s, k, 0), dv = 0;
        uint8_t *mv = mod_val(s, t, 0, 0, 0, &dv);
        if (mv) {
            if (*mv == (uint8_t)dv)
                return 0;
            *mv = (uint8_t)dv;
            return 1;
        }
        if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
            uint32_t o = slot_op(s, t, k), p = SLOT_PARAM(t);
            if (p == 32u) {   /* per-op ENV BPM flag (fidelity P8a) */
                if (s->opbpm[o] == 0u)
                    return 0;
                s->opbpm[o] = 0u;
                return 1;
            }
            uint8_t dv = (uint8_t)ri_levi_op_default(o, p);
            if (s->opv[o][p] == dv)
                return 0;
            s->opv[o][p] = dv;
            if (p == RI_LEVI_OP_MODE)
                s->opmode[o] = dv;
            return 1;
        }
        return t >= 0 ? ri_slevi_reset(s, (uint32_t)t) : 0;
    }
    d = s && idx < RI_SLEVI_NCTL ? def(s, idx) : 0;
    return d ? ri_slevi_set_value(s, idx, d->def_v) : 0;
}

int ri_slevi_led(const struct RISectLevi *s, uint32_t idx) {
    uint32_t e;
    if (!s || idx >= RI_SLEVI_NCTL)
        return 0;
    e = s->edit_step % RI_PATTERN_STEPS;
    if (idx >= RI_SLEVI_STEP0 && idx < RI_SLEVI_STEP0 + RI_PATTERN_STEPS)
        return ri_levi_on(&s->pat, idx - RI_SLEVI_STEP0, s->sel);
    if (idx >= RI_SLEVI_KEY0 && idx < RI_SLEVI_KEY0 + RI_SLEVI_KEYS)
        return ri_levi_get(&s->pat, e, s->sel) ==
            (uint32_t)(RI_SLEVI_MIDDLE_C + (idx - RI_SLEVI_KEY0));
    return 0;
}

int ri_slevi_display(const struct RISectLevi *s) {
    if (!s)
        return 0;
    return (int)(s->edit_step % RI_PATTERN_STEPS) + 1;
}

int ri_slevi_algo_display(const struct RISectLevi *s) {
    uint32_t a;
    if (!s)
        return 0;
    if (s->val[RI_SLEVI_AMODE] == 2)
        return 0;                          /* custom: the readout shows C */
    a = (uint32_t)s->val[RI_SLEVI_ALGO] + 1u;
    return a < 1u ? 1 : (int)(a > 64u ? 64u : a);
}

/* ---- Hardware page UI (fidelity plan P1/P2, owner 2026-09-30) ----
 * Page slots follow the manual's control-knob order per module: osc
 * settings pages 1-5 (pp. 35-40), envelopes (p. 71), digital filter
 * (p. 62), analog filter (p. 66), VCA (p. 69), FX (pp. 83-86), LFO
 * (p. 76), arp (p. 99). The Oscillator Group Edit keys show one param
 * for all 8 oscillators (encoder k = oscillator k). */
struct LeviSlot {
    int16_t idx;
    const char *name;
};

static const char *const OSC_NAME[RI_LEVI_NOPS] = {
    "OSC 1", "OSC 2", "OSC 3", "OSC 4", "OSC 5", "OSC 6", "OSC 7", "OSC 8"
};

#define OP(p) SLOT_OP(RI_LEVI_OP_##p)
static const struct LeviSlot P_OSC[5][8] = {
    { { OP(MODE), "MODE" }, { OP(WAVE), "WAVE" }, { OP(COARSE), "PITCH" }, { OP(FINE), "FINE" },
      { OP(INIT), "INIT LVL" }, { OP(ENVL), "ENV LVL" }, { OP(FEEDBACK), "FEEDBK" }, { OP(KEYTRK), "KEYTRK" } },
    { { OP(ATTACK), "ATTACK" }, { OP(DECAY), "DECAY" }, { OP(SUSTAIN), "SUSTAIN" }, { OP(RELEASE), "RELEASE" },
      { OP(DELAY), "DELAY" }, { OP(HOLD), "HOLD" }, { OP(SPEED), "SPEED" }, { SLOT_OPBPM, "BPM SYNC" } },
    { { OP(ACURVE), "ATK CRV" }, { OP(DCURVE), "DEC CRV" }, { OP(QUANT), "QUANTIZE" }, { OP(RCURVE), "REL CRV" },
      { OP(LEGATO), "LEGATO" }, { OP(RESET), "RESET" }, { OP(FREERUN), "FREERUN" }, { OP(LOOP), "ENV LOOP" } },
    { { SLOT_DEAD, "TRIG 1" }, { SLOT_DEAD, "TRIG 2" }, { SLOT_DEAD, "TRIG 3" }, { SLOT_DEAD, "TRIG 4" },
      { SLOT_DEAD, "VEL CRV" }, { OP(VELENV), "VEL>ENV" }, { OP(STAGELOOP), "STG LOOP" }, { SLOT_DEAD, "TAP TRIG" } },
    { { OP(PMODE), "PITCH MD" }, { OP(DIRECT), "DIRECT" }, { OP(PHASE), "PHASE" }, { SLOT_DEAD, "KEYSCALE" },
      { OP(INVERT), "INVERT" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" } },
};
#undef OP
/* ENV 1-5 pages (P5, pp. 71-75). BPM sync waits for the clock (P8),
 * velocity for P9, tap trigger for the button model. */
#define ME(p) SLOT_ME(RI_LEVI_OP_##p)
static const struct LeviSlot P_ENV[4][8] = {
    { { ME(ATTACK), "ATTACK" }, { ME(DECAY), "DECAY" }, { ME(SUSTAIN), "SUSTAIN" }, { ME(RELEASE), "RELEASE" },
      { ME(DELAY), "DELAY" }, { ME(HOLD), "HOLD" }, { ME(SPEED), "SPEED" }, { SLOT_MEBPM, "BPM SYNC" } },
    { { ME(ACURVE), "ATK CRV" }, { ME(DCURVE), "DEC CRV" }, { ME(QUANT), "QUANTIZE" }, { ME(RCURVE), "REL CRV" },
      { ME(LEGATO), "LEGATO" }, { ME(RESET), "RESET" }, { ME(FREERUN), "FREERUN" }, { ME(LOOP), "ENV LOOP" } },
    { { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" },
      { SLOT_DEAD, "VEL CRV" }, { SLOT_DEAD, "VEL>ENV" }, { ME(STAGELOOP), "STG LOOP" },
      { SLOT_ME(RI_LEVI_ME_LEVEL), "LEVEL" } },
    { { SLOT_ME(0), "TRIG 1" }, { SLOT_ME(1), "TRIG 2" }, { SLOT_ME(2), "TRIG 3" }, { SLOT_ME(3), "TRIG 4" },
      { SLOT_DEAD, "TAP TRIG" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" } }
};
#undef ME
/* Filter and VCA pages (P4, pp. 62-70; env amounts + initial level P5).
 * Velocity and PolyAT amounts wait for P9. */
static const struct LeviSlot P_DFILT[2][8] = {
    { { RI_SLEVI_DTYPE, "TYPE" }, { RI_SLEVI_DMORPH, "DRIVE" }, { RI_SLEVI_CUTOFF, "CUTOFF" },
      { RI_SLEVI_RESO, "RESO" }, { RI_SLEVI_DENV1, "ENV1 AMT" }, { SLOT_DEAD, "VEL>ENV" }, { SLOT_DEAD, "POLYAT" },
      { RI_SLEVI_DKEYTRK, "KEYTRK" } },
    { { SLOT_DEAD, "" }, { RI_SLEVI_DPOST, "DRV POS" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" },
      { RI_SLEVI_VORDER, "VOW ORDER" }, { SLOT_DEAD, "" }, { RI_SLEVI_DLFO1, "LFO1 AMT" },
      { RI_SLEVI_DLEVEL, "DFILT LVL" } }
};
static const struct LeviSlot P_AFILT[8] = {
    { RI_SLEVI_DRIVE, "PRE-DRV" }, { RI_SLEVI_ALFO2, "LFO2 AMT" }, { RI_SLEVI_CUTOFF2, "CUTOFF" },
    { RI_SLEVI_RESO2, "RESO" }, { RI_SLEVI_AENV2, "ENV2 AMT" }, { SLOT_DEAD, "VEL>ENV" },
    { SLOT_DEAD, "POLYAT" }, { RI_SLEVI_AKEYTRK, "KEYTRK" }
};
static const struct LeviSlot P_VCA[8] = {
    { RI_SLEVI_OSCLVL, "OSCS LVL" }, { RI_SLEVI_DLEVEL, "DFILT LVL" }, { RI_SLEVI_VCALVL, "VCA LVL" },
    { RI_SLEVI_PATCHLVL, "PATCH LVL" }, { RI_SLEVI_VLFO3, "LFO3 AMT" }, { SLOT_DEAD, "VEL>ENV" },
    { SLOT_DEAD, "POLYAT" }, { RI_SLEVI_VINIT, "INIT LVL" }
};
static const struct LeviSlot P_PREFX[8] = {
    { RI_SLEVI_FXPRE, "ON" }, { RI_SLEVI_PTYPE, "TYPE" }, { RI_SLEVI_PPRESET, "PRESET" }, { RI_SLEVI_PP1, "PARAM 1" },
    { RI_SLEVI_PP2, "PARAM 2" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { RI_SLEVI_PDRYWET, "DRY/WET" }
};
static const struct LeviSlot P_POSTFX[8] = {
    { RI_SLEVI_FXPOST, "ON" }, { RI_SLEVI_OTYPE, "TYPE" }, { RI_SLEVI_OPRESET, "PRESET" }, { RI_SLEVI_OP1, "PARAM 1" },
    { RI_SLEVI_OP2, "PARAM 2" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { RI_SLEVI_ODRYWET, "DRY/WET" }
};
static const struct LeviSlot P_DELAY[8] = {
    { RI_SLEVI_FXDLY, "ON" }, { RI_SLEVI_DLYTYPE, "TYPE" }, { RI_SLEVI_DLYTIME, "TIME" }, { RI_SLEVI_DLYFB, "FEEDBACK" },
    { RI_SLEVI_DLYWTONE, "WET TONE" }, { RI_SLEVI_DLYBPM, "BPM SYNC" }, { RI_SLEVI_DLYFBTONE, "FB TONE" }, { RI_SLEVI_DLYDRYWET, "DRY/WET" }
};
static const struct LeviSlot P_REVERB[8] = {
    { RI_SLEVI_FXREV, "ON" }, { RI_SLEVI_RTYPE, "TYPE" }, { RI_SLEVI_RPREDLY, "PRE-DLY" }, { RI_SLEVI_RTIME, "TIME" },
    { RI_SLEVI_RTONE, "TONE" }, { RI_SLEVI_RHIDAMP, "HI DAMP" }, { RI_SLEVI_RLODAMP, "LO DAMP" }, { RI_SLEVI_RDRYWET, "DRY/WET" }
};
static const struct LeviSlot P_REVERB2[8] = {
    { RI_SLEVI_RFREEZE, "FREEZE" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" },
    { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }
};
/* LFO 1-5 pages (P5, pp. 76-78). Steps show for the step wave or step
 * one-shot, stagger for trig sync off; BPM sync, semi lock and the step
 * editor wait for P8. */
#define LF(p) SLOT_LF(RI_LEVI_LP_##p)
static const struct LeviSlot P_LFO[2][8] = {
    { { LF(WAVE), "WAVE" }, { LF(RATE), "RATE" }, { LF(SPEED), "SPEED" }, { LF(TRIG), "TRIG SYNC" },
      { LF(DELAY), "DELAY" }, { LF(FADE), "FADE IN" }, { LF(QUANT), "QUANTIZE" }, { LF(LEVEL), "LEVEL" } },
    { { LF(STEPS), "STEPS" }, { LF(SMOOTH), "SMOOTH" }, { LF(BPM), "BPM SYNC" }, { LF(ONESHOT), "ONE-SHOT" },
      { LF(PHASE), "PHASE" }, { LF(STAGGER), "STAGGER" }, { SLOT_DEAD, "SEMI LOCK" }, { SLOT_DEAD, "STEP EDIT" } }
};
#undef LF
/* Algorithm pages (P3, pp. 58-61): mode, algorithm, morph position,
 * FM/PM, solo, mutes; the 8-slot morph list; then the custom grid, one
 * page per target column (encoder k = oscillator k). */
static const struct LeviSlot P_ALGO[5][8] = {
    { { RI_SLEVI_AMODE, "MODE" }, { RI_SLEVI_ALGO, "ALGO" }, { RI_SLEVI_MPOS, "MORPH" },
      { RI_SLEVI_MODE, "PM/FM" }, { RI_SLEVI_SOLO, "SOLO" }, { RI_SLEVI_MUTELO, "MUTE 1-7" },
      { RI_SLEVI_MUTEHI, "MUTE 8" }, { SLOT_OP(RI_LEVI_OP_DIRECT), "DIRECT" } },
    { { RI_SLEVI_SLOT0 + 0, "SLOT 1" }, { RI_SLEVI_SLOT0 + 1, "SLOT 2" }, { RI_SLEVI_SLOT0 + 2, "SLOT 3" },
      { RI_SLEVI_SLOT0 + 3, "SLOT 4" }, { RI_SLEVI_SLOT0 + 4, "SLOT 5" }, { RI_SLEVI_SLOT0 + 5, "SLOT 6" },
      { RI_SLEVI_SLOT0 + 6, "SLOT 7" }, { RI_SLEVI_SLOT0 + 7, "SLOT 8" } },
    { { 0 } }, { { 0 } }, { { 0 } }                 /* custom grid pages: see slot() */
};
static const struct LeviSlot P_ARP[8] = {
    { RI_SLEVI_ARPRATE, "DIVISION" }, { RI_SLEVI_ARPOCTMODE, "OCT MODE" }, { RI_SLEVI_ARPOCTRANGE, "OCT RANGE" }, { RI_SLEVI_ARPGATE, "GATE" },
    { RI_SLEVI_ARPMODE, "MODE" }, { RI_SLEVI_ARPLEN, "LENGTH" }, { RI_SLEVI_ARPPHRASE, "PHRASE" }, { SLOT_DEAD, "TEMPO" }
};
static const struct LeviSlot P_ARP2[8] = {
    { RI_SLEVI_ARPENTROPY, "ENTROPY" }, { RI_SLEVI_ARPSWING, "SWING" }, { RI_SLEVI_ARPRATCHET, "RATCHET" }, { RI_SLEVI_ARPCHANCE, "CHANCE" },
    { RI_SLEVI_ARPLATCH, "LATCH" }, { RI_SLEVI_ARPCLOCK, "CLK LOCK" }, { RI_SLEVI_ARPSTEPPOFF, "STEP OFF" }, { SLOT_DEAD, "" }
};
static const struct LeviSlot P_SEQ[8] = {
    { RI_SLEVI_SEQLEN, "LENGTH" }, { RI_SLEVI_SEQRATE, "RATE" }, { RI_SLEVI_SEQMODE, "MODE" }, { RI_SLEVI_SEQSWING, "SWING" },
    { RI_SLEVI_SEQGATE, "GATE" }, { RI_SLEVI_SEQPROB, "PROB" }, { RI_SLEVI_SEQDRIFT, "DRIFT" }, { RI_SLEVI_SEQTRANSP, "TRANSPOSE" }
};
static const struct LeviSlot P_SEQ2[8] = {
    { RI_SLEVI_SEQTRKLEN, "TRK LEN" }, { RI_SLEVI_SEQREC, "REC" }, { RI_SLEVI_SEQSTEP, "STEP" }, { RI_SLEVI_SEQCLEAR, "CLEAR" },
    { RI_SLEVI_SEQSTRIG, "ST TRIG" }, { RI_SLEVI_SEQSPROB, "ST PROB" }, { RI_SLEVI_SEQSDRIFT, "ST DRIFT" }, { RI_SLEVI_SEQSENTR, "ST ENTR" }
};
static const struct LeviSlot P_RIBBON[8] = {
    { RI_SLEVI_RBNMODE, "MODE" }, { RI_SLEVI_RBNPOS, "POS" }, { RI_SLEVI_RBNTOUCH, "TOUCH" }, { SLOT_DEAD, "" },
    { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }
};
static const struct LeviSlot P_VOICE[8] = {
    { RI_SLEVI_POLYMODE, "POLYPHONY" }, { RI_SLEVI_UDENSITY, "DENSITY" }, { RI_SLEVI_ULIMIT, "LIMIT" }, { RI_SLEVI_VDETUNE, "DETUNE" },
    { RI_SLEVI_VAFEEL, "ANALOG FL" }, { RI_SLEVI_VRNDPH, "RND PHASE" }, { RI_SLEVI_VPAN, "PAN" }, { RI_SLEVI_VWIDTH, "WIDTH" }
};
static const struct LeviSlot P_VOICE2[8] = {
    { RI_SLEVI_VPANMODE, "PAN MODE" }, { RI_SLEVI_VBENDRNG, "BEND RNG" }, { RI_SLEVI_VVIBRATE, "VIB RATE" }, { RI_SLEVI_VVIBAMT, "VIB AMT" },
    { RI_SLEVI_VVIBDLY, "VIB DLY" }, { RI_SLEVI_VGLIDE, "GLIDE" }, { RI_SLEVI_VGLTIME, "GL TIME" }, { RI_SLEVI_VGLCURVE, "GL CURVE" }
};
static const struct LeviSlot P_VOICE3[8] = {
    { RI_SLEVI_VOSCPAN1 + 0u, "OSCPAN 1" }, { RI_SLEVI_VOSCPAN1 + 1u, "OSCPAN 2" },
    { RI_SLEVI_VOSCPAN1 + 2u, "OSCPAN 3" }, { RI_SLEVI_VOSCPAN1 + 3u, "OSCPAN 4" },
    { RI_SLEVI_VOSCPAN1 + 4u, "OSCPAN 5" }, { RI_SLEVI_VOSCPAN1 + 5u, "OSCPAN 6" },
    { RI_SLEVI_VOSCPAN1 + 6u, "OSCPAN 7" }, { RI_SLEVI_VOSCPAN1 + 7u, "OSCPAN 8" }
};
static const struct LeviSlot P_VOICE4[8] = {
    { RI_SLEVI_VINTAGE, "VINTAGE" }, { RI_SLEVI_VSCALE, "SCALE" }, { RI_SLEVI_VMICRO, "MICRO" }, { RI_SLEVI_VKEYLOCK, "KEY LOCK" },
    { RI_SLEVI_VSPREAD, "SPREAD" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }
};
/* Oscillator Group Edit keys -> the per-op param they show. */
static const uint8_t GROUP_PARAM[12] = {
    RI_LEVI_OP_MODE, RI_LEVI_OP_WAVE, RI_LEVI_OP_COARSE, RI_LEVI_OP_FINE, RI_LEVI_OP_FEEDBACK, RI_LEVI_OP_INIT,
    RI_LEVI_OP_DELAY, RI_LEVI_OP_ATTACK, RI_LEVI_OP_HOLD, RI_LEVI_OP_DECAY, RI_LEVI_OP_SUSTAIN, RI_LEVI_OP_RELEASE
};

static uint32_t module(const struct RISectLevi *s) {
    int m = s->val[RI_SLEVI_MODULE];
    return (m < 0 || m >= (int)RI_SLEVI_NMOD) ? RI_SLEVI_M_OSC : (uint32_t)m;
}

uint32_t ri_slevi_page_count(const struct RISectLevi *s) {
    if (!s)
        return 1u;
    return (module(s) == RI_SLEVI_M_OSC || module(s) == RI_SLEVI_M_ALGO) ? 5u
        : (module(s) >= RI_SLEVI_M_ENV1 && module(s) < RI_SLEVI_M_ENV1 + 5u) ? 4u
        : module(s) == RI_SLEVI_M_DFILT || (module(s) >= RI_SLEVI_M_LFO1 && module(s) < RI_SLEVI_M_LFO1 + 5u) ? 2u
        : module(s) == RI_SLEVI_M_VOICE ? 4u
        : module(s) == RI_SLEVI_M_REVERB ? 2u
        : module(s) == RI_SLEVI_M_ARP ? 2u
        : module(s) == RI_SLEVI_M_SEQ ? 2u
        : module(s) == RI_SLEVI_M_MATRIX ? 16u : module(s) == RI_SLEVI_M_MACRO ? 34u : 1u;
}

static int dtype_morphs(const struct RISectLevi *s) {
    int t = s->val[RI_SLEVI_DTYPE];
    return t == (int)RI_LEVI_DF_SVF_LBH || t == (int)RI_LEVI_DF_SVF_LNH || t == (int)RI_LEVI_DF_VOWEL;
}

static int slot(const struct RISectLevi *s, uint32_t k, const char **name) {
    const struct LeviSlot *p;
    uint32_t m = module(s);
    if (k >= RI_SLEVI_NENC) {
        if (name)
            *name = "";
        return SLOT_DEAD;
    }
    if (m >= RI_SLEVI_M_GMODE && m <= RI_SLEVI_M_GRELEASE) {
        if (name)
            *name = OSC_NAME[k];
        return SLOT_GOP(GROUP_PARAM[m - RI_SLEVI_M_GMODE]);
    }
    if (m == RI_SLEVI_M_MATRIX) {                   /* 2 routes a page: source, module, param, depth */
        static const char *const F[4] = { "SOURCE", "MODULE", "PARAM", "DEPTH" };
        if (name)
            *name = F[k & 3u];
        return SLOT_MX((s->page % 16u) * 2u + k / 4u, k & 3u);
    }
    if (m == RI_SLEVI_M_MACRO) {                    /* knobs, buttons, then 4 route pages a macro */
        static const char *const KN[8] = { "MACRO 1", "MACRO 2", "MACRO 3", "MACRO 4", "MACRO 5", "MACRO 6",
            "MACRO 7", "MACRO 8" };
        static const char *const BT[8] = { "BUTTON 1", "BUTTON 2", "BUTTON 3", "BUTTON 4", "BUTTON 5",
            "BUTTON 6", "BUTTON 7", "BUTTON 8" };
        static const char *const F[4] = { "MODULE", "PARAM", "DEPTH", "BTN VAL" };
        uint32_t pg = s->page < 34u ? s->page : 0u;
        if (pg == 0u) {
            if (name)
                *name = KN[k];
            return (int)(RI_SLEVI_MKNOB0 + k);
        }
        if (pg == 1u) {
            if (name)
                *name = BT[k];
            return (int)(RI_SLEVI_MBTN0 + k);
        }
        if (name)
            *name = F[k & 3u];
        return SLOT_MR((pg - 2u) / 4u, ((pg - 2u) % 4u) * 2u + k / 4u, k & 3u);
    }
    if (m == RI_SLEVI_M_ALGO && s->page >= 2u && s->page < 5u) {
        if (name)
            *name = OSC_NAME[k];
        return SLOT_GOP(RI_LEVI_OP_TGT1 + (uint32_t)(s->page - 2u));
    }
    p = m == RI_SLEVI_M_OSC ? P_OSC[s->page < 5u ? s->page : 0u]
        : (m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + 5u) ? P_ENV[s->page < 4u ? s->page : 0u]
        : m == RI_SLEVI_M_DFILT ? P_DFILT[s->page == 1u ? 1u : 0u] : m == RI_SLEVI_M_AFILT ? P_AFILT
        : m == RI_SLEVI_M_VCA ? P_VCA : m == RI_SLEVI_M_PREFX ? P_PREFX
        : m == RI_SLEVI_M_DELAY ? P_DELAY
        : m == RI_SLEVI_M_REVERB ? (s->page == 1u ? P_REVERB2 : P_REVERB)
        : m == RI_SLEVI_M_POSTFX ? P_POSTFX
        : (m >= RI_SLEVI_M_LFO1 && m < RI_SLEVI_M_LFO1 + 5u) ? P_LFO[s->page == 1u ? 1u : 0u]
        : m == RI_SLEVI_M_ALGO ? P_ALGO[s->page < 2u ? s->page : 0u] : m == RI_SLEVI_M_ARP ? (s->page == 1u ? P_ARP2 : P_ARP)
        : m == RI_SLEVI_M_SEQ ? (s->page == 1u ? P_SEQ2 : P_SEQ)
        : m == RI_SLEVI_M_RIBBON ? P_RIBBON
        : m == RI_SLEVI_M_VOICE
        ? (s->page == 3u ? P_VOICE4 : s->page == 2u ? P_VOICE3 : s->page == 1u ? P_VOICE2 : P_VOICE)
        : P_VOICE;
    if (name) {
        *name = p[k].name;
        if (p[k].idx == SLOT_OP(RI_LEVI_OP_COARSE))  /* label follows the pitch mode */
            *name = s->opv[s->opsel][RI_LEVI_OP_PMODE] == 0u ? "SEMI"
                : s->opv[s->opsel][RI_LEVI_OP_PMODE] == 2u ? "FREQ" : "RATIO";
        if (p[k].idx == SLOT_OP(RI_LEVI_OP_FINE))
            *name = s->opv[s->opsel][RI_LEVI_OP_PMODE] == 0u ? "CENT" : "FINE";
        if (p[k].idx == (int)RI_SLEVI_DMORPH)      /* p. 62: morph for SVF & vowel */
            *name = dtype_morphs(s) ? "MORPH" : "DRIVE";
        if (p[k].idx == (int)RI_SLEVI_CUTOFF && s->val[RI_SLEVI_DTYPE] == (int16_t)RI_LEVI_DF_VOWEL)
            *name = "VOWEL";
        if ((p[k].idx == (int)RI_SLEVI_VORDER && s->val[RI_SLEVI_DTYPE] != (int16_t)RI_LEVI_DF_VOWEL) ||
            (p[k].idx == (int)RI_SLEVI_DPOST && dtype_morphs(s)))
            *name = "";
    }
    /* Vowel order shows only on the vowel model, drive position only
     * where Drive exists (p. 65). */
    if (p[k].idx == (int)RI_SLEVI_VORDER && s->val[RI_SLEVI_DTYPE] != (int16_t)RI_LEVI_DF_VOWEL)
        return SLOT_DEAD;
    if (p[k].idx == (int)RI_SLEVI_DPOST && dtype_morphs(s))
        return SLOT_DEAD;
    if (SLOT_IS_LF(p[k].idx)) {                     /* p. 77: conditional LFO params */
        const uint8_t *u = s->lfv[(m - RI_SLEVI_M_LFO1) % RI_LEVI_NLFO];
        if ((p[k].idx == SLOT_LF(RI_LEVI_LP_STEPS) && u[RI_LEVI_LP_WAVE] != RI_LEVI_LW_STEP &&
                u[RI_LEVI_LP_ONESHOT] != 2u) ||
            (p[k].idx == SLOT_LF(RI_LEVI_LP_STAGGER) && u[RI_LEVI_LP_TRIG] != 2u)) {
            if (name)
                *name = "";
            return SLOT_DEAD;
        }
    }
    return p[k].idx;
}

static uint32_t slot_op(const struct RISectLevi *s, int t, uint32_t k) {
    return SLOT_IS_GOP(t) ? (k & 7u) : (uint32_t)(s->opsel & 7u);
}

/* ENV / LFO slot t of the current module: its value, range, key and
 * default (P5). NULL when t is not such a slot. */
static uint8_t *mod_val(const struct RISectLevi *s, int t, int *lo, int *hi, uint16_t *key, int *dv) {
    uint32_t m = module(s), u, p = SLOT_MPARAM(t);
    int l0 = 0, h0 = 0;
    if (SLOT_IS_ME(t) && m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + RI_LEVI_NMENV) {
        u = m - RI_SLEVI_M_ENV1;
        (void)ri_levi_menv_range(p, &l0, &h0);
        if (key)
            *key = RI_LEVI_MEKEY(u, p);
        if (dv)
            *dv = ri_levi_menv_default(u, p);
        if (lo) { *lo = l0; *hi = h0; }
        return (uint8_t *)&s->mev[u][p];
    }
    if (t == SLOT_MEBPM && m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + RI_LEVI_NMENV) {
        /* Per-menv BPM flag (fidelity P8a): switch 0/1 on the 0x0E key. */
        u = m - RI_SLEVI_M_ENV1;
        if (key)
            *key = (uint16_t)(RI_CTL_LEVI_MEBPM0 + u);
        if (dv)
            *dv = 0;
        if (lo) { *lo = 0; *hi = 1; }
        return (uint8_t *)&s->mebpm[u];
    }
    if (SLOT_IS_MX(t)) {
        uint32_t r = (uint32_t)(-t - 160) / 4u, f = (uint32_t)(-t - 160) % 4u;
        l0 = 0;
        h0 = f == 0u ? (int)RI_LEVI_MS_UI_N - 1 : f == 1u ? (int)RI_LEVI_DM_N - 1
            : f == 2u ? (int)(ri_levi_dm_nparam(s->mxv[r][1]) ? ri_levi_dm_nparam(s->mxv[r][1]) - 1u : 0u) : 127;
        if (key)
            *key = RI_LEVI_MXKEY(r, f);
        if (dv)
            *dv = f == 3u ? 64 : 0;
        if (lo) { *lo = l0; *hi = h0; }
        return (uint8_t *)&s->mxv[r][f];
    }
    if (SLOT_IS_MR(t)) {
        uint32_t q = (uint32_t)(-t - 288), mi = q / 32u, r = (q / 4u) % 8u, f = q % 4u;
        l0 = 0;
        h0 = f == 0u ? (int)RI_LEVI_DM_N - 1
            : f == 1u ? (int)(ri_levi_dm_nparam(s->mrv[mi][r][0]) ? ri_levi_dm_nparam(s->mrv[mi][r][0]) - 1u : 0u) : 127;
        if (key)
            *key = RI_LEVI_MRKEY(mi, r, f);
        if (dv)
            *dv = f == 2u ? 64 : 0;
        if (lo) { *lo = l0; *hi = h0; }
        return (uint8_t *)&s->mrv[mi][r][f];
    }
    if (SLOT_IS_LF(t) && m >= RI_SLEVI_M_LFO1 && m < RI_SLEVI_M_LFO1 + RI_LEVI_NLFO) {
        u = m - RI_SLEVI_M_LFO1;
        (void)ri_levi_lfo_range(p, &l0, &h0);
        if (key)
            *key = RI_LEVI_LFOKEY(u, p);
        if (dv)
            *dv = ri_levi_lfo_default(p);
        if (lo) { *lo = l0; *hi = h0; }
        return (uint8_t *)&s->lfv[u][p];
    }
    return 0;
}

static int enc_of(uint32_t idx) {
    return idx >= RI_SLEVI_ENC0 && idx < RI_SLEVI_ENC0 + RI_SLEVI_NENC ? (int)(idx - RI_SLEVI_ENC0) : -1;
}

/* Target value scaled to 0..127 (encoder position / LED ring). */
static int enc_value(const struct RISectLevi *s, uint32_t k) {
    int t = slot(s, k, 0), lo, hi, v;
    const struct RICtlDef *d;
    const uint8_t *mv = mod_val(s, t, &lo, &hi, 0, 0);
    if (mv)
        return hi > lo ? ((int)*mv - lo) * 127 / (hi - lo) : 0;
    if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
        uint32_t o = slot_op(s, t, k), p = SLOT_PARAM(t);
        if (p == 32u)   /* per-op ENV BPM flag: switch truth as-is */
            return s->opbpm[o] ? 127 : 0;
        if (ri_levi_op_range(p, &lo, &hi) != 0 || hi <= lo)
            return 0;
        return ((int)s->opv[o][p] - lo) * 127 / (hi - lo);
    }
    if (t < 0)
        return 0;
    d = def(s, (uint32_t)t);
    if (!d)
        return 0;
    lo = d->min_v;
    hi = t == (int)RI_SLEVI_FTYPE ? 3 : d->max_v;
    v = s->val[t];
    return hi > lo ? (v - lo) * 127 / (hi - lo) : 0;
}

int ri_slevi_value(const struct RISectLevi *s, uint32_t idx) {
    int k;
    if (!s || idx >= RI_SLEVI_NCTL)
        return 0;
    k = enc_of(idx);
    if (k >= 0)
        return enc_value(s, (uint32_t)k);
    if (idx == RI_SLEVI_PAGE)
        return (int)module(s) * 8 + (int)s->page;
    if (idx == RI_SLEVI_OPSEL)
        return (int)s->opsel;
    return s->val[idx];
}

uint32_t ri_slevi_ctl_idx(const struct RISectLevi *s, uint32_t idx) {
    int k, t;
    if (!s)
        return idx;
    k = enc_of(idx);
    if (k < 0)
        return idx;
    t = slot(s, (uint32_t)k, 0);
    return t >= 0 ? (uint32_t)t : idx;
}

int ri_slevi_ctl_key(const struct RISectLevi *s, uint32_t idx, uint16_t *key, int *val) {
    int k, t;
    uint32_t o, p;
    if (!s || !key || !val)
        return 0;
    k = enc_of(idx);
    if (k < 0)
        return 0;
    t = slot(s, (uint32_t)k, 0);
    {
        const uint8_t *mv = mod_val(s, t, 0, 0, key, 0);
        if (mv) {
            *val = *mv;
            return 1;
        }
    }
    if (!SLOT_IS_OP(t) && !SLOT_IS_GOP(t))
        return 0;
    o = slot_op(s, t, (uint32_t)k);
    p = SLOT_PARAM(t);
    if (p == 32u) {   /* per-op ENV BPM flag (fidelity P8a) */
        *key = (uint16_t)(RI_CTL_LEVI_OPBPM0 + o);
        *val = s->opbpm[o];
        return 1;
    }
    *key = RI_LEVI_OPKEY(o, p);
    *val = s->opv[o][p];
    return 1;
}

int ri_slevi_page_reaches(uint32_t idx) {
    static const uint8_t DT[2] = { RI_LEVI_DF_LP_12, RI_LEVI_DF_VOWEL };   /* model-dependent slots */
    struct RISectLevi t;
    uint32_t m, k, pg, d;
    ri_slevi_init(&t);
    for (d = 0u; d < 2u; d++)
    for (m = 0u; m < RI_SLEVI_NMOD; m++) {
        t.val[RI_SLEVI_DTYPE] = (int16_t)DT[d];
        t.val[RI_SLEVI_MODULE] = (int16_t)m;
        for (pg = 0u; pg < 5u; pg++) {
            t.page = (uint8_t)pg;
            for (k = 0u; k < RI_SLEVI_NENC; k++) {
                int x = slot(&t, k, 0);
                if (x >= 0 && (uint32_t)x == idx)
                    return 1;
            }
            if (ri_slevi_page_count(&t) <= pg + 1u)
                break;
        }
    }
    return 0;
}

int ri_slevi_legacy(uint32_t idx) {
    /* v1 two-algorithm morph (ALGOB/MORPH) gives way to the P3 slot list. */
    return idx == RI_SLEVI_RATIO || idx == RI_SLEVI_OPMODE || (idx >= RI_SLEVI_ATTACK && idx <= RI_SLEVI_LOOP) ||
        idx == RI_SLEVI_ALGOB || idx == RI_SLEVI_MORPH || idx == RI_SLEVI_FTYPE ||
        (idx >= RI_SLEVI_ROUTE0 && idx < RI_SLEVI_ROUTE0 + 8u);
}

int ri_slevi_enc_live(const struct RISectLevi *s, uint32_t k) {
    return s && k < RI_SLEVI_NENC && slot(s, k, 0) != SLOT_DEAD;
}

const char *ri_slevi_page_title(const struct RISectLevi *s) {
    static const char *const T[RI_SLEVI_NMOD] = {
        "OSC", "GROUP: MODE", "GROUP: WAVE", "GROUP: PITCH", "GROUP: FINE", "GROUP: FEEDBACK",
        "GROUP: LEVEL", "GROUP: DELAY", "GROUP: ATTACK", "GROUP: HOLD", "GROUP: DECAY",
        "GROUP: SUSTAIN", "GROUP: RELEASE", "ENV 1", "ENV 2", "ENV 3", "ENV 4", "ENV 5",
        "DIGITAL FILTER", "ANALOG FILTER", "VCA", "PRE-FX", "DELAY", "REVERB", "POST-FX",
        "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "ALGORITHM", "ARPEGGIATOR", "SEQUENCER",
        "MOD MATRIX", "VOICE", "MACRO ASSIGN", "RIBBON"
    };
    static const char *const OSC_PG[RI_LEVI_NOPS][5] = {
        { "OSC 1  1/5", "OSC 1  2/5", "OSC 1  3/5", "OSC 1  4/5", "OSC 1  5/5" },
        { "OSC 2  1/5", "OSC 2  2/5", "OSC 2  3/5", "OSC 2  4/5", "OSC 2  5/5" },
        { "OSC 3  1/5", "OSC 3  2/5", "OSC 3  3/5", "OSC 3  4/5", "OSC 3  5/5" },
        { "OSC 4  1/5", "OSC 4  2/5", "OSC 4  3/5", "OSC 4  4/5", "OSC 4  5/5" },
        { "OSC 5  1/5", "OSC 5  2/5", "OSC 5  3/5", "OSC 5  4/5", "OSC 5  5/5" },
        { "OSC 6  1/5", "OSC 6  2/5", "OSC 6  3/5", "OSC 6  4/5", "OSC 6  5/5" },
        { "OSC 7  1/5", "OSC 7  2/5", "OSC 7  3/5", "OSC 7  4/5", "OSC 7  5/5" },
        { "OSC 8  1/5", "OSC 8  2/5", "OSC 8  3/5", "OSC 8  4/5", "OSC 8  5/5" },
    };
    uint32_t m;
    if (!s)
        return "";
    m = module(s);
    static const char *const ALGO_PG[5] = { "ALGORITHM  1/5", "MORPH SLOTS  2/5", "CUSTOM TGT 1  3/5",
        "CUSTOM TGT 2  4/5", "CUSTOM TGT 3  5/5" };
    if (m == RI_SLEVI_M_ALGO)
        return ALGO_PG[s->page < 5u ? s->page : 0u];
    if (m == RI_SLEVI_M_DFILT)
        return s->page == 1u ? "DIGITAL FILTER  2/2" : "DIGITAL FILTER  1/2";
    if (m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + 5u) {
        static const char *const EP[5][4] = {
            { "ENV 1  1/4", "ENV 1  2/4", "ENV 1  3/4", "ENV 1  4/4" },
            { "ENV 2  1/4", "ENV 2  2/4", "ENV 2  3/4", "ENV 2  4/4" },
            { "ENV 3  1/4", "ENV 3  2/4", "ENV 3  3/4", "ENV 3  4/4" },
            { "ENV 4  1/4", "ENV 4  2/4", "ENV 4  3/4", "ENV 4  4/4" },
            { "ENV 5  1/4", "ENV 5  2/4", "ENV 5  3/4", "ENV 5  4/4" }
        };
        return EP[m - RI_SLEVI_M_ENV1][s->page < 4u ? s->page : 0u];
    }
    if (m == RI_SLEVI_M_MATRIX || m == RI_SLEVI_M_MACRO) {
        /* "MATRIX 3|4  2/16", "MACRO 2 R1|2  7/34" (UI thread only). */
        static char tb[32];
        uint32_t pg = s->page;
        tb[0] = 0;
        if (m == RI_SLEVI_M_MATRIX) {
            put_str(tb, sizeof tb, "MATRIX ");
            cat_num(tb, sizeof tb, (int)(pg % 16u) * 2 + 1);
            cat_str(tb, sizeof tb, "|");
            cat_num(tb, sizeof tb, (int)(pg % 16u) * 2 + 2);
            cat_str(tb, sizeof tb, "  ");
            cat_num(tb, sizeof tb, (int)(pg % 16u) + 1);
            cat_str(tb, sizeof tb, "/16");
        } else {
            put_str(tb, sizeof tb, pg == 0u ? "MACRO KNOBS" : pg == 1u ? "MACRO BUTTONS" : "MACRO ");
            if (pg >= 2u && pg < 34u) {
                cat_num(tb, sizeof tb, (int)(pg - 2u) / 4 + 1);
                cat_str(tb, sizeof tb, " R");
                cat_num(tb, sizeof tb, (int)((pg - 2u) % 4u) * 2 + 1);
                cat_str(tb, sizeof tb, "|");
                cat_num(tb, sizeof tb, (int)((pg - 2u) % 4u) * 2 + 2);
            }
            cat_str(tb, sizeof tb, "  ");
            cat_num(tb, sizeof tb, (int)(pg < 34u ? pg : 0u) + 1);
            cat_str(tb, sizeof tb, "/34");
        }
        return tb;
    }
    if (m >= RI_SLEVI_M_LFO1 && m < RI_SLEVI_M_LFO1 + 5u) {
        static const char *const LP[5][2] = {
            { "LFO 1  1/2", "LFO 1  2/2" }, { "LFO 2  1/2", "LFO 2  2/2" }, { "LFO 3  1/2", "LFO 3  2/2" },
            { "LFO 4  1/2", "LFO 4  2/2" }, { "LFO 5  1/2", "LFO 5  2/2" }
        };
        return LP[m - RI_SLEVI_M_LFO1][s->page == 1u ? 1u : 0u];
    }
    return m == RI_SLEVI_M_OSC ? OSC_PG[s->opsel & 7u][s->page < 5u ? s->page : 0u] : T[m];
}

const char *ri_slevi_enc_name(const struct RISectLevi *s, uint32_t k) {
    const char *n = "";
    if (!s)
        return "";
    (void)slot(s, k, &n);
    return n ? n : "";
}

static void put_num(char *buf, uint32_t cap, int v) {
    char t[12];
    int n = 0, neg = v < 0;
    uint32_t i = 0u, u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + (int)(u % 10u));
        u /= 10u;
    } while (u && n < 10);
    if (neg && i + 1u < cap)
        buf[i++] = '-';
    while (n && i + 1u < cap)
        buf[i++] = t[--n];
    buf[i] = 0;
}

static void put_str(char *buf, uint32_t cap, const char *src) {
    uint32_t i = 0u;
    while (src[i] && i + 1u < cap) {
        buf[i] = src[i];
        i++;
    }
    buf[i] = 0;
}

static void cat_str(char *buf, uint32_t cap, const char *src) {
    uint32_t i = 0u;
    while (buf[i] && i + 1u < cap)
        i++;
    put_str(buf + i, cap - i, src);
}

static void cat_num(char *buf, uint32_t cap, int v) {
    char t[12];
    put_num(t, sizeof t, v);
    cat_str(buf, cap, t);
}

/* v in hundredths -> "12.34" (cap-safe). */
static void put_fix2(char *buf, uint32_t cap, int v) {
    char t[4];
    int neg = v < 0, a = neg ? -v : v;
    buf[0] = 0;
    if (neg)
        cat_str(buf, cap, "-");
    cat_num(buf, cap, a / 100);
    t[0] = '.';
    t[1] = (char)('0' + (a / 10) % 10);
    t[2] = (char)('0' + a % 10);
    t[3] = 0;
    cat_str(buf, cap, t);
}

static void time_text(char *buf, uint32_t cap, float t) {
    if (t < 1.0f) {
        put_num(buf, cap, (int)(t * 1000.0f + 0.5f));
        cat_str(buf, cap, "MS");
    } else {
        put_fix2(buf, cap, (int)(t * 100.0f + 0.5f));
        cat_str(buf, cap, "S");
    }
}

/* Per-oscillator value text (manual units where they exist). */
static void op_text(const uint8_t *u, uint32_t p, char *buf, uint32_t cap) {
    static const char *const MODES[7] = { "FREQ MOD", "PHASE MOD", "PW MOD", "HTE SYNC", "PD SAW",
        "PD SQUARE", "PD SAW PLS" };
    static const char *const PMODES[3] = { "SEMITONE", "RATIO", "FREQUENCY" };
    static const char *const STAGES[3] = { "DLY>ATK", "DLY>HOLD", "DLY>DEC" };
    int v = u[p], pm = u[RI_LEVI_OP_PMODE];
    buf[0] = 0;
    switch (p) {
    case RI_LEVI_OP_MODE: put_str(buf, cap, MODES[v % 7]); return;
    case RI_LEVI_OP_WAVE:
        put_str(buf, cap, ri_levi_wave_name((uint32_t)v));
        if (v >= 16) {
            cat_str(buf, cap, " ");
            cat_num(buf, cap, (v & 15) + 1);
        }
        return;
    case RI_LEVI_OP_PMODE: put_str(buf, cap, PMODES[v > 2 ? 2 : v]); return;
    case RI_LEVI_OP_COARSE:
        if (pm == 0) {
            int semi = v - 64 < -36 ? -36 : v - 64 > 36 ? 36 : v - 64;
            if (semi > 0)
                put_str(buf, cap, "+");
            cat_num(buf, cap, semi);
        } else if (pm == 1) {
            put_fix2(buf, cap, (int)(ri_levi_ratio((uint8_t)v) * 100.0f + 0.5f));
        } else {
            float c = (float)v / 127.0f;
            put_num(buf, cap, (int)(10000.0f * c * c * c + 0.5f));
            cat_str(buf, cap, "HZ");
        }
        return;
    case RI_LEVI_OP_FINE:
        if (pm == 2) {
            put_fix2(buf, cap, v * 99 / 127);
        } else {
            int f = v - 64;
            if (pm == 0 && f > 50)
                f = 50;
            if (pm == 0 && f < -50)
                f = -50;
            if (pm == 1)
                f = f < 0 ? f * 50 / 64 : f * 100 / 63;
            if (f > 0)
                put_str(buf, cap, "+");
            cat_num(buf, cap, f);
        }
        return;
    case RI_LEVI_OP_INIT: put_num(buf, cap, v * 128 / 127); return;
    case RI_LEVI_OP_ENVL:
        if (v >= 127) {
            put_str(buf, cap, "+128");
            return;
        }
        if (v > 64)
            put_str(buf, cap, "+");
        cat_num(buf, cap, (v - 64) * 2);
        return;
    case RI_LEVI_OP_FEEDBACK: put_num(buf, cap, v * 100 / 127); cat_str(buf, cap, "%"); return;
    case RI_LEVI_OP_KEYTRK: put_num(buf, cap, (v - 64) * 100 / 32); cat_str(buf, cap, "%"); return;
    case RI_LEVI_OP_PHASE: put_num(buf, cap, v * 360 / 128); return;
    case RI_LEVI_OP_DELAY: case RI_LEVI_OP_ATTACK: case RI_LEVI_OP_HOLD:
    case RI_LEVI_OP_DECAY: case RI_LEVI_OP_RELEASE:
        time_text(buf, cap, ri_levi_env_time(p, (uint8_t)v, u[RI_LEVI_OP_SPEED] ? 1u : 0u));
        return;
    case RI_LEVI_OP_SUSTAIN: put_num(buf, cap, v * 128 / 127); return;
    case RI_LEVI_OP_SPEED: put_str(buf, cap, v ? "SLOW" : "FAST"); return;
    case RI_LEVI_OP_ACURVE: case RI_LEVI_OP_DCURVE: case RI_LEVI_OP_RCURVE:
        if (v == 64) {
            put_str(buf, cap, "LIN");
            return;
        }
        if (v > 64)
            put_str(buf, cap, "+");
        cat_num(buf, cap, v - 64);
        return;
    case RI_LEVI_OP_QUANT: if (!v) put_str(buf, cap, "OFF"); else put_num(buf, cap, v); return;
    case RI_LEVI_OP_LOOP:
        if (!v)
            put_str(buf, cap, "OFF");
        else if (v >= 50)
            put_str(buf, cap, "INF");
        else
            put_num(buf, cap, v + 1);
        return;
    case RI_LEVI_OP_STAGELOOP: put_str(buf, cap, STAGES[v > 2 ? 2 : v]); return;
    case RI_LEVI_OP_VELENV: put_num(buf, cap, v - 64); return;
    case RI_LEVI_OP_TGT1: case RI_LEVI_OP_TGT2: case RI_LEVI_OP_TGT3:
        if (!v) {
            put_str(buf, cap, "---");
            return;
        }
        put_str(buf, cap, "OSC ");
        cat_num(buf, cap, v);
        return;
    default: put_str(buf, cap, v ? "ON" : "OFF"); return;
    }
}

/* ENV / LFO value text (P5; manual units where they exist). */
static void mod_text(const struct RISectLevi *s, int t, char *buf, uint32_t cap) {
    static const char *const TS[RI_LEVI_TS_N] = { "OFF", "NOTE ON", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5",
        "RBN ON", "RBN REL", "SUSPED ON" };
    static const char *const TRIG[3] = { "POLY", "SINGLE", "OFF" };
    static const char *const ONE[3] = { "OFF", "ON", "STEP" };
    const uint8_t *mv = mod_val(s, t, 0, 0, 0, 0);
    uint32_t p = SLOT_MPARAM(t);
    int v = *mv;
    buf[0] = 0;
    if (SLOT_IS_MX(t) || SLOT_IS_MR(t)) {
        uint32_t f = SLOT_IS_MX(t) ? (uint32_t)(-t - 160) % 4u : (uint32_t)(-t - 288) % 4u;
        const uint8_t *u = mv - f;                     /* the route's four fields */
        if (SLOT_IS_MX(t) && f == 0u)
            put_str(buf, cap, v ? ri_levi_ms_name(ri_levi_ms_by_ui((uint32_t)v)) : "---");
        else if ((SLOT_IS_MX(t) && f == 1u) || (SLOT_IS_MR(t) && f == 0u))
            put_str(buf, cap, ri_levi_dm_name((uint32_t)v));
        else if ((SLOT_IS_MX(t) && f == 2u) || (SLOT_IS_MR(t) && f == 1u))
            put_str(buf, cap, ri_levi_dp_name(u[SLOT_IS_MX(t) ? 1 : 0], (uint32_t)v));
        else if ((SLOT_IS_MX(t) && f == 3u) || (SLOT_IS_MR(t) && f == 2u)) {
            if (v > 64)
                put_str(buf, cap, "+");
            cat_num(buf, cap, (v - 64) * 128 / 63);   /* +/-128.0 on the manual's scale */
        } else
            put_num(buf, cap, v * 128 / 127);
        return;
    }
    if (SLOT_IS_ME(t)) {
        const uint8_t *u = mv - p;
        if (p <= 3u)
            put_str(buf, cap, TS[v < (int)RI_LEVI_TS_N ? v : 0]);
        else if (p == RI_LEVI_ME_LEVEL)
            put_num(buf, cap, v * 128 / 127);
        else
            op_text(u, p, buf, cap);
        return;
    }
    if (t == SLOT_MEBPM) {   /* per-menv BPM flag (fidelity P8a) */
        put_str(buf, cap, v ? "SYNC" : "FREE");
        return;
    }
    {
        const uint8_t *u = mv - p;
        uint32_t slow = u[RI_LEVI_LP_SPEED] ? 0u : 1u;
        switch (p) {
        case RI_LEVI_LP_WAVE: put_str(buf, cap, ri_levi_lfo_wave_name((uint32_t)v)); return;
        case RI_LEVI_LP_RATE: {
            float hz = ri_levi_lfo_hz(u[RI_LEVI_LP_SPEED], (uint8_t)v);
            if (hz < 10.0f)
                put_fix2(buf, cap, (int)(hz * 100.0f + 0.5f));
            else
                put_num(buf, cap, (int)(hz + 0.5f));
            cat_str(buf, cap, "HZ");
            return;
        }
        case RI_LEVI_LP_SPEED: put_str(buf, cap, v ? "FAST" : "SLOW"); return;
        case RI_LEVI_LP_TRIG: put_str(buf, cap, TRIG[v > 2 ? 2 : v]); return;
        case RI_LEVI_LP_DELAY: time_text(buf, cap, ri_levi_env_time(RI_LEVI_OP_DELAY, (uint8_t)v, slow)); return;
        case RI_LEVI_LP_FADE: time_text(buf, cap, ri_levi_env_time(RI_LEVI_OP_ATTACK, (uint8_t)v, slow)); return;
        case RI_LEVI_LP_QUANT: if (!v) put_str(buf, cap, "OFF"); else put_num(buf, cap, v); return;
        case RI_LEVI_LP_LEVEL: put_num(buf, cap, v * 128 / 127); return;
        case RI_LEVI_LP_ONESHOT: put_str(buf, cap, ONE[v > 2 ? 2 : v]); return;
        case RI_LEVI_LP_BPM: put_str(buf, cap, v ? "SYNC" : "FREE"); return;
        case RI_LEVI_LP_PHASE: case RI_LEVI_LP_STAGGER: put_num(buf, cap, v * 360 / 128); return;
        default: put_num(buf, cap, v); return;
        }
    }
}

void ri_slevi_enc_text(const struct RISectLevi *s, uint32_t k, char *buf, uint32_t cap) {
    static const char *const FTYPES[4] = { "LP", "HP", "BP", "NOTCH" };
    const struct RICtlDef *d;
    int t;
    if (!buf || !cap)
        return;
    buf[0] = 0;
    if (!s || k >= RI_SLEVI_NENC)
        return;
    t = slot(s, k, 0);
    if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
        if (SLOT_PARAM(t) == 32u) {   /* per-op ENV BPM flag */
            put_str(buf, cap, s->opbpm[slot_op(s, t, k)] ? "SYNC" : "FREE");
            return;
        }
        op_text(s->opv[slot_op(s, t, k)], SLOT_PARAM(t), buf, cap);
        return;
    }
    if (mod_val(s, t, 0, 0, 0, 0)) {
        mod_text(s, t, buf, cap);
        return;
    }
    if (t < 0)
        return;
    if (t == (int)RI_SLEVI_FTYPE) {
        put_str(buf, cap, FTYPES[s->val[t] & 3]);
        return;
    }
    if (t == (int)RI_SLEVI_MODE) {
        put_str(buf, cap, s->val[t] ? "PM" : "FM");
        return;
    }
    if (t == (int)RI_SLEVI_ALGO || t == (int)RI_SLEVI_ALGOB) {
        put_num(buf, cap, s->val[t] + 1);
        return;
    }
    if (t == (int)RI_SLEVI_DTYPE) {
        put_str(buf, cap, ri_levi_df_name((uint32_t)s->val[t]));
        return;
    }
    if (t == (int)RI_SLEVI_DPOST) {
        put_str(buf, cap, s->val[t] ? "POST" : "PRE");
        return;
    }
    if (t == (int)RI_SLEVI_VORDER) {                 /* the engine's own orders */
        static const char *const VO[8] = { "AEIOU", "UOIEA", "AOUEI", "IEAOU", "EIAUO", "OAEUI", "UIOAE",
            "EUAIO" };
        put_str(buf, cap, VO[s->val[t] & 7]);
        return;
    }
    if (t == (int)RI_SLEVI_DKEYTRK || t == (int)RI_SLEVI_AKEYTRK) {
        put_num(buf, cap, (s->val[t] - 64) * 100 / 32);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_DLFO1 || t == (int)RI_SLEVI_ALFO2 || t == (int)RI_SLEVI_VLFO3 ||
        t == (int)RI_SLEVI_DENV1 || t == (int)RI_SLEVI_AENV2) {
        if (s->val[t] > 64)
            put_str(buf, cap, "+");
        cat_num(buf, cap, s->val[t] - 64);
        return;
    }
    if (t == (int)RI_SLEVI_DLEVEL || t == (int)RI_SLEVI_OSCLVL || t == (int)RI_SLEVI_VCALVL ||
        t == (int)RI_SLEVI_PATCHLVL || t == (int)RI_SLEVI_VINIT) {
        put_num(buf, cap, s->val[t] * 128 / 127);   /* 64 = unity (p. 68) */
        return;
    }
    if (t == (int)RI_SLEVI_AMODE) {
        static const char *const AM[3] = { "SINGLE", "MORPH", "CUSTOM" };
        put_str(buf, cap, AM[s->val[t] > 2 ? 2 : s->val[t] < 0 ? 0 : s->val[t]]);
        return;
    }
    if (t == (int)RI_SLEVI_POLYMODE) {
        static const char *const PM[9] = { "ROTATE", "REASSIGN", "MONO", "MONO LO", "MONO HI", "UNISON",
            "UNIS LO", "UNIS HI", "UNISPLY" };
        int m = s->val[t];
        put_str(buf, cap, PM[m >= 0 && m < 9 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_UDENSITY) {
        put_num(buf, cap, s->val[t] * 8 / 127 + 1);
        return;
    }
    if (t == (int)RI_SLEVI_ULIMIT) {
        put_num(buf, cap, s->val[t] * 6 / 127 + 1);
        return;
    }
    if (t == (int)RI_SLEVI_VDETUNE) {
        put_num(buf, cap, s->val[t] * 50 / 127);
        cat_str(buf, cap, " C");
        return;
    }
    if (t == (int)RI_SLEVI_VAFEEL || t == (int)RI_SLEVI_VRNDPH || t == (int)RI_SLEVI_VWIDTH) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_VPAN) {
        int p = (s->val[t] - 64) * 100 / 63;
        if (p == 0)
            put_str(buf, cap, "CENTRE");
        else if (p < 0) {
            put_str(buf, cap, "L");
            cat_num(buf, cap, -p);
        } else {
            put_str(buf, cap, "R");
            cat_num(buf, cap, p);
        }
        return;
    }
    if (t == (int)RI_SLEVI_VPANMODE) {
        static const char *const VM[3] = { "BALANCE", "POWER", "WIDE" };
        int m = s->val[t];
        put_str(buf, cap, VM[m >= 0 && m < 3 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_VBENDRNG) {
        put_num(buf, cap, s->val[t] * 24 / 127);
        cat_str(buf, cap, "ST");
        return;
    }
    if (t == (int)RI_SLEVI_VVIBRATE) {
        float hz = 0.1f * ri_pow2((float)s->val[t] / 127.0f * 7.6439f);
        int h = (int)(hz * 10.0f + 0.5f);
        put_num(buf, cap, h / 10);
        cat_str(buf, cap, ".");
        cat_num(buf, cap, h % 10);
        cat_str(buf, cap, "HZ");
        return;
    }
    if (t == (int)RI_SLEVI_VVIBAMT) {
        put_num(buf, cap, s->val[t] * 400 / 127);
        cat_str(buf, cap, " C");
        return;
    }
    if (t == (int)RI_SLEVI_VVIBDLY || t == (int)RI_SLEVI_VGLTIME) {
        int h = t == (int)RI_SLEVI_VVIBDLY ? s->val[t] * 500 / 127   /* 0..5 s linear */
            : s->val[t] * s->val[t] * 500 / (127 * 127);             /* 0..5 s quadratic */
        put_num(buf, cap, h / 100);
        cat_str(buf, cap, ".");
        cat_num(buf, cap, (h % 100) / 10);
        cat_str(buf, cap, "S");
        return;
    }
    if (t == (int)RI_SLEVI_VGLIDE) {
        put_str(buf, cap, s->val[t] == 0 ? "OFF" : s->val[t] < 64 ? "GLIDE" : "GLISS");
        return;
    }
    if (t == (int)RI_SLEVI_VGLCURVE) {
        if (s->val[t] == 64)
            put_str(buf, cap, "LIN");
        else if (s->val[t] < 64)
            put_str(buf, cap, "FAST");
        else
            put_str(buf, cap, "SLOW");
        return;
    }
    if (t == (int)RI_SLEVI_VINTAGE) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_VSCALE) {
        int m = s->val[t];
        put_str(buf, cap, ri_levi_scale_name((uint32_t)(m < 0 ? 0 : m > 15 ? 15 : m)));
        return;
    }
    if (t == (int)RI_SLEVI_VMICRO) {
        int m = s->val[t];
        put_str(buf, cap, ri_levi_micro_name((uint32_t)(m < 0 ? 0 : m > 7 ? 7 : m)));
        return;
    }
    if (t == (int)RI_SLEVI_VKEYLOCK) {
        put_str(buf, cap, s->val[t] ? "ON" : "OFF");
        return;
    }
    if (t == (int)RI_SLEVI_VSPREAD) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t >= (int)RI_SLEVI_VOSCPAN1 && t < (int)RI_SLEVI_VOSCPAN1 + 8) {
        int p = (s->val[t] - 64) * 100 / 63;
        if (p == 0)
            put_str(buf, cap, "CENTRE");
        else if (p < 0) {
            put_str(buf, cap, "L");
            cat_num(buf, cap, -p);
        } else {
            put_str(buf, cap, "R");
            cat_num(buf, cap, p);
        }
        return;
    }
    if (t == (int)RI_SLEVI_DLYTYPE) {
        static const char *const DT[4] = { "CLEAN", "ANALOG", "TAPE", "PINGPONG" };
        int m = s->val[t];
        put_str(buf, cap, DT[m >= 0 && m < 4 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_DLYTIME) {
        float s_ = ri_levi_delay_time((uint8_t)(s->val[t] < 0 ? 0 : s->val[t] > 127 ? 127 : s->val[t]));
        if (s_ < 1.0f) {
            put_num(buf, cap, (int)(s_ * 1000.0f + 0.5f));
            cat_str(buf, cap, "MS");
        } else {
            put_num(buf, cap, (int)(s_ * 100.0f + 0.5f) / 100);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(s_ * 100.0f + 0.5f) % 100 / 10);
            cat_str(buf, cap, "S");
        }
        return;
    }
    if (t == (int)RI_SLEVI_DLYFB) {
        put_num(buf, cap, s->val[t] * 95 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_DLYWTONE || t == (int)RI_SLEVI_DLYFBTONE) {
        float hz = t == (int)RI_SLEVI_DLYWTONE
            ? 200.0f * ri_pow2((float)s->val[t] / 127.0f * 6.4919f)
            : 100.0f * ri_pow2((float)s->val[t] / 127.0f * 6.3219f);
        if (hz < 1000.0f) {
            put_num(buf, cap, (int)(hz + 0.5f));
            cat_str(buf, cap, "HZ");
        } else {
            put_num(buf, cap, (int)(hz / 100.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz / 100.0f + 0.5f) % 10);
            cat_str(buf, cap, "KHZ");
        }
        return;
    }
    if (t == (int)RI_SLEVI_DLYDRYWET) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_DLYBPM) {
        put_str(buf, cap, s->val[t] ? "SYNC" : "FREE");
        return;
    }
    if (t == (int)RI_SLEVI_RTYPE) {
        static const char *const RT[4] = { "ROOM", "HALL", "PLATE", "CHAMBER" };
        int m = s->val[t];
        put_str(buf, cap, RT[m >= 0 && m < 4 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_RPREDLY) {
        put_num(buf, cap, s->val[t] * 250 / 127);
        cat_str(buf, cap, "MS");
        return;
    }
    if (t == (int)RI_SLEVI_RTIME) {
        put_num(buf, cap, s->val[t] * 95 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_RTONE || t == (int)RI_SLEVI_RHIDAMP) {
        float hz = 200.0f * ri_pow2((float)s->val[t] / 127.0f * 6.4919f);
        if (hz < 1000.0f) {
            put_num(buf, cap, (int)(hz + 0.5f));
            cat_str(buf, cap, "HZ");
        } else {
            put_num(buf, cap, (int)(hz / 100.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz / 100.0f + 0.5f) % 10);
            cat_str(buf, cap, "KHZ");
        }
        return;
    }
    if (t == (int)RI_SLEVI_RLODAMP) {
        float hz = 20.0f * ri_pow2((float)s->val[t] / 127.0f * 4.6439f);
        if (hz < 1000.0f) {
            put_num(buf, cap, (int)(hz + 0.5f));
            cat_str(buf, cap, "HZ");
        } else {
            put_num(buf, cap, (int)(hz / 100.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz / 100.0f + 0.5f) % 10);
            cat_str(buf, cap, "KHZ");
        }
        return;
    }
    if (t == (int)RI_SLEVI_RDRYWET) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_RFREEZE) {
        put_str(buf, cap, s->val[t] ? "HELD" : "OFF");
        return;
    }
    if (t == (int)RI_SLEVI_PTYPE || t == (int)RI_SLEVI_OTYPE) {
        static const char *const MT[9] = { "CHORUS", "FLANGER", "ROTARY", "PHASER", "LO-FI",
            "TREMOLO", "EQ", "COMP", "DISTORT" };
        int m = s->val[t];
        put_str(buf, cap, MT[m >= 0 && m < 9 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_PPRESET || t == (int)RI_SLEVI_OPRESET) {
        put_str(buf, cap, "P");
        cat_num(buf, cap, (s->val[t] % 4) + 1);
        return;
    }
    if (t == (int)RI_SLEVI_PP1 || t == (int)RI_SLEVI_OP1 ||
        t == (int)RI_SLEVI_PP2 || t == (int)RI_SLEVI_OP2) {
        /* Per-type units: the TYPE row of the same slot selects the map. */
        int tyrow = (t <= (int)RI_SLEVI_PDRYWET) ? (int)RI_SLEVI_PTYPE : (int)RI_SLEVI_OTYPE;
        int ty = s->val[tyrow];
        int is_p1 = (t == (int)RI_SLEVI_PP1 || t == (int)RI_SLEVI_OP1);
        int v = s->val[t];
        if (ty < 0 || ty > 8)
            ty = 0;
        if (is_p1 && (ty == 0 || ty == 1)) {          /* chorus/flanger rate Hz */
            float hz = 0.05f + (float)v / 127.0f * 7.95f;
            put_num(buf, cap, (int)(hz * 10.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz * 10.0f + 0.5f) % 10);
            cat_str(buf, cap, "HZ");
        } else if (is_p1 && ty == 2) {               /* rotary rate Hz */
            float hz = 0.5f + (float)v / 127.0f * 7.5f;
            put_num(buf, cap, (int)(hz * 10.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz * 10.0f + 0.5f) % 10);
            cat_str(buf, cap, "HZ");
        } else if (is_p1 && ty == 3) {               /* phaser rate Hz */
            float hz = 0.05f + (float)v / 127.0f * 3.95f;
            put_num(buf, cap, (int)(hz * 10.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz * 10.0f + 0.5f) % 10);
            cat_str(buf, cap, "HZ");
        } else if (is_p1 && ty == 4) {               /* lo-fi bits */
            put_num(buf, cap, 16 - v * 12 / 127);
            cat_str(buf, cap, "BIT");
        } else if (is_p1 && ty == 5) {               /* tremolo rate Hz */
            float hz = 0.5f + (float)v / 127.0f * 14.5f;
            put_num(buf, cap, (int)(hz * 10.0f + 0.5f) / 10);
            cat_str(buf, cap, ".");
            cat_num(buf, cap, (int)(hz * 10.0f + 0.5f) % 10);
            cat_str(buf, cap, "HZ");
        } else if (is_p1 && ty == 6) {               /* EQ bass dB */
            int db = v * 24 / 127 - 12;
            if (db < 0) {
                put_str(buf, cap, "-");
                put_num(buf, cap, -db);
            } else {
                put_str(buf, cap, "+");
                put_num(buf, cap, db);
            }
            cat_str(buf, cap, "DB");
        } else if (is_p1 && ty == 7) {               /* comp threshold dB */
            put_num(buf, cap, v * 40 / 127 - 40);
            cat_str(buf, cap, "DB");
        } else if (!is_p1 && ty == 4) {              /* lo-fi decim */
            put_str(buf, cap, "X");
            cat_num(buf, cap, 1 + v * 15 / 127);
        } else if (!is_p1 && ty == 6) {              /* EQ treble dB */
            int db = v * 24 / 127 - 12;
            if (db < 0) {
                put_str(buf, cap, "-");
                put_num(buf, cap, -db);
            } else {
                put_str(buf, cap, "+");
                put_num(buf, cap, db);
            }
            cat_str(buf, cap, "DB");
        } else if (!is_p1 && ty == 7) {              /* comp ratio */
            put_num(buf, cap, 1 + v * 19 / 127);
            cat_str(buf, cap, ":1");
        } else if (!is_p1 && ty == 8) {              /* distort tone Hz */
            float hz = 500.0f + (float)v / 127.0f * 17000.0f;
            if (hz < 1000.0f) {
                put_num(buf, cap, (int)(hz + 0.5f));
                cat_str(buf, cap, "HZ");
            } else {
                put_num(buf, cap, (int)(hz / 100.0f + 0.5f) / 10);
                cat_str(buf, cap, ".");
                cat_num(buf, cap, (int)(hz / 100.0f + 0.5f) % 10);
                cat_str(buf, cap, "KHZ");
            }
        } else {                                     /* depths, drive: % */
            put_num(buf, cap, v * 100 / 127);
            cat_str(buf, cap, "%");
        }
        return;
    }
    if (t == (int)RI_SLEVI_PDRYWET || t == (int)RI_SLEVI_ODRYWET) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_ARPOCTMODE) {
        static const char *const OM[3] = { "OFF", "UP", "DOWN" };
        int m = s->val[t];
        put_str(buf, cap, OM[m >= 0 && m < 3 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_ARPOCTRANGE) {
        put_num(buf, cap, 1 + s->val[t] * 3 / 127);
        return;
    }
    if (t == (int)RI_SLEVI_ARPGATE) {
        put_num(buf, cap, 5 + s->val[t] * 145 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_ARPMODE) {
        static const char *const AM[9] = { "UP", "DOWN", "UPDOWN", "CHORD", "OCTUP", "OCTDOWN",
            "RANDOM", "ENTROPY", "PHRASE" };
        int m = s->val[t];
        put_str(buf, cap, AM[m >= 0 && m < 9 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_ARPLEN) {
        put_num(buf, cap, 1 + s->val[t] * 15 / 127);
        return;
    }
    if (t == (int)RI_SLEVI_ARPPHRASE) {
        int v = s->val[t];
        put_str(buf, cap, v < 64 ? "F" : "U");
        cat_num(buf, cap, (v % 64) + 1);
        return;
    }
    if (t == (int)RI_SLEVI_ARPENTROPY || t == (int)RI_SLEVI_ARPSWING ||
        t == (int)RI_SLEVI_ARPRATCHET || t == (int)RI_SLEVI_ARPCHANCE) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_ARPLATCH) {
        put_str(buf, cap, s->val[t] ? "ON" : "OFF");
        return;
    }
    if (t == (int)RI_SLEVI_ARPCLOCK) {
        put_str(buf, cap, s->val[t] ? "LOCK" : "FREE");
        return;
    }
    if (t == (int)RI_SLEVI_ARPSTEPPOFF) {
        put_num(buf, cap, s->val[t] * 15 / 127);
        return;
    }
    if (t == (int)RI_SLEVI_SEQRATE) {
        int v = s->val[t];
        put_num(buf, cap, v <= 42 ? 1 : v <= 85 ? 2 : 4);
        cat_str(buf, cap, "/Q");
        return;
    }
    if (t == (int)RI_SLEVI_SEQMODE) {
        static const char *const SM[3] = { "OFF", "PARA", "SERIES" };
        int m = s->val[t];
        put_str(buf, cap, SM[m >= 0 && m < 3 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_SEQSWING || t == (int)RI_SLEVI_SEQGATE ||
        t == (int)RI_SLEVI_SEQPROB) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_SEQDRIFT) {
        int v = s->val[t] - 64;
        if (v < 0) {
            put_str(buf, cap, "-");
            put_num(buf, cap, -v);
        } else {
            put_str(buf, cap, "+");
            put_num(buf, cap, v);
        }
        cat_str(buf, cap, "TK");
        return;
    }
    if (t == (int)RI_SLEVI_SEQTRANSP) {
        int v = (s->val[t] - 64) * 48 / 127;
        if (v < 0) {
            put_str(buf, cap, "-");
            put_num(buf, cap, -v);
        } else {
            put_str(buf, cap, "+");
            put_num(buf, cap, v);
        }
        return;
    }
    if (t == (int)RI_SLEVI_SEQTRKLEN) {
        put_num(buf, cap, 1 + s->val[t]);
        return;
    }
    if (t == (int)RI_SLEVI_SEQREC) {
        put_str(buf, cap, s->val[t] ? "ARM" : "OFF");
        return;
    }
    if (t == (int)RI_SLEVI_SEQSTEP) {
        put_num(buf, cap, s->val[t] + 1);
        return;
    }
    if (t == (int)RI_SLEVI_SEQCLEAR) {
        put_str(buf, cap, "CLR");
        return;
    }
    if (t == (int)RI_SLEVI_SEQSTRIG) {
        put_num(buf, cap, 1 + s->val[t] * 3 / 127);
        return;
    }
    if (t == (int)RI_SLEVI_SEQSPROB) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_SEQSDRIFT) {
        int v = s->val[t] - 64;
        if (v < 0) {
            put_str(buf, cap, "-");
            put_num(buf, cap, -v);
        } else {
            put_str(buf, cap, "+");
            put_num(buf, cap, v);
        }
        cat_str(buf, cap, "TK");
        return;
    }
    if (t == (int)RI_SLEVI_SEQSENTR) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_RBNMODE) {
        static const char *const RM[4] = { "OFF", "ABS", "REL", "THEREMIN" };
        int m = s->val[t];
        put_str(buf, cap, RM[m >= 0 && m < 4 ? m : 0]);
        return;
    }
    if (t == (int)RI_SLEVI_RBNPOS) {
        put_num(buf, cap, s->val[t] * 100 / 127);
        cat_str(buf, cap, "%");
        return;
    }
    if (t == (int)RI_SLEVI_RBNTOUCH) {
        put_str(buf, cap, s->val[t] ? "ON" : "OFF");
        return;
    }
    if (t >= (int)RI_SLEVI_SLOT0 && t < (int)RI_SLEVI_SLOT0 + 8) {
        int a = s->val[t];
        if (a >= (int)RI_LEVI_SLOT_OFF)
            put_str(buf, cap, "OFF");
        else if (a == (int)RI_LEVI_SLOT_SILENCE)
            put_str(buf, cap, "SILENCE");
        else
            put_num(buf, cap, a + 1);
        return;
    }
    if (t == (int)RI_SLEVI_SOLO) {
        if (!s->val[t]) {
            put_str(buf, cap, "OFF");
            return;
        }
        put_str(buf, cap, "OSC ");
        cat_num(buf, cap, s->val[t]);
        return;
    }
    if (t == (int)RI_SLEVI_MUTELO) {                 /* one mark per oscillator 1-7 */
        uint32_t i;
        char m[8];
        for (i = 0u; i < 7u; i++)
            m[i] = (char)((s->val[t] >> i) & 1 ? (char)('1' + i) : '-');
        m[7] = 0;
        put_str(buf, cap, m);
        return;
    }
    d = def(s, (uint32_t)t);
    if (d && d->kind == RI_CK_SWITCH) {
        put_str(buf, cap, s->val[t] ? "ON" : "OFF");
        return;
    }
    put_num(buf, cap, s->val[t]);
}

/* Encoder k turned to v (0..127): scale onto the target's range. */
static int enc_set(struct RISectLevi *s, uint32_t k, int v) {
    int t = slot(s, k, 0), lo, hi, w;
    const struct RICtlDef *d;
    uint8_t *mv;
    if (v < 0)
        v = 0;
    if (v > 127)
        v = 127;
    mv = mod_val(s, t, &lo, &hi, 0, 0);
    if (mv) {
        w = lo + (v * (hi - lo) + 63) / 127;
        if (*mv == (uint8_t)w)
            return 0;
        *mv = (uint8_t)w;
        return 1;
    }
    if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
        uint32_t o = slot_op(s, t, k), p = SLOT_PARAM(t);
        if (p == 32u) {   /* per-op ENV BPM flag: switch 0/1 */
            w = v >= 64 ? 1 : 0;
            if (s->opbpm[o] == (uint8_t)w)
                return 0;
            s->opbpm[o] = (uint8_t)w;
            return 1;
        }
        if (ri_levi_op_range(p, &lo, &hi) != 0)
            return 0;
        w = lo + (v * (hi - lo) + 63) / 127;
        if (s->opv[o][p] == (uint8_t)w)
            return 0;
        s->opv[o][p] = (uint8_t)w;
        if (p == RI_LEVI_OP_MODE)
            s->opmode[o] = (uint8_t)w;
        return 1;
    }
    if (t < 0)
        return 0;
    d = def(s, (uint32_t)t);
    if (!d)
        return 0;
    lo = d->min_v;
    hi = t == (int)RI_SLEVI_FTYPE ? 3 : d->max_v;
    if (d->kind == RI_CK_SWITCH) {
        w = v >= 64 ? hi : lo;
        if (s->val[t] == w)
            return 0;
        s->val[t] = (int16_t)w;
        return 1;
    }
    w = lo + (v * (hi - lo) + 63) / 127;
    return ri_slevi_set_value(s, (uint32_t)t, w);
}
