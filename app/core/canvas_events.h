/* canvas_events.h — backend-free canvas event logic (portability plan T3b).
 * Extracted from gui/widgets/rsection.mcc.c HandleEvent: hit test, drag
 * accumulation with the 150-px law, arrow repeat on ticks, key routing.
 * Pure C99, host-tested with synthetic events. The AROS MCC shell maps
 * IDCMP messages onto these calls; other backends map their own events.
 */
#ifndef RI_CANVAS_EVENTS_H
#define RI_CANVAS_EVENTS_H
#include <stdint.h>

#include "gui/sectui.h"
#include "gui/panelui.h"
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"

#define RI_CEV_NODRAG 0xFFFFu

struct RICevState {
    uint16_t drag_id;   /* active drag target or RI_CEV_NODRAG */
    double acc_dx, acc_dy;
    int last_x, last_y;
    long drag_n0;       /* 0..127 start value */
    uint16_t rep_idx;   /* arrow repeat target or RI_CEV_NODRAG */
    int8_t rep_dir;
    uint32_t rep_ticks;
};

void ri_cev_init(struct RICevState *st);

/* Results: caller redraws on CHANGED, consumes on EAT. */
#define RI_CEV_CHANGED 1u
#define RI_CEV_EAT 2u

/* Intuiticks repeat (p. 18): held arrows step every 4th tick. */
uint32_t ri_cev_tick(struct RICevState *st, struct RISectUI *ui);

/* Raw key: decode + route through the panel. key_owner mirrors the MCC's
 * one-owner-per-window rule; 0 means "not ours, leave to the system". */
uint32_t ri_cev_key(struct RICevState *st, struct RISectUI *ui,
    struct RIPanelUI *panel, int key_owner, uint32_t code, uint32_t qual);

/* Pointer button. kind: 0 select-down, 1 select-up, 2 menu-down.
 * lx/ly are canvas-relative; w/h the canvas size (focus click is
 * inside-tested like the MCC). Returns CHANGED/EAT. */
uint32_t ri_cev_button(struct RICevState *st, struct RISectUI *ui,
    struct RIPanelUI *panel, const struct RIGeoSection *geo, int zoom,
    int lx, int ly, int w, int h, int kind);

/* Pointer motion while a drag is active. shift = fine mode. */
uint32_t ri_cev_move(struct RICevState *st, struct RISectUI *ui,
    int x, int y, int shift);

#endif
