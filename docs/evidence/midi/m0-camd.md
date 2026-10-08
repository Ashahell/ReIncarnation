# M0: AROS CAMD prerequisites (2026-10-08)

## Upstream verification (all confirmed 2026-10-08)

Upstream master tip `7b9b93dcb1`; both fixes by Jaime Dias, 2026-10-05,
both ancestors of `origin/master`:

- `28ec43a517` "camd: format cluster names through a va_list" —
  `mysprintf()` read varargs as `void *start = &fmt + 1` (stack-only);
  now `VNewRawDoFmt` through a `va_list`. Same fix G7 made locally.
- `cb8c4c5f39` "camd: skip arena hunks when scanning a driver for
  MidiDeviceData" — ELF arena loader stores hunk size 0; unscanned,
  `LoadDriver()` wrapped to ~4 GB off the end of memory, which is why
  the first camd open blocked on `DEVS:Midi/debugdriver`.

Our trees (both lack both fixes):

- ABIv1 (`src/abi/v1/AROS @ c5e043a2ff`): `merge-base --is-ancestor`
  says no for both; `strings.c:47` still has `void *start=&fmt+1;`.
- ABIv11 (`src/abi/v11/AROS @ fbc242c104`): `strings.c` still broken;
  `drivers.c`/`openmididevice.c` have no arena-hunk handling.

## Patch carriage (Vulkan4AROS branch `levi-midi-m0`)

Upstream diffs carried verbatim with a `#` credit header (upstream
commit + author; `git apply`/`patch -p1` tolerate the header —
dry-run proven). Content verified to apply cleanly to both trees.

- v1 (SERIES mechanism): `src/abi-patches/v1/aros/0073-camd-cluster-names-va_list.diff`,
  `0074-camd-loaddriver-skip-arena-hunks.diff`, appended to `SERIES`.
- v11 (`patches/*.v11.patch` mechanism, e1000 precedent):
  `patches/camd-cluster-names-va_list.v11.patch`,
  `patches/camd-loaddriver-skip-arena-hunks.v11.patch`.
- The G7 local diff is retired as superseded (note beside it, file kept).

Trees were left pristine after verification (patches applied for the
build checks only, then reverted); the carriage is the record.

## ABIv1 build (riqemu1 lane)

`make workbench-libs-camd` in `src/abi/v1/core-pc-x86_64` (in-tree
16.1.0 toolchain):

- before: sha256 `6d0b0fae…`, 53408 B.
- after: sha256 `d4a7f97d…`, 53472 B, `$VER: camd.library 41.1 (28.9.2026)`.
- Fix proven in the binary: `mysprintf` spills the registers into a
  `va_list` and calls `VNewRawDoFmt` (`call *-0x448(%r8)`); the
  `&fmt+1` pattern is gone.
- ABI: entry points read the base from C args; nested calls pass the
  callee base in RDX (e.g. `mov %r14,%rdx; call *-0x48(%r14)`); R12 is
  only callee-saved base-hold. Modern rdx convention, correct for ABIv1
  by construction (same in-tree toolchain as the running system).

## ABIv11 build (Dell lane) — PARTIAL, link blocked

- Both patched files compile under the v11 16.1.0 toolchain against
  the v11 SDK; the v11 `mysprintf` object carries the fix.
- The full `camd.library` link is blocked on this machine: the v11
  build tree (`core-pc-x86_64`, configured 2026-09-14) references gcc
  `10.5.0` internal headers, but the toolchain dir now holds only
  `16.1.0` (`compiler/alib` fails first). Needs an owner
  re-provision (reconfigure) of the v11 tree — out of M0 scope.
- Existing v11 binary (unbuilt here): `$VER: camd.library 41.1
  (14.9.2026)`, 54904 B, sha256 `5470d033…`.

## Blocked on lanes (no Dell/riqemu1 access from here)

- riqemu1: deploy the v1 binary, reboot at 1280x1024, re-run the G7
  proof (`MIDISEND` into `RISECT remote`), un-park
  `DEVS:Midi/debugdriver` and record whether the first camd open
  still blocks. Ready when the lane is up.
- Dell: probe the running `camd.library` (version string, size, hash;
  `MIDISEND SELFTEST` for garbled cluster names), stage the fixed
  ABIv11 build in `RAM:`, owner installs + reboots. Ready when the
  lane is up.
