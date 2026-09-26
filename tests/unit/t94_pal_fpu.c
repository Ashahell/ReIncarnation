/* t94_pal_fpu — portability T9: FTZ setter pins denormal flushing.
 * Calls ri_pal_fpu_setup, checks the x86 MXCSR FTZ+DAZ bits, proves a
 * runtime-computed subnormal flushes to zero, then restores the CSR.
 */
#include <stdio.h>
#include <stdint.h>
#include "tests/helpers/ri_assert.h"
#include "platform/pal/ri_pal_fpu.h"

#if defined(__x86_64__) || defined(__i386__)
#include <xmmintrin.h>
#define T94_X86 1
#else
#define T94_X86 0
#endif

int main(void) {
#if T94_X86
    unsigned int before, after;
    volatile float d;
    float r;
    before = (unsigned int)_mm_getcsr();
    ri_pal_fpu_setup();
    after = (unsigned int)_mm_getcsr();
    RI_ASSERT((after & 0x8040u) == 0x8040u, "ftz+daz bits %08x", after);
    d = 1.0e-38f;
    r = (float)(d * 0.5f); /* 5e-39: subnormal -> must flush */
    RI_ASSERT(r == 0.0f, "denormal flushed");
    _mm_setcsr(before); /* polite: restore the test process state */
    RI_ASSERT(((unsigned int)_mm_getcsr() & 0x8040u) == (before & 0x8040u), "restored");
#else
    ri_pal_fpu_setup();
    printf("note: non-x86 FTZ assumed by backend\n");
#endif
    RI_RESULT("pal_fpu");
}
