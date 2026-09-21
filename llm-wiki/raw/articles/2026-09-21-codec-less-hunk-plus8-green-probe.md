# 2026-09-21 — Codec-less guest: HUNK +8 payload offset found; audio stack runs crash-free to first green probe (rc=0)

> Source: session evidence (AROS v1 guest serial log, Guru screenshots/OCR, host disassembly), compiled by agent
> Collected: 2026-09-21
> Published: 2026-09-21

## Disposition
New. Local-only driver work (worktree `/tmp/ari-hda-fix`, branch
`hda-driverinit-nocodec-fix`, NO upstream push). One deliverable fix in-repo
(`audio_io/probe_ahi.c`, uncommitted).

## TL;DR
The codec-less AROS guest (`aros_v1`, no HDA codec) crashed on every AHI probe.
Root causes found and fixed, each proven by forward motion of the crash point:
(1) ReadConfig stale-reloc `movaps` → scalar stores (prior session);
(2) stock `sb128.audio` `DriverInit+0x135` `movaps (.rodata+0x440)` → 1-byte
`movups` patch (scan proceeds) + durable volatile source fix (this session);
(3) `ahi.device` Mix/MixerFunc ~60 vector `movaps/movdqa` → clang
`-fno-vectorize -fno-slp-vectorize` rebuild (0 vector ops), `ld.lld -r`
relink + hand clib stubs (device loads/opens);
(4) **probe bug**: P4 never opened/copied r2 (`dev=0 unit=0`) → `SendIO(r2)`
fault — fixed by documented copy method (`audio_io/probe_ahi.c`).
First full green run: rc=0, SUMMARY complete, zero GPFs.
Terminal honest state on codec-less: VOID fallback loads, CMD_WRITE accepted,
never completes (abort clean, err=-2, dev_min=0); CloseDevice hangs
(ExpungeUnit Wait — unit process never answers CTRL_F).

## The breakthrough: HUNK payload = AllocVec()+8 (consultant-confirmed mechanism)
- Guru disassembly put stock sb128 `DriverInit` at load `0x4A458358`;
  file VMA `0x590` → `.ltext` payload base `0x4A457DC8`, mod16 = **8**.
- Allocation base = payload − 8 (`GETHUNKPTR(x) = BADDR(hunktab[x]) +
  sizeof(BPTR)`, x86-64 `sizeof(BPTR)` = 8) → mod16 = **0**.
- So: allocation 16-aligned, payload 8-shifted. ELF `sh_addralign = 16` is
  NOT preserved at runtime. Any compiler-emitted `movaps/movdqa` against
  HUNK section data can #GP. This revises "AllocMem gives 8" → the loader's
  +8 header is the culprit (consultant's Exp-5, proven from existing evidence,
  no new run needed).
- Rule going forward: **no `movaps/movdqa` from HUNK memory** in x86-64 AROS
  modules until the loader contract changes (`movups/movdqu` or aligned
  runtime buffers). Affects constants, tables, auto-vectorized loops,
  store-merged stores, memcpy expansions.
- sb128 case: GCC fused 8 scalar vendor-table stores into a 16-byte const
  (`movaps .rodata+0x440`, mod16 residue 8 at load). 1-byte patch
  `0F28→0F10` at file `0xBC6` (`/tmp/ri/patch_sb128.py`, patched 48976-B
  binary at `/tmp/ri/sb128_patched.audio`) unblocked the scan; durable fix
  is volatile-data init in `Drivers/SB128/driver-init.c` (verified: rebuilt
  TU has 0 vector ops, no const blob).
- Mix case: `Mix`@0xD1E0 (`movaps (%rax)`, rax = `.rodata+0x2280` reloc) —
  same class. Fixed by TU rebuild flags, not by hand edits (962-insn DSP).

## SendIO gate decoded (consultant Exp-1, verified live)
- `Exec_77_SendIO`: RSI = IORequest*, +0x20 = io_Device, **+0x28 = io_Unit**,
  +0x30 = io_Command. Pre-SendIO dump showed r1 fully initialized
  (dev/unit non-NULL) while r2 was `(0,0)` — never opened. The "gate bug"
  theory is dead; it was a malformed second request.
- Marker discipline mattered: `[PDBG] SendIO r1 done` sits AFTER the call,
  which is what separated r1-ok from r2-fault.

## P4 ladder result (consultant Exp-2/3/4)
- P4.0 single NULL-link: SendIO ok, CheckIO still pending after 1 s,
  Abort+WaitIO clean (err=-2). VOID accepts but never completes CMD_WRITE.
- Linked ladder (r2 fixed): all 7 rungs SendIO r1+r2 clean, abort clean,
  no crash. dev_min=0 honest (nothing ever completes).
- CloseDevice hangs (no Guru, no IRQ): prime suspect ExpungeUnit `Wait()`
  — VOID unit process never answers the CTRL_F teardown (VOID playback path
  likely blocked). Skipping CloseDevice yields the green run. Needs its own
  fix; probe must not gate on close on codec-less.

## Loader notes
- `devs:`-assign shadowing does NOT deliver bytes to lddemon (stock ran
  despite verified assign); `PROGDIR:ahi.device` forced-load works and the
  resident node then serves all later opens. Consultant Exp-6 (ZZTEST.device
  cache experiment) still open — do before touching lddemon.
- `ld.lld -r` relink: 35 vs 20 sections harmless (loads/opens fine); 7 UND
  (strcpy/strlen/snprintf/memcpy/memset/HookEntry/NewList) do NOT bind at
  load (elf2hunk rejects non-zero-reloc SHN_UNDEF; HUNK_EXT unimplemented) —
  hand stubs required. Stub semantics replicated from disassembly of the
  prior working link (HookEntry trampoline, overlap-safe memcpy, bounded
  minimal snprintf — only call site is `"%s/%s.audio"`).
- Treat post-UnLoadSeg PCs as use-after-unload (the old
  `hdaudio DriverInit+0x125` attribution is superseded).

## Files
- Shadows (test-only, `/tmp`): `ahi_newsafe.device` (225352 B),
  `sb128_patched.audio`, `hdaudio.audio` (59000 B, instrumented),
  probes (`probe_ahi_dbg`, `progdir_probe`, `devlist_probe`,
  `loadseg_probe2`, `nomodescan_probe`).
- Provenance: `mixer_safe.o` (clang, no-vectorize), `clib_stubs.c`,
  `sb128_driver_init_fixed.o` (volatile, 0 vecops), `patch_sb128.py`.
- Repo change: `audio_io/probe_ahi.c` r2-copy fix (UNCOMMITTED).
- Worktree change: `Drivers/SB128/driver-init.c` volatile fix (UNCOMMITTED,
  local branch).

## Open (ordered)
1. CloseDevice/ExpungeUnit hang with VOID (unit process never answers CTRL_F).
2. VOID CMD_WRITE never completes (driver-level; decides M1.1 gate wording
   on codec-less).
3. ZZTEST.device lddemon-cache experiment.
4. Durable sb128 rebuild via driver make flow (source fix present).
5. Formal HUNK-alignment build rule for x86-64 module builds
   (`-fno-vectorize -fno-slp-vectorize`, no `movaps` on HUNK data).

## Addendum — same day: CloseDevice hang root-caused to HookEntry stub (FIXED)

> Status of the "Open (ordered)" list above: items 1 and 2 are now CLOSED.
> The CloseDevice hang and the VOID non-completion shared one root cause
> below. Remaining open: linked-r2 natural completion semantics, ZZTEST,
> durable sb128 rebuild, HUNK build rule.

Boundary markers (M40-M47, device + ExpungeUnit + DevProc) showed the unit
process DID receive CTRL_F (M47) but never reached post-FreeHardware (no
M48): hang inside `AHI_FreeAudio` → `AHIsub_Stop` → VOID `Wait(mastersignal)`
for slave death. VOID Stop instrumentation (M50a/b/c, own rebuilt
`void.audio`) showed kill sent but no death: the slave never checks its kill
signal because it never runs its loop body — it is stuck in the hook
trampoline.

Root cause: my hand `HookEntry` stub jumped to `hook->h_Entry` (+0x10).
`struct MinNode` is 16 bytes (Succ+Pred only — the Type/Pri/Name fields
belong to `struct Node`), so `h_Entry` is at +0x10 and `h_SubEntry` at
+0x18, and the hooks are installed as `h_Entry = HookEntry` (the stub
itself). Jumping to +0x10 is infinite self-recursion as a tight `jmp` loop:
no stack growth, no fault, burns a CPU, never answers kill. The original
stub bytes (`mov 0x18(%rdi),%rax; jmp *%rax`) jump to `h_SubEntry` — the
real C function. I had mis-derived MinNode as 18 bytes (Node layout).

Fix: stub jumps to `h_SubEntry`; rebuilt stub disassembles to
`jmp *0x18(%rdi)`, matching the original. Verified live: M50b kill sent →
M50c slave dead → M48e/M48 → M46 (ExpungeUnit post-Wait) → M43
(post-expunge) → P4 `CloseDevice done`.

Terminal green run (same shadows + fixed stub): rc=0 in 7714 ms, agent
alive, SUMMARY complete. `P4.0 TRUE-WaitIO done err=0` — a single unlinked
CMD_WRITE completes naturally (no abort): VOID completion path proven.
Ladder: err1=0 every rung (r1 completes naturally), err2=-2 (bounded-poll
abort). `P4 reqs deleted`, `P5 summary` — full exit clean.

Revised terminal state on codec-less: everything works except natural
completion of the LINKED second request (r2, circular `ahir_Link` r1↔r2):
r2 stays pending until abort. Single requests complete (err=0). Whether
that is VOID timing, link-direction semantics, or device chaining is the
remaining playback question — no crash, no hang (bounded), M1.1 gate wording
should treat linked-double-buffer as a separate compatibility item
(consultant's P4-ladder point).
