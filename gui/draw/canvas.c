/* canvas.c — display-list bodies (portability plan T2). */
#include "gui/draw/canvas.h"

#include <string.h>

void ri_dlist_init(struct ri_dlist *dl, struct ri_dcmd *backing, uint32_t cap,
    char *spool, uint32_t spcap) {
    if (!dl)
        return;
    dl->cmd = backing;
    dl->n = 0u;
    dl->cap = (backing && cap) ? cap : 0u;
    dl->spool = spool;
    dl->spn = 0u;
    dl->spcap = (spool && spcap) ? spcap : 0u;
}

void ri_dlist_clear(struct ri_dlist *dl) {
    if (!dl)
        return;
    dl->n = 0u;
    dl->spn = 0u;
}

int ri_dlist_push(struct ri_dlist *dl, const struct ri_dcmd *c) {
    if (!dl || !c || !dl->cmd || dl->n >= dl->cap)
        return 1;
    dl->cmd[dl->n++] = *c;
    return 0;
}

static int emit(struct ri_dlist *dl, uint8_t op, int x0, int y0, int x1, int y1,
    uint32_t rgb, uint16_t img, uint16_t frame, uint8_t align, const char *text) {
    struct ri_dcmd c;
    c.op = op;
    c.align = align;
    c.pad[0] = c.pad[1] = 0;
    c.x0 = (int16_t)x0;
    c.y0 = (int16_t)y0;
    c.x1 = (int16_t)x1;
    c.y1 = (int16_t)y1;
    c.rgb = rgb;
    c.img = img;
    c.frame = frame;
    c.text = text;
    return ri_dlist_push(dl, &c);
}

int ri_draw_rect(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t rgb) {
    if (x1 < x0 || y1 < y0)
        return 0; /* degenerate: skip like fill_rect */
    return emit(dl, RI_D_RECT, x0, y0, x1, y1, rgb, 0u, 0u, 0u, 0);
}

int ri_draw_line(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t rgb) {
    return emit(dl, RI_D_LINE, x0, y0, x1, y1, rgb, 0u, 0u, 0u, 0);
}

int ri_draw_circle(struct ri_dlist *dl, int cx, int cy, int r, uint32_t rgb) {
    if (r < 0)
        return 0;
    return emit(dl, RI_D_CIRCLE, cx, cy, r, 0, rgb, 0u, 0u, 0u, 0);
}

int ri_draw_text(struct ri_dlist *dl, int x, int y, uint8_t align, uint32_t rgb, const char *text) {
    uint32_t len = 0u;
    const char *dst;
    if (!dl || !text || !text[0])
        return 0;
    while (text[len] && len < 64u)
        len++;
    if (!dl->spool || dl->spn + len + 1u > dl->spcap)
        return 1; /* no pool room: caller sizes it (test pins sizes) */
    dst = dl->spool + dl->spn;
    memcpy(dl->spool + dl->spn, text, len + (text[len] ? 0u : 1u));
    if (text[len])
        dl->spool[dl->spn + len] = 0;
    dl->spn += len + 1u;
    return emit(dl, RI_D_TEXT, x, y, 0, 0, rgb, 0u, 0u, align, dst);
}

int ri_draw_image(struct ri_dlist *dl, int x, int y, uint16_t img, uint16_t frame) {
    return emit(dl, RI_D_IMAGE, x, y, 0, 0, 0u, img, frame, 0u, 0);
}

int ri_draw_clip(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    return emit(dl, RI_D_CLIP, x0, y0, x1, y1, 0u, 0u, 0u, 0u, 0);
}

uint32_t ri_dlist_hash(const struct ri_dlist *dl) {
    uint32_t h = 2166136261u, i, k;
    const unsigned char *p;
    if (!dl || !dl->cmd)
        return h;
    for (i = 0u; i < dl->n; i++) {
        /* Hash the value fields, not padding/pointers (text hashed by bytes). */
        uint32_t words[6];
        words[0] = ((uint32_t)dl->cmd[i].op << 24) | ((uint32_t)dl->cmd[i].align << 16) |
            ((uint32_t)(uint16_t)dl->cmd[i].x0);
        words[1] = ((uint32_t)(uint16_t)dl->cmd[i].y0 << 16) | (uint32_t)(uint16_t)dl->cmd[i].x1;
        words[2] = (uint32_t)(uint16_t)dl->cmd[i].y1;
        words[3] = dl->cmd[i].rgb;
        words[4] = ((uint32_t)dl->cmd[i].img << 16) | dl->cmd[i].frame;
        words[5] = 0u;
        p = (const unsigned char *)words;
        for (k = 0u; k < sizeof words; k++) {
            h ^= p[k];
            h *= 16777619u;
        }
        if (dl->cmd[i].op == RI_D_TEXT && dl->cmd[i].text) {
            const char *s = dl->cmd[i].text;
            while (*s) {
                h ^= (unsigned char)*s++;
                h *= 16777619u;
            }
        }
    }
    return h;
}
