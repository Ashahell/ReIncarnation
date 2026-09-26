/* t87_pal_image — portability T7: host PNG decode of shipped skins.
 * PAL decode of every 808-RI skin PNG cross-checked against a direct
 * libpng read (dims + full pixels), plus mkskin's known spot pixels
 * (bg02 (4,4) = 808 lacquer; knob_45x70 body right of centre).
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <png.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_image.h"

static int read_file(const char *path, unsigned char **out, uint32_t *n) {
    FILE *f;
    long sz;
    unsigned char *b;
    f = fopen(path, "rb");
    if (!f)
        return 1;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 8 * 1024 * 1024) {
        fclose(f);
        return 1;
    }
    b = (unsigned char *)malloc((size_t)sz);
    if (!b) {
        fclose(f);
        return 1;
    }
    if (fread(b, 1u, (size_t)sz, f) != (size_t)sz) {
        free(b);
        fclose(f);
        return 1;
    }
    fclose(f);
    *out = b;
    *n = (uint32_t)sz;
    return 0;
}

/* Independent libpng read (no PAL): RGBA bytes, for cross-check. */
static int direct_read(const unsigned char *file, uint32_t len,
    unsigned char **px, uint32_t *w, uint32_t *h) {
    png_image img;
    unsigned char *out = 0;
    memset(&img, 0, sizeof img);
    img.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&img, file, len))
        return 1;
    img.format = PNG_FORMAT_RGBA;
    out = (unsigned char *)malloc(PNG_IMAGE_SIZE(img));
    if (!out) {
        png_image_free(&img);
        return 1;
    }
    if (!png_image_finish_read(&img, 0, out, 0, 0)) {
        free(out);
        png_image_free(&img);
        return 1;
    }
    *px = out;
    *w = img.width;
    *h = img.height;
    png_image_free(&img);
    return 0;
}

static uint32_t argb(unsigned r, unsigned g, unsigned b, unsigned a) {
    return ((a & 255u) << 24) | ((r & 255u) << 16) | ((g & 255u) << 8) | (b & 255u);
}

int main(void) {
    const char *dir = "skins/808-RI";
    DIR *dp;
    struct dirent *de;
    uint32_t nfiles = 0u;
    int saw_bg02 = 0, saw_knob = 0;
    dp = opendir(dir);
    RI_ASSERT(dp != 0, "open skins dir");
    while ((de = readdir(dp)) != 0) {
        char path[512];
        unsigned char *file = 0, *flat = 0;
        uint32_t len = 0u, w = 0u, h = 0u, dw = 0u, dh = 0u, *px = 0, i;
        size_t namelen = strlen(de->d_name);
        if (namelen < 5u || strcmp(de->d_name + namelen - 4u, ".png") != 0)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, de->d_name);
        RI_ASSERT(read_file(path, &file, &len) == 0, "read %s", de->d_name);
        RI_ASSERT(ri_pal_image_decode(file, len, &px, &w, &h) == 0, "pal %s", de->d_name);
        RI_ASSERT(px != 0 && w > 0u && h > 0u && w <= 8192u && h <= 8192u, "dims %s", de->d_name);
        RI_ASSERT(direct_read(file, len, &flat, &dw, &dh) == 0, "direct %s", de->d_name);
        RI_ASSERT(dw == w && dh == h, "dim match %s", de->d_name);
        for (i = 0u; i < w * h; i++) {
            unsigned char *q = flat + (size_t)i * 4u;
            uint32_t want = argb(q[0], q[1], q[2], q[3]);
            if (px[i] != want) {
                RI_ASSERT(0, "pixel %s[%u]", de->d_name, i);
                break;
            }
        }
        if (!strcmp(de->d_name, "bg02.png")) {
            RI_ASSERT(w > 4u && h > 4u, "bg02 size");
            RI_ASSERT(px[4u * w + 4u] == argb(0x17u, 0x14u, 0x10u, 255u), "bg02 spot");
            saw_bg02 = 1;
        }
        if (!strcmp(de->d_name, "knob_45x70.png")) {
            /* mkskin spot: frame 0 body right of centre (w2=45,h2=70). */
            uint32_t sx = 45u / 2u + (45u / 2u - 1u) / 2u, sy = 70u / 2u;
            RI_ASSERT(w == 45u, "knob w");
            RI_ASSERT(px[sy * w + sx] == argb(0x2fu, 0x2bu, 0x27u, 255u), "knob spot");
            saw_knob = 1;
        }
        ri_pal_image_free(px);
        free(flat);
        free(file);
        nfiles++;
    }
    closedir(dp);
    RI_ASSERT(nfiles >= 10u, "files %u", nfiles);
    RI_ASSERT(saw_bg02, "bg02 seen");
    RI_ASSERT(saw_knob, "knob seen");
    RI_RESULT("pal_image");
}
