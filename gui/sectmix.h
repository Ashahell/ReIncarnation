/* gui/sectmix.h — mixers + master front-panel behaviour (§12.10 G3/G4).
 * Pure C, host-tested. ReBirth 2.0.1 Owner's Manual p. 23-24, 56-57,
 * 59-70, 157-158: each section mixer has an on/off (mute) button, an
 * output meter, a volume fader, Pan, Delay (send) and Dist/PCF/Comp insert
 * switches; the Master has a level fader, L/R meters and a Comp switch.
 * The four mixers and the master share ONE board: an insert unit has one
 * owner at a time and switching it on elsewhere steals it (manual p. 70
 * "only one section at a time can use the PCF", p. 67 Comp: one section or
 * the master). Dist follows the engine's radio routing (engine/fx/route.h;
 * the manual's p. 59 "four distortion units" text contradicts its own
 * p. 157 reference — see docs/evidence/gui/panel-geometry.md).
 * Values index = registry index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECTMIX_H
#define RI_SECTMIX_H
#include <stdint.h>
#include "engine/fx/route.h"

/* section mixer indices (RI_SEC_MIX_*) */
#define RI_SMIX_ONOFF 0u
#define RI_SMIX_METER 1u
#define RI_SMIX_LEVEL 2u
#define RI_SMIX_PAN 3u
#define RI_SMIX_DELAY 4u
#define RI_SMIX_DIST 5u
#define RI_SMIX_PCF 6u
#define RI_SMIX_COMP 7u
#define RI_SMIX_NCTL 8u
/* master indices (RI_SEC_MASTER) */
#define RI_SMST_LEVEL 0u
#define RI_SMST_METER_L 1u
#define RI_SMST_METER_R 2u
#define RI_SMST_COMP 3u
#define RI_SMST_NCTL 4u

#define RI_SMIX_NSTRIPS 6u /* 0..3 = 303A 303B 808 909 mixers, 4 = master, 5 = Levi */

struct RIMixBoard {
    int16_t val[RI_SMIX_NSTRIPS][RI_SMIX_NCTL]; /* switches 5..7 unused: route */
    uint8_t meter[RI_SMIX_NSTRIPS][2];          /* 0..127, fed by the engine */
    struct RIRoute route;
    /* Which section's insert lamp the LAST press displaced, or
     * RI_SMX_STALE_NONE. Insert routing is radio across strips and the strips
     * are separate canvases, so the loser gets no event of its own and its
     * lamp would freeze lit (owner 2026-10-14). Same shape as the FX rack's
     * `displaced` (P-FX4). */
    uint8_t stale;
};

void ri_smix_init(struct RIMixBoard *b);
/* Strip of a section (RI_SEC_MIX_* -> 0..3, RI_SEC_MASTER -> 4), -1 otherwise. */
int ri_smix_strip(uint32_t section);
/* Same contract as the other sections (return 1 when state changed). */
/* Sentinel for "no canvas owes a repaint". Not a RI_SEC_* value. */
#define RI_SMX_STALE_NONE 0xFFu
int ri_smix_press(struct RIMixBoard *b, uint32_t section, uint32_t idx);
/* Section whose insert lamp needs repainting after the last press, or
 * RI_SMX_STALE_NONE. READS AND CLEARS, so it fires once per displacement.
 * Portable deliberately: the only other home is app/riapp.c, which cannot link
 * on the host -- a decision placed there is untestable by construction, which
 * is exactly how mutate_fx_lamp m6 survived its first run. */
uint32_t ri_smix_stale_section(struct RIMixBoard *b);
int ri_smix_set_value(struct RIMixBoard *b, uint32_t section, uint32_t idx, int v);
int ri_smix_reset(struct RIMixBoard *b, uint32_t section, uint32_t idx);
int ri_smix_value(const struct RIMixBoard *b, uint32_t section, uint32_t idx);
/* On/off: 1 = on (sounding); insert switch: 1 = this strip owns the unit. */
int ri_smix_led(const struct RIMixBoard *b, uint32_t section, uint32_t idx);
/* Meter feed (display only; never automation): ch 0 = mono/L, 1 = R. */
void ri_smix_meter_set(struct RIMixBoard *b, uint32_t section, uint32_t ch, int level);
#endif
