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
#define __DATATYPES_LIBBASE s_dtypes
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <graphics/rastport.h>
#include <clib/alib_protos.h>
#include <inline/cybergraphics.h>
#include <inline/datatypes.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "gui/skin.h"
#include "gui/skin_aros.h"

static struct Library *s_cyber;
static struct Library *s_dtypes;
static const struct RISkin *s_active;
/* Loader-owned buffers, indexed by part slot of s_owner. */
static const struct RISkin *s_owner;
static uint32_t *s_master[RI_SKIN_MAX_PARTS];
static uint32_t *s_scaled[RI_SKIN_MAX_PARTS];

static int lazy_bases(void) {
    if (!s_cyber)
        s_cyber = (struct Library *)OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    if (!s_dtypes)
        s_dtypes = (struct Library *)OpenLibrary((CONST_STRPTR)"datatypes.library", 0);
    return s_cyber && s_dtypes ? 1 : 0;
}

/* Forget every tracked buffer of skins other than `keep` (single-owner
 * table: only one skin's pixels are live at a time). */
static void drop_owner(const struct RISkin *keep) {
    uint32_t i;
    if (s_owner == keep)
        return;
    for (i = 0u; i < RI_SKIN_MAX_PARTS; i++) {
        if (s_master[i]) {
            FreeVec(s_master[i]);
            s_master[i] = 0;
        }
        if (s_scaled[i]) {
            FreeVec(s_scaled[i]);
            s_scaled[i] = 0;
        }
    }
    s_owner = keep;
}

/* Swizzle RGBA bytes (datatype order) to core ARGB words. The blit order
 * conversion (ARGB -> WritePixelArrayAlpha words) happens once per zoom
 * change via ri_skin_swizzle_blit, never per frame. */
static void rgba_to_argb(const unsigned char *src, uint32_t *dst, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++) {
        uint32_t r = src[4u * i], g = src[4u * i + 1u];
        uint32_t b = src[4u * i + 2u], a = src[4u * i + 3u];
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

static int decode_part(const char *path, uint32_t **out, uint32_t *w, uint32_t *h) {
    Object *dto;
    ULONG nw = 0u, nh = 0u, ok = 0u;
    uint32_t *px = 0, n;
    unsigned char *raw = 0;
    if (!lazy_bases())
        return -1;
    dto = (Object *)NewDTObject((APTR)path, DTA_SourceType, DTST_FILE,
                                DTA_GroupID, GID_PICTURE, TAG_DONE);
    if (!dto)
        return -2;
    GetDTAttrs(dto, DTA_NominalHoriz, &nw, DTA_NominalVert, &nh, TAG_DONE);
    if (nw == 0u || nh == 0u || nw > 4096u || nh > 4096u) {
        DisposeDTObject(dto);
        return -3;
    }
    n = nw * nh;
    px = (uint32_t *)AllocVec(n * 4u, MEMF_CLEAR);
    raw = (unsigned char *)AllocVec(n * 4u, 0);
    if (!px || !raw) {
        if (px)
            FreeVec(px);
        if (raw)
            FreeVec(raw);
        DisposeDTObject(dto);
        return -4;
    }
    ok = (ULONG)DoMethod(dto, PDTM_READPIXELARRAY, (IPTR)raw, (IPTR)PBPAFMT_RGBA,
                         (IPTR)(nw * 4u), (IPTR)0, (IPTR)0, (IPTR)nw, (IPTR)nh);
    DisposeDTObject(dto);
    if (!ok) {
        FreeVec(px);
        FreeVec(raw);
        return -5;
    }
    rgba_to_argb(raw, px, n);
    FreeVec(raw);
    *out = px;
    *w = nw;
    *h = nh;
    return 0;
}

int ri_skin_aros_load(const char *dir, struct RISkin *skin) {
    BPTR fh;
    char mpath[256], *text;
    LONG got = 0, total = 0;
    int rc, bound = 0;
    uint16_t i;
    char *base;
    if (!dir || !skin || dir[0] == '\0')
        return -1;
    drop_owner(skin);
    /* manifest path: dir + "/Skin.manifest" (bounded join) */
    {
        uint32_t k = 0u;
        while (dir[k] && k < 240u) {
            mpath[k] = dir[k];
            k++;
        }
        if (!k)
            return -1;
        if (mpath[k - 1u] != '/' && mpath[k - 1u] != ':') {
            if (k >= 240u)
                return -1;
            mpath[k++] = '/';
        }
        {
            static const char tail[] = "Skin.manifest";
            uint32_t j = 0u;
            while (tail[j] && k < 255u)
                mpath[k++] = tail[j++];
            if (tail[j])
                return -1;
        }
        mpath[k] = '\0';
    }
    fh = Open((CONST_STRPTR)mpath, MODE_OLDFILE);
    if (!fh)
        return -2;
    text = (char *)AllocVec(16384u + 1u, MEMF_CLEAR);
    if (!text) {
        Close(fh);
        return -2;
    }
    for (;;) {
        LONG r = Read(fh, text + total, 16384u - (uint32_t)total);
        if (r <= 0)
            break;
        total += r;
        got = total;
        if (total >= (LONG)16384)
            break;
    }
    Close(fh);
    if (got <= 0) {
        FreeVec(text);
        return -2;
    }
    text[total] = '\0';
    rc = ri_skin_parse(text, skin);
    FreeVec(text);
    if (rc != 0)
        return -3;
    /* mod name = last path component of dir */
    base = (char *)dir;
    {
        const char *p = dir;
        while (*p) {
            if (*p == '/' || *p == ':')
                base = (char *)p + 1;
            p++;
        }
    }
    {
        uint32_t k = 0u;
        while (base[k] && k < RI_SKIN_FILE_NAME) {
            skin->name[k] = base[k];
            k++;
        }
        skin->name[k] = '\0';
    }
    for (i = 0u; i < skin->nparts; i++) {
        char ppath[256];
        uint32_t k = 0u, j = 0u;
        uint32_t *px = 0, w = 0u, h = 0u;
        while (dir[k] && k < 128u) {
            ppath[k] = dir[k];
            k++;
        }
        if (k && ppath[k - 1u] != '/' && ppath[k - 1u] != ':')
            ppath[k++] = '/';
        while (skin->parts[i].file[j] && k < 255u)
            ppath[k++] = skin->parts[i].file[j++];
        if (skin->parts[i].file[j]) {
            skin->parts[i].rgba = 0; /* overlong path: fallback, keep going */
            continue;
        }
        ppath[k] = '\0';
        if (decode_part(ppath, &px, &w, &h) != 0 || !px) {
            skin->parts[i].rgba = 0; /* undecodable: Classic fallback */
            continue;
        }
        if (ri_skin_bind(skin, i, px, (uint16_t)w, (uint16_t)h) != 0) {
            FreeVec(px);
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
    if (w > 32767u || fh > 32767u)
        return 0;
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
                FreeVec(s_master[i]);
                s_master[i] = 0;
            }
            if (s_scaled[i]) {
                FreeVec(s_scaled[i]);
                s_scaled[i] = 0;
            }
        }
        s_owner = 0;
    }
    for (i = 0u; i < skin->nparts; i++) {
        skin->parts[i].rgba = 0;
        skin->parts[i].w = skin->parts[i].h = 0u;
        skin->parts[i].zrgba = 0;
        skin->parts[i].zw = skin->parts[i].zh = 0u;
    }
    if (s_active == skin)
        s_active = 0;
}

void ri_skin_aros_set_active(struct RISkin *skin) {
    s_active = skin;
}

const struct RISkin *ri_skin_aros_active(void) {
    return s_active;
}
