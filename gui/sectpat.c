/* gui/sectpat.c — Pattern section front-panel behaviour (§12.10 G3/G4). */
#include "gui/sectpat.h"
#include "gui/ctlreg.h"

static int ispat(uint32_t section) {
    return section >= RI_SEC_PAT_SYNTH1 && section <= RI_SEC_PAT_909;
}

int ri_spat_init(struct RISectPat *s, uint8_t section) {
    uint32_t i;
    if (!s || !ispat(section))
        return 2;
    s->section = section;
    s->off = s->bank = s->pattern = s->armed_bank = s->shuffle = 0;
    for (i = 0; i < 32; i++)
        s->length[i] = 16;
    return 0;
}

int ri_spat_selected(const struct RISectPat *s) {
    return s ? s->bank * 8 + s->pattern : 0;
}

int ri_spat_press(struct RISectPat *s, uint32_t idx) {
    if (!s || !ispat(s->section))
        return 0;
    if (idx == RI_SPAT_OFF) {
        s->off = (uint8_t)!s->off;
        return 1;
    }
    if (idx == RI_SPAT_SHUFFLE) {
        s->shuffle = (uint8_t)!s->shuffle;
        return 1;
    }
    return 0;
}

int ri_spat_set_value(struct RISectPat *s, uint32_t idx, int v) {
    if (!s || !ispat(s->section))
        return 0;
    if (idx == RI_SPAT_BANK) {
        if (v < 0 || v > 3 || v == s->armed_bank)
            return 0;
        s->armed_bank = (uint8_t)v;
        return 1;
    }
    if (idx == RI_SPAT_PATTERN) {
        if (v < 0 || v > 7 || (v == s->pattern && s->bank == s->armed_bank))
            return 0;
        s->bank = s->armed_bank;
        s->pattern = (uint8_t)v;
        return 1;
    }
    if (idx == RI_SPAT_LENGTH) {
        uint8_t *len = &s->length[ri_spat_selected(s)];
        v = v < 1 ? 1 : v > 16 ? 16 : v;
        if (*len == v)
            return 0;
        *len = (uint8_t)v;
        return 1;
    }
    return 0;
}

int ri_spat_reset(struct RISectPat *s, uint32_t idx) {
    return idx == RI_SPAT_LENGTH ? ri_spat_set_value(s, idx, 16) : 0;
}

int ri_spat_step(struct RISectPat *s, uint32_t idx, int dir) {
    if (!s || idx != RI_SPAT_LENGTH || !dir)
        return 0;
    return ri_spat_set_value(s, idx, s->length[ri_spat_selected(s)] + (dir > 0 ? 1 : -1));
}

int ri_spat_value(const struct RISectPat *s, uint32_t idx) {
    if (!s || !ispat(s->section))
        return 0;
    switch (idx) {
    case RI_SPAT_OFF: return s->off;
    case RI_SPAT_BANK: return s->armed_bank;
    case RI_SPAT_PATTERN: return s->bank == s->armed_bank ? s->pattern : -1;
    case RI_SPAT_LENGTH: return s->length[ri_spat_selected(s)];
    case RI_SPAT_SHUFFLE: return s->shuffle;
    default: return 0;
    }
}

int ri_spat_led(const struct RISectPat *s, uint32_t idx) {
    if (!s || !ispat(s->section))
        return 0;
    return idx == RI_SPAT_OFF ? !s->off : idx == RI_SPAT_SHUFFLE ? s->shuffle : 0;
}
