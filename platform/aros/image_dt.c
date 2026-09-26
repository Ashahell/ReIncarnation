/* image_dt.c — AROS backend for ri_pal_image (portability plan T7).
 * AROS-only. Decodes via datatypes picture.class (size from
 * PDTA_BitMapHeader, RGBA pixel array, swizzled to core 0xAARRGGBB —
 * the same shuffle as the proven knob_blit path).
 * datatypes decodes from FILES, while the PAL interface takes a memory
 * buffer: stage through RI_PATH_TEMP (bounded name, deleted after).
 */
#ifndef __AROS__
#error "image_dt.c is AROS-only (portability plan T7)"
#endif

#include "platform/pal/ri_pal_image.h"
#include "platform/pal/ri_pal_fs.h"

#define __DATATYPES_LIBBASE s_dtypes
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/pictureclass.h>
#include <clib/alib_protos.h>
#include <inline/datatypes.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <string.h>

static struct Library *s_dtypes;

static int lazy_base(void) {
    if (!s_dtypes)
        s_dtypes = (struct Library *)OpenLibrary("datatypes.library", 0);
    return s_dtypes ? 1 : 0;
}

static void rgba_to_argb(const unsigned char *src, uint32_t *dst, uint32_t n) {
    uint32_t i;
    for (i = 0u; i < n; i++) {
        uint32_t r = src[4u * i], g = src[4u * i + 1u];
        uint32_t b = src[4u * i + 2u], a = src[4u * i + 3u];
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

static int decode_path(const char *path, uint32_t **out, uint32_t *w, uint32_t *h) {
    Object *dto;
    IPTR nw = 0u, nh = 0u, bmhp = 0u; /* GetDTAttrs stores IPTR: ULONG is 4 of 8 bytes */
    ULONG ok = 0u;
    uint32_t *px = 0, n;
    unsigned char *raw = 0;
    if (!lazy_base())
        return -1;
    dto = (Object *)NewDTObject((APTR)path, DTA_SourceType, DTST_FILE,
                                DTA_GroupID, GID_PICTURE, TAG_DONE);
    if (!dto)
        return -2;
    /* Picture size from the BitMapHeader (every picture.class subclass
     * sets it); DTA_Nominal* is only a fallback. The Dell's v11 png.datatype
     * 42.5 leaves DTA_Nominal* at 0 (Dell 2026-09-26: every part fell back). */
    GetDTAttrs(dto, PDTA_BitMapHeader, &bmhp, TAG_DONE);
    if (bmhp) {
        nw = ((struct BitMapHeader *)bmhp)->bmh_Width;
        nh = ((struct BitMapHeader *)bmhp)->bmh_Height;
    }
    if (nw == 0u || nh == 0u)
        GetDTAttrs(dto, DTA_NominalHoriz, &nw, DTA_NominalVert, &nh, TAG_DONE);
    if (nw == 0u || nh == 0u || nw > 4096u || nh > 4096u) {
        DisposeDTObject(dto);
        return -3;
    }
    n = (uint32_t)nw * (uint32_t)nh;
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
                         (IPTR)((uint32_t)nw * 4u), (IPTR)0, (IPTR)0,
                         (IPTR)nw, (IPTR)nh);
    DisposeDTObject(dto);
    if (!ok) {
        FreeVec(px);
        FreeVec(raw);
        return -5;
    }
    rgba_to_argb(raw, px, n);
    FreeVec(raw);
    *out = px;
    *w = (uint32_t)nw;
    *h = (uint32_t)nh;
    return 0;
}

int ri_pal_image_decode(const void *file, uint32_t len, uint32_t **rgba,
    uint32_t *w, uint32_t *h) {
    char base[48], stage[96];
    BPTR f;
    LONG wr;
    int rc;
    if (!file || len == 0u || !rgba || !w || !h)
        return 1;
    if (ri_pal_path(RI_PATH_TEMP, base, sizeof base) != 0)
        return 1;
    if (ri_pal_path_join(stage, sizeof stage, base, "riimg.tmp") != 0)
        return 1;
    f = Open((STRPTR)stage, MODE_NEWFILE);
    if (!f)
        return 1;
    wr = Write(f, (APTR)file, len);
    Close(f);
    if (wr != (LONG)len) {
        DeleteFile((STRPTR)stage);
        return 1;
    }
    rc = decode_path(stage, rgba, w, h);
    DeleteFile((STRPTR)stage);
    return rc;
}

void ri_pal_image_free(uint32_t *rgba) {
    if (rgba)
        FreeVec(rgba);
}
