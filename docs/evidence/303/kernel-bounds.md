# Ledger row: deterministic kernel error bounds (Task 2, gate G2)

- Claim: `ri_tanh`, `ri_exp`, `ri_sin`, `ri_pow2` (`engine/dsp/kernels.c`)
  are bit-deterministic (same input => identical output, 10k LCG inputs),
  finite on all documented domains, and meet these measured max absolute
  errors vs libm on the T2 operating grids:
  tanh 5.96e-08 (bound 1e-6, grid [-4,4] step 0.01, n=801),
  exp 0 bit-identical (bound 2e-6, grid [-4,4] step 0.01, n=801),
  sin 5.96e-08 (bound 2e-6, grid [-12.56,12.56] step 0.02),
  pow2 0 bit-identical (bound 4e-6, grid [-10,10] step 0.05, n=400).
  Off-grid worst cases are large-magnitude points and are ~1 ulp
  relative: exp 1.22e-04 at x=7.0677 (ref 1173.4, rel 1.04e-07),
  pow2 6.10e-05 at x=9.5226 (ref 735.484, rel 8.3e-08).
- Source (T2 run output, pasted verbatim):
  ```
  PASS kernels
  BUILD test OK
  ```
  (`bash scripts/ri_build_host.sh test t2_kernels`, rc=0.)
  Max-error probes (throwaway, `/tmp/ri/run/t2/maxerr*.c`, not committed):
  `maxerr tanh=5.96e-08 exp=0 sin=5.96e-08 pow2=0`,
  `rand100k+fine maxerr tanh=5.96e-08 exp=0.000122 sin=5.96e-08 pow2=6.1e-05`,
  `exp rand max=0.000122 at x=7.0677 (ref=1173.4)`,
  `pow2 rand max=6.1e-05 at x=9.5226 (ref=735.484)`,
  `exp grid n=801 nonzero=0 max=0`,
  `pow2 grid n=400 nonzero=0 max=0`.
- Class: E4 (independently reproduced and regression-tested: T2 property
  test `tests/property/t2_kernels.c` asserts the bounds every run).
- Confidence: HIGH (bounds pass with 16x-33x margin on-grid; off-grid
  relative error ~1 ulp; determinism + finiteness asserted, incl.
  denormal-range inputs per lab rules).
- Method: operating-grid sweep vs libm (`tanhf`/`expf`/`sinf`/`powf`)
  with grid steps 0.01/0.01/0.02/0.05 (see test); 10k LCG inputs
  (seed 0x12345678) for determinism; denormal sweep 1e-38..1e-45.
- Fixture: `tests/property/t2_kernels.c` + `engine/dsp/kernels.h/.c`.
- Status: ACCEPTED (gate G2 green: `test t2_kernels` PASS + audit 0b green).
- Note: implementation deviates from the brief's Step-3 recipe (65-entry
  linear tanh table, order-4 exp, order-5 sin): measured/analytic check
  shows that recipe cannot meet the test bounds (linear-table worst
  1.499e-03 at x=-0.6875, i.e. 1500x over 1e-6; order-4 exp remainder
  ~4.2e-05 over the 2e-6 bound; order-5 sin remainder ~0.6 on
  [-pi,pi]). Kernels instead use double-precision interiors with exact
  power-of-two scaling and a single final float rounding; no libm calls,
  no globals, one constant-bounded `for (i=0;i<32;i++)` scaling loop.
  D1 scope only (same binary/arch); no D2 claim.
