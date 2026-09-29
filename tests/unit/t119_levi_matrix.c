/* t119_levi_matrix — mod-matrix slot core (v2 feature 4a, owner order).
 * 32 slots (source, destination, depth%); evaluation sums normalized
 * offsets per destination from the signals that exist today (per-op
 * contours, keytrack note). LFO/macro/velocity sources arrive with
 * their slices; the table shape already fits them. Pure, no DSP.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "tests/helpers/ri_assert.h"
#include "engine/dsp/levi_matrix.h"

int main(void) {
    struct RILeviMatrix mx;
    float dst[RI_LEVI_MD_N];
    float openv[8] = { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t i;
    ri_levi_matrix_init(&mx);
    /* Fail-closed. */
    RI_ASSERT(ri_levi_matrix_set(0, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 50) == 2, "null mx");
    RI_ASSERT(ri_levi_matrix_set(&mx, 32u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 50) == 2, "bad slot");
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_N, RI_LEVI_MD_CUTOFF, 50) == 2, "bad src");
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_N, 50) == 2, "bad dst");
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 101) == 2, "depth hi");
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, -101) == 2, "depth lo");
    RI_ASSERT(ri_levi_matrix_eval(0, openv, 60u, dst) == 2, "eval null");
    RI_ASSERT(ri_levi_matrix_eval(&mx, 0, 60u, dst) == 2, "eval null src");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, 0) == 2, "eval null dst");
    RI_ASSERT(ri_levi_matrix_enable(0, 2u, 1u) == 2, "enable null");
    RI_ASSERT(ri_levi_matrix_enable(&mx, 32u, 1u) == 2, "enable bad");
    /* Empty table = silence (all zeros). */
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, dst) == 0, "empty ok");
    for (i = 0u; i < RI_LEVI_MD_N; i++)
        RI_ASSERT(dst[i] == 0.0f, "empty zero %u", i);
    /* Slot 0: op-3 contour -> cutoff, +50%; env is 1.0 -> +0.5. */
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 50) == 0, "set");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, dst) == 0, "eval");
    RI_ASSERT(dst[RI_LEVI_MD_CUTOFF] == 0.5f, "offset %f", dst[RI_LEVI_MD_CUTOFF]);
    RI_ASSERT(dst[RI_LEVI_MD_RESO] == 0.0f, "other zero");
    /* Depth -100 inverts; depth 0 mutes the slot. */
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, -100) == 0, "invert");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, dst) == 0, "eval2");
    RI_ASSERT(dst[RI_LEVI_MD_CUTOFF] == -1.0f, "neg %f", dst[RI_LEVI_MD_CUTOFF]);
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 0) == 0, "mute");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, dst) == 0, "eval3");
    RI_ASSERT(dst[RI_LEVI_MD_CUTOFF] == 0.0f, "muted");
    /* Two slots sum on one destination (apply clamps downstream). */
    RI_ASSERT(ri_levi_matrix_set(&mx, 0u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 100) == 0, "d1");
    RI_ASSERT(ri_levi_matrix_set(&mx, 1u, RI_LEVI_MS_OPENV3, RI_LEVI_MD_CUTOFF, 100) == 0, "d2");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, dst) == 0, "eval4");
    RI_ASSERT(dst[RI_LEVI_MD_CUTOFF] == 2.0f, "sum %f", dst[RI_LEVI_MD_CUTOFF]);
    /* Keytrack: note 60 -> 0, 120 -> +1 (bipolar (n-60)/60 law). */
    RI_ASSERT(ri_levi_matrix_set(&mx, 2u, RI_LEVI_MS_NOTE, RI_LEVI_MD_RESO, 100) == 0, "rk");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 60u, dst) == 0, "eval5");
    RI_ASSERT(dst[RI_LEVI_MD_RESO] == 0.0f, "center %f", dst[RI_LEVI_MD_RESO]);
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 120u, dst) == 0, "eval6");
    RI_ASSERT(dst[RI_LEVI_MD_RESO] == 1.0f, "top %f", dst[RI_LEVI_MD_RESO]);
    /* Slot enable gates without losing depth. */
    RI_ASSERT(ri_levi_matrix_enable(&mx, 2u, 0u) == 0, "disable");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 120u, dst) == 0, "eval7");
    RI_ASSERT(dst[RI_LEVI_MD_RESO] == 0.0f, "disabled");
    RI_ASSERT(ri_levi_matrix_enable(&mx, 2u, 1u) == 0, "enable");
    RI_ASSERT(ri_levi_matrix_eval(&mx, openv, 120u, dst) == 0, "eval8");
    RI_ASSERT(dst[RI_LEVI_MD_RESO] == 1.0f, "reenabled");
    RI_RESULT("levimatrix");
}
