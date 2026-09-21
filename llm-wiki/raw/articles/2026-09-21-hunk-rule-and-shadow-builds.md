# 2026-09-21 — x86-64 HUNK alignment rule + shadow-build recipes (device/drivers)

> Source: session evidence (host builds, disassembly, guest serial/Guru) plus third-party consultant mechanism, compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Formalizes the alignment finding from the codec-less campaign into a build
rule, and records the exact shadow-build recipes so every test binary in
play can be rebuilt from source. (Retires the 1-byte sb128 binary patch.)

## 1. The rule

An x86-64 AROS HUNK module MUST NOT assume HUNK section data is 16-byte
aligned, even when its ELF section declares `sh_addralign = 16`.

- Mechanism: the AOS HUNK loader defines the payload as
  `GETHUNKPTR(x) = BADDR(hunktab[x]) + sizeof(BPTR)`; on x86-64
  `sizeof(BPTR)` is 8. An allocation that is 16-byte aligned therefore
  yields a payload at mod16 = 8. (Consultant-sourced; matches AROS sources.)
- Proof on this guest: stock sb128 `DriverInit` file VMA `0x590` appeared
  in the Guru disassembly at load `0x4A458358`, so `.ltext` payload base is
  `0x4A458358 - 0x590 = 0x4A457DC8`, and `0x4A457DC8` mod 16 = 8.
  Its `movaps (.rodata+0x440)` (in-section offset mod16 = 0) faulted with
  #GP exactly as predicted.
- Forbidden against HUNK data: `movaps`, `movapd`, `movdqa` (aligned loads,
  stores, and compiler-synthesized constant/table copies). Safe:
  `movups`, `movdqu`, or independently allocated 16-aligned runtime buffers.
- Applies to: constants, vector globals, static tables, aligned locals,
  constant pools, struct fields, SIMD temporaries, `memcpy`/`memset`
  expansions, auto-vectorized loops, and store-merged scalar sequences.
- Compiler flags that remove the class for affected TUs (clang 20.1.0,
  verified by disassembly to 0 vector ops): `-fno-vectorize
  -fno-slp-vectorize`. Store-merged sequences additionally need source
  treatment: initialize through a `volatile`-data pointer (verified: the
  16-byte `.rodata` vendor blob disappears, scalar stores remain).
- Gate every rebuilt module host-side before deploying:
  `objdump -d` shows 0 `movaps|movapd|movdqa` in the affected functions;
  `readelf -s` shows exactly 1 UND entry (the null entry); `readelf -h`
  shows `Type: REL`.

## 2. Toolchain facts used by all recipes below

- Compiler: `src/abi/v1/toolchain-core-x86_64/bin/clang`
  (`--target=x86_64-unknown-aros`), sysroot
  `src/abi/v1/core-pc-x86_64/bin/pc-x86_64/AROS/Developer`.
- Common TU flags: `-mcmodel=large -mno-red-zone
  -fno-asynchronous-unwind-tables -fno-omit-frame-pointer -fno-common
  -fno-builtin-floor -O2 -DCPU='"x86_64"'
  -DAROS_BUILD_TYPE=AROS_BUILD_TYPE_PERSONAL -DAROS_USE_LOGRES
  -Wno-pointer-sign` plus include dirs (build dir for `version.h`,
  generated `gatestubs.h` dir where needed, driver/Device dir, Common dir,
  AHI tree, `gen/workbench/devs/AHI`, `gen/.../Include/gcc`, Developer
  include). Probes additionally use x86_64-aros-gcc with `-ffixed-r12
  -fno-builtin` and the `libcrt` shim + `-lstdcio -lposixc -ldos -lexec`.
- Link: `ld.lld -r` (the clang driver path needs the unbuilt
  `collect-aros`, so link lld directly). Result must be ET_REL.
- `gatestubs.o`: generated from the common `ahi_sub_lib.sfd`, reused
  across drivers as-is. `library.o` (Common/library.c) must be rebuilt PER
  driver (embeds `LibName` via `-DDRIVER`).
- `_set_call_funcs` (libautoinit) will be UND: none of these objects carry
  `.init_array`/ctors (verified via readelf), so a stub returning 1 is
  behaviorally identical and avoids the libautoinit archive chain (which
  pulls further UNDs). AROS ELF symbols carry the leading underscore.
- Driver TUs that use out-of-line exec access additionally need a BSS
  `struct ExecBase *SysBase` definition (`sysbase_stub.c`) plus `NewList`
  (from `clib_stubs.o`); keep `SysBase` OUT of `clib_stubs.o` (the device
  already defines one via its own header — duplicates break its relink).

## 3. Proven recipes and outputs (all 2026-09-21, all guest-tested)

- `ahi.device` shadow (`ahi_newsafe.device`, 177480 B): all 24 device
  objects with `mixer.o` replaced by `mixer_safe.o` (mixer.c +
  `-fno-vectorize -fno-slp-vectorize`), plus `clib_stubs.o` (HookEntry to
  `h_SubEntry`, NewList, overlap-safe memcpy, memset, strcpy, strlen,
  bounded minimal snprintf — the device's only snprintf site is
  `"%s/%s.audio"`). Markers M20/M22-M24/M30-M31/M40-M48 present.
- `sb128.audio` shadow (48984 B, volatile-init `driver-init.c`):
  `driver-init.o sb128hw.o accel.o misc.o interrupt.o pci_wrapper.o` +
  per-driver `library.o` + reused `gatestubs.o` + `setcall_stub.o` +
  `sysbase_stub.o` + `clib_stubs.o`. Version 5.28 (`version.h`:
  `VERSION 5`, `REVISION 28`, `VERS "5.28 (15.11.2025)"`).
  Guest-verified in place of the retired 1-byte patch: scan passes SB128
  silently, full green run preserved.
- `void.audio` shadow with Stop markers (12760 B): `void-init.o
  void-main.o void-playslave.o void-accel.o` (93+59-line TUs are genuinely
  tiny) + per-driver `library.o` + reused `gatestubs.o` +
  `setcall_stub.o`. Version 6.3 (`VERSION 6`, `REVISION 3`,
  `VERS "6.3 (28.09.05)"`).
- `hdaudio.audio` shadow (59000 B, M1–M11 instrumented): pre-existing
  `/tmp/hdabuild` flow (`driver-init.o main.o misc.o accel.o interrupt.o
  hda_hidd.o` + `library.o` + `gatestubs.o`).
- Probes (v1 SDK, `-no-pie`, startup.o, CRT shim): `probe_ahi` (+r2
  device/unit copy fix, in-repo), `probe_ahi_dbg` (PDBG breadcrumbs, P4
  ladder, abort bounds), `progdir_probe` (PROGDIR forced load),
  `nomodescan_probe`, `devlist_probe`, `loadseg_probe2`.
- Deployment: `PROGDIR:ahi.device` forced load (the `devs:` assign path
  serves stock bytes to lddemon for unknown reasons — ZZTEST experiment
  still open); drivers via `DEVS:` assign (works for explicit
  `DEVS:AHI/*.audio` opens).

## 4. Long-term options (consultant's, not yet decided)

- A: toolchain contract — never assume >8-byte HUNK alignment.
- B: linker/converter padding preserving section alignment.
- C: loader allocating so `BADDR(hunk)+sizeof(BPTR)` is 16-aligned
  (allocation mod16 = 8). Consequences for allocator bookkeeping — do not
  casually patch. Loader comment notes 64-bit HUNK segments deliberately
  live in 32-bit address space (`MEMF_31BIT`).

## 5. Standing measurements this rule protects

- P4.0/P4.1 TRUE-WaitIO err=0; ladder 7/7 REPLIED err1=err2=0;
  `dev_min_frames=64`; rc=0 SUMMARY; zero IRQ lines; CloseDevice returns
  (M46/M43); audit 0 errors / 2 pre-existing env warnings.
