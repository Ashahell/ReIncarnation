/* image_host.c — host (CI) backend for ri_pal_image (portability plan T7).
 * libpng decode to 0xAARRGGBB core words (same order as the AROS datatype
 * path). NOTE §8.1: libpng is already the skins toolchain dep (tools/mkskin.c
 * links -lpng); host-only. Windows still needs its own call (WIC/stb).
 */
#include "platform/pal/ri_pal_image.h"

#include <png.h>
#include <stdlib.h>
#include <string.h>

struct ri_mem {
    const unsigned char *p;
    uint32_t n, i;
};

static void ri_mem_read(png_structp png, png_bytep out, size_t n) {
    struct ri_mem *m = (struct ri_mem *)png_get_io_ptr(png);
    if (!m || m->i + n > m->n)
        png_error(png, "short read");
    memcpy(out, m->p + m->i, n);
    m->i += (uint32_t)n;
}

int ri_pal_image_decode(const void *file, uint32_t len, uint32_t **rgba,
    uint32_t *w, uint32_t *h) {
    png_structp png = 0;
    png_infop info = 0;
    png_uint_32 rw = 0u, rh = 0u;
    int bit = 0, ct = 0, y = 0;
    /* volatile: modified between setjmp and png_error's longjmp. */
    png_bytep *volatile rows = 0;
    uint32_t *volatile px = 0;
    struct ri_mem m;
    if (!file || len == 0u || !rgba || !w || !h)
        return 1;
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info = png_create_info_struct(png);
    if (!png || !info)
        return 1;
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, 0);
        if (rows)
            free(rows);
        if (px)
            free(px);
        return 1;
    }
    m.p = (const unsigned char *)file;
    m.n = len;
    m.i = 0u;
    png_set_read_fn(png, &m, ri_mem_read);
    png_read_info(png, info);
    png_get_IHDR(png, info, &rw, &rh, &bit, &ct, 0, 0, 0);
    /* Host cap 8192 (AROS keeps a 4096 device guard in image_dt.c; knob
     * strips reach 4480 tall and fall back to Classic there — divergence
     * recorded in t7-image.md, owner call to unify). */
    if (rw == 0u || rh == 0u || rw > 8192u || rh > 8192u)
        png_error(png, "dims");
    if (bit == 16)
        png_set_strip_16(png);
    if (ct == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);
    if (ct == PNG_COLOR_TYPE_GRAY || ct == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);
    if (!(ct & PNG_COLOR_MASK_ALPHA))
        png_set_filler(png, 0xFFu, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    px = (uint32_t *)malloc((size_t)rw * rh * 4u);
    rows = (png_bytep *)malloc(sizeof(png_bytep) * rh);
    if (!px || !rows)
        png_error(png, "oom");
    for (y = 0; (png_uint_32)y < rh; y++)
        rows[y] = (png_bytep)(px + (size_t)y * rw);
    png_read_image(png, rows);
    png_read_end(png, info);
    png_destroy_read_struct(&png, &info, 0);
    free(rows);
    /* libpng RGBA bytes -> core 0xAARRGGBB words. */
    for (y = 0; (png_uint_32)y < rh; y++) {
        uint32_t x;
        for (x = 0u; x < rw; x++) {
            unsigned char *q = (unsigned char *)&px[(size_t)y * rw + x];
            uint32_t r = q[0], g = q[1], b = q[2], a = q[3];
            px[(size_t)y * rw + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    *rgba = px;
    *w = rw;
    *h = rh;
    return 0;
}

void ri_pal_image_free(uint32_t *rgba) {
    free(rgba);
}
