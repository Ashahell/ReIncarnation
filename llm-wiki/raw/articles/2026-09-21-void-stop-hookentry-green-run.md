# 2026-09-21 — VOID Stop verdict (M50), HookEntry MinNode fix, TRUE-WaitIO green run

> Source: session evidence (guest serial markers, host builds, Guru screenshots), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

Follow-up to the codec-less HUNK record. CloseDevice hung in ExpungeUnit `Wait()`; boundary markers M40–M47 showed the unit process received CTRL_F (M47) but never reached post-FreeHardware (no M48).

## M50 verdict: kill sent, slave never dies

Rebuilt the stock VOID driver from source with Stop-path markers (M50a/b/c).
Two Stops observed per run:

- `M50a Stop slave=0x0000000000000000 slavesig=-1 mastersig=20` →
  `M50b Stop no slave` (hardware actl, playback never started: clean skip).
- `M50a Stop slave=0x000000004a3db7a0 slavesig=16 mastersig=20` →
  `M50b Stop kill sent` → NO M50c. Kill signal valid and sent, slave never
  dies. The slave never runs its loop body (which checks kill each spin), so
  it is stuck inside its first hook call — the mixer trampoline.

## Root cause: HookEntry stub jumped to h_Entry (+0x10, itself)

The hand `HookEntry` stub in `clib_stubs.c` (written for the `ld.lld -r`
device relink) jumped to `hook->h_Entry`. `struct MinNode` is 16 bytes
(mln_Succ + mln_Pred only — Type/Pri/Name belong to `struct Node`), so
`h_Entry` sits at +0x10 and `h_SubEntry` at +0x18, and AHI installs the stub
itself as `h_Entry`. Jumping to +0x10 is infinite self-recursion as a tight
`jmp`: no stack growth, no fault, burns a CPU, never answers kill — which
hung VOID Stop and therefore CloseDevice/ExpungeUnit. The original stub
bytes (`mov 0x18(%rdi),%rax; jmp *%rax`) jump to `h_SubEntry`. The error was
a mis-derived MinNode size (Node layout assumed).

Fix: stub jumps to `h_SubEntry`; rebuilt stub disassembles to
`jmp *0x18(%rdi)`, matching the original.

## VOID driver rebuild recipe (proven, local shadow)

- `version.h`: `VERSION 6`, `REVISION 3`, `VERS "6.3 (28.09.05)"`
  (from `Drivers/Void/version.rev` + `version.date`).
- Compile the 4 TUs (`void-init`, `void-main`, `void-playslave`,
  `void-accel`) plus `Drivers/Common/library.c` with the driver clang
  recipe (`--target=x86_64-unknown-aros`, `-mcmodel=large -mno-red-zone`,
  `-O2`, `-DDRIVER='"void.audio"'`, `-I` for build dir (version.h),
  hdabuild dir (generated `gatestubs.h`), VOID dir, Common dir, AHI tree,
  gen dirs, Developer include).
- Reuse `hdabuild/gatestubs.o` as-is (generated from the common
  `ahi_sub_lib.sfd`, driver-independent).
- `library.o` must be rebuilt per driver (embeds `LibName` via `-DDRIVER`).
- Link with `ld.lld -r` (clang driver needs unbuilt `collect-aros`).
- `_set_call_funcs` (libautoinit) will be UND: none of the VOID objects
  contain `.init_array`/ctors (verified via readelf), so a stub returning 1
  is behaviorally identical and avoids the libautoinit archive chain
  (which pulls further UNDs: `set_call_libfuncs`, `GetDataStreamFromFormat`).
  AROS ELF symbols carry the leading underscore (`_set_call_funcs`).
- Gates: UND count 1 (null entry), marker strings present.
  Result `void.audio` 12760 B vs stock 18728 B.

## Green run (TRUE-WaitIO, CloseDevice enabled)

rc=0 in 7714 ms, agent alive, SUMMARY complete:

- `P4.0 TRUE-WaitIO done err=0` — single unlinked CMD_WRITE completes
  naturally, no abort. VOID completion path proven.
- Ladder err1=0 all 7 rungs (4096–64), err2=-2 (bounded-poll aborts).
- `P4 CloseDevice done`, `P4 reqs deleted`, `P5 summary`; M50c, M46
  (ExpungeUnit post-Wait), M43 (post-expunge) all present.
- Zero IRQ/Guru lines in the run's serial.

Revised terminal state on codec-less: everything works except natural
completion of the LINKED second request (r2, circular `ahir_Link` r1↔r2
stays pending until abort). Single requests complete (err=0). Linked-r2
completion (VOID timing vs link-direction semantics vs device chaining) is
the remaining playback question — no crash, no hang when bounded.

## Files (all /tmp scratch unless noted)

- `/tmp/voidbuild/void.audio` (12760 B, M50 instrumented, UND-clean).
- `/tmp/voidbuild/setcall_stub.c`, `/tmp/voidbuild/version.h`.
- `/tmp/ri/ahibuild/clib_stubs.c` (HookEntry fixed), `device.o` (M40–M48),
  `audioctrl.o` (M49a–d), `mixer_safe.o`, `ahi_newsafe.device` (177480 B).
- `/tmp/ri/sb128_patched.audio`, `/tmp/hdabuild/hdaudio.audio` (M1–M11).
- Probes: `probe_ahi_dbg` (PDBG + P4 ladder + abort bounds),
  `progdir_probe`, `nomodescan_probe`, `devlist_probe`.
- Repo change (uncommitted): `audio_io/probe_ahi.c` r2 device/unit copy.
- Worktree changes (uncommitted, local branch): SB128 volatile init,
  `device.c` M20/M40–M48 markers, `audioctrl.c` M31/M49 markers,
  `Void/void-main.c` M50 markers.
