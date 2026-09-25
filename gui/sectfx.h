/* gui/sectfx.h — PCF / Delay / Dist / Comp front-panel behaviour
 * (§12.10 G3/G4). Pure C, host-tested. ReBirth 2.0.1 Owner's Manual
 * p. 159-164: each unit has an on/off (bypass) lamp button and an input
 * meter; PCF: Pattern display (arrows), LP/BP Mode switch, Freq/Q/Amt/
 * Decay sliders; Delay: Steps display (arrows), 16th/8th-triplet switch,
 * Pan and F.Back knobs; Dist: Amount, Shape; Comp: Ratio, Threshold and a
 * Level Reduction meter. Value displays step by one per arrow click and
 * stop at their ends (p. 18). Which section feeds a unit is set in the
 * mixers (gui/sectmix.h). Index = registry index within the section.
 */
#ifndef RI_SECTFX_H
#define RI_SECTFX_H
#include <stdint.h>

#define RI_SFX_ONOFF 0u
#define RI_SFX_METER 1u
#define RI_SFX_PCF_PATTERN 2u
#define RI_SFX_PCF_MODE 3u      /* 0 = LP, 1 = BP */
#define RI_SFX_DLY_STEPS 2u
#define RI_SFX_DLY_TRIPLET 3u   /* 0 = 16ths, 1 = 8th triplets */
#define RI_SFX_COMP_GR 4u       /* Level Reduction meter */
#define RI_SFX_NCTL 8u

struct RISectFx {
    uint8_t section;            /* RI_SEC_PCF / DELAY / DIST / COMP */
    uint8_t pad[3];
    int16_t val[RI_SFX_NCTL];   /* meters included: fed by the engine */
};

int ri_sfx_init(struct RISectFx *s, uint8_t section); /* 0 ok, 2 not an FX section */
int ri_sfx_press(struct RISectFx *s, uint32_t idx);
int ri_sfx_set_value(struct RISectFx *s, uint32_t idx, int v);
int ri_sfx_reset(struct RISectFx *s, uint32_t idx);
/* Arrow button of a value display: dir > 0 up, < 0 down; clamps. */
int ri_sfx_step(struct RISectFx *s, uint32_t idx, int dir);
int ri_sfx_led(const struct RISectFx *s, uint32_t idx);
void ri_sfx_meter_set(struct RISectFx *s, uint32_t idx, int level);
#endif
