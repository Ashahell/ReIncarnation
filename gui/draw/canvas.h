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
/* Damage clip (Dell 2026-10-02: build_dl was 59 % of a box repaint, and
 * the chase issues ~96 of them per window, because the damage path built the
 * WHOLE section and then threw away everything outside the box). With a clip
 * set, ri_dlist_push drops commands that cannot paint inside it, so the build
 * only pays for what the replay would have drawn anyway.
 *
 * This is exactly equivalent, not an approximation: the backend's clipped
 * replay already skips every command for which ri_dcmd_hits_box is 0, so the
 * surviving stream is identical. A command that straddles the box still hits
 * and is kept, so partial coverage is unchanged. With no clip set the push
 * path is byte-for-byte the old one, which is what keeps the goldens.
 * RI_D_CLIP is a no-op for replay and is dropped when clipping. */
void ri_dlist_set_clip(struct ri_dlist *dl, int x0, int y0, int x1, int y1);
void ri_dlist_clear_clip(struct ri_dlist *dl);
/* True when a run of art confined to the band [y0,y1] (absolute, the same space
 * as the commands) cannot paint inside the damage clip, so the code that would
 * emit it can be skipped outright.
 *
 * This is the second half of the item cull in art_section.c. That cull works at
 * item granularity; the background has no items, it is one function that fills
 * band-structured art -- a 303 keyboard key is ~28 horizontal strips, each with
 * two colour computations and a division, and a 64x16 damage box wants perhaps
 * two of them. bench_build measures the background at 39 % of the remaining
 * clipped build (67 % of SYNTH1, 54 % of LEVI) and its op split says it is
 * ~760 plain RECTs, not text, so the cost is the strips and not font
 * measurement.
 *
 * A band test is exact here for the same reason the item cull is: the clipped
 * replay already drops every command that misses the box, so anything this
 * rejects could only have produced commands the replay was going to throw
 * away. Bands are tested in absolute coordinates, exactly like the commands,
 * which is the only convention ri_dcmd_hits_box uses.
 *
 * With no clip set this returns 0, so the full-draw path is unchanged -- which
 * is what keeps the goldens (t92/t93) still. */
int ri_dlist_band_missed(const struct ri_dlist *dl, int y0, int y1);
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

/* Command extent in canvas px (S3 dirty-rect culling). TEXT uses the
 * legend-face metrics when the command carries a face, else a generous
 * system-font estimate; IMAGE is backend-sized, so it hits everything;
 * CLIP is a no-op and never hits. Returns 0 ok, 2 when empty/unknown. */
int ri_dcmd_bbox(const struct ri_dcmd *c, int *x0, int *y0, int *x1, int *y1);
/* 1 when the command paints inside the box (inclusive edges). */
int ri_dcmd_hits_box(const struct ri_dcmd *c, int x0, int y0, int x1, int y1);

/* FNV-1a hash over the command stream (golden pin per section/zoom/skin). */
uint32_t ri_dlist_hash(const struct ri_dlist *dl);

#endif
