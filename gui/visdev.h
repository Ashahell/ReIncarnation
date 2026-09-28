/* visdev.h — visible classic devices for tabbed panels (owner 2026-09-27).
 * Pure C, host-tested. The visible set drives RIAPP tab pages; device
 * names come from ri_panel_get (single source with the wiring). Mixer,
 * FX, transport and patterns are always-visible frame (not devices).
 * Grows into the extensible-rack visibility model (Korg/Leviasynth later).
 */
#ifndef RI_VISDEV_H
#define RI_VISDEV_H
#include <stdint.h>

#define RI_VIS_MAX 5u /* 303A 303B 808 909 Levi */

struct RIVisSet {
    uint8_t vis[RI_VIS_MAX]; /* 0 hidden, 1 shown */
};

void ri_vis_init(struct RIVisSet *s); /* all shown */
int ri_vis_set(struct RIVisSet *s, uint32_t dev, int show); /* 0 ok, 2 bad */
int ri_vis_get(const struct RIVisSet *s, uint32_t dev); /* -1 bad, else 0/1 */
uint32_t ri_vis_count(const struct RIVisSet *s); /* shown devices */
/* Tab titles for shown devices, device order, into out[] (up to cap).
 * Returns titles written. */
uint32_t ri_vis_titles(const struct RIVisSet *s, const char **out, uint32_t cap);

#endif
