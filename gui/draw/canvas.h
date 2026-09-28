/* canvas.h — backend-free display list (portability plan T2).
 * gui/draw/ builds the list from pure section state; backends replay it
 * (AROS RastPort pens + friend bitmap, host software rasterizer).
 * Caller owns all storage (dlist + backing array); bounded; no allocation.
 */
#ifndef RI_DRAW_CANVAS_H
#define RI_DRAW_CANVAS_H
#include <stdint.h>

#include "platform/pal/ri_pal_draw.h"

void ri_dlist_init(struct ri_dlist *dl, struct ri_dcmd *backing, uint32_t cap,
    char *spool, uint32_t spcap);
void ri_dlist_clear(struct ri_dlist *dl);
/* Push one command; returns 0 ok, 1 full (counts on the caller to size). */
int ri_dlist_push(struct ri_dlist *dl, const struct ri_dcmd *c);

/* Emitters (all coordinates canvas px, rgb 0xRRGGBB). */
int ri_draw_rect(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t rgb);
int ri_draw_line(struct ri_dlist *dl, int x0, int y0, int x1, int y1, uint32_t rgb);
int ri_draw_circle(struct ri_dlist *dl, int cx, int cy, int r, uint32_t rgb);
int ri_draw_text(struct ri_dlist *dl, int x, int y, uint8_t align, uint32_t rgb, const char *text);
/* Legend-face text (S2): face is RI_FACE_* (0 = system font, same as above). */
int ri_draw_text_face(struct ri_dlist *dl, int x, int y, uint8_t align, uint32_t rgb, int face,
    const char *text);
/* Legend face state for the list (ri_art_text_c reads it); 0 = system. */
void ri_dlist_set_face(struct ri_dlist *dl, int face);
int ri_draw_image(struct ri_dlist *dl, int x, int y, uint16_t img, uint16_t frame);
int ri_draw_clip(struct ri_dlist *dl, int x0, int y0, int x1, int y1);

/* FNV-1a hash over the command stream (golden pin per section/zoom/skin). */
uint32_t ri_dlist_hash(const struct ri_dlist *dl);

#endif
