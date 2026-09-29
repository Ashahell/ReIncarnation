/* sectlevi.c — Levi section behaviour bodies (owner 2026-09-28). */
#include "gui/sectlevi.h"
#include "gui/ctlreg.h"
#include "engine/dsp/levi.h"

/* Page-slot codes (see the page UI block below). */
#define SLOT_DEAD (-1)
#define SLOT_GMODE (-2)

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
        return 1;
    }
    if (idx == RI_SLEVI_SELECT) {
        uint8_t sel = v < 0 ? 0u : v > 5 ? 5u : (uint8_t)v;
        if (s->sel == sel)
            return 0;
        s->sel = sel;
        return 1;
    }
    if (idx == RI_SLEVI_ALGO || idx == RI_SLEVI_ALGOB || idx == RI_SLEVI_FTYPE) {
        /* Plain selectors (selector idiom, like Lane). FTYPE clamps 0..3. */
        int hi = idx == RI_SLEVI_FTYPE ? 3 : 7;
        int w = v < 0 ? 0 : v > hi ? hi : v;
        if (s->val[idx] == w)
            return 0;
        s->val[idx] = (int16_t)w;
        return 1;
    }
    if (idx == RI_SLEVI_OPSEL) {
        uint8_t o = v < 0 ? 0u : v > 7 ? 7u : (uint8_t)v;
        int packed = (int)o * 16 + s->opmode[o];
        if (s->opsel == o && s->val[RI_SLEVI_OPMODE] == packed &&
            s->val[RI_SLEVI_MODULE] == (int16_t)RI_SLEVI_M_OSC)
            return 0;
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
        int t = slot(s, (uint32_t)enc_of(idx), 0);
        if (t == SLOT_GMODE) {
            uint32_t k = (uint32_t)enc_of(idx);
            if (s->opmode[k] == RI_LEVI_FM)
                return 0;
            s->opmode[k] = RI_LEVI_FM;
            s->val[RI_SLEVI_OPMODE] = (int16_t)(k * 16u);
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
    a = (uint32_t)s->val[RI_SLEVI_ALGO] + 1u;
    return a < 1u ? 1 : (int)(a > 8u ? 8u : a);
}

/* ---- Hardware page UI (fidelity plan P1, owner 2026-09-30) ----
 * Page slots follow the manual's control-knob order per module
 * (osc settings p. 35, envelopes p. 71, digital filter p. 62, analog
 * filter p. 66, VCA p. 69, FX pp. 83-86, LFO p. 76, arp p. 99). A slot
 * is a parameter index, SLOT_DEAD (engine lands in a later phase) or
 * SLOT_GMODE (Oscillator Group Edit MODE: encoder k edits op k). */

struct LeviSlot {
    int16_t idx;
    const char *name;
};

static const char *const OSC_NAME[RI_LEVI_NOPS] = {
    "OSC 1", "OSC 2", "OSC 3", "OSC 4", "OSC 5", "OSC 6", "OSC 7", "OSC 8"
};

static const struct LeviSlot P_OSC[8] = {
    { RI_SLEVI_OPMODE, "MODE" }, { SLOT_DEAD, "WAVE" }, { RI_SLEVI_RATIO, "RATIO" },
    { SLOT_DEAD, "FINE" }, { SLOT_DEAD, "INIT LVL" }, { SLOT_DEAD, "ENV LVL" },
    { SLOT_DEAD, "FEEDBK" }, { SLOT_DEAD, "KEYTRK" }
};
static const struct LeviSlot P_OSCENV[8] = {
    { RI_SLEVI_ATTACK, "ATTACK" }, { RI_SLEVI_DECAY, "DECAY" }, { RI_SLEVI_SUSTAIN, "SUSTAIN" },
    { RI_SLEVI_RELEASE, "RELEASE" }, { SLOT_DEAD, "DELAY" }, { SLOT_DEAD, "HOLD" },
    { SLOT_DEAD, "SPEED" }, { RI_SLEVI_LOOP, "LOOP" }
};
static const struct LeviSlot P_GPITCH[8] = {
    { RI_SLEVI_RATIO, "RATIO" }, { SLOT_DEAD, "OSC 2" }, { SLOT_DEAD, "OSC 3" }, { SLOT_DEAD, "OSC 4" },
    { SLOT_DEAD, "OSC 5" }, { SLOT_DEAD, "OSC 6" }, { SLOT_DEAD, "OSC 7" }, { SLOT_DEAD, "OSC 8" }
};
static const struct LeviSlot P_ENV[8] = {
    { SLOT_DEAD, "ATTACK" }, { SLOT_DEAD, "DECAY" }, { SLOT_DEAD, "SUSTAIN" }, { SLOT_DEAD, "RELEASE" },
    { SLOT_DEAD, "DELAY" }, { SLOT_DEAD, "HOLD" }, { SLOT_DEAD, "SPEED" }, { SLOT_DEAD, "BPM SYNC" }
};
static const struct LeviSlot P_DFILT[8] = {
    { RI_SLEVI_FTYPE, "TYPE" }, { SLOT_DEAD, "MORPH" }, { RI_SLEVI_CUTOFF, "CUTOFF" }, { RI_SLEVI_RESO, "RESO" },
    { SLOT_DEAD, "ENV1 AMT" }, { SLOT_DEAD, "VEL>ENV" }, { SLOT_DEAD, "POLYAT" }, { SLOT_DEAD, "KEYTRK" }
};
static const struct LeviSlot P_AFILT[8] = {
    { RI_SLEVI_DRIVE, "PRE-DRV" }, { SLOT_DEAD, "LFO2 AMT" }, { RI_SLEVI_CUTOFF2, "CUTOFF" },
    { RI_SLEVI_RESO2, "RESO" }, { SLOT_DEAD, "ENV2 AMT" }, { SLOT_DEAD, "VEL>ENV" },
    { SLOT_DEAD, "POLYAT" }, { SLOT_DEAD, "KEYTRK" }
};
static const struct LeviSlot P_VCA[8] = {
    { SLOT_DEAD, "OSCS LVL" }, { SLOT_DEAD, "DFILT LVL" }, { SLOT_DEAD, "VCA LVL" }, { SLOT_DEAD, "PATCH LVL" },
    { SLOT_DEAD, "LFO3 AMT" }, { SLOT_DEAD, "VEL>ENV" }, { SLOT_DEAD, "POLYAT" }, { SLOT_DEAD, "INIT LVL" }
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
static const struct LeviSlot P_ALGO[8] = {
    { SLOT_DEAD, "ALGO MODE" }, { RI_SLEVI_ALGO, "ALGO" }, { RI_SLEVI_ALGOB, "TARGET" }, { SLOT_DEAD, "SLOT" },
    { RI_SLEVI_MORPH, "MORPH" }, { RI_SLEVI_MODE, "PM/FM" }, { SLOT_DEAD, "SOLO" }, { SLOT_DEAD, "CUSTOM" }
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

static uint32_t module(const struct RISectLevi *s) {
    int m = s->val[RI_SLEVI_MODULE];
    return (m < 0 || m >= (int)RI_SLEVI_NMOD) ? RI_SLEVI_M_OSC : (uint32_t)m;
}

/* Slot k of the current page; name out (may be NULL). */
static int slot(const struct RISectLevi *s, uint32_t k, const char **name) {
    const struct LeviSlot *p = 0;
    uint32_t m = module(s);
    static const char *const dead_osc = "";
    if (k >= RI_SLEVI_NENC) {
        if (name)
            *name = dead_osc;
        return SLOT_DEAD;
    }
    if (m == RI_SLEVI_M_GMODE) {
        if (name)
            *name = OSC_NAME[k];
        return SLOT_GMODE;
    }
    if (m == RI_SLEVI_M_GWAVE || m == RI_SLEVI_M_GFINE || m == RI_SLEVI_M_GFEEDBK || m == RI_SLEVI_M_GLEVEL) {
        if (name)
            *name = OSC_NAME[k];
        return SLOT_DEAD;
    }
    p = m == RI_SLEVI_M_OSC ? P_OSC
        : m == RI_SLEVI_M_GPITCH ? P_GPITCH
        : (m >= RI_SLEVI_M_GDELAY && m <= RI_SLEVI_M_GRELEASE) ? P_OSCENV
        : (m >= RI_SLEVI_M_ENV1 && m < RI_SLEVI_M_ENV1 + 5u) ? P_ENV
        : m == RI_SLEVI_M_DFILT ? P_DFILT : m == RI_SLEVI_M_AFILT ? P_AFILT
        : m == RI_SLEVI_M_VCA ? P_VCA : m == RI_SLEVI_M_PREFX ? P_PREFX
        : m == RI_SLEVI_M_DELAY ? P_DELAY : m == RI_SLEVI_M_REVERB ? P_REVERB
        : m == RI_SLEVI_M_POSTFX ? P_POSTFX
        : (m >= RI_SLEVI_M_LFO1 && m < RI_SLEVI_M_LFO1 + 5u) ? P_LFO
        : m == RI_SLEVI_M_ALGO ? P_ALGO : m == RI_SLEVI_M_ARP ? P_ARP
        : m == RI_SLEVI_M_SEQ ? P_SEQ : m == RI_SLEVI_M_MATRIX ? P_MATRIX : P_VOICE;
    if (name)
        *name = p[k].name;
    return p[k].idx;
}

static int enc_of(uint32_t idx) {
    return idx >= RI_SLEVI_ENC0 && idx < RI_SLEVI_ENC0 + RI_SLEVI_NENC ? (int)(idx - RI_SLEVI_ENC0) : -1;
}

/* Target value scaled to 0..127 (encoder position / LED ring). */
static int enc_value(const struct RISectLevi *s, uint32_t k) {
    int t = slot(s, k, 0), lo, hi, v;
    const struct RICtlDef *d;
    if (t == SLOT_GMODE)
        return (int)s->opmode[k] * 127 / 6;
    if (t < 0)
        return 0;
    if (t == (int)RI_SLEVI_OPMODE)
        return (int)s->opmode[s->opsel] * 127 / 6;
    d = def(s, (uint32_t)t);
    if (!d)
        return 0;
    lo = d->min_v;
    hi = d->max_v;
    v = s->val[t];
    if (t == (int)RI_SLEVI_FTYPE)
        hi = 3;
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
        return (int)module(s);
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
    return t == SLOT_GMODE ? RI_SLEVI_OPMODE : t >= 0 ? (uint32_t)t : idx;
}

int ri_slevi_page_reaches(uint32_t idx) {
    struct RISectLevi t;
    uint32_t m, k;
    ri_slevi_init(&t);
    for (m = 0u; m < RI_SLEVI_NMOD; m++) {
        t.val[RI_SLEVI_MODULE] = (int16_t)m;
        for (k = 0u; k < RI_SLEVI_NENC; k++) {
            int x = slot(&t, k, 0);
            if ((x >= 0 && (uint32_t)x == idx) || (x == SLOT_GMODE && idx == RI_SLEVI_OPMODE))
                return 1;
        }
    }
    return 0;
}

int ri_slevi_enc_live(const struct RISectLevi *s, uint32_t k) {
    return s && k < RI_SLEVI_NENC && slot(s, k, 0) != SLOT_DEAD;
}

const char *ri_slevi_page_title(const struct RISectLevi *s) {
    static const char *const T[RI_SLEVI_NMOD] = {
        "OSC", "GROUP: MODE", "GROUP: WAVE", "GROUP: PITCH", "GROUP: FINE", "GROUP: FEEDBACK",
        "GROUP: LEVEL", "OSC ENVELOPES", "OSC ENVELOPES", "OSC ENVELOPES", "OSC ENVELOPES",
        "OSC ENVELOPES", "OSC ENVELOPES", "ENV 1", "ENV 2", "ENV 3", "ENV 4", "ENV 5",
        "DIGITAL FILTER", "ANALOG FILTER", "VCA", "PRE-FX", "DELAY", "REVERB", "POST-FX",
        "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "ALGORITHM", "ARPEGGIATOR", "SEQUENCER",
        "MOD MATRIX", "VOICE"
    };
    uint32_t m;
    if (!s)
        return "";
    m = module(s);
    return m == RI_SLEVI_M_OSC ? OSC_NAME[s->opsel % RI_LEVI_NOPS] : T[m];
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

void ri_slevi_enc_text(const struct RISectLevi *s, uint32_t k, char *buf, uint32_t cap) {
    static const char *const MODES[7] = { "FREQ MOD", "PHASE MOD", "PW MOD", "HTE SYNC", "PD SAW",
        "PD SQUARE", "PD SAW PLS" };
    static const char *const FTYPES[4] = { "LP", "HP", "BP", "NOTCH" };
    const struct RICtlDef *d;
    int t;
    if (!buf || !cap)
        return;
    buf[0] = 0;
    if (!s || k >= RI_SLEVI_NENC)
        return;
    t = slot(s, k, 0);
    if (t == SLOT_GMODE) {
        put_str(buf, cap, MODES[s->opmode[k] % 7u]);
        return;
    }
    if (t < 0)
        return;
    if (t == (int)RI_SLEVI_OPMODE) {
        put_str(buf, cap, MODES[s->opmode[s->opsel] % 7u]);
        return;
    }
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
    d = def(s, (uint32_t)t);
    if (d && d->kind == RI_CK_SWITCH) {
        put_str(buf, cap, s->val[t] ? "ON" : "OFF");
        return;
    }
    put_num(buf, cap, s->val[t]);
}

/* Encoder k turned to v (0..127): scale onto the target's range. Group
 * MODE writes op k's mode and points the packed wire value at op k. */
static int enc_set(struct RISectLevi *s, uint32_t k, int v) {
    int t = slot(s, k, 0), lo, hi, w;
    const struct RICtlDef *d;
    if (v < 0)
        v = 0;
    if (v > 127)
        v = 127;
    if (t == SLOT_GMODE) {
        uint8_t m = (uint8_t)((v * 6 + 63) / 127);
        int packed = (int)k * 16 + m;
        if (s->opmode[k] == m && s->val[RI_SLEVI_OPMODE] == packed)
            return 0;
        s->opmode[k] = m;
        s->val[RI_SLEVI_OPMODE] = (int16_t)packed;
        return 1;
    }
    if (t < 0)
        return 0;
    if (t == (int)RI_SLEVI_OPMODE)
        return ri_slevi_set_value(s, (uint32_t)t, (v * 6 + 63) / 127);
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
