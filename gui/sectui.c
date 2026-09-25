/* gui/sectui.c — section behaviour dispatch (§12.10 G4). */
#include "gui/sectui.h"
#include "gui/ctlreg.h"

static int is303(const struct RISectUI *s) {
    return s->section == RI_SEC_SYNTH1 || s->section == RI_SEC_SYNTH2;
}

int ri_sui_init(struct RISectUI *s, uint8_t section) {
    if (!s)
        return 2;
    s->section = section;
    s->pad[0] = s->pad[1] = s->pad[2] = 0;
    if (section == RI_SEC_SYNTH1 || section == RI_SEC_SYNTH2)
        return ri_s303_init(&s->u.s303, section);
    if (section == RI_SEC_808)
        return ri_s808_init(&s->u.s808);
    if (section == RI_SEC_909)
        return ri_s909_init(&s->u.s909);
    return 2;
}

int ri_sui_press(struct RISectUI *s, uint32_t idx) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_press(&s->u.s303, idx)
         : s->section == RI_SEC_808 ? ri_s808_press(&s->u.s808, idx)
         : s->section == RI_SEC_909 ? ri_s909_press(&s->u.s909, idx) : 0;
}

int ri_sui_set(struct RISectUI *s, uint32_t idx, int v) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_set_value(&s->u.s303, idx, v)
         : s->section == RI_SEC_808 ? ri_s808_set_value(&s->u.s808, idx, v)
         : s->section == RI_SEC_909 ? ri_s909_set_value(&s->u.s909, idx, v) : 0;
}

int ri_sui_reset(struct RISectUI *s, uint32_t idx) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_reset(&s->u.s303, idx)
         : s->section == RI_SEC_808 ? ri_s808_reset(&s->u.s808, idx)
         : s->section == RI_SEC_909 ? ri_s909_reset(&s->u.s909, idx) : 0;
}

int ri_sui_value(const struct RISectUI *s, uint32_t idx) {
    if (!s)
        return 0;
    if (is303(s))
        return idx < RI_S303_NCTL ? s->u.s303.val[idx] : 0;
    if (s->section == RI_SEC_808)
        return idx < RI_S808_NCTL ? s->u.s808.val[idx] : 0;
    if (s->section == RI_SEC_909)
        return idx < RI_S909_NCTL ? s->u.s909.val[idx] : 0;
    return 0;
}

int ri_sui_led(const struct RISectUI *s, uint32_t idx, uint32_t which) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_led(&s->u.s303, idx, which)
         : s->section == RI_SEC_808 ? ri_s808_led(&s->u.s808, idx)
         : s->section == RI_SEC_909 ? ri_s909_led(&s->u.s909, idx) : 0;
}

int ri_sui_display(const struct RISectUI *s, uint32_t idx) {
    if (s && is303(s) && idx == RI_S303_DISPLAY)
        return ri_s303_display(&s->u.s303);
    return 0;
}
