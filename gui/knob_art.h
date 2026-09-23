/* gui/knob_art.h — procedural 909-style knob frames (Module 2.9 art).
 *
 * Original pixels, drawn not copied: warm dark-olive body, orange
 * short pointer, tick ring, offset shadow; 80x80 RGBA frames.
 * Geometry measured from reference photos (E0 values below);
 * pointer sweep matches ri_knob_pointer_mdeg (tested).
 *
 * Host + AROS safe: stdint only, no libm (Q15 sine table), no
 * allocation (caller-owned buffer). The AROS blit path consumes
 * these frames; eyeball verification against references lives in
 * the session record, not in this header.
 */
#ifndef RI_KNOB_ART_H
#define RI_KNOB_ART_H
#include <stdint.h>

#define RI_KNOB_PX 80u /* frame edge */
#define RI_KNOB_BODY_TOP 0x4c4534u
#define RI_KNOB_BODY_BOT 0x31281bu
#define RI_KNOB_RIM 0x262012u
#define RI_KNOB_POINTER 0xe37c3bu
#define RI_KNOB_TICK 0x3e3a34u
#define RI_KNOB_SHADOW_ALPHA 64u
#define RI_KNOB_SHADOW_DX 2
#define RI_KNOB_SHADOW_DY 3

/* Q15 sine table, degrees 0..359 (exact cardinals, norm-checked). */
extern const int16_t RI_SIN_Q15[360];

/* Render one frame into rgba (80*80*4 bytes, row-major RGBA):
 * shadow, gradient body, rim + rim-light + highlight, tick ring
 * (11 dashes, 270-degree span, bottom gap), orange pointer
 * (r 15..24, 5 px). Deterministic. */
void ri_knob_render_frame(unsigned char *rgba, int value);

#endif
