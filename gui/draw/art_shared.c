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
    0xD6D6CEu, 0x8C8C84u, 0x141414u, 0xF4F4F0u, 0xB4B4AEu, 0xF0F0EAu, 0x5A5A56u,
    0xFF2A1Au, 0x5A1410u, 0xC8C8C4u, 0x3C3C3Cu, 0x2A2A2Au, 0x1E1E1Eu, 0xF0F0F0u,
    0x280808u, 0xFF3020u, 0x9C9C96u,
    0x3A362Eu, 0x6A6458u, 0xC41E1Eu, 0xE6E6E0u, 0xE8E0C8u, 0xFFF6D0u,
    0xC82020u, 0xE07418u, 0xE6D21Eu, 0xE4E4DCu, 0x2A2620u,
    0xDCDCD4u, 0x2E2E2Cu, 0xE8761Eu, 0x4A4A48u, 0xE8901Eu, 0x30D040u, 0xC4C4BCu,
    0x5E6A72u, 0x8E8A3Au, 0xF2EAB8u, 0x1A1E22u, 0x38E040u, 0x1E4A22u, 0x9AA0A6u, 0xE8ECEEu,
    0x6E7276u, 0x2E3A7Au, 0x4A1210u, 0x8C2444u, 0xF08888u, 0x7A7E82u
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

void ri_art_bevel(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int face) {
    ri_art_rect(dl, x0, y0, x1, y1, face);
    ri_art_line(dl, x0, y0, x1, y0, C_BTN_HI);
    ri_art_line(dl, x0, y0, x0, y1, C_BTN_HI);
    ri_art_line(dl, x0, y1, x1, y1, C_BTN_LO);
    ri_art_line(dl, x1, y0, x1, y1, C_BTN_LO);
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
    int ptr, int ticks, float deg) {
    int k, rb = body / 2, rr = ring / 2, x0, y0, x1, y1;
    if (ticks)
        for (k = 0; k <= 10; k++) {
            art_polar(cx, cy, (float)(-135 + 27 * k), (float)(rb + 2), &x0, &y0);
            art_polar(cx, cy, (float)(-135 + 27 * k), (float)rr, &x1, &y1);
            ri_art_line(dl, x0, y0, x1, y1, C_TICK);
        }
    ri_art_circle(dl, cx, cy, rb, C_KNOB_RIM);
    ri_art_circle(dl, cx, cy, rb - 2, face);
    art_polar(cx, cy, deg, (float)(rb / 3), &x0, &y0);
    art_polar(cx, cy, deg, (float)(rb - 3), &x1, &y1);
    ri_art_line(dl, x0, y0, x1, y1, ptr);
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
        ri_art_rect(dl, x0 + 2, yb - h + 2, x1 - 2, yb, lit ? on : off);
    }
}

void ri_art_fader(struct ri_dlist *dl, int cx, int y0, int y1, int capw, int caph, int n, int skinned) {
    int travel = (y1 - y0) - caph, cy = y0 + caph / 2 + (int)((long)travel * (127 - n) / 127);
    ri_art_rect(dl, cx - 1, y0, cx + 1, y1, skinned ? C_MIX_KNOB : C_BLACK);
    ri_art_bevel(dl, cx - capw / 2, cy - caph / 2, cx + capw / 2, cy + caph / 2,
        skinned ? C_BTN_LO : C_MIX_SLOT);
    ri_art_line(dl, cx - capw / 2 + 2, cy, cx + capw / 2 - 2, cy, C_MIX_TEXT);
}

void ri_art_key_909(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int z, int lamp) {
    int f = ri_geo_px(6, z), lw = (x1 - x0) / 4;
    ri_art_rect(dl, x0, y0, x1, y1, C_909_BAR);
    ri_art_bevel(dl, x0 + f, y0 + f, x1 - f, y1 - f, C_WHITEKEY);
    ri_art_rect(dl, (x0 + x1) / 2 - lw, y0 + f + ri_geo_px(8, z),
        (x0 + x1) / 2 + lw, y0 + f + ri_geo_px(16, z), lamp);
}

void ri_art_note_glyph(struct ri_dlist *dl, int x, int y, int flags, int z) {
    int k;
    ri_art_circle(dl, x, y, ri_geo_px(6, z), C_MIX_TEXT);
    ri_art_line(dl, x + ri_geo_px(6, z), y, x + ri_geo_px(6, z), y - ri_geo_px(28, z), C_MIX_TEXT);
    for (k = 0; k < flags; k++)
        ri_art_line(dl, x + ri_geo_px(6, z), y - ri_geo_px(28 - 8 * k, z), x + ri_geo_px(16, z),
            y - ri_geo_px(20 - 8 * k, z), C_MIX_TEXT);
}

void ri_art_led_digits(struct ri_dlist *dl, int x0, int y0, int x1, int y1, int v, int ndig) {
    char b[4];
    int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, k, p = 1;
    if (ndig < 1 || ndig > 3)
        return;
    ri_art_rect(dl, x0, y0, x1, y1, C_SEG_BG);
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
        ri_art_circle(dl, x0 + w * k + w / 2, (y0 + y1) / 2, (y1 - y0) / 2 - 1, lit ? on : off);
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
