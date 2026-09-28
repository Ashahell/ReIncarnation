/* t111_legend_face — S2 clean-room legend bitmap face (2026-09-28).
 * In-house condensed sans (S cap 5 / M cap 7 / L cap 9): every printable
 * except space has pixels; no bits outside w or below desc; advance =
 * w + 1 (1-px tracking); width sums advances; no two uppercase glyphs
 * bit-identical; zoom mapping S/M/L/S.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "gui/draw/font_legend.h"

static int count_bits(const struct ri_glyph *g, int h) {
    int n = 0, r, c;
    for (r = 0; r < h; r++)
        for (c = 0; c < g->w; c++)
            if (g->rows[r] & (0x8000u >> c))
                n++;
    return n;
}

static int out_of_bounds(const struct ri_glyph *g, int h) {
    int r;
    uint32_t allow;
    if (g->w > 16)
        return 1;
    allow = g->w >= 16 ? 0xFFFFu : ((1u << g->w) - 1u) << (16u - g->w);
    for (r = 0; r < 12; r++) {
        if (r >= h && g->rows[r] != 0u)
            return 1; /* below desc */
        if ((uint32_t)g->rows[r] & ~allow)
            return 1; /* outside w */
    }
    return 0;
}

static int same_glyph(const struct ri_glyph *a, const struct ri_glyph *b, int h) {
    int r;
    if (a->w != b->w)
        return 0;
    for (r = 0; r < h; r++)
        if (a->rows[r] != b->rows[r])
            return 0;
    return 1;
}

int main(void) {
    static const uint8_t CAP[3] = { 5, 7, 9 };
    static const uint8_t ASC[3] = { 6, 8, 10 };
    int id;
    RI_ASSERT(ri_face_by_id(0) == 0 && ri_face_by_id(4) == 0, "bad id null");
    RI_ASSERT(ri_face_for_zoom(0)->cap == 5, "z0 S");
    RI_ASSERT(ri_face_for_zoom(1)->cap == 7, "z1 M");
    RI_ASSERT(ri_face_for_zoom(2)->cap == 9, "z2 L");
    RI_ASSERT(ri_face_for_zoom(3)->cap == 5, "compact S");
    RI_ASSERT(ri_face_for_zoom(9)->cap == 5, "else S");
    RI_ASSERT(ri_face_id_for_zoom(0) == 1 && ri_face_id_for_zoom(1) == 2, "ids 0/1");
    RI_ASSERT(ri_face_id_for_zoom(2) == 3 && ri_face_id_for_zoom(3) == 1, "ids 2/3");
    RI_ASSERT(ri_face_width(0, "TB-303") == 0, "null face");
    for (id = 1; id <= 3; id++) {
        const struct ri_face *f = ri_face_by_id(id);
        int h, c, i, j, wsum = 0;
        RI_ASSERT(f != 0, "face %d", id);
        RI_ASSERT(f->cap == CAP[id - 1] && f->asc == ASC[id - 1] && f->desc == 2 &&
            f->adv_gap == 1, "metrics %d", id);
        h = (int)f->asc + (int)f->desc;
        for (c = 32; c <= 126; c++) {
            const struct ri_glyph *g = &f->g[c - 32];
            RI_ASSERT(!out_of_bounds(g, h), "bounds %d ch %d", id, c);
            if (c == 32)
                continue;
            RI_ASSERT(count_bits(g, h) >= 1, "pixels %d ch %d", id, c);
        }
        for (i = 0; i < 26; i++)
            for (j = i + 1; j < 26; j++)
                RI_ASSERT(!same_glyph(&f->g['A' - 32 + i], &f->g['A' - 32 + j], h),
                    "upper dup %d %c/%c", id, 'A' + i, 'A' + j);
        for (c = 32; c <= 126; c++)
            wsum += (int)f->g[c - 32].w + (int)f->adv_gap;
        RI_ASSERT(wsum > 95, "widths sane %d", id);
        {
            /* "TB-303" width equals the sum of advances (trailing gap kept). */
            const char *s = "TB-303";
            int k, expect = 0;
            for (k = 0; s[k]; k++)
                expect += (int)f->g[(unsigned char)s[k] - 32].w + (int)f->adv_gap;
            RI_ASSERT(ri_face_width(f, s) == expect, "width sum %d", id);
            RI_ASSERT(ri_face_width(f, "") == 0, "empty %d", id);
        }
    }
    RI_RESULT("legend_face");
}
