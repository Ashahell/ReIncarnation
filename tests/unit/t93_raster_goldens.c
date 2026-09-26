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

static struct ri_dcmd T_BACK[8192];
static char T_SPOOL[32768];
static struct RIMixBoard T_BOARD;

static const uint8_t T_SECS[] = {
    RI_SEC_SYNTH1, RI_SEC_SYNTH2, RI_SEC_808, RI_SEC_909,
    RI_SEC_MIX_SYNTH1, RI_SEC_MIX_SYNTH2, RI_SEC_MIX_808, RI_SEC_MIX_909, RI_SEC_MASTER,
    RI_SEC_PCF, RI_SEC_DELAY, RI_SEC_DIST, RI_SEC_COMP,
    RI_SEC_TRANSPORT, RI_SEC_PAT_SYNTH1, RI_SEC_PAT_SYNTH2, RI_SEC_PAT_808, RI_SEC_PAT_909
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
    ri_dlist_init(&dl, T_BACK, 8192u, T_SPOOL, sizeof T_SPOOL);
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
    { 0u, 0u, 0x30c59a04u },
    { 0u, 1u, 0x9760f422u },
    { 0u, 2u, 0x2861513cu },
    { 0u, 3u, 0xbe597133u },
    { 1u, 0u, 0x30c59a04u },
    { 1u, 1u, 0x9760f422u },
    { 1u, 2u, 0x2861513cu },
    { 1u, 3u, 0xbe597133u },
    { 2u, 0u, 0xb89a2b83u },
    { 2u, 1u, 0x2c062e4bu },
    { 2u, 2u, 0xe0d16e0eu },
    { 2u, 3u, 0xcf0a1b13u },
    { 3u, 0u, 0x0f1d57d8u },
    { 3u, 1u, 0x93fd9547u },
    { 3u, 2u, 0x0d12d1aau },
    { 3u, 3u, 0x62a4f764u },
    { 4u, 0u, 0x036f32bbu },
    { 4u, 1u, 0x1f75fbbbu },
    { 4u, 2u, 0xd40220adu },
    { 4u, 3u, 0x9855a21bu },
    { 5u, 0u, 0x036f32bbu },
    { 5u, 1u, 0x1f75fbbbu },
    { 5u, 2u, 0xd40220adu },
    { 5u, 3u, 0x9855a21bu },
    { 6u, 0u, 0x036f32bbu },
    { 6u, 1u, 0x1f75fbbbu },
    { 6u, 2u, 0xd40220adu },
    { 6u, 3u, 0x9855a21bu },
    { 7u, 0u, 0x036f32bbu },
    { 7u, 1u, 0x1f75fbbbu },
    { 7u, 2u, 0xd40220adu },
    { 7u, 3u, 0x9855a21bu },
    { 8u, 0u, 0x9cc55879u },
    { 8u, 1u, 0x9ac125b5u },
    { 8u, 2u, 0x6d5ac1e9u },
    { 8u, 3u, 0x389ebdedu },
    { 9u, 0u, 0x15199d6eu },
    { 9u, 1u, 0x9ee70ed6u },
    { 9u, 2u, 0xaf94e8beu },
    { 9u, 3u, 0x5b64888du },
    { 10u, 0u, 0x18a801e5u },
    { 10u, 1u, 0x6ee9f139u },
    { 10u, 2u, 0x40dfd8a1u },
    { 10u, 3u, 0x92c4a276u },
    { 11u, 0u, 0x6a43d9efu },
    { 11u, 1u, 0x94e15e9fu },
    { 11u, 2u, 0xf2beb7c7u },
    { 11u, 3u, 0xd4512d13u },
    { 12u, 0u, 0x7780b3b7u },
    { 12u, 1u, 0x6e7a3d93u },
    { 12u, 2u, 0x8ccf260bu },
    { 12u, 3u, 0x46cf61e4u },
    { 13u, 0u, 0x5ff44cc8u },
    { 13u, 1u, 0x84d08956u },
    { 13u, 2u, 0x67b55970u },
    { 13u, 3u, 0x4f336e08u },
    { 14u, 0u, 0x0a5ee4d6u },
    { 14u, 1u, 0xbc91c79eu },
    { 14u, 2u, 0xc01690dau },
    { 14u, 3u, 0x92b79c22u },
    { 15u, 0u, 0x0a5ee4d6u },
    { 15u, 1u, 0xbc91c79eu },
    { 15u, 2u, 0xc01690dau },
    { 15u, 3u, 0x92b79c22u },
    { 16u, 0u, 0x0a5ee4d6u },
    { 16u, 1u, 0xbc91c79eu },
    { 16u, 2u, 0xc01690dau },
    { 16u, 3u, 0x92b79c22u },
    { 17u, 0u, 0x0a5ee4d6u },
    { 17u, 1u, 0xbc91c79eu },
    { 17u, 2u, 0xc01690dau },
    { 17u, 3u, 0x92b79c22u },
};
static const uint32_t T_SKIN_PIN = 0xc0e692e5u;

static uint32_t pin_lookup(uint8_t sec, uint8_t z) {
    uint32_t i;
    for (i = 0u; i < sizeof(T_PIN) / sizeof(T_PIN[0]); i++)
        if (T_PIN[i].sec == sec && T_PIN[i].z == (uint8_t)z)
            return T_PIN[i].h;
    return 0u;
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
    RI_RESULT("raster_goldens");
}
