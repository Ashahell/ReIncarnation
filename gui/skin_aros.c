/* gui/skin_aros.c — AROS-only skin/mod loader (§12.10 G8.1, S7 registry).
 *
 * AROS-ONLY. Manifest IO through dos.library (the sectproof Open/Read
 * pattern); images through datatypes picture.class into RGBA bytes,
 * swizzled to core 0xAARRGGBB words (same shuffle as the proven
 * knob_blit path); blits through cybergraphics WritePixelArrayAlpha.
 * Own static libbases (never the knob_blit CyberGfxBase global, so this
 * TU links with or without knob_blit.o). Loader-owned buffers are
 * tracked per registry slot; the core never frees. Must NEVER enter
 * the host build (audit gates it with the other AROS-only TUs).
 *
 * S7: one shared load per mod directory (slots, refcounted through the
 * pure ri_skinuse_* core, which the host tests pin). The canvas asks
 * for() its section every draw; a failed load stays slotted but empty
 * so its sections fall back to Classic (never a substitute).
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
#include "gui/skinsect.h"
#include "platform/pal/ri_pal_fs.h"
#include "platform/pal/ri_pal_image.h"

static struct Library *s_cyber;

/* One shared load. Pixel buffers parallel the skin's part slots. */
static struct {
    struct RISkin skin;
    uint32_t *master[RI_SKIN_MAX_PARTS];
    uint32_t *scaled[RI_SKIN_MAX_PARTS];
    int8_t zoom;   /* zoom cache level, -1 = none built */
    uint8_t ok;    /* manifest parsed (parts may still fall back piecemeal) */
} s_slots[RI_SKIN_AROS_SLOTS];
static struct RISkinUse s_uses[RI_SKIN_AROS_SLOTS];
static struct RISkinAssign s_assign;
static int s_have_assign;
/* Previously wanted set: sync releases (prev - curr) and acquires
 * (curr - prev), so refcounts stay 0/1 and loads persist across syncs. */
static char s_prev[RI_SKIN_AROS_SLOTS][RI_SKINSECT_NAME + 1u];
static uint32_t s_nprev;

/* Free every loader-owned buffer of slot i and unbind it. Keeps the
 * manifest/lookup (name/version/parts) so a retry is cheap. */
static void slot_free(uint32_t i) {
    uint32_t k;
    if (i >= RI_SKIN_AROS_SLOTS)
        return;
    for (k = 0u; k < RI_SKIN_MAX_PARTS; k++) {
        if (s_slots[i].master[k]) {
            ri_pal_image_free(s_slots[i].master[k]); /* PAL-decoded (T7) */
            s_slots[i].master[k] = 0;
        }
        if (s_slots[i].scaled[k]) {
            FreeVec(s_slots[i].scaled[k]);
            s_slots[i].scaled[k] = 0;
        }
    }
    ri_skin_unbind(&s_slots[i].skin);
    s_slots[i].zoom = -1;
    s_slots[i].ok = 0;
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

/* (Re)build the zoom-scaled cache of slot i. Idempotent per (slot, zoom)
 * (callers skip when slot.zoom already matches). */
static int slot_zoom(uint32_t i, int zoom) {
    struct RISkin *skin;
    uint32_t num, k;
    if (i >= RI_SKIN_AROS_SLOTS)
        return -1;
    skin = &s_slots[i].skin;
    num = ri_skin_zoom_num(zoom);
    if (!num)
        return -2;
    {
        int scaled = 0;
        for (k = 0u; k < skin->nparts; k++) {
            uint32_t w = skin->parts[k].w, h = skin->parts[k].h;
            uint32_t tw, th, n;
            uint32_t *argb, *dst;
            if (!skin->parts[k].rgba || !w || !h)
                continue;
            tw = w * num / 8u;
            th = h * num / 8u;
            if (!tw)
                tw = 1u;
            if (!th)
                th = 1u;
            n = tw * th;
            if (s_slots[i].scaled[k])
                FreeVec(s_slots[i].scaled[k]);
            s_slots[i].scaled[k] = 0;
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
            ri_skin_downscale(skin->parts[k].rgba, w, h, argb, tw, th);
            ri_skin_swizzle_blit(argb, dst, n);
            FreeVec(argb);
            if (ri_skin_bind_zoom(skin, k, dst, (uint16_t)tw, (uint16_t)th) != 0) {
                FreeVec(dst);
                continue;
            }
            s_slots[i].scaled[k] = dst;
            scaled++;
        }
        s_slots[i].zoom = (int8_t)zoom;
        return scaled;
    }
}

/* Load dir/Skin.manifest + part images into an empty slot. Returns parts
 * bound (>= 0); manifest problems return -2/-3/-4 like the G8.1 loader
 * (missing/undecodable/wrong-size parts are NOT errors: Classic
 * fallback per part, counted in nstale for the caller to report). */
static int slot_load(uint32_t i, const char *dir) {
    struct RISkin *skin;
    char mpath[256];
    unsigned char *text = 0;
    uint32_t ntext = 0u;
    int rc, bound = 0;
    uint16_t k;
    char *base;
    if (i >= RI_SKIN_AROS_SLOTS || !dir || dir[0] == '\0')
        return -1;
    skin = &s_slots[i].skin;
    slot_free(i);
    memset(skin, 0, sizeof *skin);
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
    for (k = 0u; k < skin->nparts; k++) {
        char ppath[256];
        unsigned char *file = 0;
        uint32_t nfile = 0u;
        uint32_t *px = 0, w = 0u, h = 0u;
        if (ri_pal_path_join(ppath, sizeof ppath, dir, skin->parts[k].file) != 0) {
            skin->parts[k].rgba = 0; /* overlong path: fallback, keep going */
            continue;
        }
        if (read_whole(ppath, &file, &nfile) != 0 || !file) {
            skin->parts[k].rgba = 0; /* unreadable: Classic fallback */
            continue;
        }
        if (ri_pal_image_decode(file, nfile, &px, &w, &h) != 0 || !px) {
            FreeVec(file);
            skin->parts[k].rgba = 0; /* undecodable: Classic fallback */
            continue;
        }
        FreeVec(file);
        if (ri_skin_bind(skin, k, px, (uint16_t)w, (uint16_t)h) != 0) {
            ri_pal_image_free(px); /* bad dims, or -2 stale size: counted in nstale */
            skin->parts[k].rgba = 0;
            continue;
        }
        s_slots[i].master[k] = px;
        bound++;
    }
    s_slots[i].ok = 1;
    return bound;
}

int ri_skin_aros_sync(const struct RISkinAssign *a, const char *mods_dir,
                      int zoom) {
    /* Desired set: distinct non-empty effective mods, insertion order. */
    char want[RI_SKIN_AROS_SLOTS][RI_SKINSECT_NAME + 1u];
    uint32_t nw = 0u, s, i, k;
    int loaded = 0;
    if (!a || !mods_dir || mods_dir[0] == '\0')
        return -1;
    if (ri_skin_zoom_num(zoom) == 0u)
        return -2;
    s_assign = *a;
    s_have_assign = 1;
    for (s = 0u; s < RI_SEC_COUNT && nw < RI_SKIN_AROS_SLOTS; s++) {
        const char *m = ri_skinassign_get(a, s);
        uint32_t j;
        if (!m || m[0] == '\0')
            continue;
        for (j = 0u; j < nw; j++) {
            uint32_t c = 0u;
            while (c <= RI_SKINSECT_NAME && want[j][c] == m[c]) {
                if (m[c] == '\0')
                    break;
                c++;
            }
            if (m[c] == '\0' && want[j][c] == '\0')
                break;
        }
        if (j < nw)
            continue;
        for (k = 0u; k < RI_SKINSECT_NAME && m[k]; k++)
            want[nw][k] = m[k];
        want[nw][k] = '\0';
        nw++;
    }
    /* Release slots no longer wanted (prev minus curr). */
    for (i = 0u; i < s_nprev; i++) {
        uint32_t j;
        int32_t slot = -1;
        for (j = 0u; j < nw; j++) {
            uint32_t c = 0u;
            while (c <= RI_SKINSECT_NAME && s_prev[i][c] == want[j][c]) {
                if (want[j][c] == '\0')
                    break;
                c++;
            }
            if (want[j][c] == '\0' && s_prev[i][c] == '\0')
                break;
        }
        if (j < nw)
            continue;
        /* Find the parallel buffer slot first: release clears the name. */
        for (j = 0u; j < RI_SKIN_AROS_SLOTS; j++) {
            uint32_t c = 0u;
            if (s_uses[j].users == 0u)
                continue;
            while (c <= RI_SKINSECT_NAME && s_uses[j].mod[c] == s_prev[i][c]) {
                if (s_prev[i][c] == '\0')
                    break;
                c++;
            }
            if (s_prev[i][c] == '\0' && s_uses[j].mod[c] == '\0') {
                slot = (int32_t)j;
                break;
            }
        }
        if (slot < 0)
            continue;
        if (ri_skinuse_release(s_uses, RI_SKIN_AROS_SLOTS, s_prev[i]) == 0 &&
            s_uses[slot].users == 0u)
            slot_free((uint32_t)slot);
    }
    /* Acquire wanted mods (curr minus prev); load on miss, retry fails. */
    for (i = 0u; i < nw; i++) {
        char dir[256];
        int32_t s;
        uint32_t d = 0u, p;
        for (p = 0u; p < s_nprev; p++) {
            uint32_t c = 0u;
            while (c <= RI_SKINSECT_NAME && s_prev[p][c] == want[i][c]) {
                if (want[i][c] == '\0')
                    break;
                c++;
            }
            if (want[i][c] == '\0' && s_prev[p][c] == '\0')
                break;
        }
        if (p < s_nprev)
            continue;
        while (mods_dir[d] && d < 190u) {
            dir[d] = mods_dir[d];
            d++;
        }
        if (dir[d - 1u] != ':' && dir[d - 1u] != '/') {
            if (d >= 255u)
                continue;
            dir[d++] = '/';
        }
        for (k = 0u; k < RI_SKINSECT_NAME && want[i][k]; k++) {
            if (d >= 255u)
                break;
            dir[d++] = want[i][k];
        }
        if (want[i][k] != '\0')
            continue;
        dir[d] = '\0';
        if (ri_skinuse_acquire(s_uses, RI_SKIN_AROS_SLOTS, want[i]) != 0)
            continue;
        /* Slot index parallels the uses table (first acquire takes the
         * first free slot, in the same order). */
        for (s = 0; s < (int32_t)RI_SKIN_AROS_SLOTS; s++) {
            uint32_t c = 0u;
            while (c <= RI_SKINSECT_NAME && s_uses[s].mod[c] == want[i][c]) {
                if (want[i][c] == '\0')
                    break;
                c++;
            }
            if (want[i][c] == '\0' && s_uses[s].mod[c] == '\0')
                break;
        }
        if (s >= (int32_t)RI_SKIN_AROS_SLOTS) {
            ri_skinuse_release(s_uses, RI_SKIN_AROS_SLOTS, want[i]);
            continue;
        }
        if (!s_slots[s].ok)
            slot_load((uint32_t)s, dir);
    }
    /* Zoom every held load to the requested level (kept slots skip the
     * acquire loop above, so this pass is where zoom changes land). */
    for (i = 0u; i < RI_SKIN_AROS_SLOTS; i++) {
        if (s_uses[i].users == 0u || !s_slots[i].ok)
            continue;
        if (s_slots[i].zoom != (int8_t)zoom)
            slot_zoom(i, zoom);
        loaded++;
    }
    /* Remember the wanted set for the next diff. */
    for (i = 0u; i < nw; i++) {
        for (k = 0u; k <= RI_SKINSECT_NAME; k++) {
            s_prev[i][k] = want[i][k];
            if (want[i][k] == '\0')
                break;
        }
    }
    s_nprev = nw;
    return loaded;
}

const struct RISkin *ri_skin_aros_for(uint8_t section) {
    const char *m;
    uint32_t i;
    if (!s_have_assign)
        return 0;
    m = ri_skinassign_get(&s_assign, section);
    if (!m || m[0] == '\0')
        return 0;
    for (i = 0u; i < RI_SKIN_AROS_SLOTS; i++) {
        uint32_t c = 0u;
        if (s_uses[i].users == 0u)
            continue;
        while (c <= RI_SKINSECT_NAME && s_uses[i].mod[c] == m[c]) {
            if (m[c] == '\0')
                break;
            c++;
        }
        if (m[c] == '\0' && s_uses[i].mod[c] == '\0')
            return &s_slots[i].skin;
    }
    return 0;
}

int ri_skin_aros_blit(struct RastPort *rp, const struct RISkin *skin,
                      uint8_t sec, uint8_t kind, const char *part,
                      uint32_t frame, int dx, int dy) {
    int idx;
    if (!rp || !skin || !part)
        return -1;
    if (!s_cyber && !(s_cyber = (struct Library *)OpenLibrary(
                         (CONST_STRPTR)"cybergraphics.library", 0)))
        return -1;
    idx = ri_skin_find(skin, sec, kind, part);
    if (idx < 0)
        return 0;
    return ri_skin_aros_blit_idx(rp, skin, idx, frame, dx, dy);
}

int ri_skin_aros_blit_idx(struct RastPort *rp, const struct RISkin *skin,
    int idx, uint32_t frame, int dx, int dy) {
    const uint32_t *px;
    uint32_t w, h, fh;
    ULONG rc;
    if (!rp || !skin || idx < 0 || idx >= (int)RI_SKIN_MAX_PARTS)
        return -1;
    if (!s_cyber && !(s_cyber = (struct Library *)OpenLibrary(
                         (CONST_STRPTR)"cybergraphics.library", 0)))
        return -1;
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
