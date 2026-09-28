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
#include "gui/ctlreg.h"
#include "gui/sectui.h"
#include "gui/sectmix.h"
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
    { 0u, 0u, 0xc47cd3c5u },
    { 0u, 1u, 0x00018449u },
    { 0u, 2u, 0xfa6a05f3u },
    { 0u, 3u, 0x0eccdb29u },
    { 1u, 0u, 0xc47cd3c5u },
    { 1u, 1u, 0x00018449u },
    { 1u, 2u, 0xfa6a05f3u },
    { 1u, 3u, 0x0eccdb29u },
    { 2u, 0u, 0x2105c559u },
    { 2u, 1u, 0x81ce007du },
    { 2u, 2u, 0xbce03e09u },
    { 2u, 3u, 0x1ee24c4eu },
    { 3u, 0u, 0x065b822fu },
    { 3u, 1u, 0xbb7e49b3u },
    { 3u, 2u, 0xf7f85387u },
    { 3u, 3u, 0x8ea4d158u },
    { 4u, 0u, 0xb55ebe13u },
    { 4u, 1u, 0x478f5a5au },
    { 4u, 2u, 0x458f1e3eu },
    { 4u, 3u, 0x3f2f717fu },
    { 5u, 0u, 0x97548624u },
    { 5u, 1u, 0xfd8c7a6fu },
    { 5u, 2u, 0x46c10a09u },
    { 5u, 3u, 0xa0ae358bu },
    { 6u, 0u, 0xc7e9f41cu },
    { 6u, 1u, 0xb13149d7u },
    { 6u, 2u, 0x799b0929u },
    { 6u, 3u, 0x26e94c3bu },
    { 7u, 0u, 0x35b460f8u },
    { 7u, 1u, 0xdf7571dfu },
    { 7u, 2u, 0x4be08a69u },
    { 7u, 3u, 0xe18d8b2bu },
    { 8u, 0u, 0x11ace733u },
    { 8u, 1u, 0x4c4fb259u },
    { 8u, 2u, 0xbe928047u },
    { 8u, 3u, 0xb0d7f400u },
    { 9u, 0u, 0xaacb4086u },
    { 9u, 1u, 0xb393e7e7u },
    { 9u, 2u, 0xfd6e7ee3u },
    { 9u, 3u, 0xe7c74874u },
    { 10u, 0u, 0xe3ec8863u },
    { 10u, 1u, 0x8fa8c77cu },
    { 10u, 2u, 0x2a2711fcu },
    { 10u, 3u, 0xd6c90d1eu },
    { 11u, 0u, 0x1f4e571cu },
    { 11u, 1u, 0x657392bbu },
    { 11u, 2u, 0xed8b7cd9u },
    { 11u, 3u, 0x8d33532au },
    { 12u, 0u, 0x00caeaf8u },
    { 12u, 1u, 0x59249702u },
    { 12u, 2u, 0x64930a55u },
    { 12u, 3u, 0x24570736u },
    { 13u, 0u, 0xf2264acdu },
    { 13u, 1u, 0x4dfb31f4u },
    { 13u, 2u, 0xc879672eu },
    { 13u, 3u, 0x4e2bd61au },
    { 14u, 0u, 0x07874516u },
    { 14u, 1u, 0x5bfd94a2u },
    { 14u, 2u, 0x8fd556a2u },
    { 14u, 3u, 0x1e2faf3du },
    { 15u, 0u, 0x3e90aad4u },
    { 15u, 1u, 0xa6dd2d1bu },
    { 15u, 2u, 0x33f31f9fu },
    { 15u, 3u, 0x9271f591u },
    { 16u, 0u, 0x65d99a64u },
    { 16u, 1u, 0x2d3a5883u },
    { 16u, 2u, 0x6dd25de7u },
    { 16u, 3u, 0x04ad5ce1u },
    { 17u, 0u, 0xf4a7c0c8u },
    { 17u, 1u, 0xe8b5e2d7u },
    { 17u, 2u, 0xb2403dc3u },
    { 17u, 3u, 0xb81b6285u },
    { 18u, 0u, 0xf619e7e4u },
    { 18u, 1u, 0x0a3e19cbu },
    { 18u, 2u, 0x7b6d196eu },
    { 18u, 3u, 0x312c0a18u },
    { 19u, 0u, 0xf602c09cu },
    { 19u, 1u, 0x44310248u },
    { 19u, 2u, 0xc4174a51u },
    { 19u, 3u, 0xd05fa851u },
    { 20u, 0u, 0xf7195a2cu },
    { 20u, 1u, 0x993c59e0u },
    { 20u, 2u, 0x4e5e0bd8u },
    { 20u, 3u, 0xfc3c91d7u },
};
static const uint32_t T_SKIN_PIN = 0x9a05fa63u;

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
    tab_checks(px);
    RI_RESULT("raster_goldens");
}
