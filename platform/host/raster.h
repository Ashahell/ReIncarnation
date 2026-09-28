/* raster.h — host software rasterizer (portability plan T2).
 * Host-only CI backend: replays a display list into an RGBA buffer with a
 * built-in clean-room 5x7 bitmap face (tests only — AROS uses its own
 * fonts). Images resolve through the skin zoom cache (zrgba/zw/zh), built
 * by the test with pure skin.c calls + image_host decodes.
 */
#ifndef RI_HOST_RASTER_H
#define RI_HOST_RASTER_H
#include <stdint.h>

#include "platform/pal/ri_pal_draw.h"
#include "gui/skin.h"

struct ri_raster {
    uint32_t *px; /* 0xAARRGGBB, row-major */
    uint32_t w, h;
};

void ri_raster_init(struct ri_raster *r, uint32_t *backing, uint32_t w, uint32_t h);
void ri_raster_clear(struct ri_raster *r, uint32_t rgb);
void ri_raster_replay(struct ri_raster *r, const struct ri_dlist *dl,
    const struct RISkin *skin);
/* S3 partial replay: skip commands missing the box, clip the rest to it. */
void ri_raster_replay_box(struct ri_raster *r, const struct ri_dlist *dl,
    const struct RISkin *skin, int x0, int y0, int x1, int y1);
uint32_t ri_raster_hash(const struct ri_raster *r);
/* 5x7 face metrics (matches the replay centering). */
#define RI_RASTER_GLYPH_W 5u
#define RI_RASTER_GLYPH_H 7u
#define RI_RASTER_ADVANCE 6u
int ri_raster_text_width(void *ctx, const char *s);
/* PNG snapshot for evidence (libpng, host-only). 0 ok. */
int ri_raster_write_png(const char *path, const struct ri_raster *r);

#endif
