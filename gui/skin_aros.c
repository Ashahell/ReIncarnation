/* gui/skin_aros.c — AROS-only skin/mod loader (§12.10 G8.1).
 *
 * AROS-ONLY. Manifest IO through dos.library (the sectproof Open/Read
 * pattern); images through datatypes picture.class into RGBA bytes,
 * swizzled to core 0xAARRGGBB words (same shuffle as the proven
 * knob_blit path); blits through cybergraphics WritePixelArrayAlpha.
 * Own static libbases (never the knob_blit CyberGfxBase global, so this
 * TU links with or without knob_blit.o). Loader-owned buffers are
 * tracked in this TU; the core never frees. Must NEVER enter the host
 * build (audit gates it with the other AROS-only TUs).
 */

#ifndef __AROS__
#error "gui/skin_aros.c is AROS-only: datatypes/cybergraphics, never in the host build"
#endif

#define __CYBERGRAPHICS_LIBBASE s_cyber
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/rastport.h>
#include <clib/alib_protos.h>
#include <inline/cybergraphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "gui/skin.h"
#include "gui/skin_aros.h"
#include "platform/pal/ri_pal_fs.h"
#include "platform/pal/ri_pal_image.h"

static struct Library *s_cyber;
static const struct RISkin *s_active;
/* Loader-owned buffers, indexed by part slot of s_owner. */
static const struct RISkin *s_owner;
static uint32_t *s_master[RI_SKIN_MAX_PARTS];
static uint32_t *s_scaled[RI_SKIN_MAX_PARTS];

/* Forget every tracked buffer of skins other than `keep` (single-owner
 * table: only one skin's pixels are live at a time). The dropped owner's
 * struct is unbound (no dangling pointers) and deactivated when active. */
static void drop_owner(const struct RISkin *keep) {
    uint32_t i;
    if (s_owner == keep)
        return;
    for (i = 0u; i < RI_SKIN_MAX_PARTS; i++) {
        if (s_master[i]) {
            ri_pal_image_free(s_master[i]); /* PAL-decoded (T7) */
            s_master[i] = 0;
        }
        if (s_scaled[i]) {
            FreeVec(s_scaled[i]);
            s_scaled[i] = 0;
        }
    }
    if (s_owner) {
        ri_skin_unbind((struct RISkin *)s_owner);
        if (s_active == s_owner)
            s_active = 0;
    }
    s_owner = keep;
}

/* Read a whole file into an AllocVec buffer (bounded 8 MB).
 * Decoding itself lives in platform/aros/image_dt.c (T7). */
static int read_whole(const char *path, unsigned char **out, uint32_t *n) {
    BPTR f;
    struct FileInfoBlock *fib;
    LONG size;
    unsigned char *b;
    LONG got = 0;
    if (!path || !out || !n)
        return -1;
    f = Open((CONST_STRPTR)path, MODE_OLDFILE);
    if (!f)
        return -1;
    fib = (struct FileInfoBlock *)AllocVec(sizeof *fib, MEMF_CLEAR);
    if (!fib) {
        Close(f);
        return -1;
    }
    if (!ExamineFH(f, fib)) {
        FreeVec(fib);
        Close(f);
        return -1;
    }
    size = fib->fib_Size;
    FreeVec(fib);
    if (size <= 0 || size > 8 * 1024 * 1024) {
        Close(f);
        return -1;
    }
    b = (unsigned char *)AllocVec((uint32_t)size + 1u, MEMF_CLEAR);
    if (!b) {
        Close(f);
        return -1;
    }
    while (got < size) {
        LONG r = Read(f, b + got, (uint32_t)size - (uint32_t)got);
        if (r <= 0)
            break;
        got += r;
    }
    Close(f);
    if (got != size) {
        FreeVec(b);
        return -1;
    }
    *out = b;
    *n = (uint32_t)size;
    return 0;
}

int ri_skin_aros_load(const char *dir, struct RISkin *skin) {
    char mpath[256];
    unsigned char *text = 0;
    uint32_t ntext = 0u;
    int rc, bound = 0;
    uint16_t i;
    char *base;
    if (!dir || !skin || dir[0] == '\0')
        return -1;
    if (s_owner == skin) {
        /* Reload into the active skin: release its buffers first so the
         * decode loop below never orphans them. */
        ri_skin_aros_free(skin);
    }
    drop_owner(skin);
    /* manifest path via PAL join (T6); file via read_whole (bounded). */
    if (ri_pal_path_join(mpath, sizeof mpath, dir, "Skin.manifest") != 0)
        return -1;
    if (read_whole(mpath, &text, &ntext) != 0 || !text || ntext == 0u)
        return -2;
    if (ntext > 16384u) {
        FreeVec(text);
        return -2;
    }
    text[ntext] = '\0'; /* read_whole has no NUL; manifest needs one */
    rc = ri_skin_parse((char *)text, skin);
    FreeVec(text);
    if (rc != 0)
        return -3;
    /* NAME must equal the mod directory (format 1): a renamed or copied
     * directory would otherwise store a wrong MODR name in songs. */
    base = (char *)dir;
    {
        const char *p = dir;
        while (*p) {
            if (*p == '/' || *p == ':')
                base = (char *)p + 1;
            p++;
        }
    }
    if (!ri_skin_name_matches(skin, base)) {
        memset(skin, 0, sizeof *skin);
        return -4;
    }
    for (i = 0u; i < skin->nparts; i++) {
        char ppath[256];
        unsigned char *file = 0;
        uint32_t nfile = 0u;
        uint32_t *px = 0, w = 0u, h = 0u;
        if (ri_pal_path_join(ppath, sizeof ppath, dir, skin->parts[i].file) != 0) {
            skin->parts[i].rgba = 0; /* overlong path: fallback, keep going */
            continue;
        }
        if (read_whole(ppath, &file, &nfile) != 0 || !file) {
            skin->parts[i].rgba = 0; /* unreadable: Classic fallback */
            continue;
        }
        if (ri_pal_image_decode(file, nfile, &px, &w, &h) != 0 || !px) {
            FreeVec(file);
            skin->parts[i].rgba = 0; /* undecodable: Classic fallback */
            continue;
        }
        FreeVec(file);
        if (ri_skin_bind(skin, i, px, (uint16_t)w, (uint16_t)h) != 0) {
            ri_pal_image_free(px); /* bad dims, or -2 stale size: counted in nstale */
            skin->parts[i].rgba = 0;
            continue;
        }
        s_master[i] = px;
        bound++;
    }
    if (bound > 0)
        ri_skin_aros_zoom(skin, 0); /* zoom cache always exists after load */
    return bound;
}

int ri_skin_aros_zoom(struct RISkin *skin, int zoom) {
    uint32_t num, i;
    if (!skin)
        return -1;
    num = ri_skin_zoom_num(zoom);
    if (!num)
        return -2;
    drop_owner(skin);
    {
        int scaled = 0;
        for (i = 0u; i < skin->nparts; i++) {
            uint32_t w = skin->parts[i].w, h = skin->parts[i].h;
            uint32_t tw, th, n;
            uint32_t *argb, *dst;
            if (!skin->parts[i].rgba || !w || !h)
                continue;
            tw = w * num / 8u;
            th = h * num / 8u;
            if (!tw)
                tw = 1u;
            if (!th)
                th = 1u;
            n = tw * th;
            if (s_scaled[i])
                FreeVec(s_scaled[i]);
            s_scaled[i] = 0;
            /* downscale in core ARGB order, then swizzle once to the
             * proven WritePixelArrayAlpha word order (R-G8.1-5). */
            argb = (uint32_t *)AllocVec(n * 4u, MEMF_CLEAR);
            dst = (uint32_t *)AllocVec(n * 4u, MEMF_CLEAR);
            if (!argb || !dst) {
                if (argb)
                    FreeVec(argb);
                if (dst)
                    FreeVec(dst);
                continue;
            }
            ri_skin_downscale(skin->parts[i].rgba, w, h, argb, tw, th);
            ri_skin_swizzle_blit(argb, dst, n);
            FreeVec(argb);
            if (ri_skin_bind_zoom(skin, i, dst, (uint16_t)tw, (uint16_t)th) != 0) {
                FreeVec(dst);
                continue;
            }
            s_scaled[i] = dst;
            scaled++;
        }
        return scaled;
    }
}

int ri_skin_aros_blit(struct RastPort *rp, const struct RISkin *skin,
                      uint8_t sec, uint8_t kind, const char *part,
                      uint32_t frame, int dx, int dy) {
    int idx;
    const uint32_t *px;
    uint32_t w, h, fh;
    ULONG rc;
    if (!rp || !skin || !part)
        return -1;
    if (!s_cyber && !(s_cyber = (struct Library *)OpenLibrary(
                         (CONST_STRPTR)"cybergraphics.library", 0)))
        return -1;
    idx = ri_skin_find(skin, sec, kind, part);
    if (idx < 0)
        return 0;
    /* Blits read the zoom cache only (built at load, rebuilt per zoom
     * change): masters stay in core ARGB order for future re-zooms. */
    px = skin->parts[idx].zrgba;
    w = skin->parts[idx].zw;
    h = skin->parts[idx].zh;
    if (!px || !w || !h || skin->parts[idx].frames == 0u)
        return 0;
    fh = h / skin->parts[idx].frames;
    if (!fh)
        return 0;
    if (frame >= skin->parts[idx].frames)
        frame = skin->parts[idx].frames - 1u;
    if (dx < 0 || dy < 0)
        return 0; /* canvases blit in-window; negatives are caller bugs */
    if (dx > 32767 || dy > 32767 || w > 16383u || fh > 32767u)
        return 0; /* WORD/UWORD casts below must not wrap; corrupt or
                   * oversize decodes fall back to Classic instead */
    rc = WritePixelArrayAlpha((APTR)(px + frame * fh * w), 0, 0, (UWORD)(w * 4u),
                              rp, (WORD)dx, (WORD)dy, (UWORD)w, (UWORD)fh,
                              0xffffffffUL);
    return rc ? 1 : 0;
}

void ri_skin_aros_free(struct RISkin *skin) {
    uint32_t i;
    if (!skin)
        return;
    if (s_owner == skin) {
        for (i = 0u; i < RI_SKIN_MAX_PARTS; i++) {
            if (s_master[i]) {
                ri_pal_image_free(s_master[i]); /* PAL-decoded (T7) */
                s_master[i] = 0;
            }
            if (s_scaled[i]) {
                FreeVec(s_scaled[i]);
                s_scaled[i] = 0;
            }
        }
        s_owner = 0;
    }
    ri_skin_unbind(skin);
    if (s_active == skin)
        s_active = 0;
}

void ri_skin_aros_set_active(struct RISkin *skin) {
    s_active = skin;
}

const struct RISkin *ri_skin_aros_active(void) {
    return s_active;
}
