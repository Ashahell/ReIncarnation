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

static void replay_dl(struct RastPort *rp, const struct ri_dlist *dl);

static void draw_section(struct RastPort *rp, struct RSectionData *dd, int ox, int oy) {
    struct ri_dlist dl;
    struct ri_text_metrics tm;
    tm.width = rtext_width;
    tm.height = rp ? rp->TxHeight : 8;
    tm.baseline = rp ? rp->TxBaseline : 6;
    tm.ctx = rp;
    ri_dlist_init(&dl, s_dl_back, 24576u, s_dl_spool, sizeof s_dl_spool);
    ri_draw_section(&dl, &dd->ui, dd->ui.section, (int)dd->zoom, ox, oy, &tm,
        ri_skin_aros_active(), dd->panel);
    replay_dl(rp, &dl);
}

/* Rack furniture and other app-side art (riapp: bay, rails, power
 * buttons) replays through the same colour path. Pens and the direct-RGB
 * switch come from the section class's MUIM_Setup, so a section canvas
 * must be set up on the same screen (RIAPP always has the transport). */
void ri_rsection_replay(struct RastPort *rp, const struct ri_dlist *dl) {
    if (rp && dl)
        replay_dl(rp, dl);
}

static void replay_dl(struct RastPort *rp, const struct ri_dlist *dl) {
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
            ri_skin_aros_blit_idx(rp, ri_skin_aros_active(), (int)c->img, c->frame, c->x0, c->y0);
            break;
        case RI_D_CLIP:
            break; /* art never emits clips today */
        default:
            break;
        }
    }
    /* bevel() decomposes in the canvas; no direct backend needed. */
}
