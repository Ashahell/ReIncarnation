/* rsection_replay.inc — AROS display-list replayer (portability plan T2).
 * Pasting note: this file is #include-d by rsection.mcc.c, never compiled
 * alone. It replaces every direct painter (moved to gui/draw/): draw_section
 * builds the list, then replays it with pens, the friend bitmap and
 * WritePixelArrayAlpha — pixel-identical by construction (same font path,
 * same pen RGB, same blits).
 */

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
