#ifndef RI_KERNELS_H
#define RI_KERNELS_H
#include <stdint.h>
/* Measured deterministic kernels (Task 2, gate G2; totality §2.1/D-j).
 * Pure C, no libm calls, no globals, no while loops.
 * All loops are constant-bounded (scaling loops only).
 * Internal polynomial evaluation uses double precision with a single
 * final float rounding; IEEE-754 double ops are bit-deterministic for
 * the same binary on the same arch (D1).
 * Totality (D-j): every kernel is finite for every finite input.
 * Documented domains are PRECISION statements, not safety boundaries:
 * tanh accurate on [-8,8] (±1 outside by guard); exp accurate on
 * (-87, 88.73) (0 below, +Inf above); sin accurate while the cycle
 * count fits int64 (|x| < ~5.7e19; bounded 0.0 beyond — deterministic,
 * not meaningful); pow2 accurate on (-127, 128) (0 below, +Inf above).
 * In-domain code paths are unchanged bit-for-bit: existing goldens
 * re-render identically (verified by audit Phase 1/8 cmp).
 */
#ifdef RI_LEVI_PROFILE
/* Host-only call counters (levi-perf P1). Every kernel call through these
 * macros bumps its counter; the shipping build (macro undefined) compiles to
 * the plain declarations below, byte-identical (see G5 evidence). The bench
 * links profile objects from a separate directory; gated tests never define
 * this macro. */
extern uint64_t ri_prof_sin, ri_prof_pow2, ri_prof_log2, ri_prof_tanh,
    ri_prof_exp;
float ri_sin_impl(float x);
float ri_pow2_impl(float x);
float ri_log2_impl(float x);
float ri_tanh_impl(float x);
float ri_exp_impl(float x);
void ri_prof_reset(void);
#define ri_sin(x) (ri_prof_sin++, ri_sin_impl(x))
#define ri_pow2(x) (ri_prof_pow2++, ri_pow2_impl(x))
#define ri_log2(x) (ri_prof_log2++, ri_log2_impl(x))
#define ri_tanh(x) (ri_prof_tanh++, ri_tanh_impl(x))
#define ri_exp(x) (ri_prof_exp++, ri_exp_impl(x))
#else
float ri_tanh(float x);
float ri_exp(float x);
float ri_sin(float x);
float ri_pow2(float x);
/* log2(x) for x > 0 (exact at powers of two); -Inf for x <= 0 (deterministic
 * precision statement per D-j); NaN propagates; +Inf for +Inf. Bit tricks +
 * double polynomial, no libm. */
float ri_log2(float x);
#endif
#endif
