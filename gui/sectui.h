/* gui/sectui.h — one front-panel behaviour interface for every laid-out
 * section (§12.10 G4). Pure C, host-tested: the AROS canvas (RSection)
 * talks only to this, never to a section's own module. Index = registry
 * index within the section (reg_id & 0xFF).
 */
#ifndef RI_SECTUI_H
#define RI_SECTUI_H
#include <stdint.h>
#include "gui/sect303.h"
#include "gui/sect808.h"
#include "gui/sect909.h"
#include "gui/sectlevi.h"
#include "gui/sectlevi.h"
#include "gui/sectmix.h"
#include "gui/sectfx.h"
#include "gui/sectpat.h"
#include "gui/secttr.h"

struct RISectUI {
    uint8_t section;  /* RI_SEC_* */
    uint8_t pad[3];
    union {
        struct RISect303 s303;
        struct RISect808 s808;
        struct RISect909 s909;
        struct RISectLevi slevi;
        struct RISectFx fx;       /* RI_SEC_PCF..RI_SEC_COMP */
        struct RISectPat pat;     /* RI_SEC_PAT_* */
        struct RISectTr tr;       /* RI_SEC_TRANSPORT */
        struct {                  /* RI_SEC_MIX_* / RI_SEC_MASTER */
            struct RIMixBoard *board; /* shared board (own unless bound) */
            struct RIMixBoard own;
        } mix;
    } u;
};

int ri_sui_init(struct RISectUI *s, uint8_t section); /* 0 ok, 2 not laid out */
/* Mixers/master: share one board across canvases (insert radio routing
 * spans sections). Returns 0 ok, 2 when s is not a mixer/master. */
int ri_sui_bind_board(struct RISectUI *s, struct RIMixBoard *b);
int ri_sui_press(struct RISectUI *s, uint32_t idx);
/* Tap tempo (P9e): the one press that needs a wall clock, so the clock is
 * passed in at the press instead of living in the panel state. ms == 0 means
 * no clock and the tap is ignored. 1 when the tempo moved. */
int ri_sui_tap(struct RISectUI *s, uint32_t idx, uint32_t ms);
int ri_sui_set(struct RISectUI *s, uint32_t idx, int v);
int ri_sui_reset(struct RISectUI *s, uint32_t idx);
/* Arrow button of a value display (dir > 0 up); 0 when not applicable. */
int ri_sui_step(struct RISectUI *s, uint32_t idx, int dir);
int ri_sui_value(const struct RISectUI *s, uint32_t idx);
int ri_sui_led(const struct RISectUI *s, uint32_t idx, uint32_t which);
/* numeric readout of a DISPLAY control (303 EDIT STEP 1..16); 0 otherwise */
int ri_sui_display(const struct RISectUI *s, uint32_t idx);
/* Control index to send for a hit on idx: a page encoder (Levi master
 * control) sends its target parameter; everything else sends idx. */
uint32_t ri_sui_ctl_idx(const struct RISectUI *s, uint32_t idx);
/* Explicit control key for a hit (Levi per-oscillator params, fidelity
 * P2): 1 with key and val set, else 0 (send ri_sui_ctl_idx through the
 * registry as usual). */
int ri_sui_ctl_key(const struct RISectUI *s, uint32_t idx, uint16_t *key, int *val);
#endif
