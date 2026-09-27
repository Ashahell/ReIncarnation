/* visdev.c — visible-device bodies (owner 2026-09-27). */
#include "gui/visdev.h"
#include "gui/panels.h"

void ri_vis_init(struct RIVisSet *s) {
    uint32_t i;
    if (!s)
        return;
    for (i = 0; i < RI_VIS_MAX; i++)
        s->vis[i] = 1u;
}

int ri_vis_set(struct RIVisSet *s, uint32_t dev, int show) {
    if (!s || dev >= RI_VIS_MAX)
        return 2;
    s->vis[dev] = show ? 1u : 0u;
    return 0;
}

int ri_vis_get(const struct RIVisSet *s, uint32_t dev) {
    if (!s || dev >= RI_VIS_MAX)
        return -1;
    return (int)s->vis[dev];
}

uint32_t ri_vis_count(const struct RIVisSet *s) {
    uint32_t i, n = 0u;
    if (!s)
        return 0u;
    for (i = 0; i < RI_VIS_MAX; i++)
        n += s->vis[i] ? 1u : 0u;
    return n;
}

uint32_t ri_vis_titles(const struct RIVisSet *s, const char **out, uint32_t cap) {
    uint32_t i, n = 0u;
    if (!s || !out)
        return 0u;
    for (i = 0; i < RI_VIS_MAX; i++) {
        const struct RIPanelDesc *d;
        if (!s->vis[i] || n >= cap)
            continue;
        d = ri_panel_get(i);
        if (!d || !d->name)
            continue;
        out[n++] = d->name;
    }
    return n;
}
