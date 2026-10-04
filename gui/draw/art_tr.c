/* art_tr.c — transport background + anchored text (portability plan T2). */
#include "gui/draw/art.h"

#include <string.h>
#include "gui/draw/font_legend.h"
#include "gui/panelgeo.h"

/* ---- Which song is playing ------------------------------------------
 *
 * "The user cannot see which song is playing" was the report (owner
 * 2026-10-03), and it is a real gap: the log has said
 * `RIAPP song <path>: N bars at M BPM` since songs landed, so the only way to
 * find out what was playing was to pull the log off a guest by hand.
 *
 * This is deliberately NOT a ctlreg entry. A RI_STR_* control has to be
 * automatable, stepper-clampable and MIDI-mappable to satisfy ctlreg, and a
 * read-only caption wants none of those; it would also put a non-numeric value
 * into a display that renders numbers. The label is plate decoration, exactly
 * like "PATTERN" and "SONG MODE" beside it, so it lives here with them.
 *
 * Every bound in this file is written as `n + 1u <= sizeof dst`, i.e. the
 * terminator slot is inside the comparison, and the terminator is then written
 * outside the loop. The first version wrote it the other way round --
 * `n + 1u < sizeof dst && src[n]` -- and that shape survived 14 of 15
 * mutants in mut_fixM9, because the test cannot see a buffer that happens to
 * be large enough on the day. The form below leaves no gap between the last
 * byte written and the byte after it.
 */
#define TR_SONG_MAX RI_ART_TR_SONG_MAX

static char tr_song_name[TR_SONG_MAX];

/* Bounded copy, shared by the setter and the draw path so there is one shape
 * to get right. Copies at most cap-1 bytes and always terminates. */
void ri_art_tr_copy(char *dst, uint32_t cap, const char *src) {
    uint32_t n = 0u;
    if (!dst || cap == 0u)
        return;
    if (!src)
        src = "";
    while (n + 1u <= cap - 1u && src[n] != '\0') {
        dst[n] = src[n];
        n++;
    }
    dst[n] = '\0';
}

void ri_art_tr_set_song(const char *name) {
    ri_art_tr_copy(tr_song_name, (uint32_t)sizeof tr_song_name, name);
}

const char *ri_art_tr_song(void) {
    return tr_song_name;
}

/* Shorten to `room` pixels using real face metrics when they are available,
 * so the ellipsis lands on a character boundary rather than a guessed count.
 * Without metrics the pixel budget is approximated by character count, which
 * is what the old fallback did and is honest about being approximate. */
void ri_art_tr_fit(char *dst, uint32_t cap, const char *src,
    const struct ri_text_metrics *tm, int room) {
    uint32_t len, keep;
    if (!dst || cap < 5u) {                 /* need room for at least "x..." */
        if (dst && cap > 0u)
            dst[0] = '\0';
        return;
    }
    ri_art_tr_copy(dst, cap, src);
    if (dst[0] == '\0')
        return;
    if (!tm || !tm->width || tm->width(tm->ctx, dst) <= room)
        return;
    /* Too wide: shorten a character at a time until it fits or too little is
     * left to be worth showing. dst is always terminated by the copy and again
     * after every truncation, so the width probe never reads past the end. */
    len = 0u;
    while (dst[len] != '\0')
        len++;
    /* Shortest name that fits WITH its ellipsis: keep-3 characters plus "...".
     * Testing the bare prefix against `room` is what the first version did, and
     * it silently dropped the ellipsis -- the name came out looking complete
     * while being cut off, which is the worst of both. */
    keep = len;
    for (;;) {
        if (keep < 4u)
            break;
        dst[keep - 3u] = '.';
        dst[keep - 2u] = '.';
        dst[keep - 1u] = '.';
        dst[keep] = '\0';
        if (tm->width(tm->ctx, dst) <= room)
            return;
        keep--;
        dst[keep] = '\0';
    }
    /* Nothing but the ellipsis fits -- including when room is smaller still,
     * so the ellipsis itself is measured before it is emitted. */
    ri_art_tr_copy(dst, cap, "");
    if (tm->width(tm->ctx, "...") <= room)
        ri_art_tr_copy(dst, cap, "...");
}

void ri_art_text_at(struct ri_dlist *dl, int x, int cy, const char *t, int col, int align,
    const struct ri_text_metrics *tm) {
    int w = 0;
    if (!t)
        return;
    if (dl && dl->cur_face) {
        /* Face metrics (S2): identical on host and AROS by construction. */
        const struct ri_face *f = ri_face_by_id(dl->cur_face);
        if (f)
            w = ri_face_width(f, t);
    } else if (tm && tm->width)
        w = tm->width(tm->ctx, t);
    /* Without metrics the backend centres at x (documented fallback;
     * AROS always passes metrics, keeping the old TextLength behaviour). */
    ri_draw_text(dl, align < 0 ? x - w / 2 : align > 0 ? x + w / 2 : x, cy, 1u,
        ri_art_rgb(col), t);
}

void ri_art_bg_tr(struct ri_dlist *dl, const struct RIGeoSection *g, int ox, int oy, int z,
    const struct ri_text_metrics *tm) {
#define PX(q) ri_geo_px((q), z)
    /* Sized from tr_song_name, not guessed (2026-10-04).
     *
     * This was `char name[40]`, and ri_art_tr_copy fills at most cap-1, so the
     * fit buffer held 39 characters against a 64-byte song buffer. Measured from
     * this call site that made the ellipsis branch UNREACHABLE AT EVERY ZOOM:
     * 39 chars x 6 px advance = 233 px, while the row is PX(1560) = 390 / 585 /
     * 780 / 292 px (compact is the smallest). 233 < 292 everywhere, so the fit
     * never shortened anything and any name over 39 characters was emitted
     * hard-cut with no ellipsis -- "the name looks complete while being cut
     * off", which is precisely what the ellipsis exists to prevent.
     *
     * TR_SONG_MAX + slack for the ellipsis logic, which writes dst[len] on the
     * way down. With this size a 63-character name now reaches the fit intact
     * and is shortened WITH an ellipsis at the one zoom whose row is narrow
     * enough to need it. */
    char name[TR_SONG_MAX + 8];
    ri_art_panel(dl, ox, oy, ox + PX(g->w) - 1, oy + PX(g->h) - 1, ri_art_rgb(C_TR_PANEL), 0);
    ri_art_text_c(dl, ox + PX(25), oy + PX(150), "0", C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(132), oy + PX(150), "10", C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(242), oy + PX(40), "SYNC", C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(372), oy + PX(40), "MIDI", C_MIX_TEXT);
    ri_art_text_at(dl, ox + PX(642), oy + PX(45), "PATTERN", C_MIX_TEXT, -1, tm);
    ri_art_text_at(dl, ox + PX(740), oy + PX(45), "SONG MODE", C_MIX_TEXT, 1, tm);
    ri_art_rect(dl, ox + PX(384), oy + PX(94), ox + PX(1028), oy + PX(180), C_MIX_SLOT);
    ri_art_line(dl, ox + PX(1330), oy + PX(38), ox + PX(1400), oy + PX(38), C_MIX_TEXT);
    ri_art_text_c(dl, ox + PX(1444), oy + PX(38), "LOOP", C_MIX_TEXT);
    ri_art_line(dl, ox + PX(1488), oy + PX(38), ox + PX(1560), oy + PX(38), C_MIX_TEXT);
    /* Song name, left-aligned under the transport row. Empty until a song
     * loads, which is the correct state for a pattern-mode session.
     *
     * The baseline is 196, below the transport's own 180-tall slot, because
     * the panel is only 208 tall and the plate text at 45 sits above it. */
    if (tr_song_name[0] != '\0') {
        /* Width 1560 of 1684 and baseline 190 of 208: the plate's own text
         * sits at 45 and 150, so 190 is the last row clear of the bar, and
         * 1560 leaves the right-hand LOOP legend alone. Both are in plate
         * units -- PX() halves them at zoom 1, and the earlier PX(900)/PX(197)
         * put the label at x=18 y=73, i.e. inside the SYNC/MIDI legend row. */
        ri_art_tr_fit(name, sizeof name, tr_song_name, tm, PX(1560));
        if (name[0] != '\0')
            ri_art_text_at(dl, ox + PX(25), oy + PX(190), name, C_MIX_TEXT, 0, tm);
    }
#undef PX
}