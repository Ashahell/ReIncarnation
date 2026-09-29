/* t93_raster_goldens — portability T2: host raster pixel goldens.
 * Every laid-out section × zoom (procedural) plus the 808-RI-skinned 808:
 * replay into RGBA, pin pixel hashes; snapshots go to
 * docs/evidence/gui/host-raster/.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/art.h"
#include "gui/draw/font_legend.h"
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/sectmix.h"
#include "gui/sectlevi.h"
#include "gui/skin.h"
#include "platform/host/raster.h"
#include "platform/pal/ri_pal_image.h"

static struct ri_dcmd T_BACK[24576];
static char T_SPOOL[32768];
static struct RIMixBoard T_BOARD;

static const uint8_t T_SECS[] = {
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909, RI_SEC_LEVI,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MASTER,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_TRANSPORT, RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909,
    RI_SEC_PAT_LEVI, RI_SEC_MIX_LEVI
};

static uint32_t render_one(uint8_t sec, int z, const struct RISkin *skin,
    uint32_t *px, uint32_t cap, const char *shot) {
    struct RISectUI ui;
    struct ri_dlist dl;
    struct ri_raster r;
    const struct RIGeoSection *g;
    struct ri_text_metrics tm;
    uint32_t w, h;
    if (ri_sui_init(&ui, sec) != 0)
        return 0u;
    if (ri_smix_strip(sec) >= 0 || sec == RI_SEC_MASTER)
        ri_sui_bind_board(&ui, &T_BOARD);
    g = ri_geo_section(sec == RI_SEC_SYNTH2 ? RI_SEC_SYNTH1 : sec);
    if (!g)
        return 0u;
    w = (uint32_t)ri_geo_px((int)g->w, z);
    h = (uint32_t)ri_geo_px((int)g->h, z);
    if (w == 0u || h == 0u || (uint64_t)w * h > cap)
        return 0u;
    ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
    tm.width = ri_raster_text_width;
    tm.height = 7;
    tm.baseline = 5;
    tm.ctx = 0;
    ri_draw_section(&dl, &ui, sec, z, 0, 0, &tm, skin, 0);
    if (dl.n == 0u || dl.n >= dl.cap)
        return 0u;
    ri_raster_init(&r, px, w, h);
    ri_raster_clear(&r, 0x000000u);
    ri_raster_replay(&r, &dl, skin);
    if (shot)
        RI_ASSERT(ri_raster_write_png(shot, &r) == 0, "png %s", shot);
    return ri_raster_hash(&r);
}

static int read_file(const char *path, unsigned char **out, uint32_t *n) {
    FILE *f;
    long sz;
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
    *out = (unsigned char *)malloc((size_t)sz);
    if (!*out) {
        fclose(f);
        return 1;
    }
    if (fread(*out, 1u, (size_t)sz, f) != (size_t)sz) {
        free(*out);
        fclose(f);
        return 1;
    }
    fclose(f);
    *n = (uint32_t)sz;
    return 0;
}

/* Host skin load: manifest + image_host decodes + pure zoom cache. */
static int load_skin(const char *dir, int zoom, struct RISkin *skin) {
    char mpath[512];
    unsigned char *text = 0;
    uint32_t ntext = 0u, i;
    uint32_t num;
    snprintf(mpath, sizeof mpath, "%s/Skin.manifest", dir);
    if (read_file(mpath, &text, &ntext) != 0)
        return 1;
    if (ri_skin_parse((char *)text, skin) != 0) {
        free(text);
        return 1;
    }
    free(text);
    num = ri_skin_zoom_num(zoom);
    if (!num)
        return 1;
    for (i = 0u; i < skin->nparts; i++) {
        char ppath[512];
        unsigned char *file = 0;
        uint32_t nfile = 0u, *px = 0, w = 0u, h = 0u;
        uint32_t tw, th, *scaled = 0;
        snprintf(ppath, sizeof ppath, "%s/%s", dir, skin->parts[i].file);
        if (read_file(ppath, &file, &nfile) != 0)
            continue;
        if (ri_pal_image_decode(file, nfile, &px, &w, &h) != 0 || !px) {
            free(file);
            continue;
        }
        free(file);
        if (ri_skin_bind(skin, i, px, (uint16_t)w, (uint16_t)h) != 0) {
            ri_pal_image_free(px);
            continue;
        }
        tw = w * num / 8u;
        th = h * num / 8u;
        if (!tw)
            tw = 1u;
        if (!th)
            th = 1u;
        scaled = (uint32_t *)malloc((size_t)tw * th * 4u);
        if (!scaled)
            continue;
        ri_skin_downscale(px, w, h, scaled, tw, th);
        if (ri_skin_bind_zoom(skin, i, scaled, (uint16_t)tw, (uint16_t)th) != 0)
            free(scaled);
    }
    return 0;
}

static const struct { uint8_t sec, z; uint32_t h; } T_PIN[] = {
    { 0u, 0u, 0xdfb9963eu },
    { 0u, 1u, 0xf517804du },
    { 0u, 2u, 0xf3fab28eu },
    { 0u, 3u, 0x54b69548u },
    { 1u, 0u, 0xdfb9963eu },
    { 1u, 1u, 0xf517804du },
    { 1u, 2u, 0xf3fab28eu },
    { 1u, 3u, 0x54b69548u },
    { 2u, 0u, 0xe7967b1du },
    { 2u, 1u, 0x30f72db9u },
    { 2u, 2u, 0x22ead32cu },
    { 2u, 3u, 0xfcc63febu },
    { 3u, 0u, 0x3a945fa4u },
    { 3u, 1u, 0x773857edu },
    { 3u, 2u, 0x1ebd33b5u },
    { 3u, 3u, 0xdab4ad43u },
    { 4u, 0u, 0x75b9e822u },
    { 4u, 1u, 0x13eb76cfu },
    { 4u, 2u, 0x48da0083u },
    { 4u, 3u, 0xd4a95c86u },
    { 5u, 0u, 0x1a589940u },
    { 5u, 1u, 0xafd03416u },
    { 5u, 2u, 0x9b2c46fbu },
    { 5u, 3u, 0x0f4f7b98u },
    { 6u, 0u, 0x2e26ea20u },
    { 6u, 1u, 0xde310c72u },
    { 6u, 2u, 0xea87d93bu },
    { 6u, 3u, 0x06ed109du },
    { 7u, 0u, 0x74ae20e4u },
    { 7u, 1u, 0x5ffd3716u },
    { 7u, 2u, 0xb1bf0e0fu },
    { 7u, 3u, 0xaf0e30adu },
    { 8u, 0u, 0xd0898aedu },
    { 8u, 1u, 0x3f39a759u },
    { 8u, 2u, 0x2e6c04e2u },
    { 8u, 3u, 0xdf8e9b9au },
    { 9u, 0u, 0x61a7cb09u },
    { 9u, 1u, 0x30abc0fau },
    { 9u, 2u, 0xd410477fu },
    { 9u, 3u, 0x51f30ffau },
    { 10u, 0u, 0x92647d79u },
    { 10u, 1u, 0x547865c5u },
    { 10u, 2u, 0x4dfc43ceu },
    { 10u, 3u, 0x2cb5f969u },
    { 11u, 0u, 0xeace214bu },
    { 11u, 1u, 0xaf133f14u },
    { 11u, 2u, 0x7ae9d4e7u },
    { 11u, 3u, 0x62946158u },
    { 12u, 0u, 0x0fe80faeu },
    { 12u, 1u, 0x2ec3d1f4u },
    { 12u, 2u, 0x94e3467eu },
    { 12u, 3u, 0xda4318cbu },
    { 13u, 0u, 0x88c72895u },
    { 13u, 1u, 0x0153a2edu },
    { 13u, 2u, 0x53c3c6fbu },
    { 13u, 3u, 0xd0f0d2d8u },
    { 14u, 0u, 0x01149021u },
    { 14u, 1u, 0x10d88c9cu },
    { 14u, 2u, 0x1a01eb0eu },
    { 14u, 3u, 0x1e5c492fu },
    { 15u, 0u, 0x41d931fcu },
    { 15u, 1u, 0xbd18996du },
    { 15u, 2u, 0xf217dc37u },
    { 15u, 3u, 0x1fadc16cu },
    { 16u, 0u, 0xa830acb4u },
    { 16u, 1u, 0x6ec4f384u },
    { 16u, 2u, 0xd5893f23u },
    { 16u, 3u, 0xfca76474u },
    { 17u, 0u, 0xbed3bf80u },
    { 17u, 1u, 0xf040cea0u },
    { 17u, 2u, 0xfc368a9bu },
    { 17u, 3u, 0x0376ba78u },
    { 18u, 0u, 0xb99f3c6fu },
    { 18u, 1u, 0x6be8bcf1u },
    { 18u, 2u, 0x79191fb1u },
    { 18u, 3u, 0x43067d9bu },
    { 19u, 0u, 0xc663c06au },
    { 19u, 1u, 0x854d5ab5u },
    { 19u, 2u, 0xecbfc991u },
    { 19u, 3u, 0x54d512dau },
    { 20u, 0u, 0xa603fae0u },
    { 20u, 1u, 0x1cf076e3u },
    { 20u, 2u, 0x0c328674u },
    { 20u, 3u, 0x8dff2147u },
};
static const uint32_t T_SKIN_PIN = 0x1237847fu;

static uint32_t pin_lookup(uint8_t sec, uint8_t z) {
    uint32_t i;
    for (i = 0u; i < sizeof(T_PIN) / sizeof(T_PIN[0]); i++)
        if (T_PIN[i].sec == sec && T_PIN[i].z == (uint8_t)z)
            return T_PIN[i].h;
    return 0u;
}

/* Rack furniture (owner 2026-09-28): property checks, not hash pins — the
 * owner is still tuning the look. */
static int t_green(uint32_t c) {
    int r = (int)(c >> 16 & 0xFF), g = (int)(c >> 8 & 0xFF), b = (int)(c & 0xFF);
    return g > 150 && g > r + 60 && g > b + 60;
}

static void t_art(struct ri_raster *r, uint32_t *px, uint32_t w, uint32_t h,
    void (*paint)(struct ri_dlist *, uint32_t, uint32_t, int), int arg) {
    struct ri_dlist dl;
    ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
    paint(&dl, w, h, arg);
    RI_ASSERT(dl.n > 0u && dl.n < dl.cap, "rack art emitted %u", dl.n);
    ri_raster_init(r, px, w, h);
    ri_raster_clear(r, 0xFF00FFu);
    ri_raster_replay(r, &dl, 0);
}
static void p_bay(struct ri_dlist *dl, uint32_t w, uint32_t h, int a) {
    (void)a;
    ri_art_bay(dl, 0, 0, (int)w - 1, (int)h - 1);
}
static void p_rail(struct ri_dlist *dl, uint32_t w, uint32_t h, int a) {
    (void)a;
    ri_art_rack_rail(dl, 0, 0, (int)w - 1, (int)h - 1);
}
static void p_power(struct ri_dlist *dl, uint32_t w, uint32_t h, int a) {
    ri_art_power(dl, 0, 0, (int)w - 1, (int)h - 1, "303A", a & 1, (a >> 1) & 1);
}
static void p_tab(struct ri_dlist *dl, uint32_t w, uint32_t h, int a) {
    ri_art_tab(dl, 0, 0, (int)w - 1, (int)h - 1, "SYNTHS", a & 1, (a >> 1) & 1);
}

/* Strip headers (owner 2026-09-29) read one face above the section face:
 * face M at z0 for every mixer strip and the master header. */
static void headerface_checks(void) {
    static const uint8_t secs[6] = { 4u, 5u, 6u, 7u, 8u, 20u };
    static const char *const want[6] =
        { "TB-303 A", "TB-303 B", "TR-808", "TR-909", "MASTER", "LEVI" };
    uint32_t s;
    for (s = 0u; s < 6u; s++) {
        struct RISectUI ui;
        struct ri_dlist dl;
        struct ri_text_metrics tm;
        uint32_t i;
        int found = 0;
        if (ri_sui_init(&ui, secs[s]) != 0)
            continue;
        if (ri_smix_strip(secs[s]) >= 0 || secs[s] == RI_SEC_MASTER)
            ri_sui_bind_board(&ui, &T_BOARD);
        ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
        tm.width = ri_raster_text_width;
        tm.height = 7;
        tm.baseline = 5;
        tm.ctx = 0;
        ri_draw_section(&dl, &ui, secs[s], 0, 0, 0, &tm, 0, 0);
        for (i = 0u; i < dl.n; i++)
            if (dl.cmd[i].op == RI_D_TEXT && dl.cmd[i].text &&
                !strcmp(dl.cmd[i].text, want[s])) {
                RI_ASSERT(dl.cmd[i].pad[0] == RI_FACE_M, "header face sec=%u", secs[s]);
                found = 1;
            }
        RI_ASSERT(found, "header text sec=%u", secs[s]);
    }
}

/* Legend-face parity (S2): a face-M line centres by ri_face_width with
 * its left edge exactly at cx - W/2 (same math on host and AROS). */
static void textface_checks(uint32_t *px) {
    struct ri_raster r;
    struct ri_dlist dl;
    const struct ri_face *f = ri_face_by_id(RI_FACE_M);
    const char *s = "CUTOFF 303";
    int w, x0, x, y, lo = 1 << 30, hi = -(1 << 30);
    RI_ASSERT(f != 0, "face M");
    w = ri_face_width(f, s);
    RI_ASSERT(w > 20, "face width %d", w);
    ri_dlist_init(&dl, T_BACK, 24576u, T_SPOOL, sizeof T_SPOOL);
    ri_draw_text_face(&dl, 100, 12, 1u, 0xFFFFFFu, RI_FACE_M, s);
    RI_ASSERT(dl.n == 1u && dl.cmd[0].pad[0] == RI_FACE_M, "face carried %u", dl.n);
    ri_raster_init(&r, px, 220u, 24u);
    ri_raster_clear(&r, 0x000000u);
    ri_raster_replay(&r, &dl, 0);
    x0 = 100 - w / 2;
    for (y = 0; y < 24; y++)
        for (x = 0; x < 220; x++)
            if ((px[(uint32_t)y * 220u + (uint32_t)x] & 0xFFFFFFu) != 0u) {
                if (x < lo)
                    lo = x;
                if (x > hi)
                    hi = x;
            }
    RI_ASSERT(lo == x0, "face left %d want %d", lo, x0);
    RI_ASSERT(hi == x0 + w - 1 - (int)f->adv_gap, "face right %d", hi);
}

static int t_luma(uint32_t c) {
    return (int)(((c >> 16 & 0xFF) * 299u + (c >> 8 & 0xFF) * 587u + (c & 0xFF) * 114u) / 1000u);
}

static void tab_checks(uint32_t *px) {
    struct ri_raster r;
    uint32_t x, y, on_green = 0u, off_green = 0u, hash_on, hash_off, hash_press;
    int face_luma, text_luma, dl = 0;
    /* Active shows a green LED strip; inactive shows none. */
    t_art(&r, px, 96u, 24u, p_tab, 1);
    for (y = 0u; y < 24u; y++)
        for (x = 0u; x < 96u; x++)
            on_green += t_green(px[y * 96u + x]);
    hash_on = ri_raster_hash(&r);
    t_art(&r, px, 96u, 24u, p_tab, 0);
    for (y = 0u; y < 24u; y++)
        for (x = 0u; x < 96u; x++)
            off_green += t_green(px[y * 96u + x]);
    hash_off = ri_raster_hash(&r);
    RI_ASSERT(on_green >= 20u, "tab on: %u green px", on_green);
    RI_ASSERT(off_green == 0u, "tab off: %u green px", off_green);
    RI_ASSERT(hash_on != hash_off, "tab active looks inactive");
    /* Label contrasts with the face (hardware printed legend). */
    face_luma = t_luma(px[23u * 96u + 2u] & 0xFFFFFFu);
    text_luma = 0;
    for (y = 8u; y < 22u; y++)
        for (x = 8u; x < 88u; x++) {
            int l = t_luma(px[y * 96u + x] & 0xFFFFFFu);
            if (l > text_luma)
                text_luma = l;
        }
    dl = text_luma - face_luma;
    if (dl < 0)
        dl = -dl;
    RI_ASSERT(dl >= 60, "tab label contrast %d (face %d text %d)", dl, face_luma, text_luma);
    /* Pressed sinks the cap. */
    t_art(&r, px, 96u, 24u, p_tab, 3);
    hash_press = ri_raster_hash(&r);
    RI_ASSERT(hash_press != hash_on, "tab pressed looks unpressed");
}

static void rack_checks(uint32_t *px) {
    struct ri_raster r;
    uint32_t x, y, n, prev, rows = 0u, magenta = 0u, lum = 0u;
    uint32_t on_green = 0u, off_green = 0u, lit_text = 0u, dim_text = 0u, hash_up, hash_dn;
    /* Bay: fully covered, dark, brushed (row tones vary down a column). */
    t_art(&r, px, 200u, 120u, p_bay, 0);
    prev = 0xFFFFFFFFu;
    for (y = 0u; y < 120u; y++) {
        uint32_t c = px[y * 200u + 100u] & 0xFFFFFFu;
        if (c != prev)
            rows++;
        prev = c;
        for (x = 0u; x < 200u; x++) {
            uint32_t p = px[y * 200u + x] & 0xFFFFFFu;
            magenta += p == 0xFF00FFu;
            lum += ((p >> 16 & 0xFF) + (p >> 8 & 0xFF) + (p & 0xFF)) / 3u;
        }
    }
    RI_ASSERT(magenta == 0u, "bay leaves %u px uncovered", magenta);
    RI_ASSERT(lum / (200u * 120u) < 70u, "bay too light: mean %u", lum / (200u * 120u));
    RI_ASSERT(rows >= 40u, "bay not brushed: %u tone changes down a column", rows);
    /* Rail: black holes and a light screw head near the top. */
    t_art(&r, px, 18u, 200u, p_rail, 0);
    for (n = 0u, y = 0u; y < 200u; y++)
        n += (px[y * 18u + 9u] & 0xFFFFFFu) == 0x0C0C0Du;
    RI_ASSERT(n >= 8u, "rail holes: %u px", n);
    for (n = 0u, y = 0u; y < 20u; y++)
        for (x = 4u; x < 14u; x++)
            n += ((px[y * 18u + x] >> 8) & 0xFF) > 0xB0u;
    RI_ASSERT(n >= 4u, "rail top screw head: %u light px", n);
    /* Power: the glyph is the LED (green only when on); label dims off. */
    t_art(&r, px, 90u, 26u, p_power, 1);
    for (y = 0u; y < 26u; y++)
        for (x = 0u; x < 26u; x++)
            on_green += t_green(px[y * 90u + x]);
    for (y = 0u; y < 26u; y++)
        for (x = 30u; x < 90u; x++)
            lit_text += ((px[y * 90u + x] >> 8) & 0xFF) > 0xD0u;
    hash_up = ri_raster_hash(&r);
    t_art(&r, px, 90u, 26u, p_power, 0);
    for (y = 0u; y < 26u; y++)
        for (x = 0u; x < 26u; x++)
            off_green += t_green(px[y * 90u + x]);
    for (y = 0u; y < 26u; y++)
        for (x = 30u; x < 90u; x++)
            dim_text += ((px[y * 90u + x] >> 8) & 0xFF) > 0xD0u;
    RI_ASSERT(on_green >= 20u, "power on: %u green px", on_green);
    RI_ASSERT(off_green == 0u, "power off: %u green px", off_green);
    RI_ASSERT(lit_text >= 10u && dim_text == 0u, "label on %u / off %u bright px", lit_text, dim_text);
    t_art(&r, px, 90u, 26u, p_power, 3);
    hash_dn = ri_raster_hash(&r);
    RI_ASSERT(hash_dn != hash_up, "pressed cap looks unpressed");
}

/* Levi hardware panel (fidelity plan P1, 2026-09-30): property checks at
 * 1.5x (the Dell's Levi zoom). Teal section titles, legends printed on
 * every live cap, a dark LCD page with teal text, encoder rings lit only
 * on live slots. */
static int t_is_teal(uint32_t c) {
    return (c & 0xFFFFFFu) == 0x22BCB9u;
}

static void levi_checks(uint32_t *px) {
    const struct RIGeoSection *g = ri_geo_section(RI_SEC_LEVI);
    uint32_t w, h, i, n, x, y, teal = 0u;
    uint32_t hash = render_one(RI_SEC_LEVI, 1, 0, px, 2048u * 1024u, 0);
    RI_ASSERT(g && hash != 0u, "levi render");
    if (!g || !hash)
        return;
    w = (uint32_t)ri_geo_px(g->w, 1);
    h = (uint32_t)ri_geo_px(g->h, 1);
    for (y = 0u; y < (uint32_t)ri_geo_px(40, 1); y++)
        for (x = 0u; x < w; x++)
            teal += t_is_teal(px[y * w + x]);
    RI_ASSERT(teal >= 200u, "levi teal titles: %u px", teal);
    for (i = 0u; i < g->nitems; i++) {
        const struct RIGeoItem *it = &g->items[i];
        uint32_t idx = it->reg_id & 0xFFu, bright = 0u;
        int cx = ri_geo_px(it->cx, 1), cy = ri_geo_px(it->cy, 1);
        int hw = ri_geo_px(it->w, 1) / 2, hh = ri_geo_px(it->h, 1) / 2, xx, yy;
        int cap = it->shape == RI_GEO_OPTION || (it->shape == RI_GEO_RECT &&
            (idx == RI_SLEVI_ARPON || idx == RI_SLEVI_SEQON || idx == RI_SLEVI_STEP || idx == RI_SLEVI_BACK));
        if (!cap)
            continue;
        for (yy = cy - hh + 1; yy < cy + hh; yy++)
            for (xx = cx - hw + 1; xx < cx + hw; xx++) {
                uint32_t c = px[(uint32_t)yy * w + (uint32_t)xx];
                int l = (int)(((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF)) / 3;
                bright += l > 70;
            }
        RI_ASSERT(bright >= 3u, "levi cap %u/%d has no legend (%u px)", idx, it->opt, bright);
    }
    for (i = 0u; i < g->nitems; i++) {
        const struct RIGeoItem *it = &g->items[i];
        uint32_t idx = it->reg_id & 0xFFu, lum = 0u, cnt = 0u, tl = 0u, white = 0u;
        int cx = ri_geo_px(it->cx, 1), cy = ri_geo_px(it->cy, 1);
        int hw = ri_geo_px(it->w, 1) / 2, hh = ri_geo_px(it->h, 1) / 2, xx, yy;
        if (idx != RI_SLEVI_PAGE && !(idx >= RI_SLEVI_ENC0 && idx < RI_SLEVI_ENC0 + 3u))
            continue;
        for (yy = cy - hh; yy <= cy + hh; yy++)
            for (xx = cx - hw; xx <= cx + hw; xx++) {
                uint32_t c = px[(uint32_t)yy * w + (uint32_t)xx];
                lum += (((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF)) / 3;
                cnt++;
                tl += t_is_teal(c);
                white += (c & 0xFFFFFFu) == 0xF2F3F3u;
            }
        if (idx == RI_SLEVI_PAGE)
            RI_ASSERT(lum / (cnt ? cnt : 1u) < 60u && tl >= 20u, "levi lcd dark %u teal %u", lum / (cnt ? cnt : 1u), tl);
        else if (idx == RI_SLEVI_ENC0 + 1u)                 /* OSC page slot 2 (WAVE): later phase */
            RI_ASSERT(white == 0u, "levi dead encoder ring lit (%u px)", white);
        else if (idx == RI_SLEVI_ENC0 + 2u)                 /* OSC page slot 3 (RATIO 32/127) */
            RI_ASSERT(white >= 4u, "levi live encoder ring dark (%u px)", white);
    }
    (void)h;
    (void)n;
}

int main(void) {
    /* Max raster: 909 z2-ish bounds; 2048x1024 covers every section. */
    static uint32_t px[2048u * 1024u];
    uint32_t s, z;
    mkdir("docs/evidence/gui/host-raster", 0755);
    ri_smix_init(&T_BOARD);
    for (s = 0u; s < sizeof(T_SECS); s++) {
        for (z = 0u; z < 4u; z++) {
            char shot[256];
            uint32_t h;
            snprintf(shot, sizeof shot, "docs/evidence/gui/host-raster/sec%02u-z%u.png",
                T_SECS[s], z);
            h = render_one(T_SECS[s], (int)z, 0, px, 2048u * 1024u,
                (T_SECS[s] == RI_SEC_SYNTH1 && z == 0u) ||
                    (T_SECS[s] == RI_SEC_909 && z == 0u) ? shot : 0);
            RI_ASSERT(h != 0u, "render sec=%u z=%u", T_SECS[s], z);
            RI_ASSERT(h == pin_lookup(T_SECS[s], z), "pin sec=%u z=%u got %08x", T_SECS[s], z, h);
        }
    }
    /* Skinned 808 (zoom cache at z0): exercises the IMAGE path. */
    {
        struct RISkin skin;
        uint32_t h;
        memset(&skin, 0, sizeof skin);
        RI_ASSERT(load_skin("skins/808-RI", 0, &skin) == 0, "skin load");
        h = render_one(RI_SEC_808, 0, &skin, px, 2048u * 1024u,
            "docs/evidence/gui/host-raster/sec02-skin-z0.png");
        RI_ASSERT(h != 0u, "skin render");
        RI_ASSERT(h == T_SKIN_PIN, "pin skin got %08x", h);
    }
    rack_checks(px);
    levi_checks(px);
    tab_checks(px);
    textface_checks(px);
    headerface_checks();
    RI_RESULT("raster_goldens");
}
