/* gui/widgets/rknb.h — RKnB custom knob class API (Module 2.9 MCC).
 *
 * AROS-ONLY. Custom Numeric subclass rendering the measured 80 px
 * 909 frames; value math stays in gui/knob_logic.h (host-tested).
 * Must NEVER enter the host build (audit gates it).
 */
#ifndef RI_RKNB_H
#define RI_RKNB_H

#ifndef __AROS__
#error "rknb.h is AROS-only: Zune custom class API, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include "gui/knob_logic.h"

struct MUI_CustomClass;

/* Per-release commit counter (read-only LONG): incremented exactly
 * once per completed gesture (press→release with begun gesture =
 * one undo unit, even with zero moves). Notify on it like any
 * Numeric attribute. */
#define MUIA_RKnB_Commits (TAG_USER + 0x524Bu)

struct MUI_CustomClass *ri_rknb_class(void);
void ri_rknb_dispose_class(void);

/* Create one knob (0..127, initial 64, right-click default dflt).
 * NULL = class failed (loud failure upstream). */
APTR ri_rknb_create(LONG dflt);

/* Thin drag/gesture shells over knob_logic (host-tested math). */
LONG ri_rknb_drag_value(LONG start, double dx_px, double dy_px,
    BOOL fine);
void ri_rknb_begin(struct RiGesture *g);
int ri_rknb_move(struct RiGesture *g);
int ri_rknb_release(struct RiGesture *g);

#endif
