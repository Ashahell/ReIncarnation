/* art_shared.c — palette + shared painters (portability plan T2).
 * Transliterated from gui/widgets/rsection.mcc.c (same geometry, colours,
 * strings); RastPort calls become display-list emits.
 */
#include "gui/draw/art.h"

#include <string.h>
#include "engine/dsp/kernels.h"
#include "gui/panelgeo.h"
#include "gui/panelui.h"

static const uint32_t RI_ART_RGB[C_NCOL] = {
    0xCACCC7u, 0x8C8C84u, 0x141414u, 0xF4F4F0u, 0xB4B4AEu, 0xF0F0EAu, 0x5A5A56u,
    0xFF2A1Au, 0x5A1410u, 0x2C2C2Cu, 0x3C3C3Cu, 0x2A2A2Au, 0x1E1E1Eu, 0xF0F0F0u,
    0x280808u, 0xFF3020u, 0x9C9C96u,
    0x3A362Eu, 0x6A6458u, 0xC41E1Eu, 0xE6E6E0u, 0xE8E0C8u, 0xFFF6D0u,
    0xC82020u, 0xE07418u, 0xE6D21Eu, 0xE4E4DCu, 0x2A2620u,
    0xDCDCD4u, 0x2E2E2Cu, 0xE8761Eu, 0x4A4A48u, 0xE8901Eu, 0x30D040u, 0xC4C4BCu,
    0x5E6A72u, 0x8E8A3Au, 0xF2EAB8u, 0x1A1E22u, 0x38E040u, 0x1E4A22u, 0x9AA0A6u, 0xE8ECEEu,
    0x6E7276u, 0x2E3A7Au, 0x4A1210u, 0x8C2444u, 0xF08888u, 0x7A7E82u,
    0x141518u, 0xD8A93Cu, 0x3A3D45u
};

uint32_t ri_art_rgb(int idx) {
    if (idx < 0 || idx >= C_NCOL)
        idx = C_BLACK;
    return RI_ART_RGB[idx];
}

int ri_art_index(uint32_t rgb) {
    int i;
    for (i = 0; i < C_NCOL; i++)
        if (RI_ART_RGB[i] == rgb)
            return i;
    return -1;
}

void ri_art_rect(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int col) {
    ri_draw_rect(dl, x0, y0, x1, y1, ri_art_rgb(col));
}

void ri_art_line(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int col) {
    ri_draw_line(dl, x0, y0, x1, y1, ri_art_rgb(col));
}

void ri_art_circle(struct ri_dlist *dl, int cx, int cy, int r, int col) {
    int dy;
    if (r < 0)
        return;
    /* Scanline fill, like the old fill_circle. */
    for (dy = -r; dy <= r; dy++) {
        int dx = r;
        while (dx > 0 && dx * dx + dy * dy > r * r)
            dx--;
        ri_draw_rect(dl, cx - dx, cy + dy, cx + dx, cy + dy, ri_art_rgb(col));
    }
}

uint32_t ri_art_mix(uint32_t a, uint32_t b, int t) {
    uint32_t r, g, bl;
    if (t <= 0)
        return a & 0xFFFFFFu;
    if (t >= 256)
        return b & 0xFFFFFFu;
    r = (((a >> 16) & 0xFFu) * (uint32_t)(256 - t) + ((b >> 16) & 0xFFu) * (uint32_t)t) >> 8;
    g = (((a >> 8) & 0xFFu) * (uint32_t)(256 - t) + ((b >> 8) & 0xFFu) * (uint32_t)t) >> 8;
    bl = ((a & 0xFFu) * (uint32_t)(256 - t) + (b & 0xFFu) * (uint32_t)t) >> 8;
    return (r << 16) | (g << 8) | bl;
}

uint32_t ri_art_shade(uint32_t c, int pct) {
    if (pct >= 0)
        return ri_art_mix(c, 0xFFFFFFu, pct * 256 / 100);
    return ri_art_mix(c, 0x000000u, -pct * 256 / 100);
}

int ri_art_luma(uint32_t c) {
    return (int)((((c >> 16) & 0xFFu) * 299u + ((c >> 8) & 0xFFu) * 587u + (c & 0xFFu) * 114u) / 1000u);
}

/* Integer half-width of a disc row. */
static int art_hw(int r, int dy) {
    int dx = r;
    while (dx > 0 && dx * dx + dy * dy > r * r)
        dx--;
    return dx;
}

void ri_art_disc_grad(struct ri_dlist *dl, int cx, int cy, int r, uint32_t top, uint32_t bot) {
    int dy;
    if (r < 0)
        return;
    for (dy = -r; dy <= r; dy++) {
        int w = art_hw(r, dy);
        int t = r > 0 ? (dy + r) * 256 / (2 * r) : 128;
        ri_draw_rect(dl, cx - w, cy + dy, cx + w, cy + dy, ri_art_mix(top, bot, t));
    }
}

/* Panel plate: satin vertical grade (or brushed hairlines) + a rolled edge
 * (light top/left, dark bottom/right), like a folded metal front. */
void ri_art_panel(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t base, int brushed) {
    int y, h = y1 - y0;
    if (h <= 0 || x1 <= x0)
        return;
    if (brushed) {
        ri_draw_rect(dl, x0, y0, x1, y1, base);
        for (y = y0 + 1; y < y1; y += 2)
            ri_draw_line(dl, x0, y, x1, y, ri_art_shade(base, ((y / 2) % 3 == 0) ? 7 : -4));
    } else {
        int bands = h < 64 ? h : 64, b;
        for (b = 0; b < bands; b++) {
            int ya = y0 + b * h / bands, yb = y0 + (b + 1) * h / bands - 1;
            ri_draw_rect(dl, x0, ya, x1, yb < ya ? ya : yb,
                ri_art_mix(ri_art_shade(base, 6), ri_art_shade(base, -7), b * 256 / (bands > 1 ? bands - 1 : 1)));
        }
    }
    ri_draw_line(dl, x0, y0, x1, y0, ri_art_shade(base, 38));
    ri_draw_line(dl, x0, y0, x0, y1, ri_art_shade(base, 22));
    ri_draw_line(dl, x0, y1, x1, y1, ri_art_shade(base, -45));
    ri_draw_line(dl, x1, y0, x1, y1, ri_art_shade(base, -35));
}

/* Countersunk slotted screw. */
void ri_art_screw(struct ri_dlist *dl, int cx, int cy, int r, uint32_t panel) {
    if (r < 2)
        return;
    ri_art_disc_grad(dl, cx + 1, cy + 1, r, ri_art_shade(panel, -30), ri_art_shade(panel, -30));
    ri_art_disc_grad(dl, cx, cy, r, ri_art_shade(0xB8B8B4u, 25), ri_art_shade(0xB8B8B4u, -40));
    ri_art_disc_grad(dl, cx, cy, r - 1, ri_art_shade(0xB8B8B4u, 10), ri_art_shade(0xB8B8B4u, -25));
    ri_draw_line(dl, cx - r + 2, cy + r - 2, cx + r - 2, cy - r + 2, ri_art_shade(0xB8B8B4u, -60));
}

/* LED lens: lit = glow halo + bright graded core + specular; unlit = dark
 * lens in a bezel with a faint glint. */
void ri_art_led(struct ri_dlist *dl, int cx, int cy, int r, int col, int lit, uint32_t panel) {
    uint32_t c = ri_art_rgb(col);
    int hr = r / 2 > 1 ? r / 2 : 1, sr = r / 3 > 0 ? r / 3 : 1;
    if (r < 1)
        return;
    if (lit) {
        ri_art_disc_grad(dl, cx, cy, r + hr + 1, ri_art_mix(panel, c, 60), ri_art_mix(panel, c, 60));
        ri_art_disc_grad(dl, cx, cy, r + hr / 2 + 1, ri_art_mix(panel, c, 130), ri_art_mix(panel, c, 110));
        ri_art_disc_grad(dl, cx, cy, r, ri_art_shade(c, 35), c);
        ri_art_disc_grad(dl, cx - r / 3, cy - r / 3, sr, ri_art_shade(c, 80), ri_art_shade(c, 55));
    } else {
        ri_art_disc_grad(dl, cx, cy, r + 1, ri_art_shade(panel, -45), ri_art_shade(panel, -25));
        ri_art_disc_grad(dl, cx, cy, r, ri_art_shade(c, 10), ri_art_shade(c, -45));
        ri_draw_rect(dl, cx - r / 3, cy - r / 2, cx - r / 3 + (sr > 1 ? 1 : 0), cy - r / 2, ri_art_shade(c, 45));
    }
}

/* Moulded key/button: dark outline with softened corners, graded face,
 * top highlight, bottom inner shadow. */
void ri_art_bevel(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int face) {
    uint32_t f = ri_art_rgb(face), ol = ri_art_shade(f, -48), top = ri_art_shade(f, 16), bot = ri_art_shade(f, -12);
    int y, h = y1 - y0;
    if (x1 - x0 < 3 || h < 3) {
        ri_draw_rect(dl, x0, y0, x1, y1, f);
        return;
    }
    for (y = y0 + 1; y < y1; y++)
        ri_draw_rect(dl, x0 + 1, y, x1 - 1, y, ri_art_mix(top, bot, (y - y0) * 256 / h));
    ri_draw_line(dl, x0 + 1, y0, x1 - 1, y0, ol);
    ri_draw_line(dl, x0 + 1, y1, x1 - 1, y1, ri_art_shade(ol, -20));
    ri_draw_line(dl, x0, y0 + 1, x0, y1 - 1, ol);
    ri_draw_line(dl, x1, y0 + 1, x1, y1 - 1, ri_art_shade(ol, -12));
    ri_draw_line(dl, x0 + 2, y0 + 1, x1 - 2, y0 + 1, ri_art_shade(f, 45));
    ri_draw_line(dl, x0 + 1, y0 + 2, x0 + 1, y1 - 2, ri_art_shade(f, 25));
    ri_draw_line(dl, x0 + 2, y1 - 1, x1 - 2, y1 - 1, ri_art_shade(f, -25));
}

void ri_art_text_c(struct ri_dlist *dl, int cx, int cy, const char *s0, int col) {
    char s[24];
    int n = 0;
    if (!s0)
        return;
    while (s0[n] && n < 23) {
        s[n] = (char)((s0[n] >= 'a' && s0[n] <= 'z') ? s0[n] - 32 : s0[n]);
        n++;
    }
    s[n] = 0;
    if (!n)
        return;
    /* Backend centers with its own metrics (AROS: TextLength + baseline). */
    ri_draw_text(dl, cx, cy, 1u, ri_art_rgb(col), s);
}

static void art_polar(int cx, int cy, float deg, float r, int *x, int *y) {
    float a = (deg - 90.0f) * 0.0174533f; /* 0 deg = up, clockwise */
    *x = cx + (int)(ri_sin(a + 1.5707963f) * r);
    *y = cy + (int)(ri_sin(a) * r);
}

void ri_art_knob(struct ri_dlist *dl, int cx, int cy, int body, int ring, int face,
    int ptr, int ticks, float deg, uint32_t panel) {
    uint32_t f = ri_art_rgb(face), p = ri_art_rgb(ptr);
    int k, rb = body / 2, rr = ring / 2, x0, y0, x1, y1, rbody, rcap, sh, th, t;
    uint32_t tick = ri_art_luma(panel) > 128 ? ri_art_shade(panel, -62) : ri_art_shade(panel, 58);
    if (rb < 3)
        return;
    if (ticks)                                     /* printed scale: 11 dots, ends + centre larger */
        for (k = 0; k <= 10; k++) {
            int dr = (k == 0 || k == 5 || k == 10) ? (rb / 12 > 1 ? rb / 12 : 1) + 1 : (rb / 14 > 0 ? rb / 14 : 1);
            art_polar(cx, cy, (float)(-135 + 27 * k), (float)(rb + (rr - rb) * 2 / 3 + 1), &x0, &y0);
            ri_art_disc_grad(dl, x0, y0, dr, tick, tick);
        }
    sh = rb / 9 > 1 ? rb / 9 : 1;                  /* soft drop shadow, light from top-left */
    ri_art_disc_grad(dl, cx + sh, cy + sh + sh / 2 + 1, rb, ri_art_shade(panel, -22), ri_art_shade(panel, -38));
    ri_art_disc_grad(dl, cx, cy, rb, ri_art_shade(f, -6), ri_art_shade(f, -48));        /* skirt */
    rbody = rb * 86 / 100;
    ri_art_disc_grad(dl, cx, cy, rbody, ri_art_shade(f, 30), ri_art_shade(f, -22));      /* body */
    rcap = rb * 64 / 100;
    ri_art_disc_grad(dl, cx, cy, rcap, ri_art_shade(f, -6), ri_art_shade(f, 14));        /* concave cap */
    t = rb / 8 > 1 ? rb / 8 : 1;                                                        /* glint */
    ri_art_disc_grad(dl, cx - rb * 38 / 100, cy - rb * 46 / 100, t, ri_art_shade(f, 42), ri_art_shade(f, 22));
    th = rb / 11 > 0 ? rb / 11 : 0;                /* pointer: a solid bar th*2+1 px wide */
    art_polar(cx, cy, deg, (float)(rcap / 5), &x0, &y0);
    art_polar(cx, cy, deg, (float)(rbody - 1), &x1, &y1);
    {
        float a = (deg - 90.0f) * 0.0174533f;      /* direction (cos a, sin a); perpendicular (-sin a, cos a) */
        float sx = -ri_sin(a), sy = ri_sin(a + 1.5707963f);
        for (k = -th; k <= th; k++) {              /* x and y offsets both: no rounding gaps */
            int px = (int)(sx * (float)k), py = (int)(sy * (float)k);
            ri_draw_line(dl, x0 + px, y0 + py, x1 + px, y1 + py, p);
            ri_draw_line(dl, x0 + k, y0, x1 + k, y1, p);
            ri_draw_line(dl, x0, y0 + k, x1, y1 + k, p);
        }
    }
}

void ri_art_meter(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int level, int nseg, int master) {
    int k, h;
    if (nseg <= 0)
        return;
    h = (y1 - y0 - 2) / nseg;
    ri_art_rect(dl, x0, y0, x1, y1, C_MIX_SLOT);
    for (k = 0; k < nseg; k++) {                   /* k = 0 bottom */
        int lit = level * nseg > k * 127 && level > 0;
        int top = k == nseg - 1, warn = k >= nseg - (master ? 3 : 2);
        int on = top ? C_LED_ON : warn ? C_STEP_YELLOW : C_MIX_GREEN;
        int off = top ? C_LED_OFF : warn ? C_LAMP_OFF : C_MIX_GREEN_OFF;
        int yb = y1 - 1 - k * h;
        {                                          /* segment lens: bright centre line when lit */
            uint32_t c = ri_art_rgb(lit ? on : off);
            ri_draw_rect(dl, x0 + 2, yb - h + 2, x1 - 2, yb, lit ? c : ri_art_shade(c, -15));
            ri_draw_line(dl, x0 + 3, yb - h + 2, x1 - 3, yb - h + 2, ri_art_shade(c, lit ? 45 : 10));
        }
    }
}

void ri_art_fader(struct ri_dlist *dl, int cx, int y0, int y1, int capw, int caph, int n, int skinned) {
    int travel = (y1 - y0) - caph, cy = y0 + caph / 2 + (int)((long)travel * (127 - n) / 127), k;
    uint32_t slot = ri_art_rgb(skinned ? C_MIX_KNOB : C_BLACK), cap = ri_art_rgb(skinned ? C_BTN_LO : C_MIX_SLOT);
    int cx0 = cx - capw / 2, cx1 = cx + capw / 2, cy0 = cy - caph / 2, cy1 = cy + caph / 2;
    /* groove: dark slot with a lit lower lip */
    ri_draw_rect(dl, cx - 2, y0, cx + 2, y1, ri_art_shade(slot, -30));
    ri_draw_line(dl, cx - 2, y0, cx - 2, y1, ri_art_shade(slot, -55));
    ri_draw_line(dl, cx + 3, y0, cx + 3, y1, ri_art_shade(slot, 35));
    /* cap: drop shadow, graded moulding, finger ridges, white index line */
    ri_draw_rect(dl, cx0 + 2, cy0 + 3, cx1 + 2, cy1 + 3, ri_art_mix(slot, 0x000000u, 120));
    for (k = cy0; k <= cy1; k++)
        ri_draw_rect(dl, cx0, k, cx1, k, ri_art_mix(ri_art_shade(cap, 38), ri_art_shade(cap, -30),
            (k - cy0) * 256 / (caph > 0 ? caph : 1)));
    ri_draw_line(dl, cx0, cy0, cx1, cy0, ri_art_shade(cap, 60));
    ri_draw_line(dl, cx0, cy1, cx1, cy1, ri_art_shade(cap, -60));
    for (k = 1; k <= 2; k++) {
        int dy = k * caph / 6;
        ri_draw_line(dl, cx0 + 2, cy - dy, cx1 - 2, cy - dy, ri_art_shade(cap, -35));
        ri_draw_line(dl, cx0 + 2, cy + dy, cx1 - 2, cy + dy, ri_art_shade(cap, -35));
    }
    ri_draw_rect(dl, cx0 + 1, cy, cx1 - 1, cy, ri_art_rgb(C_MIX_TEXT));
}

void ri_art_key_909(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int z, int lamp) {
    int f = ri_geo_px(6, z), lw = (x1 - x0) / 4;
    uint32_t bar = ri_art_rgb(C_909_BAR);
    ri_draw_rect(dl, x0, y0, x1, y1, bar);
    ri_draw_line(dl, x0, y1, x1, y1, ri_art_shade(bar, 30));
    ri_art_bevel(dl, x0 + f, y0 + f, x1 - f, y1 - f, C_WHITEKEY);
    /* lamp window: dark lens, lit lamps glow through a diffuser */
    {
        int lx0 = (x0 + x1) / 2 - lw, lx1 = (x0 + x1) / 2 + lw;
        int ly0 = y0 + f + ri_geo_px(8, z), ly1 = y0 + f + ri_geo_px(16, z), yy;
        uint32_t c = ri_art_rgb(lamp);
        int lit = lamp != C_LAMP_OFF;
        ri_draw_rect(dl, lx0 - 1, ly0 - 1, lx1 + 1, ly1 + 1, ri_art_shade(bar, -20));
        for (yy = ly0; yy <= ly1; yy++)
            ri_draw_rect(dl, lx0, yy, lx1, yy, lit ? ri_art_mix(ri_art_shade(c, 45), c, (yy - ly0) * 256 / (ly1 - ly0 + 1))
                : ri_art_mix(ri_art_shade(c, 12), ri_art_shade(c, -30), (yy - ly0) * 256 / (ly1 - ly0 + 1)));
    }
}

void ri_art_note_glyph(struct ri_dlist *dl, int x, int y, int flags, int z) {
    int k;
    ri_art_circle(dl, x, y, ri_geo_px(6, z), C_MIX_TEXT);
    ri_art_line(dl, x + ri_geo_px(6, z), y, x + ri_geo_px(6, z), y - ri_geo_px(28, z), C_MIX_TEXT);
    for (k = 0; k < flags; k++)
        ri_art_line(dl, x + ri_geo_px(6, z), y - ri_geo_px(28 - 8 * k, z), x + ri_geo_px(16, z),
            y - ri_geo_px(20 - 8 * k, z), C_MIX_TEXT);
}

/* LCD/LED window: black bezel + smoked glass with a top glint. */
void ri_art_lcd_bg(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    uint32_t bg = ri_art_rgb(C_SEG_BG);
    int k;
    ri_draw_rect(dl, x0 - 1, y0 - 1, x1 + 1, y1 + 1, 0x0A0A0Au);
    for (k = y0; k <= y1; k++)
        ri_draw_rect(dl, x0, k, x1, k, ri_art_mix(ri_art_shade(bg, 12), ri_art_shade(bg, -35),
            (k - y0) * 256 / (y1 - y0 + 1)));
    ri_draw_line(dl, x0 + 1, y0 + 1, x1 - 1, y0 + 1, ri_art_shade(bg, 30));
}

void ri_art_led_digits(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int v, int ndig) {
    char b[4];
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, k, p = 1;
    if (ndig < 1 || ndig > 3)
        return;
    ri_art_lcd_bg(dl, x0, y0, x1, y1);
    ri_art_text_c(dl, cx, cy, ndig == 3 ? "888" : "88", C_SEG_DIM);
    for (k = ndig - 1; k >= 0; k--, p *= 10)       /* leading digits blank */
        b[k] = (char)(k == ndig - 1 || v >= p ? '0' + v / p % 10 : ' ');
    b[ndig] = 0;
    ri_art_text_c(dl, cx, cy, b, C_SEG);
}

void ri_art_gr_row(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int v) {
    int k, n = v > 0 ? 1 + (v * 4 + 126) / 127 : 0, w = (x1 - x0) / 9;
    for (k = 0; k < 9; k++) {
        int lit = k <= 4 && 4 - k < n;
        int on = k <= 2 ? C_MIX_GREEN : C_STEP_YELLOW;
        int off = k <= 2 ? C_MIX_GREEN_OFF : k <= 4 ? C_LAMP_OFF : C_LED_OFF;
        ri_art_led(dl, x0 + w * k + w / 2, (y0 + y1) / 2, (y1 - y0) / 2 - 1, lit ? on : off, lit,
            ri_art_rgb(C_FX_PANEL));
    }
}

void ri_art_arrow_btn(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int up) {
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, h = (y1 - y0) / 4, k;
    ri_art_bevel(dl, x0, y0, x1, y1, C_WHITEKEY);
    for (k = 0; k <= h; k++)                       /* filled triangle */
        ri_art_line(dl, cx - k, up ? cy - h / 2 + k : cy + h / 2 - k,
            cx + k, up ? cy - h / 2 + k : cy + h / 2 - k, C_BLACK);
}

int ri_art_chase(const struct RIPanelUI *panel, uint8_t section, uint32_t step) {
    int f;
    if (!panel)
        return 0;
    f = ri_panel_focus_of(section);
    return f >= 0 && panel->playhead[f] == (int8_t)step;
}

void ri_art_tr_key(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t idx, int lit, int z) {
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, h = ri_geo_px(14, z), k, j;
    int col = lit ? (idx == 8 ? C_LED_ON : C_MIX_GREEN) : C_BLACK;
    ri_art_bevel(dl, x0, y0, x1, y1, C_BTN);
    if (idx == 5) {
        ri_art_rect(dl, cx - h / 2, cy - h / 2, cx + h / 2, cy + h / 2, C_BLACK);
    } else if (idx == 8) {
        ri_art_circle(dl, cx, cy, h * 2 / 3, lit ? C_LED_ON : C_LED_OFF);
    } else {
        int n = idx == 4 ? 1 : 2, left = idx == 6;
        for (j = 0; j < n; j++) {
            int bx = cx + (n == 2 ? (j ? h / 2 : -h / 2) : 0) - h / 2;
            for (k = 0; k <= h; k++) {
                int w = (k <= h / 2 ? k : h - k);
                if (left)
                    ri_art_line(dl, bx + h / 2 - w, cy - h / 2 + k, bx + h / 2, cy - h / 2 + k, col);
                else
                    ri_art_line(dl, bx, cy - h / 2 + k, bx + w, cy - h / 2 + k, col);
            }
        }
    }
}

/* ---- Rack furniture (owner 2026-09-28) ---- */

static uint32_t art_hash(uint32_t v) {
    v ^= v >> 16;
    v *= 0x7FEB352Du;
    v ^= v >> 15;
    v *= 0x846CA68Bu;
    v ^= v >> 16;
    return v;
}

/* Brushed dark plate: every row its own tone (fine horizontal grain) plus
 * a few longer bright streaks, rolled top edge and shadowed bottom. */
void ri_art_bay(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    const uint32_t base = RI_ART_BAY_BASE;
    int y, w = x1 - x0 + 1;
    if (x1 < x0 || y1 < y0)
        return;
    for (y = y0; y <= y1; y++) {
        uint32_t h = art_hash((uint32_t)(y - y0) * 2654435761u + 17u);
        int k;
        ri_draw_rect(dl, x0, y, x1, y, ri_art_shade(base, (int)(h % 15u) - 8));
        for (k = 0; k < 2; k++) {
            uint32_t s = art_hash(h + (uint32_t)k * 0x9E37u);
            int len, sx;
            if (s % 3u)
                continue;
            len = 24 + (int)((s >> 8) % 160u);
            sx = x0 + (int)((s >> 16) % (uint32_t)(w > 1 ? w : 1));
            ri_draw_rect(dl, sx, y, sx + len < x1 ? sx + len : x1, y,
                ri_art_shade(base, 4 + (int)((s >> 4) % 6u)));
        }
    }
    ri_draw_line(dl, x0, y0, x1, y0, ri_art_shade(base, 30));
    if (y1 > y0)
        ri_draw_line(dl, x0, y1, x1, y1, ri_art_shade(base, -55));
}

void ri_art_rack_rail(struct ri_dlist *dl, int x0, int y0, int x1, int y1) {
    const uint32_t steel = 0x8E9296u;
    int x, y, hx0, hx1, cx, n = 0, first = -1, last = -1;
    if (x1 - x0 < 5 || y1 - y0 < 12)
        return;
    for (x = x0; x <= x1; x++) {       /* vertical grain */
        uint32_t h = art_hash((uint32_t)(x - x0) + 91u);
        ri_draw_rect(dl, x, y0, x, y1, ri_art_shade(steel, (int)(h % 9u) - 4));
    }
    ri_draw_rect(dl, x0, y0, x0 + 1, y1, ri_art_shade(steel, 40));
    ri_draw_rect(dl, x1 - 1, y0, x1, y1, ri_art_shade(steel, -55));
    ri_draw_line(dl, x0, y0, x1, y0, ri_art_shade(steel, 30));
    ri_draw_line(dl, x0, y1, x1, y1, ri_art_shade(steel, -60));
    cx = (x0 + x1) / 2;
    hx0 = cx - 4;
    hx1 = cx + 4;
    /* 1U = 44 px: three holes, gaps 16/16/12. */
    for (y = y0 + 8; y + 5 <= y1 - 4; n++) {
        ri_draw_rect(dl, hx0, y, hx1, y, ri_art_shade(steel, -50));
        ri_draw_rect(dl, hx0, y + 1, hx1, y + 4, 0x0C0C0Du);
        ri_draw_rect(dl, hx0 + 1, y + 5, hx1 - 1, y + 5, ri_art_shade(steel, 35));
        if (first < 0)
            first = y;
        last = y;
        y += (n % 3 < 2) ? 16 : 12;
    }
    if (first >= 0) {
        ri_art_screw(dl, cx, first + 2, 5, steel);
        if (last != first)
            ri_art_screw(dl, cx, last + 2, 5, steel);
    }
}

void ri_art_seam(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int right) {
    if (x1 < x0 || y1 < y0)
        return;
    ri_draw_rect(dl, x0, y0, x1, y1, 0x151517u);
    if (right)
        ri_draw_rect(dl, x1, y0, x1, y1, 0x0A0A0Bu);
    else
        ri_draw_rect(dl, x0, y0, x0, y1, 0x0A0A0Bu);
}

void ri_art_power(struct ri_dlist *dl, int x0, int y0, int x1, int y1, const char *label,
    int on, int pressed) {
    const uint32_t green = ri_art_rgb(C_MIX_GREEN);
    int h = y1 - y0 + 1, r = h / 2 - 3, cx = x0 + h / 2, cy = (y0 + y1) / 2 + (pressed ? 1 : 0);
    uint32_t cap_t = pressed ? 0x3C3E42u : 0x5E6166u, cap_b = pressed ? 0x222326u : 0x2A2C30u;
    uint32_t well = pressed ? 0x2A2C30u : 0x34363Au;
    uint32_t led = on ? green : 0x44524Au;
    int gr = r - 4;               /* glyph ring radius */
    if (r < 5 || x1 <= x0)
        return;
    ri_art_disc_grad(dl, cx + 1, (y0 + y1) / 2 + 2, r + 1, 0x0B0B0Cu, 0x0B0B0Cu); /* drop shadow */
    ri_art_disc_grad(dl, cx, (y0 + y1) / 2, r + 1, 0x101113u, 0x1A1B1Du);          /* bezel */
    ri_art_disc_grad(dl, cx, cy, r, cap_t, cap_b);                                 /* cap */
    ri_art_disc_grad(dl, cx, cy, r - 2, ri_art_shade(well, -15), ri_art_shade(well, 12)); /* dish */
    if (on)                                                                        /* glow */
        ri_art_disc_grad(dl, cx, cy, gr + 2, ri_art_mix(well, green, 70), ri_art_mix(well, green, 50));
    /* Power glyph = the LED: open ring with a bar through the gap. */
    ri_art_disc_grad(dl, cx, cy, gr, ri_art_shade(led, on ? 30 : 0), led);
    ri_art_disc_grad(dl, cx, cy, gr - 2, on ? ri_art_mix(well, green, 70) : well,
        on ? ri_art_mix(well, green, 50) : well);
    ri_draw_rect(dl, cx - 2, cy - gr - 1, cx + 1, cy - 1, on ? ri_art_mix(well, green, 70) : well);
    ri_draw_rect(dl, cx - 1, cy - gr - 1, cx, cy + 1, ri_art_shade(led, on ? 45 : 5));
    ri_draw_line(dl, cx - r + 3, cy - r + 2, cx - 2, cy - r, ri_art_shade(cap_t, 45)); /* glint */
    if (label && label[0])
        ri_draw_text(dl, (x0 + h + x1) / 2, (y0 + y1) / 2, 1u,
            on ? 0xE6E8EAu : 0x8C9096u, label);
}

/* Hardware tab key (S1, 2026-09-28): moulded dark key with a 3 px LED
 * strip above the label. Active = lit strip + 1 px latched + darker face;
 * pressed = 1 px sink + darker face. Label is a palette C_* (pens). */
void ri_art_tab(struct ri_dlist *dl, int x0, int y0, int x1, int y1, const char *label,
    int active, int pressed) {
    const uint32_t base = 0x3A3C40u;
    uint32_t face = base;
    int latch, lx0, lx1, ly0, ly1, cx, cy;
    uint32_t led;
    if (x1 < x0 || y1 < y0)
        return;
    if (active)
        face = ri_art_shade(base, -12);
    if (pressed)
        face = ri_art_shade(face, -10);
    latch = (active ? 1 : 0) + (pressed ? 1 : 0);
    if (latch > 2)
        latch = 2;
    ri_draw_rect(dl, x0, y0, x1, y1, face);
    ri_draw_line(dl, x0, y0, x1, y0, ri_art_shade(face, 38));
    ri_draw_line(dl, x0, y0, x0, y1, ri_art_shade(face, 22));
    ri_draw_line(dl, x0, y1, x1, y1, ri_art_shade(face, -45));
    ri_draw_line(dl, x1, y0, x1, y1, ri_art_shade(face, -35));
    if (x1 - x0 >= 4 && y1 - y0 >= 4) {
        ri_draw_line(dl, x0 + 1, y0 + 1, x1 - 1, y0 + 1, ri_art_shade(face, 18));
        ri_draw_line(dl, x0 + 1, y1 - 1, x1 - 1, y1 - 1, ri_art_shade(face, -25));
    }
    lx0 = x0 + 6;
    lx1 = x1 - 6;
    ly0 = y0 + 4 + latch;
    ly1 = ly0 + 2;
    led = active ? ri_art_rgb(C_MIX_GREEN) : ri_art_rgb(C_MIX_GREEN_OFF);
    if (lx1 >= lx0 && ly1 <= y1 - 2 && ly0 <= ly1) {
        ri_draw_rect(dl, lx0, ly0, lx1, ly1, led);
        ri_draw_line(dl, lx0, ly0, lx1, ly0, ri_art_shade(led, active ? 35 : 10));
    }
    if (label && label[0]) {
        cx = (x0 + x1) / 2;
        cy = (y0 + y1) / 2 + 4 + latch;
        ri_art_text_c(dl, cx, cy, label, active ? C_TEXT_INV : C_MIX_TEXT);
    }
}
