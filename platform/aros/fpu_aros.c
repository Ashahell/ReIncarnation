/* fpu_aros.c — AROS FTZ backend (portability plan T9).
 * AROS-only. x86-64 render task: SSE MXCSR FTZ + DAZ, same bits as host.
 */
#ifndef __AROS__
#error "fpu_aros.c is AROS-only (portability plan T9)"
#endif

#include "platform/pal/ri_pal_fpu.h"

#include <xmmintrin.h>
#include <pmmintrin.h>

void ri_pal_fpu_setup(void) {
    _mm_setcsr((unsigned int)(_mm_getcsr() | 0x8040u));
}
