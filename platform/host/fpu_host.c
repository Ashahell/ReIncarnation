/* fpu_host.c — host FTZ backend (portability plan T9).
 * x86/x64: SSE MXCSR FTZ (bit 15) + DAZ (bit 6). AArch64: FPCR FZ (bit 24).
 * Anything else: no-op (documented; CI runs x86-64).
 */
#include "platform/pal/ri_pal_fpu.h"

#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#include <pmmintrin.h>
void ri_pal_fpu_setup(void) {
    _mm_setcsr((unsigned int)(_mm_getcsr() | 0x8040u));
}
#elif defined(__aarch64__) || defined(_M_ARM64)
void ri_pal_fpu_setup(void) {
#if defined(_MSC_VER)
    __writefpcr(__readfpcr() | (1u << 24));
#else
    unsigned long fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ul << 24);
    __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
}
#else
void ri_pal_fpu_setup(void) {
}
#endif
