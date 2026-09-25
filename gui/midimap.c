/* gui/midimap.c — Remote MIDI Control, Standard Mapping (§12.10 G7). */
#include "gui/midimap.h"
#include "gui/ctlreg.h"

void ri_midi_init(struct RIMidiIn *m, uint8_t channel) {
    if (!m)
        return;
    m->channel = (uint8_t)(channel & 15u);
    m->status = 0;
    m->data[0] = m->data[1] = 0;
    m->ndata = 0;
    m->in_sysex = 0;
    m->clocks = 0;
    m->midi_led_ms = 0;
    m->messages = m->ignored = 0;
}

static int led(struct RIPanelUI *p, uint32_t idx, int v) {
    int old;
    if (!p->tr)
        return 0;
    old = ri_sui_value(p->tr, idx);
    ri_str_indicator_set(&p->tr->u.tr, idx, v);
    return old != v;
}

/* The section state a registry section lives in, or NULL. */
static struct RISectUI *ui_of(struct RIPanelUI *p, uint32_t sec) {
    if (sec == RI_SEC_SYNTH1 || sec == RI_SEC_SYNTH2)
        return p->synth[sec - RI_SEC_SYNTH1];
    if (sec == RI_SEC_808 || sec == RI_SEC_909)
        return p->drum[sec - RI_SEC_808];
    if (sec >= RI_SEC_MIX_SYNTH1 && sec <= RI_SEC_MASTER)
        return p->mix[sec - RI_SEC_MIX_SYNTH1];
    if (sec >= RI_SEC_PCF && sec <= RI_SEC_COMP)
        return p->fx[sec - RI_SEC_PCF];
    if (sec >= RI_SEC_PAT_SYNTH1 && sec <= RI_SEC_PAT_909)
        return p->pat[sec - RI_SEC_PAT_SYNTH1];
    return sec == RI_SEC_TRANSPORT ? p->tr : 0;
}

/* Control Change: Appendix C via the registry. */
static int cc(struct RIPanelUI *p, uint8_t num, uint8_t val) {
    const struct RICtlDef *d = ri_ctlreg_by_cc(num);
    struct RISectUI *u;
    uint32_t idx;
    int span, v;
    if (!d || !(u = ui_of(p, d->section)))
        return 0;
    idx = d->reg_id & 0xFFu;
    span = d->max_v - d->min_v;
    if (d->kind == RI_CK_SWITCH) {           /* on at >= 64 (E0); press to match */
        int want = val >= 64u, now = ri_sui_value(u, idx) != 0;
        return want != now ? ri_sui_press(u, idx) : 0;
    }
    if (d->kind == RI_CK_SELECTOR)           /* n positions over 0..127 */
        v = d->min_v + (int)((uint32_t)val * (uint32_t)(span + 1) / 128u);
    else                                      /* knob / fader: linear, 127 = max */
        v = d->min_v + (int)(((uint32_t)val * (uint32_t)span + 63u) / 127u);
    return ri_sui_set(u, idx, v);
}

/* Note On — Appendix C "Various Switches" (p. 199, every mode). */
static int note_various(struct RIPanelUI *p, uint8_t n) {
    struct RISectUI *u;
    if (n >= 65u && n <= 68u) {              /* focus to Synth 1 / Synth 2 / 808 / 909 */
        if (p->focus == n - 65u)
            return 0;
        p->focus = (uint8_t)(n - 65u);
        return 1;
    }
    switch (n) {
    case 96: p->opts.select_patterns = (uint8_t)!p->opts.select_patterns; return 1;
    case 95: p->opts.program_synth = (uint8_t)!p->opts.program_synth; return 1;
    case 64:   /* "Select Pattern/Program Synth": swap which of the two is on (E0) */
        p->opts.select_patterns = (uint8_t)!p->opts.select_patterns;
        p->opts.program_synth = (uint8_t)!p->opts.select_patterns;
        return 1;
    case 69: return p->tr ? ri_sui_press(p->tr, RI_STR_PLAY) : 0;
    /* p. 199 prints "B3 70 Record" / "A#3 71 Stop": the key column runs in
     * order (C#4 C4 B3 A#3 A3), the number column does not — the numbers
     * are swapped. Keys win: A#3 = 70 = Stop, B3 = 71 = Record (E0). */
    case 70: return p->tr ? ri_sui_press(p->tr, RI_STR_STOP) : 0;
    case 71: return p->tr ? ri_sui_press(p->tr, RI_STR_RECORD) : 0;
    case 72: return p->tr ? ri_sui_step(p->tr, RI_STR_BAR, -1) : 0;
    case 73: return p->tr ? ri_sui_step(p->tr, RI_STR_BAR, 1) : 0;
    case 74: case 75: case 76: case 77:     /* PCF / Delay / Dist / Comp enable */
        u = p->fx[n - 74u];
        return u ? ri_sui_press(u, RI_SFX_ONOFF) : 0;
    case 94: return p->mix[4] ? ri_sui_press(p->mix[4], RI_SMST_COMP) : 0;
    default: break;
    }
    if (n >= 78u && n <= 93u) {              /* per section: Mix, Dist, PCF, Comp */
        static const uint8_t sw[4] = { RI_SMIX_ONOFF, RI_SMIX_DIST, RI_SMIX_PCF, RI_SMIX_COMP };
        u = p->mix[(n - 78u) / 4u];
        return u ? ri_sui_press(u, sw[(n - 78u) % 4u]) : 0;
    }
    return 0;
}

/* Note On — Pattern Selection (p. 200–201): 13 notes per section from 12:
 * section on/off, Bank A-D, Pattern 1-8. */
static int note_pattern(struct RIPanelUI *p, uint8_t n) {
    uint32_t s = (uint32_t)(n - 12u) / 13u, k = (uint32_t)(n - 12u) % 13u;
    struct RISectUI *u = p->pat[s];
    int ch;
    if (!u)
        return 0;
    if (k == 0u)
        return ri_sui_press(u, RI_SPAT_OFF);
    if (k <= 4u)
        return ri_sui_set(u, RI_SPAT_BANK, (int)k - 1);
    ch = ri_sui_set(u, RI_SPAT_PATTERN, (int)k - 5);
    return ri_panel_pattern_selected(p, RI_SEC_PAT_SYNTH1 + s) | ch;
}

/* Note On — the focused section's switches (p. 202–204). */
static int note_section(struct RIPanelUI *p, uint8_t n) {
    uint32_t f = p->focus;
    if (f <= RI_FOCUS_SYNTH2) {
        static const uint8_t sw[8] = { RI_S303_DOWN, RI_S303_UP, RI_S303_ACCENT, RI_S303_SLIDE,
            RI_S303_NOTEPAUSE, RI_S303_BACK, RI_S303_STEP, RI_S303_PITCHMODE };
        struct RISectUI *u = p->synth[f];
        if (!u || n > 32u)
            return 0;
        return ri_sui_press(u, n <= 24u ? RI_S303_KEY0 + (uint32_t)(n - 12u) : sw[n - 25u]);
    } else {
        struct RISectUI *u = p->drum[f - RI_FOCUS_808];
        uint32_t step0 = f == RI_FOCUS_808 ? RI_S808_STEP0 : RI_S909_STEP0;
        uint32_t sel = f == RI_FOCUS_808 ? RI_S808_SELECT : RI_S909_SELECT;
        if (!u || n > 39u)
            return 0;
        if (n <= 27u)                        /* Step 1..16 on/off = a step click */
            return ri_sui_press(u, step0 + (uint32_t)(n - 12u));
        return ri_sui_set(u, sel, n - 28);   /* Instrument AC .. (Selection order) */
    }
}

static int note_on(struct RIPanelUI *p, uint8_t n) {
    if (n >= 64u)
        return note_various(p, n);
    if (n < 12u)
        return 0;
    /* Same precedence as the typewriter keys (docs/evidence/gui/keyboard.md):
     * the focused section's switches win the overlap with pattern notes. */
    if (p->opts.program_synth && ((p->focus <= RI_FOCUS_SYNTH2 && n <= 32u) || (p->focus >= RI_FOCUS_808 && n <= 39u)))
        return note_section(p, n);
    if (p->opts.select_patterns)
        return note_pattern(p, n);
    return 0;
}

/* Sync LED from MIDI clock (p. 145): red on the downbeat, green on the
 * other beats, each lit for the first quarter of its beat (E0). 4/4. */
static int sync_clock(struct RIMidiIn *m, struct RIPanelUI *p) {
    uint32_t c = m->clocks++;
    if (c % 24u == 0u)
        return led(p, RI_STR_SYNC, c % 96u == 0u ? 1 : 2);
    if (c % 24u == 6u)
        return led(p, RI_STR_SYNC, 0);
    return 0;
}

int ri_midi_msg(struct RIMidiIn *m, struct RIPanelUI *p, uint8_t status, uint8_t d1, uint8_t d2) {
    int ch = 0;
    if (!m || !p || status < 0x80u)
        return 0;
    if (status == 0xF0u || status == 0xF7u)  /* SysEx: no LED (p. 144) */
        return 0;
    m->messages++;
    m->midi_led_ms = RI_MIDI_LED_MS;
    ch |= led(p, RI_STR_MIDI, 1);
    if (status >= 0xF8u) {                   /* realtime */
        if (status == 0xFAu)                 /* Start: count from the downbeat */
            m->clocks = 0;
        if (status == 0xF8u)
            ch |= sync_clock(m, p);
        if (status == 0xFCu)
            ch |= led(p, RI_STR_SYNC, 0);
        if (ch)
            p->changes++;
        return ch;
    }
    if (status >= 0xF0u || (status & 15u) != m->channel) {  /* other channels ignored (p. 134) */
        m->ignored++;
        if (ch)
            p->changes++;
        return ch;
    }
    d1 &= 0x7Fu;
    d2 &= 0x7Fu;
    switch (status & 0xF0u) {
    case 0xB0: ch |= cc(p, d1, d2); break;
    case 0x90: if (d2) ch |= note_on(p, d1); break;   /* velocity 0 = note off */
    default: m->ignored++; break;
    }
    if (ch)
        p->changes++;
    return ch;
}

static uint32_t data_len(uint8_t status) {
    switch (status & 0xF0u) {
    case 0xC0: case 0xD0: return 1u;
    case 0xF0: return status == 0xF1u || status == 0xF3u ? 1u : status == 0xF2u ? 2u : 0u;
    default: return 2u;
    }
}

int ri_midi_byte(struct RIMidiIn *m, struct RIPanelUI *p, uint8_t b) {
    if (!m)
        return 0;
    if (b >= 0xF8u)                          /* realtime may interleave anywhere */
        return ri_midi_msg(m, p, b, 0, 0);
    if (b == 0xF0u) {
        m->in_sysex = 1;
        m->status = 0;
        return 0;
    }
    if (b == 0xF7u) {
        m->in_sysex = 0;
        return 0;
    }
    if (m->in_sysex)
        return 0;
    if (b & 0x80u) {                         /* new status */
        m->status = b >= 0xF0u ? 0u : b;     /* system common cancels running status */
        m->ndata = 0;
        if (b >= 0xF0u && data_len(b) == 0u)
            return ri_midi_msg(m, p, b, 0, 0);
        if (b >= 0xF0u)
            m->status = b;
        return 0;
    }
    if (!m->status)
        return 0;                            /* stray data byte */
    m->data[m->ndata++] = b;
    if (m->ndata < data_len(m->status))
        return 0;
    m->ndata = 0;
    {
        uint8_t st = m->status;
        if (st >= 0xF0u)
            m->status = 0;                   /* no running status for system common */
        return ri_midi_msg(m, p, st, m->data[0], data_len(st) > 1u ? m->data[1] : 0);
    }
}

int ri_midi_elapse(struct RIMidiIn *m, struct RIPanelUI *p, uint32_t ms) {
    if (!m || !p || !m->midi_led_ms)
        return 0;
    if (ms < m->midi_led_ms) {
        m->midi_led_ms = (uint16_t)(m->midi_led_ms - ms);
        return 0;
    }
    m->midi_led_ms = 0;
    if (!led(p, RI_STR_MIDI, 0))
        return 0;
    p->changes++;
    return 1;
}
