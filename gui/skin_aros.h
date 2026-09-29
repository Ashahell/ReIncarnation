/* gui/skin_aros.h — AROS-only skin/mod loader (§12.10 G8.1).
 *
 * Decodes skin part images through datatypes picture.class
 * (PDTM_READPIXELARRAY, PBPAFMT_RGBA, swizzled to the core 0xAARRGGBB
 * words), binds 2x masters into the pure core, keeps one zoom-scaled
 * copy per part (rebuilt once per zoom change, never per frame), and
 * blits through cybergraphics WritePixelArrayAlpha (the proven
 * knob_blit path). Undecodable parts stay unbound: the caller renders
 * the procedural Classic path (per-part fallback, design E0-2).
 * Must NEVER enter the host build.
 */
#ifndef __AROS__
#error "gui/skin_aros.h is AROS-only: datatypes/cybergraphics, never in the host build"
#endif
#ifndef RI_SKIN_AROS_H
#define RI_SKIN_AROS_H
#include <exec/types.h>
#include <graphics/rastport.h>
#include "gui/skin.h"
#include "gui/skinsect.h"

/* S7 shared-load registry (at most RI_SKIN_AROS_SLOTS distinct mods).
 * sync() reconciles the loader with the assignment: loads misses (idle
 * path; a failed load stays slotted-but-empty so its sections fall back
 * to Classic), releases dropped mods, and (re)builds zoom caches.
 * Returns loaded mod count, -1 bad arg, -2 bad zoom (no changes).
 * for() answers the canvas every draw (NULL = Classic). */
#define RI_SKIN_AROS_SLOTS 8u
int ri_skin_aros_sync(const struct RISkinAssign *a, const char *mods_dir,
                      int zoom);
const struct RISkin *ri_skin_aros_for(uint8_t section);
/* Blit part frame (0-based) top-left at (dx, dy) in the RastPort.
 * Returns 1 drawn, 0 fallback (caller renders procedurally), -1 bad
 * arg or no cybergraphics. Reads the zoom cache built at load / zoom
 * change (blit-order words); without a cache entry the part falls back. */
int ri_skin_aros_blit(struct RastPort *rp, const struct RISkin *skin,
                      uint8_t sec, uint8_t kind, const char *part,
                      uint32_t frame, int dx, int dy);
/* Blit zoom-cache part by slot index (T2 display-list replay): same as
 * blit() without the name lookup. Returns 1 drawn, 0 fallback, -1 bad arg. */
int ri_skin_aros_blit_idx(struct RastPort *rp, const struct RISkin *skin,
    int idx, uint32_t frame, int dx, int dy);
#endif
