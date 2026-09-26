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

/* Load dir/Skin.manifest + every part image. Returns parts bound (>= 0),
 * or -1 bad arg, -2 manifest unreadable, -3 manifest rejected. A missing
 * or undecodable part image is NOT an error (Classic fallback per part).
 * The loaded skin is NOT made active (see set_active). A successful load
 * also builds the zoom-0 cache, so blits work without a zoom() call. */
int ri_skin_aros_load(const char *dir, struct RISkin *skin);
/* (Re)build the zoom-scaled cache for zoom index 0..3 (panelgeo factors
 * 4/8, 6/8, 8/8, 3/8 of the 2x masters). Returns parts scaled, or -1 bad
 * arg, -2 bad zoom. Idempotent per (skin, zoom). */
int ri_skin_aros_zoom(struct RISkin *skin, int zoom);
/* Blit part frame (0-based) top-left at (dx, dy) in the RastPort.
 * Returns 1 drawn, 0 fallback (caller renders procedurally), -1 bad
 * arg or no cybergraphics. Reads the zoom cache built at load / zoom
 * change (blit-order words); without a cache entry the part falls back. */
int ri_skin_aros_blit(struct RastPort *rp, const struct RISkin *skin,
                      uint8_t sec, uint8_t kind, const char *part,
                      uint32_t frame, int dx, int dy);
/* Release every loader-owned pixel buffer of this skin and clear its
 * bindings (manifest/lookup survive). Deactivates it when active. */
void ri_skin_aros_free(struct RISkin *skin);
/* NULL = Classic (no skin). The canvas reads this every MUIM_Draw. */
void ri_skin_aros_set_active(struct RISkin *skin);
const struct RISkin *ri_skin_aros_active(void);
#endif
