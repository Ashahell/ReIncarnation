# T9 — Compiler and float portability (D1 cross-compiler proof)

- Date: 2026-09-26. Toolchains: `gcc 16.2.1`, `clang 22.1.8` (both Linux x86-64).
- Flags (both): `-std=c99 -O2 -Wall -Wextra -Werror -pedantic
  -ffp-contract=off -fno-unsafe-math-optimizations -ftrapv`
  (+ `-DPCF_TABLE_VERIFIED=1` for `engine/fx/pcf.c`).
- Method: compiled the full host core TU list with clang into
  `/tmp/ri/clang-out/`, linked `tools/render.c`, re-rendered goldens,
  `cmp -s` against the committed GCC-rendered goldens.

## Result: bit-identical across GCC and Clang

- `tests/golden/songs/sched-check.wav` + events: IDENTICAL
- `tests/golden/303/first-light.wav`: IDENTICAL
- 808 `bd/sd/ch/oh/cy/storm`: IDENTICAL (6/6 sampled)
- 909 `bd/sd/ch/oh/cr/rd`: IDENTICAL (6/6 sampled)
- `tests/golden/pcf/pcf-sweep.wav`: IDENTICAL
- `tests/golden/mixer/mix-four.wav`: IDENTICAL

No golden differs; no per-golden root-cause list needed.

## Remaining T9 items (open)

- mingw-w64 compile-only gate: `x86_64-w64-mingw32-gcc` not installed on
  this machine — gate pending (T10 owns the gate; record SKIP here).
- MSVC `/fp:strict` proof: no Windows toolchain here — deferred to T11.
- Denormal policy: no explicit FTZ/DAZ setup today (implicit per OS).
  Proposal (owner decision §8.3): flush-to-zero ON at render-thread start
  through `ri_pal_fpu_setup()`; needs owner sign-off before wiring.
- VLA / `%llu` / struct-`fwrite` sweep: pending (T10 grep gate + mingw gate
  will enforce; `rbng.c`/`rbnm.c` must be verified to never `fwrite` a struct).
