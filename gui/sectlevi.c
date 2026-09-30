/* sectlevi.c — Levi section behaviour bodies (owner 2026-09-28). */
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"
#include "engine/dsp/levi.h"

/* Page-slot codes (see the page UI block below): a panel control index
 * (>= 0), a dead slot (engine in a later phase), or a per-oscillator
 * param of the page's oscillator (SLOT_OP) or of oscillator k on a group
 * page (SLOT_GOP). */
#define SLOT_DEAD (-1)
#define SLOT_OP(p) (-(16 + (int)(p)))
#define SLOT_GOP(p) (-(64 + (int)(p)))
#define SLOT_IS_OP(x) ((x) <= -16 && (x) > -64)
#define SLOT_IS_GOP(x) ((x) <= -64 && (x) > -96)
#define SLOT_PARAM(x) ((uint32_t)(SLOT_IS_GOP(x) ? -(x) - 64 : -(x) - 16))

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
    if (idx == RI_SLEVI_ALGO || idx == RI_SLEVI_ALGOB || idx == RI_SLEVI_FTYPE || idx == RI_SLEVI_AMODE ||
        idx == RI_SLEVI_DTYPE || idx == RI_SLEVI_VORDER) {
        /* Plain selectors (selector idiom, like Lane). FTYPE clamps 0..3. */
        int hi = idx == RI_SLEVI_FTYPE ? 3 : idx == RI_SLEVI_AMODE ? 2 : idx == RI_SLEVI_DTYPE ? 17
            : idx == RI_SLEVI_VORDER ? 7 : 63;
        int w = v < 0 ? 0 : v > hi ? hi : v;
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
    if (d->kind != RI_CK_KNOB && d->kind != RI_CK_SWITCH)
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
        int t = slot(s, k, 0);
        if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
            uint32_t o = slot_op(s, t, k), p = SLOT_PARAM(t);
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
      { OP(DELAY), "DELAY" }, { OP(HOLD), "HOLD" }, { OP(SPEED), "SPEED" }, { SLOT_DEAD, "BPM SYNC" } },
    { { OP(ACURVE), "ATK CRV" }, { OP(DCURVE), "DEC CRV" }, { OP(QUANT), "QUANTIZE" }, { OP(RCURVE), "REL CRV" },
      { OP(LEGATO), "LEGATO" }, { OP(RESET), "RESET" }, { OP(FREERUN), "FREERUN" }, { OP(LOOP), "ENV LOOP" } },
    { { SLOT_DEAD, "TRIG 1" }, { SLOT_DEAD, "TRIG 2" }, { SLOT_DEAD, "TRIG 3" }, { SLOT_DEAD, "TRIG 4" },
      { SLOT_DEAD, "VEL CRV" }, { OP(VELENV), "VEL>ENV" }, { OP(STAGELOOP), "STG LOOP" }, { SLOT_DEAD, "TAP TRIG" } },
    { { OP(PMODE), "PITCH MD" }, { OP(DIRECT), "DIRECT" }, { OP(PHASE), "PHASE" }, { SLOT_DEAD, "KEYSCALE" },
      { OP(INVERT), "INVERT" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" } },
};
#undef OP
static const struct LeviSlot P_ENV[8] = {
    { SLOT_DEAD, "ATTACK" }, { SLOT_DEAD, "DECAY" }, { SLOT_DEAD, "SUSTAIN" }, { SLOT_DEAD, "RELEASE" },
    { SLOT_DEAD, "DELAY" }, { SLOT_DEAD, "HOLD" }, { SLOT_DEAD, "SPEED" }, { SLOT_DEAD, "BPM SYNC" }
};
/* Filter and VCA pages (P4, pp. 62-70). Env/velocity/PolyAT amounts and
 * the VCA initial level wait for the P5 envelopes and P9 pressure. */
static const struct LeviSlot P_DFILT[2][8] = {
    { { RI_SLEVI_DTYPE, "TYPE" }, { RI_SLEVI_DMORPH, "DRIVE" }, { RI_SLEVI_CUTOFF, "CUTOFF" },
      { RI_SLEVI_RESO, "RESO" }, { SLOT_DEAD, "ENV1 AMT" }, { SLOT_DEAD, "VEL>ENV" }, { SLOT_DEAD, "POLYAT" },
      { RI_SLEVI_DKEYTRK, "KEYTRK" } },
    { { SLOT_DEAD, "" }, { RI_SLEVI_DPOST, "DRV POS" }, { SLOT_DEAD, "" }, { SLOT_DEAD, "" },
      { RI_SLEVI_VORDER, "VOW ORDER" }, { SLOT_DEAD, "" }, { RI_SLEVI_DLFO1, "LFO1 AMT" },
      { RI_SLEVI_DLEVEL, "DFILT LVL" } }
};
static const struct LeviSlot P_AFILT[8] = {
    { RI_SLEVI_DRIVE, "PRE-DRV" }, { RI_SLEVI_ALFO2, "LFO2 AMT" }, { RI_SLEVI_CUTOFF2, "CUTOFF" },
    { RI_SLEVI_RESO2, "RESO" }, { SLOT_DEAD, "ENV2 AMT" }, { SLOT_DEAD, "VEL>ENV" },
    { SLOT_DEAD, "POLYAT" }, { RI_SLEVI_AKEYTRK, "KEYTRK" }
};
static const struct LeviSlot P_VCA[8] = {
    { RI_SLEVI_OSCLVL, "OSCS LVL" }, { RI_SLEVI_DLEVEL, "DFILT LVL" }, { RI_SLEVI_VCALVL, "VCA LVL" },
    { RI_SLEVI_PATCHLVL, "PATCH LVL" }, { RI_SLEVI_VLFO3, "LFO3 AMT" }, { SLOT_DEAD, "VEL>ENV" },
    { SLOT_DEAD, "POLYAT" }, { SLOT_DEAD, "INIT LVL" }
};
static const struct LeviSlot P_PREFX[8] = {
    { RI_SLEVI_FXPRE, "ON" }, { SLOT_DEAD, "PRESET" }, { SLOT_DEAD, "PARAM 1" }, { SLOT_DEAD, "PARAM 2" },
    { SLOT_DEAD, "PARAM 3" }, { SLOT_DEAD, "PARAM 4" }, { SLOT_DEAD, "PARAM 5" }, { SLOT_DEAD, "DRY/WET" }
};
static const struct LeviSlot P_POSTFX[8] = {
    { RI_SLEVI_FXPOST, "ON" }, { SLOT_DEAD, "PRESET" }, { SLOT_DEAD, "PARAM 1" }, { SLOT_DEAD, "PARAM 2" },
    { SLOT_DEAD, "PARAM 3" }, { SLOT_DEAD, "PARAM 4" }, { SLOT_DEAD, "PARAM 5" }, { SLOT_DEAD, "DRY/WET" }
};
static const struct LeviSlot P_DELAY[8] = {
    { RI_SLEVI_FXDLY, "ON" }, { SLOT_DEAD, "TIME" }, { SLOT_DEAD, "FEEDBACK" }, { SLOT_DEAD, "WET TONE" },
    { SLOT_DEAD, "TYPE" }, { SLOT_DEAD, "BPM SYNC" }, { SLOT_DEAD, "FB TONE" }, { SLOT_DEAD, "DRY/WET" }
};
static const struct LeviSlot P_REVERB[8] = {
    { RI_SLEVI_FXREV, "ON" }, { SLOT_DEAD, "PRE-DLY" }, { SLOT_DEAD, "TIME" }, { SLOT_DEAD, "TONE" },
    { SLOT_DEAD, "TYPE" }, { SLOT_DEAD, "HI DAMP" }, { SLOT_DEAD, "LO DAMP" }, { SLOT_DEAD, "DRY/WET" }
};
static const struct LeviSlot P_LFO[8] = {
    { SLOT_DEAD, "WAVE" }, { SLOT_DEAD, "RATE" }, { SLOT_DEAD, "SPEED" }, { SLOT_DEAD, "TRIG SYNC" },
    { SLOT_DEAD, "DELAY" }, { SLOT_DEAD, "FADE IN" }, { SLOT_DEAD, "QUANTIZE" }, { SLOT_DEAD, "LEVEL" }
};
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
    { RI_SLEVI_ARPRATE, "DIVISION" }, { SLOT_DEAD, "OCT MODE" }, { SLOT_DEAD, "OCT RANGE" }, { SLOT_DEAD, "GATE" },
    { SLOT_DEAD, "MODE" }, { SLOT_DEAD, "LENGTH" }, { SLOT_DEAD, "PHRASE" }, { SLOT_DEAD, "TEMPO" }
};
static const struct LeviSlot P_SEQ[8] = {
    { RI_SLEVI_SEQLEN, "LENGTH" }, { SLOT_DEAD, "RATE" }, { SLOT_DEAD, "MODE" }, { SLOT_DEAD, "SWING" },
    { SLOT_DEAD, "GATE" }, { SLOT_DEAD, "PROB" }, { SLOT_DEAD, "DRIFT" }, { SLOT_DEAD, "TRANSPOSE" }
};
static const struct LeviSlot P_MATRIX[8] = {
    { RI_SLEVI_ROUTE0 + 0, "ROUTE 1" }, { RI_SLEVI_ROUTE0 + 1, "ROUTE 2" }, { RI_SLEVI_ROUTE0 + 2, "ROUTE 3" },
    { RI_SLEVI_ROUTE0 + 3, "ROUTE 4" }, { RI_SLEVI_ROUTE0 + 4, "ROUTE 5" }, { RI_SLEVI_ROUTE0 + 5, "ROUTE 6" },
    { RI_SLEVI_ROUTE0 + 6, "ROUTE 7" }, { RI_SLEVI_ROUTE0 + 7, "ROUTE 8" }
};
static const struct LeviSlot P_VOICE[8] = {
    { SLOT_DEAD, "POLYPHONY" }, { SLOT_DEAD, "DENSITY" }, { SLOT_DEAD, "DETUNE" }, { SLOT_DEAD, "ANALOG FL" },
    { SLOT_DEAD, "RND PHASE" }, { SLOT_DEAD, "PANNER" }, { SLOT_DEAD, "WIDTH" }, { SLOT_DEAD, "PAN MODE" }
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
        : module(s) == RI_SLEVI_M_DFILT ? 2u : 1u;
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
    if (m == RI_SLEVI_M_ALGO && s->page >= 2u && s->page < 5u) {
        if (name)
            *name = OSC_NAME[k];
        return SLOT_GOP(RI_LEVI_OP_TGT1 + (uint32_t)(s->page - 2u));
    }
    p = m == RI_SLEVI_M_OSC ? P_OSC[s->page < 5u ? s->page : 0u]
        : (m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + 5u) ? P_ENV
        : m == RI_SLEVI_M_DFILT ? P_DFILT[s->page == 1u ? 1u : 0u] : m == RI_SLEVI_M_AFILT ? P_AFILT
        : m == RI_SLEVI_M_VCA ? P_VCA : m == RI_SLEVI_M_PREFX ? P_PREFX
        : m == RI_SLEVI_M_DELAY ? P_DELAY : m == RI_SLEVI_M_REVERB ? P_REVERB
        : m == RI_SLEVI_M_POSTFX ? P_POSTFX
        : (m >= RI_SLEVI_M_LFO1 && m < RI_SLEVI_M_LFO1 + 5u) ? P_LFO
        : m == RI_SLEVI_M_ALGO ? P_ALGO[s->page < 2u ? s->page : 0u] : m == RI_SLEVI_M_ARP ? P_ARP
        : m == RI_SLEVI_M_SEQ ? P_SEQ : m == RI_SLEVI_M_MATRIX ? P_MATRIX : P_VOICE;
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
    return p[k].idx;
}

static uint32_t slot_op(const struct RISectLevi *s, int t, uint32_t k) {
    return SLOT_IS_GOP(t) ? (k & 7u) : (uint32_t)(s->opsel & 7u);
}

static int enc_of(uint32_t idx) {
    return idx >= RI_SLEVI_ENC0 && idx < RI_SLEVI_ENC0 + RI_SLEVI_NENC ? (int)(idx - RI_SLEVI_ENC0) : -1;
}

/* Target value scaled to 0..127 (encoder position / LED ring). */
static int enc_value(const struct RISectLevi *s, uint32_t k) {
    int t = slot(s, k, 0), lo, hi, v;
    const struct RICtlDef *d;
    if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
        uint32_t o = slot_op(s, t, k), p = SLOT_PARAM(t);
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
    if (!SLOT_IS_OP(t) && !SLOT_IS_GOP(t))
        return 0;
    o = slot_op(s, t, (uint32_t)k);
    p = SLOT_PARAM(t);
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
        idx == RI_SLEVI_ALGOB || idx == RI_SLEVI_MORPH || idx == RI_SLEVI_FTYPE;
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
        "MOD MATRIX", "VOICE"
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
static void op_text(const struct RISectLevi *s, uint32_t o, uint32_t p, char *buf, uint32_t cap) {
    static const char *const MODES[7] = { "FREQ MOD", "PHASE MOD", "PW MOD", "HTE SYNC", "PD SAW",
        "PD SQUARE", "PD SAW PLS" };
    static const char *const PMODES[3] = { "SEMITONE", "RATIO", "FREQUENCY" };
    static const char *const STAGES[3] = { "DLY>ATK", "DLY>HOLD", "DLY>DEC" };
    int v = s->opv[o][p], pm = s->opv[o][RI_LEVI_OP_PMODE];
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
        time_text(buf, cap, ri_levi_env_time(p, (uint8_t)v, s->opv[o][RI_LEVI_OP_SPEED] ? 1u : 0u));
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
        op_text(s, slot_op(s, t, k), SLOT_PARAM(t), buf, cap);
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
    if (t == (int)RI_SLEVI_DLFO1 || t == (int)RI_SLEVI_ALFO2 || t == (int)RI_SLEVI_VLFO3) {
        if (s->val[t] > 64)
            put_str(buf, cap, "+");
        cat_num(buf, cap, s->val[t] - 64);
        return;
    }
    if (t == (int)RI_SLEVI_DLEVEL || t == (int)RI_SLEVI_OSCLVL || t == (int)RI_SLEVI_VCALVL ||
        t == (int)RI_SLEVI_PATCHLVL) {
        put_num(buf, cap, s->val[t] * 128 / 127);   /* 64 = unity (p. 68) */
        return;
    }
    if (t == (int)RI_SLEVI_AMODE) {
        static const char *const AM[3] = { "SINGLE", "MORPH", "CUSTOM" };
        put_str(buf, cap, AM[s->val[t] > 2 ? 2 : s->val[t] < 0 ? 0 : s->val[t]]);
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
    if (v < 0)
        v = 0;
    if (v > 127)
        v = 127;
    if (SLOT_IS_OP(t) || SLOT_IS_GOP(t)) {
        uint32_t o = slot_op(s, t, k), p = SLOT_PARAM(t);
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
