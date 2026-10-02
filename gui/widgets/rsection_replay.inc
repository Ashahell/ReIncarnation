/* rsection_replay.inc — AROS display-list replayer (portability plan T2).
 * Pasting note: this file is #include-d by rsection.mcc.c, never compiled
 * alone. It replaces every direct painter (moved to gui/draw/): draw_section
 * builds the list, then replays it with pens, the friend bitmap and
 * WritePixelArrayAlpha — pixel-identical by construction (same font path,
 * same pen RGB, same blits).
 */
#include <aros/debug.h>
#include "gui/draw/font_legend.h"

/* Colour for one primitive. The shaded art (graded knobs, LEDs, panels)
 * needs arbitrary colours: on hi/truecolor screens rects and spans go
 * through cybergraphics FillPixelArray with the exact ARGB (this AROS
 * ignores RPTAG_FgColor — Dell 2026-09-28: every canvas painted one colour).
 * Pens (the nearest C_* palette entry) remain for text, diagonal lines
 * (pointers: palette colours) and palette screens. */
static int s_direct_rgb;

static int rpen_nearest(uint32_t rgb) {
    int i, best = C_BLACK;
    long bd = 0x7FFFFFFFL;
    for (i = 0; i < C_NCOL; i++) {
        uint32_t c = ri_art_rgb(i);
        long dr = (long)((rgb >> 16) & 0xFF) - (long)((c >> 16) & 0xFF);
        long dg = (long)((rgb >> 8) & 0xFF) - (long)((c >> 8) & 0xFF);
        long db = (long)(rgb & 0xFF) - (long)(c & 0xFF);
        long d = dr * dr * 3 + dg * dg * 6 + db * db;
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

static void rpen(struct RastPort *rp, uint32_t rgb) {
    int i = ri_art_index(rgb);
    SetAPen(rp, (ULONG)s_pens[i >= 0 ? i : rpen_nearest(rgb)]);
}

/* Exact-colour fill (truecolor): 1 when drawn, 0 = caller uses a pen. */
static int rfill_rgb(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    if (!s_direct_rgb || !s_rcyber || x1 < x0 || y1 < y0)
        return 0;
    FillPixelArray(rp, (UWORD)x0, (UWORD)y0, (UWORD)(x1 - x0 + 1), (UWORD)(y1 - y0 + 1),
        (ULONG)(0xFF000000u | (rgb & 0xFFFFFFu)));
    return 1;
}

static void rrect(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    if (x1 < x0 || y1 < y0)
        return;
    if (rfill_rgb(rp, x0, y0, x1, y1, rgb))
        return;
    rpen(rp, rgb);
    RectFill(rp, x0, y0, x1, y1);
}

static void rline(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    if ((x0 == x1 || y0 == y1) &&                  /* axis lines: exact colour */
        rfill_rgb(rp, x0 < x1 ? x0 : x1, y0 < y1 ? y0 : y1, x0 < x1 ? x1 : x0, y0 < y1 ? y1 : y0, rgb))
        return;
    rpen(rp, rgb);
    Move(rp, x0, y0);
    Draw(rp, x1, y1);
}

static void rcircle(struct RastPort *rp, int cx, int cy, int r, uint32_t rgb) {
    int dy;
    if (r < 0)
        return;
    rpen(rp, rgb);
    for (dy = -r; dy <= r; dy++) {
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r)
            dx--;
        if (!rfill_rgb(rp, cx - dx, cy + dy, cx + dx, cy + dy, rgb))
            RectFill(rp, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

static void rtext(struct RastPort *rp, int cx, int cy, const char *s, uint32_t rgb) {
    int n, w;
    if (!s || !s[0])
        return;
    n = (int)strlen(s);
    rpen(rp, rgb);
    SetDrMd(rp, JAM1);
    w = (int)TextLength(rp, (STRPTR)s, (ULONG)n);
    Move(rp, cx - w / 2, cy + rp->TxBaseline / 2);
    Text(rp, (STRPTR)s, (ULONG)n);
}

static int rtext_width(void *ctx, const char *s) {
    struct RastPort *rp = (struct RastPort *)ctx;
    int n;
    if (!rp || !s)
        return 0;
    n = (int)strlen(s);
    return n ? (int)TextLength(rp, (STRPTR)s, (ULONG)n) : 0;
}

/* Legend-face templates (S2): one packed 1bpp plane per face in chip
 * memory. AllocRaster, never AllocBitMap with a NULL friend (that returns
 * NULL on this AROS — the 2026-09-27 saga, step b). Planes are
 * screen-independent; pens still come from the section class setup.
 * Glyph i of face f starts at bit s_face_off[f][i]; rows are s_face_rb[f]
 * bytes wide (MSB = leftmost). BltTemplate draws with APen/JAM1; when a
 * plane is missing, 1px exact fills keep the string correct but slow. */
static UBYTE *s_face_plane[3];
static int s_face_rb[3], s_face_ph[3];
static uint16_t s_face_off[3][95];
static int s_face_built, s_face_failed, s_face_logged;

static void face_templates_build(void) {
    int f;
    s_face_built = 1;
    for (f = 0; f < 3; f++) {
        const struct ri_face *fc = ri_face_by_id(f + 1);
        int i, total = 0, pw, rb, hh;
        UBYTE *pl;
        if (!fc)
            continue;
        for (i = 0; i < 95; i++)
            total += (int)fc->g[i].w + (int)fc->adv_gap;
        pw = total + 32;
        hh = (int)fc->asc + (int)fc->desc;
        rb = ((pw + 15) / 16) * 2;
        pl = AllocRaster((ULONG)pw, (ULONG)hh);
        if (!pl)
            continue;
        memset(pl, 0, (size_t)rb * (size_t)hh);
        total = 0;
        for (i = 0; i < 95; i++) {
            const struct ri_glyph *gl = &fc->g[i];
            int r, c;
            s_face_off[f][i] = (uint16_t)total;
            for (r = 0; r < hh; r++)
                for (c = 0; c < gl->w; c++)
                    if (gl->rows[r] & (0x8000u >> c)) {
                        int bit = total + c, by = r * rb + bit / 8;
                        pl[by] |= (UBYTE)(1u << (7u - (unsigned)(bit % 8)));
                    }
            total += (int)gl->w + (int)fc->adv_gap;
        }
        s_face_plane[f] = pl;
        s_face_rb[f] = rb;
        s_face_ph[f] = hh;
    }
    if (!s_face_plane[0] && !s_face_plane[1] && !s_face_plane[2])
        s_face_failed = 1;
}

static void face_templates_free(void) {
    int f;
    for (f = 0; f < 3; f++) {
        const struct ri_face *fc = ri_face_by_id(f + 1);
        int pw = 0, i;
        if (!s_face_plane[f])
            continue;
        if (fc) {
            for (i = 0; i < 95; i++)
                pw += (int)fc->g[i].w + (int)fc->adv_gap;
            pw += 32;
        }
        FreeRaster(s_face_plane[f], (ULONG)pw, (ULONG)s_face_ph[f]);
        s_face_plane[f] = NULL;
    }
    s_face_built = s_face_failed = 0;
}

/* Legend-face string (S2): same placement math as the host rasterizer
 * (centred by ri_face_width, baseline at cy + cap/2), so both backends
 * show the same pixels. Colour is exact on truecolor: pens are
 * PRECISION_EXACT (or the 1px exact-fill fallback). */
static void rtext_face(struct RastPort *rp, int cx, int cy, const char *s, uint32_t rgb, int face_id) {
    const struct ri_face *f = ri_face_by_id(face_id);
    int w, x0, base, top, hh, x, i, fb = 0;
    if (!rp || !f || !s || !s[0])
        return;
    w = 0;
    for (i = 0; s[i]; i++) {
        unsigned c = (unsigned char)s[i];
        if (c >= 32u && c <= 126u)
            w += (int)f->g[c - 32u].w + (int)f->adv_gap;
    }
    if (!w)
        return;
    x0 = cx - w / 2;
    base = cy + (int)f->cap / 2;
    top = base - (int)f->asc;
    hh = (int)f->asc + (int)f->desc;
    rpen(rp, rgb);
    SetDrMd(rp, JAM1);
    if (!s_face_built && !s_face_failed)
        face_templates_build();
    x = x0;
    for (i = 0; s[i]; i++) {
        unsigned c = (unsigned char)s[i];
        const struct ri_glyph *gl;
        int off;
        if (c < 32u || c > 126u)
            continue;
        gl = &f->g[c - 32u];
        off = (int)s_face_off[face_id - 1][c - 32u];
        if (s_face_plane[face_id - 1] && gl->w > 0)
            BltTemplate(s_face_plane[face_id - 1] + (off / 16) * 2, (WORD)(off % 16),
                (WORD)s_face_rb[face_id - 1], rp, (WORD)x, (WORD)top, (WORD)gl->w, (WORD)hh);
        else {
            int r, cc;
            for (r = 0; r < hh; r++)
                for (cc = 0; cc < gl->w; cc++)
                    if (gl->rows[r] & (0x8000u >> cc))
                        rrect(rp, x + cc, top + r, x + cc, top + r, rgb);
            fb = 1;
        }
        x += (int)gl->w + (int)f->adv_gap;
    }
    if (fb && !s_face_logged) {
        s_face_logged = 1;
        kprintf("rsection: legend-face template missing, 1px fallback\n");
    }
}

/* Display-list backing: one GUI task draws at a time (MUI app task). */
static struct ri_dcmd s_dl_back[24576]; /* shaded art: ~8k cmds per 909 at 2x */
static char s_dl_spool[32768];

static void replay_dl(struct RastPort *rp, const struct ri_dlist *dl,
    const struct RISkin *skin);

/* Display-list backing: one GUI task draws at a time (MUI app task). */
static struct ri_dcmd s_dl_back[24576]; /* shaded art: ~8k cmds per 909 at 2x */
static char s_dl_spool[32768];

static void replay_dl(struct RastPort *rp, const struct ri_dlist *dl,
    const struct RISkin *skin);

/* S3 damage replay into an off-screen bitmap: only commands hitting the
 * box paint, clipped to it. Rects/spans intersect; lines clip
 * (Cohen–Sutherland, integer); face glyphs take BltTemplate when fully
 * inside, else per-pixel exact fills; skin parts sub-blit. Returns 1 ok,
 * 0 = bail to a full redraw (system-font text, or an image straddling
 * the box with no cybergraphics). Sections always carry a face (S2), and
 * Classic has no images, so bailouts stay off the hot path. */
static int dmg_empty(int x0, int y0, int x1, int y1) {
    return x1 < x0 || y1 < y0;
}

static void dmg_rect(int *x0, int *y0, int *x1, int *y1, int dx0, int dy0, int dx1, int dy1) {
    if (*x0 < dx0) *x0 = dx0;
    if (*y0 < dy0) *y0 = dy0;
    if (*x1 > dx1) *x1 = dx1;
    if (*y1 > dy1) *y1 = dy1;
}

static int dmg_clip_line(int *x0, int *y0, int *x1, int *y1, int dx0, int dy0, int dx1, int dy1) {
    int c0, c1;
    for (;;) {
        c0 = (*x0 < dx0 ? 1 : *x0 > dx1 ? 2 : 0) | (*y0 < dy0 ? 4 : *y0 > dy1 ? 8 : 0);
        c1 = (*x1 < dx0 ? 1 : *x1 > dx1 ? 2 : 0) | (*y1 < dy0 ? 4 : *y1 > dy1 ? 8 : 0);
        if (!(c0 | c1))
            return 1;
        if (c0 & c1)
            return 0;
        {
            int c = c0 ? c0 : c1, *x = c0 ? x0 : x1, *y = c0 ? y0 : y1;
            if (c & 1)
                *y = *y0 + (*y1 - *y0) * (dx0 - *x0) / (*x1 != *x0 ? *x1 - *x0 : 1), *x = dx0;
            else if (c & 2)
                *y = *y0 + (*y1 - *y0) * (dx1 - *x0) / (*x1 != *x0 ? *x1 - *x0 : 1), *x = dx1;
            else if (c & 4)
                *x = *x0 + (*x1 - *x0) * (dy0 - *y0) / (*y1 != *y0 ? *y1 - *y0 : 1), *y = dy0;
            else
                *x = *x0 + (*x1 - *x0) * (dy1 - *y0) / (*y1 != *y0 ? *y1 - *y0 : 1), *y = dy1;
        }
    }
}

static void rrect_dmg(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb,
    int dx0, int dy0, int dx1, int dy1) {
    dmg_rect(&x0, &y0, &x1, &y1, dx0, dy0, dx1, dy1);
    if (!dmg_empty(x0, y0, x1, y1))
        rrect(rp, x0, y0, x1, y1, rgb);
}

static void rline_dmg(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb,
    int dx0, int dy0, int dx1, int dy1) {
    if (dmg_clip_line(&x0, &y0, &x1, &y1, dx0, dy0, dx1, dy1))
        rline(rp, x0, y0, x1, y1, rgb);
}

static void rcircle_dmg(struct RastPort *rp, int cx, int cy, int r, uint32_t rgb,
    int dx0, int dy0, int dx1, int dy1) {
    int dy;
    if (r < 0)
        return;
    rpen(rp, rgb);
    for (dy = -r; dy <= r; dy++) {
        int dx = r, y = cy + dy, x0, x1;
        while (dx > 0 && dx * dx + dy * dy > r * r)
            dx--;
        if (y < dy0 || y > dy1)
            continue;
        x0 = cx - dx;
        x1 = cx + dx;
        dmg_rect(&x0, &y, &x1, &y, dx0, dy0, dx1, dy1);
        if (!dmg_empty(x0, y, x1, y)) {
            if (!rfill_rgb(rp, x0, y, x1, y, rgb))
                RectFill(rp, x0, y, x1, y);
        }
    }
}

/* Face glyph at (x, top), w x hh: template when fully inside, per-pixel
 * exact fills when straddling (glyphs are tiny; straddles are rare). */
static void rglyph_dmg(struct RastPort *rp, const struct ri_glyph *gl, int x, int top, int hh,
    uint32_t rgb, int face_id, int off, int dx0, int dy0, int dx1, int dy1) {
    int r, cc;
    if (gl->w <= 0)
        return;
    if (x >= dx0 && top >= dy0 && x + gl->w - 1 <= dx1 && top + hh - 1 <= dy1 &&
        s_face_plane[face_id - 1]) {
        BltTemplate(s_face_plane[face_id - 1] + (off / 16) * 2, (WORD)(off % 16),
            (WORD)s_face_rb[face_id - 1], rp, (WORD)x, (WORD)top, (WORD)gl->w, (WORD)hh);
        return;
    }
    for (r = 0; r < hh; r++) {
        int y = top + r;
        if (y < dy0 || y > dy1)
            continue;
        for (cc = 0; cc < gl->w; cc++) {
            int px = x + cc;
            if ((gl->rows[r] & (0x8000u >> cc)) && px >= dx0 && px <= dx1)
                rrect(rp, px, y, px, y, rgb);
        }
    }
}

static int rtext_face_dmg(struct RastPort *rp, int cx, int cy, const char *s, uint32_t rgb,
    int face_id, int dx0, int dy0, int dx1, int dy1) {
    const struct ri_face *f = ri_face_by_id(face_id);
    int w, x0, base, top, hh, x, i;
    if (!rp || !f || !s || !s[0])
        return 1;
    w = 0;
    for (i = 0; s[i]; i++) {
        unsigned c = (unsigned char)s[i];
        if (c >= 32u && c <= 126u)
            w += (int)f->g[c - 32u].w + (int)f->adv_gap;
    }
    if (!w)
        return 1;
    x0 = cx - w / 2;
    base = cy + (int)f->cap / 2;
    top = base - (int)f->asc;
    hh = (int)f->asc + (int)f->desc;
    rpen(rp, rgb);
    SetDrMd(rp, JAM1);
    if (!s_face_built && !s_face_failed)
        face_templates_build();
    x = x0;
    for (i = 0; s[i]; i++) {
        unsigned c = (unsigned char)s[i];
        if (c < 32u || c > 126u)
            continue;
        rglyph_dmg(rp, &f->g[c - 32u], x, top, hh, rgb, face_id,
            (int)s_face_off[face_id - 1][c - 32u], dx0, dy0, dx1, dy1);
        x += (int)f->g[c - 32u].w + (int)f->adv_gap;
    }
    return 1;
}

/* Skin part sub-blit clipped to the damage box (WritePixelArrayAlpha takes
 * source offsets). 1 ok (incl. nothing to do), 0 = bail to full redraw. */
static int image_dmg(struct RastPort *rp, const struct RISkin *skin, int idx, uint32_t frame,
    int dx, int dy, int dx0, int dy0, int dx1, int dy1) {
    const uint32_t *px;
    uint32_t w, h, fh;
    int ix0, iy0, ix1, iy1;
    ULONG rc;
    if (!skin)
        return 1;
    if (idx < 0 || idx >= (int)RI_SKIN_MAX_PARTS)
        return 1;
    px = skin->parts[idx].zrgba;
    w = skin->parts[idx].zw;
    h = skin->parts[idx].zh;
    if (!px || !w || !h || skin->parts[idx].frames == 0u)
        return 1;
    fh = h / skin->parts[idx].frames;
    if (!fh)
        return 1;
    if (frame >= skin->parts[idx].frames)
        frame = skin->parts[idx].frames - 1u;
    ix0 = dx > dx0 ? dx : dx0;
    iy0 = dy > dy0 ? dy : dy0;
    ix1 = dx + (int)w - 1 < dx1 ? dx + (int)w - 1 : dx1;
    iy1 = dy + (int)fh - 1 < dy1 ? dy + (int)fh - 1 : dy1;
    if (ix1 < ix0 || iy1 < iy0)
        return 1;
    if (!s_rcyber)
        return 0;
    rc = WritePixelArrayAlpha((APTR)(px + frame * fh * w), (WORD)(ix0 - dx), (WORD)(iy0 - dy),
        (UWORD)(w * 4u), rp, (WORD)ix0, (WORD)iy0, (UWORD)(ix1 - ix0 + 1), (UWORD)(iy1 - iy0 + 1),
        0xffffffffUL);
    return rc ? 1 : 1;
}

static int replay_dl_dmg(struct RastPort *rp, const struct ri_dlist *dl,
    const struct RISkin *skin, int dx0, int dy0, int dx1, int dy1) {
    uint32_t i;
    for (i = 0u; i < dl->n; i++) {
        const struct ri_dcmd *c = &dl->cmd[i];
        if (!ri_dcmd_hits_box(c, dx0, dy0, dx1, dy1))
            continue;
        switch (c->op) {
        case RI_D_RECT:
            rrect_dmg(rp, c->x0, c->y0, c->x1, c->y1, c->rgb, dx0, dy0, dx1, dy1);
            break;
        case RI_D_LINE:
            rline_dmg(rp, c->x0, c->y0, c->x1, c->y1, c->rgb, dx0, dy0, dx1, dy1);
            break;
        case RI_D_CIRCLE:
            rcircle_dmg(rp, c->x0, c->y0, c->x1, c->rgb, dx0, dy0, dx1, dy1);
            break;
        case RI_D_TEXT:
            if (!c->pad[0])
                return 0; /* system text: bail to full */
            rtext_face_dmg(rp, c->x0, c->y0, c->text, c->rgb, c->pad[0], dx0, dy0, dx1, dy1);
            break;
        case RI_D_IMAGE:
            if (!image_dmg(rp, skin, (int)c->img, c->frame, c->x0, c->y0,
                dx0, dy0, dx1, dy1))
                return 0;
            break;
        case RI_D_CLIP:
            break;
        default:
            break;
        }
    }
    return 1;
}

/* cx0..cy1 is a damage clip box, or an inverted box (cx1 < cx0) for "build
 * the whole section". The clip is set AFTER ri_dlist_init, which clears it.
 * Clipping at push time is exactly equivalent to building everything and
 * letting the backend's clipped replay skip the same commands (Dell
 * 2026-10-02: build_dl was 59 % of a box repaint, ~96 chase boxes/window). */
static void build_dl(struct RastPort *rp, struct RSectionData *dd, int ox, int oy,
    struct ri_dlist *dl, int cx0, int cy0, int cx1, int cy1) {
    struct ri_text_metrics tm;
    tm.width = rtext_width;
    tm.height = rp ? rp->TxHeight : 8;
    tm.baseline = rp ? rp->TxBaseline : 6;
    tm.ctx = rp;
    ri_dlist_init(dl, s_dl_back, 24576u, s_dl_spool, sizeof s_dl_spool);
    ri_dlist_set_clip(dl, cx0, cy0, cx1, cy1); /* inverted box => no clip */
    ri_draw_section(dl, &dd->ui, dd->ui.section, (int)dd->zoom, ox, oy, &tm,
        ri_skin_aros_for(dd->ui.section), dd->panel);
    ri_dlist_clear_clip(dl);
}

static void draw_section(struct RastPort *rp, struct RSectionData *dd, int ox, int oy) {
    struct ri_dlist dl;
    build_dl(rp, dd, ox, oy, &dl, -1, -1, -2, -2); /* inverted => whole section */
    replay_dl(rp, &dl, ri_skin_aros_for(dd->ui.section));
}

/* Rack furniture and other app-side art (riapp: bay, rails, power
 * buttons) replays through the same colour path. Pens and the direct-RGB
 * switch come from the section class's MUIM_Setup, so a section canvas
 * must be set up on the same screen (RIAPP always has the transport). */
void ri_rsection_replay(struct RastPort *rp, const struct ri_dlist *dl) {
    if (rp && dl)
        replay_dl(rp, dl, 0); /* app furniture is procedural (unskinned) */
}

static void replay_dl(struct RastPort *rp, const struct ri_dlist *dl,
    const struct RISkin *skin) {
    uint32_t i;
    for (i = 0u; i < dl->n; i++) {
        const struct ri_dcmd *c = &dl->cmd[i];
        switch (c->op) {
        case RI_D_RECT:
            rrect(rp, c->x0, c->y0, c->x1, c->y1, c->rgb);
            break;
        case RI_D_LINE:
            rline(rp, c->x0, c->y0, c->x1, c->y1, c->rgb);
            break;
        case RI_D_CIRCLE:
            rcircle(rp, c->x0, c->y0, c->x1, c->rgb);
            break;
        case RI_D_TEXT:
            if (c->pad[0])
                rtext_face(rp, c->x0, c->y0, c->text, c->rgb, c->pad[0]);
            else
                rtext(rp, c->x0, c->y0, c->text, c->rgb);
            break;
        case RI_D_IMAGE:
            ri_skin_aros_blit_idx(rp, skin, (int)c->img, c->frame, c->x0, c->y0);
            break;
        case RI_D_CLIP:
            break; /* art never emits clips today */
        default:
            break;
        }
    }
    /* bevel() decomposes in the canvas; no direct backend needed. */
}
