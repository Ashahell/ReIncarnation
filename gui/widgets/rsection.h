/* gui/widgets/rsection.h — RSection: one canvas class for every laid-out
 * section (§12.10 G4). AROS-ONLY. Draws from the control registry
 * (gui/ctlreg.h), the measured geometry (gui/panelgeo.h) and the section
 * behaviour interface (gui/sectui.h); owns its pixels.
 */
#ifndef RI_RSECTION_H
#define RI_RSECTION_H

#ifndef __AROS__
#error "rsection.h is AROS-only: Zune custom class API, never in the host build"
#endif

#include <exec/types.h>
#include <utility/tagitem.h>
#include <libraries/mui.h>
#include "gui/sectui.h"
#include "gui/panelui.h"

struct MUI_CustomClass;

/* Read-only LONG: bumps once per state change; notify on it. */
#define MUIA_RSection_Changes (TAG_USER + 0x52534301u)
/* Read-only pointer: struct RISectUI * of the live state. */
#define MUIA_RSection_State (TAG_USER + 0x52534302u)
/* Read-only pointer: const struct RSectionDiag * (event plumbing proof). */
#define MUIA_RSection_Diag (TAG_USER + 0x52534303u)

/* Settable pointer: struct RIPanelUI * shared by every canvas of a window
 * (focus bar, click-to-focus, keyboard). NULL = standalone section. */
#define MUIA_RSection_Panel (TAG_USER + 0x52534304u)
/* Settable BOOL: this canvas takes the window's raw keys for the panel
 * (exactly one canvas per window). */
#define MUIA_RSection_KeyOwner (TAG_USER + 0x52534305u)
/* Settable LONG zoom index (S5): 0 = 1x, 1 = 1.5x, 2 = 2x,
 * RI_GEO_ZOOM_COMPACT = transport strip. Stores the index and frees the
 * off-screen bitmap (reallocated at the new size on the next draw);
 * out-of-range values are ignored. Set inside InitChange/ExitChange so
 * the window relayouts around the new minima. */
#define MUIA_RSection_Zoom (TAG_USER + 0x52534306u)

struct RSectionDiag {
    LONG events;          /* MUIM_HandleEvent calls with a message */
    LONG buttons;         /* of which IDCMP_MOUSEBUTTONS */
    LONG last_x, last_y;  /* last button position, canvas-local px */
    ULONG last_hit;       /* reg_id hit, 0xFFFF none */
    LONG setups, shows;   /* lifecycle calls seen */
    ULONG df_max, df_sum; /* S3: full-draw us, max + window sum */
    LONG df_n;            /* ... and count */
    ULONG dp_max, dp_sum; /* S3: partial-draw us, max + window sum */
    LONG dp_n;            /* ... and count */
    /* WHICH SECTION this canvas is (2026-10-05). One canvas is one section, so
     * this is constant per widget and the heartbeat can pick the argmax of
     * dp_build_sum across widgets and report which section the build cost
     * actually belonged to. Added because the item cull was aimed using an
     * ASSUMPTION about which section the lane repaints, and the assumption had
     * never been measured -- the same mistake shape as the gap's `max`. */
    uint8_t dp_sec;
    ULONG blit_max;       /* full draw: BltBitMapRastPort share, max us */
    ULONG alloc_n;        /* back-buffer (re)allocations */
    /* Box repaints split by the reason they were asked for (RI_RSEC_BOX_*),
     * so an expensive partial can be attributed to the caller that caused it.
     * Index 0 unused, so _n counts line up with the reason codes. */
    ULONG dpw_max[4], dpw_sum[4];
    LONG dpw_n[4];
    /* Box repaint split build vs replay+blit (Dell A,B,B,A 2026-10-02:
     * a tab-switch box cost 46-91 ms and build_dl is the suspect; the
     * damage path does not know yet, so it is measured). */
    ULONG dp_build_max, dp_build_sum;
    /* PARTIAL-PATH REPLAY AND BLIT, timed separately (2026-10-04).
     *
     * Why this exists, and it is a gap that cost a wrong conclusion. `blit_max`
     * is written ONLY on the full-draw path; the partial path recorded `dp_*`
     * and `dpw_*` and returned without ever timing its own replay or blit. So
     * `build_max ~= part_max` was the only split available, and that reading is
     * true in one window and false by 10x in the other -- which is how a
     * 1.36 s draw got filed as a display-list BUILD problem when at least 90 % of
     * it was replay and blit. **The number that would have settled it was never
     * collected.**
     *
     * These three therefore cover the whole partial path, and the three must
     * sum to `dp_*`:
     *   dp_replay_max/sum  replay_dl_dmg
     *   dp_blit_max/sum    BltBitMapRastPort
     *   dp_build_max/sum   build_dl (already present, kept here for symmetry)
     */
    ULONG dp_replay_max, dp_replay_sum;
    /* dp_blit_* now reads ZERO for partials by construction (2026-10-05):
     * partials direct-paint into the window, so there is nothing to blit. Kept
     * because the full-draw path still blits, and because a non-zero reading
     * here would mean a partial had been routed back through the buffer --
     * which is exactly the regression worth catching. */
    ULONG dp_blit_max, dp_blit_sum;
    /* THE GAP: wall time inside draw_frame attributable to NO phase, i.e.
     * `us - (us_build + us_replay + us_blit)`. It is the four intervals between
     * the ReadEClock pairs, which contain no code at all -- so a large gap is not
     * an unmeasured phase, it is the CPU not being there.
     *
     * This is the number that distinguishes "this phase is slow" from "this draw
     * was interrupted", and it must be the SUM of the phases that is subtracted:
     * they are disjoint intervals, so subtracting the maximum instead silently
     * books the other two phases into this bucket (2026-10-05, see the
     * implementation note). With the sum it behaves as intended -- ~16 us quiet,
     * and ~2722 us during the 1.36 s stall, where all three components read
     * normal. One blit was interrupted in a different window, where the gap
     * stayed at ~19 us. Same fault, two different victims, and only the gap
     * tells them apart.
     *
     * Costs nothing: it is a subtraction of numbers already logged, not another
     * ReadEClock (each read is ~2.2 us here, ~8 % of a quiet partial). */
    ULONG dp_gap_max, dp_gap_sum;
    /* Minimum, not another average: preemption is ONE-SIDED, so the minimum of
     * N samples is a lower bound on the real work and needs no perturbation of
     * the thing being measured. `blt_max` beside `blt_min` is a direct
     * preemption-amplitude readout. */
    ULONG dp_blit_min;
    /* REDUNDANT-INVALIDATION COUNTERS (2026-10-04). The 1.36 s Dell stall was
     * attributed to this path -- box_bar with dp_build ~= dp_max, 25 refreshes
     * in one window at 74.6 ms average, against 202 us in the quiet windows --
     * and nothing recorded WHY a BAR refresh that normally costs 202 us
     * sometimes costs 74 ms, twenty-five times over.
     *
     * The leading question is whether the burst is the SAME invalidation
     * repeated. If it is, a cached display list collapses the whole burst to one
     * build and the stall is an optimisation. If each one differs, the cost is
     * real work and caching will not help. So: consecutive box invalidations
     * with an identical (why, x0, y0, x1, y1) are counted separately from the
     * ones that differ. Neither is a verdict -- it is the measurement that
     * decides between "cache it" and "do not bother".
     *
     * CORRECTED 2026-10-04: this is a CALLER-SPAM counter, NOT a cache oracle.
     * An identical damage rect does not mean identical work -- two requests for
     * the same rectangle can bracket a state change -- so dpr_rep counts how
     * often the same box was asked for twice, which is useful, and says nothing
     * about whether the result was the same. Reading it as "a cache would hit"
     * is a category error: a cache can only skip work whose RESULT is unchanged.
     */
    ULONG dpr_rep;        /* repeats: same why AND same rect as the previous */
    ULONG dpr_new;        /* first-of-a-kind: differed from the previous */
    ULONG dpr_run_max;    /* longest run of consecutive repeats */
    ULONG dpr_run_now;    /* current run, reset by dpr_new */
    int dpr_why;          /* reason of the previous invalidation */
    int dpr_x0, dpr_y0, dpr_x1, dpr_y1;
};

struct MUI_CustomClass *ri_rsection_class(void);
void ri_rsection_dispose_class(void);
/* section = RI_SEC_*; zoom 0 = 1x, 1 = 1.5x, 2 = 2x. NULL = class failed. */
APTR ri_rsection_create(ULONG section, LONG zoom);
/* Redraw after the state was changed from outside (demo / automation). */
void ri_rsection_refresh(APTR obj);
/* S3: redraw a canvas-local box after a meter/chase change (meters, lamps);
 * anything invalid falls back to a full redraw. Coordinates are canvas px
 * (sections are exactly canvas-sized). */
void ri_rsection_refresh_box(APTR obj, int x0, int y0, int x1, int y1);
/* Why a box repaint was requested, so the repaint cost can be attributed:
 * a "partial" of 4 ms and a "partial" of 300 ms are different failures, and
 * the aggregated n=/max= pair cannot tell them apart. Recorded per canvas in
 * RSectionDiag as dp_box_*; reasons that escalate to a full redraw are
 * still counted as full (that is what actually happened). */
/* The codes and the folding rule live in gui/panelui.h (RI_RSEC_BOX_*,
 * ri_rsection_box_why) so the policy is host-testable: rsection.h is
 * AROS-only and cannot be included by a host test at all. */
void ri_rsection_refresh_box_why(APTR obj, int x0, int y0, int x1, int y1, int why);
void ri_rsection_set_box_why(int why); /* meter_round's next refresh_box call */
/* Replay an app-built display list (rack furniture) with the canvas
 * colour path; needs a section canvas set up on the same screen. */
struct ri_dlist;
void ri_rsection_replay(struct RastPort *rp, const struct ri_dlist *dl);
#endif
