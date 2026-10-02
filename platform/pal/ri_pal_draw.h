/* ri_pal_draw.h — display-list replay (portability plan T2, §3.3).
 * gui/draw/ builds a backend-free display list; the backend replays it.
 * C99, includes only <stdint.h> (+stddef for NULL in impls, not here).
 */
#ifndef RI_PAL_DRAW_H
#define RI_PAL_DRAW_H
#include <stdint.h>

enum ri_dop {
    RI_D_RECT = 1,
    RI_D_LINE = 2,
    RI_D_CIRCLE = 3,
    RI_D_TEXT = 4,
    RI_D_IMAGE = 5,
    RI_D_CLIP = 6
};

struct ri_dcmd {
    uint8_t op, align, pad[2];
    int16_t x0, y0, x1, y1;
    uint32_t rgb;
    uint16_t img, frame;
    const char *text;
};
/* TEXT carries the legend face in pad[0]: 0 = backend/system font,
 * 1/2/3 = legend faces S/M/L (gui/draw/font_legend.h). */

struct ri_dlist {
    struct ri_dcmd *cmd;
    uint32_t n, cap;
    /* Borrowed-string pool: TEXT commands copy their bytes here so the
     * list is self-contained until the next clear (both backends replay
     * synchronously, but art-stack buffers would be dead on return). */
    char *spool;
    uint32_t spn, spcap;
    /* Legend face state (S2): ri_art_text_c emits TEXT with this face so
     * section legends scale with zoom; 0 = backend/system font. */
    uint8_t cur_face;
    uint8_t clip;         /* 1 = a damage clip box is set (below) */
    int16_t cx0, cy0, cx1, cy1;
    uint8_t _rfu[3];
};

struct ri_text_metrics {
    int (*width)(void *ctx, const char *s);
    int height, baseline;
    void *ctx;
};

/* Backend replay: draw the list onto the backend surface. */
void ri_pal_draw_replay(void *surface, const struct ri_dlist *dl);

#endif
