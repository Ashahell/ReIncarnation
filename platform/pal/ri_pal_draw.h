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

struct ri_dlist {
    struct ri_dcmd *cmd;
    uint32_t n, cap;
    /* Borrowed-string pool: TEXT commands copy their bytes here so the
     * list is self-contained until the next clear (both backends replay
     * synchronously, but art-stack buffers would be dead on return). */
    char *spool;
    uint32_t spn, spcap;
};

struct ri_text_metrics {
    int (*width)(void *ctx, const char *s);
    int height, baseline;
    void *ctx;
};

/* Backend replay: draw the list onto the backend surface. */
void ri_pal_draw_replay(void *surface, const struct ri_dlist *dl);

#endif
