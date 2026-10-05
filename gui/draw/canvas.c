/* canvas.c — display-list bodies (portability plan T2). */
#include "gui/draw/canvas.h"

#include <string.h>
#include "gui/draw/font_legend.h"

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
    dl->cur_face = 0u;
    dl->clip = 0u;
    dl->cx0 = dl->cy0 = dl->cx1 = dl->cy1 = 0;
    dl->_rfu[0] = dl->_rfu[1] = dl->_rfu[2] = 0u;
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
    if (dl->clip && !ri_dcmd_hits_box(c, dl->cx0, dl->cy0, dl->cx1, dl->cy1))
        return 0; /* cannot paint inside the damage box: build never sees it */
    dl->cmd[dl->n++] = *c;
    return 0;
}

void ri_dlist_set_clip(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    if (!dl)
        return;
    if (x1 < x0 || y1 < y0) {   /* an empty box can paint nothing */
        dl->clip = 0u;
        return;
    }
    dl->cx0 = (int16_t)x0;
    dl->cy0 = (int16_t)y0;
    dl->cx1 = (int16_t)x1;
    dl->cy1 = (int16_t)y1;
    dl->clip = 1u;
}

void ri_dlist_clear_clip(struct ri_dlist *dl) {
    if (dl)
        dl->clip = 0u;
}

int ri_dlist_band_missed(const struct ri_dlist *dl, int y0, int y1) {
    if (!dl || !dl->clip)
        return 0;
    /* Disjoint in Y is sufficient on its own: a band is only a bound, and a
     * band that cannot overlap the box vertically cannot overlap it at all. The
     * caller is free to be more precise (a key also has an X extent) and should
     * be -- this is the cheap half. */
    return y1 < dl->cy0 || y0 > dl->cy1;
}

static int emit(struct ri_dlist *dl, uint8_t op, int x0, int y0, int x1, int y1,
    uint32_t rgb, uint16_t img, uint16_t frame, uint8_t align, uint8_t face, const char *text) {
    struct ri_dcmd c;
    c.op = op;
    c.align = align;
    c.pad[0] = face;
    c.pad[1] = 0;
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
    return emit(dl, RI_D_RECT, x0, y0, x1, y1, rgb, 0u, 0u, 0u, 0u, 0);
}

int ri_draw_line(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t rgb) {
    return emit(dl, RI_D_LINE, x0, y0, x1, y1, rgb, 0u, 0u, 0u, 0u, 0);
}

int ri_draw_circle(struct ri_dlist *dl, int cx, int cy, int r, uint32_t rgb) {
    if (r < 0)
        return 0;
    return emit(dl, RI_D_CIRCLE, cx, cy, r, 0, rgb, 0u, 0u, 0u, 0u, 0);
}

int ri_draw_text(struct ri_dlist *dl, int x, int y, uint8_t align, uint32_t rgb, const char *text) {
    return ri_draw_text_face(dl, x, y, align, rgb, 0, text);
}

int ri_draw_text_face(struct ri_dlist *dl, int x, int y, uint8_t align, uint32_t rgb, int face,
    const char *text) {
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
    if (face < 0 || face > 3)
        face = 0;
    return emit(dl, RI_D_TEXT, x, y, 0, 0, rgb, 0u, 0u, align, (uint8_t)face, dst);
}

void ri_dlist_set_face(struct ri_dlist *dl, int face) {
    if (!dl)
        return;
    dl->cur_face = (uint8_t)(face >= 1 && face <= 3 ? face : 0);
}

int ri_dcmd_bbox(const struct ri_dcmd *c, int *x0, int *y0, int *x1, int *y1) {
    if (!c || !x0 || !y0 || !x1 || !y1)
        return 2;
    switch (c->op) {
    case RI_D_RECT:
        *x0 = c->x0 < c->x1 ? c->x0 : c->x1;
        *y0 = c->y0 < c->y1 ? c->y0 : c->y1;
        *x1 = c->x0 < c->x1 ? c->x1 : c->x0;
        *y1 = c->y0 < c->y1 ? c->y1 : c->y0;
        return 0;
    case RI_D_LINE:
        /* 1-px strokes on both backends; grow one for raster rounding. */
        *x0 = (c->x0 < c->x1 ? c->x0 : c->x1) - 1;
        *y0 = (c->y0 < c->y1 ? c->y0 : c->y1) - 1;
        *x1 = (c->x0 < c->x1 ? c->x1 : c->x0) + 1;
        *y1 = (c->y0 < c->y1 ? c->y1 : c->y0) + 1;
        return 0;
    case RI_D_CIRCLE:
        *x0 = (int)c->x0 - (int)c->x1;
        *y0 = (int)c->y0 - (int)c->x1;
        *x1 = (int)c->x0 + (int)c->x1;
        *y1 = (int)c->y0 + (int)c->x1;
        return 0;
    case RI_D_TEXT: {
        uint32_t len = 0u;
        int w, top, bot;
        if (!c->text)
            return 2;
        while (c->text[len] && len < 64u)
            len++;
        if (!len)
            return 2;
        if (c->pad[0] >= 1 && c->pad[0] <= 3) {
            const struct ri_face *f = ri_face_by_id(c->pad[0]);
            int base;
            if (!f)
                return 2;
            w = ri_face_width(f, c->text);
            base = (int)c->y0 + (int)f->cap / 2;
            top = base - (int)f->asc;
            bot = base + (int)f->desc - 1;
        } else {
            /* System font (AROS TextLength / host 5x7): generous box. */
            w = (int)len * 8;
            top = (int)c->y0 - 8;
            bot = (int)c->y0 + 8;
        }
        *x0 = (int)c->x0 - w / 2;
        *y0 = top;
        *x1 = (int)c->x0 + (w - 1) / 2;
        *y1 = bot;
        return 0;
    }
    case RI_D_IMAGE:
        /* Backend-sized skin part: always repaint (one blit, still cheap). */
        *x0 = *y0 = -30000;
        *x1 = *y1 = 30000;
        return 0;
    default:
        return 2;
    }
}

int ri_dcmd_hits_box(const struct ri_dcmd *c, int x0, int y0, int x1, int y1) {
    int a0, b0, a1, b1;
    if (!c || x1 < x0 || y1 < y0)
        return 0;
    if (ri_dcmd_bbox(c, &a0, &b0, &a1, &b1) != 0)
        return 0;
    return a0 <= x1 && a1 >= x0 && b0 <= y1 && b1 >= y0;
}

int ri_draw_image(struct ri_dlist *dl, int x, int y, uint16_t img, uint16_t frame) {
    return emit(dl, RI_D_IMAGE, x, y, 0, 0, 0u, img, frame, 0u, 0u, 0);
}

int ri_draw_clip(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    return emit(dl, RI_D_CLIP, x0, y0, x1, y1, 0u, 0u, 0u, 0u, 0u, 0);
}

uint32_t ri_dlist_hash(const struct ri_dlist *dl) {
    uint32_t h = 2166136261u, i, k;
    const unsigned char *p;
    if (!dl || !dl->cmd)
        return h;
    for (i = 0u; i < dl->n; i++) {
        /* Hash the value fields, not pointers (text hashed by bytes).
         * pad[0] (TEXT face) is a value: face switches must move pins. */
        uint32_t words[6];
        words[0] = ((uint32_t)dl->cmd[i].op << 24) | ((uint32_t)dl->cmd[i].align << 16) |
            ((uint32_t)(uint16_t)dl->cmd[i].x0);
        words[1] = ((uint32_t)(uint16_t)dl->cmd[i].y0 << 16) | (uint32_t)(uint16_t)dl->cmd[i].x1;
        words[2] = (uint32_t)(uint16_t)dl->cmd[i].y1;
        words[3] = dl->cmd[i].rgb;
        words[4] = ((uint32_t)dl->cmd[i].img << 16) | dl->cmd[i].frame;
        words[5] = ((uint32_t)dl->cmd[i].pad[0] << 8) | (uint32_t)dl->cmd[i].pad[1];
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
