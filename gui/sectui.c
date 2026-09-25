/* gui/sectui.c — section behaviour dispatch (§12.10 G4). */
#include "gui/sectui.h"
#include "gui/ctlreg.h"

static int is303(const struct RISectUI *s) {
    return s->section == RI_SEC_SYNTH1 || s->section == RI_SEC_SYNTH2;
}
static int isfx(const struct RISectUI *s) {
    return s->section >= RI_SEC_PCF && s->section <= RI_SEC_COMP;
}
static int ismix(const struct RISectUI *s) {
    return ri_smix_strip(s->section) >= 0;
}

int ri_sui_bind_board(struct RISectUI *s, struct RIMixBoard *b) {
    if (!s || !b || !ismix(s))
        return 2;
    s->u.mix.board = b;
    return 0;
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
    if (isfx(s))
        return ri_sfx_init(&s->u.fx, section);
    if (ismix(s)) {
        ri_smix_init(&s->u.mix.own);
        s->u.mix.board = &s->u.mix.own;
        return 0;
    }
    return 2;
}

int ri_sui_press(struct RISectUI *s, uint32_t idx) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_press(&s->u.s303, idx)
         : s->section == RI_SEC_808 ? ri_s808_press(&s->u.s808, idx)
         : s->section == RI_SEC_909 ? ri_s909_press(&s->u.s909, idx)
         : ismix(s) ? ri_smix_press(s->u.mix.board, s->section, idx)
         : isfx(s) ? ri_sfx_press(&s->u.fx, idx) : 0;
}

int ri_sui_set(struct RISectUI *s, uint32_t idx, int v) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_set_value(&s->u.s303, idx, v)
         : s->section == RI_SEC_808 ? ri_s808_set_value(&s->u.s808, idx, v)
         : s->section == RI_SEC_909 ? ri_s909_set_value(&s->u.s909, idx, v)
         : ismix(s) ? ri_smix_set_value(s->u.mix.board, s->section, idx, v)
         : isfx(s) ? ri_sfx_set_value(&s->u.fx, idx, v) : 0;
}

int ri_sui_reset(struct RISectUI *s, uint32_t idx) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_reset(&s->u.s303, idx)
         : s->section == RI_SEC_808 ? ri_s808_reset(&s->u.s808, idx)
         : s->section == RI_SEC_909 ? ri_s909_reset(&s->u.s909, idx)
         : ismix(s) ? ri_smix_reset(s->u.mix.board, s->section, idx)
         : isfx(s) ? ri_sfx_reset(&s->u.fx, idx) : 0;
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
    if (ismix(s))
        return ri_smix_value(s->u.mix.board, s->section, idx);
    if (isfx(s))
        return idx < RI_SFX_NCTL ? s->u.fx.val[idx] : 0;
    return 0;
}

int ri_sui_led(const struct RISectUI *s, uint32_t idx, uint32_t which) {
    if (!s)
        return 0;
    return is303(s) ? ri_s303_led(&s->u.s303, idx, which)
         : s->section == RI_SEC_808 ? ri_s808_led(&s->u.s808, idx)
         : s->section == RI_SEC_909 ? ri_s909_led(&s->u.s909, idx)
         : ismix(s) ? ri_smix_led(s->u.mix.board, s->section, idx)
         : isfx(s) ? ri_sfx_led(&s->u.fx, idx) : 0;
}

int ri_sui_step(struct RISectUI *s, uint32_t idx, int dir) {
    return s && isfx(s) ? ri_sfx_step(&s->u.fx, idx, dir) : 0;
}

int ri_sui_display(const struct RISectUI *s, uint32_t idx) {
    if (s && is303(s) && idx == RI_S303_DISPLAY)
        return ri_s303_display(&s->u.s303);
    return 0;
}
