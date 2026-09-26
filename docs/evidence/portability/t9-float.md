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
- Denormal policy: RESOLVED — see FTZ section below (was: proposal
  awaiting owner sign-off).
## FTZ locked ON (2026-09-26, owner resolution §9.3)

- `platform/pal/ri_pal_fpu.h` (`ri_pal_fpu_setup()`, stdint-only) +
  `platform/host/fpu_host.c` (x86 MXCSR FTZ+DAZ `0x8040`, AArch64 FPCR FZ,
  no-op elsewhere) + `platform/aros/fpu_aros.c` (same SSE bits).
- Wired at AROS render-task entry (`live_task`); the synchronous host null
  backend deliberately does NOT call it (would flip its caller's state) —
  host determinism stays on compiler flags. T11 WASAPI thread will call it.
- `tests/unit/t94_pal_fpu.c`: MXCSR bits pinned, runtime subnormal
  (`1e-38*0.5`) flushes to zero, CSR restored after. Mutant (DAZ off):
  `FAIL ftz+daz bits` (killed).

## T9 remainder sweep (2026-09-26, recorded for the T10 round)

- **VLAs:** clean — core, PAL, and all host backends compile with
  `-Wvla -Werror` (pcf needs its usual `-DPCF_TABLE_VERIFIED=1`).
- **`%llu`:** host-side event dumps only (`audio_io/audio.c:699`,
  `tools/render.c` ×4) — needs `__USE_MINGW_ANSI_STDIO`/PRIu64 handling at
  the mingw gate. No `long double` anywhere.
- **`fwrite`:** field-by-field everywhere (`rbng.c`, `rbnm.c`) — no struct
  dumps. (Plus `fopen` text-mode: writers use `"wb"` already — spot-held.)
