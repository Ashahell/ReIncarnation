/* raster.c — host software rasterizer bodies (portability plan T2). */
#include "platform/host/raster.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <png.h>

/* Clean-room 5x7 face (bit0 = left pixel). Uppercase + digits + marks. */
static const uint8_t RI_FONT[43][7] = {
    /* space */ { 0, 0, 0, 0, 0, 0, 0 },
    /* - */ { 0, 0, 0, 0x1F, 0, 0, 0 },
    /* . */ { 0, 0, 0, 0, 0, 0x0C, 0x0C },
    /* / */ { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 },
    /* 0 */ { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E },
    /* 1 */ { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E },
    /* 2 */ { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F },
    /* 3 */ { 0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E },
    /* 4 */ { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 },
    /* 5 */ { 0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E },
    /* 6 */ { 0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E },
    /* 7 */ { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 },
    /* 8 */ { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E },
    /* 9 */ { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C },
    /* : */ { 0, 0x0C, 0x0C, 0, 0x0C, 0x0C, 0 },
    /* + */ { 0, 0x04, 0x04, 0x1F, 0x04, 0x04, 0 },
    /* % */ { 0x19, 0x1A, 0x02, 0x04, 0x08, 0x14, 0x13 },
    /* A */ { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },
    /* B */ { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E },
    /* C */ { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E },
    /* D */ { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E },
    /* E */ { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F },
    /* F */ { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 },
    /* G */ { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F },
    /* H */ { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 },
    /* I */ { 0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E },
    /* J */ { 0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C },
    /* K */ { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 },
    /* L */ { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F },
    /* M */ { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 },
    /* N */ { 0x11, 0x19, 0x19, 0x15, 0x13, 0x13, 0x11 },
    /* O */ { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },
    /* P */ { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 },
    /* Q */ { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D },
    /* R */ { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 },
    /* S */ { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E },
    /* T */ { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
    /* U */ { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E },
    /* V */ { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 },
    /* W */ { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11 },
    /* X */ { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 },
    /* Y */ { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 },
    /* Z */ { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F },
};

static int font_idx(char c) {
    if (c == ' ')
        return 0;
    if (c == '-')
        return 1;
    if (c == '.')
        return 2;
    if (c == '/')
        return 3;
    if (c >= '0' && c <= '9')
        return 4 + (c - '0');
    if (c == ':')
        return 14;
    if (c == '+')
        return 15;
    if (c == '%')
        return 16;
    if (c >= 'A' && c <= 'Z')
        return 17 + (c - 'A');
    if (c >= 'a' && c <= 'z')
        return 17 + (c - 'a');
    return -1;
}

int ri_raster_text_width(void *ctx, const char *s) {
    uint32_t n = 0u;
    (void)ctx;
    if (!s)
        return 0;
    while (s[n])
        n++;
    return n ? (int)(n * RI_RASTER_ADVANCE - 1u) : 0;
}

void ri_raster_init(struct ri_raster *r, uint32_t *backing, uint32_t w, uint32_t h) {
    if (!r)
        return;
    r->px = backing;
    r->w = (backing && w) ? w : 0u;
    r->h = (backing && h) ? h : 0u;
}

void ri_raster_clear(struct ri_raster *r, uint32_t rgb) {
    uint32_t i, n;
    uint32_t argb;
    if (!r || !r->px)
        return;
    argb = 0xFF000000u | (rgb & 0xFFFFFFu);
    n = r->w * r->h;
    for (i = 0u; i < n; i++)
        r->px[i] = argb;
}

static void put(struct ri_raster *r, int x, int y, uint32_t argb) {
    if (!r || !r->px || x < 0 || y < 0 || (uint32_t)x >= r->w || (uint32_t)y >= r->h)
        return;
    r->px[(uint32_t)y * r->w + (uint32_t)x] = argb;
}

static void fill_rect(struct ri_raster *r, int x0, int y0, int x1, int y1, uint32_t rgb) {
    int x, y;
    uint32_t argb;
    if (!r || !r->px || x1 < x0 || y1 < y0)
        return;
    argb = 0xFF000000u | (rgb & 0xFFFFFFu);
    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    for (y = y0; (uint32_t)y <= (uint32_t)y1 && (uint32_t)y < r->h; y++)
        for (x = x0; (uint32_t)x <= (uint32_t)x1 && (uint32_t)x < r->w; x++)
            r->px[(uint32_t)y * r->w + (uint32_t)x] = argb;
}

static void line(struct ri_raster *r, int x0, int y0, int x1, int y1, uint32_t rgb) {
    int dx = x1 >= x0 ? x1 - x0 : x0 - x1, dy = y1 >= y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2, x = x0, y = y0, guard = 0;
    uint32_t argb = 0xFF000000u | (rgb & 0xFFFFFFu);
    for (;;) {
        put(r, x, y, argb);
        if (x == x1 && y == y1)
            break;
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x += sx;
        }
        if (e2 < dy) {
            err += dx;
            y += sy;
        }
        if (++guard > 100000)
            break;
    }
}

static void text(struct ri_raster *r, int cx, int cy, const char *s, uint32_t rgb) {
    uint32_t n = 0u, i;
    int x0, y0, gx, gy;
    uint32_t argb;
    if (!r || !s)
        return;
    while (s[n])
        n++;
    if (!n)
        return;
    x0 = cx - (int)(n * RI_RASTER_ADVANCE - 1u) / 2;
    y0 = cy - 3;
    argb = 0xFF000000u | (rgb & 0xFFFFFFu);
    for (i = 0u; i < n; i++) {
        int fi = font_idx(s[i]);
        if (fi < 0)
            continue;
        for (gy = 0; gy < 7; gy++)
            for (gx = 0; gx < 5; gx++)
                if (RI_FONT[fi][gy] & (1u << (4u - (uint32_t)gx)))
                    put(r, x0 + (int)(i * RI_RASTER_ADVANCE) + gx, y0 + gy, argb);
    }
}

/* Straight-alpha composite of a zoom-cache part at (dx, dy), clipped. */
static void blit_part(struct ri_raster *r, const uint32_t *px, uint32_t w, uint32_t h, int dx, int dy) {
    uint32_t sx, sy;
    if (!r || !px)
        return;
    for (sy = 0u; sy < h; sy++) {
        int y = dy + (int)sy;
        if (y < 0 || (uint32_t)y >= r->h)
            continue;
        for (sx = 0u; sx < w; sx++) {
            int x = dx + (int)sx;
            uint32_t s = px[sy * w + sx];
            uint32_t sa = (s >> 24) & 255u;
            uint32_t d;
            if (x < 0 || (uint32_t)x >= r->w)
                continue;
            if (sa == 0u)
                continue;
            d = r->px[(uint32_t)y * r->w + (uint32_t)x];
            if (sa == 255u) {
                r->px[(uint32_t)y * r->w + (uint32_t)x] = 0xFF000000u | (s & 0xFFFFFFu);
            } else {
                uint32_t sr = (s >> 16) & 255u, sg = (s >> 8) & 255u, sb = s & 255u;
                uint32_t dr = (d >> 16) & 255u, dg = (d >> 8) & 255u, db = d & 255u;
                uint32_t or_ = (sr * sa + dr * (255u - sa) + 127u) / 255u;
                uint32_t og = (sg * sa + dg * (255u - sa) + 127u) / 255u;
                uint32_t ob = (sb * sa + db * (255u - sa) + 127u) / 255u;
                r->px[(uint32_t)y * r->w + (uint32_t)x] = 0xFF000000u | (or_ << 16) | (og << 8) | ob;
            }
        }
    }
}

void ri_raster_replay(struct ri_raster *r, const struct ri_dlist *dl,
    const struct RISkin *skin) {
    uint32_t i;
    if (!r || !dl)
        return;
    for (i = 0u; i < dl->n; i++) {
        const struct ri_dcmd *c = &dl->cmd[i];
        int k;
        switch (c->op) {
        case RI_D_RECT:
            fill_rect(r, c->x0, c->y0, c->x1, c->y1, c->rgb);
            break;
        case RI_D_LINE:
            line(r, c->x0, c->y0, c->x1, c->y1, c->rgb);
            break;
        case RI_D_CIRCLE:
            for (k = -(int)c->x1; k <= (int)c->x1; k++) {
                int dx = (int)c->x1;
                while (dx > 0 && dx * dx + k * k > (int)c->x1 * (int)c->x1)
                    dx--;
                fill_rect(r, c->x0 - dx, c->y0 + k, c->x0 + dx, c->y0 + k, c->rgb);
            }
            break;
        case RI_D_TEXT:
            text(r, c->x0, c->y0, c->text, c->rgb);
            break;
        case RI_D_IMAGE: {
            uint32_t frames, fh;
            if (!skin || c->img >= RI_SKIN_MAX_PARTS || !skin->parts[c->img].zrgba ||
                !skin->parts[c->img].zw || !skin->parts[c->img].zh)
                break;
            frames = skin->parts[c->img].frames ? skin->parts[c->img].frames : 1u;
            fh = skin->parts[c->img].zh / frames;
            if (c->frame < frames)
                blit_part(r, skin->parts[c->img].zrgba + (size_t)c->frame * fh * skin->parts[c->img].zw,
                    skin->parts[c->img].zw, fh, c->x0, c->y0);
            break;
        }
        case RI_D_CLIP:
            break;
        default:
            break;
        }
    }
}

uint32_t ri_raster_hash(const struct ri_raster *r) {
    uint32_t h = 2166136261u, i, n;
    const unsigned char *p;
    if (!r || !r->px)
        return h;
    n = r->w * r->h;
    p = (const unsigned char *)r->px;
    for (i = 0u; i < n * 4u; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

int ri_raster_write_png(const char *path, const struct ri_raster *r) {
    FILE *f;
    png_structp png;
    png_infop info;
    uint32_t y;
    png_bytep row;
    if (!path || !r || !r->px)
        return 1;
    f = fopen(path, "wb");
    if (!f)
        return 1;
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return 1;
    }
    png_init_io(png, f);
    png_set_IHDR(png, info, r->w, r->h, 8, PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    row = (png_bytep)malloc((size_t)r->w * 4u);
    if (!row) {
        png_destroy_write_struct(&png, &info);
        fclose(f);
        return 1;
    }
    for (y = 0u; y < r->h; y++) {
        uint32_t x;
        for (x = 0u; x < r->w; x++) {
            uint32_t p = r->px[y * r->w + x];
            row[x * 4u] = (uint8_t)((p >> 16) & 255u);
            row[x * 4u + 1u] = (uint8_t)((p >> 8) & 255u);
            row[x * 4u + 2u] = (uint8_t)(p & 255u);
            row[x * 4u + 3u] = (uint8_t)((p >> 24) & 255u);
        }
        png_write_row(png, row);
    }
    free(row);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(f);
    return 0;
}
