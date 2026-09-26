/* rsection_replay.inc — AROS display-list replayer (portability plan T2).
 * Pasting note: this file is #include-d by rsection.mcc.c, never compiled
 * alone. It replaces every direct painter (moved to gui/draw/): draw_section
 * builds the list, then replays it with pens, the friend bitmap and
 * WritePixelArrayAlpha — pixel-identical by construction (same font path,
 * same pen RGB, same blits).
 */

/* Pen from an RGB token (palette reverse-map; unknown falls back black). */
static void rpen(struct RastPort *rp, uint32_t rgb) {
    int i = ri_art_index(rgb);
    SetAPen(rp, (ULONG)s_pens[i >= 0 ? i : C_BLACK]);
}

static void rrect(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
    if (x1 < x0 || y1 < y0)
        return;
    rpen(rp, rgb);
    RectFill(rp, x0, y0, x1, y1);
}

static void rline(struct RastPort *rp, int x0, int y0, int x1, int y1, uint32_t rgb) {
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
static struct ri_dcmd s_dl_back[8192];
static char s_dl_spool[32768];

static void draw_section(struct RastPort *rp, struct RSectionData *dd, int ox, int oy) {
    struct ri_dlist dl;
    struct ri_text_metrics tm;
    uint32_t i;
    tm.width = rtext_width;
    tm.height = rp ? rp->TxHeight : 8;
    tm.baseline = rp ? rp->TxBaseline : 6;
    tm.ctx = rp;
    ri_dlist_init(&dl, s_dl_back, 8192u, s_dl_spool, sizeof s_dl_spool);
    ri_draw_section(&dl, &dd->ui, dd->ui.section, (int)dd->zoom, ox, oy, &tm,
        ri_skin_aros_active(), dd->panel);
    for (i = 0u; i < dl.n; i++) {
        const struct ri_dcmd *c = &dl.cmd[i];
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
