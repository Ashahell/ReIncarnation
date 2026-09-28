/* font_legend.h — clean-room condensed sans bitmap face (S2, 2026-09-28).
 * In-house glyphs (no traced font): 1-px monolinear strokes, a flat-sided
 * "O", a straight-legged "R", a "G" with a spur, and a narrow "M".
 * Three sizes: S (cap 5, z0 + compact), M (cap 7, z1), L (cap 9, z2).
 * Full printable ASCII 32..126, proportional advance, 1-px tracking
 * (advance = w + adv_gap; ri_face_width sums advances, trailing gap kept).
 * Uniform storage: bit15 of rows[r] is the leftmost pixel (L needs >8 px,
 * so one uint16_t row type serves all faces; S/M use the top 8 bits).
 * Glyph rows 0..asc+desc-1 are live; the rest must be zero. Row asc is the
 * baseline: rows asc-cap..asc-1 are the cap zone, rows asc.. are descenders.
 */
#ifndef RI_FONT_LEGEND_H
#define RI_FONT_LEGEND_H
#include <stdint.h>

#define RI_FACE_S 1
#define RI_FACE_M 2
#define RI_FACE_L 3

struct ri_glyph {
    uint8_t w;
    uint16_t rows[12];
};
struct ri_face {
    uint8_t cap, asc, desc, adv_gap;
    const struct ri_glyph *g; /* 95 entries, ASCII 32..126 */
};

/* Face by id 1/2/3, else NULL. Face for a zoom index: 0->S, 1->M, 2->L,
 * 3 (compact)->S; anything else->S. Id for a zoom (same mapping, 1/2/3). */
const struct ri_face *ri_face_by_id(int id);
const struct ri_face *ri_face_for_zoom(int z);
int ri_face_id_for_zoom(int z);
/* Pixel width of a NUL-terminated string (sum of advances); 0 on NULL face. */
int ri_face_width(const struct ri_face *f, const char *s);

#endif
