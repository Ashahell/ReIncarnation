/* gui/sectmix.c — mixers + master front-panel behaviour (§12.10 G3/G4). */
#include "gui/sectmix.h"
#include "gui/ctlreg.h"

static const struct RICtlDef *def(uint32_t section, uint32_t idx) {
    return ri_smix_strip(section) >= 0 && idx < 0xFFu ? ri_ctlreg_find((uint16_t)((section << 8) | idx)) : 0;
}

int ri_smix_strip(uint32_t section) {
    if (section >= RI_SEC_MIX_SYNTH1 && section <= RI_SEC_MIX_909)
        return (int)(section - RI_SEC_MIX_SYNTH1);
    return section == RI_SEC_MASTER ? 4 : -1;
}

/* insert unit behind a switch, or -1 */
static int unit_of(uint32_t section, uint32_t idx) {
    if (section == RI_SEC_MASTER)
        return idx == RI_SMST_COMP ? (int)RI_ROUTE_COMP : -1;
    return idx == RI_SMIX_DIST ? (int)RI_ROUTE_DIST : idx == RI_SMIX_PCF ? (int)RI_ROUTE_PCF
         : idx == RI_SMIX_COMP ? (int)RI_ROUTE_COMP : -1;
}

void ri_smix_init(struct RIMixBoard *b) {
    uint32_t s, i;
    if (!b)
        return;
    for (s = 0; s < RI_SMIX_NSTRIPS; s++) {
        uint32_t sec = s < 4 ? RI_SEC_MIX_SYNTH1 + s : RI_SEC_MASTER;
        for (i = 0; i < RI_SMIX_NCTL; i++) {
            const struct RICtlDef *d = def(sec, i);
            b->val[s][i] = d ? d->def_v : 0;
        }
        b->meter[s][0] = b->meter[s][1] = 0;
    }
    ri_route_init(&b->route);
}

int ri_smix_press(struct RIMixBoard *b, uint32_t section, uint32_t idx) {
    const struct RICtlDef *d = b ? def(section, idx) : 0;
    int strip = ri_smix_strip(section), unit;
    if (!d || d->kind != RI_CK_SWITCH)
        return 0;
    unit = unit_of(section, idx);
    if (unit >= 0) {       /* radio: on here steals it; on again releases it */
        int own = ri_route_owner(&b->route, (uint32_t)unit) == strip;
        return ri_route_assign(&b->route, (uint32_t)unit, own ? RI_ROUTE_NONE : strip) != -2;
    }
    b->val[strip][idx] = (int16_t)(b->val[strip][idx] ? 0 : 1);
    return 1;
}

int ri_smix_set_value(struct RIMixBoard *b, uint32_t section, uint32_t idx, int v) {
    const struct RICtlDef *d = b ? def(section, idx) : 0;
    int strip = ri_smix_strip(section);
    if (!d || (d->kind != RI_CK_KNOB && d->kind != RI_CK_FADER))
        return 0;
    if (v < d->min_v)
        v = d->min_v;
    if (v > d->max_v)
        v = d->max_v;
    if (b->val[strip][idx] == v)
        return 0;
    b->val[strip][idx] = (int16_t)v;
    return 1;
}

int ri_smix_reset(struct RIMixBoard *b, uint32_t section, uint32_t idx) {
    const struct RICtlDef *d = b ? def(section, idx) : 0;
    return d ? ri_smix_set_value(b, section, idx, d->def_v) : 0;
}

int ri_smix_value(const struct RIMixBoard *b, uint32_t section, uint32_t idx) {
    const struct RICtlDef *d = b ? def(section, idx) : 0;
    int strip = ri_smix_strip(section);
    if (!d)
        return 0;
    if (d->kind == RI_CK_METER)
        return b->meter[strip][section == RI_SEC_MASTER && idx == RI_SMST_METER_R];
    if (unit_of(section, idx) >= 0)
        return ri_smix_led(b, section, idx);
    return b->val[strip][idx];
}

int ri_smix_led(const struct RIMixBoard *b, uint32_t section, uint32_t idx) {
    const struct RICtlDef *d = b ? def(section, idx) : 0;
    int unit = unit_of(section, idx);
    if (!d || d->kind != RI_CK_SWITCH)
        return 0;
    if (unit >= 0)
        return ri_route_owner(&b->route, (uint32_t)unit) == ri_smix_strip(section);
    return b->val[ri_smix_strip(section)][idx] != 0;
}

void ri_smix_meter_set(struct RIMixBoard *b, uint32_t section, uint32_t ch, int level) {
    int strip = ri_smix_strip(section);
    if (!b || strip < 0 || ch > 1)
        return;
    b->meter[strip][ch] = (uint8_t)(level < 0 ? 0 : level > 127 ? 127 : level);
}
