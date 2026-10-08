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
- The full `camd.library` link was blocked on this machine: the v11
  build tree (`core-pc-x86_64`, configured 2026-09-14) references gcc
  `10.5.0` internal headers, but the toolchain dir now holds only
  `16.1.0` (`compiler/alib` fails first). Worked around per owner
  direction with a fresh build dir alongside (`core-pc-x86_64-m0`,
  ignored by git): `make workbench-libs-camd` EXIT 0 (see
  `riqemu1-m0-g7.md`). The Sep-14 tree was left untouched; repairing
  its stale config stays an owner item.
- Existing v11 binary (unbuilt here): `$VER: camd.library 41.1
  (14.9.2026)`, 54904 B, sha256 `5470d033…`.

## Dell probe, pre-fix (2026-10-08, agent e6320 — lane is up)

- Running `camd.library`: **41.1, 54904 bytes** (matches the unpatched
  ABIv11 tree binary; both upstream fixes absent).
- `MIDISEND m0probe SELFTEST` (ABIv11 build staged in `RAM:`): senders
  never connect (`connected: 0, 0`), cluster names are garbage
  (`00 61 6D 64`, zeros), no echo (`got 0`, rc=20). Full log:
  `dell-m0-selftest-pre.txt`. Exactly the broken behavior §0.8 predicts.
- No RIAPP was running; probe used its own cluster and cleaned up.

## Still blocked (owner lane)

- riqemu1: deploy the v1 binary, reboot at 1280x1024, re-run the G7
  proof (`MIDISEND` into `RISECT remote`), un-park
  `DEVS:Midi/debugdriver` and record whether the first camd open
  still blocks. Lane down from here.
- Dell: the fixed ABIv11 `camd.library` binary cannot be linked on
  this machine (v11 build tree configured for gcc 10.5.0, toolchain
  now 16.1.0 — needs an owner re-provision decision: fresh build dir
  vs reconfigure in place). Once built: stage to `RAM:`, owner
  installs to `LIBS:` and reboots (never `avail flush`), then re-run
  the SELFTEST above (expect `connected: 1, 1`, readable names,
  `got 2`, rc=0).

## Dell post-reboot (2026-10-08 — M0 Dell gate PASS)

Owner rebooted onto the staged build (`LIBS:camd.library` 41.1,
53816 B). Re-PUT `MIDISEND` (reboot wiped RAM:) and re-ran
`m0post SELFTEST`: `connected: 1, 1`, name `6D 30 70 6F`
("m0po…"), echo `got 2`, rc=0. Full log:
`dell-m0-selftest-post.txt`. M0 is green on both ABIs.
