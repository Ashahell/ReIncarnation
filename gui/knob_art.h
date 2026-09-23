/* gui/knob_art.h — procedural 909-style knob frames (Module 2.9 art).
 *
 * Original pixels, drawn not copied: dark charcoal body, orange
 * radial pointer, 64x64 RGBA @2x of the locked 32 px knob. Matches
 * the ri_knob_pointer_mdeg sweep (tested in t29_knobart).
 *
 * Host + AROS safe: stdint only, no libm (Q15 sine table), no
 * allocation (caller-owned buffer). The AROS blit path consumes
 * these frames; eyeball verification against references lives in
 * the session record, not in this header.
 */
#ifndef RI_KNOB_ART_H
#define RI_KNOB_ART_H
#include <stdint.h>

#define RI_KNOB_PX 64u /* frame edge */
#define RI_KNOB_BODY_TOP 0x454545u
#define RI_KNOB_BODY_BOT 0x1c1c1cu
#define RI_KNOB_RIM 0x0d0d0du
#define RI_KNOB_POINTER 0xe07b2eu
#define RI_KNOB_HUB 0x101010u
#define RI_KNOB_HUB_RIM 0x5a5a5au

/* Q15 sine table, degrees 0..359 (exact cardinals, norm-checked). */
extern const int16_t RI_SIN_Q15[360];

/* Render one frame into rgba (64*64*4 bytes, row-major RGBA):
 * transparent outside the disc, gradient body, rim + rim-light +
 * top-left highlight, orange pointer at the value angle
 * (r 6..22, 3 px), hub + hub ring. Deterministic. */
void ri_knob_render_frame(unsigned char *rgba, int value);

#endif
