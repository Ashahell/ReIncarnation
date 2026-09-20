#ifndef RI_KERNELS_H
#define RI_KERNELS_H
/* Measured deterministic kernels (Task 2, gate G2).
 * Pure C, no libm calls, no globals, no while loops.
 * All loops are constant-bounded (scaling loops only).
 * Internal polynomial evaluation uses double precision with a single
 * final float rounding; IEEE-754 double ops are bit-deterministic for
 * the same binary on the same arch (D1).
 * Documented domains: tanh [-8,8], exp [-8,8],
 * sin any float (caller wraps phase; kernel clamps cycles to +-64),
 * pow2 [-32,32].
 */
float ri_tanh(float x);
float ri_exp(float x);
float ri_sin(float x);
float ri_pow2(float x);
#endif
