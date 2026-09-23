/* gui/knob_blit.c — knob frame blit helper (Module 2.9 art on-device).
 *
 * AROS-ONLY. Paints ri_knob_render_frame() frames (host-pinned art,
 * t29_knobart) into a RastPort via cybergraphics WritePixelArrayAlpha
 * (per-pixel alpha path; global alpha opaque). Frame computation
 * stays in gui/knob_art.c (host-tested); this TU owns ONLY the
 * device handoff (base open, RGBA→ARGB shuffle, blit call).
 * Signature/shape mirrors the proven ICD blit (lazy base open,
 * plain clib call, distinctive negative sentinels). Must NEVER
 * enter the host build (audit gates it).
 */

#ifndef __AROS__
#error "gui/knob_blit.c is AROS-only: cybergraphics blit, never in the host build"
#endif

#include <exec/types.h>
#include <graphics/rastport.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include "gui/knob_art.h"
#include "gui/knob_blit.h"

/* Owns the cybergraphics handle (lazy open like the proven ICD
 * pattern; never closed — process-lifetime app use). The clib
 * inline stubs call through this global. */
struct Library *CyberGfxBase = NULL;

static int lazy_cyber(void) {
    if (!CyberGfxBase)
        CyberGfxBase = (struct Library *)OpenLibrary(
            (CONST_STRPTR)"cybergraphics.library", 0);
    return CyberGfxBase != NULL;
}

/* One 80x80 frame staging area (file-static: deterministic, no
 * allocation; single-threaded app use like the rest of this task).
 * staged as 0xAARRGGBB words for the blit call. */
static uint32_t s_argb[80u * 80u];
static unsigned char s_frame[80u * 80u * 4u];

/* Blit one frame (value) with top-left at (dx, dy); negative dx/dy
 * clip the source (a knob centered near an edge keeps its doc
 * center instead of shifting the row). Returns painted (alpha>0)
 * pixel count (>0), 0 on bad args, -1 no cybergraphics, -2 blit
 * refused (returned 0). */
int ri_knob_blit_one(struct RastPort *rp, int value, int dx, int dy) {
    uint32_t i, n = 0;
    int sx = 0, sy = 0;
    int w = 80, h = 80;
    ULONG rc;
    if (!rp)
        return 0;
    if (!lazy_cyber())
        return -1;
    ri_knob_render_frame(s_frame, value);
    /* Words are BGRA-ordered (B in the high byte, A in the low):
     * proven by the on-device calibration run 2026-09-23 (solid
     * red/green/blue/white/black/half swatches: red/green/black
     * showed background = alpha-zero low byte; blue/white opaque;
     * half gray blended exactly 50% = 149. An ARGB packing here
     * paints ghost-blue pointers — do not "fix" this line back.) */
    for (i = 0; i < 80u * 80u; i++) {
        uint32_t r = s_frame[i * 4u];
        uint32_t g = s_frame[i * 4u + 1u];
        uint32_t b = s_frame[i * 4u + 2u];
        uint32_t a = s_frame[i * 4u + 3u];
        s_argb[i] = (b << 24) | (g << 16) | (r << 8) | a;
        if (a)
            n++;
    }
    if (dx < 0) {
        sx = -dx;
        w += dx;
        dx = 0;
    }
    if (dy < 0) {
        sy = -dy;
        h += dy;
        dy = 0;
    }
    if (w <= 0 || h <= 0)
        return 0;
    rc = WritePixelArrayAlpha(s_argb, (UWORD)sx, (UWORD)sy,
        (UWORD)(80u * 4u), rp, (UWORD)dx, (UWORD)dy, (UWORD)w,
        (UWORD)h, 0xffffffffUL);
    if (rc == 0)
        return -2;
    return (int)n;
}

/* Opaque filled rect (panel background, divider rules): callers
 * stay in-window (the proof vehicle sizes from RI_PANEL909_W/H).
 * Packs words exactly like the knob path (proven BGRA order).
 * Returns pixels painted, 0 on bad args, -1 no cybergraphics. */
static uint32_t s_scan[320u];

int ri_knob_panel_rect(struct RastPort *rp, int x, int y, int w, int h,
    uint32_t rgb) {
    int row;
    uint32_t i;
    uint32_t r = (rgb >> 16) & 0xffu;
    uint32_t g = (rgb >> 8) & 0xffu;
    uint32_t b = rgb & 0xffu;
    uint32_t word = (b << 24) | (g << 16) | (r << 8) | 0xffu;
    ULONG rc;
    if (!rp || w <= 0 || h <= 0 || w > 320 || x < 0 || y < 0)
        return 0;
    if (!lazy_cyber())
        return -1;
    for (i = 0; i < (uint32_t)w; i++)
        s_scan[i] = word;
    for (row = 0; row < h; row++) {
        rc = WritePixelArrayAlpha(s_scan, 0, 0, (UWORD)(w * 4u), rp,
            (UWORD)x, (UWORD)(y + row), (UWORD)w, 1, 0xffffffffUL);
        if (rc == 0)
            return -2;
    }
    return w * h;
}
