/* tools/mkskin.c — 808-RI skin asset generator (§12.10 G8.1).
 *
 * Host tool (manual build line below). Generates ORIGINAL 808-RI art
 * procedurally — never traced from ReBirth pixels (clean-room, spec §1):
 * dark lacquer backdrops per section group + amber knob frame strips.
 * Sizes come from gui/panelgeo (2x-master px == Q numerically). Every PNG
 * is read back and verified (dims + spot pixels) before the tool reports
 * success; any mismatch exits nonzero with no partial skin left behind.
 *
 * Writes skin format 1 (FORMAT/NAME/VERSION header, ctlreg section/kind
 * tokens, knob strips named by role via ri_skin_knob_role).
 *
 * Build: gcc -std=c99 -O2 -Wall -Wextra -I. -o /tmp/ri/mkskin tools/mkskin.c \
 *   /tmp/ri/build/panelgeo.o /tmp/ri/build/ctlreg.o /tmp/ri/build/knob_art.o \
 *   /tmp/ri/build/skin.o /tmp/ri/build/sha256.o -lpng \
 *   && mkdir -p /tmp/ri/skinsout && /tmp/ri/mkskin /tmp/ri/skinsout
 * (needs a full `ri_build_host.sh all` first for the .o files).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <png.h>
#include "gui/panelgeo.h"
#include "gui/ctlreg.h"
#include "gui/knob_art.h"
#include "gui/skin.h"

#define FRAMES 64
#define MKSKIN_VERSION 2u /* 2: format 1 + master at strip height */

static int fails;

#define CHECK(c, msg) do { \
    if (!(c)) { printf("mkskin FAIL: %s\n", msg); fails++; } \
} while (0)

/* ---------- pixel buffer ---------- */
struct Img { int w, h; unsigned char *px; }; /* RGBA bytes */

static void img_init(struct Img *im, int w, int h) {
    im->w = w;
    im->h = h;
    im->px = (unsigned char *)calloc((size_t)w * h * 4u, 1u);
    if (!im->px) {
        printf("mkskin FAIL: out of memory\n");
        exit(1);
    }
}

static void img_free(struct Img *im) {
    free(im->px);
    im->px = 0;
}

static void put(struct Img *im, int x, int y,
                unsigned r, unsigned g, unsigned b, unsigned a) {
    unsigned char *p;
    if (x < 0 || y < 0 || x >= im->w || y >= im->h)
        return;
    p = im->px + ((size_t)y * (uint32_t)im->w + (uint32_t)x) * 4u;
    p[0] = (unsigned char)r;
    p[1] = (unsigned char)g;
    p[2] = (unsigned char)b;
    p[3] = (unsigned char)a;
}

static void hline(struct Img *im, int x0, int x1, int y,
                  unsigned r, unsigned g, unsigned b, unsigned a) {
    int x;
    for (x = x0; x <= x1; x++)
        put(im, x, y, r, g, b, a);
}

static void vline(struct Img *im, int x, int y0, int y1,
                  unsigned r, unsigned g, unsigned b, unsigned a) {
    int y;
    for (y = y0; y <= y1; y++)
        put(im, x, y, r, g, b, a);
}

/* ---------- PNG write + verify ---------- */
static void write_png(const char *path, const struct Img *im) {
    FILE *f = fopen(path, "wb");
    png_structp png;
    png_infop info;
    int y;
    if (!f) {
        printf("mkskin FAIL: cannot write %s\n", path);
        fails++;
        return;
    }
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        printf("mkskin FAIL: png setup %s\n", path);
        fails++;
        if (f)
            fclose(f);
        return;
    }
    png_init_io(png, f);
    png_set_IHDR(png, info, (png_uint_32)im->w, (png_uint_32)im->h, 8,
                 PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);
    for (y = 0; y < im->h; y++)
        png_write_row(png, im->px + (size_t)y * (uint32_t)im->w * 4u);
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    fclose(f);
}

static void verify_png(const char *path, int w, int h,
                       int sx, int sy, unsigned er, unsigned eg,
                       unsigned eb, unsigned ea) {
    FILE *f = fopen(path, "rb");
    png_structp png;
    png_infop info;
    png_uint_32 rw, rh;
    int bit, ct;
    png_bytep row;
    if (!f) {
        printf("mkskin FAIL: cannot re-read %s\n", path);
        fails++;
        return;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    info = png_create_info_struct(png);
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        printf("mkskin FAIL: png read setup %s\n", path);
        fails++;
        fclose(f);
        return;
    }
    png_init_io(png, f);
    png_read_info(png, info);
    png_get_IHDR(png, info, &rw, &rh, &bit, &ct, 0, 0, 0);
    CHECK(rw == (png_uint_32)w && rh == (png_uint_32)h, "dims");
    CHECK(bit == 8 && ct == PNG_COLOR_TYPE_RGBA, "rgba8");
    row = (png_bytep)malloc((size_t)w * 4u);
    {
        int y;
        for (y = 0; y < sy + 1 && (png_uint_32)y < rh; y++)
            png_read_row(png, row, 0);
    }
    CHECK(row[4u * (uint32_t)sx] == er && row[4u * (uint32_t)sx + 1u] == eg &&
          row[4u * (uint32_t)sx + 2u] == eb && row[4u * (uint32_t)sx + 3u] == ea,
          "spot pixel");
    free(row);
    png_destroy_read_struct(&png, &info, 0);
    fclose(f);
}

/* ---------- 808-RI art (original) ---------- */
/* Group base colours: lacquer black-brown / blue-steel / deep slate. */
static void bg_base(uint32_t sec, unsigned *r, unsigned *g, unsigned *b) {
    if (sec == RI_SEC_808) {
        *r = 0x17u;
        *g = 0x14u;
        *b = 0x10u;
    } else if (sec == RI_SEC_909) {
        *r = 0x14u;
        *g = 0x16u;
        *b = 0x1au;
    } else if (sec >= RI_SEC_MIX_SYNTH1 && sec <= RI_SEC_MASTER) {
        *r = 0x10u;
        *g = 0x14u;
        *b = 0x18u;
    } else {
        *r = 0x1au;
        *g = 0x1au;
        *b = 0x1cu;
    }
}

static void gen_bg(struct Img *im, uint32_t sec) {
    unsigned br, bg, bb;
    int x, y;
    bg_base(sec, &br, &bg, &bb);
    for (y = 0; y < im->h; y++) {
        int d = (y * 16) / (im->h ? im->h : 1);
        for (x = 0; x < im->w; x++)
            put(im, x, y, br + (unsigned)(d / 2), bg + (unsigned)(d / 2),
                bb + (unsigned)(d / 2), 255u);
    }
    /* amber border + title band (legends stay procedural on top) */
    hline(im, 0, im->w - 1, 0, 0xe8u, 0xa3u, 0x3du, 255u);
    hline(im, 0, im->w - 1, 1, 0x6bu, 0x5au, 0x3eu, 255u);
    hline(im, 0, im->w - 1, im->h - 1, 0x6bu, 0x5au, 0x3eu, 255u);
    vline(im, 0, 0, im->h - 1, 0x6bu, 0x5au, 0x3eu, 255u);
    vline(im, im->w - 1, 0, im->h - 1, 0x6bu, 0x5au, 0x3eu, 255u);
    for (y = 8; y < 20 && y < im->h; y++) {
        int xx;
        for (xx = 8; xx < im->w - 8; xx++)
            put(im, xx, y, 0x2au, 0x24u, 0x1eu, 255u);
    }
}

/* One knob frame: charcoal disc, amber rim, thick amber pointer at the
 * frame angle (no tick ring — unlike Classic). Transparent outside. */
static void gen_knob_frame(struct Img *im, int frame) {
    int cx = im->w / 2, cy = im->h / 2;
    int rad = (im->w < im->h ? im->w : im->h) / 2 - 1;
    int deg = -135 + 270 * frame / (FRAMES - 1);
    int32_t sx = RI_SIN_Q15[(deg + 360) % 360];
    int32_t cxs = RI_SIN_Q15[(deg + 450) % 360];
    int x, y, t;
    for (y = 0; y < im->h; y++) {
        for (x = 0; x < im->w; x++) {
            int dx = x - cx, dy = y - cy;
            int d2 = dx * dx + dy * dy;
            int r2 = rad * rad;
            if (d2 > r2)
                continue; /* transparent */
            {
                int shade = 0x2eu - (14 * y / (im->h ? im->h : 1));
                put(im, x, y, (unsigned)shade + 8u, (unsigned)shade + 4u,
                    (unsigned)shade, 255u);
            }
            if (r2 - d2 < rad * 2)
                put(im, x, y, 0xe8u, 0xa3u, 0x3du, 255u); /* amber rim */
        }
    }
    /* pointer: 5 px wide line from centre to 70% radius + tip dot */
    for (t = 0; t < rad * 70 / 100; t++) {
        int px = (int)(cx + (sx * t) / 32767);
        int py = (int)(cy - (cxs * t) / 32767);
        int ox, oy;
        for (ox = -2; ox <= 2; ox++)
            for (oy = -2; oy <= 2; oy++)
                if (ox * ox + oy * oy <= 4)
                    put(im, px + ox, py + oy, 0xe8u, 0xa3u, 0x3du, 255u);
    }
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    const char *root;
    char path[512], man[512];
    FILE *mf;
    uint32_t s, i;
    /* distinct knob sizes first (one strip file each) */
    struct {
        int w, h;
        char name[32];
    } sizes[8];
    int nsizes = 0;
    struct {
        uint32_t sec;
        int w, h;
    } emitted[32];
    int nemitted = 0;
    if (argc != 2) {
        printf("usage: %s <skins-dir>\n", argv[0]);
        return 2;
    }
    root = argv[1];
    snprintf(path, sizeof path, "%s", root);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/808-RI", root);
    mkdir(path, 0755);
    snprintf(path, sizeof path, "%s/Template", root);
    mkdir(path, 0755);
    snprintf(man, sizeof man, "%s/808-RI/Skin.manifest", root);
    mf = fopen(man, "w");
    if (!mf) {
        printf("mkskin FAIL: manifest open\n");
        return 1;
    }
    fprintf(mf, "# 808-RI appearance half (generated by tools/mkskin.c - original art)\n");
    fprintf(mf, "FORMAT=%u\nNAME=808-RI\nVERSION=%u\n", RI_SKIN_FORMAT, MKSKIN_VERSION);
    for (s = 0; s < RI_SEC_COUNT; s++) {
        const struct RIGeoSection *g = ri_geo_section(s == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : s);
        struct Img bg;
        int bw, bh;
        char bgname[32];
        unsigned br, bgc, bb;
        if (!g || s == RI_SEC_SYNTH2)
            continue; /* SYNTH2 shares SYNTH1 art (E0-10) */
        bw = ri_geo_px((int)g->w, 2);
        bh = ri_geo_px((int)g->h, 2);
        if (bw <= 0 || bh <= 0 || bw > 2048 || bh > 2048) {
            printf("mkskin FAIL: bad bg size sec %u\n", s);
            fails++;
            continue;
        }
        snprintf(bgname, sizeof bgname, "bg%02u.png", s);
        snprintf(path, sizeof path, "%s/808-RI/%s", root, bgname);
        img_init(&bg, bw, bh);
        gen_bg(&bg, s);
        write_png(path, &bg);
        bg_base(s, &br, &bgc, &bb);
        verify_png(path, bw, bh, 4, 4, br, bgc, bb, 255u);
        img_free(&bg);
        fprintf(mf, "BACKGROUND.%s=%s\n", ri_ctlreg_section_token(s), bgname);
        /* knob strips, one per distinct KNOB size in this section */
        for (i = 0; i < g->nitems; i++) {
            const struct RICtlDef *d;
            int w2, h2, k, sizeidx = -1;
            if (g->items[i].shape != RI_GEO_KNOB)
                continue;
            d = ri_ctlreg_find((uint16_t)((s << 8) | (g->items[i].reg_id & 0xFFu)));
            if (!d || d->kind != RI_CK_KNOB)
                continue;
            w2 = ri_geo_px((int)g->items[i].w, 2);
            h2 = ri_geo_px((int)g->items[i].h, 2);
            if (w2 <= 0 || h2 <= 0 || w2 > 512 || h2 > 512) {
                printf("mkskin FAIL: bad knob size sec %u\n", s);
                fails++;
                continue;
            }
            for (k = 0; k < nsizes; k++) {
                if (sizes[k].w == w2 && sizes[k].h == h2)
                    sizeidx = k;
            }
            if (sizeidx < 0) {
                struct Img strip;
                int f;
                int rad, spx, spy;
                if (nsizes >= 8) {
                    printf("mkskin FAIL: too many knob sizes\n");
                    fails++;
                    continue;
                }
                sizeidx = nsizes;
                snprintf(sizes[nsizes].name, sizeof sizes[nsizes].name,
                         "knob_%dx%d.png", w2, h2);
                sizes[nsizes].w = w2;
                sizes[nsizes].h = h2;
                nsizes++;
                img_init(&strip, w2, h2 * FRAMES);
                for (f = 0; f < FRAMES; f++) {
                    struct Img frame = { w2, h2,
                        strip.px + (size_t)f * (uint32_t)h2 * (uint32_t)w2 * 4u };
                    gen_knob_frame(&frame, f);
                }
                snprintf(path, sizeof path, "%s/808-RI/%s", root,
                         sizes[sizeidx].name);
                write_png(path, &strip);
                /* spot: frame 0 body right of centre (pointer aims down-left) */
                rad = (w2 < h2 ? w2 : h2) / 2 - 1;
                spx = w2 / 2 + rad / 2;
                spy = h2 / 2;
                verify_png(path, w2, h2 * FRAMES, spx, spy,
                           0x2fu, 0x2bu, 0x27u, 255u);
                img_free(&strip);
            }
            { /* one PART line per (section, size): duplicates fail the parse */
                int e, dup = 0;
                for (e = 0; e < nemitted; e++) {
                    if (emitted[e].sec == s && emitted[e].w == w2 &&
                        emitted[e].h == h2)
                        dup = 1;
                }
                if (!dup) {
                    if (nemitted >= 32) {
                        printf("mkskin FAIL: too many part lines\n");
                        fails++;
                    } else {
                        emitted[nemitted].sec = s;
                        emitted[nemitted].w = w2;
                        emitted[nemitted].h = h2;
                        nemitted++;
                        const char *role = ri_skin_knob_role((uint8_t)s, (uint32_t)w2, (uint32_t)h2);
                        if (!role) {
                            printf("mkskin FAIL: no role for %dx%d sec %u\n", w2, h2, s);
                            fails++;
                        } else {
                            fprintf(mf, "PART.%s.knob.%s=%s,%d\n", ri_ctlreg_section_token(s),
                                    role, sizes[sizeidx].name, FRAMES);
                        }
                    }
                }
            }
        }
    }
    fclose(mf);
    if (fails) {
        printf("mkskin FAIL %d\n", fails);
        return 1;
    }
    printf("mkskin PASS\n");
    return 0;
}
