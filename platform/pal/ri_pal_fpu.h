/* ri_pal_fpu.h — denormal policy (portability plan T9, owner-locked FTZ ON).
 * Set at each render-thread start through ri_pal_fpu_setup().
 * C99, includes only <stdint.h> (plan §2 gate); the per-backend .c files
 * carry the compiler/arch specifics.
 */
#ifndef RI_PAL_FPU_H
#define RI_PAL_FPU_H
#include <stdint.h>

/* Enable flush-to-zero (+ denormals-are-zero where the arch has it) for
 * the CALLING thread. Idempotent; safe to call on non-render threads
 * (host test does). Backends call it at render-thread entry (AROS render
 * task, future WASAPI thread). The synchronous host null backend does NOT
 * call it (it would flip its caller's state); host determinism comes from
 * compiler flags, and the host test pins the setter itself. */
void ri_pal_fpu_setup(void);

#endif
