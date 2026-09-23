/* gui/knob_blit.h — knob frame blit helpers (Module 2.9 art on-device).
 *
 * AROS-ONLY (cybergraphics). Frame computation stays in
 * gui/knob_art.h (host-tested); this header owns ONLY the device
 * handoff declarations. Must NEVER enter the host build.
 */
#ifndef RI_KNOB_BLIT_H
#define RI_KNOB_BLIT_H

#include <exec/types.h>
#include <stdint.h>

struct RastPort;

/* Blit one 80px frame (value) with top-left at (dx, dy); negative
 * dx/dy clip the source. Returns painted (alpha>0) pixel count
 * (>0), 0 on bad args, -1 no cybergraphics, -2 blit refused. */
int ri_knob_blit_one(struct RastPort *rp, int value, int dx, int dy);

/* Opaque filled rect (panel background, divider rules). Callers
 * stay in-window. Returns pixels painted, 0 on bad args,
 * -1 no cybergraphics, -2 blit refused. */
int ri_knob_panel_rect(struct RastPort *rp, int x, int y, int w, int h,
    uint32_t rgb);

#endif
