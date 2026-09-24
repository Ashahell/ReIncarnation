#ifndef RI_KERNELS_H
#define RI_KERNELS_H
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
float ri_tanh(float x);
float ri_exp(float x);
float ri_sin(float x);
float ri_pow2(float x);
#endif
